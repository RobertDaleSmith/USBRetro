// bt_transport_usb.c - USB Bluetooth Dongle Transport
// Implements bt_transport_t using the USB BTD/L2CAP stack

#include "bt_transport.h"
#include "usb/usbh/btd/btd.h"
#include "usb/usbh/btd/l2cap.h"
#include "bt/bthid/bthid.h"
#include <string.h>
#include <stdio.h>

// ============================================================================
// STATIC DATA
// ============================================================================

static bt_connection_t usb_connections[BT_MAX_CONNECTIONS];

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

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

// ============================================================================
// TRANSPORT IMPLEMENTATION
// ============================================================================

static void usb_transport_init(void)
{
    memset(usb_connections, 0, sizeof(usb_connections));
    // BTD is initialized by the TinyUSB driver system
    printf("[BT_USB] Transport initialized\n");
}

static void usb_transport_task(void)
{
    btd_task();
    bthid_task();  // Run BT HID device driver tasks
}

static bool usb_transport_is_ready(void)
{
    return btd_is_ready();
}

static uint8_t usb_transport_get_connection_count(void)
{
    return btd_get_connection_count();
}

static const bt_connection_t* usb_transport_get_connection(uint8_t index)
{
    if (index >= BT_MAX_CONNECTIONS) {
        return NULL;
    }
    usb_update_connection(index);
    return &usb_connections[index];
}

static bool usb_transport_send_control(uint8_t conn_index, const uint8_t* data, uint16_t len)
{
    const btd_connection_t* conn = btd_get_connection(conn_index);
    if (!conn || conn->control_cid == 0) {
        return false;
    }
    return l2cap_send(conn->control_cid, data, len);
}

static bool usb_transport_send_interrupt(uint8_t conn_index, const uint8_t* data, uint16_t len)
{
    const btd_connection_t* conn = btd_get_connection(conn_index);
    if (!conn || conn->interrupt_cid == 0) {
        return false;
    }
    return l2cap_send(conn->interrupt_cid, data, len);
}

static void usb_transport_disconnect(uint8_t conn_index)
{
    btd_disconnect(conn_index);
}

static void usb_transport_set_pairing_mode(bool enable)
{
    btd_set_pairing_mode(enable);
}

static bool usb_transport_is_pairing_mode(void)
{
    return btd_is_pairing_mode();
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
