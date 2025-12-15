// l2cap.h - L2CAP (Logical Link Control and Adaptation Protocol)
// Bluetooth L2CAP layer for HID channel management
//
// Reference: Bluetooth Core Specification v5.3, Vol 3, Part A

#ifndef L2CAP_H
#define L2CAP_H

#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// L2CAP CONSTANTS
// ============================================================================

// Fixed Channel IDs (CIDs)
#define L2CAP_CID_NULL              0x0000  // Null identifier
#define L2CAP_CID_SIGNALING         0x0001  // L2CAP signaling channel
#define L2CAP_CID_CONNECTIONLESS    0x0002  // Connectionless reception
#define L2CAP_CID_AMP_MANAGER       0x0003  // AMP Manager Protocol

// BLE Fixed Channel IDs
#define L2CAP_CID_ATT               0x0004  // Attribute Protocol (ATT)
#define L2CAP_CID_LE_SIGNALING      0x0005  // LE L2CAP signaling
#define L2CAP_CID_SM                0x0006  // Security Manager

// Dynamic CID range (0x0040 - 0xFFFF)
#define L2CAP_CID_DYNAMIC_START     0x0040
#define L2CAP_CID_DYNAMIC_END       0xFFFF

// Protocol/Service Multiplexer (PSM) values
#define L2CAP_PSM_SDP               0x0001  // Service Discovery Protocol
#define L2CAP_PSM_RFCOMM            0x0003  // RFCOMM
#define L2CAP_PSM_HID_CONTROL       0x0011  // HID Control
#define L2CAP_PSM_HID_INTERRUPT     0x0013  // HID Interrupt

// ============================================================================
// L2CAP SIGNALING COMMANDS
// ============================================================================

#define L2CAP_CMD_REJECT                0x01
#define L2CAP_CMD_CONNECTION_REQUEST    0x02
#define L2CAP_CMD_CONNECTION_RESPONSE   0x03
#define L2CAP_CMD_CONFIGURE_REQUEST     0x04
#define L2CAP_CMD_CONFIGURE_RESPONSE    0x05
#define L2CAP_CMD_DISCONNECTION_REQUEST 0x06
#define L2CAP_CMD_DISCONNECTION_RESPONSE 0x07
#define L2CAP_CMD_ECHO_REQUEST          0x08
#define L2CAP_CMD_ECHO_RESPONSE         0x09
#define L2CAP_CMD_INFO_REQUEST          0x0A
#define L2CAP_CMD_INFO_RESPONSE         0x0B

// ============================================================================
// L2CAP CONNECTION RESULT CODES
// ============================================================================

#define L2CAP_CONN_SUCCESS              0x0000
#define L2CAP_CONN_PENDING              0x0001
#define L2CAP_CONN_REFUSED_PSM          0x0002  // PSM not supported
#define L2CAP_CONN_REFUSED_SECURITY     0x0003  // Security block
#define L2CAP_CONN_REFUSED_RESOURCES    0x0004  // No resources

// Connection pending status
#define L2CAP_CONN_PENDING_NONE         0x0000
#define L2CAP_CONN_PENDING_AUTH         0x0001
#define L2CAP_CONN_PENDING_AUTHZ        0x0002

// ============================================================================
// L2CAP CONFIGURATION RESULT CODES
// ============================================================================

#define L2CAP_CFG_SUCCESS               0x0000
#define L2CAP_CFG_UNACCEPTABLE          0x0001
#define L2CAP_CFG_REJECTED              0x0002
#define L2CAP_CFG_UNKNOWN_OPTIONS       0x0003
#define L2CAP_CFG_PENDING               0x0004
#define L2CAP_CFG_FLOW_SPEC_REJECTED    0x0005

// ============================================================================
// L2CAP CONFIGURATION OPTIONS
// ============================================================================

#define L2CAP_CFG_OPT_MTU               0x01
#define L2CAP_CFG_OPT_FLUSH_TIMEOUT     0x02
#define L2CAP_CFG_OPT_QOS               0x03
#define L2CAP_CFG_OPT_RETRANS_FC        0x04  // Retransmission & Flow Control
#define L2CAP_CFG_OPT_FCS               0x05  // Frame Check Sequence

// Default MTU
#define L2CAP_DEFAULT_MTU               672
#define L2CAP_MIN_MTU                   48

// ============================================================================
// L2CAP INFO REQUEST TYPES
// ============================================================================

#define L2CAP_INFO_CONNECTIONLESS_MTU   0x0001
#define L2CAP_INFO_EXTENDED_FEATURES    0x0002
#define L2CAP_INFO_FIXED_CHANNELS       0x0003

// Info response results
#define L2CAP_INFO_SUCCESS              0x0000
#define L2CAP_INFO_NOT_SUPPORTED        0x0001

// ============================================================================
// L2CAP PACKET STRUCTURES
// ============================================================================

// L2CAP basic header (for all packets)
typedef struct __attribute__((packed)) {
    uint16_t length;        // Payload length (excluding header)
    uint16_t cid;           // Channel ID
} l2cap_header_t;

// L2CAP signaling command header
typedef struct __attribute__((packed)) {
    uint8_t  code;          // Command code
    uint8_t  identifier;    // Transaction identifier
    uint16_t length;        // Data length
} l2cap_signal_header_t;

// Connection Request
typedef struct __attribute__((packed)) {
    uint16_t psm;           // Protocol/Service Multiplexer
    uint16_t source_cid;    // Source channel ID
} l2cap_conn_request_t;

// Connection Response
typedef struct __attribute__((packed)) {
    uint16_t dest_cid;      // Destination channel ID
    uint16_t source_cid;    // Source channel ID
    uint16_t result;        // Result code
    uint16_t status;        // Status (if pending)
} l2cap_conn_response_t;

// Configuration Request
typedef struct __attribute__((packed)) {
    uint16_t dest_cid;      // Destination channel ID
    uint16_t flags;         // Flags (continuation)
    // Options follow...
} l2cap_config_request_t;

// Configuration Response
typedef struct __attribute__((packed)) {
    uint16_t source_cid;    // Source channel ID
    uint16_t flags;         // Flags
    uint16_t result;        // Result code
    // Options follow...
} l2cap_config_response_t;

// Configuration Option header
typedef struct __attribute__((packed)) {
    uint8_t type;           // Option type
    uint8_t length;         // Option data length
} l2cap_config_option_t;

// MTU Option
typedef struct __attribute__((packed)) {
    uint8_t  type;          // L2CAP_CFG_OPT_MTU
    uint8_t  length;        // 2
    uint16_t mtu;           // MTU value
} l2cap_config_mtu_t;

// Disconnection Request
typedef struct __attribute__((packed)) {
    uint16_t dest_cid;      // Destination channel ID
    uint16_t source_cid;    // Source channel ID
} l2cap_disconn_request_t;

// Disconnection Response
typedef struct __attribute__((packed)) {
    uint16_t dest_cid;      // Destination channel ID
    uint16_t source_cid;    // Source channel ID
} l2cap_disconn_response_t;

// Information Request
typedef struct __attribute__((packed)) {
    uint16_t info_type;     // Information type
} l2cap_info_request_t;

// Information Response
typedef struct __attribute__((packed)) {
    uint16_t info_type;     // Information type
    uint16_t result;        // Result code
    // Data follows if successful...
} l2cap_info_response_t;

// Command Reject
typedef struct __attribute__((packed)) {
    uint16_t reason;        // Rejection reason
    // Optional data follows...
} l2cap_cmd_reject_t;

// Reject reasons
#define L2CAP_REJECT_NOT_UNDERSTOOD     0x0000
#define L2CAP_REJECT_MTU_EXCEEDED       0x0001
#define L2CAP_REJECT_INVALID_CID        0x0002

// ============================================================================
// L2CAP CHANNEL STATE
// ============================================================================

typedef enum {
    L2CAP_CHANNEL_CLOSED = 0,
    L2CAP_CHANNEL_WAIT_CONNECT,         // Waiting for connection response
    L2CAP_CHANNEL_WAIT_CONNECT_RSP,     // Received connect, sending response
    L2CAP_CHANNEL_CONFIG,               // Configuring channel
    L2CAP_CHANNEL_OPEN,                 // Channel open and ready
    L2CAP_CHANNEL_WAIT_DISCONNECT,      // Waiting for disconnect response
} l2cap_channel_state_t;

// L2CAP channel info
typedef struct {
    l2cap_channel_state_t state;
    uint16_t local_cid;         // Our CID
    uint16_t remote_cid;        // Remote's CID
    uint16_t psm;               // Protocol/Service Multiplexer
    uint16_t local_mtu;         // Our MTU
    uint16_t remote_mtu;        // Remote's MTU
    bool     local_config_done; // We sent config response
    bool     remote_config_done;// We received config response
    uint8_t  conn_index;        // BTD connection index
} l2cap_channel_t;

// ============================================================================
// L2CAP CONFIGURATION
// ============================================================================

#define L2CAP_MAX_CHANNELS      8   // Max L2CAP channels (2 per BT connection * 4 connections)

// ============================================================================
// L2CAP API
// ============================================================================

// Initialize L2CAP layer
void l2cap_init(void);

// Process incoming ACL data (called from BTD)
void l2cap_process_acl_data(uint8_t conn_index, const uint8_t* data, uint16_t len);

// Open a new L2CAP channel
// Returns local CID on success, 0 on failure
uint16_t l2cap_connect(uint8_t conn_index, uint16_t psm);

// Close an L2CAP channel
void l2cap_disconnect(uint16_t local_cid);

// Send data on an L2CAP channel
bool l2cap_send(uint16_t local_cid, const uint8_t* data, uint16_t len);

// Send data on a BLE fixed channel (ATT, LE Signaling, SM)
// Uses HCI handle directly instead of dynamic channel
bool l2cap_send_ble(uint16_t hci_handle, uint16_t cid, const uint8_t* data, uint16_t len);

// Get channel by local CID
l2cap_channel_t* l2cap_get_channel(uint16_t local_cid);

// Get channel by PSM and connection index
l2cap_channel_t* l2cap_get_channel_by_psm(uint8_t conn_index, uint16_t psm);

// Check if channel is open
bool l2cap_is_channel_open(uint16_t local_cid);

// ============================================================================
// L2CAP CALLBACKS (implemented by higher layers)
// ============================================================================

// Called when an L2CAP channel is opened
extern void l2cap_on_channel_open(uint16_t local_cid, uint16_t psm, uint8_t conn_index);

// Called when an L2CAP channel is closed
extern void l2cap_on_channel_closed(uint16_t local_cid);

// Called when data is received on an L2CAP channel
extern void l2cap_on_data(uint16_t local_cid, const uint8_t* data, uint16_t len);

// Called when BLE data is received on a fixed channel (ATT, SM)
extern void l2cap_on_ble_data(uint8_t conn_index, uint16_t cid, const uint8_t* data, uint16_t len);

#endif // L2CAP_H
