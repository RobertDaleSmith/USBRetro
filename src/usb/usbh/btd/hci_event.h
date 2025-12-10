// hci_event.h - HCI Event Definitions
// Bluetooth Host Controller Interface event codes and structures
//
// Reference: Bluetooth Core Specification v5.3, Vol 4, Part E

#ifndef HCI_EVENT_H
#define HCI_EVENT_H

#include <stdint.h>

// ============================================================================
// HCI EVENT CODES
// ============================================================================

#define HCI_EVENT_INQUIRY_COMPLETE              0x01
#define HCI_EVENT_INQUIRY_RESULT                0x02
#define HCI_EVENT_CONNECTION_COMPLETE           0x03
#define HCI_EVENT_CONNECTION_REQUEST            0x04
#define HCI_EVENT_DISCONNECTION_COMPLETE        0x05
#define HCI_EVENT_AUTH_COMPLETE                 0x06
#define HCI_EVENT_REMOTE_NAME_COMPLETE          0x07
#define HCI_EVENT_ENCRYPT_CHANGE                0x08
#define HCI_EVENT_CHANGE_CONN_LINK_KEY_COMPLETE 0x09
#define HCI_EVENT_READ_REMOTE_FEATURES_COMPLETE 0x0B
#define HCI_EVENT_READ_REMOTE_VERSION_COMPLETE  0x0C
#define HCI_EVENT_QOS_SETUP_COMPLETE            0x0D
#define HCI_EVENT_COMMAND_COMPLETE              0x0E
#define HCI_EVENT_COMMAND_STATUS                0x0F
#define HCI_EVENT_HARDWARE_ERROR                0x10
#define HCI_EVENT_ROLE_CHANGE                   0x12
#define HCI_EVENT_NUMBER_OF_COMPLETED_PACKETS   0x13
#define HCI_EVENT_MODE_CHANGE                   0x14
#define HCI_EVENT_PIN_CODE_REQUEST              0x16
#define HCI_EVENT_LINK_KEY_REQUEST              0x17
#define HCI_EVENT_LINK_KEY_NOTIFICATION         0x18
#define HCI_EVENT_DATA_BUFFER_OVERFLOW          0x1A
#define HCI_EVENT_MAX_SLOTS_CHANGE              0x1B
#define HCI_EVENT_PAGE_SCAN_REP_MODE_CHANGE     0x20
#define HCI_EVENT_INQUIRY_RESULT_WITH_RSSI      0x22
#define HCI_EVENT_READ_REMOTE_EXT_FEATURES      0x23
#define HCI_EVENT_SYNC_CONNECTION_COMPLETE      0x2C
#define HCI_EVENT_EXTENDED_INQUIRY_RESULT       0x2F
#define HCI_EVENT_IO_CAPABILITY_REQUEST         0x31
#define HCI_EVENT_IO_CAPABILITY_RESPONSE        0x32
#define HCI_EVENT_USER_CONFIRM_REQUEST          0x33
#define HCI_EVENT_USER_PASSKEY_REQUEST          0x34
#define HCI_EVENT_SIMPLE_PAIRING_COMPLETE       0x36
#define HCI_EVENT_LINK_SUPERVISION_TIMEOUT      0x38

// LE Meta Event
#define HCI_EVENT_LE_META                       0x3E

// ============================================================================
// HCI ERROR CODES
// ============================================================================

#define HCI_SUCCESS                             0x00
#define HCI_ERR_UNKNOWN_COMMAND                 0x01
#define HCI_ERR_UNKNOWN_CONNECTION              0x02
#define HCI_ERR_HARDWARE_FAILURE                0x03
#define HCI_ERR_PAGE_TIMEOUT                    0x04
#define HCI_ERR_AUTH_FAILURE                    0x05
#define HCI_ERR_PIN_OR_KEY_MISSING              0x06
#define HCI_ERR_MEMORY_EXCEEDED                 0x07
#define HCI_ERR_CONNECTION_TIMEOUT              0x08
#define HCI_ERR_CONNECTION_LIMIT_EXCEEDED       0x09
#define HCI_ERR_ACL_CONNECTION_EXISTS           0x0B
#define HCI_ERR_COMMAND_DISALLOWED              0x0C
#define HCI_ERR_REJECTED_LIMITED_RESOURCES      0x0D
#define HCI_ERR_REJECTED_SECURITY               0x0E
#define HCI_ERR_REJECTED_PERSONAL_DEVICE        0x0F
#define HCI_ERR_HOST_TIMEOUT                    0x10
#define HCI_ERR_UNSUPPORTED_FEATURE             0x11
#define HCI_ERR_INVALID_PARAMETERS              0x12
#define HCI_ERR_REMOTE_USER_TERMINATED          0x13
#define HCI_ERR_REMOTE_LOW_RESOURCES            0x14
#define HCI_ERR_REMOTE_POWER_OFF                0x15
#define HCI_ERR_LOCAL_HOST_TERMINATED           0x16
#define HCI_ERR_REPEATED_ATTEMPTS               0x17
#define HCI_ERR_PAIRING_NOT_ALLOWED             0x18
#define HCI_ERR_UNSUPPORTED_REMOTE_FEATURE      0x1A
#define HCI_ERR_UNSPECIFIED_ERROR               0x1F
#define HCI_ERR_ROLE_CHANGE_NOT_ALLOWED         0x21
#define HCI_ERR_LMP_RESPONSE_TIMEOUT            0x22
#define HCI_ERR_INSTANT_PASSED                  0x28
#define HCI_ERR_PAIRING_WITH_UNIT_KEY           0x29
#define HCI_ERR_CHANNEL_CLASSIFICATION          0x2E
#define HCI_ERR_CONTROLLER_BUSY                 0x3A
#define HCI_ERR_CONN_INTERVAL_UNACCEPTABLE      0x3B
#define HCI_ERR_ADVERTISING_TIMEOUT             0x3C
#define HCI_ERR_CONN_TERMINATED_MIC_FAILURE     0x3D
#define HCI_ERR_CONN_FAILED_TO_ESTABLISH        0x3E

// ============================================================================
// DISCONNECT REASON CODES
// ============================================================================

#define HCI_DISCONNECT_AUTH_FAILURE             0x05
#define HCI_DISCONNECT_REMOTE_USER              0x13
#define HCI_DISCONNECT_REMOTE_LOW_RESOURCES     0x14
#define HCI_DISCONNECT_REMOTE_POWER_OFF         0x15
#define HCI_DISCONNECT_LOCAL_HOST               0x16
#define HCI_DISCONNECT_REPEATED_ATTEMPTS        0x17

// ============================================================================
// LINK TYPES
// ============================================================================

#define HCI_LINK_TYPE_SCO                       0x00
#define HCI_LINK_TYPE_ACL                       0x01
#define HCI_LINK_TYPE_ESCO                      0x02

// ============================================================================
// HCI EVENT STRUCTURES
// ============================================================================

// Generic HCI event header
typedef struct __attribute__((packed)) {
    uint8_t event_code;
    uint8_t param_len;
    uint8_t params[];
} hci_event_t;

// Command Complete event
typedef struct __attribute__((packed)) {
    uint8_t  num_hci_cmd_packets;
    uint16_t opcode;
    uint8_t  return_params[];
} hci_event_cmd_complete_t;

// Command Status event
typedef struct __attribute__((packed)) {
    uint8_t  status;
    uint8_t  num_hci_cmd_packets;
    uint16_t opcode;
} hci_event_cmd_status_t;

// Connection Complete event
typedef struct __attribute__((packed)) {
    uint8_t  status;
    uint16_t handle;
    uint8_t  bd_addr[6];
    uint8_t  link_type;
    uint8_t  encryption_enabled;
} hci_event_conn_complete_t;

// Connection Request event
typedef struct __attribute__((packed)) {
    uint8_t bd_addr[6];
    uint8_t class_of_device[3];
    uint8_t link_type;
} hci_event_conn_request_t;

// Disconnection Complete event
typedef struct __attribute__((packed)) {
    uint8_t  status;
    uint16_t handle;
    uint8_t  reason;
} hci_event_disconn_complete_t;

// PIN Code Request event
typedef struct __attribute__((packed)) {
    uint8_t bd_addr[6];
} hci_event_pin_code_request_t;

// Link Key Request event
typedef struct __attribute__((packed)) {
    uint8_t bd_addr[6];
} hci_event_link_key_request_t;

// Link Key Notification event
typedef struct __attribute__((packed)) {
    uint8_t bd_addr[6];
    uint8_t link_key[16];
    uint8_t key_type;
} hci_event_link_key_notification_t;

// IO Capability Request event
typedef struct __attribute__((packed)) {
    uint8_t bd_addr[6];
} hci_event_io_cap_request_t;

// IO Capability Response event
typedef struct __attribute__((packed)) {
    uint8_t bd_addr[6];
    uint8_t io_capability;
    uint8_t oob_data_present;
    uint8_t auth_requirements;
} hci_event_io_cap_response_t;

// User Confirmation Request event
typedef struct __attribute__((packed)) {
    uint8_t  bd_addr[6];
    uint32_t numeric_value;
} hci_event_user_confirm_request_t;

// Simple Pairing Complete event
typedef struct __attribute__((packed)) {
    uint8_t status;
    uint8_t bd_addr[6];
} hci_event_simple_pairing_complete_t;

// Remote Name Request Complete event
typedef struct __attribute__((packed)) {
    uint8_t status;
    uint8_t bd_addr[6];
    char    remote_name[248];
} hci_event_remote_name_complete_t;

// Inquiry Result event (per device)
typedef struct __attribute__((packed)) {
    uint8_t  bd_addr[6];
    uint8_t  page_scan_rep_mode;
    uint8_t  reserved1;
    uint8_t  reserved2;
    uint8_t  class_of_device[3];
    uint16_t clock_offset;
} hci_inquiry_result_t;

// Inquiry Result with RSSI event (per device)
typedef struct __attribute__((packed)) {
    uint8_t  bd_addr[6];
    uint8_t  page_scan_rep_mode;
    uint8_t  reserved;
    uint8_t  class_of_device[3];
    uint16_t clock_offset;
    int8_t   rssi;
} hci_inquiry_result_rssi_t;

// Extended Inquiry Result event
typedef struct __attribute__((packed)) {
    uint8_t  num_responses;  // Always 1 for EIR
    uint8_t  bd_addr[6];
    uint8_t  page_scan_rep_mode;
    uint8_t  reserved;
    uint8_t  class_of_device[3];
    uint16_t clock_offset;
    int8_t   rssi;
    uint8_t  eir_data[240];
} hci_event_extended_inquiry_result_t;

// Number of Completed Packets event
typedef struct __attribute__((packed)) {
    uint8_t  num_handles;
    // Followed by: handle[num_handles], num_completed[num_handles]
} hci_event_num_completed_packets_t;

// Read BD_ADDR complete return params
typedef struct __attribute__((packed)) {
    uint8_t status;
    uint8_t bd_addr[6];
} hci_return_read_bd_addr_t;

// Read Local Version complete return params
typedef struct __attribute__((packed)) {
    uint8_t  status;
    uint8_t  hci_version;
    uint16_t hci_revision;
    uint8_t  lmp_version;
    uint16_t manufacturer;
    uint16_t lmp_subversion;
} hci_return_read_local_version_t;

// Read Buffer Size complete return params
typedef struct __attribute__((packed)) {
    uint8_t  status;
    uint16_t acl_data_packet_length;
    uint8_t  sco_data_packet_length;
    uint16_t total_num_acl_packets;
    uint16_t total_num_sco_packets;
} hci_return_read_buffer_size_t;

#endif // HCI_EVENT_H
