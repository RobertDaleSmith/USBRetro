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
#include "platform/platform.h"
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
    // Detect a PCEngine by CLOCK ACTIVITY on its control lines. Runs BEFORE the
    // PCE PIO claims the pins.
    //
    // The adapter board biases SEL/CLR to a fixed idle level (a static "is the
    // pin driven" test can't tell a console from a bare PC — both read that idle
    // level), so we key off the one thing only a live, scanning console does:
    // TOGGLE the lines. A running console pulses SEL and CLR every frame; on a
    // PC they sit flat at the board's idle level.
    //
    // Bias toward PLAY mode — a false config-mode on a real console breaks the
    // controller (very visible), while a false play-mode on a PC just withholds
    // CDC (the user replugs). So we watch BOTH control lines over a wide window
    // (more frames, two signals → far higher catch rate) and only fall to config
    // mode when we see no activity on either line at all.
    gpio_init(CLKIN_PIN);  gpio_set_dir(CLKIN_PIN, GPIO_IN);
    gpio_init(DATAIN_PIN); gpio_set_dir(DATAIN_PIN, GPIO_IN);
    sleep_ms(5);  // let inputs settle (board bias holds the idle level)

    int edges = 0;
    bool prev_clk = gpio_get(CLKIN_PIN);
    bool prev_sel = gpio_get(DATAIN_PIN);
    uint32_t start = to_ms_since_boot(get_absolute_time());
    while (to_ms_since_boot(get_absolute_time()) - start < 600) {  // ~36 PCE frames
        bool clk = gpio_get(CLKIN_PIN);
        bool sel = gpio_get(DATAIN_PIN);
        if (clk != prev_clk) edges++;   // any transition on either control line
        if (sel != prev_sel) edges++;
        prev_clk = clk;
        prev_sel = sel;
        if (edges >= 4) break;  // repeated toggling → a live console is scanning
    }

    bool console = (edges >= 4);

    if (!console) {
        // No console → config mode (USB device with CDC).
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

// PCEngine device/output modes (index = custom_profile_t.output_mode, and the
// built-in profiles' output_mode). Kept in sync with pcengine_device.h BUTTON_MODE_*.
static const char* const usb2pce_output_modes[] = {
    "2-Button", "6-Button", "3-Button (Sel)", "3-Button (Run)",
};

// Shared so both play mode and CDC config mode expose the PCE profiles + modes.
static const profile_config_t app_profile_config = {
    .output_profiles = { [OUTPUT_TARGET_PCENGINE] = &usb2pce_profile_set },
    .shared_profiles = &usb2pce_profile_set,
    .output_type_name = "PCEngine",
    .output_mode_names = usb2pce_output_modes,
    .output_mode_count = sizeof(usb2pce_output_modes) / sizeof(usb2pce_output_modes[0]),
};

void app_init(void)
{
    // Expose the app's true I/O (USB Host → PCEngine) to the web config in BOTH
    // modes, so the info page reflects the firmware's purpose even while config
    // mode has it enumerated as a USB CDC device.
    native_output = &pcengine_output_interface;
    native_input = &usbh_input_interface;

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
    if (!pce_config_mode) return;  // play mode: nothing to do here

    // Cold-boot recovery. If the console was powered on WITH the adapter already
    // attached, it wasn't scanning controllers during boot-time detection, so we
    // came up in config mode. Keep watching SEL/CLR: the instant the console
    // finishes booting and starts scanning (toggling the lines), reboot so the
    // boot-time detector re-runs and lands in PLAY mode. A PC never toggles the
    // lines (they sit at the board's idle level), so it stays in config mode.
    static bool watch_init = false;
    static bool prev_clk, prev_sel;
    static int edges = 0;
    static uint32_t win_start = 0;

    if (!watch_init) {
        gpio_init(CLKIN_PIN);  gpio_set_dir(CLKIN_PIN, GPIO_IN);
        gpio_init(DATAIN_PIN); gpio_set_dir(DATAIN_PIN, GPIO_IN);
        prev_clk = gpio_get(CLKIN_PIN);
        prev_sel = gpio_get(DATAIN_PIN);
        win_start = to_ms_since_boot(get_absolute_time());
        watch_init = true;
    }

    bool clk = gpio_get(CLKIN_PIN);
    bool sel = gpio_get(DATAIN_PIN);
    if (clk != prev_clk) edges++;
    if (sel != prev_sel) edges++;
    prev_clk = clk;
    prev_sel = sel;

    if (edges >= 6) {
        platform_reboot();  // console is scanning now → re-detect into play mode
    }

    // Decay the counter so a stray transition (noise) never accumulates into a
    // false reboot over minutes on a PC — only sustained toggling reaches 6.
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (now - win_start > 500) {
        edges = 0;
        win_start = now;
    }
}
