// profile_indicator.h - Profile switching feedback management
//
// Manages visual/haptic feedback when switching profiles.
// Uses core/feedback.h for per-player feedback state.
// NeoPixel LED blinking is handled separately in ws2812.c

#ifndef PROFILE_INDICATOR_H
#define PROFILE_INDICATOR_H

#include <stdint.h>
#include <stdbool.h>

// Initialize profile indicator system
void profile_indicator_init(void);

// Update profile indicator state machines (call from main loop)
void profile_indicator_task(void);

// Trigger profile indicator for a specific player
// player_index: which player triggered the switch (0-based)
// profile_index: 0-3 (profile 0 = 1 blink, profile 1 = 2 blinks, etc.)
// player_count: current player count to restore LEDs after blinking
void profile_indicator_trigger_player(uint8_t player_index, uint8_t profile_index, uint8_t player_count);

// Legacy: Trigger profile indicator for player 0
void profile_indicator_trigger(uint8_t profile_index, uint8_t player_count);

// Get current rumble value for profile indicator (0 = off, 255 = on)
// Legacy interface - prefer using feedback_get_state() for per-player access
uint8_t profile_indicator_get_rumble(void);

// Get current player LED value for profile indicator
// Returns LED bitmask from PLAYER_LEDS array
uint8_t profile_indicator_get_player_led(uint8_t player_count);

// Check if profile indicator is currently active
bool profile_indicator_is_active(void);

// Check if profile indicator is active for a specific player
bool profile_indicator_is_active_for_player(uint8_t player_index);

// Get the player index to display (overrides actual player index during indication)
// Returns -1 for OFF state, profile_index for ON state, or actual_player_index when not active
int8_t profile_indicator_get_display_player_index(int8_t actual_player_index);

#endif // PROFILE_INDICATOR_H
