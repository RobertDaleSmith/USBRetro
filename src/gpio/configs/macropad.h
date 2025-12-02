// macropad.h - Adafruit MacroPad RP2040 GPIO Configuration
// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Robert Dale Smith
//
// GPIO mappings for Adafruit MacroPad RP2040 as a gamepad.
// Based on GP2040-CE adafruit-macropad branch configuration.
// 12 keys (3x4 grid) + rotary encoder mapped to gamepad buttons.

#ifndef GPIO_CONFIG_MACROPAD_H
#define GPIO_CONFIG_MACROPAD_H

#include "../gpio_input.h"

// ============================================================================
// ADAFRUIT MACROPAD RP2040
// ============================================================================
// 12-key mechanical keyboard with rotary encoder.
// Active low buttons (pressed = GPIO low, uses internal pull-ups)
//
// Physical layout (3x4 grid):
//   [0]  [1]  [2]     -> A1(Home), R1, UP
//   [3]  [4]  [5]     -> L1, RIGHT, DOWN
//   [6]  [7]  [8]     -> S1(Select), B3, B4
//   [9]  [10] [11]    -> S2(Start), B1, B2

static const gpio_device_config_t gpio_config_macropad = {
    .name = "MacroPad",
    .active_high = false,  // MacroPad uses active low (pull-ups)

    // D-pad (mapped to keys in row 1-2)
    .dpad_up    = 2,    // Key 2 (top right)
    .dpad_down  = 5,    // Key 5 (row 2, right)
    .dpad_left  = 6,    // Key 6 (row 3, left)
    .dpad_right = 4,    // Key 4 (row 2, middle)

    // Face buttons (bottom two rows)
    .b1 = 11,   // Key 11 - A / Cross
    .b2 = 12,   // Key 12 - B / Circle
    .b3 = 8,    // Key 8 - X / Square
    .b4 = 9,    // Key 9 - Y / Triangle

    // Shoulder buttons
    .l1 = 3,    // Key 3
    .r1 = 1,    // Key 1
    .l2 = GPIO_PIN_DISABLED,
    .r2 = GPIO_PIN_DISABLED,

    // Meta buttons
    .s1 = 7,    // Key 7 - Select / Back
    .s2 = 10,   // Key 10 - Start

    // Stick clicks (not available)
    .l3 = GPIO_PIN_DISABLED,
    .r3 = GPIO_PIN_DISABLED,

    // Home button
    .a1 = 0,    // Key 0 - Home / Guide
    .a2 = GPIO_PIN_DISABLED,

    // No analog sticks (rotary encoder could be added later)
    .adc_lx = GPIO_PIN_DISABLED,
    .adc_ly = GPIO_PIN_DISABLED,
    .adc_rx = GPIO_PIN_DISABLED,
    .adc_ry = GPIO_PIN_DISABLED,

    .invert_lx = false,
    .invert_ly = false,
    .invert_rx = false,
    .invert_ry = false,
    .deadzone = 10,

    // NeoPixel on GPIO 19 (12 LEDs under keys)
    .led_pin = 19,
    .led_count = 12,
};

#endif // GPIO_CONFIG_MACROPAD_H
