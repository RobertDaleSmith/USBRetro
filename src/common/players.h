// players.h

#ifndef PLAYERS_H
#define PLAYERS_H

#include <stdint.h>
#include "tusb.h"
#ifdef CONFIG_NGC
#include "lib/joybus-pio/include/gamecube_definitions.h"
#endif

#ifndef MAX_PLAYERS
#define MAX_PLAYERS 5
#endif

// Define constants
typedef struct TU_ATTR_PACKED
{
  int dev_addr;
  int instance;
  int player_number;

  int32_t global_buttons;
  int32_t altern_buttons;
  int16_t global_x;
  int16_t global_y;

  int32_t output_buttons;
  int16_t output_analog_1x;
  int16_t output_analog_1y;
  int16_t output_analog_2x;
  int16_t output_analog_2y;
  int16_t output_analog_l;
  int16_t output_analog_r;

  uint8_t keypress[3];

  int32_t prev_buttons;

  int button_mode;
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
