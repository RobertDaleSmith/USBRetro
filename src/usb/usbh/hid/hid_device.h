#ifndef DEVICE_INTERFACE_H
#define DEVICE_INTERFACE_H

#include <stdint.h>
#include <stdbool.h>

// Device output configuration (passed from console to device drivers)
typedef struct {
    int player_index;          // Display player index (for LED patterns)
    uint8_t rumble;            // Rumble intensity (0-255, 0=off)
    uint8_t leds;              // LED pattern/state
    uint8_t trigger_threshold; // Adaptive trigger threshold (0=disabled, 1-255=threshold)
    uint8_t test;              // Test pattern counter (0=disabled)
} device_output_config_t;

typedef struct {
    const char* name; // Add this line
    bool (*is_device)(uint16_t vid, uint16_t pid);
    bool (*check_descriptor)(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len);
    void (*process)(uint8_t dev_addr, uint8_t instance, const uint8_t *report, uint16_t len);
    void (*task)(uint8_t dev_addr, uint8_t instance, device_output_config_t* config);
    bool (*init)(uint8_t dev_addr, uint8_t instance);
    void (*unmount)(uint8_t dev_addr, uint8_t instance);
    // Add other common functions as needed
} DeviceInterface;

#endif // DEVICE_INTERFACE_H
