// profile.c - Universal Profile System Implementation
//
// Provides shared profile switching logic for all output devices.
// Two usage modes:
//
// 1. Universal profiles: Output device provides usbr_profile_t array via profile_init()
//    Core manages everything, device just reads profile_get_active()
//
// 2. Device-specific profiles: Output device uses profile_init_simple() with count/default
//    Core manages index and switching, device maintains its own profile structures
//    Device can use on_switch callback to update its own state

#include "profile.h"
#include "pico/stdlib.h"
#include <stdio.h>

// External dependencies (feedback and visual indication)
extern void neopixel_indicate_profile(uint8_t profile_index);
extern bool neopixel_is_indicating(void);
extern void feedback_trigger(uint8_t profile_index, uint8_t player_count);
extern bool feedback_is_active(void);

// Flash storage
#include "core/services/storage/flash.h"

// ============================================================================
// PROFILE SYSTEM STATE
// ============================================================================

// Configuration (can be NULL for simple mode)
static const profile_config_t* config = NULL;

// Simple mode config (used when config is NULL)
static uint8_t simple_profile_count = 0;
static const char* const* simple_profile_names = NULL;

// Active index (always used)
static uint8_t active_index = 0;

// Profile switch combo state
static uint32_t select_hold_start = 0;
static bool select_was_held = false;
static bool dpad_up_was_pressed = false;
static bool dpad_down_was_pressed = false;
static bool initial_trigger_done = false;

// Timing constants
static const uint32_t INITIAL_HOLD_TIME_MS = 2000;  // Must hold 2 seconds for first trigger

// Callbacks (set by output device)
static uint8_t (*get_player_count)(void) = NULL;
static void (*on_switch_callback)(uint8_t new_index) = NULL;

// ============================================================================
// PROFILE SYSTEM API
// ============================================================================

void profile_init(const profile_config_t* cfg)
{
    config = cfg;
    simple_profile_count = 0;
    simple_profile_names = NULL;

    if (!config || config->profile_count == 0) {
        active_index = 0;
        return;
    }

    // Load saved profile from flash
    active_index = profile_load_from_flash(config->default_index);

    // Validate index
    if (active_index >= config->profile_count) {
        active_index = config->default_index;
    }
}

void profile_init_simple(uint8_t count, uint8_t default_index, const char* const* names)
{
    config = NULL;
    simple_profile_count = count;
    simple_profile_names = names;

    if (count == 0) {
        active_index = 0;
        return;
    }

    // Load saved profile from flash
    active_index = profile_load_from_flash(default_index);

    // Validate index
    if (active_index >= count) {
        active_index = default_index;
    }
}

void profile_set_player_count_callback(uint8_t (*callback)(void))
{
    get_player_count = callback;
}

void profile_set_switch_callback(void (*callback)(uint8_t new_index))
{
    on_switch_callback = callback;
}

const usbr_profile_t* profile_get_active(void)
{
    if (!config || config->profile_count == 0) {
        return NULL;
    }
    return &config->profiles[active_index];
}

uint8_t profile_get_active_index(void)
{
    return active_index;
}

uint8_t profile_get_count(void)
{
    if (config) return config->profile_count;
    return simple_profile_count;
}

const char* profile_get_name(uint8_t index)
{
    if (config) {
        if (index >= config->profile_count) return NULL;
        return config->profiles[index].name;
    }
    if (simple_profile_names && index < simple_profile_count) {
        return simple_profile_names[index];
    }
    return NULL;
}

// ============================================================================
// PROFILE SWITCHING
// ============================================================================

void profile_set_active(uint8_t index)
{
    uint8_t count = profile_get_count();
    if (count == 0 || index >= count) {
        return;
    }

    active_index = index;

    // Notify output device of switch (for device-specific profile updates)
    if (on_switch_callback) {
        on_switch_callback(index);
    }

    // Trigger visual and haptic feedback
    neopixel_indicate_profile(active_index);

    uint8_t player_count = get_player_count ? get_player_count() : 0;
    feedback_trigger(active_index, player_count);

    // Save to flash (debounced)
    profile_save_to_flash();

    const char* name = profile_get_name(index);
    printf("Profile switched to: %s\n", name ? name : "(unknown)");
}

void profile_cycle_next(void)
{
    uint8_t count = profile_get_count();
    if (count == 0) return;
    uint8_t new_index = (active_index + 1) % count;
    profile_set_active(new_index);
}

void profile_cycle_prev(void)
{
    uint8_t count = profile_get_count();
    if (count == 0) return;
    uint8_t new_index = (active_index == 0) ? (count - 1) : (active_index - 1);
    profile_set_active(new_index);
}

// ============================================================================
// PROFILE SWITCH COMBO DETECTION
// ============================================================================
// SELECT + D-pad Up/Down to cycle profiles
// Requires holding SELECT for 2 seconds before first switch

void profile_check_switch_combo(uint32_t buttons)
{
    uint8_t count = profile_get_count();
    if (count <= 1) return;

    uint8_t player_count = get_player_count ? get_player_count() : 0;
    if (player_count == 0) return;  // No controllers connected

    // Check button states (buttons are active-low in USBR format)
    bool select_held = ((buttons & USBR_BUTTON_S1) == 0);
    bool dpad_up_pressed = ((buttons & USBR_BUTTON_DU) == 0);
    bool dpad_down_pressed = ((buttons & USBR_BUTTON_DD) == 0);

    // Select released - reset everything
    if (!select_held) {
        select_hold_start = 0;
        select_was_held = false;
        dpad_up_was_pressed = false;
        dpad_down_was_pressed = false;
        initial_trigger_done = false;
        return;
    }

    // Select is held
    if (!select_was_held) {
        // Select just pressed - start timer
        select_hold_start = to_ms_since_boot(get_absolute_time());
        select_was_held = true;
    }

    uint32_t current_time = to_ms_since_boot(get_absolute_time());
    uint32_t select_hold_duration = current_time - select_hold_start;

    // Check if initial 2-second hold period has elapsed
    bool can_trigger = initial_trigger_done || (select_hold_duration >= INITIAL_HOLD_TIME_MS);

    if (!can_trigger) {
        // Still waiting for initial 2-second hold
        return;
    }

    // Don't allow switching while feedback is still active
    if (neopixel_is_indicating() || feedback_is_active()) {
        return;
    }

    // D-pad Up - cycle forward on rising edge
    if (dpad_up_pressed && !dpad_up_was_pressed) {
        profile_cycle_next();
        initial_trigger_done = true;
    }
    dpad_up_was_pressed = dpad_up_pressed;

    // D-pad Down - cycle backward on rising edge
    if (dpad_down_pressed && !dpad_down_was_pressed) {
        profile_cycle_prev();
        initial_trigger_done = true;
    }
    dpad_down_was_pressed = dpad_down_pressed;
}

// ============================================================================
// FLASH PERSISTENCE
// ============================================================================

uint8_t profile_load_from_flash(uint8_t default_index)
{
    flash_t settings;
    if (flash_load(&settings)) {
        return settings.active_profile_index;
    }
    return default_index;
}

void profile_save_to_flash(void)
{
    flash_t settings;
    settings.active_profile_index = active_index;
    flash_save(&settings);
}
