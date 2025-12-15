// bt_transport_usb.c - USB Bluetooth Dongle Transport
// Implements bt_transport_t using the USB BTD/L2CAP stack
// When USE_BTSTACK=1, uses BTstack for BLE instead of our custom stack

#include "bt_transport.h"
#include "bt/bthid/bthid.h"
#include <string.h>
#include <stdio.h>

#if USE_BTSTACK
#include "usb/usbh/btd/btstack_ble.h"
#else
#include "usb/usbh/btd/btd.h"
#include "usb/usbh/btd/btd_l2cap.h"
#endif

// ============================================================================
// STATIC DATA
// ============================================================================

static bt_connection_t usb_connections[BT_MAX_CONNECTIONS];

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

#if !USE_BTSTACK
static void usb_update_connection(uint8_t conn_index)
{
    const btd_connection_t* btd_conn = btd_get_connection(conn_index);
    bt_connection_t* bt_conn = &usb_connections[conn_index];

    if (!btd_conn || btd_conn->state == BTD_CONN_DISCONNECTED) {
        memset(bt_conn, 0, sizeof(*bt_conn));
        return;
    }

    memcpy(bt_conn->bd_addr, btd_conn->bd_addr, 6);
    strncpy(bt_conn->name, btd_conn->name, BT_MAX_NAME_LEN - 1);
    bt_conn->name[BT_MAX_NAME_LEN - 1] = '\0';
    memcpy(bt_conn->class_of_device, btd_conn->class_of_device, 3);
    bt_conn->control_cid = btd_conn->control_cid;
    bt_conn->interrupt_cid = btd_conn->interrupt_cid;
    bt_conn->connected = (btd_conn->state >= BTD_CONN_CONNECTED);
    bt_conn->hid_ready = (btd_conn->state == BTD_CONN_HID_READY);
}
#endif

// ============================================================================
// TRANSPORT IMPLEMENTATION
// ============================================================================

static void usb_transport_init(void)
{
    memset(usb_connections, 0, sizeof(usb_connections));
#if USE_BTSTACK
    printf("[BT_USB] Transport init (BTstack)\n");
    // Initialize BTstack BLE layer (but don't power on yet - wait for dongle)
    printf("[BT_USB] Initializing BTstack BLE...\n");
    btstack_ble_init();
    // Note: btstack_ble_power_on() will be called when dongle is detected
    printf("[BT_USB] BTstack initialized, waiting for dongle...\n");
#else
    // BTD is initialized by the TinyUSB driver system
    printf("[BT_USB] Using custom BT stack\n");
#endif
    printf("[BT_USB] Transport initialized\n");
}

static uint32_t task_counter = 0;

static void usb_transport_task(void)
{
    task_counter++;
    if (task_counter == 1) {
#if USE_BTSTACK
        printf("[BT_USB] task started (BTstack)\n");
#else
        printf("[BT_USB] task started (BTD)\n");
#endif
    }
#if USE_BTSTACK
    // Process BTstack BLE
    btstack_ble_process();
#else
    btd_task();
#endif
    bthid_task();  // Run BT HID device driver tasks
}

static bool usb_transport_is_ready(void)
{
#if USE_BTSTACK
    return btstack_ble_is_powered_on();
#else
    return btd_is_ready();
#endif
}

static uint8_t usb_transport_get_connection_count(void)
{
#if USE_BTSTACK
    // BTstack manages connections internally
    // TODO: Expose connection count from btstack_ble.c
    return 0;
#else
    return btd_get_connection_count();
#endif
}

static const bt_connection_t* usb_transport_get_connection(uint8_t index)
{
    if (index >= BT_MAX_CONNECTIONS) {
        return NULL;
    }
#if USE_BTSTACK
    // BTstack manages connections internally
    // TODO: Expose connection info from btstack_ble.c
    return NULL;
#else
    usb_update_connection(index);
    return &usb_connections[index];
#endif
}

static bool usb_transport_send_control(uint8_t conn_index, const uint8_t* data, uint16_t len)
{
#if USE_BTSTACK
    // BTstack BLE uses GATT write for output reports
    // TODO: Implement GATT write in btstack_ble.c
    (void)conn_index; (void)data; (void)len;
    return false;
#else
    const btd_connection_t* conn = btd_get_connection(conn_index);
    if (!conn || conn->control_cid == 0) {
        return false;
    }
    return l2cap_send(conn->control_cid, data, len);
#endif
}

static bool usb_transport_send_interrupt(uint8_t conn_index, const uint8_t* data, uint16_t len)
{
#if USE_BTSTACK
    // BTstack BLE uses GATT write for output reports
    // TODO: Implement GATT write in btstack_ble.c
    (void)conn_index; (void)data; (void)len;
    return false;
#else
    const btd_connection_t* conn = btd_get_connection(conn_index);
    if (!conn || conn->interrupt_cid == 0) {
        return false;
    }
    return l2cap_send(conn->interrupt_cid, data, len);
#endif
}

static void usb_transport_disconnect(uint8_t conn_index)
{
#if USE_BTSTACK
    // TODO: Implement disconnect in btstack_ble.c
    (void)conn_index;
#else
    btd_disconnect(conn_index);
#endif
}

static void usb_transport_set_pairing_mode(bool enable)
{
#if USE_BTSTACK
    // BTstack BLE always scans for devices
    if (enable) {
        btstack_ble_start_scan();
    } else {
        btstack_ble_stop_scan();
    }
#else
    btd_set_pairing_mode(enable);
#endif
}

static bool usb_transport_is_pairing_mode(void)
{
#if USE_BTSTACK
    return btstack_ble_is_scanning();
#else
    return btd_is_pairing_mode();
#endif
}

// ============================================================================
// TRANSPORT STRUCT
// ============================================================================

const bt_transport_t bt_transport_usb = {
    .name = "USB Dongle",
    .init = usb_transport_init,
    .task = usb_transport_task,
    .is_ready = usb_transport_is_ready,
    .get_connection_count = usb_transport_get_connection_count,
    .get_connection = usb_transport_get_connection,
    .send_control = usb_transport_send_control,
    .send_interrupt = usb_transport_send_interrupt,
    .disconnect = usb_transport_disconnect,
    .set_pairing_mode = usb_transport_set_pairing_mode,
    .is_pairing_mode = usb_transport_is_pairing_mode,
};
