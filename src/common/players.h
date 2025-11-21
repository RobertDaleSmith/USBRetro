// players.h

#ifndef PLAYERS_H
#define PLAYERS_H

#include <stdint.h>
#include "tusb.h"
#include "input_event.h"
#ifdef CONFIG_NGC
#include "lib/joybus-pio/include/gamecube_definitions.h"
#endif

#ifndef MAX_PLAYERS
#define MAX_PLAYERS 5
#endif

// Define constants
typedef struct TU_ATTR_PACKED
{
  // Device identification
  int dev_addr;
  int instance;
  int player_number;
  input_device_type_t device_type;  // NEW: Device type (gamepad, flightstick, mouse, etc.)

  // Digital inputs
  int32_t global_buttons;
  int32_t altern_buttons;
  int32_t output_buttons;
  int32_t prev_buttons;

  // Analog inputs (unified array)
  // [0]=LX, [1]=LY, [2]=RX, [3]=RY, [4]=LT, [5]=RT, [6]=Extra1, [7]=Extra2
  int16_t analog[8];              // NEW: Replaces output_analog_1x/y, 2x/y, l, r

  // Mouse/relative motion accumulators
  int16_t global_x;
  int16_t global_y;

  // Keyboard
  uint8_t keypress[3];

  // Mode
  int button_mode;

  // Console-specific extensions
#ifdef CONFIG_NGC
  gc_report_t gc_report;
#elif CONFIG_NUON
  int32_t output_buttons_alt;
  int16_t output_quad_x;
#endif
} Player_t;

// Declaration of global variables
extern Player_t players[MAX_PLAYERS];
extern int playersCount;

// used to set the LED patterns on PS3/Switch controllers
extern const uint8_t PLAYER_LEDS[11];

// Function declarations
int __not_in_flash_func(find_player_index)(int dev_addr, int instance);
int __not_in_flash_func(add_player)(int dev_addr, int instance);
void remove_players_by_address(int dev_addr, int instance);

#endif // PLAYERS_H
