// btd.c - Bluetooth Dongle Driver Implementation
// USB Bluetooth dongle HCI layer for TinyUSB host
//
// Reference: USB_Host_Shield_2.0 BTD.cpp

#include "btd.h"
#include "btd_linkkey.h"
#include "l2cap.h"
#include "tusb.h"
#include "host/usbh_pvt.h"
#include <string.h>
#include <stdio.h>

// ============================================================================
// STATIC DATA
// ============================================================================

static btd_t btd_ctx;

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

static void btd_process_event(const uint8_t* data, uint16_t len);
static void btd_state_machine(void);
static btd_connection_t* btd_find_connection_by_handle(uint16_t handle);
static btd_connection_t* btd_find_connection_by_bdaddr(const uint8_t* bd_addr);
static btd_connection_t* btd_alloc_connection(void);

// ============================================================================
// BTD INITIALIZATION
// ============================================================================

void btd_init(void)
{
    memset(&btd_ctx, 0, sizeof(btd_ctx));
    btd_ctx.state = BTD_STATE_INIT;

    // Mark all connections as disconnected
    for (int i = 0; i < BTD_MAX_CONNECTIONS; i++) {
        btd_ctx.connections[i].state = BTD_CONN_DISCONNECTED;
        btd_ctx.connections[i].handle = 0xFFFF;
    }

    // Initialize link key storage
    btd_linkkey_init();

    // Initialize L2CAP layer
    l2cap_init();

    printf("[BTD] Initialized\n");
}

// ============================================================================
// BTD TASK (Main Loop)
// ============================================================================

void btd_task(void)
{
    // Handle link key flash saves (debounced)
    btd_linkkey_task();

    if (!btd_ctx.dongle_connected) {
        return;
    }

    // Run state machine
    btd_state_machine();
}

// ============================================================================
// STATE MACHINE
// ============================================================================

static void btd_state_machine(void)
{
    // Don't send new commands if we're waiting for a response
    if (btd_ctx.pending_cmd) {
        return;
    }

    switch (btd_ctx.state) {
        case BTD_STATE_INIT:
            printf("[BTD] Starting initialization...\n");
            btd_hci_reset();
            btd_ctx.state = BTD_STATE_RESET;
            break;

        case BTD_STATE_RESET:
            // Waiting for reset complete event
            break;

        case BTD_STATE_READ_BD_ADDR:
            btd_hci_read_bd_addr();
            break;

        case BTD_STATE_READ_VERSION:
            btd_hci_read_local_version();
            break;

        case BTD_STATE_READ_BUFFER_SIZE:
            btd_hci_read_buffer_size();
            break;

        case BTD_STATE_WRITE_NAME:
            btd_hci_write_local_name("USBRetro BT");
            break;

        case BTD_STATE_WRITE_COD:
            // Class of Device: Major=Computer, Minor=Desktop
            btd_hci_write_class_of_device(0x000104);
            break;

        case BTD_STATE_WRITE_SSP:
            btd_hci_write_simple_pairing_mode(true);
            break;

        case BTD_STATE_WRITE_SCAN:
            // Enable inquiry scan + page scan for pairing
            btd_hci_write_scan_enable(HCI_SCAN_INQUIRY_AND_PAGE);
            break;

        case BTD_STATE_RUNNING:
            // Normal operation - handle connections
            break;

        case BTD_STATE_ERROR:
            // Error state - do nothing
            break;
    }
}

// ============================================================================
// HCI COMMAND SENDING
// ============================================================================

bool btd_send_hci_cmd(uint16_t opcode, const uint8_t* params, uint8_t param_len)
{
    if (!btd_ctx.dongle_connected) {
        return false;
    }

    // Build HCI command packet
    btd_ctx.cmd_buf[0] = opcode & 0xFF;
    btd_ctx.cmd_buf[1] = (opcode >> 8) & 0xFF;
    btd_ctx.cmd_buf[2] = param_len;

    if (param_len > 0 && params != NULL) {
        memcpy(&btd_ctx.cmd_buf[3], params, param_len);
    }

    uint16_t total_len = 3 + param_len;

    // Send via control transfer (USB HCI command)
    // bmRequestType: 0x20 (Class, Host-to-Device, Interface)
    // bRequest: 0x00
    tusb_control_request_t request = {
        .bmRequestType_bit = {
            .recipient = TUSB_REQ_RCPT_INTERFACE,
            .type = TUSB_REQ_TYPE_CLASS,
            .direction = TUSB_DIR_OUT
        },
        .bRequest = 0,
        .wValue = 0,
        .wIndex = btd_ctx.itf_num,
        .wLength = total_len
    };

    tuh_xfer_t xfer = {
        .daddr = btd_ctx.dev_addr,
        .ep_addr = 0,
        .setup = &request,
        .buffer = btd_ctx.cmd_buf,
        .complete_cb = NULL,
        .user_data = 0
    };

    btd_ctx.pending_cmd = 1;

    bool result = tuh_control_xfer(&xfer);
    if (!result) {
        printf("[BTD] Failed to send HCI command 0x%04X\n", opcode);
        btd_ctx.pending_cmd = 0;
    }

    return result;
}

// ============================================================================
// SPECIFIC HCI COMMANDS
// ============================================================================

bool btd_hci_reset(void)
{
    printf("[BTD] Sending HCI_Reset\n");
    return btd_send_hci_cmd(HCI_RESET, NULL, 0);
}

bool btd_hci_read_bd_addr(void)
{
    printf("[BTD] Sending HCI_Read_BD_ADDR\n");
    return btd_send_hci_cmd(HCI_READ_BD_ADDR, NULL, 0);
}

bool btd_hci_read_local_version(void)
{
    printf("[BTD] Sending HCI_Read_Local_Version_Info\n");
    return btd_send_hci_cmd(HCI_READ_LOCAL_VERSION_INFO, NULL, 0);
}

bool btd_hci_read_buffer_size(void)
{
    printf("[BTD] Sending HCI_Read_Buffer_Size\n");
    return btd_send_hci_cmd(HCI_READ_BUFFER_SIZE, NULL, 0);
}

bool btd_hci_write_local_name(const char* name)
{
    printf("[BTD] Sending HCI_Write_Local_Name: %s\n", name);
    uint8_t params[248];
    memset(params, 0, sizeof(params));
    strncpy((char*)params, name, 247);
    return btd_send_hci_cmd(HCI_WRITE_LOCAL_NAME, params, 248);
}

bool btd_hci_write_class_of_device(uint32_t cod)
{
    printf("[BTD] Sending HCI_Write_Class_Of_Device: 0x%06lX\n", (unsigned long)cod);
    uint8_t params[3] = {
        cod & 0xFF,
        (cod >> 8) & 0xFF,
        (cod >> 16) & 0xFF
    };
    return btd_send_hci_cmd(HCI_WRITE_CLASS_OF_DEVICE, params, 3);
}

bool btd_hci_write_scan_enable(uint8_t mode)
{
    printf("[BTD] Sending HCI_Write_Scan_Enable: 0x%02X\n", mode);
    return btd_send_hci_cmd(HCI_WRITE_SCAN_ENABLE, &mode, 1);
}

bool btd_hci_write_simple_pairing_mode(bool enable)
{
    printf("[BTD] Sending HCI_Write_Simple_Pairing_Mode: %d\n", enable);
    uint8_t mode = enable ? 1 : 0;
    return btd_send_hci_cmd(HCI_WRITE_SIMPLE_PAIRING_MODE, &mode, 1);
}

bool btd_hci_accept_connection(const uint8_t* bd_addr, uint8_t role)
{
    printf("[BTD] Accepting connection from ");
    char addr_str[18];
    btd_bd_addr_to_str(bd_addr, addr_str);
    printf("%s\n", addr_str);

    hci_accept_conn_params_t params;
    memcpy(params.bd_addr, bd_addr, 6);
    params.role = role;
    return btd_send_hci_cmd(HCI_ACCEPT_CONNECTION_REQUEST, (uint8_t*)&params, sizeof(params));
}

bool btd_hci_reject_connection(const uint8_t* bd_addr, uint8_t reason)
{
    uint8_t params[7];
    memcpy(params, bd_addr, 6);
    params[6] = reason;
    return btd_send_hci_cmd(HCI_REJECT_CONNECTION_REQUEST, params, 7);
}

bool btd_hci_disconnect(uint16_t handle, uint8_t reason)
{
    printf("[BTD] Disconnecting handle 0x%04X\n", handle);
    hci_disconnect_params_t params = {
        .handle = handle,
        .reason = reason
    };
    return btd_send_hci_cmd(HCI_DISCONNECT, (uint8_t*)&params, sizeof(params));
}

bool btd_hci_pin_code_reply(const uint8_t* bd_addr, const char* pin, uint8_t pin_len)
{
    hci_pin_code_reply_params_t params;
    memset(&params, 0, sizeof(params));
    memcpy(params.bd_addr, bd_addr, 6);
    params.pin_length = pin_len;
    memcpy(params.pin_code, pin, pin_len);
    return btd_send_hci_cmd(HCI_PIN_CODE_REQUEST_REPLY, (uint8_t*)&params, sizeof(params));
}

bool btd_hci_pin_code_neg_reply(const uint8_t* bd_addr)
{
    return btd_send_hci_cmd(HCI_PIN_CODE_REQUEST_NEG_REPLY, bd_addr, 6);
}

bool btd_hci_link_key_reply(const uint8_t* bd_addr, const uint8_t* link_key)
{
    hci_link_key_reply_params_t params;
    memcpy(params.bd_addr, bd_addr, 6);
    memcpy(params.link_key, link_key, 16);
    return btd_send_hci_cmd(HCI_LINK_KEY_REQUEST_REPLY, (uint8_t*)&params, sizeof(params));
}

bool btd_hci_link_key_neg_reply(const uint8_t* bd_addr)
{
    return btd_send_hci_cmd(HCI_LINK_KEY_REQUEST_NEG_REPLY, bd_addr, 6);
}

bool btd_hci_user_confirm_reply(const uint8_t* bd_addr)
{
    return btd_send_hci_cmd(HCI_USER_CONFIRM_REQUEST_REPLY, bd_addr, 6);
}

bool btd_hci_io_capability_reply(const uint8_t* bd_addr)
{
    hci_io_cap_reply_params_t params;
    memcpy(params.bd_addr, bd_addr, 6);
    params.io_capability = HCI_IO_CAP_NO_INPUT_NO_OUTPUT;
    params.oob_data_present = 0;
    params.auth_requirements = HCI_AUTH_MITM_NOT_REQUIRED;
    return btd_send_hci_cmd(HCI_IO_CAPABILITY_REQUEST_REPLY, (uint8_t*)&params, sizeof(params));
}

bool btd_hci_remote_name_request(const uint8_t* bd_addr)
{
    char addr_str[18];
    btd_bd_addr_to_str(bd_addr, addr_str);
    printf("[BTD] Requesting remote name from %s\n", addr_str);

    hci_remote_name_request_params_t params;
    memcpy(params.bd_addr, bd_addr, 6);
    params.page_scan_rep_mode = 0x01;  // R1
    params.reserved = 0;
    params.clock_offset = 0;
    return btd_send_hci_cmd(HCI_REMOTE_NAME_REQUEST, (uint8_t*)&params, sizeof(params));
}

// ============================================================================
// ACL DATA SENDING
// ============================================================================

bool btd_send_acl_data(uint16_t handle, uint8_t pb_flag, uint8_t bc_flag,
                       const uint8_t* data, uint16_t len)
{
    if (!btd_ctx.dongle_connected || btd_ctx.acl_credits == 0) {
        return false;
    }

    // Build ACL header
    uint16_t hdr = (handle & 0x0FFF) | ((pb_flag & 0x03) << 12) | ((bc_flag & 0x03) << 14);

    btd_ctx.acl_out_buf[0] = hdr & 0xFF;
    btd_ctx.acl_out_buf[1] = (hdr >> 8) & 0xFF;
    btd_ctx.acl_out_buf[2] = len & 0xFF;
    btd_ctx.acl_out_buf[3] = (len >> 8) & 0xFF;

    if (len > 0 && data != NULL) {
        memcpy(&btd_ctx.acl_out_buf[4], data, len);
    }

    // Send via bulk OUT
    btd_ctx.acl_credits--;
    return usbh_edpt_xfer(btd_ctx.dev_addr, btd_ctx.ep_acl_out, btd_ctx.acl_out_buf, 4 + len);
}

// ============================================================================
// HCI EVENT PROCESSING
// ============================================================================

static void btd_process_event(const uint8_t* data, uint16_t len)
{
    if (len < 2) return;

    const hci_event_t* evt = (const hci_event_t*)data;

    switch (evt->event_code) {
        case HCI_EVENT_COMMAND_COMPLETE: {
            const hci_event_cmd_complete_t* cc = (const hci_event_cmd_complete_t*)evt->params;
            btd_ctx.pending_cmd = 0;

            printf("[BTD] Command Complete: opcode=0x%04X\n", cc->opcode);

            // Handle specific command completions
            switch (cc->opcode) {
                case HCI_RESET:
                    printf("[BTD] Reset complete\n");
                    btd_ctx.state = BTD_STATE_READ_BD_ADDR;
                    break;

                case HCI_READ_BD_ADDR: {
                    const hci_return_read_bd_addr_t* ret = (const hci_return_read_bd_addr_t*)cc->return_params;
                    if (ret->status == HCI_SUCCESS) {
                        memcpy(btd_ctx.bd_addr, ret->bd_addr, 6);
                        char addr_str[18];
                        btd_bd_addr_to_str(btd_ctx.bd_addr, addr_str);
                        printf("[BTD] Local BD_ADDR: %s\n", addr_str);
                    }
                    btd_ctx.state = BTD_STATE_READ_VERSION;
                    break;
                }

                case HCI_READ_LOCAL_VERSION_INFO: {
                    const hci_return_read_local_version_t* ret = (const hci_return_read_local_version_t*)cc->return_params;
                    if (ret->status == HCI_SUCCESS) {
                        btd_ctx.hci_version = ret->hci_version;
                        btd_ctx.manufacturer = ret->manufacturer;
                        printf("[BTD] HCI Version: %d, Manufacturer: 0x%04X\n",
                               ret->hci_version, ret->manufacturer);
                    }
                    btd_ctx.state = BTD_STATE_READ_BUFFER_SIZE;
                    break;
                }

                case HCI_READ_BUFFER_SIZE: {
                    const hci_return_read_buffer_size_t* ret = (const hci_return_read_buffer_size_t*)cc->return_params;
                    if (ret->status == HCI_SUCCESS) {
                        btd_ctx.acl_mtu = ret->acl_data_packet_length;
                        btd_ctx.acl_credits = ret->total_num_acl_packets;
                        printf("[BTD] ACL MTU: %d, Credits: %d\n",
                               btd_ctx.acl_mtu, btd_ctx.acl_credits);
                    }
                    btd_ctx.state = BTD_STATE_WRITE_NAME;
                    break;
                }

                case HCI_WRITE_LOCAL_NAME:
                    btd_ctx.state = BTD_STATE_WRITE_COD;
                    break;

                case HCI_WRITE_CLASS_OF_DEVICE:
                    btd_ctx.state = BTD_STATE_WRITE_SSP;
                    break;

                case HCI_WRITE_SIMPLE_PAIRING_MODE:
                    btd_ctx.state = BTD_STATE_WRITE_SCAN;
                    break;

                case HCI_WRITE_SCAN_ENABLE:
                    btd_ctx.scan_enabled = true;
                    btd_ctx.state = BTD_STATE_RUNNING;
                    printf("[BTD] Initialization complete - Ready for connections\n");
                    break;
            }
            break;
        }

        case HCI_EVENT_COMMAND_STATUS: {
            const hci_event_cmd_status_t* cs = (const hci_event_cmd_status_t*)evt->params;
            if (cs->status != HCI_SUCCESS) {
                printf("[BTD] Command Status error: 0x%02X for opcode 0x%04X\n",
                       cs->status, cs->opcode);
            }
            btd_ctx.pending_cmd = 0;
            break;
        }

        case HCI_EVENT_CONNECTION_REQUEST: {
            const hci_event_conn_request_t* req = (const hci_event_conn_request_t*)evt->params;
            char addr_str[18];
            btd_bd_addr_to_str(req->bd_addr, addr_str);
            printf("[BTD] Connection request from %s, COD: %02X%02X%02X, Type: %d\n",
                   addr_str, req->class_of_device[2], req->class_of_device[1],
                   req->class_of_device[0], req->link_type);

            // Accept ACL connections
            if (req->link_type == HCI_LINK_TYPE_ACL) {
                btd_hci_accept_connection(req->bd_addr, HCI_ROLE_SLAVE);
            }
            break;
        }

        case HCI_EVENT_CONNECTION_COMPLETE: {
            const hci_event_conn_complete_t* cc = (const hci_event_conn_complete_t*)evt->params;
            char addr_str[18];
            btd_bd_addr_to_str(cc->bd_addr, addr_str);

            if (cc->status == HCI_SUCCESS) {
                printf("[BTD] Connection complete: %s, handle=0x%04X\n", addr_str, cc->handle);

                // Allocate connection slot
                btd_connection_t* conn = btd_alloc_connection();
                if (conn) {
                    conn->state = BTD_CONN_CONNECTED;
                    memcpy(conn->bd_addr, cc->bd_addr, 6);
                    conn->handle = cc->handle;
                    conn->name[0] = '\0';  // Clear name, will be filled by remote name request
                    btd_ctx.num_connections++;

                    // Request remote name to identify the device
                    btd_hci_remote_name_request(cc->bd_addr);

                    // Notify higher layer
                    uint8_t idx = conn - btd_ctx.connections;
                    btd_on_connection(idx);
                }
            } else {
                printf("[BTD] Connection failed: %s, status=0x%02X\n", addr_str, cc->status);
            }
            break;
        }

        case HCI_EVENT_DISCONNECTION_COMPLETE: {
            const hci_event_disconn_complete_t* dc = (const hci_event_disconn_complete_t*)evt->params;
            printf("[BTD] Disconnection: handle=0x%04X, reason=0x%02X\n", dc->handle, dc->reason);

            btd_connection_t* conn = btd_find_connection_by_handle(dc->handle);
            if (conn) {
                uint8_t idx = conn - btd_ctx.connections;
                conn->state = BTD_CONN_DISCONNECTED;
                conn->handle = 0xFFFF;
                btd_ctx.num_connections--;

                btd_on_disconnection(idx);
            }
            break;
        }

        case HCI_EVENT_PIN_CODE_REQUEST: {
            const hci_event_pin_code_request_t* pin = (const hci_event_pin_code_request_t*)evt->params;
            char addr_str[18];
            btd_bd_addr_to_str(pin->bd_addr, addr_str);
            printf("[BTD] PIN code request from %s\n", addr_str);

            // Use default PIN "0000" for legacy pairing
            btd_hci_pin_code_reply(pin->bd_addr, "0000", 4);
            break;
        }

        case HCI_EVENT_LINK_KEY_REQUEST: {
            const hci_event_link_key_request_t* lk = (const hci_event_link_key_request_t*)evt->params;
            char addr_str[18];
            btd_bd_addr_to_str(lk->bd_addr, addr_str);
            printf("[BTD] Link key request from %s\n", addr_str);

            // Look up stored link key
            const uint8_t* stored_key = btd_linkkey_find(lk->bd_addr);
            if (stored_key) {
                printf("[BTD] Found stored link key, replying\n");
                btd_hci_link_key_reply(lk->bd_addr, stored_key);
            } else {
                printf("[BTD] No stored link key, triggering new pairing\n");
                btd_hci_link_key_neg_reply(lk->bd_addr);
            }
            break;
        }

        case HCI_EVENT_LINK_KEY_NOTIFICATION: {
            const hci_event_link_key_notification_t* lkn = (const hci_event_link_key_notification_t*)evt->params;
            char addr_str[18];
            btd_bd_addr_to_str(lkn->bd_addr, addr_str);
            printf("[BTD] Link key notification from %s, type=%d\n", addr_str, lkn->key_type);

            // Store link key for future reconnection
            btd_linkkey_store(lkn->bd_addr, lkn->link_key, lkn->key_type);
            break;
        }

        case HCI_EVENT_IO_CAPABILITY_REQUEST: {
            const hci_event_io_cap_request_t* io = (const hci_event_io_cap_request_t*)evt->params;
            char addr_str[18];
            btd_bd_addr_to_str(io->bd_addr, addr_str);
            printf("[BTD] IO capability request from %s\n", addr_str);

            btd_hci_io_capability_reply(io->bd_addr);
            break;
        }

        case HCI_EVENT_USER_CONFIRM_REQUEST: {
            const hci_event_user_confirm_request_t* uc = (const hci_event_user_confirm_request_t*)evt->params;
            char addr_str[18];
            btd_bd_addr_to_str(uc->bd_addr, addr_str);
            printf("[BTD] User confirmation request from %s, value=%lu\n",
                   addr_str, (unsigned long)uc->numeric_value);

            // Auto-accept (no display/keyboard)
            btd_hci_user_confirm_reply(uc->bd_addr);
            break;
        }

        case HCI_EVENT_NUMBER_OF_COMPLETED_PACKETS: {
            // Replenish ACL credits
            const hci_event_num_completed_packets_t* ncp = (const hci_event_num_completed_packets_t*)evt->params;
            const uint8_t* ptr = (const uint8_t*)(ncp + 1);
            for (int i = 0; i < ncp->num_handles; i++) {
                // uint16_t handle = ptr[0] | (ptr[1] << 8);
                uint16_t completed = ptr[2] | (ptr[3] << 8);
                btd_ctx.acl_credits += completed;
                ptr += 4;
            }
            break;
        }

        case HCI_EVENT_REMOTE_NAME_COMPLETE: {
            const hci_event_remote_name_complete_t* rn = (const hci_event_remote_name_complete_t*)evt->params;
            char addr_str[18];
            btd_bd_addr_to_str(rn->bd_addr, addr_str);

            if (rn->status == HCI_SUCCESS) {
                printf("[BTD] Remote name from %s: %s\n", addr_str, rn->remote_name);

                // Store name in connection record
                btd_connection_t* conn = btd_find_connection_by_bdaddr(rn->bd_addr);
                if (conn) {
                    strncpy(conn->name, rn->remote_name, BTD_MAX_NAME_LEN - 1);
                    conn->name[BTD_MAX_NAME_LEN - 1] = '\0';
                }
            } else {
                printf("[BTD] Remote name request failed for %s: 0x%02X\n", addr_str, rn->status);
            }
            break;
        }

        default:
            printf("[BTD] Unhandled event: 0x%02X\n", evt->event_code);
            break;
    }
}

// ============================================================================
// CONNECTION MANAGEMENT HELPERS
// ============================================================================

static btd_connection_t* btd_find_connection_by_handle(uint16_t handle)
{
    for (int i = 0; i < BTD_MAX_CONNECTIONS; i++) {
        if (btd_ctx.connections[i].handle == handle) {
            return &btd_ctx.connections[i];
        }
    }
    return NULL;
}

static btd_connection_t* btd_find_connection_by_bdaddr(const uint8_t* bd_addr)
{
    for (int i = 0; i < BTD_MAX_CONNECTIONS; i++) {
        if (memcmp(btd_ctx.connections[i].bd_addr, bd_addr, 6) == 0) {
            return &btd_ctx.connections[i];
        }
    }
    return NULL;
}

static btd_connection_t* btd_alloc_connection(void)
{
    for (int i = 0; i < BTD_MAX_CONNECTIONS; i++) {
        if (btd_ctx.connections[i].state == BTD_CONN_DISCONNECTED) {
            return &btd_ctx.connections[i];
        }
    }
    return NULL;
}

// ============================================================================
// PUBLIC API
// ============================================================================

bool btd_is_ready(void)
{
    return btd_ctx.dongle_connected && btd_ctx.state == BTD_STATE_RUNNING;
}

uint8_t btd_get_connection_count(void)
{
    return btd_ctx.num_connections;
}

const btd_connection_t* btd_get_connection(uint8_t index)
{
    if (index >= BTD_MAX_CONNECTIONS) {
        return NULL;
    }
    return &btd_ctx.connections[index];
}

// Mutable accessor for internal use (btd_glue.c)
btd_connection_t* btd_get_connection_mutable(uint8_t index)
{
    if (index >= BTD_MAX_CONNECTIONS) {
        return NULL;
    }
    return &btd_ctx.connections[index];
}

void btd_set_pairing_mode(bool enable)
{
    btd_ctx.pairing_mode = enable;
    // Update scan mode
    if (btd_ctx.state == BTD_STATE_RUNNING) {
        btd_hci_write_scan_enable(enable ? HCI_SCAN_INQUIRY_AND_PAGE : HCI_SCAN_PAGE_ONLY);
    }
}

bool btd_is_pairing_mode(void)
{
    return btd_ctx.pairing_mode;
}

void btd_disconnect(uint8_t index)
{
    if (index >= BTD_MAX_CONNECTIONS) return;

    btd_connection_t* conn = &btd_ctx.connections[index];
    if (conn->state != BTD_CONN_DISCONNECTED && conn->handle != 0xFFFF) {
        btd_hci_disconnect(conn->handle, HCI_DISCONNECT_LOCAL_HOST);
    }
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

void btd_bd_addr_to_str(const uint8_t* bd_addr, char* str)
{
    sprintf(str, "%02X:%02X:%02X:%02X:%02X:%02X",
            bd_addr[5], bd_addr[4], bd_addr[3],
            bd_addr[2], bd_addr[1], bd_addr[0]);
}

void btd_print_state(void)
{
    const char* state_names[] = {
        "INIT", "RESET", "READ_BD_ADDR", "READ_VERSION", "READ_BUFFER_SIZE",
        "WRITE_NAME", "WRITE_COD", "WRITE_SSP", "WRITE_SCAN", "RUNNING", "ERROR"
    };

    printf("[BTD] State: %s\n", state_names[btd_ctx.state]);
    printf("[BTD] Dongle connected: %d\n", btd_ctx.dongle_connected);
    printf("[BTD] Connections: %d\n", btd_ctx.num_connections);
}

// ============================================================================
// TINYUSB CLASS DRIVER IMPLEMENTATION
// ============================================================================

bool btd_driver_init(void)
{
    btd_init();
    return true;
}

bool btd_driver_deinit(void)
{
    return true;
}

bool btd_driver_open(uint8_t rhport, uint8_t dev_addr,
                     tusb_desc_interface_t const* desc_itf, uint16_t max_len)
{
    (void)rhport;

    const tusb_desc_interface_t* itf = desc_itf;

    // Check for Bluetooth class (0xE0/01/01)
    if (itf->bInterfaceClass != USB_CLASS_WIRELESS_CTRL ||
        itf->bInterfaceSubClass != USB_SUBCLASS_RF ||
        itf->bInterfaceProtocol != USB_PROTOCOL_BLUETOOTH) {
        return false;
    }

    printf("[BTD] Bluetooth dongle detected at dev_addr=%d\n", dev_addr);

    btd_ctx.dev_addr = dev_addr;
    btd_ctx.itf_num = itf->bInterfaceNumber;

    // Parse endpoint descriptors
    uint16_t drv_len = sizeof(tusb_desc_interface_t);
    const uint8_t* p_desc = (const uint8_t*)desc_itf + drv_len;

    while (drv_len < max_len) {
        if (p_desc[1] == TUSB_DESC_ENDPOINT) {
            const tusb_desc_endpoint_t* ep = (const tusb_desc_endpoint_t*)p_desc;

            if (ep->bmAttributes.xfer == TUSB_XFER_INTERRUPT &&
                tu_edpt_dir(ep->bEndpointAddress) == TUSB_DIR_IN) {
                // HCI Event endpoint (Interrupt IN)
                btd_ctx.ep_evt = ep->bEndpointAddress;
                printf("[BTD] Event EP: 0x%02X\n", btd_ctx.ep_evt);
            }
            else if (ep->bmAttributes.xfer == TUSB_XFER_BULK) {
                if (tu_edpt_dir(ep->bEndpointAddress) == TUSB_DIR_IN) {
                    // ACL IN endpoint
                    btd_ctx.ep_acl_in = ep->bEndpointAddress;
                    printf("[BTD] ACL IN EP: 0x%02X\n", btd_ctx.ep_acl_in);
                } else {
                    // ACL OUT endpoint
                    btd_ctx.ep_acl_out = ep->bEndpointAddress;
                    printf("[BTD] ACL OUT EP: 0x%02X\n", btd_ctx.ep_acl_out);
                }
            }

            // Open the endpoint
            if (!tuh_edpt_open(dev_addr, ep)) {
                printf("[BTD] Failed to open endpoint 0x%02X\n", ep->bEndpointAddress);
            }
        }

        drv_len += p_desc[0];
        p_desc += p_desc[0];
    }

    return true;
}

bool btd_driver_set_config(uint8_t dev_addr, uint8_t itf_num)
{
    (void)itf_num;

    printf("[BTD] Configuration set for dev_addr=%d\n", dev_addr);

    btd_ctx.dongle_connected = true;
    btd_ctx.state = BTD_STATE_INIT;

    // Start receiving HCI events
    usbh_edpt_xfer(dev_addr, btd_ctx.ep_evt, btd_ctx.evt_buf, sizeof(btd_ctx.evt_buf));

    // Start receiving ACL data
    usbh_edpt_xfer(dev_addr, btd_ctx.ep_acl_in, btd_ctx.acl_in_buf, sizeof(btd_ctx.acl_in_buf));

    // Tell USBH we're done with enumeration
    usbh_driver_set_config_complete(dev_addr, btd_ctx.itf_num);

    return true;
}

bool btd_driver_xfer_cb(uint8_t dev_addr, uint8_t ep_addr,
                        xfer_result_t result, uint32_t xferred_bytes)
{
    (void)dev_addr;

    if (result != XFER_RESULT_SUCCESS) {
        printf("[BTD] Transfer failed on EP 0x%02X: result=%d\n", ep_addr, result);
        return false;
    }

    if (ep_addr == btd_ctx.ep_evt) {
        // HCI Event received
        if (xferred_bytes > 0) {
            btd_process_event(btd_ctx.evt_buf, xferred_bytes);
        }
        // Continue receiving events
        usbh_edpt_xfer(dev_addr, btd_ctx.ep_evt, btd_ctx.evt_buf, sizeof(btd_ctx.evt_buf));
    }
    else if (ep_addr == btd_ctx.ep_acl_in) {
        // ACL data received
        if (xferred_bytes > 4) {
            // Parse ACL header
            uint16_t hdr = btd_ctx.acl_in_buf[0] | (btd_ctx.acl_in_buf[1] << 8);
            uint16_t handle = hdr & 0x0FFF;
            uint16_t data_len = btd_ctx.acl_in_buf[2] | (btd_ctx.acl_in_buf[3] << 8);

            btd_connection_t* conn = btd_find_connection_by_handle(handle);
            if (conn) {
                uint8_t idx = conn - btd_ctx.connections;
                btd_on_acl_data(idx, &btd_ctx.acl_in_buf[4], data_len);
            }
        }
        // Continue receiving ACL data
        usbh_edpt_xfer(dev_addr, btd_ctx.ep_acl_in, btd_ctx.acl_in_buf, sizeof(btd_ctx.acl_in_buf));
    }

    return true;
}

void btd_driver_close(uint8_t dev_addr)
{
    printf("[BTD] Dongle disconnected (dev_addr=%d)\n", dev_addr);

    btd_ctx.dongle_connected = false;
    btd_ctx.state = BTD_STATE_INIT;
    btd_ctx.num_connections = 0;

    // Mark all connections as disconnected
    for (int i = 0; i < BTD_MAX_CONNECTIONS; i++) {
        if (btd_ctx.connections[i].state != BTD_CONN_DISCONNECTED) {
            btd_ctx.connections[i].state = BTD_CONN_DISCONNECTED;
            btd_on_disconnection(i);
        }
    }
}

// ============================================================================
// TINYUSB CLASS DRIVER STRUCTURE
// ============================================================================

const usbh_class_driver_t usbh_btd_driver = {
    .name = "BTD",
    .init = btd_driver_init,
    .deinit = btd_driver_deinit,
    .open = btd_driver_open,
    .set_config = btd_driver_set_config,
    .xfer_cb = btd_driver_xfer_cb,
    .close = btd_driver_close,
};

// ============================================================================
// WEAK CALLBACK IMPLEMENTATIONS (Override in higher layers)
// ============================================================================

__attribute__((weak)) void btd_on_connection(uint8_t conn_index)
{
    printf("[BTD] Connection %d established (weak handler)\n", conn_index);
}

__attribute__((weak)) void btd_on_disconnection(uint8_t conn_index)
{
    printf("[BTD] Connection %d lost (weak handler)\n", conn_index);
}

__attribute__((weak)) void btd_on_acl_data(uint8_t conn_index, const uint8_t* data, uint16_t len)
{
    printf("[BTD] ACL data on connection %d: %d bytes (weak handler)\n", conn_index, len);
}
