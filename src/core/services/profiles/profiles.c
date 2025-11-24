// profiles.c
// USBRetro Core - Universal Profile System Implementation
//
// Provides profile storage, loading, and switching infrastructure.

#include "profiles.h"
#include "common/flash_settings.h"
#include <stdio.h>
#include <string.h>

// ============================================================================
// PROFILE STATE
// ============================================================================

// Current profile system configuration
static profile_system_config_t current_config = {
    .profile_count = 1,
    .default_profile_index = 0,
};

// Active profile index (0-based)
static uint8_t active_profile_index = 0;

// Profile switch callback (optional, registered by console)
static profile_switch_callback_t switch_callback = NULL;

// ============================================================================
// INITIALIZATION
// ============================================================================

void profiles_init(const profile_system_config_t* config)
{
    if (!config) {
        printf("[profiles] ERROR: NULL config, using defaults\n");
        return;
    }

    current_config = *config;
    active_profile_index = config->default_profile_index;

    printf("[profiles] Initialized profile system\n");
    printf("[profiles]   Profile count: %d\n", config->profile_count);
    printf("[profiles]   Default profile: %d\n", config->default_profile_index);
}

// ============================================================================
// PROFILE ACCESSORS
// ============================================================================

uint8_t profile_get_active_index(void)
{
    return active_profile_index;
}

void profile_set_active(uint8_t index)
{
    if (index >= current_config.profile_count) {
        printf("[profiles] ERROR: Invalid profile index %d (max %d)\n",
            index, current_config.profile_count - 1);
        return;
    }

    active_profile_index = index;
    printf("[profiles] Active profile set to: %d\n", index);

    // Trigger callback if registered
    if (switch_callback) {
        switch_callback(index);
    }

    // Save to flash (debounced - actual write after 5 seconds)
    profile_save_active_index_to_flash(index);
}

uint8_t profile_get_count(void)
{
    return current_config.profile_count;
}

// ============================================================================
// FLASH STORAGE
// ============================================================================

uint8_t profile_load_active_index_from_flash(uint8_t default_index)
{
    // Use existing flash_settings system
    extern bool flash_settings_load(flash_settings_t* settings);
    flash_settings_t settings;

    if (flash_settings_load(&settings)) {
        // Valid settings found - check if profile index is valid
        if (settings.active_profile_index < current_config.profile_count) {
            printf("[profiles] Loaded profile from flash: %d\n", settings.active_profile_index);
            return settings.active_profile_index;
        } else {
            printf("[profiles] Invalid profile index in flash (%d), using default\n",
                settings.active_profile_index);
            return default_index;
        }
    } else {
        printf("[profiles] No valid settings in flash, using default profile\n");
        return default_index;
    }
}

void profile_save_active_index_to_flash(uint8_t index)
{
    // Use existing flash_settings system (debounced - writes after 5 seconds)
    extern void flash_settings_save(const flash_settings_t* settings);
    flash_settings_t settings;
    settings.magic = 0x47435052;  // "GCPR" magic
    settings.active_profile_index = index;
    flash_settings_save(&settings);
}

// ============================================================================
// PROFILE CYCLING
// ============================================================================

uint8_t profile_cycle_next(void)
{
    uint8_t new_index = (active_profile_index + 1) % current_config.profile_count;
    profile_set_active(new_index);
    return new_index;
}

uint8_t profile_cycle_prev(void)
{
    uint8_t new_index;
    if (active_profile_index == 0) {
        new_index = current_config.profile_count - 1;
    } else {
        new_index = active_profile_index - 1;
    }
    profile_set_active(new_index);
    return new_index;
}

// ============================================================================
// CALLBACK REGISTRATION
// ============================================================================

void profile_register_switch_callback(profile_switch_callback_t callback)
{
    switch_callback = callback;
    printf("[profiles] Profile switch callback registered\n");
}
