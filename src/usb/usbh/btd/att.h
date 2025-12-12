// att.h - Attribute Protocol (ATT) for BLE
// Used by GATT for service/characteristic discovery and data transfer

#ifndef ATT_H
#define ATT_H

#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// ATT OPCODES
// ============================================================================

#define ATT_ERROR_RSP                   0x01
#define ATT_EXCHANGE_MTU_REQ            0x02
#define ATT_EXCHANGE_MTU_RSP            0x03
#define ATT_FIND_INFORMATION_REQ        0x04
#define ATT_FIND_INFORMATION_RSP        0x05
#define ATT_FIND_BY_TYPE_VALUE_REQ      0x06
#define ATT_FIND_BY_TYPE_VALUE_RSP      0x07
#define ATT_READ_BY_TYPE_REQ            0x08
#define ATT_READ_BY_TYPE_RSP            0x09
#define ATT_READ_REQ                    0x0A
#define ATT_READ_RSP                    0x0B
#define ATT_READ_BLOB_REQ               0x0C
#define ATT_READ_BLOB_RSP               0x0D
#define ATT_READ_MULTIPLE_REQ           0x0E
#define ATT_READ_MULTIPLE_RSP           0x0F
#define ATT_READ_BY_GROUP_TYPE_REQ      0x10
#define ATT_READ_BY_GROUP_TYPE_RSP      0x11
#define ATT_WRITE_REQ                   0x12
#define ATT_WRITE_RSP                   0x13
#define ATT_WRITE_CMD                   0x52  // No response
#define ATT_PREPARE_WRITE_REQ           0x16
#define ATT_PREPARE_WRITE_RSP           0x17
#define ATT_EXECUTE_WRITE_REQ           0x18
#define ATT_EXECUTE_WRITE_RSP           0x19
#define ATT_HANDLE_VALUE_NTF            0x1B  // Notification
#define ATT_HANDLE_VALUE_IND            0x1D  // Indication
#define ATT_HANDLE_VALUE_CFM            0x1E  // Indication confirmation

// ============================================================================
// ATT ERROR CODES
// ============================================================================

#define ATT_ERROR_INVALID_HANDLE        0x01
#define ATT_ERROR_READ_NOT_PERMITTED    0x02
#define ATT_ERROR_WRITE_NOT_PERMITTED   0x03
#define ATT_ERROR_INVALID_PDU           0x04
#define ATT_ERROR_INSUFF_AUTHENTICATION 0x05
#define ATT_ERROR_REQUEST_NOT_SUPPORTED 0x06
#define ATT_ERROR_INVALID_OFFSET        0x07
#define ATT_ERROR_INSUFF_AUTHORIZATION  0x08
#define ATT_ERROR_PREPARE_QUEUE_FULL    0x09
#define ATT_ERROR_ATTRIBUTE_NOT_FOUND   0x0A
#define ATT_ERROR_ATTRIBUTE_NOT_LONG    0x0B
#define ATT_ERROR_INSUFF_ENC_KEY_SIZE   0x0C
#define ATT_ERROR_INVALID_ATTR_LENGTH   0x0D
#define ATT_ERROR_UNLIKELY_ERROR        0x0E
#define ATT_ERROR_INSUFF_ENCRYPTION     0x0F
#define ATT_ERROR_UNSUPPORTED_GROUP     0x10
#define ATT_ERROR_INSUFF_RESOURCES      0x11

// ============================================================================
// GATT UUID DEFINITIONS (16-bit standard UUIDs)
// ============================================================================

#define GATT_UUID_PRIMARY_SERVICE       0x2800
#define GATT_UUID_SECONDARY_SERVICE     0x2801
#define GATT_UUID_INCLUDE               0x2802
#define GATT_UUID_CHARACTERISTIC        0x2803
#define GATT_UUID_CHAR_EXT_PROPS        0x2900
#define GATT_UUID_CHAR_USER_DESC        0x2901
#define GATT_UUID_CCCD                  0x2902  // Client Characteristic Configuration
#define GATT_UUID_SCCD                  0x2903  // Server Characteristic Configuration
#define GATT_UUID_CHAR_FORMAT           0x2904
#define GATT_UUID_CHAR_AGGREGATE        0x2905

// Standard Service UUIDs
#define GATT_UUID_GAP_SERVICE           0x1800  // Generic Access
#define GATT_UUID_GATT_SERVICE          0x1801  // Generic Attribute
#define GATT_UUID_DEVICE_INFO_SERVICE   0x180A  // Device Information
#define GATT_UUID_BATTERY_SERVICE       0x180F  // Battery Service
#define GATT_UUID_HID_SERVICE           0x1812  // Human Interface Device

// HID Service Characteristic UUIDs
#define GATT_UUID_HID_INFORMATION       0x2A4A
#define GATT_UUID_HID_REPORT_MAP        0x2A4B
#define GATT_UUID_HID_CONTROL_POINT     0x2A4C
#define GATT_UUID_HID_REPORT            0x2A4D
#define GATT_UUID_HID_PROTOCOL_MODE     0x2A4E
#define GATT_UUID_HID_BOOT_KEYBOARD_IN  0x2A22
#define GATT_UUID_HID_BOOT_KEYBOARD_OUT 0x2A32
#define GATT_UUID_HID_BOOT_MOUSE_IN     0x2A33

// Report Reference Descriptor
#define GATT_UUID_REPORT_REFERENCE      0x2908

// ============================================================================
// ATT PDU STRUCTURES
// ============================================================================

// ATT Error Response
typedef struct __attribute__((packed)) {
    uint8_t  opcode;          // ATT_ERROR_RSP
    uint8_t  req_opcode;      // Opcode that caused error
    uint16_t handle;          // Attribute handle
    uint8_t  error_code;      // Error code
} att_error_rsp_t;

// ATT Exchange MTU Request/Response
typedef struct __attribute__((packed)) {
    uint8_t  opcode;          // ATT_EXCHANGE_MTU_REQ/RSP
    uint16_t mtu;             // Client/Server Rx MTU
} att_exchange_mtu_t;

// ATT Read By Group Type Request (for service discovery)
typedef struct __attribute__((packed)) {
    uint8_t  opcode;          // ATT_READ_BY_GROUP_TYPE_REQ
    uint16_t start_handle;
    uint16_t end_handle;
    uint16_t uuid;            // 16-bit UUID (e.g., 0x2800 for primary service)
} att_read_by_group_type_req_t;

// ATT Read By Type Request (for characteristic discovery)
typedef struct __attribute__((packed)) {
    uint8_t  opcode;          // ATT_READ_BY_TYPE_REQ
    uint16_t start_handle;
    uint16_t end_handle;
    uint16_t uuid;            // 16-bit UUID
} att_read_by_type_req_t;

// ATT Find Information Request (for descriptor discovery)
typedef struct __attribute__((packed)) {
    uint8_t  opcode;          // ATT_FIND_INFORMATION_REQ
    uint16_t start_handle;
    uint16_t end_handle;
} att_find_info_req_t;

// ATT Read Request
typedef struct __attribute__((packed)) {
    uint8_t  opcode;          // ATT_READ_REQ
    uint16_t handle;
} att_read_req_t;

// ATT Write Request/Command
typedef struct __attribute__((packed)) {
    uint8_t  opcode;          // ATT_WRITE_REQ or ATT_WRITE_CMD
    uint16_t handle;
    // Followed by value data
} att_write_t;

// ATT Handle Value Notification/Indication
typedef struct __attribute__((packed)) {
    uint8_t  opcode;          // ATT_HANDLE_VALUE_NTF or ATT_HANDLE_VALUE_IND
    uint16_t handle;
    // Followed by value data
} att_handle_value_t;

// ============================================================================
// GATT CHARACTERISTIC PROPERTIES
// ============================================================================

#define GATT_CHAR_PROP_BROADCAST        0x01
#define GATT_CHAR_PROP_READ             0x02
#define GATT_CHAR_PROP_WRITE_NO_RSP     0x04
#define GATT_CHAR_PROP_WRITE            0x08
#define GATT_CHAR_PROP_NOTIFY           0x10
#define GATT_CHAR_PROP_INDICATE         0x20
#define GATT_CHAR_PROP_AUTH_WRITE       0x40
#define GATT_CHAR_PROP_EXTENDED         0x80

// CCCD Values
#define GATT_CCCD_NONE                  0x0000
#define GATT_CCCD_NOTIFICATION          0x0001
#define GATT_CCCD_INDICATION            0x0002

// ============================================================================
// ATT CONTEXT AND STATE
// ============================================================================

#define ATT_MAX_SERVICES        8
#define ATT_MAX_CHARACTERISTICS 32
#define ATT_DEFAULT_MTU         23
#define ATT_MAX_MTU             517

// Discovered service
typedef struct {
    uint16_t start_handle;
    uint16_t end_handle;
    uint16_t uuid;            // 16-bit UUID (we only care about standard services)
} att_service_t;

// Discovered characteristic
typedef struct {
    uint16_t handle;          // Characteristic declaration handle
    uint16_t value_handle;    // Value handle
    uint16_t cccd_handle;     // CCCD handle (0 if not found)
    uint8_t  properties;
    uint16_t uuid;            // 16-bit UUID
    uint8_t  report_id;       // For HID Report characteristics
    uint8_t  report_type;     // For HID Report characteristics (1=Input, 2=Output, 3=Feature)
} att_characteristic_t;

// ATT client state for a connection
typedef enum {
    ATT_STATE_IDLE,
    ATT_STATE_MTU_EXCHANGE,
    ATT_STATE_DISCOVER_SERVICES,
    ATT_STATE_DISCOVER_CHARACTERISTICS,
    ATT_STATE_DISCOVER_DESCRIPTORS,
    ATT_STATE_READ_REPORT_MAP,
    ATT_STATE_ENABLE_NOTIFICATIONS,
    ATT_STATE_READY
} att_state_t;

typedef struct {
    uint8_t  conn_index;      // BTD connection index
    uint16_t handle;          // HCI connection handle
    uint16_t mtu;             // Negotiated MTU
    att_state_t state;

    // Discovery state
    uint16_t discover_start;
    uint16_t discover_end;
    uint8_t  current_service;
    uint8_t  current_char;

    // Discovered data
    att_service_t services[ATT_MAX_SERVICES];
    uint8_t  num_services;
    att_characteristic_t characteristics[ATT_MAX_CHARACTERISTICS];
    uint8_t  num_characteristics;

    // HID-specific
    uint16_t hid_service_start;
    uint16_t hid_service_end;
    uint8_t  report_map[512];
    uint16_t report_map_len;
} att_client_t;

// ============================================================================
// ATT API
// ============================================================================

// Initialize ATT layer
void att_init(void);

// Called when a BLE connection is established
void att_on_connect(uint8_t conn_index, uint16_t handle);

// Called when a BLE connection is disconnected
void att_on_disconnect(uint8_t conn_index);

// Process incoming ATT data (called from L2CAP on CID 0x0004)
void att_process_data(uint8_t conn_index, const uint8_t* data, uint16_t len);

// Send ATT data
bool att_send(uint8_t conn_index, const uint8_t* data, uint16_t len);

// Start GATT discovery
void att_start_discovery(uint8_t conn_index);

// ATT requests
bool att_exchange_mtu(uint8_t conn_index, uint16_t mtu);
bool att_read_by_group_type(uint8_t conn_index, uint16_t start, uint16_t end, uint16_t uuid);
bool att_read_by_type(uint8_t conn_index, uint16_t start, uint16_t end, uint16_t uuid);
bool att_find_information(uint8_t conn_index, uint16_t start, uint16_t end);
bool att_read(uint8_t conn_index, uint16_t handle);
bool att_write(uint8_t conn_index, uint16_t handle, const uint8_t* data, uint16_t len);
bool att_write_cmd(uint8_t conn_index, uint16_t handle, const uint8_t* data, uint16_t len);

// Callback when HID report notification is received
extern void att_on_hid_report(uint8_t conn_index, uint8_t report_id,
                               const uint8_t* data, uint16_t len);

#endif // ATT_H
