// profile_indicator.c - Profile switching feedback management
//
// Manages rumble and player LED feedback when switching profiles.
// Note: NeoPixel LED blinking is handled separately in ws2812.c

#include "profile_indicator.h"
#include "players.h"
#include "pico/stdlib.h"
#include <stdint.h>
#include <stdbool.h>

// Profile indicator state
static volatile uint8_t profile_to_indicate = 0;

// Rumble pattern state
static volatile uint8_t rumble_blinks_remaining = 0;
static volatile bool rumble_is_on = false;
static absolute_time_t rumble_state_change_time;

// Player LED pattern state
static volatile uint8_t player_led_blinks_remaining = 0;
static volatile bool player_led_is_on = false;
static volatile uint8_t stored_player_count = 0;  // Store player count to restore after blinking
static absolute_time_t player_led_state_change_time;

// Timing constants - all synchronized for visual/haptic/LED feedback
#define RUMBLE_OFF_TIME_US 200000   // 200ms rumble off (matches NeoPixel OFF)
#define RUMBLE_ON_TIME_US 100000    // 100ms rumble on (matches NeoPixel ON)

// Player LED timing constants (synchronized with NeoPixel and rumble timing)
#define PLAYER_LED_OFF_TIME_US 200000  // 200ms LED off (this is what we count)
#define PLAYER_LED_ON_TIME_US 100000   // 100ms LED on (brief flash between OFF blinks)

void profile_indicator_init(void)
{
    profile_to_indicate = 0;

    rumble_blinks_remaining = 0;
    rumble_is_on = false;

    player_led_blinks_remaining = 0;
    player_led_is_on = false;
    stored_player_count = 0;
}

// Trigger profile indicator rumble and player LED patterns
void profile_indicator_trigger(uint8_t profile_index, uint8_t player_count)
{
    // Only trigger if not already active
    if (rumble_blinks_remaining == 0 && player_led_blinks_remaining == 0) {
        profile_to_indicate = profile_index;

        // Trigger rumble pattern (profile 0 = 1 pulse, etc.)
        rumble_blinks_remaining = profile_index + 1;
        rumble_is_on = true;  // Start with rumble ON
        rumble_state_change_time = get_absolute_time();

        // Trigger player LED pattern - blink N times between OFF and profile LED (N = profile_index + 1)
        player_led_blinks_remaining = profile_index + 1;
        player_led_is_on = false;  // Start with all LEDs OFF
        stored_player_count = player_count;  // Store player count to restore after blinking
        player_led_state_change_time = get_absolute_time();
    }
}

// Get current rumble value for profile indicator (0 = off, 255 = on)
uint8_t profile_indicator_get_rumble(void)
{
    return rumble_is_on ? 255 : 0;
}

// Get current player LED value for profile indicator
// Returns the LED bitmask from PLAYER_LEDS array
uint8_t profile_indicator_get_player_led(uint8_t player_count)
{
    // If profile indicator is active, return the blinking pattern
    if (player_led_blinks_remaining > 0) {
        if (player_led_is_on) {
            // LED on - show player LED N where N = profile_index + 1
            // Profile 0 uses LED1, profile 1 uses LED2, etc.
            return PLAYER_LEDS[profile_to_indicate + 1];
        } else {
            // LED off - all LEDs off
            return PLAYER_LEDS[0];
        }
    }

    // Normal operation - show player number
    return PLAYER_LEDS[player_count];
}

// Check if profile indicator is currently active
bool profile_indicator_is_active(void)
{
    return (rumble_blinks_remaining > 0 || player_led_blinks_remaining > 0);
}

// Get the player index to display (overrides actual player index during indication)
// This allows device drivers to naturally show profile indicator without modification
int8_t profile_indicator_get_display_player_index(int8_t actual_player_index)
{
    // If player LED indicator is active, override the player index
    if (player_led_blinks_remaining > 0) {
        if (player_led_is_on) {
            // LED on - show profile index (0-3)
            return profile_to_indicate;
        } else {
            // LED off - return -1 to show all LEDs off
            return -1;
        }
    }

    // Normal operation - return actual player index
    return actual_player_index;
}

// Update profile indicator state machines (called from main loop)
void profile_indicator_task(void)
{
    absolute_time_t current_time = get_absolute_time();

    // Handle rumble pattern state machine (runs independently, must run every loop)
    if (rumble_blinks_remaining > 0) {
        int64_t rumble_time_in_state = absolute_time_diff_us(rumble_state_change_time, current_time);

        if (rumble_is_on) {
            // Rumble is ON, check if it's time to turn OFF
            if (rumble_time_in_state >= RUMBLE_ON_TIME_US) {
                rumble_is_on = false;
                rumble_blinks_remaining--;
                rumble_state_change_time = current_time;
            }
        } else {
            // Rumble is OFF, check if we need another pulse
            if (rumble_time_in_state >= RUMBLE_OFF_TIME_US && rumble_blinks_remaining > 0) {
                rumble_is_on = true;
                rumble_state_change_time = current_time;
            }
        }
    }

    // Handle player LED pattern state machine (runs independently, must run every loop)
    if (player_led_blinks_remaining > 0) {
        int64_t player_led_time_in_state = absolute_time_diff_us(player_led_state_change_time, current_time);

        if (player_led_is_on) {
            // LED is ON (showing player number), check if it's time to turn OFF
            if (player_led_time_in_state >= PLAYER_LED_ON_TIME_US) {
                player_led_is_on = false;
                player_led_blinks_remaining--;
                player_led_state_change_time = current_time;
            }
        } else {
            // LED is OFF (all off), check if we need another blink
            if (player_led_time_in_state >= PLAYER_LED_OFF_TIME_US && player_led_blinks_remaining > 0) {
                player_led_is_on = true;
                player_led_state_change_time = current_time;
            }
        }
    }
}
