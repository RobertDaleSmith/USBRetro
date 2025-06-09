// gamecube.h

#ifndef GAMECUBE_H
#define GAMECUBE_H

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "tusb.h"
#include "hardware/pio.h"
#include "lib/joybus-pio/include/gamecube_definitions.h"
#include "globals.h"

// Define constants
#undef MAX_PLAYERS
#define MAX_PLAYERS 4
#define GC_KEY_NOT_FOUND 0x00 // Define lookup table (all initialized to NOT_FOUND to start)

#define SHIELD_PIN_L 4  // Connector shielding mounted to GPIOs [4, 5,26,27]
#define SHIELD_PIN_R 26

#define BOOTSEL_PIN 11
#define GC_DATA_PIN 7
#define GC_3V3_PIN 6

// NGC button modes
#define BUTTON_MODE_0  0x00
#define BUTTON_MODE_1  0x01
#define BUTTON_MODE_2  0x02
#define BUTTON_MODE_3  0x03
#define BUTTON_MODE_4  0x04
#define BUTTON_MODE_KB 0x05

// Global variables
extern PIO pio;

// Function declarations
void ngc_init(void);

void __not_in_flash_func(core1_entry)(void);
void __not_in_flash_func(update_output)(void);
void __not_in_flash_func(post_globals)(uint8_t dev_addr, int8_t instance,
  uint32_t buttons, uint8_t analog_1x, uint8_t analog_1y,
  uint8_t analog_2x, uint8_t analog_2y, uint8_t analog_l,
  uint8_t analog_r, uint32_t keys, uint8_t quad_x);
void __not_in_flash_func(post_mouse_globals)(uint8_t dev_addr, int8_t instance,
  uint16_t buttons, uint8_t delta_x, uint8_t delta_y, uint8_t quad_x);

#endif // GAMECUBE_H
