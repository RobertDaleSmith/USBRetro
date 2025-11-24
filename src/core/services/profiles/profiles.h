// profiles.h
// USBRetro Core - Universal Profile System
//
// Provides profile storage, loading, and switching infrastructure.
// Console-specific profile data is defined in console implementations.
// Phase 4: Universal infrastructure
// Phase 5: App-specific profiles when apps/ layer is created

#ifndef PROFILES_H
#define PROFILES_H

#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// UNIVERSAL PROFILE STRUCTURE (Phase 4 - Minimal)
// ============================================================================
// Universal settings that apply across all consoles
// Console-specific settings (button maps, etc.) handled by console code

typedef struct {
    // Metadata
    char name[16];                  // Profile name (null-terminated)
    char description[32];           // Short description

    // Adaptive triggers (DualSense, Xbox Elite)
    uint8_t l2_threshold;           // LT threshold for digital action (0-255)
    uint8_t r2_threshold;           // RT threshold for digital action (0-255)

    // Analog stick settings
    float left_stick_sensitivity;   // Left stick scaling (0.1-2.0, 1.0 = 100%)
    float right_stick_sensitivity;  // Right stick scaling (0.1-2.0, 1.0 = 100%)
    bool invert_y_left;             // Invert left stick Y-axis
    bool invert_y_right;            // Invert right stick Y-axis

    // Deadzone settings
    uint8_t left_deadzone;          // Left stick deadzone (0-255)
    uint8_t right_deadzone;         // Right stick deadzone (0-255)

} usbretro_profile_t;

// ============================================================================
// PROFILE SYSTEM CONFIGURATION
// ============================================================================

#define MAX_PROFILES 8              // Maximum number of profiles per console

typedef struct {
    uint8_t profile_count;          // How many profiles this console has
    uint8_t default_profile_index;  // Default profile (0-based)
} profile_system_config_t;

// ============================================================================
// PROFILE SYSTEM API
// ============================================================================

// Initialize profile system (called by console at startup)
void profiles_init(const profile_system_config_t* config);

// Get active profile index (0-based)
uint8_t profile_get_active_index(void);

// Set active profile (triggers flash save after debounce)
void profile_set_active(uint8_t index);

// Get profile count
uint8_t profile_get_count(void);

// Load profile index from flash (called at startup)
// Returns saved index, or default_profile_index if no valid data
uint8_t profile_load_active_index_from_flash(uint8_t default_index);

// Save profile index to flash (debounced - actual write after 5 seconds)
void profile_save_active_index_to_flash(uint8_t index);

// Cycle to next profile (wraps around)
// Returns new profile index
uint8_t profile_cycle_next(void);

// Cycle to previous profile (wraps around)
// Returns new profile index
uint8_t profile_cycle_prev(void);

// Check if profile indicator is currently active (for debouncing profile switches)
extern bool profile_indicator_is_active(void);

// ============================================================================
// CONSOLE-SPECIFIC PROFILE HOOKS
// ============================================================================
// Consoles implement these to get notified of profile changes

// Optional: Called when profile switches
// Console can update its internal state, trigger feedback, etc.
typedef void (*profile_switch_callback_t)(uint8_t new_index);

// Register callback for profile switches
void profile_register_switch_callback(profile_switch_callback_t callback);

#endif // PROFILES_H
