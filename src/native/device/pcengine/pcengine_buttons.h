// pcengine_buttons.h - PCEngine/TurboGrafx-16 Button Aliases
//
// Aliases for USBR_BUTTON_* constants that map to PCEngine button names.
// These are purely for readability in profile definitions.
//
// Example usage in a profile:
//   { .input = USBR_BUTTON_B1, .output = PCE_BUTTON_I }
//   This reads: "USBR B1 maps to PCEngine button I"

#ifndef PCENGINE_BUTTONS_H
#define PCENGINE_BUTTONS_H

#include "core/buttons.h"

// ============================================================================
// PCENGINE BUTTON ALIASES
// ============================================================================
// These aliases equal the USBR_BUTTON_* values they represent on PCEngine.
// The mapping reflects the default/natural position on a PCEngine controller.

// Face buttons (2-button mode)
#define PCE_BUTTON_I        USBR_BUTTON_B1  // I (primary action)
#define PCE_BUTTON_II       USBR_BUTTON_B2  // II (secondary action)

// 6-button mode (Avenue Pad 6)
#define PCE_BUTTON_III      USBR_BUTTON_B3  // III
#define PCE_BUTTON_IV       USBR_BUTTON_B4  // IV
#define PCE_BUTTON_V        USBR_BUTTON_L1  // V
#define PCE_BUTTON_VI       USBR_BUTTON_R1  // VI

// System buttons
#define PCE_BUTTON_SELECT   USBR_BUTTON_S1  // Select
#define PCE_BUTTON_RUN      USBR_BUTTON_S2  // Run (Start)

// D-pad
#define PCE_BUTTON_DU       USBR_BUTTON_DU  // D-pad Up
#define PCE_BUTTON_DD       USBR_BUTTON_DD  // D-pad Down
#define PCE_BUTTON_DL       USBR_BUTTON_DL  // D-pad Left
#define PCE_BUTTON_DR       USBR_BUTTON_DR  // D-pad Right

// ============================================================================
// PCENGINE-SPECIFIC OUTPUT ACTIONS
// ============================================================================
// Special outputs for PCEngine turbo modes

typedef enum {
    PCE_ACTION_NONE = 0,

    // Turbo button variants
    PCE_ACTION_TURBO_I,         // Button I with turbo
    PCE_ACTION_TURBO_II,        // Button II with turbo

} pce_action_t;

#endif // PCENGINE_BUTTONS_H
