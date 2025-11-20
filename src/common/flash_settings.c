// flash_settings.c - Persistent settings storage in flash memory

#include "flash_settings.h"
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include <string.h>

// Flash memory layout
// - RP2040 flash is memory-mapped at XIP_BASE (0x10000000)
// - We use the last 4KB sector for settings storage
// - Flash writes require erasing entire 4KB sectors
// - Flash writes must be 256-byte aligned

#define SETTINGS_MAGIC 0x47435052  // "GCPR" - GameCube Profile
#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)
#define SAVE_DEBOUNCE_MS 5000  // Wait 5 seconds after last change before writing

// Pending save state
static bool save_pending = false;
static absolute_time_t last_change_time;
static flash_settings_t pending_settings;

void flash_settings_init(void)
{
    save_pending = false;
}

// Load settings from flash (returns true if valid settings found)
bool flash_settings_load(flash_settings_t* settings)
{
    // Flash is memory-mapped at XIP_BASE, so we can read it directly
    const flash_settings_t* flash_settings = (const flash_settings_t*)(XIP_BASE + FLASH_TARGET_OFFSET);

    // Validate magic number
    if (flash_settings->magic != SETTINGS_MAGIC) {
        return false;  // No valid settings in flash
    }

    // Copy settings from flash to RAM
    memcpy(settings, flash_settings, sizeof(flash_settings_t));
    return true;
}

// Save settings to flash (debounced - actual write happens after delay)
void flash_settings_save(const flash_settings_t* settings)
{
    // Store settings and mark as pending
    memcpy(&pending_settings, settings, sizeof(flash_settings_t));
    pending_settings.magic = SETTINGS_MAGIC;  // Ensure magic is set
    save_pending = true;
    last_change_time = get_absolute_time();
}

// Force immediate save (bypasses debouncing - use sparingly)
void flash_settings_save_now(const flash_settings_t* settings)
{
    flash_settings_t write_settings;
    memcpy(&write_settings, settings, sizeof(flash_settings_t));
    write_settings.magic = SETTINGS_MAGIC;

    // CRITICAL SECTION: Disable interrupts during flash write
    // This will cause a brief hiccup in GameCube joybus communication (~100ms)
    // but is necessary to prevent flash corruption
    uint32_t ints = save_and_disable_interrupts();

    // Erase the settings sector (4KB)
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);

    // Write settings (must be 256-byte aligned)
    flash_range_program(FLASH_TARGET_OFFSET, (const uint8_t*)&write_settings, FLASH_PAGE_SIZE);

    restore_interrupts(ints);

    save_pending = false;
}

// Task function to handle debounced flash writes (call from main loop)
void flash_settings_task(void)
{
    if (!save_pending) {
        return;
    }

    // Check if debounce time has elapsed
    int64_t time_since_change = absolute_time_diff_us(last_change_time, get_absolute_time());
    if (time_since_change >= (SAVE_DEBOUNCE_MS * 1000)) {
        // Debounce period elapsed - perform the write
        flash_settings_save_now(&pending_settings);
    }
}
