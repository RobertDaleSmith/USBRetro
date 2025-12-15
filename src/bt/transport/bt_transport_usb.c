// bt_transport_usb.c - USB Bluetooth Dongle Transport
// Implements bt_transport_t using BTstack

#include "bt_transport.h"
#include "bt/bthid/bthid.h"
#include "usb/usbh/btd/btstack_ble.h"
#include <string.h>
#include <stdio.h>

// ============================================================================
// STATIC DATA
// ============================================================================

static bt_connection_t usb_connections[BT_MAX_CONNECTIONS];

// ============================================================================
// TRANSPORT IMPLEMENTATION
// ============================================================================

static void usb_transport_init(void)
{
    memset(usb_connections, 0, sizeof(usb_connections));
    printf("[BT_USB] Transport init (BTstack)\n");
    btstack_ble_init();
    printf("[BT_USB] BTstack initialized, waiting for dongle...\n");
}

static uint32_t task_counter = 0;

static void usb_transport_task(void)
{
    task_counter++;
    if (task_counter == 1) {
        printf("[BT_USB] task started (BTstack)\n");
    }
    btstack_ble_process();
    bthid_task();  // Run BT HID device driver tasks
}

static bool usb_transport_is_ready(void)
{
    return btstack_ble_is_powered_on();
}

static uint8_t usb_transport_get_connection_count(void)
{
    // TODO: Expose connection count from btstack_ble.c
    return 0;
}

static const bt_connection_t* usb_transport_get_connection(uint8_t index)
{
    if (index >= BT_MAX_CONNECTIONS) {
        return NULL;
    }
    // TODO: Expose connection info from btstack_ble.c
    return NULL;
}

static bool usb_transport_send_control(uint8_t conn_index, const uint8_t* data, uint16_t len)
{
    // TODO: Implement GATT write for output reports
    (void)conn_index; (void)data; (void)len;
    return false;
}

static bool usb_transport_send_interrupt(uint8_t conn_index, const uint8_t* data, uint16_t len)
{
    // TODO: Implement GATT write for output reports
    (void)conn_index; (void)data; (void)len;
    return false;
}

static void usb_transport_disconnect(uint8_t conn_index)
{
    // TODO: Implement disconnect in btstack_ble.c
    (void)conn_index;
}

static void usb_transport_set_pairing_mode(bool enable)
{
    if (enable) {
        btstack_ble_start_scan();
    } else {
        btstack_ble_stop_scan();
    }
}

static bool usb_transport_is_pairing_mode(void)
{
    return btstack_ble_is_scanning();
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
