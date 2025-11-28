// hci_cmd.h - HCI Command Definitions
// Bluetooth Host Controller Interface command opcodes and structures
//
// Reference: Bluetooth Core Specification v5.3, Vol 4, Part E

#ifndef HCI_CMD_H
#define HCI_CMD_H

#include <stdint.h>

// ============================================================================
// HCI PACKET TYPES
// ============================================================================

#define HCI_COMMAND_PKT         0x01
#define HCI_ACLDATA_PKT         0x02
#define HCI_SCODATA_PKT         0x03
#define HCI_EVENT_PKT           0x04

// ============================================================================
// HCI OPCODE CONSTRUCTION
// ============================================================================

// OGF (Opcode Group Field) - upper 6 bits
#define HCI_OGF_LINK_CONTROL    0x01
#define HCI_OGF_LINK_POLICY     0x02
#define HCI_OGF_CTRL_BASEBAND   0x03
#define HCI_OGF_INFO_PARAM      0x04
#define HCI_OGF_STATUS_PARAM    0x05
#define HCI_OGF_LE_CTRL         0x08
#define HCI_OGF_VENDOR          0x3F

// Construct opcode from OGF and OCF
#define HCI_OPCODE(ogf, ocf)    ((uint16_t)((ogf) << 10) | (ocf))

// ============================================================================
// LINK CONTROL COMMANDS (OGF 0x01)
// ============================================================================

#define HCI_INQUIRY                         HCI_OPCODE(0x01, 0x0001)
#define HCI_INQUIRY_CANCEL                  HCI_OPCODE(0x01, 0x0002)
#define HCI_CREATE_CONNECTION               HCI_OPCODE(0x01, 0x0005)
#define HCI_DISCONNECT                      HCI_OPCODE(0x01, 0x0006)
#define HCI_ACCEPT_CONNECTION_REQUEST       HCI_OPCODE(0x01, 0x0009)
#define HCI_REJECT_CONNECTION_REQUEST       HCI_OPCODE(0x01, 0x000A)
#define HCI_LINK_KEY_REQUEST_REPLY          HCI_OPCODE(0x01, 0x000B)
#define HCI_LINK_KEY_REQUEST_NEG_REPLY      HCI_OPCODE(0x01, 0x000C)
#define HCI_PIN_CODE_REQUEST_REPLY          HCI_OPCODE(0x01, 0x000D)
#define HCI_PIN_CODE_REQUEST_NEG_REPLY      HCI_OPCODE(0x01, 0x000E)
#define HCI_AUTH_REQUESTED                  HCI_OPCODE(0x01, 0x0011)
#define HCI_SET_CONNECTION_ENCRYPTION       HCI_OPCODE(0x01, 0x0013)
#define HCI_REMOTE_NAME_REQUEST             HCI_OPCODE(0x01, 0x0019)
#define HCI_REMOTE_NAME_REQUEST_CANCEL      HCI_OPCODE(0x01, 0x001A)
#define HCI_IO_CAPABILITY_REQUEST_REPLY     HCI_OPCODE(0x01, 0x002B)
#define HCI_USER_CONFIRM_REQUEST_REPLY      HCI_OPCODE(0x01, 0x002C)
#define HCI_USER_CONFIRM_REQUEST_NEG_REPLY  HCI_OPCODE(0x01, 0x002D)

// ============================================================================
// LINK POLICY COMMANDS (OGF 0x02)
// ============================================================================

#define HCI_SNIFF_MODE                      HCI_OPCODE(0x02, 0x0003)
#define HCI_EXIT_SNIFF_MODE                 HCI_OPCODE(0x02, 0x0004)
#define HCI_WRITE_LINK_POLICY_SETTINGS      HCI_OPCODE(0x02, 0x000D)

// ============================================================================
// CONTROLLER & BASEBAND COMMANDS (OGF 0x03)
// ============================================================================

#define HCI_SET_EVENT_MASK                  HCI_OPCODE(0x03, 0x0001)
#define HCI_RESET                           HCI_OPCODE(0x03, 0x0003)
#define HCI_SET_EVENT_FILTER                HCI_OPCODE(0x03, 0x0005)
#define HCI_WRITE_PIN_TYPE                  HCI_OPCODE(0x03, 0x000A)
#define HCI_WRITE_LOCAL_NAME                HCI_OPCODE(0x03, 0x0013)
#define HCI_READ_LOCAL_NAME                 HCI_OPCODE(0x03, 0x0014)
#define HCI_READ_PAGE_TIMEOUT               HCI_OPCODE(0x03, 0x0017)
#define HCI_WRITE_PAGE_TIMEOUT              HCI_OPCODE(0x03, 0x0018)
#define HCI_WRITE_SCAN_ENABLE               HCI_OPCODE(0x03, 0x001A)
#define HCI_READ_CLASS_OF_DEVICE            HCI_OPCODE(0x03, 0x0023)
#define HCI_WRITE_CLASS_OF_DEVICE           HCI_OPCODE(0x03, 0x0024)
#define HCI_READ_INQUIRY_MODE               HCI_OPCODE(0x03, 0x0044)
#define HCI_WRITE_INQUIRY_MODE              HCI_OPCODE(0x03, 0x0045)
#define HCI_READ_EXTENDED_INQUIRY_RESPONSE  HCI_OPCODE(0x03, 0x0051)
#define HCI_WRITE_EXTENDED_INQUIRY_RESPONSE HCI_OPCODE(0x03, 0x0052)
#define HCI_WRITE_SIMPLE_PAIRING_MODE       HCI_OPCODE(0x03, 0x0056)
#define HCI_READ_SIMPLE_PAIRING_MODE        HCI_OPCODE(0x03, 0x0055)
#define HCI_SET_EVENT_MASK_PAGE_2           HCI_OPCODE(0x03, 0x0063)

// ============================================================================
// INFORMATIONAL PARAMETERS (OGF 0x04)
// ============================================================================

#define HCI_READ_LOCAL_VERSION_INFO         HCI_OPCODE(0x04, 0x0001)
#define HCI_READ_LOCAL_SUPPORTED_COMMANDS   HCI_OPCODE(0x04, 0x0002)
#define HCI_READ_LOCAL_SUPPORTED_FEATURES   HCI_OPCODE(0x04, 0x0003)
#define HCI_READ_LOCAL_EXTENDED_FEATURES    HCI_OPCODE(0x04, 0x0004)
#define HCI_READ_BUFFER_SIZE                HCI_OPCODE(0x04, 0x0005)
#define HCI_READ_BD_ADDR                    HCI_OPCODE(0x04, 0x0009)

// ============================================================================
// SCAN ENABLE VALUES
// ============================================================================

#define HCI_SCAN_DISABLED                   0x00
#define HCI_SCAN_INQUIRY_ONLY               0x01
#define HCI_SCAN_PAGE_ONLY                  0x02
#define HCI_SCAN_INQUIRY_AND_PAGE           0x03

// ============================================================================
// INQUIRY ACCESS CODES
// ============================================================================

#define HCI_IAC_GIAC                        0x9E8B33  // General Inquiry Access Code
#define HCI_IAC_LIAC                        0x9E8B00  // Limited Inquiry Access Code

// ============================================================================
// CONNECTION ACCEPT ROLES
// ============================================================================

#define HCI_ROLE_MASTER                     0x00
#define HCI_ROLE_SLAVE                      0x01

// ============================================================================
// IO CAPABILITY VALUES (for Simple Pairing)
// ============================================================================

#define HCI_IO_CAP_DISPLAY_ONLY             0x00
#define HCI_IO_CAP_DISPLAY_YES_NO           0x01
#define HCI_IO_CAP_KEYBOARD_ONLY            0x02
#define HCI_IO_CAP_NO_INPUT_NO_OUTPUT       0x03

// ============================================================================
// AUTHENTICATION REQUIREMENTS
// ============================================================================

#define HCI_AUTH_NO_BONDING                 0x00
#define HCI_AUTH_MITM_NOT_REQUIRED          0x02
#define HCI_AUTH_MITM_REQUIRED              0x03
#define HCI_AUTH_DEDICATED_BONDING          0x04
#define HCI_AUTH_DEDICATED_BONDING_MITM     0x05

// ============================================================================
// CLASS OF DEVICE
// ============================================================================

// Major Service Classes (bits 13-23)
#define HCI_COD_SERVICE_NETWORKING          (1 << 17)
#define HCI_COD_SERVICE_RENDERING           (1 << 18)
#define HCI_COD_SERVICE_CAPTURING           (1 << 19)
#define HCI_COD_SERVICE_OBJECT_TRANSFER     (1 << 20)
#define HCI_COD_SERVICE_AUDIO               (1 << 21)
#define HCI_COD_SERVICE_TELEPHONY           (1 << 22)
#define HCI_COD_SERVICE_INFORMATION         (1 << 23)

// Major Device Class (bits 8-12)
#define HCI_COD_MAJOR_MISC                  0x0000
#define HCI_COD_MAJOR_COMPUTER              0x0100
#define HCI_COD_MAJOR_PHONE                 0x0200
#define HCI_COD_MAJOR_LAN_ACCESS            0x0300
#define HCI_COD_MAJOR_AUDIO_VIDEO           0x0400
#define HCI_COD_MAJOR_PERIPHERAL            0x0500
#define HCI_COD_MAJOR_IMAGING               0x0600
#define HCI_COD_MAJOR_WEARABLE              0x0700
#define HCI_COD_MAJOR_TOY                   0x0800
#define HCI_COD_MAJOR_HEALTH                0x0900
#define HCI_COD_MAJOR_UNCATEGORIZED         0x1F00

// Minor Device Class for Peripheral (bits 2-7)
#define HCI_COD_MINOR_PERIPHERAL_KEYBOARD   0x40
#define HCI_COD_MINOR_PERIPHERAL_POINTING   0x80
#define HCI_COD_MINOR_PERIPHERAL_COMBO      0xC0
#define HCI_COD_MINOR_PERIPHERAL_GAMEPAD    0x08
#define HCI_COD_MINOR_PERIPHERAL_JOYSTICK   0x04

// ============================================================================
// HCI COMMAND STRUCTURES
// ============================================================================

// Generic HCI command header
typedef struct __attribute__((packed)) {
    uint16_t opcode;
    uint8_t  param_len;
    uint8_t  params[];
} hci_cmd_t;

// Inquiry command parameters
typedef struct __attribute__((packed)) {
    uint8_t  lap[3];          // Inquiry Access Code (LAP portion)
    uint8_t  inquiry_length;  // N * 1.28s (max 0x30 = 61.44s)
    uint8_t  num_responses;   // Max responses (0 = unlimited)
} hci_inquiry_params_t;

// Create Connection parameters
typedef struct __attribute__((packed)) {
    uint8_t  bd_addr[6];
    uint16_t packet_type;
    uint8_t  page_scan_rep_mode;
    uint8_t  reserved;
    uint16_t clock_offset;
    uint8_t  allow_role_switch;
} hci_create_conn_params_t;

// Disconnect parameters
typedef struct __attribute__((packed)) {
    uint16_t handle;
    uint8_t  reason;
} hci_disconnect_params_t;

// Accept Connection parameters
typedef struct __attribute__((packed)) {
    uint8_t bd_addr[6];
    uint8_t role;
} hci_accept_conn_params_t;

// PIN Code Reply parameters
typedef struct __attribute__((packed)) {
    uint8_t bd_addr[6];
    uint8_t pin_length;
    uint8_t pin_code[16];
} hci_pin_code_reply_params_t;

// Link Key Reply parameters
typedef struct __attribute__((packed)) {
    uint8_t bd_addr[6];
    uint8_t link_key[16];
} hci_link_key_reply_params_t;

// IO Capability Reply parameters
typedef struct __attribute__((packed)) {
    uint8_t bd_addr[6];
    uint8_t io_capability;
    uint8_t oob_data_present;
    uint8_t auth_requirements;
} hci_io_cap_reply_params_t;

// Write Local Name parameters (up to 248 bytes)
typedef struct __attribute__((packed)) {
    char name[248];
} hci_write_local_name_params_t;

// Write Class of Device parameters
typedef struct __attribute__((packed)) {
    uint8_t class_of_device[3];
} hci_write_cod_params_t;

// Remote Name Request parameters
typedef struct __attribute__((packed)) {
    uint8_t  bd_addr[6];
    uint8_t  page_scan_rep_mode;
    uint8_t  reserved;
    uint16_t clock_offset;
} hci_remote_name_request_params_t;

#endif // HCI_CMD_H
