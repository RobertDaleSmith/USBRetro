// ws2812.h - NeoPixel LED Control
//
// Controls WS2812 RGB LED for status indication.

#ifndef WS2812_H
#define WS2812_H

#include <stdint.h>
#include <stdbool.h>

// Initialize NeoPixel LED
void neopixel_init(void);

// Update NeoPixel LED pattern based on player count
// pat: number of connected players (0 = no players, shows idle pattern)
void neopixel_task(int pat);

// Trigger profile indicator blink pattern
// profile_index: 0-3 (blinks profile_index + 1 times)
void neopixel_indicate_profile(uint8_t profile_index);

// Check if profile indicator is currently active
bool neopixel_is_indicating(void);

// Set custom per-LED colors from GPIO config
// colors: array of [n][3] RGB values, count: number of LEDs
void neopixel_set_custom_colors(const uint8_t colors[][3], uint8_t count);

// Check if custom colors are active
bool neopixel_has_custom_colors(void);

#endif // WS2812_H
