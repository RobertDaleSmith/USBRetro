// att.c - Attribute Protocol (ATT) implementation for BLE
// Implements GATT client for HID over GATT Profile (HOGP)

#include "att.h"
#include "btd.h"
#include "l2cap.h"
#include <string.h>
#include <stdio.h>

// ============================================================================
// STATIC DATA
// ============================================================================

#define ATT_MAX_CLIENTS 4
static att_client_t att_clients[ATT_MAX_CLIENTS];

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

static att_client_t* att_get_client(uint8_t conn_index)
{
    for (int i = 0; i < ATT_MAX_CLIENTS; i++) {
        if (att_clients[i].conn_index == conn_index && att_clients[i].handle != 0) {
            return &att_clients[i];
        }
    }
    return NULL;
}

static att_client_t* att_alloc_client(uint8_t conn_index, uint16_t handle)
{
    for (int i = 0; i < ATT_MAX_CLIENTS; i++) {
        if (att_clients[i].handle == 0) {
            memset(&att_clients[i], 0, sizeof(att_client_t));
            att_clients[i].conn_index = conn_index;
            att_clients[i].handle = handle;
            att_clients[i].mtu = ATT_DEFAULT_MTU;
            att_clients[i].state = ATT_STATE_IDLE;
            return &att_clients[i];
        }
    }
    return NULL;
}

static void att_free_client(att_client_t* client)
{
    if (client) {
        memset(client, 0, sizeof(att_client_t));
    }
}

// ============================================================================
// ATT INITIALIZATION
// ============================================================================

void att_init(void)
{
    memset(att_clients, 0, sizeof(att_clients));
    printf("[ATT] Initialized\n");
}

// ============================================================================
// CONNECTION MANAGEMENT
// ============================================================================

void att_on_connect(uint8_t conn_index, uint16_t handle)
{
    printf("[ATT] BLE connection %d (handle=0x%04X)\n", conn_index, handle);

    att_client_t* client = att_alloc_client(conn_index, handle);
    if (!client) {
        printf("[ATT] ERROR: No free ATT client slots\n");
        return;
    }

    // Start discovery after a short delay (let connection stabilize)
    // For now, start immediately
    att_start_discovery(conn_index);
}

void att_on_disconnect(uint8_t conn_index)
{
    printf("[ATT] BLE disconnection %d\n", conn_index);

    att_client_t* client = att_get_client(conn_index);
    if (client) {
        att_free_client(client);
    }
}

// ============================================================================
// ATT SEND
// ============================================================================

bool att_send(uint8_t conn_index, const uint8_t* data, uint16_t len)
{
    att_client_t* client = att_get_client(conn_index);
    if (!client) {
        printf("[ATT] ERROR: No client for conn %d\n", conn_index);
        return false;
    }

    // Send via L2CAP on fixed CID 0x0004 (ATT)
    return l2cap_send_ble(client->handle, 0x0004, data, len);
}

// ============================================================================
// ATT REQUESTS
// ============================================================================

bool att_exchange_mtu(uint8_t conn_index, uint16_t mtu)
{
    printf("[ATT] Exchange MTU: %d\n", mtu);

    att_exchange_mtu_t req = {
        .opcode = ATT_EXCHANGE_MTU_REQ,
        .mtu = mtu
    };

    att_client_t* client = att_get_client(conn_index);
    if (client) {
        client->state = ATT_STATE_MTU_EXCHANGE;
    }

    return att_send(conn_index, (uint8_t*)&req, sizeof(req));
}

bool att_read_by_group_type(uint8_t conn_index, uint16_t start, uint16_t end, uint16_t uuid)
{
    printf("[ATT] Read By Group Type: 0x%04X-0x%04X uuid=0x%04X\n", start, end, uuid);

    att_read_by_group_type_req_t req = {
        .opcode = ATT_READ_BY_GROUP_TYPE_REQ,
        .start_handle = start,
        .end_handle = end,
        .uuid = uuid
    };

    return att_send(conn_index, (uint8_t*)&req, sizeof(req));
}

bool att_read_by_type(uint8_t conn_index, uint16_t start, uint16_t end, uint16_t uuid)
{
    printf("[ATT] Read By Type: 0x%04X-0x%04X uuid=0x%04X\n", start, end, uuid);

    att_read_by_type_req_t req = {
        .opcode = ATT_READ_BY_TYPE_REQ,
        .start_handle = start,
        .end_handle = end,
        .uuid = uuid
    };

    return att_send(conn_index, (uint8_t*)&req, sizeof(req));
}

bool att_find_information(uint8_t conn_index, uint16_t start, uint16_t end)
{
    printf("[ATT] Find Information: 0x%04X-0x%04X\n", start, end);

    att_find_info_req_t req = {
        .opcode = ATT_FIND_INFORMATION_REQ,
        .start_handle = start,
        .end_handle = end
    };

    return att_send(conn_index, (uint8_t*)&req, sizeof(req));
}

bool att_read(uint8_t conn_index, uint16_t handle)
{
    printf("[ATT] Read: handle=0x%04X\n", handle);

    att_read_req_t req = {
        .opcode = ATT_READ_REQ,
        .handle = handle
    };

    return att_send(conn_index, (uint8_t*)&req, sizeof(req));
}

bool att_write(uint8_t conn_index, uint16_t handle, const uint8_t* data, uint16_t len)
{
    printf("[ATT] Write: handle=0x%04X len=%d\n", handle, len);

    uint8_t buf[ATT_MAX_MTU];
    buf[0] = ATT_WRITE_REQ;
    buf[1] = handle & 0xFF;
    buf[2] = (handle >> 8) & 0xFF;

    if (len > sizeof(buf) - 3) {
        len = sizeof(buf) - 3;
    }
    memcpy(&buf[3], data, len);

    return att_send(conn_index, buf, 3 + len);
}

bool att_write_cmd(uint8_t conn_index, uint16_t handle, const uint8_t* data, uint16_t len)
{
    uint8_t buf[ATT_MAX_MTU];
    buf[0] = ATT_WRITE_CMD;
    buf[1] = handle & 0xFF;
    buf[2] = (handle >> 8) & 0xFF;

    if (len > sizeof(buf) - 3) {
        len = sizeof(buf) - 3;
    }
    memcpy(&buf[3], data, len);

    return att_send(conn_index, buf, 3 + len);
}

// ============================================================================
// DISCOVERY STATE MACHINE
// ============================================================================

void att_start_discovery(uint8_t conn_index)
{
    att_client_t* client = att_get_client(conn_index);
    if (!client) return;

    printf("[ATT] Starting GATT discovery\n");

    // Start with MTU exchange
    client->state = ATT_STATE_MTU_EXCHANGE;
    att_exchange_mtu(conn_index, ATT_MAX_MTU);
}

static void att_continue_discovery(att_client_t* client)
{
    switch (client->state) {
        case ATT_STATE_MTU_EXCHANGE:
            // MTU exchange complete, discover primary services
            printf("[ATT] Discovering primary services...\n");
            client->state = ATT_STATE_DISCOVER_SERVICES;
            client->discover_start = 0x0001;
            client->discover_end = 0xFFFF;
            att_read_by_group_type(client->conn_index, 0x0001, 0xFFFF, GATT_UUID_PRIMARY_SERVICE);
            break;

        case ATT_STATE_DISCOVER_SERVICES:
            // Look for HID service
            client->hid_service_start = 0;
            client->hid_service_end = 0;
            for (int i = 0; i < client->num_services; i++) {
                printf("[ATT] Service: uuid=0x%04X handles=0x%04X-0x%04X\n",
                       client->services[i].uuid,
                       client->services[i].start_handle,
                       client->services[i].end_handle);
                if (client->services[i].uuid == GATT_UUID_HID_SERVICE) {
                    client->hid_service_start = client->services[i].start_handle;
                    client->hid_service_end = client->services[i].end_handle;
                    printf("[ATT] *** Found HID Service! ***\n");
                }
            }

            if (client->hid_service_start != 0) {
                // Discover characteristics of HID service
                printf("[ATT] Discovering HID characteristics...\n");
                client->state = ATT_STATE_DISCOVER_CHARACTERISTICS;
                client->discover_start = client->hid_service_start;
                client->discover_end = client->hid_service_end;
                att_read_by_type(client->conn_index,
                                 client->hid_service_start,
                                 client->hid_service_end,
                                 GATT_UUID_CHARACTERISTIC);
            } else {
                printf("[ATT] ERROR: HID Service not found!\n");
                client->state = ATT_STATE_IDLE;
            }
            break;

        case ATT_STATE_DISCOVER_CHARACTERISTICS:
            // Print discovered characteristics
            for (int i = 0; i < client->num_characteristics; i++) {
                printf("[ATT] Char: uuid=0x%04X handle=0x%04X value=0x%04X props=0x%02X\n",
                       client->characteristics[i].uuid,
                       client->characteristics[i].handle,
                       client->characteristics[i].value_handle,
                       client->characteristics[i].properties);
            }

            // Start discovering descriptors for each characteristic
            client->current_char = 0;
            client->state = ATT_STATE_DISCOVER_DESCRIPTORS;
            // Fall through to start descriptor discovery

        case ATT_STATE_DISCOVER_DESCRIPTORS: {
            // Find next characteristic that needs descriptor discovery
            while (client->current_char < client->num_characteristics) {
                att_characteristic_t* ch = &client->characteristics[client->current_char];

                // Calculate descriptor range (between value handle and next char or service end)
                uint16_t desc_start = ch->value_handle + 1;
                uint16_t desc_end;
                if (client->current_char + 1 < client->num_characteristics) {
                    desc_end = client->characteristics[client->current_char + 1].handle - 1;
                } else {
                    desc_end = client->hid_service_end;
                }

                if (desc_start <= desc_end) {
                    printf("[ATT] Finding descriptors for char 0x%04X (0x%04X-0x%04X)\n",
                           ch->uuid, desc_start, desc_end);
                    client->discover_start = desc_start;
                    client->discover_end = desc_end;
                    att_find_information(client->conn_index, desc_start, desc_end);
                    return;
                }

                client->current_char++;
            }

            // All descriptors discovered, read Report Map
            printf("[ATT] Descriptor discovery complete, reading Report Map...\n");
            client->state = ATT_STATE_READ_REPORT_MAP;

            // Find Report Map characteristic and read it
            for (int i = 0; i < client->num_characteristics; i++) {
                if (client->characteristics[i].uuid == GATT_UUID_HID_REPORT_MAP) {
                    att_read(client->conn_index, client->characteristics[i].value_handle);
                    return;
                }
            }
            printf("[ATT] WARNING: Report Map not found\n");
            client->state = ATT_STATE_ENABLE_NOTIFICATIONS;
            // Fall through
        }

        case ATT_STATE_READ_REPORT_MAP:
            // Report Map read, now enable notifications on Report characteristics
            printf("[ATT] Enabling notifications on Report characteristics...\n");
            client->state = ATT_STATE_ENABLE_NOTIFICATIONS;
            client->current_char = 0;
            // Fall through

        case ATT_STATE_ENABLE_NOTIFICATIONS: {
            // Enable notifications on each Report characteristic with notify property
            while (client->current_char < client->num_characteristics) {
                att_characteristic_t* ch = &client->characteristics[client->current_char];
                client->current_char++;

                if (ch->uuid == GATT_UUID_HID_REPORT &&
                    (ch->properties & GATT_CHAR_PROP_NOTIFY) &&
                    ch->cccd_handle != 0) {
                    printf("[ATT] Enabling notifications on Report handle=0x%04X\n",
                           ch->value_handle);
                    uint8_t cccd_value[2] = {GATT_CCCD_NOTIFICATION & 0xFF,
                                              (GATT_CCCD_NOTIFICATION >> 8) & 0xFF};
                    att_write(client->conn_index, ch->cccd_handle, cccd_value, 2);
                    return;
                }
            }

            // All notifications enabled
            printf("[ATT] *** GATT Discovery Complete - Ready for HID reports ***\n");
            client->state = ATT_STATE_READY;
            break;
        }

        default:
            break;
    }
}

// ============================================================================
// ATT RESPONSE HANDLERS
// ============================================================================

static void att_handle_error_rsp(att_client_t* client, const uint8_t* data, uint16_t len)
{
    if (len < sizeof(att_error_rsp_t)) return;

    const att_error_rsp_t* err = (const att_error_rsp_t*)data;
    printf("[ATT] Error: req=0x%02X handle=0x%04X error=0x%02X\n",
           err->req_opcode, err->handle, err->error_code);

    // Handle specific errors during discovery
    if (err->error_code == ATT_ERROR_ATTRIBUTE_NOT_FOUND) {
        // This is expected at end of discovery, continue to next phase
        att_continue_discovery(client);
    }
}

static void att_handle_mtu_rsp(att_client_t* client, const uint8_t* data, uint16_t len)
{
    if (len < sizeof(att_exchange_mtu_t)) return;

    const att_exchange_mtu_t* rsp = (const att_exchange_mtu_t*)data;
    uint16_t server_mtu = rsp->mtu;

    // Use minimum of client and server MTU
    client->mtu = (ATT_MAX_MTU < server_mtu) ? ATT_MAX_MTU : server_mtu;
    printf("[ATT] MTU negotiated: %d\n", client->mtu);

    att_continue_discovery(client);
}

static void att_handle_read_by_group_type_rsp(att_client_t* client, const uint8_t* data, uint16_t len)
{
    if (len < 2) return;

    uint8_t attr_len = data[1];  // Length of each attribute data
    const uint8_t* ptr = &data[2];
    len -= 2;

    printf("[ATT] Read By Group Type Response: attr_len=%d\n", attr_len);

    // Parse service records
    while (len >= attr_len && client->num_services < ATT_MAX_SERVICES) {
        att_service_t* svc = &client->services[client->num_services];

        svc->start_handle = ptr[0] | (ptr[1] << 8);
        svc->end_handle = ptr[2] | (ptr[3] << 8);

        // UUID can be 2, 4, or 16 bytes
        if (attr_len == 6) {
            // 16-bit UUID
            svc->uuid = ptr[4] | (ptr[5] << 8);
        } else if (attr_len == 20) {
            // 128-bit UUID - extract last 2 bytes as 16-bit UUID approximation
            // (This is a simplification - full 128-bit UUID support would need more)
            svc->uuid = ptr[4] | (ptr[5] << 8);
        } else {
            svc->uuid = 0;
        }

        client->num_services++;
        client->discover_start = svc->end_handle + 1;

        ptr += attr_len;
        len -= attr_len;
    }

    // Continue discovery if more services might exist
    if (client->discover_start <= 0xFFFF && client->discover_start != 0) {
        att_read_by_group_type(client->conn_index,
                               client->discover_start, 0xFFFF,
                               GATT_UUID_PRIMARY_SERVICE);
    } else {
        att_continue_discovery(client);
    }
}

static void att_handle_read_by_type_rsp(att_client_t* client, const uint8_t* data, uint16_t len)
{
    if (len < 2) return;

    uint8_t attr_len = data[1];
    const uint8_t* ptr = &data[2];
    len -= 2;

    printf("[ATT] Read By Type Response: attr_len=%d\n", attr_len);

    // Parse characteristic declarations
    while (len >= attr_len && client->num_characteristics < ATT_MAX_CHARACTERISTICS) {
        att_characteristic_t* ch = &client->characteristics[client->num_characteristics];

        ch->handle = ptr[0] | (ptr[1] << 8);
        ch->properties = ptr[2];
        ch->value_handle = ptr[3] | (ptr[4] << 8);

        // UUID (16-bit or 128-bit)
        if (attr_len == 7) {
            ch->uuid = ptr[5] | (ptr[6] << 8);
        } else if (attr_len == 21) {
            // 128-bit UUID - extract as 16-bit approximation
            ch->uuid = ptr[5] | (ptr[6] << 8);
        } else {
            ch->uuid = 0;
        }

        ch->cccd_handle = 0;
        ch->report_id = 0;
        ch->report_type = 0;

        client->num_characteristics++;
        client->discover_start = ch->handle + 1;

        ptr += attr_len;
        len -= attr_len;
    }

    // Continue discovery if more characteristics might exist
    if (client->discover_start <= client->discover_end) {
        att_read_by_type(client->conn_index,
                         client->discover_start, client->discover_end,
                         GATT_UUID_CHARACTERISTIC);
    } else {
        att_continue_discovery(client);
    }
}

static void att_handle_find_info_rsp(att_client_t* client, const uint8_t* data, uint16_t len)
{
    if (len < 2) return;

    uint8_t format = data[1];  // 1 = 16-bit UUIDs, 2 = 128-bit UUIDs
    const uint8_t* ptr = &data[2];
    len -= 2;

    uint8_t entry_len = (format == 1) ? 4 : 18;

    printf("[ATT] Find Information Response: format=%d\n", format);

    while (len >= entry_len) {
        uint16_t handle = ptr[0] | (ptr[1] << 8);
        uint16_t uuid = (format == 1) ? (ptr[2] | (ptr[3] << 8)) : (ptr[2] | (ptr[3] << 8));

        printf("[ATT]   Descriptor: handle=0x%04X uuid=0x%04X\n", handle, uuid);

        // Check if this is a CCCD or Report Reference
        if (uuid == GATT_UUID_CCCD) {
            // Find the characteristic this belongs to
            for (int i = client->num_characteristics - 1; i >= 0; i--) {
                if (handle > client->characteristics[i].value_handle) {
                    client->characteristics[i].cccd_handle = handle;
                    printf("[ATT]   -> CCCD for char 0x%04X\n",
                           client->characteristics[i].uuid);
                    break;
                }
            }
        } else if (uuid == GATT_UUID_REPORT_REFERENCE) {
            // This tells us the report ID and type - we'd need to read it
            printf("[ATT]   -> Report Reference descriptor\n");
        }

        client->discover_start = handle + 1;
        ptr += entry_len;
        len -= entry_len;
    }

    // Continue descriptor discovery
    if (client->discover_start <= client->discover_end) {
        att_find_information(client->conn_index,
                             client->discover_start, client->discover_end);
    } else {
        // Move to next characteristic
        client->current_char++;
        att_continue_discovery(client);
    }
}

static void att_handle_read_rsp(att_client_t* client, const uint8_t* data, uint16_t len)
{
    // Skip opcode
    data++;
    len--;

    printf("[ATT] Read Response: %d bytes\n", len);

    if (client->state == ATT_STATE_READ_REPORT_MAP) {
        // Store Report Map
        if (len <= sizeof(client->report_map)) {
            memcpy(client->report_map, data, len);
            client->report_map_len = len;
            printf("[ATT] Report Map: %d bytes\n", len);

            // Print first few bytes for debug
            printf("[ATT]   Data: ");
            for (int i = 0; i < len && i < 32; i++) {
                printf("%02X ", data[i]);
            }
            printf("...\n");
        }
        att_continue_discovery(client);
    }
}

static void att_handle_write_rsp(att_client_t* client, const uint8_t* data, uint16_t len)
{
    (void)data;
    (void)len;

    printf("[ATT] Write Response OK\n");

    if (client->state == ATT_STATE_ENABLE_NOTIFICATIONS) {
        att_continue_discovery(client);
    }
}

static void att_handle_notification(att_client_t* client, const uint8_t* data, uint16_t len)
{
    if (len < 3) return;

    uint16_t handle = data[1] | (data[2] << 8);
    const uint8_t* value = &data[3];
    uint16_t value_len = len - 3;

    // Find which characteristic this is
    uint8_t report_id = 0;
    for (int i = 0; i < client->num_characteristics; i++) {
        if (client->characteristics[i].value_handle == handle) {
            report_id = client->characteristics[i].report_id;
            break;
        }
    }

    // Call HID report handler
    att_on_hid_report(client->conn_index, report_id, value, value_len);
}

// ============================================================================
// ATT DATA PROCESSING
// ============================================================================

void att_process_data(uint8_t conn_index, const uint8_t* data, uint16_t len)
{
    if (len < 1) return;

    att_client_t* client = att_get_client(conn_index);
    if (!client) {
        printf("[ATT] ERROR: No client for conn %d\n", conn_index);
        return;
    }

    uint8_t opcode = data[0];

    switch (opcode) {
        case ATT_ERROR_RSP:
            att_handle_error_rsp(client, data, len);
            break;

        case ATT_EXCHANGE_MTU_RSP:
            att_handle_mtu_rsp(client, data, len);
            break;

        case ATT_READ_BY_GROUP_TYPE_RSP:
            att_handle_read_by_group_type_rsp(client, data, len);
            break;

        case ATT_READ_BY_TYPE_RSP:
            att_handle_read_by_type_rsp(client, data, len);
            break;

        case ATT_FIND_INFORMATION_RSP:
            att_handle_find_info_rsp(client, data, len);
            break;

        case ATT_READ_RSP:
            att_handle_read_rsp(client, data, len);
            break;

        case ATT_WRITE_RSP:
            att_handle_write_rsp(client, data, len);
            break;

        case ATT_HANDLE_VALUE_NTF:
            att_handle_notification(client, data, len);
            break;

        case ATT_HANDLE_VALUE_IND:
            // Send confirmation for indication
            {
                uint8_t cfm = ATT_HANDLE_VALUE_CFM;
                att_send(conn_index, &cfm, 1);
            }
            att_handle_notification(client, data, len);
            break;

        default:
            printf("[ATT] Unknown opcode: 0x%02X\n", opcode);
            break;
    }
}

// ============================================================================
// WEAK CALLBACK
// ============================================================================

__attribute__((weak)) void att_on_hid_report(uint8_t conn_index, uint8_t report_id,
                                              const uint8_t* data, uint16_t len)
{
    printf("[ATT] HID Report: conn=%d id=%d len=%d\n", conn_index, report_id, len);
    printf("[ATT]   Data: ");
    for (int i = 0; i < len && i < 16; i++) {
        printf("%02X ", data[i]);
    }
    if (len > 16) printf("...");
    printf("\n");
}
