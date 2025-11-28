// l2cap.c - L2CAP Implementation
// Bluetooth L2CAP layer for HID channel management
//
// Reference: Bluetooth Core Specification v5.3, Vol 3, Part A

#include "l2cap.h"
#include "btd.h"
#include <string.h>
#include <stdio.h>

// ============================================================================
// STATIC DATA
// ============================================================================

static l2cap_channel_t channels[L2CAP_MAX_CHANNELS];
static uint16_t next_local_cid = L2CAP_CID_DYNAMIC_START;
static uint8_t next_signal_id = 1;

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

static void l2cap_process_signaling(uint8_t conn_index, const uint8_t* data, uint16_t len);
static void l2cap_process_channel_data(uint16_t cid, const uint8_t* data, uint16_t len);
static l2cap_channel_t* l2cap_alloc_channel(void);
static l2cap_channel_t* l2cap_find_channel_by_local_cid(uint16_t cid);
static l2cap_channel_t* l2cap_find_channel_by_remote_cid(uint8_t conn_index, uint16_t cid);
static bool l2cap_send_signaling(uint8_t conn_index, uint8_t code, uint8_t id,
                                  const uint8_t* data, uint16_t len);
static void l2cap_send_config_request(l2cap_channel_t* ch);
static void l2cap_send_config_response(l2cap_channel_t* ch, uint8_t id, uint16_t result);

// ============================================================================
// INITIALIZATION
// ============================================================================

void l2cap_init(void)
{
    memset(channels, 0, sizeof(channels));
    next_local_cid = L2CAP_CID_DYNAMIC_START;
    next_signal_id = 1;

    printf("[L2CAP] Initialized\n");
}

// ============================================================================
// ACL DATA PROCESSING
// ============================================================================

void l2cap_process_acl_data(uint8_t conn_index, const uint8_t* data, uint16_t len)
{
    if (len < sizeof(l2cap_header_t)) {
        printf("[L2CAP] Packet too short\n");
        return;
    }

    const l2cap_header_t* hdr = (const l2cap_header_t*)data;
    uint16_t payload_len = hdr->length;
    uint16_t cid = hdr->cid;

    if (len < sizeof(l2cap_header_t) + payload_len) {
        printf("[L2CAP] Incomplete packet (len=%d, expected=%d)\n",
               len, (int)(sizeof(l2cap_header_t) + payload_len));
        return;
    }

    const uint8_t* payload = data + sizeof(l2cap_header_t);

    // Route based on CID
    if (cid == L2CAP_CID_SIGNALING) {
        l2cap_process_signaling(conn_index, payload, payload_len);
    } else if (cid >= L2CAP_CID_DYNAMIC_START) {
        l2cap_process_channel_data(cid, payload, payload_len);
    } else {
        printf("[L2CAP] Unhandled CID: 0x%04X\n", cid);
    }
}

// ============================================================================
// SIGNALING CHANNEL PROCESSING
// ============================================================================

static void l2cap_process_signaling(uint8_t conn_index, const uint8_t* data, uint16_t len)
{
    while (len >= sizeof(l2cap_signal_header_t)) {
        const l2cap_signal_header_t* sig = (const l2cap_signal_header_t*)data;
        uint16_t cmd_len = sig->length;

        if (len < sizeof(l2cap_signal_header_t) + cmd_len) {
            printf("[L2CAP] Incomplete signaling command\n");
            break;
        }

        const uint8_t* cmd_data = data + sizeof(l2cap_signal_header_t);

        switch (sig->code) {
            case L2CAP_CMD_CONNECTION_REQUEST: {
                const l2cap_conn_request_t* req = (const l2cap_conn_request_t*)cmd_data;
                printf("[L2CAP] Connection request: PSM=0x%04X, SCID=0x%04X\n",
                       req->psm, req->source_cid);

                // Allocate a channel for incoming connection
                l2cap_channel_t* ch = l2cap_alloc_channel();
                uint16_t result = L2CAP_CONN_REFUSED_RESOURCES;
                uint16_t dcid = 0;

                if (ch) {
                    // Accept HID PSMs
                    if (req->psm == L2CAP_PSM_HID_CONTROL ||
                        req->psm == L2CAP_PSM_HID_INTERRUPT) {
                        ch->state = L2CAP_CHANNEL_CONFIG;
                        ch->local_cid = next_local_cid++;
                        ch->remote_cid = req->source_cid;
                        ch->psm = req->psm;
                        ch->local_mtu = L2CAP_DEFAULT_MTU;
                        ch->remote_mtu = L2CAP_DEFAULT_MTU;
                        ch->conn_index = conn_index;
                        result = L2CAP_CONN_SUCCESS;
                        dcid = ch->local_cid;
                        printf("[L2CAP] Accepted connection, DCID=0x%04X\n", dcid);
                    } else {
                        printf("[L2CAP] Rejecting PSM 0x%04X\n", req->psm);
                        result = L2CAP_CONN_REFUSED_PSM;
                        memset(ch, 0, sizeof(*ch));
                    }
                }

                // Send response
                l2cap_conn_response_t resp = {
                    .dest_cid = dcid,
                    .source_cid = req->source_cid,
                    .result = result,
                    .status = 0
                };
                l2cap_send_signaling(conn_index, L2CAP_CMD_CONNECTION_RESPONSE,
                                     sig->identifier, (uint8_t*)&resp, sizeof(resp));
                break;
            }

            case L2CAP_CMD_CONNECTION_RESPONSE: {
                const l2cap_conn_response_t* resp = (const l2cap_conn_response_t*)cmd_data;
                printf("[L2CAP] Connection response: DCID=0x%04X, SCID=0x%04X, result=%d\n",
                       resp->dest_cid, resp->source_cid, resp->result);

                l2cap_channel_t* ch = l2cap_find_channel_by_local_cid(resp->source_cid);
                if (ch && ch->state == L2CAP_CHANNEL_WAIT_CONNECT) {
                    if (resp->result == L2CAP_CONN_SUCCESS) {
                        ch->remote_cid = resp->dest_cid;
                        ch->state = L2CAP_CHANNEL_CONFIG;
                        // Send our configuration
                        l2cap_send_config_request(ch);
                    } else if (resp->result == L2CAP_CONN_PENDING) {
                        // Stay in wait state
                    } else {
                        printf("[L2CAP] Connection rejected: %d\n", resp->result);
                        ch->state = L2CAP_CHANNEL_CLOSED;
                    }
                }
                break;
            }

            case L2CAP_CMD_CONFIGURE_REQUEST: {
                const l2cap_config_request_t* req = (const l2cap_config_request_t*)cmd_data;
                printf("[L2CAP] Configure request: DCID=0x%04X, flags=0x%04X\n",
                       req->dest_cid, req->flags);

                l2cap_channel_t* ch = l2cap_find_channel_by_local_cid(req->dest_cid);
                if (ch) {
                    // Parse options
                    const uint8_t* opt_ptr = cmd_data + sizeof(l2cap_config_request_t);
                    uint16_t opt_len = cmd_len - sizeof(l2cap_config_request_t);

                    while (opt_len >= 2) {
                        const l2cap_config_option_t* opt = (const l2cap_config_option_t*)opt_ptr;
                        if (opt_len < 2 + opt->length) break;

                        if (opt->type == L2CAP_CFG_OPT_MTU && opt->length >= 2) {
                            uint16_t mtu = opt_ptr[2] | (opt_ptr[3] << 8);
                            ch->remote_mtu = mtu;
                            printf("[L2CAP] Remote MTU: %d\n", mtu);
                        }

                        opt_ptr += 2 + opt->length;
                        opt_len -= 2 + opt->length;
                    }

                    // Send response accepting the configuration
                    l2cap_send_config_response(ch, sig->identifier, L2CAP_CFG_SUCCESS);
                    ch->remote_config_done = true;

                    // If we haven't sent our config yet, do it now
                    if (!ch->local_config_done) {
                        l2cap_send_config_request(ch);
                    }

                    // Check if channel is fully configured
                    if (ch->local_config_done && ch->remote_config_done) {
                        ch->state = L2CAP_CHANNEL_OPEN;
                        printf("[L2CAP] Channel 0x%04X open (PSM=0x%04X)\n",
                               ch->local_cid, ch->psm);
                        l2cap_on_channel_open(ch->local_cid, ch->psm, ch->conn_index);
                    }
                }
                break;
            }

            case L2CAP_CMD_CONFIGURE_RESPONSE: {
                const l2cap_config_response_t* resp = (const l2cap_config_response_t*)cmd_data;
                printf("[L2CAP] Configure response: SCID=0x%04X, result=%d\n",
                       resp->source_cid, resp->result);

                l2cap_channel_t* ch = l2cap_find_channel_by_local_cid(resp->source_cid);
                if (ch && resp->result == L2CAP_CFG_SUCCESS) {
                    ch->local_config_done = true;

                    // Check if channel is fully configured
                    if (ch->local_config_done && ch->remote_config_done) {
                        ch->state = L2CAP_CHANNEL_OPEN;
                        printf("[L2CAP] Channel 0x%04X open (PSM=0x%04X)\n",
                               ch->local_cid, ch->psm);
                        l2cap_on_channel_open(ch->local_cid, ch->psm, ch->conn_index);
                    }
                }
                break;
            }

            case L2CAP_CMD_DISCONNECTION_REQUEST: {
                const l2cap_disconn_request_t* req = (const l2cap_disconn_request_t*)cmd_data;
                printf("[L2CAP] Disconnect request: DCID=0x%04X, SCID=0x%04X\n",
                       req->dest_cid, req->source_cid);

                l2cap_channel_t* ch = l2cap_find_channel_by_local_cid(req->dest_cid);
                if (ch) {
                    // Send response
                    l2cap_disconn_response_t resp = {
                        .dest_cid = req->dest_cid,
                        .source_cid = req->source_cid
                    };
                    l2cap_send_signaling(conn_index, L2CAP_CMD_DISCONNECTION_RESPONSE,
                                         sig->identifier, (uint8_t*)&resp, sizeof(resp));

                    // Close channel
                    uint16_t cid = ch->local_cid;
                    memset(ch, 0, sizeof(*ch));
                    l2cap_on_channel_closed(cid);
                }
                break;
            }

            case L2CAP_CMD_DISCONNECTION_RESPONSE: {
                const l2cap_disconn_response_t* resp = (const l2cap_disconn_response_t*)cmd_data;
                printf("[L2CAP] Disconnect response: DCID=0x%04X, SCID=0x%04X\n",
                       resp->dest_cid, resp->source_cid);

                l2cap_channel_t* ch = l2cap_find_channel_by_local_cid(resp->source_cid);
                if (ch) {
                    uint16_t cid = ch->local_cid;
                    memset(ch, 0, sizeof(*ch));
                    l2cap_on_channel_closed(cid);
                }
                break;
            }

            case L2CAP_CMD_INFO_REQUEST: {
                const l2cap_info_request_t* req = (const l2cap_info_request_t*)cmd_data;
                printf("[L2CAP] Info request: type=0x%04X\n", req->info_type);

                // Respond with "not supported" for simplicity
                uint8_t resp_buf[4];
                resp_buf[0] = req->info_type & 0xFF;
                resp_buf[1] = (req->info_type >> 8) & 0xFF;
                resp_buf[2] = L2CAP_INFO_NOT_SUPPORTED & 0xFF;
                resp_buf[3] = (L2CAP_INFO_NOT_SUPPORTED >> 8) & 0xFF;
                l2cap_send_signaling(conn_index, L2CAP_CMD_INFO_RESPONSE,
                                     sig->identifier, resp_buf, 4);
                break;
            }

            case L2CAP_CMD_ECHO_REQUEST: {
                printf("[L2CAP] Echo request\n");
                // Echo back the data
                l2cap_send_signaling(conn_index, L2CAP_CMD_ECHO_RESPONSE,
                                     sig->identifier, cmd_data, cmd_len);
                break;
            }

            default:
                printf("[L2CAP] Unhandled signaling command: 0x%02X\n", sig->code);
                break;
        }

        // Move to next command
        data += sizeof(l2cap_signal_header_t) + cmd_len;
        len -= sizeof(l2cap_signal_header_t) + cmd_len;
    }
}

// ============================================================================
// CHANNEL DATA PROCESSING
// ============================================================================

static void l2cap_process_channel_data(uint16_t cid, const uint8_t* data, uint16_t len)
{
    l2cap_channel_t* ch = l2cap_find_channel_by_local_cid(cid);
    if (!ch) {
        printf("[L2CAP] Data for unknown CID: 0x%04X\n", cid);
        return;
    }

    if (ch->state != L2CAP_CHANNEL_OPEN) {
        printf("[L2CAP] Data on non-open channel: 0x%04X\n", cid);
        return;
    }

    // Pass to higher layer
    l2cap_on_data(cid, data, len);
}

// ============================================================================
// CHANNEL MANAGEMENT
// ============================================================================

static l2cap_channel_t* l2cap_alloc_channel(void)
{
    for (int i = 0; i < L2CAP_MAX_CHANNELS; i++) {
        if (channels[i].state == L2CAP_CHANNEL_CLOSED) {
            return &channels[i];
        }
    }
    return NULL;
}

static l2cap_channel_t* l2cap_find_channel_by_local_cid(uint16_t cid)
{
    for (int i = 0; i < L2CAP_MAX_CHANNELS; i++) {
        if (channels[i].local_cid == cid && channels[i].state != L2CAP_CHANNEL_CLOSED) {
            return &channels[i];
        }
    }
    return NULL;
}

static l2cap_channel_t* l2cap_find_channel_by_remote_cid(uint8_t conn_index, uint16_t cid)
{
    for (int i = 0; i < L2CAP_MAX_CHANNELS; i++) {
        if (channels[i].remote_cid == cid &&
            channels[i].conn_index == conn_index &&
            channels[i].state != L2CAP_CHANNEL_CLOSED) {
            return &channels[i];
        }
    }
    return NULL;
}

l2cap_channel_t* l2cap_get_channel(uint16_t local_cid)
{
    return l2cap_find_channel_by_local_cid(local_cid);
}

l2cap_channel_t* l2cap_get_channel_by_psm(uint8_t conn_index, uint16_t psm)
{
    for (int i = 0; i < L2CAP_MAX_CHANNELS; i++) {
        if (channels[i].psm == psm &&
            channels[i].conn_index == conn_index &&
            channels[i].state != L2CAP_CHANNEL_CLOSED) {
            return &channels[i];
        }
    }
    return NULL;
}

bool l2cap_is_channel_open(uint16_t local_cid)
{
    l2cap_channel_t* ch = l2cap_find_channel_by_local_cid(local_cid);
    return ch && ch->state == L2CAP_CHANNEL_OPEN;
}

// ============================================================================
// SIGNALING HELPERS
// ============================================================================

static bool l2cap_send_signaling(uint8_t conn_index, uint8_t code, uint8_t id,
                                  const uint8_t* data, uint16_t len)
{
    // Get BTD connection handle
    const btd_connection_t* conn = btd_get_connection(conn_index);
    if (!conn || conn->state == BTD_CONN_DISCONNECTED) {
        return false;
    }

    // Build L2CAP signaling packet
    uint8_t buf[64];
    l2cap_header_t* l2cap_hdr = (l2cap_header_t*)buf;
    l2cap_signal_header_t* sig_hdr = (l2cap_signal_header_t*)(buf + sizeof(l2cap_header_t));

    l2cap_hdr->length = sizeof(l2cap_signal_header_t) + len;
    l2cap_hdr->cid = L2CAP_CID_SIGNALING;

    sig_hdr->code = code;
    sig_hdr->identifier = id;
    sig_hdr->length = len;

    if (len > 0 && data) {
        memcpy(buf + sizeof(l2cap_header_t) + sizeof(l2cap_signal_header_t), data, len);
    }

    uint16_t total_len = sizeof(l2cap_header_t) + sizeof(l2cap_signal_header_t) + len;
    return btd_send_acl_data(conn->handle, 0x02, 0x00, buf, total_len);
}

static void l2cap_send_config_request(l2cap_channel_t* ch)
{
    // Build config request with MTU option
    uint8_t buf[8];
    l2cap_config_request_t* req = (l2cap_config_request_t*)buf;
    l2cap_config_mtu_t* mtu_opt = (l2cap_config_mtu_t*)(buf + sizeof(l2cap_config_request_t));

    req->dest_cid = ch->remote_cid;
    req->flags = 0;

    mtu_opt->type = L2CAP_CFG_OPT_MTU;
    mtu_opt->length = 2;
    mtu_opt->mtu = ch->local_mtu;

    l2cap_send_signaling(ch->conn_index, L2CAP_CMD_CONFIGURE_REQUEST,
                         next_signal_id++, buf, sizeof(buf));
}

static void l2cap_send_config_response(l2cap_channel_t* ch, uint8_t id, uint16_t result)
{
    uint8_t buf[6];
    l2cap_config_response_t* resp = (l2cap_config_response_t*)buf;

    resp->source_cid = ch->remote_cid;
    resp->flags = 0;
    resp->result = result;

    l2cap_send_signaling(ch->conn_index, L2CAP_CMD_CONFIGURE_RESPONSE,
                         id, buf, sizeof(buf));
}

// ============================================================================
// PUBLIC API
// ============================================================================

uint16_t l2cap_connect(uint8_t conn_index, uint16_t psm)
{
    const btd_connection_t* conn = btd_get_connection(conn_index);
    if (!conn || conn->state == BTD_CONN_DISCONNECTED) {
        return 0;
    }

    l2cap_channel_t* ch = l2cap_alloc_channel();
    if (!ch) {
        printf("[L2CAP] No free channels\n");
        return 0;
    }

    ch->state = L2CAP_CHANNEL_WAIT_CONNECT;
    ch->local_cid = next_local_cid++;
    ch->remote_cid = 0;
    ch->psm = psm;
    ch->local_mtu = L2CAP_DEFAULT_MTU;
    ch->remote_mtu = L2CAP_DEFAULT_MTU;
    ch->conn_index = conn_index;
    ch->local_config_done = false;
    ch->remote_config_done = false;

    // Send connection request
    l2cap_conn_request_t req = {
        .psm = psm,
        .source_cid = ch->local_cid
    };

    printf("[L2CAP] Connecting PSM=0x%04X, SCID=0x%04X\n", psm, ch->local_cid);
    l2cap_send_signaling(conn_index, L2CAP_CMD_CONNECTION_REQUEST,
                         next_signal_id++, (uint8_t*)&req, sizeof(req));

    return ch->local_cid;
}

void l2cap_disconnect(uint16_t local_cid)
{
    l2cap_channel_t* ch = l2cap_find_channel_by_local_cid(local_cid);
    if (!ch) {
        return;
    }

    if (ch->state == L2CAP_CHANNEL_OPEN || ch->state == L2CAP_CHANNEL_CONFIG) {
        ch->state = L2CAP_CHANNEL_WAIT_DISCONNECT;

        l2cap_disconn_request_t req = {
            .dest_cid = ch->remote_cid,
            .source_cid = ch->local_cid
        };

        l2cap_send_signaling(ch->conn_index, L2CAP_CMD_DISCONNECTION_REQUEST,
                             next_signal_id++, (uint8_t*)&req, sizeof(req));
    } else {
        memset(ch, 0, sizeof(*ch));
    }
}

bool l2cap_send(uint16_t local_cid, const uint8_t* data, uint16_t len)
{
    l2cap_channel_t* ch = l2cap_find_channel_by_local_cid(local_cid);
    if (!ch || ch->state != L2CAP_CHANNEL_OPEN) {
        return false;
    }

    const btd_connection_t* conn = btd_get_connection(ch->conn_index);
    if (!conn || conn->state == BTD_CONN_DISCONNECTED) {
        return false;
    }

    // Build L2CAP data packet
    uint8_t buf[256];
    if (len + sizeof(l2cap_header_t) > sizeof(buf)) {
        printf("[L2CAP] Data too large: %d\n", len);
        return false;
    }

    l2cap_header_t* hdr = (l2cap_header_t*)buf;
    hdr->length = len;
    hdr->cid = ch->remote_cid;
    memcpy(buf + sizeof(l2cap_header_t), data, len);

    return btd_send_acl_data(conn->handle, 0x02, 0x00, buf, sizeof(l2cap_header_t) + len);
}

// ============================================================================
// WEAK CALLBACK IMPLEMENTATIONS
// ============================================================================

__attribute__((weak)) void l2cap_on_channel_open(uint16_t local_cid, uint16_t psm, uint8_t conn_index)
{
    printf("[L2CAP] Channel 0x%04X opened (PSM=0x%04X, conn=%d) - weak handler\n",
           local_cid, psm, conn_index);
}

__attribute__((weak)) void l2cap_on_channel_closed(uint16_t local_cid)
{
    printf("[L2CAP] Channel 0x%04X closed - weak handler\n", local_cid);
}

__attribute__((weak)) void l2cap_on_data(uint16_t local_cid, const uint8_t* data, uint16_t len)
{
    printf("[L2CAP] Data on channel 0x%04X: %d bytes - weak handler\n", local_cid, len);
}
