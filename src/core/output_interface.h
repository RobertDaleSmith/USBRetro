// output_interface.h
// Output abstraction for USBRetro - supports native console and USB device outputs

#ifndef OUTPUT_INTERFACE_H
#define OUTPUT_INTERFACE_H

#include <stdint.h>
#include <stdbool.h>
#include "core/input_event.h"

// Output interface - abstracts different output types (native consoles, USB device, BLE, etc.)
typedef struct {
    const char* name;                                      // Output name (e.g., "GameCube", "USB Device (XInput)")

    void (*init)(void);                                    // Initialize output hardware/protocol
    void (*handle_input)(const input_event_t* event);      // Handle incoming input event
    void (*core1_entry)(void);                             // Core1 entry point (NULL if not needed)
    void (*task)(void);                                    // Periodic task (NULL if not needed)

    // Feedback to USB input devices (rumble, LEDs)
    uint8_t (*get_rumble)(void);                           // Get rumble state (0-255), NULL = no rumble
    uint8_t (*get_player_led)(void);                       // Get player LED state, NULL = no LED override

    // Profile system (output-specific profiles)
    // Each output defines its own profile structure with console-specific button mappings
    uint8_t (*get_profile_count)(void);                    // Get number of available profiles, NULL = no profiles
    uint8_t (*get_active_profile)(void);                   // Get active profile index (0-based)
    void (*set_active_profile)(uint8_t index);             // Set active profile (triggers flash save)
    const char* (*get_profile_name)(uint8_t index);        // Get profile name for display, NULL = use index

    // Input device feedback (from current profile)
    uint8_t (*get_trigger_threshold)(void);                // Get L2/R2 threshold for adaptive triggers, NULL = 0
} OutputInterface;

// Active output interface (set at compile-time, selected in common/output.c)
extern const OutputInterface* active_output;

#endif // OUTPUT_INTERFACE_H
