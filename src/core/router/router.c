// router.c
// USBRetro Core Router Implementation
//
// Zero-latency event-driven routing system.
// Replaces console-specific post_input_event() with unified routing.

#include "router.h"
#include "common/players.h"
#include <string.h>
#include <stdio.h>

// ============================================================================
// OUTPUT STATE (replaces players[] array)
// ============================================================================

// Output state per output type (GameCube, PCEngine, 3DO, etc.)
// Each output has up to MAX_PLAYERS_PER_OUTPUT player slots
static output_state_t router_outputs[MAX_OUTPUTS][MAX_PLAYERS_PER_OUTPUT];

// Router configuration (set at init)
static router_config_t router_config;

// Active output count (for broadcast mode)
static output_target_t active_outputs[MAX_OUTPUTS];
static uint8_t active_output_count = 0;

// ============================================================================
// INITIALIZATION
// ============================================================================

void router_init(const router_config_t* config) {
    if (!config) {
        printf("[router] ERROR: NULL config\n");
        return;
    }

    // Copy configuration
    router_config = *config;

    printf("[router] Initializing router\n");
    printf("[router]   Mode: %s\n",
        config->mode == ROUTING_MODE_SIMPLE ? "SIMPLE" :
        config->mode == ROUTING_MODE_MERGE ? "MERGE" :
        config->mode == ROUTING_MODE_BROADCAST ? "BROADCAST" : "CONFIGURABLE");

    if (config->mode == ROUTING_MODE_MERGE) {
        printf("[router]   Merge mode: %s\n",
            config->merge_mode == MERGE_PRIORITY ? "PRIORITY" :
            config->merge_mode == MERGE_BLEND ? "BLEND" : "ALL");
        printf("[router]   Merge all inputs: %s\n", config->merge_all_inputs ? "YES" : "NO");
    }

    // Initialize output states
    for (uint8_t output = 0; output < MAX_OUTPUTS; output++) {
        for (uint8_t player = 0; player < MAX_PLAYERS_PER_OUTPUT; player++) {
            init_input_event(&router_outputs[output][player].current_state);
            router_outputs[output][player].updated = false;
            router_outputs[output][player].player_id = player;
            router_outputs[output][player].source = INPUT_SOURCE_USB_HOST;  // Default
        }
    }

    printf("[router] Initialized successfully\n");
}

// ============================================================================
// INPUT SUBMISSION (Core 0 - Event Driven)
// ============================================================================

// Bridge: Update legacy players[] array from input_event_t
// TODO: Remove this in Phase 5 (Console-Specific State Migration)
// Currently still needed by:
// - Xbox One: mouse accumulator (players[i].global_x/y, players[i].analog[])
// - Can be removed once console-specific state is moved to local structures
static inline void update_legacy_players_array(const input_event_t* event, int player_index) {
    extern Player_t players[];

    if (player_index < 0 || player_index >= MAX_PLAYERS) return;

    // Update digital inputs
    players[player_index].global_buttons = event->buttons;
    players[player_index].output_buttons = event->buttons;

    // Update analog inputs (new unified array format)
    for (int i = 0; i < 8; i++) {
        players[player_index].analog[i] = event->analog[i];
    }

    // Update keyboard state
    players[player_index].keypress[0] = event->keys;

    // Update device type
    players[player_index].device_type = event->type;
}

// SIMPLE MODE: Direct 1:1 pass-through (zero overhead, can be inlined)
static inline void router_simple_mode(const input_event_t* event, output_target_t output) {
    // Find or add player
    int player_index = find_player_index(event->dev_addr, event->instance);

    if (player_index < 0) {
        // Check if any button pressed (auto-assign on first press)
        uint16_t buttons_pressed = (~(event->buttons | 0x800)) | event->keys;
        if (buttons_pressed) {
            player_index = add_player(event->dev_addr, event->instance);
            if (player_index >= 0) {
                printf("[router] Player %d assigned (dev_addr=%d, instance=%d)\n",
                    player_index + 1, event->dev_addr, event->instance);
            }
        }
    }

    if (player_index >= 0 && player_index < router_config.max_players_per_output[output]) {
        // Direct pass-through (atomic write)
        router_outputs[output][player_index].current_state = *event;
        router_outputs[output][player_index].updated = true;
        router_outputs[output][player_index].source = INPUT_SOURCE_USB_HOST;  // Default for now

        // Bridge: Update legacy players[] array
        update_legacy_players_array(event, player_index);
    }
}

// MERGE MODE: Multiple inputs → single output
static inline void router_merge_mode(const input_event_t* event, output_target_t output) {
    if (router_config.merge_all_inputs) {
        // MERGE_ALL: Latest active input wins (current GCUSB behavior)
        // All USB controllers merged to single output port

        // Register player if not already registered (for LED and rumble support)
        int player_index = find_player_index(event->dev_addr, event->instance);
        if (player_index < 0) {
            uint16_t buttons_pressed = (~(event->buttons | 0x800)) | event->keys;
            if (buttons_pressed || event->type == INPUT_TYPE_MOUSE) {
                player_index = add_player(event->dev_addr, event->instance);
                if (player_index >= 0) {
                    printf("[router] Player %d assigned in merge mode (dev_addr=%d, instance=%d)\n",
                        player_index + 1, event->dev_addr, event->instance);
                }
            }
        }

        // Update output state for registered players (process all events, not just button presses)
        if (player_index >= 0) {
            router_outputs[output][0].current_state = *event;
            router_outputs[output][0].updated = true;
            router_outputs[output][0].source = INPUT_SOURCE_USB_HOST;  // Default for now

            // Bridge: Update legacy players[] array (always player 0 in merge mode)
            update_legacy_players_array(event, 0);
        }
    } else {
        // MERGE_PRIORITY: High priority input wins, low priority fallback
        // Used by Super3D0USB (USB priority, SNES fallback)

        // TODO: Implement priority-based merging
        // For now, use simple pass-through
        router_simple_mode(event, output);
    }
}

// Main input submission function (called by input drivers)
void __not_in_flash_func(router_submit_input)(const input_event_t* event) {
    if (!event) return;

    // Determine output target based on routing mode
    // For Phase 2, we assume single output (current behavior)
    // Future: Support multiple outputs and routing tables

    output_target_t output;

    // Map compile-time CONFIG to output target
    #if defined(CONFIG_NGC)
        output = OUTPUT_TARGET_GAMECUBE;
    #elif defined(CONFIG_PCE)
        output = OUTPUT_TARGET_PCENGINE;
    #elif defined(CONFIG_3DO)
        output = OUTPUT_TARGET_3DO;
    #elif defined(CONFIG_NUON)
        output = OUTPUT_TARGET_NUON;
    #elif defined(CONFIG_XB1)
        output = OUTPUT_TARGET_XBOXONE;
    #elif defined(CONFIG_LOOPY)
        output = OUTPUT_TARGET_LOOPY;
    #else
        #error "No console output defined!"
    #endif

    // Route based on mode
    switch (router_config.mode) {
        case ROUTING_MODE_SIMPLE:
            router_simple_mode(event, output);
            break;

        case ROUTING_MODE_MERGE:
            router_merge_mode(event, output);
            break;

        case ROUTING_MODE_BROADCAST:
            // TODO: Implement broadcast mode (Phase 7)
            router_simple_mode(event, output);
            break;

        case ROUTING_MODE_CONFIGURABLE:
            // TODO: Implement configurable mode (Phase 7)
            router_simple_mode(event, output);
            break;
    }

    // Signal Core 1 that new data is available
    // Note: In current architecture, update_output() is called by post_input_event()
    // which we're replacing. For now, Core 1 will poll router_get_output().
    // Future: Add explicit signaling mechanism.
}

// ============================================================================
// OUTPUT RETRIEVAL (Core 1 - Poll or Event Driven)
// ============================================================================

const input_event_t* __not_in_flash_func(router_get_output)(output_target_t output, uint8_t player_id) {
    if (output >= MAX_OUTPUTS || player_id >= MAX_PLAYERS_PER_OUTPUT) {
        return NULL;
    }

    if (router_outputs[output][player_id].updated) {
        router_outputs[output][player_id].updated = false;  // Mark as read
        return &router_outputs[output][player_id].current_state;
    }

    // Return current state even if not updated (for continuous polling)
    return &router_outputs[output][player_id].current_state;
}

bool router_has_updates(output_target_t output) {
    if (output >= MAX_OUTPUTS) return false;

    for (uint8_t player = 0; player < MAX_PLAYERS_PER_OUTPUT; player++) {
        if (router_outputs[output][player].updated) {
            return true;
        }
    }
    return false;
}

uint8_t router_get_player_count(output_target_t output) {
    if (output >= MAX_OUTPUTS) return 0;

    // Return current playersCount (from player management system)
    extern int playersCount;
    return (uint8_t)playersCount;
}

// ============================================================================
// ROUTING CONFIGURATION
// ============================================================================

void router_add_route(input_source_t input, output_target_t output, uint8_t priority) {
    // TODO: Implement routing table (Phase 6)
    printf("[router] Route added: input=%d → output=%d (priority=%d)\n", input, output, priority);
}

void router_clear_routes(void) {
    // TODO: Implement routing table (Phase 6)
    printf("[router] Routes cleared\n");
}

void router_set_merge_mode(output_target_t output, merge_mode_t mode) {
    router_config.merge_mode = mode;
    printf("[router] Merge mode set: %s\n",
        mode == MERGE_PRIORITY ? "PRIORITY" :
        mode == MERGE_BLEND ? "BLEND" : "ALL");
}

void router_set_active_outputs(output_target_t* outputs, uint8_t count) {
    if (!outputs || count > MAX_OUTPUTS) return;

    active_output_count = count;
    for (uint8_t i = 0; i < count; i++) {
        active_outputs[i] = outputs[i];
    }

    printf("[router] Active outputs set: count=%d\n", count);
}

// ============================================================================
// DEBUG/TESTING
// ============================================================================

output_state_t* router_get_state_ptr(output_target_t output) {
    if (output >= MAX_OUTPUTS) return NULL;
    return router_outputs[output];
}
