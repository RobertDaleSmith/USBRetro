// btd_glue.c - BTD/L2CAP to BT Transport Integration Layer
// Routes BTD and L2CAP callbacks to the BT transport layer

#include "btd.h"
#include "l2cap.h"
#include "bt/transport/bt_transport.h"
#include <stdio.h>
#include <string.h>

// ============================================================================
// INTERNAL STATE
// Track which connections have both HID channels open
// ============================================================================

static struct {
    bool control_open;
    bool interrupt_open;
    uint16_t control_cid;
    uint16_t interrupt_cid;
} hid_channel_state[BTD_MAX_CONNECTIONS];

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

// Find connection index from L2CAP channel
static int find_conn_index_by_cid(uint16_t cid)
{
    l2cap_channel_t* ch = l2cap_get_channel(cid);
    if (ch) {
        return ch->conn_index;
    }
    return -1;
}

// Update BTD connection with L2CAP channel info
static void update_btd_connection_cids(uint8_t conn_index)
{
    // Access BTD connection to update CIDs
    // Note: btd_get_connection returns const, we need internal access
    // For now, we track in our local state and BTD will read from L2CAP
    extern btd_connection_t* btd_get_connection_mutable(uint8_t index);
    btd_connection_t* conn = btd_get_connection_mutable(conn_index);
    if (conn) {
        conn->control_cid = hid_channel_state[conn_index].control_cid;
        conn->interrupt_cid = hid_channel_state[conn_index].interrupt_cid;

        if (hid_channel_state[conn_index].control_open &&
            hid_channel_state[conn_index].interrupt_open) {
            conn->state = BTD_CONN_HID_READY;
        }
    }
}

// ============================================================================
// BTD CALLBACK IMPLEMENTATIONS
// Override weak implementations in btd.c
// ============================================================================

void btd_on_connection(uint8_t conn_index)
{
    printf("[BTD_GLUE] Connection %d established\n", conn_index);

    // Reset HID channel state for this connection
    memset(&hid_channel_state[conn_index], 0, sizeof(hid_channel_state[0]));

    // L2CAP channels will be created when the remote device sends
    // L2CAP connection requests for HID PSMs
}

void btd_on_disconnection(uint8_t conn_index)
{
    printf("[BTD_GLUE] Connection %d lost\n", conn_index);

    // Clear HID channel state
    memset(&hid_channel_state[conn_index], 0, sizeof(hid_channel_state[0]));

    // Notify transport layer
    bt_on_disconnect(conn_index);
}

void btd_on_acl_data(uint8_t conn_index, const uint8_t* data, uint16_t len)
{
    // Route ACL data to L2CAP layer for processing
    l2cap_process_acl_data(conn_index, data, len);
}

// ============================================================================
// L2CAP CALLBACK IMPLEMENTATIONS
// Override weak implementations in l2cap.c
// ============================================================================

void l2cap_on_channel_open(uint16_t local_cid, uint16_t psm, uint8_t conn_index)
{
    printf("[BTD_GLUE] L2CAP channel 0x%04X opened (PSM=0x%04X, conn=%d)\n",
           local_cid, psm, conn_index);

    if (conn_index >= BTD_MAX_CONNECTIONS) {
        return;
    }

    // Track HID channels
    if (psm == L2CAP_PSM_HID_CONTROL) {
        hid_channel_state[conn_index].control_open = true;
        hid_channel_state[conn_index].control_cid = local_cid;
        printf("[BTD_GLUE] HID Control channel ready\n");
    }
    else if (psm == L2CAP_PSM_HID_INTERRUPT) {
        hid_channel_state[conn_index].interrupt_open = true;
        hid_channel_state[conn_index].interrupt_cid = local_cid;
        printf("[BTD_GLUE] HID Interrupt channel ready\n");
    }

    // Update BTD connection state
    update_btd_connection_cids(conn_index);

    // Check if both HID channels are now open
    if (hid_channel_state[conn_index].control_open &&
        hid_channel_state[conn_index].interrupt_open) {
        printf("[BTD_GLUE] Both HID channels ready - connection %d is HID ready\n", conn_index);
        bt_on_hid_ready(conn_index);
    }
}

void l2cap_on_channel_closed(uint16_t local_cid)
{
    printf("[BTD_GLUE] L2CAP channel 0x%04X closed\n", local_cid);

    // Find which connection this channel belonged to
    for (int i = 0; i < BTD_MAX_CONNECTIONS; i++) {
        if (hid_channel_state[i].control_cid == local_cid) {
            hid_channel_state[i].control_open = false;
            hid_channel_state[i].control_cid = 0;
            update_btd_connection_cids(i);
            break;
        }
        if (hid_channel_state[i].interrupt_cid == local_cid) {
            hid_channel_state[i].interrupt_open = false;
            hid_channel_state[i].interrupt_cid = 0;
            update_btd_connection_cids(i);
            break;
        }
    }
}

void l2cap_on_data(uint16_t local_cid, const uint8_t* data, uint16_t len)
{
    // Find connection index
    int conn_index = find_conn_index_by_cid(local_cid);
    if (conn_index < 0) {
        printf("[BTD_GLUE] Data on unknown channel 0x%04X\n", local_cid);
        return;
    }

    // Check if this is the interrupt channel (HID reports come on interrupt)
    if (local_cid == hid_channel_state[conn_index].interrupt_cid) {
        // HID input report - forward to transport layer
        bt_on_hid_report(conn_index, data, len);
    }
    else if (local_cid == hid_channel_state[conn_index].control_cid) {
        // HID control data (handshake, feature reports, etc.)
        printf("[BTD_GLUE] HID Control data: %d bytes\n", len);
        // TODO: Handle control channel responses if needed
    }
}
