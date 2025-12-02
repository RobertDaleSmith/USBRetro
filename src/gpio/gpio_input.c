// gpio_input.c - GPIO Input Interface Implementation
// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Robert Dale Smith
//
// Reads buttons and analog sticks wired directly to GPIO pins.
// Each registered config creates a controller input source.

#include "gpio_input.h"
#include "core/buttons.h"
#include "core/input_event.h"
#include "core/router/router.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include <stdio.h>
#include <string.h>

// ============================================================================
// INTERNAL STATE
// ============================================================================

// Registered device configurations
static const gpio_device_config_t* gpio_devices[GPIO_MAX_DEVICES];
static uint8_t gpio_device_count = 0;

// Current input state per device
static input_event_t gpio_events[GPIO_MAX_DEVICES];

// Debounce state (simple: require 2 consecutive reads)
static uint32_t gpio_prev_buttons[GPIO_MAX_DEVICES];

// ADC initialized flag
static bool adc_initialized = false;

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

// Initialize a single GPIO pin as input with appropriate pull
static void gpio_init_button_pin(int8_t pin, bool active_high) {
    if (pin < 0 || pin > 29) return;

    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);

    // Pull opposite to active state
    if (active_high) {
        gpio_pull_down(pin);  // Active high: pull down, button connects to VCC
    } else {
        gpio_pull_up(pin);    // Active low: pull up, button connects to GND
    }
}

// Read a button pin and return true if pressed
static bool gpio_read_button(int8_t pin, bool active_high) {
    if (pin < 0) return false;

    bool state = gpio_get(pin);
    return active_high ? state : !state;
}

// Read ADC channel and return 0-255 value
static uint8_t gpio_read_adc(int8_t channel, bool invert) {
    if (channel < 0 || channel > 3) return 128;  // Centered

    adc_select_input(channel);
    uint16_t raw = adc_read();  // 12-bit: 0-4095

    // Convert to 8-bit
    uint8_t value = raw >> 4;  // 0-255

    if (invert) {
        value = 255 - value;
    }

    return value;
}

// Apply deadzone to analog value (centered at 128)
static uint8_t apply_deadzone(uint8_t value, uint8_t deadzone) {
    int16_t centered = (int16_t)value - 128;

    if (centered > -deadzone && centered < deadzone) {
        return 128;  // In deadzone, return center
    }

    return value;
}

// Initialize GPIO pins for a device config
static void gpio_init_device_pins(const gpio_device_config_t* config) {
    if (!config) return;

    bool ah = config->active_high;

    // Initialize button pins
    gpio_init_button_pin(config->dpad_up, ah);
    gpio_init_button_pin(config->dpad_down, ah);
    gpio_init_button_pin(config->dpad_left, ah);
    gpio_init_button_pin(config->dpad_right, ah);

    gpio_init_button_pin(config->b1, ah);
    gpio_init_button_pin(config->b2, ah);
    gpio_init_button_pin(config->b3, ah);
    gpio_init_button_pin(config->b4, ah);

    gpio_init_button_pin(config->l1, ah);
    gpio_init_button_pin(config->r1, ah);
    gpio_init_button_pin(config->l2, ah);
    gpio_init_button_pin(config->r2, ah);

    gpio_init_button_pin(config->s1, ah);
    gpio_init_button_pin(config->s2, ah);
    gpio_init_button_pin(config->l3, ah);
    gpio_init_button_pin(config->r3, ah);
    gpio_init_button_pin(config->a1, ah);
    gpio_init_button_pin(config->a2, ah);

    // Initialize ADC if any analog inputs are used
    bool has_analog = (config->adc_lx >= 0 || config->adc_ly >= 0 ||
                       config->adc_rx >= 0 || config->adc_ry >= 0);

    if (has_analog && !adc_initialized) {
        adc_init();
        adc_initialized = true;
    }

    // Initialize ADC pins (GPIO 26-29 are ADC0-3)
    if (config->adc_lx >= 0 && config->adc_lx <= 3) {
        adc_gpio_init(26 + config->adc_lx);
    }
    if (config->adc_ly >= 0 && config->adc_ly <= 3) {
        adc_gpio_init(26 + config->adc_ly);
    }
    if (config->adc_rx >= 0 && config->adc_rx <= 3) {
        adc_gpio_init(26 + config->adc_rx);
    }
    if (config->adc_ry >= 0 && config->adc_ry <= 3) {
        adc_gpio_init(26 + config->adc_ry);
    }

    printf("[gpio] Initialized device: %s (active_%s)\n",
           config->name, ah ? "high" : "low");
}

// Poll a single device and update its input event
static void gpio_poll_device(uint8_t device_index) {
    if (device_index >= gpio_device_count) return;

    const gpio_device_config_t* config = gpio_devices[device_index];
    input_event_t* event = &gpio_events[device_index];
    bool ah = config->active_high;

    // Read buttons into bitmap
    uint32_t buttons = 0;

    // D-pad
    if (gpio_read_button(config->dpad_up, ah))    buttons |= USBR_BUTTON_DU;
    if (gpio_read_button(config->dpad_down, ah))  buttons |= USBR_BUTTON_DD;
    if (gpio_read_button(config->dpad_left, ah))  buttons |= USBR_BUTTON_DL;
    if (gpio_read_button(config->dpad_right, ah)) buttons |= USBR_BUTTON_DR;

    // Face buttons
    if (gpio_read_button(config->b1, ah)) buttons |= USBR_BUTTON_B1;
    if (gpio_read_button(config->b2, ah)) buttons |= USBR_BUTTON_B2;
    if (gpio_read_button(config->b3, ah)) buttons |= USBR_BUTTON_B3;
    if (gpio_read_button(config->b4, ah)) buttons |= USBR_BUTTON_B4;

    // Shoulders/triggers
    if (gpio_read_button(config->l1, ah)) buttons |= USBR_BUTTON_L1;
    if (gpio_read_button(config->r1, ah)) buttons |= USBR_BUTTON_R1;
    if (gpio_read_button(config->l2, ah)) buttons |= USBR_BUTTON_L2;
    if (gpio_read_button(config->r2, ah)) buttons |= USBR_BUTTON_R2;

    // Meta buttons
    if (gpio_read_button(config->s1, ah)) buttons |= USBR_BUTTON_S1;
    if (gpio_read_button(config->s2, ah)) buttons |= USBR_BUTTON_S2;
    if (gpio_read_button(config->l3, ah)) buttons |= USBR_BUTTON_L3;
    if (gpio_read_button(config->r3, ah)) buttons |= USBR_BUTTON_R3;
    if (gpio_read_button(config->a1, ah)) buttons |= USBR_BUTTON_A1;
    if (gpio_read_button(config->a2, ah)) buttons |= USBR_BUTTON_A2;

    // Simple debounce: only update if same as previous read
    // (This filters out single-sample glitches)
    if (buttons == gpio_prev_buttons[device_index]) {
        event->buttons = buttons;
    }
    gpio_prev_buttons[device_index] = buttons;

    // Read analog sticks
    uint8_t dz = config->deadzone;

    if (config->adc_lx >= 0) {
        event->analog[ANALOG_X] = apply_deadzone(
            gpio_read_adc(config->adc_lx, config->invert_lx), dz);
    }
    if (config->adc_ly >= 0) {
        event->analog[ANALOG_Y] = apply_deadzone(
            gpio_read_adc(config->adc_ly, config->invert_ly), dz);
    }
    if (config->adc_rx >= 0) {
        event->analog[ANALOG_Z] = apply_deadzone(
            gpio_read_adc(config->adc_rx, config->invert_rx), dz);
    }
    if (config->adc_ry >= 0) {
        event->analog[ANALOG_RX] = apply_deadzone(
            gpio_read_adc(config->adc_ry, config->invert_ry), dz);
    }
}

// ============================================================================
// PUBLIC API
// ============================================================================

int gpio_input_add_device(const gpio_device_config_t* config) {
    if (!config || gpio_device_count >= GPIO_MAX_DEVICES) {
        return -1;
    }

    uint8_t index = gpio_device_count;
    gpio_devices[index] = config;

    // Initialize input event for this device
    init_input_event(&gpio_events[index]);
    gpio_events[index].dev_addr = 0xF0 + index;  // Virtual address for GPIO devices
    gpio_events[index].instance = index;
    gpio_events[index].type = INPUT_TYPE_GAMEPAD;

    gpio_prev_buttons[index] = 0;

    gpio_device_count++;

    return index;
}

void gpio_input_clear_devices(void) {
    gpio_device_count = 0;
    memset(gpio_devices, 0, sizeof(gpio_devices));
}

uint8_t gpio_input_get_device_count(void) {
    return gpio_device_count;
}

// ============================================================================
// INPUT INTERFACE IMPLEMENTATION
// ============================================================================

static void gpio_input_init(void) {
    printf("[gpio] Initializing GPIO input interface\n");

    // Initialize pins for all registered devices
    for (uint8_t i = 0; i < gpio_device_count; i++) {
        gpio_init_device_pins(gpio_devices[i]);
    }

    printf("[gpio] Initialized %d GPIO device(s)\n", gpio_device_count);
}

static void gpio_input_task(void) {
    // Poll all registered devices
    for (uint8_t i = 0; i < gpio_device_count; i++) {
        gpio_poll_device(i);

        // Submit to router
        router_submit_input(&gpio_events[i]);
    }
}

static bool gpio_input_is_connected(void) {
    // GPIO devices are always "connected" if configured
    return gpio_device_count > 0;
}

static uint8_t gpio_input_device_count(void) {
    return gpio_device_count;
}

// Export interface
const InputInterface gpio_input_interface = {
    .name = "GPIO",
    .source = INPUT_SOURCE_GPIO,
    .init = gpio_input_init,
    .task = gpio_input_task,
    .is_connected = gpio_input_is_connected,
    .get_device_count = gpio_input_device_count,
};
