// switch_bt.c - Switch Pro Controller BT Classic HID-device role (see switch_bt.h)
// SPDX-License-Identifier: Apache-2.0
//
// The BTstack Classic HID-device transport for the switch_proto engine. Structure
// follows BTstack's hid_*_demo device pattern; the Pro Controller HID report
// descriptor below is the published hardware report map (dekuNukem; identical across
// every Pro Controller and emulator — a functional fact, not copied code).

#include "switch_bt.h"

#ifdef CONFIG_BT_CLASSIC_OUTPUT

#include "switch_proto.h"
#include "core/input_event.h"
#include "core/buttons.h"
#include "core/router/router.h"
#include "platform/platform.h"

#include "btstack.h"
#include "classic/hid_device.h"
#include "classic/device_id_server.h"

#include "pico/rand.h"
#include <stdio.h>
#include <string.h>

// Nintendo identity the Switch matches on.
#define SW_VID        0x057E
#define SW_PID        0x2009
#define SW_COD        0x002508   // peripheral / gamepad
#define SW_NAME       "Pro Controller"
#define SEND_INTERVAL_MS 8       // ~120 Hz report cadence

// Published Pro Controller BT HID report descriptor (134 bytes): input reports
// 0x21/0x30 (48B vendor) + 0x3F (basic HID), output reports 0x01/0x10 (48B vendor).
static const uint8_t k_hid_descriptor[] = {
    0x05,0x01, 0x09,0x05, 0xA1,0x01,
        0x85,0x21, 0x06,0x01,0xFF, 0x09,0x21, 0x15,0x00, 0x25,0x00, 0x75,0x08, 0x95,0x30, 0x81,0x02,
        0x85,0x30, 0x09,0x30, 0x15,0x00, 0x25,0x00, 0x75,0x08, 0x95,0x30, 0x81,0x02,
        0x85,0x3F, 0x05,0x09, 0x19,0x01, 0x29,0x10, 0x15,0x00, 0x25,0x01, 0x75,0x01, 0x95,0x10, 0x81,0x02,
        0x05,0x01, 0x09,0x39, 0x15,0x00, 0x25,0x07, 0x75,0x04, 0x95,0x01, 0x81,0x42,
        0x75,0x04, 0x95,0x01, 0x81,0x03,
        0x09,0x30, 0x09,0x31, 0x09,0x33, 0x09,0x34, 0x15,0x00, 0x27,0xFF,0xFF,0x00,0x00, 0x75,0x10, 0x95,0x04, 0x81,0x02,
        0x85,0x01, 0x06,0x01,0xFF, 0x09,0x01, 0x15,0x00, 0x27,0xFF,0xFF,0x00,0x00, 0x75,0x08, 0x95,0x30, 0x91,0x02,
        0x85,0x10, 0x09,0x10, 0x15,0x00, 0x27,0xFF,0xFF,0x00,0x00, 0x75,0x08, 0x95,0x30, 0x91,0x02,
    0xC0
};

static switch_proto_t   s_proto;
static switch_input_t   s_input;
static uint16_t         s_hid_cid;
static bool             s_connected;
static uint32_t         s_last_send_ms;

static uint8_t          s_hid_sdp[300];
static uint8_t          s_devid_sdp[100];
static btstack_packet_callback_registration_t s_hci_cb;

// ---- engine hooks -----------------------------------------------------------------
static void hook_set_player(void* c, uint8_t player, uint8_t mask)
{ (void)c; (void)mask; printf("[switch_bt] player LED -> P%u\n", player); }
static void hook_set_imu(void* c, bool en)
{ (void)c; printf("[switch_bt] IMU %s\n", en ? "on" : "off"); }
static void hook_set_rumble(void* c, uint8_t l, uint8_t r)
{ (void)c; (void)l; (void)r; }
static void hook_store_pairing(void* c, const uint8_t mac[6], const uint8_t* key)
{ (void)c; (void)mac; (void)key; }  // BTstack owns the BR/EDR link key; nothing to persist here yet
static void hook_random(void* c, uint8_t* out, size_t n)
{ (void)c; for (size_t i = 0; i < n; i += 4) { uint32_t r = get_rand_32(); size_t k = n-i<4?n-i:4; memcpy(out+i,&r,k); } }
static void hook_shutdown(void* c)
{ (void)c; printf("[switch_bt] host requested HCI shutdown\n"); }

static const switch_proto_hooks_t s_hooks = {
    .set_player = hook_set_player, .set_imu = hook_set_imu, .set_rumble = hook_set_rumble,
    .store_pairing = hook_store_pairing, .random_bytes = hook_random, .shutdown = hook_shutdown,
};

// ---- incoming host reports (OUT: 0x01 rumble+subcmd, 0x10 rumble) ------------------
// hid_device passes the report payload without the leading report-id byte, so we
// rebuild [id | payload] for the engine (which keys off data[0]).
static void report_data_cb(uint16_t cid, hid_report_type_t type, uint16_t report_id,
                           int size, uint8_t* report)
{
    (void)cid; (void)type;
    if (size < 0 || size > 62) return;
    uint8_t buf[64];
    buf[0] = (uint8_t)report_id;
    if (size) memcpy(&buf[1], report, (size_t)size);
    switch_proto_handle_output(&s_proto, buf, (uint16_t)(size + 1));
    if (s_connected && s_proto.reply_pending)
        hid_device_request_can_send_now_event(s_hid_cid);
}

static void packet_handler(uint8_t type, uint16_t channel, uint8_t* packet, uint16_t size)
{
    (void)channel; (void)size;
    if (type != HCI_EVENT_PACKET) return;
    switch (hci_event_packet_get_type(packet)) {
    case HCI_EVENT_HID_META:
        switch (hci_event_hid_meta_get_subevent_code(packet)) {
        case HID_SUBEVENT_CONNECTION_OPENED:
            if (hid_subevent_connection_opened_get_status(packet) != ERROR_CODE_SUCCESS) {
                s_connected = false; break;
            }
            s_hid_cid = hid_subevent_connection_opened_get_hid_cid(packet);
            s_connected = true;
            s_proto.report_mode = SW_MODE_SIMPLE;   // fresh session; host will promote
            printf("[switch_bt] Switch connected (cid=0x%04x)\n", s_hid_cid);
            hid_device_request_can_send_now_event(s_hid_cid);
            break;
        case HID_SUBEVENT_CONNECTION_CLOSED:
            printf("[switch_bt] Switch disconnected\n");
            s_connected = false;
            gap_discoverable_control(1);
            gap_connectable_control(1);
            break;
        case HID_SUBEVENT_CAN_SEND_NOW: {
            uint8_t report[64];
            int n = switch_proto_build_input(&s_proto, &s_input, report);
            if (n > 0) hid_device_send_interrupt_message(s_hid_cid, report, (uint16_t)n);
            break;
        }
        default: break;
        }
        break;
    default: break;
    }
}

void switch_bt_init(void)
{
    bd_addr_t local;
    // Best-effort local MAC for the device-info/SPI responses; refreshed after HCI up.
    memset(local, 0, sizeof(local));
    switch_proto_init(&s_proto, local, /*usb=*/false, &s_hooks, NULL);
    memset(&s_input, 0, sizeof(s_input));
    s_input.lx = s_input.ly = s_input.rx = s_input.ry = 128;
    s_input.battery = 8;
}

void switch_bt_late_init(void)
{
    printf("[switch_bt] bringing up Switch Pro (BT Classic) HID device\n");

    // We are emulating a controller, not reading one — stop the BT-HID host scan so
    // it doesn't fight for the radio while we pair with the Switch.
    extern void btstack_host_suppress_scan(bool suppress);
    btstack_host_suppress_scan(true);

    l2cap_init();

    // SDP: HID service record (gamepad subclass) + Device ID (Nintendo VID/PID).
    sdp_init();
    memset(s_hid_sdp, 0, sizeof(s_hid_sdp));
    hid_sdp_record_t rec = {
        .hid_device_subclass      = 0x08,    // gamepad/joystick
        .hid_country_code         = 0x00,
        .hid_virtual_cable        = 0,
        .hid_remote_wake          = 1,
        .hid_reconnect_initiate   = 1,       // controller initiates reconnect to the Switch
        .hid_normally_connectable = 1,
        .hid_boot_device          = 0,
        .hid_ssr_host_max_latency = 0xFFFF,
        .hid_ssr_host_min_timeout = 0xFFFF,
        .hid_supervision_timeout  = 3200,
        .hid_descriptor           = k_hid_descriptor,
        .hid_descriptor_size      = sizeof(k_hid_descriptor),
        .device_name              = SW_NAME,
    };
    hid_create_sdp_record(s_hid_sdp, sdp_create_service_record_handle(), &rec);
    sdp_register_service(s_hid_sdp);

    memset(s_devid_sdp, 0, sizeof(s_devid_sdp));
    device_id_create_sdp_record(s_devid_sdp, sdp_create_service_record_handle(),
                                DEVICE_ID_VENDOR_ID_SOURCE_BLUETOOTH, SW_VID, SW_PID, 0x0100);
    sdp_register_service(s_devid_sdp);

    // Classic GAP identity: name + gamepad class, discoverable + just-works pairing.
    gap_set_local_name(SW_NAME);
    gap_set_class_of_device(SW_COD);
    gap_ssp_set_io_capability(SSP_IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    gap_set_allow_role_switch(true);
    gap_discoverable_control(1);
    gap_connectable_control(1);

    // HID device role.
    hid_device_init(/*boot_protocol=*/0, sizeof(k_hid_descriptor), k_hid_descriptor);
    s_hci_cb.callback = &packet_handler;
    hci_add_event_handler(&s_hci_cb);
    hid_device_register_packet_handler(&packet_handler);
    hid_device_register_report_data_callback(&report_data_cb);

    // Use our real BD_ADDR in the emulated device-info / SPI responses.
    bd_addr_t local;
    gap_local_bd_addr(local);
    memcpy(s_proto.mac, local, 6);
}

void switch_bt_task(void)
{
    // Pull merged controller state (routed to the BLE peripheral target).
    const input_event_t* ev = router_get_output(OUTPUT_TARGET_BLE_PERIPHERAL, 0);
    if (ev) {
        s_input.buttons = ev->buttons;
        s_input.lx = ev->analog[ANALOG_LX];
        s_input.ly = ev->analog[ANALOG_LY];
        s_input.rx = ev->analog[ANALOG_RX];
        s_input.ry = ev->analog[ANALOG_RY];
    }

    if (!s_connected) return;
    uint32_t now = platform_time_ms();
    if ((uint32_t)(now - s_last_send_ms) >= SEND_INTERVAL_MS) {
        s_last_send_ms = now;
        hid_device_request_can_send_now_event(s_hid_cid);
    }
}

bool switch_bt_is_connected(void) { return s_connected; }

#endif // CONFIG_BT_CLASSIC_OUTPUT
