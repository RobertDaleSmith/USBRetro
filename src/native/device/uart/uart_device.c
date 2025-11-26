// uart_device.c - UART Device Implementation
//
// Sends controller outputs to a remote device over UART. Supports streaming
// input events, responding to status queries, and receiving feedback commands.

#include "uart_device.h"
#include "core/uart/uart_protocol.h"
#include "core/router/router.h"
#include "core/input_event.h"
#include "core/services/players/manager.h"
#include "core/services/profiles/profile.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include <string.h>
#include <stdio.h>

// ============================================================================
// INTERNAL STATE
// ============================================================================

static bool initialized = false;
static uart_inst_t* uart_port = UART_DEVICE_PERIPHERAL;
static uart_device_mode_t device_mode = UART_DEVICE_MODE_ON_CHANGE;

// Transmit queue for input events
#define TX_QUEUE_SIZE 16
static uart_input_event_t tx_queue[TX_QUEUE_SIZE];
static volatile uint8_t tx_queue_head = 0;
static volatile uint8_t tx_queue_tail = 0;

// Previous state for change detection
#define UART_MAX_PLAYERS 8  // Maximum players tracked by UART device
static uint32_t prev_buttons[UART_MAX_PLAYERS];
static uint8_t prev_analog[UART_MAX_PLAYERS][6];

// Receive state machine (for feedback packets)
typedef enum {
    RX_STATE_SYNC,
    RX_STATE_LENGTH,
    RX_STATE_TYPE,
    RX_STATE_PAYLOAD,
    RX_STATE_CRC,
} rx_state_t;

static rx_state_t rx_state = RX_STATE_SYNC;
static uint8_t rx_buffer[UART_PROTOCOL_MAX_PAYLOAD + UART_OVERHEAD];
static uint8_t rx_index = 0;
static uint8_t rx_length = 0;
static uint8_t rx_type = 0;

// Statistics
static uint32_t tx_count = 0;
static uint32_t rx_count = 0;
static uint32_t error_count = 0;
static uint32_t queue_drops = 0;
static uint32_t last_rx_time = 0;

// Callbacks
static uart_device_rumble_callback_t rumble_callback = NULL;
static uart_device_led_callback_t led_callback = NULL;

// ============================================================================
// TRANSMIT HELPERS
// ============================================================================

// Send a raw packet with header and CRC
void uart_device_send_packet(uint8_t type, const void* payload, uint8_t len)
{
    if (!initialized) return;
    if (len > UART_PROTOCOL_MAX_PAYLOAD) return;

    uint8_t packet[UART_PROTOCOL_MAX_PAYLOAD + UART_OVERHEAD];
    uint8_t idx = 0;

    // Header
    packet[idx++] = UART_PROTOCOL_SYNC_BYTE;
    packet[idx++] = len;
    packet[idx++] = type;

    // Payload
    if (len > 0 && payload != NULL) {
        memcpy(&packet[idx], payload, len);
        idx += len;
    }

    // CRC over length + type + payload
    uint8_t crc = uart_crc8(&packet[1], len + 2);
    packet[idx++] = crc;

    // Send packet
    uart_write_blocking(uart_port, packet, idx);
    tx_count++;
}

// ============================================================================
// TRANSMIT QUEUE MANAGEMENT
// ============================================================================

static inline uint8_t tx_queue_count(void)
{
    return (tx_queue_head - tx_queue_tail) & (TX_QUEUE_SIZE - 1);
}

static inline bool tx_queue_full(void)
{
    return tx_queue_count() >= (TX_QUEUE_SIZE - 1);
}

static inline bool tx_queue_empty(void)
{
    return tx_queue_head == tx_queue_tail;
}

static bool tx_queue_push(const uart_input_event_t* event)
{
    if (tx_queue_full()) {
        queue_drops++;
        return false;
    }
    tx_queue[tx_queue_head] = *event;
    tx_queue_head = (tx_queue_head + 1) & (TX_QUEUE_SIZE - 1);
    return true;
}

static bool tx_queue_pop(uart_input_event_t* event)
{
    if (tx_queue_empty()) {
        return false;
    }
    *event = tx_queue[tx_queue_tail];
    tx_queue_tail = (tx_queue_tail + 1) & (TX_QUEUE_SIZE - 1);
    return true;
}

// ============================================================================
// RECEIVE PACKET PROCESSING (for feedback)
// ============================================================================

static void process_rx_packet(uint8_t type, const uint8_t* payload, uint8_t len)
{
    switch (type) {
        case UART_PKT_NOP:
            // Keepalive
            break;

        case UART_PKT_PING:
            // Respond with PONG
            uart_device_send_packet(UART_PKT_PONG, NULL, 0);
            break;

        case UART_PKT_GET_STATUS:
            uart_device_send_status();
            break;

        case UART_PKT_RUMBLE: {
            if (len >= sizeof(uart_rumble_cmd_t) && rumble_callback) {
                const uart_rumble_cmd_t* cmd = (const uart_rumble_cmd_t*)payload;
                rumble_callback(cmd->player_index, cmd->left_motor,
                               cmd->right_motor, cmd->duration_ms);
            }
            break;
        }

        case UART_PKT_LED: {
            if (len >= sizeof(uart_led_cmd_t) && led_callback) {
                const uart_led_cmd_t* cmd = (const uart_led_cmd_t*)payload;
                led_callback(cmd->player_index, cmd->pattern, cmd->r, cmd->g, cmd->b);
            }
            break;
        }

        case UART_PKT_GET_PROFILE:
            // Send current profile index
            {
                uint8_t profile = profile_get_active_index(router_get_primary_output());
                uart_device_send_packet(UART_PKT_PROFILE, &profile, 1);
            }
            break;

        case UART_PKT_GET_PLAYERS:
            // Send player count
            {
                uint8_t count = router_get_player_count(router_get_primary_output());
                uart_device_send_packet(UART_PKT_PLAYERS, &count, 1);
            }
            break;

        default:
            error_count++;
            break;
    }
}

static void process_rx_byte(uint8_t byte)
{
    switch (rx_state) {
        case RX_STATE_SYNC:
            if (byte == UART_PROTOCOL_SYNC_BYTE) {
                rx_buffer[0] = byte;
                rx_index = 1;
                rx_state = RX_STATE_LENGTH;
            }
            break;

        case RX_STATE_LENGTH:
            rx_length = byte;
            rx_buffer[rx_index++] = byte;
            if (rx_length > UART_PROTOCOL_MAX_PAYLOAD) {
                error_count++;
                rx_state = RX_STATE_SYNC;
            } else {
                rx_state = RX_STATE_TYPE;
            }
            break;

        case RX_STATE_TYPE:
            rx_type = byte;
            rx_buffer[rx_index++] = byte;
            if (rx_length == 0) {
                rx_state = RX_STATE_CRC;
            } else {
                rx_state = RX_STATE_PAYLOAD;
            }
            break;

        case RX_STATE_PAYLOAD:
            rx_buffer[rx_index++] = byte;
            if (rx_index >= UART_HEADER_SIZE + rx_length) {
                rx_state = RX_STATE_CRC;
            }
            break;

        case RX_STATE_CRC: {
            uint8_t received_crc = byte;
            uint8_t calculated_crc = uart_crc8(&rx_buffer[1], rx_length + 2);

            if (received_crc == calculated_crc) {
                rx_count++;
                last_rx_time = to_ms_since_boot(get_absolute_time());
                process_rx_packet(rx_type, &rx_buffer[UART_HEADER_SIZE], rx_length);
            } else {
                error_count++;
            }
            rx_state = RX_STATE_SYNC;
            break;
        }
    }
}

// ============================================================================
// PUBLIC API
// ============================================================================

void uart_device_init(void)
{
    uart_device_init_pins(UART_DEVICE_TX_PIN, UART_DEVICE_RX_PIN,
                          UART_PROTOCOL_BAUD_DEFAULT);
}

void uart_device_init_pins(uint8_t tx_pin, uint8_t rx_pin, uint32_t baud)
{
    printf("[uart_device] Initializing UART device\n");
    printf("[uart_device]   TX=%d, RX=%d, BAUD=%lu\n", tx_pin, rx_pin, baud);

    // Initialize UART
    uart_init(uart_port, baud);

    // Set GPIO functions
    gpio_set_function(tx_pin, GPIO_FUNC_UART);
    gpio_set_function(rx_pin, GPIO_FUNC_UART);

    // Configure UART format: 8N1
    uart_set_format(uart_port, 8, 1, UART_PARITY_NONE);

    // Enable FIFO
    uart_set_fifo_enabled(uart_port, true);

    // Initialize state
    memset(prev_buttons, 0xFF, sizeof(prev_buttons));  // All released
    memset(prev_analog, 128, sizeof(prev_analog));     // Centered
    tx_queue_head = 0;
    tx_queue_tail = 0;
    rx_state = RX_STATE_SYNC;

    initialized = true;
    printf("[uart_device] Initialization complete\n");
}

void uart_device_task(void)
{
    if (!initialized) return;

    // Process incoming bytes (feedback commands)
    while (uart_is_readable(uart_port)) {
        uint8_t byte = uart_getc(uart_port);
        process_rx_byte(byte);
    }

    // Send queued input events
    uart_input_event_t event;
    while (tx_queue_pop(&event)) {
        uart_device_send_packet(UART_PKT_INPUT_EVENT, &event, sizeof(event));
    }
}

void uart_device_set_mode(uart_device_mode_t mode)
{
    device_mode = mode;
}

uart_device_mode_t uart_device_get_mode(void)
{
    return device_mode;
}

void uart_device_queue_input(const input_event_t* event, uint8_t player_index)
{
    if (!initialized) return;
    if (device_mode == UART_DEVICE_MODE_OFF) return;
    if (player_index >= UART_MAX_PLAYERS) return;

    // Check for change if in ON_CHANGE mode
    if (device_mode == UART_DEVICE_MODE_ON_CHANGE) {
        bool changed = (event->buttons != prev_buttons[player_index]);

        // Check analog axes
        if (!changed) {
            if (event->analog[ANALOG_X] != prev_analog[player_index][0] ||
                event->analog[ANALOG_Y] != prev_analog[player_index][1] ||
                event->analog[ANALOG_Z] != prev_analog[player_index][2] ||
                event->analog[ANALOG_RX] != prev_analog[player_index][3] ||
                event->analog[ANALOG_RZ] != prev_analog[player_index][4] ||
                event->analog[ANALOG_SLIDER] != prev_analog[player_index][5]) {
                changed = true;
            }
        }

        if (!changed) return;

        // Update previous state
        prev_buttons[player_index] = event->buttons;
        prev_analog[player_index][0] = event->analog[ANALOG_X];
        prev_analog[player_index][1] = event->analog[ANALOG_Y];
        prev_analog[player_index][2] = event->analog[ANALOG_Z];
        prev_analog[player_index][3] = event->analog[ANALOG_RX];
        prev_analog[player_index][4] = event->analog[ANALOG_RZ];
        prev_analog[player_index][5] = event->analog[ANALOG_SLIDER];
    }

    // Build UART event
    uart_input_event_t uart_event;
    uart_event.player_index = player_index;
    uart_event.device_type = event->type;
    uart_event.buttons = event->buttons;
    uart_event.analog[0] = event->analog[ANALOG_X];
    uart_event.analog[1] = event->analog[ANALOG_Y];
    uart_event.analog[2] = event->analog[ANALOG_Z];
    uart_event.analog[3] = event->analog[ANALOG_RX];
    uart_event.analog[4] = event->analog[ANALOG_RZ];
    uart_event.analog[5] = event->analog[ANALOG_SLIDER];
    uart_event.delta_x = event->delta_x;
    uart_event.delta_y = event->delta_y;

    tx_queue_push(&uart_event);
}

void uart_device_send_connect(uint8_t player_index, uint8_t device_type,
                               uint16_t vid, uint16_t pid)
{
    if (!initialized) return;

    uart_connect_event_t evt;
    evt.player_index = player_index;
    evt.device_type = device_type;
    evt.vid = vid;
    evt.pid = pid;

    uart_device_send_packet(UART_PKT_INPUT_CONNECT, &evt, sizeof(evt));
}

void uart_device_send_disconnect(uint8_t player_index)
{
    if (!initialized) return;

    uart_disconnect_event_t evt;
    evt.player_index = player_index;

    uart_device_send_packet(UART_PKT_INPUT_DISCONNECT, &evt, sizeof(evt));
}

void uart_device_send_status(void)
{
    if (!initialized) return;

    uart_status_t status;
    status.player_count = router_get_player_count(router_get_primary_output());
    status.output_target = router_get_primary_output();
    status.profile_index = profile_get_active_index(router_get_primary_output());
    status.flags = UART_STATUS_OUTPUT_ACTIVE;
    status.uptime_sec = to_ms_since_boot(get_absolute_time()) / 1000;

    uart_device_send_packet(UART_PKT_STATUS, &status, sizeof(status));
}

void uart_device_send_version(void)
{
    if (!initialized) return;

    uart_version_t ver;
    ver.major = 1;
    ver.minor = 0;
    ver.patch = 0;
    ver.board_type = UART_BOARD_RP2040;
    ver.features = UART_FEATURE_USB_HOST;

    uart_device_send_packet(UART_PKT_VERSION, &ver, sizeof(ver));
}

bool uart_device_is_connected(void)
{
    if (!initialized) return false;

    uint32_t now = to_ms_since_boot(get_absolute_time());
    return (now - last_rx_time) < 5000;
}

uint32_t uart_device_get_tx_count(void) { return tx_count; }
uint32_t uart_device_get_rx_count(void) { return rx_count; }
uint32_t uart_device_get_error_count(void) { return error_count; }
uint32_t uart_device_get_queue_drops(void) { return queue_drops; }

void uart_device_set_rumble_callback(uart_device_rumble_callback_t callback)
{
    rumble_callback = callback;
}

void uart_device_set_led_callback(uart_device_led_callback_t callback)
{
    led_callback = callback;
}
