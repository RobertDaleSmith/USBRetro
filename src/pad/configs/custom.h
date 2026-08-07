// custom.h - Generic DIY Controller Pad Configuration
// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Robert Dale Smith
//
// Starting point for a hand-wired controller: buttons soldered straight to
// GPIO, no expander, no specific donor shell.
//
// The pin map below is only a DEFAULT. Every pin in this struct is
// remappable at runtime over the config tool (config.joypad.ai) with
// PAD.CONFIG.SET, and the result is persisted to flash. On boot,
// app_init() calls pad_config_load_runtime() and only falls back to this
// struct when flash holds no valid record — so a user with different
// wiring never has to rebuild the firmware.
//
// Buttons are active-low: wire each GPIO to one side of the switch and the
// other side to GND. The internal pull-up is enabled for you
// (pad_init_button_pin -> platform_gpio_init_input(pin, !active_high)).
//
// NOTE ON ANALOG: the adc_* fields take an ADC *channel* (0-3), not a GPIO
// number. On RP2040 channel 0-3 = GPIO 26-29. Writing `.adc_lx = 26` reads
// as out-of-range and silently returns a centred axis. Sticks are left
// disabled here on purpose: an ADC pin with nothing wired to it floats, and
// a wandering stick looks like broken firmware.

#ifndef PAD_CONFIG_CUSTOM_H
#define PAD_CONFIG_CUSTOM_H

#include "../pad_input.h"

// ============================================================================
// CUSTOM PAD - digital gamepad on contiguous low GPIO
// ============================================================================
// Defaults chosen to be board-agnostic across RP2040 boards: GP0-GP14 only,
// which are broken out on the Waveshare RP2040-Zero castellations and the
// Pico header alike. GP16 is deliberately avoided — it is the onboard
// WS2812 on the RP2040-Zero (WS2812_PIN=16).

static const pad_device_config_t pad_config_custom = {
    .name = "Custom Pad",
    .active_high = false,   // switch to GND, internal pull-up

    // No I2C expanders
    .i2c_sda = PAD_PIN_DISABLED,
    .i2c_scl = PAD_PIN_DISABLED,

    // D-pad
    .dpad_up    = 0,
    .dpad_down  = 1,
    .dpad_left  = 2,
    .dpad_right = 3,

    // Face buttons
    .b1 = 4,    // A / Cross
    .b2 = 5,    // B / Circle
    .b3 = 6,    // X / Square
    .b4 = 7,    // Y / Triangle

    // Shoulders
    .l1 = 8,
    .r1 = 9,
    .l2 = 10,
    .r2 = 11,

    // Meta
    .s1 = 12,   // Select
    .s2 = 13,   // Start

    // Stick clicks
    .l3 = PAD_PIN_DISABLED,
    .r3 = PAD_PIN_DISABLED,

    // Home/Capture
    .a1 = 14,   // Home
    .a2 = PAD_PIN_DISABLED,
    .a3 = PAD_PIN_DISABLED,
    .a4 = PAD_PIN_DISABLED,

    // Extra paddles
    .l4 = PAD_PIN_DISABLED,
    .r4 = PAD_PIN_DISABLED,

    .f1 = PAD_PIN_DISABLED,
    .f2 = PAD_PIN_DISABLED,

    .toggle = {
        { .pin = PAD_PIN_DISABLED, .function = 0, .invert = false },
        { .pin = PAD_PIN_DISABLED, .function = 0, .invert = false },
    },

    // Analog sticks off by default — see NOTE ON ANALOG above.
    // To enable a left stick on GPIO 26/27: .adc_lx = 0, .adc_ly = 1
    .adc_lx = PAD_PIN_DISABLED,
    .adc_ly = PAD_PIN_DISABLED,
    .adc_rx = PAD_PIN_DISABLED,
    .adc_ry = PAD_PIN_DISABLED,
    .adc_lt = PAD_PIN_DISABLED,
    .adc_rt = PAD_PIN_DISABLED,

    .invert_lx = false,
    .invert_ly = false,
    .invert_rx = false,
    .invert_ry = false,
    .deadzone = 10,

    // 0 = use the board default LED pin (WS2812_PIN)
    .led_pin = 0,
    .led_count = 0,

    // No QWIIC/UART link
    .qwiic_tx = PAD_PIN_DISABLED,
    .qwiic_rx = PAD_PIN_DISABLED,
    .qwiic_i2c_inst = PAD_PIN_DISABLED,
    .usb_host_dp = PAD_PIN_DISABLED,
    .joywing = {
        { .i2c_bus = 0, .sda = PAD_PIN_DISABLED, .scl = PAD_PIN_DISABLED, .addr = 0x49 },
        { .i2c_bus = 0, .sda = PAD_PIN_DISABLED, .scl = PAD_PIN_DISABLED, .addr = 0x49 },
    },
};

#endif // PAD_CONFIG_CUSTOM_H
