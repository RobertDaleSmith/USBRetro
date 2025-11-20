// gamecube_config.h - GameCube Controller Profile Configuration
//
// This file defines button mapping profiles that can be switched at runtime.
// Profiles are stored in flash memory and selected via button combo.
//
// PROFILE SWITCHING:
// Hold SELECT + START + L1 + R1 for 2 seconds to cycle through profiles
// Profile selection prints to UART debug output

#ifndef GAMECUBE_CONFIG_H
#define GAMECUBE_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// GAMECUBE OUTPUT ACTIONS
// ============================================================================
// All possible outputs on GameCube controller
// These can be assigned to any USBRetro input button

typedef enum {
    // No action
    GC_BTN_NONE = 0,

    // Digital buttons
    GC_BTN_A,                   // A button
    GC_BTN_B,                   // B button
    GC_BTN_X,                   // X button
    GC_BTN_Y,                   // Y button
    GC_BTN_Z,                   // Z button
    GC_BTN_START,               // Start button

    // D-pad directions
    GC_BTN_DPAD_UP,
    GC_BTN_DPAD_DOWN,
    GC_BTN_DPAD_LEFT,
    GC_BTN_DPAD_RIGHT,

    // Shoulder buttons (digital + analog combinations)
    GC_BTN_L,                   // L digital only
    GC_BTN_R,                   // R digital only
    GC_BTN_L_FULL,              // L digital + L analog forced to 255
    GC_BTN_R_FULL,              // R digital + R analog forced to 255
    GC_BTN_L_LIGHT,             // L analog forced to 1 (light shield for SSBM)

    // C-stick directions (forces C-stick to specific position)
    GC_BTN_C_UP,                // C-stick forced up (255)
    GC_BTN_C_DOWN,              // C-stick forced down (0)
    GC_BTN_C_LEFT,              // C-stick forced left (0)
    GC_BTN_C_RIGHT,             // C-stick forced right (255)

} gc_button_output_t;

// Trigger analog behavior (for L2/R2)
typedef enum {
    GC_TRIGGER_NONE = 0,            // No action
    GC_TRIGGER_L_THRESHOLD,         // L digital at threshold + L analog passthrough
    GC_TRIGGER_R_THRESHOLD,         // R digital at threshold + R analog passthrough
    GC_TRIGGER_L_FULL,              // L digital + L analog forced to 255
    GC_TRIGGER_R_FULL,              // R digital + R analog forced to 255
    GC_TRIGGER_Z_INSTANT,           // Z button (uses trigger threshold)
    GC_TRIGGER_L_CUSTOM,            // L digital at threshold + custom L analog value (uses l2_analog_value)
    GC_TRIGGER_R_CUSTOM,            // R digital at threshold + custom R analog value (uses r2_analog_value)
    GC_TRIGGER_LR_BOTH,             // Both L and R digital at threshold (for SSBM quit combo)
} gc_trigger_behavior_t;

// ============================================================================
// PROFILE STRUCTURE
// ============================================================================

typedef struct {
    const char* name;           // Profile name (for debugging)
    const char* description;    // Human-readable description

    // Trigger thresholds (0-255)
    uint8_t l2_threshold;       // LT analog threshold for digital action
    uint8_t r2_threshold;       // RT analog threshold for digital action

    // Custom trigger analog values (for GC_TRIGGER_L/R_CUSTOM behaviors)
    uint8_t l2_analog_value;    // Custom L analog value (0 = use passthrough)
    uint8_t r2_analog_value;    // Custom R analog value (0 = use passthrough)

    // Stick sensitivity (0.0-1.0, typically 1.0 = 100%)
    float left_stick_sensitivity;
    float right_stick_sensitivity;

    // ========================================================================
    // COMPLETE BUTTON MAPPING
    // Map every USBRetro button to any GameCube output
    // ========================================================================

    // Face buttons (B1-B4)
    gc_button_output_t b1_button;   // USBRetro B1 → GameCube ?
    gc_button_output_t b2_button;   // USBRetro B2 → GameCube ?
    gc_button_output_t b3_button;   // USBRetro B3 → GameCube ?
    gc_button_output_t b4_button;   // USBRetro B4 → GameCube ?

    // Shoulder buttons (L1/R1)
    gc_button_output_t l1_button;   // USBRetro L1 → GameCube ?
    gc_button_output_t r1_button;   // USBRetro R1 → GameCube ?

    // System buttons (S1/S2 - Select/Start)
    gc_button_output_t s1_button;   // USBRetro S1 (Select/Back) → GameCube ?
    gc_button_output_t s2_button;   // USBRetro S2 (Start) → GameCube ?

    // Stick buttons (L3/R3)
    gc_button_output_t l3_button;   // USBRetro L3 → GameCube ?
    gc_button_output_t r3_button;   // USBRetro R3 → GameCube ?

    // Auxiliary buttons (A1/A2 - Home/Capture)
    gc_button_output_t a1_button;   // USBRetro A1 (Guide/Home) → GameCube ?
    gc_button_output_t a2_button;   // USBRetro A2 (Capture/Touchpad) → GameCube ?

    // Trigger behavior (L2/R2 - separate from button mapping)
    gc_trigger_behavior_t l2_behavior;  // How LT trigger behaves
    gc_trigger_behavior_t r2_behavior;  // How RT trigger behaves

} gc_profile_t;

// ============================================================================
// PROFILE DEFINITIONS
// ============================================================================
// To add a new profile:
// 1. Copy an existing profile block below
// 2. Modify the values to your preference
// 3. Add it to the profiles[] array in gamecube.c
// 4. Increment GC_PROFILE_COUNT

// ----------------------------------------------------------------------------
// Profile: default - Standard GameCube mapping
// ----------------------------------------------------------------------------
#define GC_PROFILE_DEFAULT { \
    .name = "default", \
    .description = "Standard mapping matching GameCube layout", \
    \
    /* Thresholds */ \
    .l2_threshold = 250, \
    .r2_threshold = 250, \
    \
    /* Custom trigger analog values */ \
    .l2_analog_value = 0, \
    .r2_analog_value = 0, \
    \
    /* Stick sensitivity */ \
    .left_stick_sensitivity = 1.0f, \
    .right_stick_sensitivity = 1.0f, \
    \
    /* Face buttons (B1-B4) */ \
    .b1_button = GC_BTN_B,          /* B1 → B (bottom face button) */ \
    .b2_button = GC_BTN_A,          /* B2 → A (right face button) */ \
    .b3_button = GC_BTN_Y,          /* B3 → Y (left face button) */ \
    .b4_button = GC_BTN_X,          /* B4 → X (top face button) */ \
    \
    /* Shoulder buttons (L1/R1) */ \
    .l1_button = GC_BTN_NONE,       /* L1 → nothing */ \
    .r1_button = GC_BTN_Z,          /* R1 → Z */ \
    \
    /* System buttons (S1/S2) */ \
    .s1_button = GC_BTN_NONE,       /* S1 (Select) → nothing */ \
    .s2_button = GC_BTN_START,      /* S2 (Start) → Start */ \
    \
    /* Stick buttons (L3/R3) */ \
    .l3_button = GC_BTN_NONE,       /* L3 → nothing */ \
    .r3_button = GC_BTN_NONE,       /* R3 → nothing */ \
    \
    /* Auxiliary buttons (A1/A2) */ \
    .a1_button = GC_BTN_NONE,       /* A1 (Home) → nothing */ \
    .a2_button = GC_BTN_NONE,       /* A2 (Capture) → nothing */ \
    \
    /* Trigger behavior */ \
    .l2_behavior = GC_TRIGGER_L_THRESHOLD,  /* LT → L button at threshold + analog */ \
    .r2_behavior = GC_TRIGGER_R_THRESHOLD,  /* RT → R button at threshold + analog */ \
}

// ----------------------------------------------------------------------------
// Profile: snes - Original SNES controller mapping
// ----------------------------------------------------------------------------
#define GC_PROFILE_SNES { \
    .name = "snes", \
    .description = "Original SNES mapping: Select→Z, LB/RB→L/R(full)", \
    \
    /* Thresholds */ \
    .l2_threshold = 250, \
    .r2_threshold = 250, \
    \
    /* Custom trigger analog values */ \
    .l2_analog_value = 0, \
    .r2_analog_value = 0, \
    \
    /* Stick sensitivity */ \
    .left_stick_sensitivity = 1.0f, \
    .right_stick_sensitivity = 1.0f, \
    \
    /* Face buttons (B1-B4) */ \
    .b1_button = GC_BTN_B,          /* B1 → B */ \
    .b2_button = GC_BTN_A,          /* B2 → A */ \
    .b3_button = GC_BTN_Y,          /* B3 → Y */ \
    .b4_button = GC_BTN_X,          /* B4 → X */ \
    \
    /* Shoulder buttons (L1/R1) */ \
    .l1_button = GC_BTN_L_FULL,     /* L1 → L digital + L analog at 255 */ \
    .r1_button = GC_BTN_R_FULL,     /* R1 → R digital + R analog at 255 */ \
    \
    /* System buttons (S1/S2) */ \
    .s1_button = GC_BTN_Z,          /* S1 (Select) → Z */ \
    .s2_button = GC_BTN_START,      /* S2 (Start) → Start */ \
    \
    /* Stick buttons (L3/R3) */ \
    .l3_button = GC_BTN_NONE,       /* L3 → nothing */ \
    .r3_button = GC_BTN_NONE,       /* R3 → nothing */ \
    \
    /* Auxiliary buttons (A1/A2) */ \
    .a1_button = GC_BTN_NONE,       /* A1 (Home) → nothing */ \
    .a2_button = GC_BTN_NONE,       /* A2 (Capture) → nothing */ \
    \
    /* Trigger behavior */ \
    .l2_behavior = GC_TRIGGER_L_THRESHOLD,  /* LT → L button at threshold + analog */ \
    .r2_behavior = GC_TRIGGER_R_THRESHOLD,  /* RT → R button at threshold + analog */ \
}

// ----------------------------------------------------------------------------
// Profile: ssbm - Super Smash Bros Melee competitive mapping (Yoink1975's config)
// ----------------------------------------------------------------------------
#define GC_PROFILE_SSBM { \
    .name = "ssbm", \
    .description = "SSBM: LB→Z, LT→Light(43), RT→L+R, RB→X, 85% stick", \
    \
    /* Thresholds */ \
    .l2_threshold = 225,        /* LT threshold for L digital (88%) */ \
    .r2_threshold = 140,        /* RT threshold for L+R digital (55%) */ \
    \
    /* Custom trigger analog values */ \
    .l2_analog_value = 43,      /* L analog at 43 (~17% light shield) */ \
    .r2_analog_value = 0, \
    \
    /* Stick sensitivity (85% for Melee precision) */ \
    .left_stick_sensitivity = 0.85f, \
    .right_stick_sensitivity = 1.0f, \
    \
    /* Face buttons (B1-B4) */ \
    .b1_button = GC_BTN_B,          /* B1 → B */ \
    .b2_button = GC_BTN_A,          /* B2 → A */ \
    .b3_button = GC_BTN_Y,          /* B3 (X) → Y */ \
    .b4_button = GC_BTN_X,          /* B4 (Y) → X */ \
    \
    /* Shoulder buttons (L1/R1) */ \
    .l1_button = GC_BTN_Z,          /* L1 (LB) → Z */ \
    .r1_button = GC_BTN_X,          /* R1 (RB) → X */ \
    \
    /* System buttons (S1/S2) */ \
    .s1_button = GC_BTN_NONE,       /* S1 (Select) → nothing */ \
    .s2_button = GC_BTN_START,      /* S2 (Start) → Start */ \
    \
    /* Stick buttons (L3/R3) */ \
    .l3_button = GC_BTN_A,          /* L3 → A (for testing) */ \
    .r3_button = GC_BTN_NONE,       /* R3 → nothing */ \
    \
    /* Auxiliary buttons (A1/A2) */ \
    .a1_button = GC_BTN_NONE,       /* A1 (Home) → nothing */ \
    .a2_button = GC_BTN_NONE,       /* A2 (Capture) → nothing */ \
    \
    /* Trigger behavior */ \
    .l2_behavior = GC_TRIGGER_L_CUSTOM,    /* LT → L digital + custom analog (43) */ \
    .r2_behavior = GC_TRIGGER_LR_BOTH,     /* RT → L+R both (for quit combo) */ \
}

// ----------------------------------------------------------------------------
// Profile: mkwii - Mario Kart Wii drift mapping (Eggzact123's config)
// ----------------------------------------------------------------------------
#define GC_PROFILE_MKWII { \
    .name = "mkwii", \
    .description = "Mario Kart Wii drift: RB→R(full), RT→Z(instant), LB→D-pad Up", \
    \
    /* Thresholds */ \
    .l2_threshold = 250, \
    .r2_threshold = 10,             /* Very sensitive RT for instant Z */ \
    \
    /* Custom trigger analog values */ \
    .l2_analog_value = 0, \
    .r2_analog_value = 0, \
    \
    /* Stick sensitivity */ \
    .left_stick_sensitivity = 1.0f, \
    .right_stick_sensitivity = 1.0f, \
    \
    /* Face buttons (B1-B4) */ \
    .b1_button = GC_BTN_B,          /* B1 → B */ \
    .b2_button = GC_BTN_A,          /* B2 → A */ \
    .b3_button = GC_BTN_Y,          /* B3 → Y */ \
    .b4_button = GC_BTN_X,          /* B4 → X */ \
    \
    /* Shoulder buttons (L1/R1) */ \
    .l1_button = GC_BTN_DPAD_UP,    /* L1 → D-pad Up */ \
    .r1_button = GC_BTN_R_FULL,     /* R1 → R digital + R analog at 255 */ \
    \
    /* System buttons (S1/S2) */ \
    .s1_button = GC_BTN_NONE,       /* S1 → nothing */ \
    .s2_button = GC_BTN_START,      /* S2 → Start */ \
    \
    /* Stick buttons (L3/R3) */ \
    .l3_button = GC_BTN_NONE,       /* L3 → nothing */ \
    .r3_button = GC_BTN_NONE,       /* R3 → nothing */ \
    \
    /* Auxiliary buttons (A1/A2) */ \
    .a1_button = GC_BTN_NONE,       /* A1 → nothing */ \
    .a2_button = GC_BTN_NONE,       /* A2 → nothing */ \
    \
    /* Trigger behavior */ \
    .l2_behavior = GC_TRIGGER_L_THRESHOLD,  /* LT → L button at threshold + analog */ \
    .r2_behavior = GC_TRIGGER_Z_INSTANT,    /* RT → Z button (instant) */ \
}

// ----------------------------------------------------------------------------
// Profile: fighting - Fighting game mapping (L1→C-Up, right stick disabled)
// ----------------------------------------------------------------------------
#define GC_PROFILE_FIGHTING { \
    .name = "fighting", \
    .description = "Fighting game: L1→C-Up, right stick disabled for in-game config", \
    \
    /* Thresholds */ \
    .l2_threshold = 250, \
    .r2_threshold = 250, \
    \
    /* Custom trigger analog values */ \
    .l2_analog_value = 0, \
    .r2_analog_value = 0, \
    \
    /* Stick sensitivity */ \
    .left_stick_sensitivity = 1.0f, \
    .right_stick_sensitivity = 0.0f,  /* Right stick disabled */ \
    \
    /* Face buttons (B1-B4) */ \
    .b1_button = GC_BTN_B,          /* B1 → B */ \
    .b2_button = GC_BTN_A,          /* B2 → A */ \
    .b3_button = GC_BTN_Y,          /* B3 → Y */ \
    .b4_button = GC_BTN_X,          /* B4 → X */ \
    \
    /* Shoulder buttons (L1/R1) */ \
    .l1_button = GC_BTN_C_UP,       /* L1 → C-stick Up */ \
    .r1_button = GC_BTN_Z,          /* R1 → Z */ \
    \
    /* System buttons (S1/S2) */ \
    .s1_button = GC_BTN_NONE,       /* S1 → nothing */ \
    .s2_button = GC_BTN_START,      /* S2 → Start */ \
    \
    /* Stick buttons (L3/R3) */ \
    .l3_button = GC_BTN_NONE,       /* L3 → nothing */ \
    .r3_button = GC_BTN_NONE,       /* R3 → nothing */ \
    \
    /* Auxiliary buttons (A1/A2) */ \
    .a1_button = GC_BTN_NONE,       /* A1 → nothing */ \
    .a2_button = GC_BTN_NONE,       /* A2 → nothing */ \
    \
    /* Trigger behavior */ \
    .l2_behavior = GC_TRIGGER_L_THRESHOLD,  /* LT → L button at threshold + analog */ \
    .r2_behavior = GC_TRIGGER_R_THRESHOLD,  /* RT → R button at threshold + analog */ \
}

// Total number of profiles (update when adding new profiles)
#define GC_PROFILE_COUNT 5

// Default profile index (0 = default, 1 = mkwii, 2 = snes, 3 = ssbm)
#define GC_DEFAULT_PROFILE_INDEX 0

#endif // GAMECUBE_CONFIG_H
