// core/services/storage/flash.h - Persistent settings storage in flash memory
//
// Stores user settings (like active profile index) in the last sector of flash.
// Settings persist across power cycles and firmware updates (unless flash is erased).

#ifndef FLASH_H
#define FLASH_H

#include <stdint.h>
#include <stdbool.h>

// Settings structure stored in flash
typedef struct {
    uint32_t magic;              // Validation magic number (0x47435052 = "GCPR")
    uint8_t active_profile_index; // Currently selected profile (0-N)
    uint8_t usb_output_mode;     // USB device output mode (0=HID, 1=XboxOG, etc.)
    uint8_t reserved[250];        // Reserved for future settings (padding to 256 bytes)
} flash_t;

// Initialize flash settings system
void flash_init(void);

// Load settings from flash (returns true if valid settings found)
bool flash_load(flash_t* settings);

// Save settings to flash (debounced - actual write happens after delay)
void flash_save(const flash_t* settings);

// Force immediate save (bypasses debouncing - use sparingly)
void flash_save_now(const flash_t* settings);

// Task function to handle debounced flash writes (call from main loop)
void flash_task(void);

#endif // FLASH_H
