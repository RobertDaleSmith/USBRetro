// gpio_input.h - GPIO Input Interface
// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Robert Dale Smith
//
// Input interface for controllers built with buttons/sticks wired directly
// to GPIO pins. Enables building custom controllers, arcade sticks, etc.
// Each gpio_device_config_t creates a controller input source.
//
// Supports:
// - Direct GPIO pins (0-29)
// - I2C I/O expanders (pins 100-115 for expander 0, 200-215 for expander 1)
// - ADC for analog sticks (GPIO 26-29 = ADC 0-3)

#ifndef GPIO_INPUT_H
#define GPIO_INPUT_H

#include <stdint.h>
#include <stdbool.h>
#include "core/input_interface.h"

// ============================================================================
// PIN ADDRESSING
// ============================================================================
//
// Pin numbers use virtual addressing:
//   0-29:    Direct GPIO pins
//   100-115: I2C I/O expander 0, pins 0-15
//   200-215: I2C I/O expander 1, pins 0-15
//
// This follows the Alpakka firmware convention.

// Pin value for disabled/unused pins
#define GPIO_PIN_DISABLED (-1)

// I2C expander virtual pin bases
#define GPIO_I2C_EXPANDER_0_BASE 100
#define GPIO_I2C_EXPANDER_1_BASE 200

// I2C expander I2C addresses (PCA9555/TCA9555 compatible)
#define GPIO_I2C_EXPANDER_ADDR_0 0x20
#define GPIO_I2C_EXPANDER_ADDR_1 0x21

// Maximum GPIO configs (each becomes a controller input)
#define GPIO_MAX_DEVICES 4

// GPIO device configuration - defines a controller's pin mapping
// Pin values: 0-29 = direct GPIO, 100-115 = I2C expander 0, 200-215 = I2C expander 1
typedef struct {
    const char* name;           // Config name (e.g., "Fisher Price", "Alpakka")
    bool active_high;           // true = pressed when high, false = pressed when low

    // I2C configuration (for I/O expanders)
    int8_t i2c_sda;             // I2C SDA pin (GPIO_PIN_DISABLED = no I2C)
    int8_t i2c_scl;             // I2C SCL pin

    // Digital button pins (GPIO_PIN_DISABLED = not used)
    // Use 0-29 for direct GPIO, 100-115/200-215 for I2C expanders
    int16_t dpad_up;
    int16_t dpad_down;
    int16_t dpad_left;
    int16_t dpad_right;

    int16_t b1;                 // A / Cross
    int16_t b2;                 // B / Circle
    int16_t b3;                 // X / Square
    int16_t b4;                 // Y / Triangle

    int16_t l1;                 // LB / L1
    int16_t r1;                 // RB / R1
    int16_t l2;                 // LT / L2 (digital)
    int16_t r2;                 // RT / R2 (digital)

    int16_t s1;                 // Select / Back
    int16_t s2;                 // Start
    int16_t l3;                 // Left stick click
    int16_t r3;                 // Right stick click
    int16_t a1;                 // Home / Guide
    int16_t a2;                 // Capture / Touchpad

    // Extra buttons (for controllers with more than standard layout)
    int16_t l4;                 // Extra left trigger/paddle
    int16_t r4;                 // Extra right trigger/paddle

    // Analog stick ADC channels (0-3 for GPIO 26-29, GPIO_PIN_DISABLED = not used)
    // Note: RP2040 has 4 ADC channels on GPIO 26, 27, 28, 29
    int8_t adc_lx;              // Left stick X (ADC channel 0-3)
    int8_t adc_ly;              // Left stick Y (ADC channel 0-3)
    int8_t adc_rx;              // Right stick X (ADC channel 0-3)
    int8_t adc_ry;              // Right stick Y (ADC channel 0-3)

    bool invert_lx;             // Invert left X axis
    bool invert_ly;             // Invert left Y axis
    bool invert_rx;             // Invert right X axis
    bool invert_ry;             // Invert right Y axis

    // Analog stick deadzone (0-127, applied to center)
    uint8_t deadzone;

    // NeoPixel LED pin (GPIO_PIN_DISABLED = not used)
    int8_t led_pin;
    uint8_t led_count;          // Number of LEDs
} gpio_device_config_t;

// ============================================================================
// GPIO INPUT API
// ============================================================================

// Initialize GPIO input with a device configuration
// Can be called multiple times to add multiple GPIO controllers
// Returns device index (0-3) or -1 on failure
int gpio_input_add_device(const gpio_device_config_t* config);

// Remove all GPIO devices
void gpio_input_clear_devices(void);

// Get number of registered GPIO devices
uint8_t gpio_input_get_device_count(void);

// GPIO input interface (implements InputInterface pattern)
extern const InputInterface gpio_input_interface;

// ============================================================================
// HELPER MACROS FOR CONFIG DEFINITIONS
// ============================================================================

// Initialize all pins to disabled
#define GPIO_CONFIG_INIT(config_name) { \
    .name = config_name, \
    .active_high = false, \
    .i2c_sda = GPIO_PIN_DISABLED, \
    .i2c_scl = GPIO_PIN_DISABLED, \
    .dpad_up = GPIO_PIN_DISABLED, \
    .dpad_down = GPIO_PIN_DISABLED, \
    .dpad_left = GPIO_PIN_DISABLED, \
    .dpad_right = GPIO_PIN_DISABLED, \
    .b1 = GPIO_PIN_DISABLED, \
    .b2 = GPIO_PIN_DISABLED, \
    .b3 = GPIO_PIN_DISABLED, \
    .b4 = GPIO_PIN_DISABLED, \
    .l1 = GPIO_PIN_DISABLED, \
    .r1 = GPIO_PIN_DISABLED, \
    .l2 = GPIO_PIN_DISABLED, \
    .r2 = GPIO_PIN_DISABLED, \
    .s1 = GPIO_PIN_DISABLED, \
    .s2 = GPIO_PIN_DISABLED, \
    .l3 = GPIO_PIN_DISABLED, \
    .r3 = GPIO_PIN_DISABLED, \
    .a1 = GPIO_PIN_DISABLED, \
    .a2 = GPIO_PIN_DISABLED, \
    .l4 = GPIO_PIN_DISABLED, \
    .r4 = GPIO_PIN_DISABLED, \
    .adc_lx = GPIO_PIN_DISABLED, \
    .adc_ly = GPIO_PIN_DISABLED, \
    .adc_rx = GPIO_PIN_DISABLED, \
    .adc_ry = GPIO_PIN_DISABLED, \
    .invert_lx = false, \
    .invert_ly = false, \
    .invert_rx = false, \
    .invert_ry = false, \
    .deadzone = 10, \
    .led_pin = GPIO_PIN_DISABLED, \
    .led_count = 0, \
}

#endif // GPIO_INPUT_H
