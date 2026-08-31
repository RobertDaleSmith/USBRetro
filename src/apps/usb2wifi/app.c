// app.c - USB2WiFi App Entry Point
// USB controllers -> PS Remote Play (WiFi) output on Pico W.
// The firmware calls app_init() after core system init.

#include "app.h"
#include "core/router/router.h"
#include "core/services/players/manager.h"
#include "core/services/players/feedback.h"
#include "core/input_interface.h"
#include "core/output_interface.h"
#include "core/services/leds/leds.h"
#include "usb/usbh/usbh.h"
#include "usb/usbd/usbd.h"
#include "net/remoteplay/remoteplay_output.h"
#include "net/remoteplay/wifi_station.h"
#include "net/remoteplay/rp_session.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include <stdio.h>

// ============================================================================
// INPUT / OUTPUT INTERFACES
// ============================================================================

static const InputInterface* input_interfaces[] = {
    &usbh_input_interface,
};

const InputInterface** app_get_input_interfaces(uint8_t* count)
{
    *count = sizeof(input_interfaces) / sizeof(input_interfaces[0]);
    return input_interfaces;
}

// remoteplay first so main.c auto-discovers it as native_output (web config
// provisioning target); usbd second provides the CDC web-config channel.
static const OutputInterface* output_interfaces[] = {
    &remoteplay_output_interface,
    &usbd_output_interface,
};

const OutputInterface** app_get_output_interfaces(uint8_t* count)
{
    *count = sizeof(output_interfaces) / sizeof(output_interfaces[0]);
    return output_interfaces;
}

// ============================================================================
// INIT
// ============================================================================

void app_init(void)
{
    printf("[app:usb2wifi] Initializing usb2wifi v%s\n", JOYPAD_VERSION);

    native_input = &usbh_input_interface;
    native_output = &remoteplay_output_interface;

    router_config_t router_cfg = {
        .mode = ROUTING_MODE_MERGE,
        .merge_mode = MERGE_BLEND,
        .max_players_per_output = {
            [OUTPUT_TARGET_REMOTE_PLAY] = 1,
        },
        .merge_all_inputs = true,
    };
    router_init(&router_cfg);

    // USB controllers -> Remote Play
    router_add_route(INPUT_SOURCE_USB_HOST, OUTPUT_TARGET_REMOTE_PLAY, 0);

    player_config_t player_cfg = {
        .slot_mode = PLAYER_SLOT_FIXED,
        .max_slots = 1,
        .auto_assign_on_press = true,
    };
    players_init_with_config(&player_cfg);

    // Native USB device = CDC only (web config); the real output is over WiFi.
    usbd_set_mode(USB_OUTPUT_MODE_CDC);

    printf("[app:usb2wifi] Init complete — USB host in, Remote Play (WiFi) out, "
           "CDC config. Session engine: %s\n", rp_session_state_str());
}

// ============================================================================
// TASK
// ============================================================================

void app_task(void)
{
    // Onboard CYW43 LED: solid when WiFi connected, blink while connecting.
    static uint32_t last = 0;
    static bool on = false;
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (wifi_station_is_connected()) {
        if (!on) { cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1); on = true; }
    } else if (now - last > 250) {
        last = now; on = !on;
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, on ? 1 : 0);
    }

    // Rumble feedback from the session -> USB controllers.
    output_feedback_t fb;
    if (remoteplay_output_interface.get_feedback &&
        remoteplay_output_interface.get_feedback(&fb)) {
        for (int i = 0; i < playersCount; i++) {
            feedback_set_rumble(i, fb.rumble_left, fb.rumble_right);
        }
    }
}
