// smp.h - Security Manager Protocol (SMP) for BLE
// Handles BLE pairing and encryption

#ifndef SMP_H
#define SMP_H

#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// SMP OPCODES
// ============================================================================

#define SMP_PAIRING_REQUEST         0x01
#define SMP_PAIRING_RESPONSE        0x02
#define SMP_PAIRING_CONFIRM         0x03
#define SMP_PAIRING_RANDOM          0x04
#define SMP_PAIRING_FAILED          0x05
#define SMP_ENCRYPTION_INFO         0x06
#define SMP_MASTER_IDENT            0x07
#define SMP_IDENTITY_INFO           0x08
#define SMP_IDENTITY_ADDR_INFO      0x09
#define SMP_SIGNING_INFO            0x0A
#define SMP_SECURITY_REQUEST        0x0B
#define SMP_PAIRING_PUBLIC_KEY      0x0C
#define SMP_PAIRING_DHKEY_CHECK     0x0D
#define SMP_PAIRING_KEYPRESS_NOTIF  0x0E

// ============================================================================
// SMP IO CAPABILITIES
// ============================================================================

#define SMP_IO_DISPLAY_ONLY         0x00
#define SMP_IO_DISPLAY_YES_NO       0x01
#define SMP_IO_KEYBOARD_ONLY        0x02
#define SMP_IO_NO_INPUT_NO_OUTPUT   0x03
#define SMP_IO_KEYBOARD_DISPLAY     0x04

// ============================================================================
// SMP OOB DATA FLAGS
// ============================================================================

#define SMP_OOB_NOT_PRESENT         0x00
#define SMP_OOB_PRESENT             0x01

// ============================================================================
// SMP AUTH REQ FLAGS
// ============================================================================

#define SMP_AUTH_NONE               0x00
#define SMP_AUTH_BONDING            0x01
#define SMP_AUTH_MITM               0x04
#define SMP_AUTH_SC                 0x08  // Secure Connections
#define SMP_AUTH_KEYPRESS           0x10
#define SMP_AUTH_CT2                0x20

// ============================================================================
// SMP KEY DISTRIBUTION FLAGS
// ============================================================================

#define SMP_KEY_ENC_KEY             0x01  // LTK (Long Term Key)
#define SMP_KEY_ID_KEY              0x02  // IRK (Identity Resolving Key)
#define SMP_KEY_SIGN_KEY            0x04  // CSRK (Connection Signature Resolving Key)
#define SMP_KEY_LINK_KEY            0x08  // Link Key (BR/EDR derivation)

// ============================================================================
// SMP ERROR CODES
// ============================================================================

#define SMP_ERROR_NONE                      0x00
#define SMP_ERROR_PASSKEY_ENTRY_FAILED      0x01
#define SMP_ERROR_OOB_NOT_AVAILABLE         0x02
#define SMP_ERROR_AUTH_REQUIREMENTS         0x03
#define SMP_ERROR_CONFIRM_VALUE_FAILED      0x04
#define SMP_ERROR_PAIRING_NOT_SUPPORTED     0x05
#define SMP_ERROR_ENCRYPTION_KEY_SIZE       0x06
#define SMP_ERROR_COMMAND_NOT_SUPPORTED     0x07
#define SMP_ERROR_UNSPECIFIED_REASON        0x08
#define SMP_ERROR_REPEATED_ATTEMPTS         0x09
#define SMP_ERROR_INVALID_PARAMETERS        0x0A
#define SMP_ERROR_DHKEY_CHECK_FAILED        0x0B
#define SMP_ERROR_NUMERIC_COMPARISON_FAILED 0x0C
#define SMP_ERROR_BR_EDR_IN_PROGRESS        0x0D
#define SMP_ERROR_CROSS_TRANSPORT_KEY       0x0E

// ============================================================================
// SMP PDU STRUCTURES
// ============================================================================

// Pairing Request/Response (7 bytes)
typedef struct __attribute__((packed)) {
    uint8_t code;
    uint8_t io_capability;
    uint8_t oob_data_flag;
    uint8_t auth_req;
    uint8_t max_key_size;
    uint8_t initiator_key_dist;
    uint8_t responder_key_dist;
} smp_pairing_t;

// Pairing Confirm (17 bytes)
typedef struct __attribute__((packed)) {
    uint8_t code;
    uint8_t confirm[16];
} smp_pairing_confirm_t;

// Pairing Random (17 bytes)
typedef struct __attribute__((packed)) {
    uint8_t code;
    uint8_t random[16];
} smp_pairing_random_t;

// Pairing Failed (2 bytes)
typedef struct __attribute__((packed)) {
    uint8_t code;
    uint8_t reason;
} smp_pairing_failed_t;

// Encryption Information (17 bytes)
typedef struct __attribute__((packed)) {
    uint8_t code;
    uint8_t ltk[16];
} smp_encryption_info_t;

// Master Identification (11 bytes)
typedef struct __attribute__((packed)) {
    uint8_t code;
    uint16_t ediv;
    uint8_t rand[8];
} smp_master_ident_t;

// ============================================================================
// SMP STATE
// ============================================================================

typedef enum {
    SMP_STATE_IDLE,
    SMP_STATE_PAIRING_REQ_SENT,
    SMP_STATE_PAIRING_RSP_RECEIVED,
    SMP_STATE_CONFIRM_SENT,
    SMP_STATE_RANDOM_SENT,
    SMP_STATE_KEY_EXCHANGE,
    SMP_STATE_ENCRYPTED,
    SMP_STATE_FAILED
} smp_state_t;

typedef struct {
    uint8_t  conn_index;
    uint16_t handle;
    smp_state_t state;

    // Pairing parameters
    uint8_t io_capability;
    uint8_t auth_req;
    uint8_t max_key_size;

    // Pairing data
    uint8_t preq[7];        // Pairing Request we sent
    uint8_t pres[7];        // Pairing Response we received
    uint8_t tk[16];         // Temporary Key (all zeros for Just Works)
    uint8_t mrand[16];      // Our random value (initiator)
    uint8_t srand[16];      // Their random value (responder)
    uint8_t mconfirm[16];   // Our confirm value
    uint8_t sconfirm[16];   // Their confirm value
    uint8_t stk[16];        // Short Term Key (result of pairing)

    // Long Term Key (for reconnection)
    uint8_t ltk[16];
    uint16_t ediv;
    uint8_t rand[8];
    bool has_ltk;
} smp_context_t;

// ============================================================================
// SMP API
// ============================================================================

// Initialize SMP
void smp_init(void);

// Called when BLE connection is established
void smp_on_connect(uint8_t conn_index, uint16_t handle);

// Called when BLE connection is disconnected
void smp_on_disconnect(uint8_t conn_index);

// Process incoming SMP data (from L2CAP CID 0x0006)
void smp_process_data(uint8_t conn_index, const uint8_t* data, uint16_t len);

// Initiate pairing (Just Works mode)
bool smp_start_pairing(uint8_t conn_index);

// Send SMP data
bool smp_send(uint8_t conn_index, const uint8_t* data, uint16_t len);

// Check if connection is encrypted
bool smp_is_encrypted(uint8_t conn_index);

// Callback when encryption is enabled
extern void smp_on_encrypted(uint8_t conn_index);

// Called when HCI encryption change event indicates success
void smp_on_encryption_enabled(uint8_t conn_index);

#endif // SMP_H
