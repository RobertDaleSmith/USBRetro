#ifndef DEVICE_INTERFACE_H
#define DEVICE_INTERFACE_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    bool (*is_device)(uint16_t vid, uint16_t pid);
    void (*process)(uint8_t dev_addr, uint8_t instance, const uint8_t *report, uint16_t len);
    void (*task)(uint8_t dev_addr, uint8_t instance, uint8_t player_index, uint8_t rumble);
    // Add other common functions as needed
} DeviceInterface;

#endif // DEVICE_INTERFACE_H
