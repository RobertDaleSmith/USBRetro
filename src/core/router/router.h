// router.h
// USBRetro Core Router - Zero-latency N:M input/output routing
//
// Replaces console-specific post_input_event() with unified routing system.
// Supports 4 modes: Simple (1:1), Merge (N:1), Broadcast (1:N), Configurable (N:M)

#ifndef ROUTER_H
#define ROUTER_H

#include <stdint.h>
#include <stdbool.h>
#include "common/input_event.h"

// ============================================================================
// ROUTING MODES
// ============================================================================

typedef enum {
    ROUTING_MODE_SIMPLE,        // 1:1 fixed (GCUSB, USB2PCE, current products)
    ROUTING_MODE_MERGE,         // N:1 merge inputs (Super3D0USB, current GCUSB all→port1)
    ROUTING_MODE_BROADCAST,     // 1:N broadcast (MultiOut: USB → GC+USBD+BLE)
    ROUTING_MODE_CONFIGURABLE,  // N:M user-defined (Universal app)
} routing_mode_t;

typedef enum {
    MERGE_PRIORITY,             // High priority wins (Super3D0USB: USB > SNES)
    MERGE_BLEND,                // Blend button states (OR buttons together)
    MERGE_ALL,                  // Latest active wins (current GCUSB behavior)
} merge_mode_t;

// ============================================================================
// INPUT/OUTPUT SOURCES
// ============================================================================

typedef enum {
    INPUT_SOURCE_USB_HOST,
    INPUT_SOURCE_BLE_CENTRAL,
    INPUT_SOURCE_NATIVE_SNES,
    INPUT_SOURCE_NATIVE_3DO,
    INPUT_SOURCE_GPIO,
    INPUT_SOURCE_SENSORS,
} input_source_t;

typedef enum {
    OUTPUT_TARGET_GAMECUBE,
    OUTPUT_TARGET_PCENGINE,
    OUTPUT_TARGET_3DO,
    OUTPUT_TARGET_NUON,
    OUTPUT_TARGET_XBOXONE,
    OUTPUT_TARGET_LOOPY,
    OUTPUT_TARGET_USB_DEVICE,
    OUTPUT_TARGET_BLE_PERIPHERAL,
} output_target_t;

// ============================================================================
// ROUTER CONFIGURATION
// ============================================================================

#define MAX_OUTPUTS 8
#define MAX_PLAYERS_PER_OUTPUT 8

typedef struct {
    routing_mode_t mode;
    merge_mode_t merge_mode;
    uint8_t max_players_per_output[MAX_OUTPUTS];  // Per-output limits (GC=4, 3DO=8, PCE=5)
    bool merge_all_inputs;                        // Merge all inputs to single output (current GC)
} router_config_t;

// ============================================================================
// ROUTER INITIALIZATION
// ============================================================================

// Initialize router with configuration
void router_init(const router_config_t* config);

// ============================================================================
// INPUT SUBMISSION (Core 0 - Event Driven, replaces post_input_event)
// ============================================================================

// Called immediately when input arrives (USB report, BLE notification, etc.)
// Processes event and updates output state atomically
// NOTE: This is the ONLY function input drivers should call!
void router_submit_input(const input_event_t* event);

// ============================================================================
// OUTPUT RETRIEVAL (Core 1 - Poll or Event Driven)
// ============================================================================

// Get latest input state for this output+player (returns NULL if no update)
// Lock-free read, zero-copy (returns pointer to internal state)
const input_event_t* router_get_output(output_target_t output, uint8_t player_id);

// Check if any player has new data (fast scan for multi-player outputs)
bool router_has_updates(output_target_t output);

// Get player count for this output
uint8_t router_get_player_count(output_target_t output);

// ============================================================================
// ROUTING CONFIGURATION (called by apps at init or runtime)
// ============================================================================

// Add route (input → output mapping)
void router_add_route(input_source_t input, output_target_t output, uint8_t priority);

// Clear all routes (for runtime reconfiguration)
void router_clear_routes(void);

// Set merge mode for output
void router_set_merge_mode(output_target_t output, merge_mode_t mode);

// Set active outputs (for broadcast mode)
void router_set_active_outputs(output_target_t* outputs, uint8_t count);

// ============================================================================
// INTERNAL STATE (exposed for debugging, don't modify directly)
// ============================================================================

// Output state structure (replaces players[] array)
typedef struct {
    input_event_t current_state;    // Latest event (atomic write)
    volatile bool updated;           // New data flag
    uint8_t player_id;               // Player slot assignment
    input_source_t source;           // Source of this input (for priority)
} output_state_t;

// Get pointer to output state array (for debugging/testing)
output_state_t* router_get_state_ptr(output_target_t output);

#endif // ROUTER_H
