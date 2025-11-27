// app.c - GCUSB App Entry Point
// USB to GameCube adapter
//
// This file contains app-specific initialization and logic.
// The firmware calls app_init() after core system initialization.

#include "app.h"
#include "profiles.h"
#include "core/router/router.h"
#include "core/services/players/manager.h"
#include "core/services/profiles/profile.h"
#include "core/input_interface.h"
#include "core/output_interface.h"
#include "native/device/gamecube/gamecube_device.h"
#include "usb/usbh/usbh.h"
#include <stdio.h>

// ============================================================================
// APP PROFILE CONFIGURATION
// ============================================================================

static const profile_config_t app_profile_config = {
    .output_profiles = {
        [OUTPUT_TARGET_GAMECUBE] = &gc_profile_set,
    },
    .shared_profiles = NULL,
};

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

extern const OutputInterface gamecube_output_interface;

static const OutputInterface* output_interfaces[] = {
    &gamecube_output_interface,
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
    printf("[app:usb2gc] Initializing GCUSB v%s\n", APP_VERSION);

    // Configure router for GCUSB
    router_config_t router_cfg = {
        .mode = ROUTING_MODE,
        .merge_mode = MERGE_MODE,
        .max_players_per_output = {
            [OUTPUT_TARGET_GAMECUBE] = GAMECUBE_OUTPUT_PORTS,
        },
        .merge_all_inputs = true,  // Merge all USB inputs to single port
        .transform_flags = TRANSFORM_FLAGS,
        .mouse_drain_rate = 8,
    };
    router_init(&router_cfg);

    // Add default route: USB → GameCube
    router_add_route(INPUT_SOURCE_USB_HOST, OUTPUT_TARGET_GAMECUBE, 0);

    // Configure player management
    player_config_t player_cfg = {
        .slot_mode = PLAYER_SLOT_MODE,
        .max_slots = MAX_PLAYER_SLOTS,
        .auto_assign_on_press = AUTO_ASSIGN_ON_PRESS,
    };
    players_init_with_config(&player_cfg);

    // Initialize profile system with app-defined profiles
    profile_init(&app_profile_config);

    uint8_t profile_count = profile_get_count(OUTPUT_TARGET_GAMECUBE);
    const char* active_name = profile_get_name(OUTPUT_TARGET_GAMECUBE,
                                                profile_get_active_index(OUTPUT_TARGET_GAMECUBE));

    printf("[app:usb2gc] Initialization complete\n");
    printf("[app:usb2gc]   Routing: %s\n", "MERGE_BLEND (blend all USB → single GC port)");
    printf("[app:usb2gc]   Player slots: %d (FIXED mode for future 4-port)\n", MAX_PLAYER_SLOTS);
    printf("[app:usb2gc]   Profiles: %d (active: %s)\n", profile_count, active_name ? active_name : "none");
}

// ============================================================================
// APP TASK (Optional - called from main loop)
// ============================================================================

void app_task(void)
{
    // App-specific periodic tasks go here
    // For GCUSB, most logic is in gamecube_device.c
}
