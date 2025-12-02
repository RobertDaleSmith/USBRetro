// app.c - Universal Controller App
// GPIO buttons → USB HID Gamepad output
//
// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Robert Dale Smith

#include "app.h"
#include "core/router/router.h"
#include "core/input_interface.h"
#include "core/output_interface.h"
#include "gpio/gpio_input.h"
#include "usb/usbd/usbd.h"
#include <stdio.h>

// ============================================================================
// CONTROLLER TYPE CONFIGURATION
// ============================================================================

#if defined(CONTROLLER_TYPE_FISHERPRICE)
    #include "gpio/configs/fisherprice.h"
    #define GPIO_CONFIG gpio_config_fisherprice
    #define CONTROLLER_NAME "Fisher Price"
#elif defined(CONTROLLER_TYPE_FISHERPRICE_ANALOG)
    #include "gpio/configs/fisherprice.h"
    #define GPIO_CONFIG gpio_config_fisherprice_analog
    #define CONTROLLER_NAME "Fisher Price Analog"
#elif defined(CONTROLLER_TYPE_ALPAKKA)
    #include "gpio/configs/alpakka.h"
    #define GPIO_CONFIG gpio_config_alpakka
    #define CONTROLLER_NAME "Alpakka"
#elif defined(CONTROLLER_TYPE_MACROPAD)
    #include "gpio/configs/macropad.h"
    #define GPIO_CONFIG gpio_config_macropad
    #define CONTROLLER_NAME "MacroPad"
#else
    #error "No controller type defined! Define one of: CONTROLLER_TYPE_FISHERPRICE, CONTROLLER_TYPE_ALPAKKA, etc."
#endif

// ============================================================================
// APP INPUT INTERFACES
// ============================================================================

static const InputInterface* input_interfaces[] = {
    &gpio_input_interface,
};

const InputInterface** app_get_input_interfaces(uint8_t* count)
{
    *count = sizeof(input_interfaces) / sizeof(input_interfaces[0]);
    return input_interfaces;
}

// ============================================================================
// APP OUTPUT INTERFACES
// ============================================================================

static const OutputInterface* output_interfaces[] = {
    &usbd_output_interface,
};

const OutputInterface** app_get_output_interfaces(uint8_t* count)
{
    *count = sizeof(output_interfaces) / sizeof(output_interfaces[0]);
    return output_interfaces;
}

// ============================================================================
// APP INITIALIZATION
// ============================================================================

void app_init(void)
{
    printf("[app:controller] Initializing %s Controller v%s\n", CONTROLLER_NAME, APP_VERSION);

    // Register GPIO device configuration BEFORE interface init
    int dev_idx = gpio_input_add_device(&GPIO_CONFIG);

    if (dev_idx < 0) {
        printf("[app:controller] ERROR: Failed to register GPIO device!\n");
        return;
    }

    printf("[app:controller] GPIO config: %s\n", GPIO_CONFIG.name);

    // Configure router for GPIO → USB Device
    router_config_t router_cfg = {
        .mode = ROUTING_MODE_SIMPLE,
        .merge_mode = MERGE_PRIORITY,
        .max_players_per_output = {
            [OUTPUT_TARGET_USB_DEVICE] = 1,
        },
        .merge_all_inputs = false,
        .transform_flags = 0,
        .mouse_drain_rate = 0,
    };
    router_init(&router_cfg);

    // Add route: GPIO → USB Device
    router_add_route(INPUT_SOURCE_GPIO, OUTPUT_TARGET_USB_DEVICE, 0);

    printf("[app:controller] Initialization complete\n");
    printf("[app:controller]   Routing: GPIO → USB Device (HID Gamepad)\n");
}

// ============================================================================
// APP TASK
// ============================================================================

void app_task(void)
{
    // Nothing extra needed - GPIO input and USB output tasks
    // are called by main loop via their interfaces
}
