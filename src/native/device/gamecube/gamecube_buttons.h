// gamecube_buttons.h - GameCube Button Aliases
//
// Aliases for USBR_BUTTON_* constants that map to GameCube button names.
// These are purely for readability in profile definitions.
//
// Example usage in a profile:
//   { .input = USBR_BUTTON_B1, .output = GC_BUTTON_A }
//   This reads: "USBR B1 maps to GameCube A button"

#ifndef GAMECUBE_BUTTONS_H
#define GAMECUBE_BUTTONS_H

#include "core/buttons.h"

// ============================================================================
// GAMECUBE BUTTON ALIASES
// ============================================================================
// These aliases equal the USBR_BUTTON_* values they represent on GameCube.
// The mapping reflects the default/natural position on a GameCube controller.

// Face buttons
#define GC_BUTTON_A         USBR_BUTTON_B1  // A (large green button)
#define GC_BUTTON_B         USBR_BUTTON_B2  // B (small red button)
#define GC_BUTTON_X         USBR_BUTTON_B4  // X (right of A)
#define GC_BUTTON_Y         USBR_BUTTON_B3  // Y (above A)

// Shoulder/Trigger buttons
#define GC_BUTTON_Z         USBR_BUTTON_R1  // Z (digital, above R)
#define GC_BUTTON_L         USBR_BUTTON_L2  // L trigger (analog + digital)
#define GC_BUTTON_R         USBR_BUTTON_R2  // R trigger (analog + digital)

// System
#define GC_BUTTON_START     USBR_BUTTON_S2  // Start

// D-pad
#define GC_BUTTON_DU        USBR_BUTTON_DU  // D-pad Up
#define GC_BUTTON_DD        USBR_BUTTON_DD  // D-pad Down
#define GC_BUTTON_DL        USBR_BUTTON_DL  // D-pad Left
#define GC_BUTTON_DR        USBR_BUTTON_DR  // D-pad Right

// ============================================================================
// GAMECUBE-SPECIFIC OUTPUT ACTIONS
// ============================================================================
// These are special output behaviors specific to GameCube that go beyond
// simple button presses. Used in gc_profile_t for trigger behaviors.

typedef enum {
    GC_ACTION_NONE = 0,

    // Trigger analog behaviors
    GC_ACTION_L_ANALOG,         // L analog passthrough
    GC_ACTION_R_ANALOG,         // R analog passthrough
    GC_ACTION_L_FULL,           // L digital + analog forced to 255
    GC_ACTION_R_FULL,           // R digital + analog forced to 255
    GC_ACTION_L_LIGHT,          // L analog at custom value (light shield)
    GC_ACTION_R_LIGHT,          // R analog at custom value
    GC_ACTION_LR_BOTH,          // Both L+R digital (for SSBM quit combo)

    // C-stick directions (forces C-stick to specific position)
    GC_ACTION_C_UP,             // C-stick up
    GC_ACTION_C_DOWN,           // C-stick down
    GC_ACTION_C_LEFT,           // C-stick left
    GC_ACTION_C_RIGHT,          // C-stick right

} gc_action_t;

#endif // GAMECUBE_BUTTONS_H
