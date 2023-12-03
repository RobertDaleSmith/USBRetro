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

bool is_fun;
unsigned char fun_inc;
unsigned char fun_player;
const char* dpad_str[9];

#endif // GLOBALS_H
