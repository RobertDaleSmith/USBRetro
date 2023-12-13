#ifndef DEVICE_INTERFACE_H
#define DEVICE_INTERFACE_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    const char* name; // Add this line
    bool (*is_device)(uint16_t vid, uint16_t pid);
    bool (*check_descriptor)(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len);
    void (*process)(uint8_t dev_addr, uint8_t instance, const uint8_t *report, uint16_t len);
    void (*task)(uint8_t dev_addr, uint8_t instance, int player_index, uint8_t rumble, uint8_t leds);
    bool (*init)(uint8_t dev_addr, uint8_t instance);
    void (*unmount)(uint8_t dev_addr, uint8_t instance);
    // Add other common functions as needed
} DeviceInterface;

#endif // DEVICE_INTERFACE_H
