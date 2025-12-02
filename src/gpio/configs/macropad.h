// macropad.h - Adafruit MacroPad RP2040 GPIO Configuration
// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Robert Dale Smith
//
// GPIO mappings for Adafruit MacroPad RP2040.
// 12 mechanical keys in 4x3 grid + rotary encoder with button.
//
// Physical layout:
//   [1] [2] [3]
//   [4] [5] [6]
//   [7] [8] [9]
//   [10][11][12]
//   + Rotary encoder (button on GPIO 0)

#ifndef GPIO_CONFIG_MACROPAD_H
#define GPIO_CONFIG_MACROPAD_H

#include "../gpio_input.h"

// ============================================================================
// MACROPAD - Arcade/Fightstick Layout
// ============================================================================
// Maps 12 keys to standard gamepad buttons for arcade/fightstick use.
// Rotary encoder button = Home.
//
// Default mapping (arcade 8-button + extras):
//   [S1] [S2] [A1]     <- Select, Start, Home
//   [B3] [B4] [R1]     <- X, Y, RB
//   [B1] [B2] [R2]     <- A, B, RT
//   [L1] [L2] [A2]     <- LB, LT, Capture

static const gpio_device_config_t gpio_config_macropad = {
    .name = "MacroPad",
    .active_high = false,   // Keys have pull-ups (pressed = low)

    // No I2C expanders
    .i2c_sda = GPIO_PIN_DISABLED,
    .i2c_scl = GPIO_PIN_DISABLED,

    // No D-pad (use keys for buttons instead)
    .dpad_up    = GPIO_PIN_DISABLED,
    .dpad_down  = GPIO_PIN_DISABLED,
    .dpad_left  = GPIO_PIN_DISABLED,
    .dpad_right = GPIO_PIN_DISABLED,

    // Face buttons (row 2-3: keys 4-5, 7-8)
    .b1 = 7,                // A / Cross (Key 7)
    .b2 = 8,                // B / Circle (Key 8)
    .b3 = 4,                // X / Square (Key 4)
    .b4 = 5,                // Y / Triangle (Key 5)

    // Shoulder buttons
    .l1 = 10,               // LB / L1 (Key 10)
    .r1 = 6,                // RB / R1 (Key 6)
    .l2 = 11,               // LT / L2 (Key 11)
    .r2 = 9,                // RT / R2 (Key 9)

    // Meta buttons (row 1: keys 1-2)
    .s1 = 1,                // Select / Back (Key 1)
    .s2 = 2,                // Start (Key 2)

    // No stick clicks
    .l3 = GPIO_PIN_DISABLED,
    .r3 = GPIO_PIN_DISABLED,

    // Home on encoder button, Capture on Key 12
    .a1 = 0,                // Home / Guide (Encoder button)
    .a2 = 12,               // Capture (Key 12)

    // Extra buttons (Key 3 unused in this layout)
    .l4 = GPIO_PIN_DISABLED,
    .r4 = 3,                // Extra right (Key 3)

    // No analog sticks
    .adc_lx = GPIO_PIN_DISABLED,
    .adc_ly = GPIO_PIN_DISABLED,
    .adc_rx = GPIO_PIN_DISABLED,
    .adc_ry = GPIO_PIN_DISABLED,

    .invert_lx = false,
    .invert_ly = false,
    .invert_rx = false,
    .invert_ry = false,
    .deadzone = 10,

    // NeoPixel on GPIO 19 (12 LEDs, one per key)
    .led_pin = 19,
    .led_count = 12,
};

#endif // GPIO_CONFIG_MACROPAD_H
