// ipega_bt.c - iPega PG-9021 Bluetooth gamepad driver
//
// The PG-9021 pairs over classic Bluetooth and reports as a USB HID gamepad
// (VID 0x1949 / PID 0x0404 in BT mode, 0x0402 wired; BT name "HID GamePad").
// It sends an 11-byte input report with Report ID 7.
//
// Layout — matching the user-provided spec (standard Android/PC mode layout; the pad's bytes
// are NOT swapped vs the usage names):
//
//   [0]  = 0x07 (Report ID)
//   [1]  = LX    left stick X,  0-255, center 128
//   [2]  = LY    left stick Y,  0-255, center 128
//   [3]  = RX    right stick X, 0-255, center 128
//   [4]  = RY    right stick Y, 0-255, center 128
//   [5]  = HAT   switch, 4-bit (0-7 = N..NW, 0x0F = neutral)
//   [6]  = buttons: A=bit0, B=bit1, X=bit3, Y=bit4, LB=bit6, RB=bit7
//   [7]  = Select=bit2, Start=bit3, L3=bit5, R3=bit6
//   [8]  = RT    right trigger analog (0-255)
//   [9]  = LT    left trigger analog (0-255)
//   [10] = counter / varies (ignored)
//
// Home lives in the separate Consumer Control interface (Report ID 2, byte1
// bit7 = 0x80 pressed / 0x00 released). It maps to JP_BUTTON_A1 with an
// edge + auto-release timeout so it can never stay stuck pressed, even on
// modes that stop sending the release report.
//
// Note: byte6 bit0 (A) may read active at rest on some units - check whether
// the button is physically held down (e.g. by a clip) before touching the
// decoder.

#include "ipega_bt.h"
#include "bt/bthid/bthid.h"
#include "core/input_event.h"
#include "core/router/router.h"
#include "core/buttons.h"
#include "core/services/players/manager.h"
#include "core/services/players/feedback.h"
#include <string.h>
#include <stdio.h>

// ============================================================================
// iPega IDs
// ============================================================================

#define IPEGA_VID           0x1949
#define IPEGA_PID_BT        0x0404   // BT mode
#define IPEGA_PID_USB       0x0402   // wired (defensive match with name)

#define IPEGA_REPORT_ID     0x07     // gamepad input report ID
#define IPEGA_REPORT_LEN    11       // full report including report ID byte
#define IPEGA_REPORT_ID_HOME 0x02    // Consumer Control (Home = byte1 bit7)
#define IPEGA_HOME_AUTOFADE 60       // reports w/o fresh press => auto-release (~300ms)

// ============================================================================
// BUTTON BIT MASKS (16-bit field at bytes 6-7)
// ============================================================================

#define IPEGA_BTN_A         0x0001
#define IPEGA_BTN_B         0x0002
#define IPEGA_BTN_X         0x0008
#define IPEGA_BTN_Y         0x0010
#define IPEGA_BTN_L1        0x0040
#define IPEGA_BTN_R1        0x0080
#define IPEGA_BTN_BACK      0x0400
#define IPEGA_BTN_START     0x0800
#define IPEGA_BTN_L3        0x2000
#define IPEGA_BTN_R3        0x4000

// ============================================================================
// HAT SWITCH LOOKUP (0=N, 1=NE, 2=E, 3=SE, 4=S, 5=SW, 6=W, 7=NW)
// Returns packed dpad bits: bit0=up, bit1=right, bit2=down, bit3=left
// ============================================================================

static const uint8_t IPEGA_HAT_TO_DPAD[] = {
    0b0001, 0b0011, 0b0010, 0b0110, 0b0100, 0b1100, 0b1000, 0b1001
};

// ============================================================================
// DRIVER DATA
// ============================================================================

typedef struct {
    input_event_t event;
    bool initialized;
    bool first_report_dumped;
    bool home_pending;
    uint16_t home_no_refresh;
} ipega_bt_data_t;

static ipega_bt_data_t ipega_data[BTHID_MAX_DEVICES];

// ============================================================================
// HELPERS
// ============================================================================

static bool ipega_name_confirms_model(const char* device_name);

static bool ci_strstr(const char* haystack, const char* needle)
{
    if (!haystack || !needle || !haystack[0] || !needle[0]) {
        return false;
    }
    size_t needle_len = strlen(needle);
    for (const char* p = haystack; p[0]; p++) {
        size_t i = 0;
        while (i < needle_len && p[i] &&
               (p[i] == needle[i] ||
                (p[i] >= 'A' && p[i] <= 'Z' && p[i] + 32 == needle[i]) ||
                (needle[i] >= 'A' && needle[i] <= 'Z' && p[i] == needle[i] + 32))) {
            i++;
        }
        if (i == needle_len) {
            return true;
        }
    }
    return false;
}

// ============================================================================
// DRIVER IMPLEMENTATION
// ============================================================================

static bool ipega_match(const char* device_name, const uint8_t* class_of_device,
                        uint16_t vendor_id, uint16_t product_id, bool is_ble)
{
    (void)class_of_device;
    (void)is_ble;

    // iPega VID 0x1949 with either iPega PID identifies the PG-9021 family.
    // The pad may advertise any name over a generic BT dongle (commonly
    // "HID GamePad"), so match on VID/PID alone - DO NOT require the model
    // in the name, otherwise the generic driver grabs it and mis-maps it.
    if (vendor_id == IPEGA_VID &&
        (product_id == IPEGA_PID_BT || product_id == IPEGA_PID_USB)) {
        return true;
    }

    // Some stacks never resolve VID/PID (no SDP Device ID) - rely on name.
    if (ipega_name_confirms_model(device_name)) {
        return true;
    }

    return false;
}

static bool ipega_name_confirms_model(const char* device_name)
{
    if (!device_name || !device_name[0]) {
        return false;
    }
    if (ci_strstr(device_name, "PG-9021")) {
        return true;
    }
    if (ci_strstr(device_name, "9021")) {
        return true;
    }
    if (ci_strstr(device_name, "ipega classic gamepad")) {
        return true;
    }
    return false;
}

static bool ipega_init(bthid_device_t* device)
{
    printf("[IPEGA_BT] Init for device: %s\n", device->name);

    for (int i = 0; i < BTHID_MAX_DEVICES; i++) {
        if (!ipega_data[i].initialized) {
            init_input_event(&ipega_data[i].event);
            ipega_data[i].initialized = true;
            ipega_data[i].first_report_dumped = false;

            ipega_data[i].event.type = INPUT_TYPE_GAMEPAD;
            ipega_data[i].event.transport = device->is_ble ?
                INPUT_TRANSPORT_BT_BLE : INPUT_TRANSPORT_BT_CLASSIC;
            ipega_data[i].event.dev_addr = device->conn_index;
            ipega_data[i].event.instance = 0;

            device->driver_data = &ipega_data[i];
            return true;
        }
    }

    return false;
}

static void ipega_process_report(bthid_device_t* device, const uint8_t* data, uint16_t len)
{
    ipega_bt_data_t* id = (ipega_bt_data_t*)device->driver_data;
    if (!id) return;

    // Dump the first report of every connection. Because each PG-9021 console
    // mode (XInput/PS3/PS4/Switch) uses a different report layout, connect the
    // pad in each mode and capture this line to reverse-engineer the mapping.
    if (!id->first_report_dumped) {
        id->first_report_dumped = true;
        printf("[IPEGA_BT] Report (%d bytes):", len);
        for (int i = 0; i < len && i < 20; i++) printf(" %02x", data[i]);
        printf("\n");
    }

    // Consumer Control report (RID 2): byte1 bit7 (0x80) = Home pressed,
    // 0x00 = released. Only flips the pending flag here - the button is
    // applied on the next gamepad report so the event has a single submit.
    // Auto-release (fade) happens in the gamepad path to avoid a stuck Home.
    if (data[0] == IPEGA_REPORT_ID_HOME && len >= 2) {
        if (data[1] & 0x80) {
            id->home_pending = true;
            id->home_no_refresh = 0;
        } else {
            id->home_pending = false;
        }
        return;
    }

    // Only the gamepad input report (RID 7, 11 bytes) is decoded here.
    if (len < IPEGA_REPORT_LEN || data[0] != IPEGA_REPORT_ID) {
        return;
    }

    // Analog sticks (native HID: 0=min, 128=center, 255=max)
    // Mapping per the user-provided spec (standard HID layout of the PG-9021's
    // Android/PC mode): byte1=LX, byte2=LY, byte3=RX, byte4=RY.
    uint8_t lx = data[1];   // left stick X
    uint8_t ly = data[2];   // left stick Y
    uint8_t rx = data[3];   // right stick X
    uint8_t ry = data[4];   // right stick Y
    uint8_t lt = data[9];   // LT (left trigger)
    uint8_t rt = data[8];   // RT (right trigger)

    uint16_t buttons = (uint16_t)data[6] | ((uint16_t)data[7] << 8);
    uint32_t b = 0;

    // D-pad (4-bit hat switch; 0x0F = neutral)
    uint8_t hat = data[5] & 0x0F;
    if (hat < 8) {
        uint8_t dpad = IPEGA_HAT_TO_DPAD[hat];
        if (dpad & 0x01) b |= JP_BUTTON_DU;
        if (dpad & 0x02) b |= JP_BUTTON_DR;
        if (dpad & 0x04) b |= JP_BUTTON_DD;
        if (dpad & 0x08) b |= JP_BUTTON_DL;
    }

    // Face / shoulder / system buttons
    if (buttons & IPEGA_BTN_A)    b |= JP_BUTTON_B1;   // A
    if (buttons & IPEGA_BTN_B)    b |= JP_BUTTON_B2;   // B
    if (buttons & IPEGA_BTN_X)    b |= JP_BUTTON_B3;   // X
    if (buttons & IPEGA_BTN_Y)    b |= JP_BUTTON_B4;   // Y
    if (buttons & IPEGA_BTN_L1)   b |= JP_BUTTON_L1;   // L1
    if (buttons & IPEGA_BTN_R1)   b |= JP_BUTTON_R1;   // R1
    if (buttons & IPEGA_BTN_BACK) b |= JP_BUTTON_S1;   // Back/Select
    if (buttons & IPEGA_BTN_START) b |= JP_BUTTON_S2;  // Start
    if (buttons & IPEGA_BTN_L3)   b |= JP_BUTTON_L3;   // L3
    if (buttons & IPEGA_BTN_R3)   b |= JP_BUTTON_R3;   // R3

    // Home (A1) comes from the Consumer Control report. Auto-release after
    // IPEGA_HOME_AUTOFADE reports without a fresh press so it can never stay
    // stuck pressed, even if a mode stops sending the release report.
    if (id->home_pending) {
        b |= JP_BUTTON_A1;
        if (++id->home_no_refresh >= IPEGA_HOME_AUTOFADE) {
            id->home_pending = false;
        }
    }

    id->event.buttons = b;
    id->event.button_count = 10;
    id->event.analog[ANALOG_LX] = lx;
    id->event.analog[ANALOG_LY] = ly;
    id->event.analog[ANALOG_RX] = rx;
    id->event.analog[ANALOG_RY] = ry;
    id->event.analog[ANALOG_L2] = lt;
    id->event.analog[ANALOG_R2] = rt;

    router_submit_input(&id->event);
}

static void ipega_task(bthid_device_t* device)
{
    // PG-9021 has no rumble support - just consume pending feedback state so
    // the player feedback system does not keep a stale dirty flag.
    ipega_bt_data_t* id = (ipega_bt_data_t*)device->driver_data;
    if (!id) return;

    int player_idx = find_player_index(id->event.dev_addr, id->event.instance);
    if (player_idx < 0) return;

    feedback_state_t* fb = feedback_get_state(player_idx);
    if (!fb || !fb->rumble_dirty) return;
    feedback_clear_dirty(player_idx);
}

static void ipega_disconnect(bthid_device_t* device)
{
    printf("[IPEGA_BT] Disconnect: %s\n", device->name);

    ipega_bt_data_t* id = (ipega_bt_data_t*)device->driver_data;
    if (id) {
        // Clear router state first (sends zeroed input report)
        router_device_disconnected(id->event.dev_addr, id->event.instance);
        // Remove player assignment
        remove_players_by_address(id->event.dev_addr, id->event.instance);

        init_input_event(&id->event);
        id->initialized = false;
        id->home_pending = false;
        id->home_no_refresh = 0;
    }
}

// ============================================================================
// DRIVER STRUCT
// ============================================================================

const bthid_driver_t ipega_bt_driver = {
    .name = "iPega PG-9021 BT",
    .match = ipega_match,
    .init = ipega_init,
    .process_report = ipega_process_report,
    .task = ipega_task,
    .disconnect = ipega_disconnect,
};

void ipega_bt_register(void)
{
    bthid_register_driver(&ipega_bt_driver);
}
