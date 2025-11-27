// app.c - SNES2USB App Entry Point
// SNES/NES native controller input to USB HID gamepad output adapter
//
// This app polls native SNES/NES controllers and routes input to USB device output.

#include "app.h"
#include "core/router/router.h"
#include "core/services/players/manager.h"
#include "core/output_interface.h"
#include "usb/usbd/usbd.h"
#include "native/host/snes/snes_host.h"
#include <stdio.h>

// ============================================================================
// APP OUTPUT INTERFACE (wraps USB interface + SNES host polling)
// ============================================================================

// Wrapper task that polls SNES input AND runs USB output task
static void snes2usb_task(void)
{
    // Poll native SNES controller input
    snes_host_task();

    // Run USB device output task (if any)
    usbd_task();
}

// Runtime-assembled interface (copies USB interface, overrides task)
static OutputInterface runtime_interface;

const OutputInterface* app_get_output_interface(void)
{
    // Copy USB device interface as base
    runtime_interface = usbd_output_interface;

    // Override name and task
    runtime_interface.name = "SNES2USB";
    runtime_interface.task = snes2usb_task;

    return &runtime_interface;
}

// ============================================================================
// APP INITIALIZATION
// ============================================================================

void app_init(void)
{
    printf("[app:snes2usb] Initializing SNES2USB v%s\n", APP_VERSION);

    // Initialize SNES host driver (native SNES controller input)
    snes_host_init_pins(SNES_PIN_CLOCK, SNES_PIN_LATCH, SNES_PIN_DATA0,
                        SNES_PIN_DATA1, SNES_PIN_IOBIT);

    // Configure router for SNES → USB routing
    router_config_t router_cfg = {
        .mode = ROUTING_MODE,
        .merge_mode = MERGE_MODE,
        .max_players_per_output = {
            [OUTPUT_TARGET_USB_DEVICE] = USB_OUTPUT_PORTS,
        },
        .merge_all_inputs = false,
        .transform_flags = TRANSFORM_NONE,
        .mouse_drain_rate = 8,
    };
    router_init(&router_cfg);

    // Add route: Native SNES → USB Device
    router_add_route(INPUT_SOURCE_NATIVE_SNES, OUTPUT_TARGET_USB_DEVICE, 0);

    // Configure player management
    player_config_t player_cfg = {
        .slot_mode = PLAYER_SLOT_MODE,
        .max_slots = MAX_PLAYER_SLOTS,
        .auto_assign_on_press = AUTO_ASSIGN_ON_PRESS,
    };
    players_init_with_config(&player_cfg);

    printf("[app:snes2usb] Initialization complete\n");
    printf("[app:snes2usb]   Routing: SNES/NES → USB HID Gamepad\n");
    printf("[app:snes2usb]   SNES pins: CLK=%d LATCH=%d D0=%d D1=%d IO=%d\n",
           SNES_PIN_CLOCK, SNES_PIN_LATCH, SNES_PIN_DATA0,
           SNES_PIN_DATA1, SNES_PIN_IOBIT);
}
