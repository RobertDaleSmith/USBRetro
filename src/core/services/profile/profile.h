// profile.h - Universal Profile System
//
// Provides a standardized profile structure that works across all output devices.
// Each output device converts universal outputs to its native button format.
//
// Architecture:
//   Input Device → USBR_BUTTON_* → Profile Mapping → USBR_OUTPUT_* → Output Device → Native
//
// Profile switching is handled by core with visual/haptic feedback.
// Profile definitions can be at app level or device level.

#ifndef CORE_PROFILE_H
#define CORE_PROFILE_H

#include <stdint.h>
#include <stdbool.h>
#include "core/buttons.h"  // USBR_BUTTON_* definitions

// ============================================================================
// UNIVERSAL OUTPUT SLOTS
// ============================================================================
// These represent logical output positions that every output device maps
// to its native buttons. This allows profiles to be portable across outputs.

typedef enum {
    USBR_OUT_NONE = 0,

    // Face buttons (physical layout positions)
    USBR_OUT_B1,        // Primary action - bottom (GC:A, 3DO:B, PS:Cross, SNES:B)
    USBR_OUT_B2,        // Secondary - right (GC:B, 3DO:C, PS:Circle, SNES:A)
    USBR_OUT_B3,        // Tertiary - left (GC:Y, 3DO:A, PS:Square, SNES:Y)
    USBR_OUT_B4,        // Quaternary - top (GC:X, 3DO:X, PS:Triangle, SNES:X)

    // Shoulder buttons (digital)
    USBR_OUT_L1,        // Left shoulder (GC:none, 3DO:L, PS:L1)
    USBR_OUT_R1,        // Right shoulder (GC:Z, 3DO:R, PS:R1)

    // Triggers (directly output to L2/R2 - see trigger_behavior for analog handling)
    USBR_OUT_L2,        // Left trigger digital (GC:L, PS:L2)
    USBR_OUT_R2,        // Right trigger digital (GC:R, PS:R2)

    // Combined shoulder+trigger (press outputs both digital states)
    USBR_OUT_L1_L2,     // L1 + L2 together
    USBR_OUT_R1_R2,     // R1 + R2 together
    USBR_OUT_L2_R2,     // L2 + R2 together (SSBM quit combo)

    // System buttons
    USBR_OUT_S1,        // Select/Back/Share (3DO:X, GC:none)
    USBR_OUT_S2,        // Start/Options (GC:Start, 3DO:P)

    // Stick clicks
    USBR_OUT_L3,        // Left stick click
    USBR_OUT_R3,        // Right stick click

    // Auxiliary
    USBR_OUT_A1,        // Guide/Home/PS button
    USBR_OUT_A2,        // Capture/Touchpad

    // D-pad directions (as button outputs)
    USBR_OUT_DU,        // D-pad Up
    USBR_OUT_DD,        // D-pad Down
    USBR_OUT_DL,        // D-pad Left
    USBR_OUT_DR,        // D-pad Right

    // Right stick as buttons (C-stick directions for GC, or general use)
    USBR_OUT_RS_UP,     // Right stick up (GC: C-Up)
    USBR_OUT_RS_DOWN,   // Right stick down (GC: C-Down)
    USBR_OUT_RS_LEFT,   // Right stick left (GC: C-Left)
    USBR_OUT_RS_RIGHT,  // Right stick right (GC: C-Right)

    // Full analog press (digital + analog forced to max)
    USBR_OUT_L2_FULL,   // L2 digital + L2 analog at 255 (GC: L full press)
    USBR_OUT_R2_FULL,   // R2 digital + R2 analog at 255 (GC: R full press)

    // Light press (for SSBM light shield - analog at custom value)
    USBR_OUT_L2_LIGHT,  // L2 analog at light value (uses l2_analog_value)
    USBR_OUT_R2_LIGHT,  // R2 analog at light value (uses r2_analog_value)

    // Device-specific extensions (output interprets these)
    USBR_OUT_SPECIAL_1, // 3DO: Fire button (joystick mode)
    USBR_OUT_SPECIAL_2, // Reserved
    USBR_OUT_SPECIAL_3, // Reserved
    USBR_OUT_SPECIAL_4, // Reserved

    USBR_OUT_COUNT
} usbr_output_t;

// ============================================================================
// TRIGGER BEHAVIOR
// ============================================================================
// How analog triggers (L2/R2) behave - separate from button mapping

typedef enum {
    TRIGGER_PASSTHROUGH = 0,    // Analog value passed through, digital at threshold
    TRIGGER_DIGITAL_ONLY,       // Digital only, no analog output
    TRIGGER_FULL_PRESS,         // Digital + analog forced to 255
    TRIGGER_LIGHT_PRESS,        // Digital + analog at custom value (l2/r2_analog_value)
    TRIGGER_INSTANT,            // Digital triggers immediately (threshold = 1)
    TRIGGER_DISABLED,           // No output
} trigger_behavior_t;

// ============================================================================
// UNIVERSAL BUTTON MAPPING
// ============================================================================
// Maps each USBR_BUTTON_* input to a USBR_OUT_* output

typedef struct {
    // Face buttons
    usbr_output_t b1;           // USBR_BUTTON_B1 → ?
    usbr_output_t b2;           // USBR_BUTTON_B2 → ?
    usbr_output_t b3;           // USBR_BUTTON_B3 → ?
    usbr_output_t b4;           // USBR_BUTTON_B4 → ?

    // Shoulder buttons
    usbr_output_t l1;           // USBR_BUTTON_L1 → ?
    usbr_output_t r1;           // USBR_BUTTON_R1 → ?

    // System buttons
    usbr_output_t s1;           // USBR_BUTTON_S1 (Select) → ?
    usbr_output_t s2;           // USBR_BUTTON_S2 (Start) → ?

    // Stick clicks
    usbr_output_t l3;           // USBR_BUTTON_L3 → ?
    usbr_output_t r3;           // USBR_BUTTON_R3 → ?

    // Auxiliary
    usbr_output_t a1;           // USBR_BUTTON_A1 (Guide) → ?
    usbr_output_t a2;           // USBR_BUTTON_A2 (Capture) → ?
} usbr_button_map_t;

// ============================================================================
// UNIVERSAL PROFILE STRUCTURE
// ============================================================================

typedef struct {
    // Identity
    const char* name;           // Short name (e.g., "default", "ssbm")
    const char* description;    // Human-readable description

    // Button mappings (input → output)
    usbr_button_map_t buttons;

    // Trigger configuration
    trigger_behavior_t l2_behavior;
    trigger_behavior_t r2_behavior;
    uint8_t l2_threshold;       // Analog threshold for digital activation (0-255)
    uint8_t r2_threshold;       // Analog threshold for digital activation (0-255)
    uint8_t l2_analog_value;    // Custom analog value for TRIGGER_LIGHT_PRESS
    uint8_t r2_analog_value;    // Custom analog value for TRIGGER_LIGHT_PRESS

    // Analog stick settings
    float left_stick_sensitivity;   // 0.0-1.0 (1.0 = 100%)
    float right_stick_sensitivity;  // 0.0-1.0 (0.0 = disabled)

    // DualSense adaptive trigger feedback
    bool adaptive_triggers;

} usbr_profile_t;

// ============================================================================
// PROFILE SYSTEM API
// ============================================================================

// Profile system configuration (provided by app/device)
typedef struct {
    const usbr_profile_t* profiles;     // Array of profiles
    uint8_t profile_count;              // Number of profiles
    uint8_t default_index;              // Default profile index
} profile_config_t;

// Initialize profile system with universal profile configuration
void profile_init(const profile_config_t* config);

// Initialize profile system in simple mode (device maintains its own profile structures)
// Useful when device has complex/legacy profile format but wants core switching logic
void profile_init_simple(uint8_t count, uint8_t default_index, const char* const* names);

// Set player count callback (output device provides this for feedback)
void profile_set_player_count_callback(uint8_t (*callback)(void));

// Set callback for when profile switches (device updates its own profile pointer)
void profile_set_switch_callback(void (*callback)(uint8_t new_index));

// Get current profile
const usbr_profile_t* profile_get_active(void);
uint8_t profile_get_active_index(void);
uint8_t profile_get_count(void);
const char* profile_get_name(uint8_t index);

// Switch profiles (with feedback)
void profile_set_active(uint8_t index);
void profile_cycle_next(void);
void profile_cycle_prev(void);

// Check for profile switch combo (call from output device's update loop)
// Returns true if combo triggered a profile switch
void profile_check_switch_combo(uint32_t buttons);

// Load/save profile index from flash
uint8_t profile_load_from_flash(uint8_t default_index);
void profile_save_to_flash(void);

// ============================================================================
// DEFAULT PROFILE MACROS
// ============================================================================
// Helper macros to define common profile configurations

// Standard 1:1 mapping (input matches output position)
#define USBR_BUTTON_MAP_DEFAULT { \
    .b1 = USBR_OUT_B1, \
    .b2 = USBR_OUT_B2, \
    .b3 = USBR_OUT_B3, \
    .b4 = USBR_OUT_B4, \
    .l1 = USBR_OUT_L1, \
    .r1 = USBR_OUT_R1, \
    .s1 = USBR_OUT_S1, \
    .s2 = USBR_OUT_S2, \
    .l3 = USBR_OUT_L3, \
    .r3 = USBR_OUT_R3, \
    .a1 = USBR_OUT_A1, \
    .a2 = USBR_OUT_A2, \
}

// Default profile with standard settings
#define USBR_PROFILE_DEFAULT { \
    .name = "default", \
    .description = "Standard 1:1 mapping", \
    .buttons = USBR_BUTTON_MAP_DEFAULT, \
    .l2_behavior = TRIGGER_PASSTHROUGH, \
    .r2_behavior = TRIGGER_PASSTHROUGH, \
    .l2_threshold = 128, \
    .r2_threshold = 128, \
    .l2_analog_value = 0, \
    .r2_analog_value = 0, \
    .left_stick_sensitivity = 1.0f, \
    .right_stick_sensitivity = 1.0f, \
    .adaptive_triggers = false, \
}

#endif // CORE_PROFILE_H
