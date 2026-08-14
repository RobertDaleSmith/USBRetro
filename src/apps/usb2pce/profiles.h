// profiles.h - USB2PCE App Profiles
// SPDX-License-Identifier: Apache-2.0
//
// Built-in profiles select the PCEngine controller button mode AND own the
// per-mode turbo, applied through the normal profile pipeline (profile_apply).
// Switch profiles with the universal profile hotkey (SELECT + D-pad Up/Down) or
// the web config; the choice persists and restores on boot. The PCEngine driver
// reads the active profile's output_mode for the 2- vs 6-button byte format and
// applies the button_map (remaps + autofire) each scan (see pcengine_device.c).

#ifndef USB2PCE_PROFILES_H
#define USB2PCE_PROFILES_H

#include "core/services/profiles/profile.h"

// Default turbo rate for the III/IV button holes in 2- and 3-button modes. The
// on-the-fly rapid-fire gesture can override any button live at runtime.
#define PCE_TURBO_RATE  AUTOFIRE_15HZ

// PCE button holes → controller buttons in the JoyPad model (pcengine_device.c):
//   I  is driven by B2, II is driven by B1. In 2-/3-button modes the III/IV
//   holes act as auto-fire of II/I (matching the classic PCE turbo pad):
//     B3 (III) → auto-fire II (B1),  B4 (IV) → auto-fire I (B2).

// 2-Button: III/IV holes = turbo II/I.
static const button_map_entry_t usb2pce_2btn_map[] = {
    MAP_AUTOFIRE(JP_BUTTON_B3, JP_BUTTON_B1, PCE_TURBO_RATE),
    MAP_AUTOFIRE(JP_BUTTON_B4, JP_BUTTON_B2, PCE_TURBO_RATE),
};

// 3-Button (Select): III → Select. (IV/B4 has no function in 3-button mode.)
static const button_map_entry_t usb2pce_3sel_map[] = {
    MAP_BUTTON(JP_BUTTON_B3, JP_BUTTON_S1),
};

// 3-Button (Run): III → Run. (IV/B4 has no function in 3-button mode.)
static const button_map_entry_t usb2pce_3run_map[] = {
    MAP_BUTTON(JP_BUTTON_B3, JP_BUTTON_S2),
};

// output_mode → PCEngine byte format (pcengine_device.h BUTTON_MODE_*):
//   0 = 2-button, 1 = 6-button, 2 = 3-button (Select), 3 = 3-button (Run)
static const profile_t usb2pce_profiles[] = {
    {
        .name = "2-Button",
        .description = "Standard 2-button, III/IV = turbo",
        .button_map = usb2pce_2btn_map,
        .button_map_count = sizeof(usb2pce_2btn_map) / sizeof(usb2pce_2btn_map[0]),
        .combo_map = NULL, .combo_map_count = 0,
        PROFILE_TRIGGERS_DEFAULT,
        PROFILE_ANALOG_DEFAULT,
        .adaptive_triggers = false,
        .output_mode = 0,
    },
    {
        .name = "6-Button",
        .description = "6-button pad (Avenue Pad 6), no turbo",
        .button_map = NULL, .button_map_count = 0,
        .combo_map = NULL, .combo_map_count = 0,
        PROFILE_TRIGGERS_DEFAULT,
        PROFILE_ANALOG_DEFAULT,
        .adaptive_triggers = false,
        .output_mode = 1,
    },
    {
        .name = "3-Button (Sel)",
        .description = "3-button, III = Select",
        .button_map = usb2pce_3sel_map,
        .button_map_count = sizeof(usb2pce_3sel_map) / sizeof(usb2pce_3sel_map[0]),
        .combo_map = NULL, .combo_map_count = 0,
        PROFILE_TRIGGERS_DEFAULT,
        PROFILE_ANALOG_DEFAULT,
        .adaptive_triggers = false,
        .output_mode = 2,
    },
    {
        .name = "3-Button (Run)",
        .description = "3-button, III = Run",
        .button_map = usb2pce_3run_map,
        .button_map_count = sizeof(usb2pce_3run_map) / sizeof(usb2pce_3run_map[0]),
        .combo_map = NULL, .combo_map_count = 0,
        PROFILE_TRIGGERS_DEFAULT,
        PROFILE_ANALOG_DEFAULT,
        .adaptive_triggers = false,
        .output_mode = 3,
    },
};

static const profile_set_t usb2pce_profile_set = {
    .profiles = usb2pce_profiles,
    .profile_count = sizeof(usb2pce_profiles) / sizeof(usb2pce_profiles[0]),
    .default_index = 0,
};

#endif // USB2PCE_PROFILES_H
