// button.h - User button input service
// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Robert Dale Smith
//
// Detects click, double-click, and hold events from the board's user button.
// Used for mode switching and other user interactions.

#ifndef BUTTON_H
#define BUTTON_H

#include <stdint.h>
#include <stdbool.h>

// Button event types
typedef enum {
    BUTTON_EVENT_NONE = 0,
    BUTTON_EVENT_CLICK,         // Single short press
    BUTTON_EVENT_DOUBLE_CLICK,  // Two presses in quick succession
    BUTTON_EVENT_HOLD,          // Long press (fires once when threshold reached)
    BUTTON_EVENT_RELEASE,       // Released after hold
} button_event_t;

// Button event callback type
typedef void (*button_callback_t)(button_event_t event);

// ============================================================================
// CONFIGURATION
// ============================================================================

// Default GPIO pin for user button (can be overridden per-board)
#ifndef BUTTON_USER_GPIO
#define BUTTON_USER_GPIO 7  // Boot button on Feather RP2040
#endif

// Timing configuration (in milliseconds)
#define BUTTON_DEBOUNCE_MS      20    // Debounce time
#define BUTTON_CLICK_MAX_MS     500   // Max press duration for a click
#define BUTTON_DOUBLE_CLICK_MS  300   // Max gap between clicks for double-click
#define BUTTON_HOLD_MS          1500  // Hold duration to trigger hold event

// ============================================================================
// PUBLIC API
// ============================================================================

// Initialize the button service
void button_init(void);

// Process button state (call from main loop)
// Returns the current event, if any
button_event_t button_task(void);

// Register a callback for button events
void button_set_callback(button_callback_t callback);

// Get current button state (true = pressed)
bool button_is_pressed(void);

// Get time button has been held (0 if not pressed)
uint32_t button_held_ms(void);

#endif // BUTTON_H
