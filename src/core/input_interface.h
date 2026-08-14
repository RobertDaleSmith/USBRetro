// input_interface.h
// Input abstraction for Joypad - supports USB host, native, BLE, and UART inputs
//
// Mirrors OutputInterface pattern - apps declare which inputs they use.

#ifndef INPUT_INTERFACE_H
#define INPUT_INTERFACE_H

#include <stdint.h>
#include <stdbool.h>
#include "core/router/router.h"

// Input interface - abstracts different input sources
typedef struct {
    const char* name;                    // Input name (e.g., "USB Host", "SNES", "BLE")
    input_source_t source;               // Router source type for routing table

    void (*init)(void);                  // Initialize input hardware/protocol
    void (*task)(void);                  // Core 0 polling task (NULL if not needed)

    // Status (optional)
    bool (*is_connected)(void);          // Any device connected? (NULL = always true)
    uint8_t (*get_device_count)(void);   // Number of connected devices (NULL = unknown)
} InputInterface;

// Maximum inputs per app (USB host + native + BLE + UART)
#define MAX_INPUT_INTERFACES 4

// The app's canonical/primary input source, set by the app in app_init even when
// the live input isn't running (e.g. a console adapter in CDC config mode, where
// the USB port is a device). Mirrors native_output so the web config can report
// the firmware's true I/O ("USB Host → PCEngine") regardless of transport. NULL
// when the app doesn't declare one.
extern const InputInterface* native_input;

#endif // INPUT_INTERFACE_H
