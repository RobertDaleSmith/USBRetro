// app.c - USB2USB App Entry Point
// USB to USB HID gamepad adapter
//
// This file contains app-specific initialization and logic.
// The firmware calls app_init() after core system initialization.

#include "app.h"
#include "core/router/router.h"
#include "core/services/players/manager.h"
#include "core/output_interface.h"
#include "usb/usbd/usbd.h"
#include <stdio.h>

// ============================================================================
// APP OUTPUT INTERFACE
// ============================================================================

const OutputInterface* app_get_output_interface(void)
{
    return &usbd_output_interface;
}

// ============================================================================
// APP INITIALIZATION
// ============================================================================

void app_init(void)
{
    printf("[app:usb2usb] Initializing USB2USB v%s\n", APP_VERSION);

    // Configure router for USB2USB
    router_config_t router_cfg = {
        .mode = ROUTING_MODE,
        .merge_mode = MERGE_MODE,
        .max_players_per_output = {
            [OUTPUT_TARGET_USB_DEVICE] = USB_OUTPUT_PORTS,
        },
        .merge_all_inputs = true,  // Merge all USB inputs to single output
        .transform_flags = TRANSFORM_FLAGS,
        .mouse_drain_rate = 8,
    };
    router_init(&router_cfg);

    // Add default route: USB Host → USB Device
    router_add_route(INPUT_SOURCE_USB_HOST, OUTPUT_TARGET_USB_DEVICE, 0);

    // Configure player management
    player_config_t player_cfg = {
        .slot_mode = PLAYER_SLOT_MODE,
        .max_slots = MAX_PLAYER_SLOTS,
        .auto_assign_on_press = AUTO_ASSIGN_ON_PRESS,
    };
    players_init_with_config(&player_cfg);

    printf("[app:usb2usb] Initialization complete\n");
    printf("[app:usb2usb]   Routing: USB Host → USB Device (HID Gamepad)\n");
    printf("[app:usb2usb]   Player slots: %d\n", MAX_PLAYER_SLOTS);
}

// ============================================================================
// APP TASK (Optional - called from main loop)
// ============================================================================

void app_task(void)
{
    // App-specific periodic tasks go here
}
