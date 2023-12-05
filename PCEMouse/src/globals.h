// globals.h
#ifndef GLOBALS_H
#define GLOBALS_H

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

void remove_players_by_address(int device_address, int instance);

// output logging dpad directions
const char* dpad_str[9];

// used to set the LED patterns on PS3/Switch controllers
const uint8_t PLAYER_LEDS[11];

// konami code easter egg
bool is_fun;
unsigned char fun_inc;
unsigned char fun_player;

#endif // GLOBALS_H
