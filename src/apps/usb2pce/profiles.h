// profiles.h - USB2PCE App Profiles
// SPDX-License-Identifier: Apache-2.0
//
// Built-in profiles select the PCEngine controller button mode. Switch them with
// the universal profile hotkey (SELECT + D-pad Up/Down) or the web config; the
// choice persists and restores on boot. The PCEngine driver reads the active
// profile's output_mode (see pcengine_device.c).

#ifndef USB2PCE_PROFILES_H
#define USB2PCE_PROFILES_H

#include "core/services/profiles/profile.h"

// output_mode maps to the PCEngine driver's BUTTON_MODE_* (pcengine_device.h):
//   0 = 2-button, 1 = 6-button, 2 = 3-button (Select), 3 = 3-button (Run)
static const profile_t usb2pce_profiles[] = {
    {
        .name = "2-Button",
        .description = "PCEngine standard 2-button pad",
        .button_map = NULL, .button_map_count = 0,
        .combo_map = NULL, .combo_map_count = 0,
        PROFILE_TRIGGERS_DEFAULT,
        PROFILE_ANALOG_DEFAULT,
        .adaptive_triggers = false,
        .output_mode = 0,
    },
    {
        .name = "6-Button",
        .description = "PCEngine 6-button pad (Avenue Pad 6)",
        .button_map = NULL, .button_map_count = 0,
        .combo_map = NULL, .combo_map_count = 0,
        PROFILE_TRIGGERS_DEFAULT,
        PROFILE_ANALOG_DEFAULT,
        .adaptive_triggers = false,
        .output_mode = 1,
    },
    {
        .name = "3-Button (Sel)",
        .description = "PCEngine 3-button, Select toggles set",
        .button_map = NULL, .button_map_count = 0,
        .combo_map = NULL, .combo_map_count = 0,
        PROFILE_TRIGGERS_DEFAULT,
        PROFILE_ANALOG_DEFAULT,
        .adaptive_triggers = false,
        .output_mode = 2,
    },
    {
        .name = "3-Button (Run)",
        .description = "PCEngine 3-button, Run toggles set",
        .button_map = NULL, .button_map_count = 0,
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
