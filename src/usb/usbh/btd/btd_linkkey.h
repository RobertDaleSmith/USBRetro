// btd_linkkey.h - Bluetooth Link Key Storage
// Persistent storage for paired device link keys
//
// Stores link keys in flash to allow reconnection without re-pairing.

#ifndef BTD_LINKKEY_H
#define BTD_LINKKEY_H

#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// CONSTANTS
// ============================================================================

#define BTD_LINKKEY_MAX_DEVICES     8   // Max number of paired devices to store
#define BTD_LINKKEY_SIZE            16  // Link key is 16 bytes
#define BTD_BDADDR_SIZE             6   // BD_ADDR is 6 bytes

// ============================================================================
// TYPES
// ============================================================================

// Stored link key entry
typedef struct {
    uint8_t  bd_addr[BTD_BDADDR_SIZE];   // Device Bluetooth address
    uint8_t  link_key[BTD_LINKKEY_SIZE]; // Link key for this device
    uint8_t  key_type;                    // Key type (from Link Key Notification)
    uint8_t  flags;                       // Flags (valid, etc.)
} btd_linkkey_entry_t;

// Flags for link key entry
#define BTD_LINKKEY_FLAG_VALID      0x01
#define BTD_LINKKEY_FLAG_PERSISTENT 0x02  // Don't auto-delete on fail

// Link key storage structure (stored in flash)
typedef struct {
    uint32_t magic;                                      // Validation: 0x42544C4B ("BTLK")
    uint32_t version;                                    // Storage format version
    btd_linkkey_entry_t entries[BTD_LINKKEY_MAX_DEVICES];
    uint8_t  reserved[16];                               // Reserved for future use
} btd_linkkey_storage_t;

#define BTD_LINKKEY_MAGIC       0x42544C4B  // "BTLK"
#define BTD_LINKKEY_VERSION     1

// ============================================================================
// API
// ============================================================================

// Initialize link key storage (loads from flash)
void btd_linkkey_init(void);

// Look up a link key by BD_ADDR
// Returns pointer to link key (16 bytes) or NULL if not found
const uint8_t* btd_linkkey_find(const uint8_t* bd_addr);

// Get key type for a stored link key
// Returns key type or 0xFF if not found
uint8_t btd_linkkey_get_type(const uint8_t* bd_addr);

// Store a new link key (or update existing)
// Returns true on success
bool btd_linkkey_store(const uint8_t* bd_addr, const uint8_t* link_key, uint8_t key_type);

// Delete a link key entry
// Returns true if entry was found and deleted
bool btd_linkkey_delete(const uint8_t* bd_addr);

// Delete all link keys (unpair all devices)
void btd_linkkey_delete_all(void);

// Get number of stored link keys
uint8_t btd_linkkey_count(void);

// Get link key entry by index (for enumeration)
// Returns NULL if index out of range or entry invalid
const btd_linkkey_entry_t* btd_linkkey_get_entry(uint8_t index);

// Save link keys to flash (debounced)
void btd_linkkey_save(void);

// Force immediate save to flash
void btd_linkkey_save_now(void);

// Task function for debounced saves (call from main loop)
void btd_linkkey_task(void);

#endif // BTD_LINKKEY_H
