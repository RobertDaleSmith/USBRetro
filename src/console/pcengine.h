// pcengine.h

#ifndef PCENGINE_H
#define PCENGINE_H

#include <stdint.h>

#include "tusb.h"
#include "hardware/pio.h"
#include "plex.pio.h"
#include "clock.pio.h"
#include "select.pio.h"
#include "globals.h"

// Define constants
#undef MAX_PLAYERS
#define MAX_PLAYERS 5               // PCE supports up to 5 players

// ADAFRUIT_KB2040                  // build for Adafruit KB2040 board
#define DATAIN_PIN  18
#define CLKIN_PIN   DATAIN_PIN + 1  // Note - in pins must be a consecutive 'in' group
#define OUTD0_PIN   26              // Note - out pins must be a consecutive 'out' group
#define OUTD1_PIN   27
#define OUTD2_PIN   28
#define OUTD3_PIN   29

// PCE button modes
#define BUTTON_MODE_2 0x00
#define BUTTON_MODE_6 0x01
#define BUTTON_MODE_3_SEL 0x02
#define BUTTON_MODE_3_RUN 0x03

// Declaration of global variables
uint64_t cpu_frequency;
uint64_t timer_threshold;
uint64_t timer_threshold_a;
uint64_t timer_threshold_b;
uint64_t turbo_frequency;

PIO pio;
uint sm1, sm2, sm3; // sm1 = plex; sm2 = clock, sm3 = select

// Function declarations
void pce_init(void);
void pce_task(void);
void turbo_init(void);
void __not_in_flash_func(core1_entry)(void);
void __not_in_flash_func(update_output)(void);
void __not_in_flash_func(post_globals)(uint8_t dev_addr, int8_t instance,
  uint32_t buttons, uint8_t analog_1x, uint8_t analog_1y,
  uint8_t analog_2x, uint8_t analog_2y, uint8_t analog_l,
  uint8_t analog_r, uint32_t keys, uint8_t quad_x);
void __not_in_flash_func(post_mouse_globals)(uint8_t dev_addr, int8_t instance,
  uint16_t buttons, uint8_t delta_x, uint8_t delta_y, uint8_t quad_x);

#endif // PCENGINE_H
