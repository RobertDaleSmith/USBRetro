// host_interface.h - Native Host Interface
//
// Abstraction for native controller input sources (SNES, N64, Genesis, etc.)
// Mirrors the DeviceInterface pattern used for USB input devices.

#ifndef HOST_INTERFACE_H
#define HOST_INTERFACE_H

#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// HOST INTERFACE
// ============================================================================

typedef struct {
    const char* name;                    // Host name (e.g., "SNES", "N64")

    // Initialization
    void (*init)(void);                  // Initialize with default pins
    void (*init_pins)(const uint8_t* pins, uint8_t pin_count);  // Initialize with custom pins

    // Polling
    void (*task)(void);                  // Poll controllers, submit to router

    // Status
    bool (*is_connected)(void);          // Any controller connected?
    int8_t (*get_device_type)(uint8_t port);  // Get device type at port (-1=none)
    uint8_t (*get_port_count)(void);     // Number of ports supported
} HostInterface;

#endif // HOST_INTERFACE_H
