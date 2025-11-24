// 3do_config.h - 3DO Controller Profile Configuration
//
// This file defines button mapping profiles that can be switched at runtime.
// Profiles are stored in flash memory and selected via button combo.
//
// PROFILE SWITCHING:
// Hold SELECT for 2 seconds, then press D-pad Up/Down to cycle through profiles
// Profile selection is indicated via NeoPixel LED and saved to flash

#ifndef TDO_CONFIG_H
#define TDO_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// 3DO OUTPUT ACTIONS
// ============================================================================
// All possible button outputs on 3DO controller
// These can be assigned to any USBRetro input button

typedef enum {
    TDO_BTN_NONE = 0,

    // Joypad buttons (7 buttons)
    TDO_BTN_A,
    TDO_BTN_B,
    TDO_BTN_C,
    TDO_BTN_X,
    TDO_BTN_L,
    TDO_BTN_R,
    TDO_BTN_P,

    // Joystick mode adds FIRE button (8th button)
    TDO_BTN_FIRE,
} tdo_button_output_t;

// ============================================================================
// PROFILE STRUCTURE
// ============================================================================

typedef struct {
    const char* name;           // Profile name (for debugging)
    const char* description;    // Human-readable description

    // ========================================================================
    // JOYPAD MODE MAPPINGS (7 buttons + D-pad)
    // ========================================================================
    struct {
        tdo_button_output_t b1_button;   // USBRetro B1 → 3DO ?
        tdo_button_output_t b2_button;   // USBRetro B2 → 3DO ?
        tdo_button_output_t b3_button;   // USBRetro B3 → 3DO ?
        tdo_button_output_t b4_button;   // USBRetro B4 → 3DO ?
        tdo_button_output_t l1_button;   // USBRetro L1 → 3DO ?
        tdo_button_output_t l2_button;   // USBRetro L2 → 3DO ?
        tdo_button_output_t r1_button;   // USBRetro R1 → 3DO ?
        tdo_button_output_t r2_button;   // USBRetro R2 → 3DO ?
        tdo_button_output_t s1_button;   // USBRetro S1 → 3DO ?
        tdo_button_output_t s2_button;   // USBRetro S2 → 3DO ?
    } joypad;

    // ========================================================================
    // JOYSTICK MODE MAPPINGS (8 buttons + D-pad + 4 analog axes)
    // ========================================================================
    struct {
        tdo_button_output_t b1_button;   // USBRetro B1 → 3DO ?
        tdo_button_output_t b2_button;   // USBRetro B2 → 3DO ?
        tdo_button_output_t b3_button;   // USBRetro B3 → 3DO ?
        tdo_button_output_t b4_button;   // USBRetro B4 → 3DO ?
        tdo_button_output_t l1_button;   // USBRetro L1 → 3DO ?
        tdo_button_output_t l2_button;   // USBRetro L2 → 3DO ?
        tdo_button_output_t r1_button;   // USBRetro R1 → 3DO ?
        tdo_button_output_t r2_button;   // USBRetro R2 → 3DO ?
        tdo_button_output_t s1_button;   // USBRetro S1 → 3DO ?
        tdo_button_output_t s2_button;   // USBRetro S2 → 3DO ?
    } joystick;
} tdo_profile_t;

// ============================================================================
// PROFILE DEFINITIONS
// ============================================================================

// Profile 0: Default SNES23DO Layout
// Standard mapping matching SNES button positions
#define TDO_PROFILE_DEFAULT { \
    .name = "Default", \
    .description = "Standard SNES23DO layout", \
    .joypad = { \
        .b1_button = TDO_BTN_B,      /* B1 (A/Cross) → B */ \
        .b2_button = TDO_BTN_C,      /* B2 (B/Circle) → C */ \
        .b3_button = TDO_BTN_A,      /* B3 (X/Square) → A */ \
        .b4_button = TDO_BTN_NONE,   /* B4 (Y/Triangle) → (see S2) */ \
        .l1_button = TDO_BTN_L,      /* L1 → L */ \
        .l2_button = TDO_BTN_L,      /* L2 → L (OR with L1) */ \
        .r1_button = TDO_BTN_R,      /* R1 → R */ \
        .r2_button = TDO_BTN_R,      /* R2 → R (OR with R1) */ \
        .s1_button = TDO_BTN_X,      /* S1 (Select/Share) → X */ \
        .s2_button = TDO_BTN_P,      /* S2 (Start/Options) → P */ \
    }, \
    .joystick = { \
        .b1_button = TDO_BTN_FIRE,   /* B1 (A/Cross) → FIRE */ \
        .b2_button = TDO_BTN_C,      /* B2 (B/Circle) → C */ \
        .b3_button = TDO_BTN_A,      /* B3 (X/Square) → A */ \
        .b4_button = TDO_BTN_NONE,   /* B4 (Y/Triangle) → (see S2) */ \
        .l1_button = TDO_BTN_L,      /* L1 → L */ \
        .l2_button = TDO_BTN_L,      /* L2 → L (OR with L1) */ \
        .r1_button = TDO_BTN_R,      /* R1 → R */ \
        .r2_button = TDO_BTN_R,      /* R2 → R (OR with R1) */ \
        .s1_button = TDO_BTN_X,      /* S1 (Select/Share) → X */ \
        .s2_button = TDO_BTN_P,      /* S2 (Start/Options) → P */ \
    }, \
}

// Profile 1: Fighting Games
// Optimized 6-button layout for fighting games
#define TDO_PROFILE_FIGHTING { \
    .name = "Fighting", \
    .description = "6-button fighting game layout", \
    .joypad = { \
        .b1_button = TDO_BTN_B,      /* B1 → Light Punch (B) */ \
        .b2_button = TDO_BTN_C,      /* B2 → Medium Punch (C) */ \
        .b3_button = TDO_BTN_A,      /* B3 → Heavy Punch (A) */ \
        .b4_button = TDO_BTN_P,      /* B4 → Light Kick (P) */ \
        .l1_button = TDO_BTN_L,      /* L1 → Medium Kick (L) */ \
        .l2_button = TDO_BTN_R,      /* L2 → Medium Kick (R) */ \
        .r1_button = TDO_BTN_R,      /* R1 → Heavy Kick (R) */ \
        .r2_button = TDO_BTN_L,      /* R2 → Heavy Kick (L) */ \
        .s1_button = TDO_BTN_X,      /* S1 → X */ \
        .s2_button = TDO_BTN_P,      /* S2 → P */ \
    }, \
    .joystick = { \
        .b1_button = TDO_BTN_B,      /* B1 → Light Punch (B) */ \
        .b2_button = TDO_BTN_C,      /* B2 → Medium Punch (C) */ \
        .b3_button = TDO_BTN_A,      /* B3 → Heavy Punch (A) */ \
        .b4_button = TDO_BTN_P,      /* B4 → Light Kick (P) */ \
        .l1_button = TDO_BTN_L,      /* L1 → Medium Kick (L) */ \
        .l2_button = TDO_BTN_R,      /* L2 → Medium Kick (R) */ \
        .r1_button = TDO_BTN_R,      /* R1 → Heavy Kick (R) */ \
        .r2_button = TDO_BTN_L,      /* R2 → Heavy Kick (L) */ \
        .s1_button = TDO_BTN_X,      /* S1 → X */ \
        .s2_button = TDO_BTN_P,      /* S2 → P */ \
    }, \
}

// Profile 2: Shooter Layout
// Optimized for shooters with shoulder buttons as primary fire
#define TDO_PROFILE_SHOOTER { \
    .name = "Shooter", \
    .description = "Shooter-optimized layout", \
    .joypad = { \
        .b1_button = TDO_BTN_C,      /* B1 → Jump (C) */ \
        .b2_button = TDO_BTN_B,      /* B2 → Action (B) */ \
        .b3_button = TDO_BTN_A,      /* B3 → Weapon Switch (A) */ \
        .b4_button = TDO_BTN_X,      /* B4 → Special (X) */ \
        .l1_button = TDO_BTN_L,      /* L1 → Primary Fire (L) */ \
        .l2_button = TDO_BTN_L,      /* L2 → Primary Fire (L) */ \
        .r1_button = TDO_BTN_R,      /* R1 → Secondary Fire (R) */ \
        .r2_button = TDO_BTN_R,      /* R2 → Secondary Fire (R) */ \
        .s1_button = TDO_BTN_NONE,   /* S1 → (unused) */ \
        .s2_button = TDO_BTN_P,      /* S2 → Pause (P) */ \
    }, \
    .joystick = { \
        .b1_button = TDO_BTN_FIRE,   /* B1 → Primary Fire (FIRE) */ \
        .b2_button = TDO_BTN_B,      /* B2 → Action (B) */ \
        .b3_button = TDO_BTN_A,      /* B3 → Weapon Switch (A) */ \
        .b4_button = TDO_BTN_X,      /* B4 → Special (X) */ \
        .l1_button = TDO_BTN_L,      /* L1 → Jump (L) */ \
        .l2_button = TDO_BTN_L,      /* L2 → Jump (L) */ \
        .r1_button = TDO_BTN_R,      /* R1 → Secondary Fire (R) */ \
        .r2_button = TDO_BTN_R,      /* R2 → Secondary Fire (R) */ \
        .s1_button = TDO_BTN_NONE,   /* S1 → (unused) */ \
        .s2_button = TDO_BTN_P,      /* S2 → Pause (P) */ \
    }, \
}

// ============================================================================
// PROFILE COUNT AND DEFAULT
// ============================================================================

#define TDO_PROFILE_COUNT 3
#define TDO_DEFAULT_PROFILE_INDEX 0

#endif // TDO_CONFIG_H
