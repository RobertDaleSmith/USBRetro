// app.c - USB2PCE App Entry Point
// USB to PCEngine/TurboGrafx-16 adapter
//
// Mode detection (at boot, before the PCE PIO claims the pins):
//   CLKIN toggling (a powered console polls the pad every frame) → Play mode
//                                                     (USB host → PCEngine output)
//   CLKIN silent (no console: our pull-down holds it LOW)        → Config mode
//                                                     (USB device with CDC for web config)

#include "app.h"
#include "core/router/router.h"
#include "core/services/players/manager.h"
#include "core/services/profiles/profile.h"
#include "core/services/leds/leds.h"
#include "core/input_interface.h"
#include "core/output_interface.h"
#include "native/device/pcengine/pcengine_device.h"
#include "profiles.h"
#include "usb/usbh/usbh.h"
#include "usb/usbd/usbd.h"
#include "pico/stdlib.h"
#include <stdio.h>

// True when no PCEngine was detected at boot → USB-device/CDC config mode.
static bool pce_config_mode = false;

// ============================================================================
// APP INPUT INTERFACES
// ============================================================================

static const InputInterface* input_interfaces[] = {
    &usbh_input_interface,
};

const InputInterface** app_get_input_interfaces(uint8_t* count)
{
    if (pce_config_mode) {
        // Config mode: the USB port is a device (CDC), not a controller host.
        *count = 0;
        return NULL;
    }
    *count = sizeof(input_interfaces) / sizeof(input_interfaces[0]);
    return input_interfaces;
}

// ============================================================================
// APP OUTPUT INTERFACES
// ============================================================================

extern const OutputInterface pcengine_output_interface;

static const OutputInterface* pce_output_interfaces[] = {
    &pcengine_output_interface,
};

static const OutputInterface* cdc_output_interfaces[] = {
    &usbd_output_interface,
};

const OutputInterface** app_get_output_interfaces(uint8_t* count)
{
    // Detect a PCEngine by CLOCK ACTIVITY on CLKIN. A powered console polls the
    // controller every frame (~60 Hz), so it drives CLKIN; a PC provides no
    // clock. We pull the line down (steady LOW with no console) and watch for
    // it to take BOTH levels over a short window — that only happens when a
    // console is toggling it. Runs BEFORE the PCE PIO reconfigures the pin.
    gpio_init(CLKIN_PIN);
    gpio_set_dir(CLKIN_PIN, GPIO_IN);
    gpio_pull_down(CLKIN_PIN);
    sleep_ms(2);  // let the pull settle

    bool saw_hi = false, saw_lo = false;
    uint32_t start = to_ms_since_boot(get_absolute_time());
    while (to_ms_since_boot(get_absolute_time()) - start < 80) {  // ~5 PCE frames
        if (gpio_get(CLKIN_PIN)) saw_hi = true; else saw_lo = true;
        if (saw_hi && saw_lo) break;  // line is toggling → console present
    }

    if (!(saw_hi && saw_lo)) {
        // No clock → no console → config mode (USB device with CDC).
        pce_config_mode = true;
        *count = sizeof(cdc_output_interfaces) / sizeof(cdc_output_interfaces[0]);
        return cdc_output_interfaces;
    }

    // PCEngine detected → play mode (USB host → PCE output).
    pce_config_mode = false;
    *count = sizeof(pce_output_interfaces) / sizeof(pce_output_interfaces[0]);
    return pce_output_interfaces;
}

// ============================================================================
// APP INITIALIZATION
// ============================================================================

// Shared so both play mode and CDC config mode expose the PCE profiles.
static const profile_config_t app_profile_config = {
    .output_profiles = { [OUTPUT_TARGET_PCENGINE] = &usb2pce_profile_set },
    .shared_profiles = &usb2pce_profile_set,
};

void app_init(void)
{
    // Expose the PCEngine output to the web config in BOTH modes.
    native_output = &pcengine_output_interface;

    if (pce_config_mode) {
        printf("[app:usb2pce] Config mode - CDC serial for web configuration\n");
        leds_set_color(64, 32, 0);  // orange = config mode

        router_config_t router_cfg = {
            .mode = ROUTING_MODE_MERGE,
            .merge_mode = MERGE_BLEND,
            .max_players_per_output = {
                [OUTPUT_TARGET_USB_DEVICE] = 1,
            },
            .merge_all_inputs = true,
        };
        router_init(&router_cfg);
        profile_init(&app_profile_config);
        return;
    }

    printf("[app:usb2pce] Initializing USB2PCE v%s\n", JOYPAD_VERSION);

    // Configure router for USB2PCE
    router_config_t router_cfg = {
        .mode = ROUTING_MODE,
        .merge_mode = MERGE_MODE,
        .max_players_per_output = {
            [OUTPUT_TARGET_PCENGINE] = PCENGINE_OUTPUT_PORTS,  // 5 players via multitap
        },
        .merge_all_inputs = false,  // Simple 1:1 mapping (each USB device → multitap port)
        .transform_flags = TRANSFORM_FLAGS,
        .mouse_drain_rate = 8,
    };
    router_init(&router_cfg);

    // Add default route: USB → PCEngine
    router_add_route(INPUT_SOURCE_USB_HOST, OUTPUT_TARGET_PCENGINE, 0);

    // Built-in profiles select the PCEngine button mode (2/6/3-button) and own
    // the per-mode turbo. Switch with the universal profile hotkey or web config.
    profile_init(&app_profile_config);

    // Configure player management
    player_config_t player_cfg = {
        .slot_mode = PLAYER_SLOT_MODE,
        .max_slots = MAX_PLAYER_SLOTS,
        .auto_assign_on_press = AUTO_ASSIGN_ON_PRESS,
    };
    players_init_with_config(&player_cfg);

    printf("[app:usb2pce] Initialization complete\n");
    printf("[app:usb2pce]   Routing: %s\n", "SIMPLE (USB → PCE multitap 1:1)");
    printf("[app:usb2pce]   Player slots: %d (SHIFT mode - players shift on disconnect)\n", MAX_PLAYER_SLOTS);
    printf("[app:usb2pce]   Mouse support: enabled (Populous)\n");
}

// ============================================================================
// APP TASK (called in main loop)
// ============================================================================

void app_task(void)
{
    // No app-specific task work needed for usb2pce
}
