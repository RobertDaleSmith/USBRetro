// uart_protocol.h - UART Bridge Protocol
//
// Defines the UART-based communication protocol for inter-MCU communication.
// Used for ESP32 <-> RP2040 AI integration, multi-board setups, etc.
//
// Physical Layer:
//   - Standard UART (TX/RX)
//   - Default: 1Mbaud, 8N1
//   - Works over Qwiic cable (GND, 3.3V, TX, RX)
//
// Packet Format:
//   [SYNC][LEN][TYPE][PAYLOAD...][CRC8]
//   - SYNC: 0xAA (start of packet marker)
//   - LEN: Payload length (0-255)
//   - TYPE: Packet type enum
//   - PAYLOAD: Type-specific data
//   - CRC8: CRC-8 of LEN+TYPE+PAYLOAD
//
// All multi-byte values are little-endian.

#ifndef UART_PROTOCOL_H
#define UART_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// UART CONFIGURATION
// ============================================================================

#define UART_PROTOCOL_BAUD_DEFAULT  1000000     // 1Mbaud
#define UART_PROTOCOL_SYNC_BYTE     0xAA
#define UART_PROTOCOL_MAX_PAYLOAD   64

// ============================================================================
// PACKET TYPES
// ============================================================================

typedef enum {
    // System packets (0x00-0x0F)
    UART_PKT_NOP            = 0x00,     // No operation / keepalive
    UART_PKT_PING           = 0x01,     // Ping request
    UART_PKT_PONG           = 0x02,     // Ping response
    UART_PKT_VERSION        = 0x03,     // Version info
    UART_PKT_RESET          = 0x04,     // Reset request
    UART_PKT_ACK            = 0x05,     // Acknowledgment
    UART_PKT_NAK            = 0x06,     // Negative acknowledgment

    // Input events (0x10-0x1F)
    UART_PKT_INPUT_EVENT    = 0x10,     // Controller input event
    UART_PKT_INPUT_CONNECT  = 0x11,     // Controller connected
    UART_PKT_INPUT_DISCONNECT = 0x12,   // Controller disconnected

    // Feedback (0x20-0x2F)
    UART_PKT_RUMBLE         = 0x20,     // Rumble command
    UART_PKT_LED            = 0x21,     // LED command
    UART_PKT_FEEDBACK_ACK   = 0x22,     // Feedback acknowledgment

    // State queries (0x30-0x3F)
    UART_PKT_GET_STATUS     = 0x30,     // Request status
    UART_PKT_STATUS         = 0x31,     // Status response
    UART_PKT_GET_PLAYERS    = 0x32,     // Request player info
    UART_PKT_PLAYERS        = 0x33,     // Player info response

    // Profile/config (0x40-0x4F)
    UART_PKT_SET_PROFILE    = 0x40,     // Set active profile
    UART_PKT_GET_PROFILE    = 0x41,     // Get active profile
    UART_PKT_PROFILE        = 0x42,     // Profile response
    UART_PKT_SET_MODE       = 0x43,     // Set output mode

    // AI/Injection (0x50-0x5F) - For ESP32 AI use case
    UART_PKT_AI_INJECT      = 0x50,     // Inject AI input
    UART_PKT_AI_BLEND_MODE  = 0x51,     // Set blend mode
    UART_PKT_AI_OBSERVE     = 0x52,     // Request observation mode

} uart_packet_type_t;

// ============================================================================
// PACKET HEADER
// ============================================================================

typedef struct __attribute__((packed)) {
    uint8_t sync;           // Always UART_PROTOCOL_SYNC_BYTE
    uint8_t length;         // Payload length
    uint8_t type;           // uart_packet_type_t
} uart_packet_header_t;

#define UART_HEADER_SIZE    3
#define UART_CRC_SIZE       1
#define UART_OVERHEAD       (UART_HEADER_SIZE + UART_CRC_SIZE)

// ============================================================================
// INPUT EVENT PACKET
// ============================================================================

// Compact input event for UART transfer
typedef struct __attribute__((packed)) {
    uint8_t  player_index;      // Player slot (0-7)
    uint8_t  device_type;       // INPUT_TYPE_* enum
    uint32_t buttons;           // Button state (active-low)
    uint8_t  analog[6];         // [0]=LX, [1]=LY, [2]=RX, [3]=RY, [4]=L2, [5]=R2
    int8_t   delta_x;           // Mouse delta X
    int8_t   delta_y;           // Mouse delta Y
} uart_input_event_t;

#define UART_INPUT_EVENT_SIZE   sizeof(uart_input_event_t)  // 12 bytes

// ============================================================================
// CONNECT/DISCONNECT PACKETS
// ============================================================================

typedef struct __attribute__((packed)) {
    uint8_t  player_index;      // Player slot
    uint8_t  device_type;       // Device type
    uint16_t vid;               // USB VID (0 for native)
    uint16_t pid;               // USB PID (0 for native)
} uart_connect_event_t;

typedef struct __attribute__((packed)) {
    uint8_t  player_index;      // Player slot
} uart_disconnect_event_t;

// ============================================================================
// RUMBLE/LED PACKETS
// ============================================================================

typedef struct __attribute__((packed)) {
    uint8_t  player_index;      // Target player
    uint8_t  left_motor;        // Left motor (0-255)
    uint8_t  right_motor;       // Right motor (0-255)
    uint16_t duration_ms;       // Duration (0 = stop)
} uart_rumble_cmd_t;

typedef struct __attribute__((packed)) {
    uint8_t  player_index;      // Target player
    uint8_t  pattern;           // LED pattern
    uint8_t  r, g, b;           // RGB color
} uart_led_cmd_t;

// ============================================================================
// STATUS PACKETS
// ============================================================================

typedef struct __attribute__((packed)) {
    uint8_t  player_count;      // Connected players
    uint8_t  output_target;     // Active output
    uint8_t  profile_index;     // Active profile
    uint8_t  flags;             // Status flags
    uint16_t uptime_sec;        // Uptime in seconds
} uart_status_t;

// Status flags
#define UART_STATUS_USB_CONNECTED   0x01
#define UART_STATUS_OUTPUT_ACTIVE   0x02
#define UART_STATUS_AI_ENABLED      0x04
#define UART_STATUS_ERROR           0x80

// ============================================================================
// AI INJECTION PACKETS
// ============================================================================

typedef enum {
    UART_BLEND_OFF          = 0,    // AI disabled
    UART_BLEND_OBSERVE      = 1,    // Observe only
    UART_BLEND_ASSIST       = 2,    // OR with player input
    UART_BLEND_OVERRIDE     = 3,    // AI can override
    UART_BLEND_TAKEOVER     = 4,    // AI full control
} uart_blend_mode_t;

typedef struct __attribute__((packed)) {
    uint8_t  player_index;      // Target player
    uint8_t  blend_mode;        // uart_blend_mode_t
    uint32_t buttons;           // Buttons to inject
    uint8_t  analog[6];         // Analog values
    uint8_t  duration_frames;   // Duration (0 = single frame)
} uart_ai_inject_t;

typedef struct __attribute__((packed)) {
    uint8_t  player_index;      // Target player
    uint8_t  blend_mode;        // uart_blend_mode_t
} uart_blend_mode_cmd_t;

// ============================================================================
// VERSION PACKET
// ============================================================================

typedef struct __attribute__((packed)) {
    uint8_t  major;
    uint8_t  minor;
    uint8_t  patch;
    uint8_t  board_type;        // Board identifier
    uint32_t features;          // Feature flags
} uart_version_t;

// Board types
#define UART_BOARD_RP2040       0x01
#define UART_BOARD_ESP32S3      0x02

// Feature flags
#define UART_FEATURE_USB_HOST   0x0001
#define UART_FEATURE_USB_DEVICE 0x0002
#define UART_FEATURE_WIFI       0x0004
#define UART_FEATURE_BLE        0x0008
#define UART_FEATURE_DISPLAY    0x0010
#define UART_FEATURE_AUDIO      0x0020
#define UART_FEATURE_AI         0x0040

// ============================================================================
// CRC-8 CALCULATION
// ============================================================================

// CRC-8 polynomial: x^8 + x^2 + x + 1 (0x07)
static inline uint8_t uart_crc8(const uint8_t* data, uint8_t len) {
    uint8_t crc = 0x00;
    while (len--) {
        crc ^= *data++;
        for (uint8_t i = 0; i < 8; i++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x07;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

// ============================================================================
// PACKET HELPERS
// ============================================================================

// Calculate total packet size
static inline uint8_t uart_packet_size(uint8_t payload_len) {
    return UART_HEADER_SIZE + payload_len + UART_CRC_SIZE;
}

// Validate packet header
static inline bool uart_validate_header(const uart_packet_header_t* hdr) {
    return (hdr->sync == UART_PROTOCOL_SYNC_BYTE &&
            hdr->length <= UART_PROTOCOL_MAX_PAYLOAD);
}

#endif // UART_PROTOCOL_H
