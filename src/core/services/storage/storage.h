// storage.h - Storage Subsystem
//
// Unified storage control for persistent settings.
// Currently wraps flash, but can expand to SD card or other mediums.

#ifndef STORAGE_H
#define STORAGE_H

#include <stdint.h>
#include <stdbool.h>

// Initialize storage subsystem
void storage_init(void);

// Update storage state (call from main loop)
void storage_task(void);

#endif // STORAGE_H
