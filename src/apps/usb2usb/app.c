// app.c - USB2USB App Entry Point
// USB to USB HID gamepad adapter
//
// This file contains app-specific initialization and logic.
// The firmware calls app_init() after core system initialization.

#include "app.h"
#include "core/router/router.h"
#include "core/services/players/manager.h"
#include "core/services/players/feedback.h"
#include "core/services/button/button.h"
#include "core/input_interface.h"
#include "core/output_interface.h"
#include "usb/usbh/usbh.h"
#include "usb/usbd/usbd.h"
#include "tusb.h"
#include "pico/stdlib.h"
#include <stdio.h>

// ============================================================================
// BUTTON EVENT HANDLER
// ============================================================================

static void on_button_event(button_event_t event)
{
    switch (event) {
        case BUTTON_EVENT_CLICK:
            printf("[app:usb2usb] Button click - current mode: %s\n",
                   usbd_get_mode_name(usbd_get_mode()));
            break;

        case BUTTON_EVENT_DOUBLE_CLICK:
            printf("[app:usb2usb] Button double-click\n");
            break;

        case BUTTON_EVENT_HOLD:
            printf("[app:usb2usb] Button hold - switching USB output mode...\n");
            // Flush CDC and give USB stack time to transmit
            tud_task();
            sleep_ms(50);
            tud_task();

            // Cycle to next mode
            usb_output_mode_t current = usbd_get_mode();
            usb_output_mode_t next;
            if (current == USB_OUTPUT_MODE_HID) {
                next = USB_OUTPUT_MODE_XBOX_ORIGINAL;
            } else {
                next = USB_OUTPUT_MODE_HID;
            }
            printf("[app:usb2usb] Switching from %s to %s\n",
                   usbd_get_mode_name(current), usbd_get_mode_name(next));
            tud_task();
            sleep_ms(50);
            tud_task();

            usbd_set_mode(next);  // This will reset the device
            break;

        case BUTTON_EVENT_RELEASE:
            printf("[app:usb2usb] Button released after hold\n");
            break;

        default:
            break;
    }
}

// ============================================================================
// APP INPUT INTERFACES
// ============================================================================

static const InputInterface* input_interfaces[] = {
    &usbh_input_interface,
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
    printf("[app:usb2usb] Initializing USB2USB v%s\n", APP_VERSION);

    // Initialize button service
    button_init();
    button_set_callback(on_button_event);

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
    printf("[app:usb2usb]   Hold button (GPIO7) for 1.5s to switch USB mode\n");
}

// ============================================================================
// APP TASK (Optional - called from main loop)
// ============================================================================

void app_task(void)
{
    // Process button input
    button_task();

    // Route rumble from USB device output to USB host input controllers
    // The output interface receives rumble from the console (e.g., Xbox OG)
    // and we forward it to connected controllers via the feedback system
    if (usbd_output_interface.get_rumble) {
        uint8_t rumble = usbd_output_interface.get_rumble();
        // Set rumble for all active players (including 0 to stop rumble)
        for (int i = 0; i < playersCount; i++) {
            feedback_set_rumble(i, rumble, rumble);
        }
    }
}
