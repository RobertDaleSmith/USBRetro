// btd_linkkey.c - Bluetooth Link Key Storage Implementation
// Persistent storage for paired device link keys in flash

#include "btd_linkkey.h"
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/flash.h"
#include <string.h>
#include <stdio.h>

// ============================================================================
// FLASH LAYOUT
// ============================================================================

// Use the second-to-last 4KB sector for BT link keys
// (Last sector is used by flash.c for general settings)
#define BTD_FLASH_OFFSET    (PICO_FLASH_SIZE_BYTES - (2 * FLASH_SECTOR_SIZE))
#define BTD_SAVE_DEBOUNCE_MS 3000  // Wait 3 seconds before writing

// ============================================================================
// STATIC DATA
// ============================================================================

static btd_linkkey_storage_t linkkey_storage;
static bool save_pending = false;
static absolute_time_t last_change_time;
static bool initialized = false;

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

static int find_entry_index(const uint8_t* bd_addr)
{
    for (int i = 0; i < BTD_LINKKEY_MAX_DEVICES; i++) {
        if ((linkkey_storage.entries[i].flags & BTD_LINKKEY_FLAG_VALID) &&
            memcmp(linkkey_storage.entries[i].bd_addr, bd_addr, BTD_BDADDR_SIZE) == 0) {
            return i;
        }
    }
    return -1;
}

static int find_free_entry(void)
{
    for (int i = 0; i < BTD_LINKKEY_MAX_DEVICES; i++) {
        if (!(linkkey_storage.entries[i].flags & BTD_LINKKEY_FLAG_VALID)) {
            return i;
        }
    }
    return -1;  // Storage full
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void btd_linkkey_init(void)
{
    // Read link keys from flash
    const btd_linkkey_storage_t* flash_storage =
        (const btd_linkkey_storage_t*)(XIP_BASE + BTD_FLASH_OFFSET);

    if (flash_storage->magic == BTD_LINKKEY_MAGIC &&
        flash_storage->version == BTD_LINKKEY_VERSION) {
        // Valid storage found - load it
        memcpy(&linkkey_storage, flash_storage, sizeof(btd_linkkey_storage_t));
        printf("[BTD] Loaded %d paired devices from flash\n", btd_linkkey_count());
    } else {
        // No valid storage - initialize empty
        memset(&linkkey_storage, 0, sizeof(btd_linkkey_storage_t));
        linkkey_storage.magic = BTD_LINKKEY_MAGIC;
        linkkey_storage.version = BTD_LINKKEY_VERSION;
        printf("[BTD] Link key storage initialized (empty)\n");
    }

    save_pending = false;
    initialized = true;
}

// ============================================================================
// LINK KEY OPERATIONS
// ============================================================================

const uint8_t* btd_linkkey_find(const uint8_t* bd_addr)
{
    int idx = find_entry_index(bd_addr);
    if (idx >= 0) {
        return linkkey_storage.entries[idx].link_key;
    }
    return NULL;
}

uint8_t btd_linkkey_get_type(const uint8_t* bd_addr)
{
    int idx = find_entry_index(bd_addr);
    if (idx >= 0) {
        return linkkey_storage.entries[idx].key_type;
    }
    return 0xFF;
}

bool btd_linkkey_store(const uint8_t* bd_addr, const uint8_t* link_key, uint8_t key_type)
{
    if (!initialized) {
        btd_linkkey_init();
    }

    // Check if device already exists
    int idx = find_entry_index(bd_addr);

    if (idx < 0) {
        // New device - find free slot
        idx = find_free_entry();
        if (idx < 0) {
            // Storage full - remove oldest entry (index 0) and shift
            printf("[BTD] Link key storage full, removing oldest entry\n");
            memmove(&linkkey_storage.entries[0],
                    &linkkey_storage.entries[1],
                    sizeof(btd_linkkey_entry_t) * (BTD_LINKKEY_MAX_DEVICES - 1));
            idx = BTD_LINKKEY_MAX_DEVICES - 1;
        }
    }

    // Store the link key
    memcpy(linkkey_storage.entries[idx].bd_addr, bd_addr, BTD_BDADDR_SIZE);
    memcpy(linkkey_storage.entries[idx].link_key, link_key, BTD_LINKKEY_SIZE);
    linkkey_storage.entries[idx].key_type = key_type;
    linkkey_storage.entries[idx].flags = BTD_LINKKEY_FLAG_VALID;

    // Log
    printf("[BTD] Stored link key for %02X:%02X:%02X:%02X:%02X:%02X (type=%d)\n",
           bd_addr[5], bd_addr[4], bd_addr[3],
           bd_addr[2], bd_addr[1], bd_addr[0], key_type);

    // Schedule save
    btd_linkkey_save();
    return true;
}

bool btd_linkkey_delete(const uint8_t* bd_addr)
{
    int idx = find_entry_index(bd_addr);
    if (idx < 0) {
        return false;
    }

    // Clear the entry
    memset(&linkkey_storage.entries[idx], 0, sizeof(btd_linkkey_entry_t));

    printf("[BTD] Deleted link key for %02X:%02X:%02X:%02X:%02X:%02X\n",
           bd_addr[5], bd_addr[4], bd_addr[3],
           bd_addr[2], bd_addr[1], bd_addr[0]);

    btd_linkkey_save();
    return true;
}

void btd_linkkey_delete_all(void)
{
    for (int i = 0; i < BTD_LINKKEY_MAX_DEVICES; i++) {
        memset(&linkkey_storage.entries[i], 0, sizeof(btd_linkkey_entry_t));
    }
    printf("[BTD] Deleted all link keys\n");
    btd_linkkey_save();
}

uint8_t btd_linkkey_count(void)
{
    uint8_t count = 0;
    for (int i = 0; i < BTD_LINKKEY_MAX_DEVICES; i++) {
        if (linkkey_storage.entries[i].flags & BTD_LINKKEY_FLAG_VALID) {
            count++;
        }
    }
    return count;
}

const btd_linkkey_entry_t* btd_linkkey_get_entry(uint8_t index)
{
    if (index >= BTD_LINKKEY_MAX_DEVICES) {
        return NULL;
    }
    if (!(linkkey_storage.entries[index].flags & BTD_LINKKEY_FLAG_VALID)) {
        return NULL;
    }
    return &linkkey_storage.entries[index];
}

// ============================================================================
// FLASH PERSISTENCE
// ============================================================================

void btd_linkkey_save(void)
{
    save_pending = true;
    last_change_time = get_absolute_time();
}

static void __no_inline_not_in_flash_func(btd_linkkey_flash_write)(void* param)
{
    btd_linkkey_storage_t* storage = (btd_linkkey_storage_t*)param;

    // Erase the sector (4KB)
    flash_range_erase(BTD_FLASH_OFFSET, FLASH_SECTOR_SIZE);

    // Write storage (must write in 256-byte pages)
    // Our storage struct should fit in one page, but write full pages to be safe
    size_t write_size = ((sizeof(btd_linkkey_storage_t) + FLASH_PAGE_SIZE - 1)
                         / FLASH_PAGE_SIZE) * FLASH_PAGE_SIZE;
    flash_range_program(BTD_FLASH_OFFSET, (const uint8_t*)storage, write_size);
}

void btd_linkkey_save_now(void)
{
    if (!initialized) {
        return;
    }

    printf("[BTD] Saving link keys to flash...\n");

    // CRITICAL SECTION: Pause Core 1 and disable interrupts during flash write
    uint32_t ints = save_and_disable_interrupts();
    flash_safe_execute(btd_linkkey_flash_write, &linkkey_storage, UINT32_MAX);
    restore_interrupts(ints);

    save_pending = false;
    printf("[BTD] Link keys saved\n");
}

void btd_linkkey_task(void)
{
    if (!save_pending) {
        return;
    }

    // Check if debounce time has elapsed
    int64_t time_since_change = absolute_time_diff_us(last_change_time, get_absolute_time());
    if (time_since_change >= (BTD_SAVE_DEBOUNCE_MS * 1000)) {
        btd_linkkey_save_now();
    }
}
