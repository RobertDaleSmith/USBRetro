// globals.h
#ifndef GLOBALS_H
#define GLOBALS_H

#include "codes.h"
#include "players.h"

#define MAX_DEVICES 6

//
Player_t players[MAX_PLAYERS];
int playersCount;
bool update_pending;

// GameCube rumble and keyboard LED states
uint8_t gc_rumble;
uint8_t gc_kb_led;

// output logging dpad directions
const char* dpad_str[9];

// used to set the LED patterns on PS3/Switch controllers
const uint8_t PLAYER_LEDS[11];

// konami code easter egg
bool is_fun;
unsigned char fun_inc;
unsigned char fun_player;

// common console response for controller data
void __not_in_flash_func(post_globals)(
  uint8_t dev_addr,
  int8_t instance,
  uint32_t buttons,
  uint8_t analog_1x,
  uint8_t analog_1y,
  uint8_t analog_2x,
  uint8_t analog_2y,
  uint8_t analog_l,
  uint8_t analog_r,
  uint32_t keys,
  uint8_t quad_x
);

// common console response for mouse data
void __not_in_flash_func(post_mouse_globals)(
  uint8_t dev_addr,
  int8_t instance,
  uint16_t buttons,
  uint8_t delta_x,
  uint8_t delta_y,
  uint8_t spinner
);

int __not_in_flash_func(find_player_index)(int device_address, int instance_number);
int __not_in_flash_func(add_player)(int device_address, int instance_number);
void remove_players_by_address(int device_address, int instance);

#endif // GLOBALS_H
