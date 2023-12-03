#ifndef DEVICE_UTILS_H
#define DEVICE_UTILS_H

#include <stdint.h>
#include <stdbool.h>

// Common utility function declarations
bool diff_than_n(uint16_t x, uint16_t y, uint8_t n);

void ensureAllNonZero(uint8_t* axis_1x, uint8_t* axis_1y, uint8_t* axis_2x, uint8_t* axis_2y);

#endif // DEVICE_UTILS_H
