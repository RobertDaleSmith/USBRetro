// input_event.h
// Unified Input Event System for USBRetro
// Supports all device types with extensible analog axis arrays

#ifndef INPUT_EVENT_H
#define INPUT_EVENT_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// ============================================================================
// Device Type Classification
// ============================================================================

typedef enum {
    INPUT_TYPE_NONE = 0,        // Uninitialized / disconnected
    INPUT_TYPE_GAMEPAD,         // Standard gamepad (Xbox, PlayStation, Switch, etc.)
    INPUT_TYPE_FLIGHTSTICK,     // Flight stick with 3+ axes (Thrustmaster, Logitech, etc.)
    INPUT_TYPE_WHEEL,           // Racing wheel with pedals (Logitech G29, Thrustmaster, etc.)
    INPUT_TYPE_MOUSE,           // Mouse with relative motion
    INPUT_TYPE_KEYBOARD,        // Keyboard with keys only
    INPUT_TYPE_LIGHTGUN,        // Light gun with absolute position
    INPUT_TYPE_ARCADE_STICK,    // Arcade stick (8-way joystick + buttons)
} input_device_type_t;

// ============================================================================
// Analog Axis Indices (matches USB HID usage order)
// ============================================================================

typedef enum {
    ANALOG_X = 0,       // Left stick X / Flight stick X / Steering wheel
    ANALOG_Y = 1,       // Left stick Y / Flight stick Y
    ANALOG_Z = 2,       // Right stick X / Rudder / Twist
    ANALOG_RX = 3,      // Right stick X (alt) / Throttle slider
    ANALOG_RY = 4,      // Right stick Y (alt)
    ANALOG_RZ = 5,      // Triggers / Brake pedal
    ANALOG_SLIDER = 6,  // Throttle / Gas pedal
    ANALOG_DIAL = 7,    // Extra slider / Clutch pedal
} analog_axis_index_t;

// ============================================================================
// Unified Input Event Structure
// ============================================================================

typedef struct {
    // Device identification
    uint8_t dev_addr;           // USB device address
    int8_t instance;            // Instance number (for multi-controller devices)
    input_device_type_t type;   // Device type classification

    // Digital inputs
    uint32_t buttons;           // Button bitmap (USBR_BUTTON_* defines from globals.h)
    uint32_t keys;              // Keyboard keys (modifier + scancodes)

    // Absolute analog inputs (0-255, centered at 128)
    // All values are normalized regardless of device type
    uint8_t analog[8];          // 8 analog axes (see analog_axis_index_t)
                                // [0] = X-axis (Left stick X / Flight stick X / Steering)
                                // [1] = Y-axis (Left stick Y / Flight stick Y)
                                // [2] = Z-axis (Right stick X / Rudder / Twist)
                                // [3] = RX-axis (Right stick X alt / Throttle)
                                // [4] = RY-axis (Right stick Y alt)
                                // [5] = RZ-axis (Triggers / Brake)
                                // [6] = Slider (Throttle / Gas pedal)
                                // [7] = Dial (Extra slider / Clutch)

    // Relative inputs (mouse, spinner, trackball)
    int8_t delta_x;             // Horizontal delta (-127 to +127)
    int8_t delta_y;             // Vertical delta (-127 to +127)
    int8_t delta_wheel;         // Scroll wheel delta

    // Hat switches / D-pad alternatives (encoded as 8-direction)
    uint8_t hat[4];             // Up to 4 hat switches
                                // Values: 0-7 = direction, 0xFF = centered
                                // 0=N, 1=NE, 2=E, 3=SE, 4=S, 5=SW, 6=W, 7=NW

    // Extended features (for future use)
    bool has_rumble;            // Device supports rumble
    bool has_force_feedback;    // Device supports force feedback
} input_event_t;

// ============================================================================
// Helper Functions
// ============================================================================

// Initialize event with safe defaults
static inline void init_input_event(input_event_t* event) {
    memset(event, 0, sizeof(input_event_t));

    // Set analog axes to centered position (128 = neutral)
    for (int i = 0; i < 8; i++) {
        event->analog[i] = 128;
    }

    // Set hat switches to centered
    for (int i = 0; i < 4; i++) {
        event->hat[i] = 0xFF;
    }

    event->type = INPUT_TYPE_NONE;
}

// Convert old post_globals() parameters to input_event_t (for migration)
static inline void gamepad_to_input_event(
    input_event_t* event,
    uint8_t dev_addr,
    int8_t instance,
    uint32_t buttons,
    uint8_t analog_1x, uint8_t analog_1y,
    uint8_t analog_2x, uint8_t analog_2y,
    uint8_t analog_l, uint8_t analog_r,
    uint32_t keys,
    uint8_t quad_x)  // Ignored - consoles accumulate delta_x into spinner
{
    init_input_event(event);

    event->dev_addr = dev_addr;
    event->instance = instance;
    event->type = INPUT_TYPE_GAMEPAD;
    event->buttons = buttons;
    event->keys = keys;

    // Map to standard gamepad layout
    event->analog[ANALOG_X] = analog_1x;      // Left stick X
    event->analog[ANALOG_Y] = analog_1y;      // Left stick Y
    event->analog[ANALOG_Z] = analog_2x;      // Right stick X
    event->analog[ANALOG_RX] = analog_2y;     // Right stick Y
    event->analog[ANALOG_RZ] = analog_l;      // Left trigger
    event->analog[ANALOG_SLIDER] = analog_r;  // Right trigger
}

// Convert old post_mouse_globals() parameters to input_event_t (for migration)
static inline void mouse_to_input_event(
    input_event_t* event,
    uint8_t dev_addr,
    int8_t instance,
    uint16_t buttons,
    uint8_t delta_x,
    uint8_t delta_y,
    uint8_t spinner)  // Ignored - consoles accumulate delta_x into spinner
{
    init_input_event(event);

    event->dev_addr = dev_addr;
    event->instance = instance;
    event->type = INPUT_TYPE_MOUSE;
    event->buttons = buttons;
    event->delta_x = (int8_t)delta_x;
    event->delta_y = (int8_t)delta_y;
}

#endif // INPUT_EVENT_H
