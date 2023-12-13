/* 
 * PCEMouse - Adapts a USB mouse for use with the PC Engine
 *            For Raspberry Pi Pico or other RP2040 MCU
 *            In particular, I like the Adafruit QT Py RP2040 board
 *
 * This code is based on the TinyUSB Host HID example from pico-SDK v1.?.?
 *
 * Modifications for PCEMouse
 * Copyright (c) 2021 David Shadoff
 *
 * ------------------------------------
 *
 * The MIT License (MIT)
 *
 * Original TinyUSB example
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bsp/board_api.h"
#include "tusb.h"

#include "pico/stdlib.h"
#include "pico/time.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "globals.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+

#ifdef CONFIG_PCE

#include "plex.pio.h"
#include "clock.pio.h"
#include "select.pio.h"

uint64_t cpu_frequency;
uint64_t timer_threshold;
uint64_t timer_threshold_a;
uint64_t timer_threshold_b;
uint64_t turbo_frequency;

#define MAX_PLAYERS 5 // PCE supports up to 5 players

// ADAFRUIT_KB2040          // if build for Adafruit KB2040 board
#define DATAIN_PIN      18
#define CLKIN_PIN       DATAIN_PIN + 1  // Note - in pins must be a consecutive 'in' group
#define OUTD0_PIN       26              // Note - out pins must be a consecutive 'out' group
#define OUTD1_PIN       27
#define OUTD2_PIN       28
#define OUTD3_PIN       29

// PCE button modes
#define BUTTON_MODE_2 0x00
#define BUTTON_MODE_6 0x01
#define BUTTON_MODE_3_SEL 0x02
#define BUTTON_MODE_3_RUN 0x03

#elif CONFIG_NGC
  #include "lib/joybus-pio/include/gamecube_definitions.h"
  #include "joybus.pio.h"
  #include "GamecubeConsole.h"
  #include "pico/bootrom.h"
  // GameCube JoyBus resources:
  // https://github.com/NicoHood/Nintendo
  // https://n64brew.dev/wiki/Joybus_Protocol
  // http://hitmen.c02.at/files/yagcd/yagcd/chap9.html
  // https://simplecontrollers.com/blogs/resources/gamecube-protocol
  // https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering
  // https://sites.google.com/site/consoleprotocols/home/nintendo-joy-bus-documentation

  #define MAX_PLAYERS 4

  #define SHIELD_PIN_L 4  // Connector shielding mounted to GPIOs [4, 5,26,27]
  #define SHIELD_PIN_R 26

  #define BOOTSEL_PIN 11
  #define GC_DATA_PIN 7
  #define GC_3V3_PIN 6

  extern void GamecubeConsole_init(GamecubeConsole* console, uint pin, PIO pio, int sm, int offset);
  extern bool GamecubeConsole_WaitForPoll(GamecubeConsole* console);
  extern void GamecubeConsole_SendReport(GamecubeConsole* console, gc_report_t *report);
  extern void GamecubeConsole_SetMode(GamecubeConsole* console, GamecubeMode mode);

  GamecubeConsole gc;
  gc_report_t gc_report;

  // Define your lookup table (all initialized to NOT_FOUND to start)
  #define GC_KEY_NOT_FOUND 0x00
  uint8_t hid_to_gc_key[256] = {[0 ... 255] = GC_KEY_NOT_FOUND};

  // NGC button modes
  #define BUTTON_MODE_0  0x00
  #define BUTTON_MODE_1  0x01
  #define BUTTON_MODE_2  0x02
  #define BUTTON_MODE_3  0x03
  #define BUTTON_MODE_4  0x04
  #define BUTTON_MODE_KB 0x05
#elif CONFIG_XB1
  #include "hardware/i2c.h"
  #include "pico/i2c_slave.h"

  #define MAX_PLAYERS 4

  #ifdef ADAFRUIT_QTPY_RP2040      // if build for QtPy RP2040 board
    #define I2C_SLAVE_PORT i2c0
    #define I2C_SLAVE_SDA_PIN 4 // TP33
    #define I2C_SLAVE_SCL_PIN 5 // TP34

    #define I2C_DAC_PORT i2c1
    #define I2C_DAC_SDA_PIN 22
    #define I2C_DAC_SCL_PIN 23

    #define XBOX_R3_BTN_PIN 25  // TP43
    #define XBOX_L3_BTN_PIN 24  // TP42
    #define XBOX_GUIDE_PIN 20   // Cathode side of D27
    #define XBOX_B_BTN_PIN 21   // TP41

    #define PICO_DEFAULT_WS2812_PIN 12
    #define NEOPIXEL_POWER_PIN 11
    #define BOOT_BUTTON_PIN 21

  #else // #ifdef ADAFRUIT_KB2040  // if build for Adafruit KB2040 boardd
  
    #define I2C_SLAVE_PORT i2c1
    #define I2C_SLAVE_SDA_PIN 2
    #define I2C_SLAVE_SCL_PIN 3

    #define I2C_DAC_PORT i2c0
    #define I2C_DAC_SDA_PIN 12
    #define I2C_DAC_SCL_PIN 13

    #define XBOX_R3_BTN_PIN 6
    #define XBOX_L3_BTN_PIN 7
    #define XBOX_GUIDE_PIN 8
    #define XBOX_B_BTN_PIN 9
  #endif

#define I2C_SLAVE_ADDRESS 0x21
#define MCP4728_I2C_ADDR0 0x60
#define MCP4728_I2C_ADDR1 0x61

static uint8_t i2c_slave_read_buffer[2] = {0xFA, 0xFF};
static uint8_t i2c_slave_write_buffer[256];
static int i2c_slave_write_buffer_index = 0;
#elif CONFIG_NUON
  #include "pico/util/queue.h"
  #include "polyface_read.pio.h"
  #include "polyface_send.pio.h"

  #define MAX_PLAYERS       4

  #define DATAIO_PIN        2
  #define CLKIN_PIN         DATAIO_PIN + 1  // Note - in pins must be a consecutive 'in' group
  
  #define PACKET_TYPE_READ  1
  #define PACKET_TYPE_WRITE 0

  #define ATOD_CHANNEL_NONE 0x00
  #define ATOD_CHANNEL_MODE 0x01
  #define ATOD_CHANNEL_X1 0x02
  #define ATOD_CHANNEL_Y1 0x03
  #define ATOD_CHANNEL_X2 0x04
  #define ATOD_CHANNEL_Y2 0x05

  // NUON Controller Probe Options
  #define DEFCFG 1
  #define VERSION 11
  #define TYPE 3
  #define MFG 0
  #define CRC16 0x8005
  #define MAGIC 0x4A554445 // HEX to ASCII == "JUDE" (The Polyface inventor)

  int crc_calc(unsigned char data,int crc);
  static int crc_lut[256]; // crc look up table
  uint32_t crc_data_packet(int32_t value, int8_t size);

  uint32_t __rev(uint32_t);
  uint8_t eparity(uint32_t);

  uint32_t output_buttons_0 = 0;
  uint32_t output_analog_1x = 0;
  uint32_t output_analog_1y = 0;
  uint32_t output_analog_2x = 0;
  uint32_t output_analog_2y = 0;
  uint32_t output_quad_x = 0;

  uint32_t device_mode = 0b10111001100000111001010100000000;
  uint32_t device_config = 0b10000000100000110000001100000000;
  uint32_t device_switch = 0b10000000100000110000001100000000;
  queue_t packet_queue;

#endif

bool update_pending;
uint8_t gc_rumble = 0;
uint8_t gc_kb_led = 0;
uint8_t gc_last_rumble = 0;
uint8_t gc_kb_counter = 0;

// CHEAT CODES :: is fun easter egg
#define CHEAT_LENGTH 10
#ifdef CONFIG_NUON
#define KONAMI_CODE {0x0200, 0x0200, 0x0800, 0x0800, 0x0400, 0x0100, 0x0400, 0x0100, 0x0008, 0x4000}
#else
#define KONAMI_CODE {0x01, 0x01, 0x04, 0x04, 0x08, 0x02, 0x08, 0x02, 0x20, 0x10}
#endif

uint32_t cheat_buffer[CHEAT_LENGTH] = {0};
uint32_t konami_code[CHEAT_LENGTH] = KONAMI_CODE;

void led_blinking_task(void);

extern void hid_app_init();
extern void hid_app_task(uint8_t rumble, uint8_t leds);

extern void neopixel_init(void);

extern void neopixel_task(int pat);

void xinput_task(void);

bool tuh_xinput_set_led(uint8_t dev_addr, uint8_t instance, uint8_t quadrant, bool block);
bool tuh_xinput_set_rumble(uint8_t dev_addr, uint8_t instance, uint8_t lValue, uint8_t rValue, bool block);

typedef struct TU_ATTR_PACKED
{
  int device_address;
  int instance_number;
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

Player_t players[MAX_PLAYERS];
int playersCount = 0;
bool is_fun = false;
unsigned char fun_inc = 0;
unsigned char fun_player = 1;
int last_player_count = 0;

// When PCE reads, set interlock to ensure atomic update
//
volatile bool  output_exclude = false;

// output_word -> is the word sent to the state machine for output
//
// Structure of the word sent to the FIFO from the ARM:
// |00000000|00ssbbbb|xxxxxxxx|yyyyyyyy
// Where:
//  - 0 = must be zero
//  - s = state (which nybble to output, 3/2/1/0)
//  - b = button values, arranged in Run/Sel/II/I sequence for PC Engine use
//  - x = mouse 'x' movement; left is {1 - 0x7F} ; right is {0xFF - 0x80 }
//  - y = mouse 'y' movement;  up  is {1 - 0x7F} ; down  is {0xFF - 0x80 }
//
uint32_t output_word_0 = 0;
uint32_t output_word_1 = 0;

int state = 0;          // countdown sequence for shift-register position

static absolute_time_t init_time;
static absolute_time_t current_time;
static absolute_time_t loop_time;
static const int64_t reset_period = 600;  // at 600us, reset the scan exclude flag

PIO pio;
uint sm1, sm2, sm3;   // sm1 = plex; sm2 = clock, sm3 = select

/*------------- MAIN -------------*/

// note that "__not_in_flash_func" functions are loaded
// and "pinned" in SRAM - not paged in/out from XIP flash
//

// Function to find a player in the array based on their device_address and instance_number.
int __not_in_flash_func(find_player_index)(int device_address, int instance_number) {
    for(int i = 0; i < playersCount; i++) {
        if(players[i].device_address == device_address && players[i].instance_number == instance_number) {
            return i;
        }
    }
    // If we reached here, the player was not found.
    return -1;
}

// An example function to add a player to the array.
static int __not_in_flash_func(add_player)(int device_address, int instance_number) {
    if(playersCount == MAX_PLAYERS) {
        return -1;
    }

    players[playersCount].device_address = device_address;
    players[playersCount].instance_number = instance_number;
    players[playersCount].player_number = playersCount + 1;

    players[playersCount].global_buttons = 0xFFFFF;
    players[playersCount].altern_buttons = 0xFFFFF;
    players[playersCount].global_x = 0;
    players[playersCount].global_y = 0;

    players[playersCount].output_buttons = 0xFFFFF;
    players[playersCount].output_analog_1x = 0;
    players[playersCount].output_analog_1y = 0;
    players[playersCount].button_mode = 0;
    players[playersCount].prev_buttons = 0xFFFFF;

    playersCount++;
    return playersCount-1; // returns player_index
}

// is_fun easter egg
void __not_in_flash_func(shift_buffer_and_insert)(uint32_t new_value) {
    // Shift all elements to the left by 1
    for (int i = 0; i < CHEAT_LENGTH - 1; i++) {
        cheat_buffer[i] = cheat_buffer[i + 1];
    }

    // Insert the new value at the end
    cheat_buffer[CHEAT_LENGTH - 1] = new_value;
}

void __not_in_flash_func(check_for_konami_code)(void)
{
  // DEBUG LOGGING
  // printf("Buffer content: ");
  // for (int i = 0; i < CHEAT_LENGTH; i++) {
  //     printf("%x ", cheat_buffer[i]);
  // }
  // printf("\n");

  for (int i = 0; i < CHEAT_LENGTH; i++) {
    if (cheat_buffer[i] != konami_code[i]) {
      return;
    }
  }
  printf("is_fun!\n");
  // The Konami Code has been entered
  is_fun = !is_fun;
}

#ifdef CONFIG_NGC
uint8_t gc_kb_key_lookup(uint8_t hid_key) {
    return hid_to_gc_key[hid_key];
}

uint8_t furthest_from_center(uint8_t a, uint8_t b, uint8_t center) {
    int distance_a = abs(a - center);
    int distance_b = abs(b - center);

    if (distance_a > distance_b) {
        return a;
    } else {
        return b;
    }
}
#elif CONFIG_NUON
uint8_t eparity(uint32_t data) {
  uint32_t eparity;
  eparity = (data>>16)^data;
  eparity ^= (eparity>>8);
  eparity ^= (eparity>>4);
  eparity ^= (eparity>>2);
  eparity ^= (eparity>>1);
  return ((eparity)&0x1);
}

// generates data response packet with crc check bytes
uint32_t crc_data_packet(int32_t value, int8_t size) {
  u_int32_t packet = 0;
  u_int16_t crc = 0;

  // calculate crc and place bytes into packet position
  for (int i=0; i<size; i++) {
    u_int8_t byte_val = (((value>>((size-i-1)*8)) & 0xff));
    crc = (crc_calc(byte_val, crc) & 0xffff);
    packet |= (byte_val << ((3-i)*8));
  }

  // place crc check bytes in packet position
  packet |= (crc << ((2-size)*8));

  return (packet);
}

int crc_build_lut() {
	int i,j,k;
	for (i=0; i<256; i++) {
		for(j=i<<8,k=0; k<8; k++) {
			j=(j&0x8000) ? (j<<1)^CRC16 : (j<<1); crc_lut[i]=j;
		}
	}
	return(0);
}

int crc_calc(unsigned char data, int crc) {
	if (crc_lut[1]==0) crc_build_lut();
	return(((crc_lut[((crc>>8)^data)&0xff])^(crc<<8))&0xffff);
}
#endif

//
// update_output - updates output_word with multi-tap plex data that
//                 is sent to PCE based on state and device types
//
void __not_in_flash_func(update_output)(void)
{

#ifdef CONFIG_PCE
  static uint32_t turbo_timer = 0;
  static bool turbo_state = false;
  int8_t bytes[5] = { 0 };
  int16_t hotkey = 0;

  // Increment the timer and check if it reaches the threshold
  turbo_timer++;
  if (turbo_timer >= timer_threshold) {
    turbo_timer = 0;
    turbo_state = !turbo_state;
  }

  unsigned short int i;
  for (i = 0; i < MAX_PLAYERS; ++i) {
    // base controller/mouse buttons
    int8_t byte = (players[i].output_buttons & 0xff);

    if (i >= playersCount && !hotkey) {
      bytes[i] = 0xff;
      continue;
    }

    // check for 6-button enable/disable hotkeys
    if (!(players[i].output_buttons & 0b0000000010000001))
      players[i].button_mode = BUTTON_MODE_6;
    else if (!(players[i].output_buttons & 0b0000000010000100))
      players[i].button_mode = BUTTON_MODE_2;
    else if (!(players[i].output_buttons & 0b0000000010000010))
      players[i].button_mode = BUTTON_MODE_3_SEL;
    else if (!(players[i].output_buttons & 0b0000000010001000))
      players[i].button_mode = BUTTON_MODE_3_RUN;

    // Turbo EverDrive Pro hot-key fix
    if (hotkey) byte &= hotkey;
    else if (i == 0) {
      int16_t btns= (~players[i].output_buttons & 0xff);
      if     (btns == 0x82) hotkey = ~0x82; // RUN + RIGHT
      else if(btns == 0x88) hotkey = ~0x88; // RUN + LEFT
      else if(btns == 0x84) hotkey = ~0x84; // RUN + DOWN
    }

    bool has6Btn = !(players[i].output_buttons & 0x0800);
    bool isMouse = !(players[i].output_buttons & 0x000f);
    bool is6btn = has6Btn && players[i].button_mode == BUTTON_MODE_6;
    bool is3btnSel = has6Btn && players[i].button_mode == BUTTON_MODE_3_SEL;
    bool is3btnRun = has6Btn && players[i].button_mode == BUTTON_MODE_3_RUN;

    // 6 button extra four buttons (III/IV/V/VI)
    if (is6btn) {
      if (state == 2) {
        byte = ((players[i].output_buttons>>8) & 0xf0);
      }
    }

    //
    else if (is3btnSel) {
      if ((~(players[i].output_buttons>>8)) & 0x30) {
        byte &= 0b01111111;
      }
    }

    //
    else if (is3btnRun) {
      if ((~(players[i].output_buttons>>8)) & 0x30) {
        byte &= 0b10111111;
      }
    }

    // Simulated Turbo buttons X/Y for II/I and L/R for speeds 1/2
    else {
      // Update the button state based on the turbo_state
      if (turbo_state) {
        // Set the button state as pressed
        if ((~(players[i].output_buttons>>8)) & 0x20) byte &= 0b11011111;
        if ((~(players[i].output_buttons>>8)) & 0x10) byte &= 0b11101111;
      } else {
        // Set the button state as released
      }

      if ((~(players[i].output_buttons>>8)) & 0x40) timer_threshold = timer_threshold_a;
      if ((~(players[i].output_buttons>>8)) & 0x80) timer_threshold = timer_threshold_b;
    }

    // mouse x/y states
    if (isMouse) {
      switch (state) {
        case 3: // state 3: x most significant nybble
          byte |= (((players[i].output_analog_1x>>1) & 0xf0) >> 4);
          break;
        case 2: // state 2: x least significant nybble
          byte |= (((players[i].output_analog_1x>>1) & 0x0f));
          break;
        case 1: // state 1: y most significant nybble
          byte |= (((players[i].output_analog_1y>>1) & 0xf0) >> 4);
          break;
        case 0: // state 0: y least significant nybble
          byte |= (((players[i].output_analog_1y>>1) & 0x0f));
          break;
      }
    }

    bytes[i] = byte;
  }

  output_word_0 = ((bytes[0] & 0xff))      | // player 1
                  ((bytes[1] & 0xff) << 8) | // player 2
                  ((bytes[2] & 0xff) << 16)| // player 3
                  ((bytes[3] & 0xff) << 24); // player 4
  output_word_1 = ((bytes[4] & 0xff));       // player 5
#elif CONFIG_NGC
  static bool kbModeButtonHeld = false;

  if (players[0].button_mode == BUTTON_MODE_KB) {
    gc_report = default_gc_kb_report;
  } else {
    gc_report = default_gc_report;
  }

  unsigned short int i;
  for (i = 0; i < playersCount; ++i) {
    // base controller buttons
    int16_t byte = (players[i].output_buttons & 0xffff);
    bool kbModeButtonPress = players[i].keypress[0] == HID_KEY_SCROLL_LOCK || players[i].keypress[0] == HID_KEY_F14;
    if (kbModeButtonPress) {
      if (!kbModeButtonHeld) {
          if (players[0].button_mode != BUTTON_MODE_KB) {
          players[0].button_mode = BUTTON_MODE_KB; // global
          players[i].button_mode = BUTTON_MODE_KB;
          GamecubeConsole_SetMode(&gc, GamecubeMode_KB);
          gc_report = default_gc_kb_report;
          // players[i].gc_report = default_gc_kb_report;
          gc_kb_led = 0x4;
        } else {
          players[0].button_mode = BUTTON_MODE_3; // global
          players[i].button_mode = BUTTON_MODE_3;
          GamecubeConsole_SetMode(&gc, GamecubeMode_3);
          gc_report = default_gc_report;
          // players[i].gc_report = default_gc_report;
          gc_kb_led = 0;
        }
      }
      kbModeButtonHeld = true;
    } else {
      kbModeButtonHeld = false;
    }

    if (players[0].button_mode != BUTTON_MODE_KB) {
      // global buttons
      gc_report.dpad_up    |= ((byte & 0x0001) == 0) ? 1 : 0; // up
      gc_report.dpad_right |= ((byte & 0x0002) == 0) ? 1 : 0; // right
      gc_report.dpad_down  |= ((byte & 0x0004) == 0) ? 1 : 0; // down
      gc_report.dpad_left  |= ((byte & 0x0008) == 0) ? 1 : 0; // left
      gc_report.a          |= ((byte & 0x0010) == 0) ? 1 : 0; // b
      gc_report.b          |= ((byte & 0x0020) == 0) ? 1 : 0; // a
      gc_report.z          |= ((byte & 0x0040) == 0) ? 1 : 0; // select
      gc_report.start      |= ((byte & 0x0080) == 0) ? 1 : 0; // start
      gc_report.x          |= ((byte & 0x01000) == 0) ? 1 : 0; // y
      gc_report.y          |= ((byte & 0x02000) == 0) ? 1 : 0; // x
      gc_report.l          |= ((byte & 0x04000) == 0) ? 1 : 0; // l
      gc_report.r          |= ((byte & 0x08000) == 0) ? 1 : 0; // r

      // global dominate axis
      gc_report.stick_x    = furthest_from_center(gc_report.stick_x, players[i].output_analog_1x, 128);
      gc_report.stick_y    = furthest_from_center(gc_report.stick_y, players[i].output_analog_1y, 128);
      gc_report.cstick_x   = furthest_from_center(gc_report.cstick_x, players[i].output_analog_2x, 128);
      gc_report.cstick_y   = furthest_from_center(gc_report.cstick_y, players[i].output_analog_2y, 128);
      gc_report.l_analog   = furthest_from_center(gc_report.l_analog, players[i].output_analog_l, 0);
      gc_report.r_analog   = furthest_from_center(gc_report.r_analog, players[i].output_analog_r, 0);
    } else {
      gc_report.keyboard.keypress[0] = gc_kb_key_lookup(players[i].keypress[2]);
      gc_report.keyboard.keypress[1] = gc_kb_key_lookup(players[i].keypress[1]);
      gc_report.keyboard.keypress[2] = gc_kb_key_lookup(players[i].keypress[0]);
      gc_report.keyboard.checksum = gc_report.keyboard.keypress[0] ^
                                    gc_report.keyboard.keypress[1] ^
                                    gc_report.keyboard.keypress[2] ^ gc_kb_counter;
      gc_report.keyboard.counter = gc_kb_counter;
    }

    // if (players[i].button_mode != BUTTON_MODE_KB) {
    //   // player buttons
    //   players[i].gc_report.dpad_up    = ((byte & 0x0001) == 0) ? 1 : 0; // up
    //   players[i].gc_report.dpad_right = ((byte & 0x0002) == 0) ? 1 : 0; // right
    //   players[i].gc_report.dpad_down  = ((byte & 0x0004) == 0) ? 1 : 0; // down
    //   players[i].gc_report.dpad_left  = ((byte & 0x0008) == 0) ? 1 : 0; // left
    //   players[i].gc_report.a          = ((byte & 0x0010) == 0) ? 1 : 0; // b
    //   players[i].gc_report.b          = ((byte & 0x0020) == 0) ? 1 : 0; // a
    //   players[i].gc_report.z          = ((byte & 0x0040) == 0) ? 1 : 0; // select
    //   players[i].gc_report.start      = ((byte & 0x0080) == 0) ? 1 : 0; // start
    //   players[i].gc_report.x          = ((byte & 0x01000) == 0) ? 1 : 0; // y
    //   players[i].gc_report.y          = ((byte & 0x02000) == 0) ? 1 : 0; // x
    //   players[i].gc_report.l          = ((byte & 0x04000) == 0) ? 1 : 0; // l
    //   players[i].gc_report.r          = ((byte & 0x08000) == 0) ? 1 : 0; // r

    //   // player axis
    //   players[i].gc_report.stick_x    = players[i].output_analog_1x;
    //   players[i].gc_report.stick_y    = players[i].output_analog_1y;
    //   players[i].gc_report.cstick_x   = players[i].output_analog_2x;
    //   players[i].gc_report.cstick_y   = players[i].output_analog_2y;
    //   players[i].gc_report.l_analog   = players[i].output_analog_l;
    //   players[i].gc_report.r_analog   = players[i].output_analog_r;
    // } else {
    //   // player keyboard
    //   players[i].gc_report.keyboard.keypress[0] = gc_kb_key_lookup(players[i].keypress[2]);
    //   players[i].gc_report.keyboard.keypress[1] = gc_kb_key_lookup(players[i].keypress[1]);
    //   players[i].gc_report.keyboard.keypress[2] = gc_kb_key_lookup(players[i].keypress[0]);
    //   players[i].gc_report.keyboard.checksum = players[i].gc_report.keyboard.keypress[0] ^
    //                                 players[i].gc_report.keyboard.keypress[1] ^
    //                                 players[i].gc_report.keyboard.keypress[2] ^ gc_kb_counter;
    //   players[i].gc_report.keyboard.counter = gc_kb_counter;
    // }
  }

#elif CONFIG_XB1
  unsigned short int i;
  for (i = 0; i < playersCount; ++i) {
    // base controller buttons
    int16_t byte = (players[i].output_buttons & 0xffff);
    i2c_slave_read_buffer[0] = 0xFA;
    i2c_slave_read_buffer[0] ^= ((byte & 0x02000) == 0) ? 0x02 : 0; // X
    i2c_slave_read_buffer[0] ^= ((byte & 0x01000) == 0) ? 0x08 : 0; // Y
    i2c_slave_read_buffer[0] ^= ((byte & 0x08000) == 0) ? 0x10 : 0; // R
    i2c_slave_read_buffer[0] ^= ((byte & 0x04000) == 0) ? 0x20 : 0; // L
    i2c_slave_read_buffer[0] ^= ((byte & 0x0080) == 0) ? 0x80 : 0; // MENU

    i2c_slave_read_buffer[1] = 0xFF;
    i2c_slave_read_buffer[1] ^= ((byte & 0x0001) == 0) ? 0x02 : 0; // UP
    i2c_slave_read_buffer[1] ^= ((byte & 0x0002) == 0) ? 0x04 : 0; // RIGHT
    i2c_slave_read_buffer[1] ^= ((byte & 0x0004) == 0) ? 0x10 : 0; // DOWN
    i2c_slave_read_buffer[1] ^= ((byte & 0x0008) == 0) ? 0x08 : 0; // LEFT
    i2c_slave_read_buffer[1] ^= ((byte & 0x0040) == 0) ? 0x20 : 0; // VIEW
    i2c_slave_read_buffer[1] ^= ((byte & 0x0020) == 0) ? 0x80 : 0; // A
  }
#elif CONFIG_NUON
  // Calculate and set Nuon output packet values here.
  int32_t buttons = (players[0].output_buttons & 0xffff) |
                    (players[0].output_buttons_alt & 0xffff);

  output_buttons_0 = crc_data_packet(buttons, 2);
  output_analog_1x = crc_data_packet(players[0].output_analog_1x, 1);
  output_analog_1y = crc_data_packet(players[0].output_analog_1y, 1);
  output_analog_2x = crc_data_packet(players[0].output_analog_2x, 1);
  output_analog_2y = crc_data_packet(players[0].output_analog_2y, 1);
  output_quad_x    = crc_data_packet(players[0].output_quad_x, 1);
#endif
  int32_t btns= (~players[0].output_buttons & 0xffff);
  int32_t prev_btns= (~players[0].prev_buttons & 0xffff);

  // Stash previous buttons to detect release
  if (!btns || btns != prev_btns) {
    players[0].prev_buttons = players[0].output_buttons;
  }

  // Check if the Konami Code has been entered
#ifdef CONFIG_NUON
  if (btns != 0xff7f && btns != prev_btns) {
    shift_buffer_and_insert(~btns & 0xff7f);
#else
  if ((btns & 0xff) && btns != prev_btns) {
    shift_buffer_and_insert(btns & 0xff);
#endif
    check_for_konami_code();
  }

  update_pending = true;
}

uint32_t map_nuon_buttons (uint32_t buttons) {
    uint32_t nuon_buttons = 0x0080;

    // Mapping the buttons from the old format to the new format, inverting the logic
    nuon_buttons |= (!(buttons & 0x00010)) ? 0x8000 : 0;  // Circle -> C-DOWN
    nuon_buttons |= (!(buttons & 0x00020)) ? 0x4000 : 0;  // Cross -> A
    nuon_buttons |= (!(buttons & 0x00080)) ? 0x2000 : 0;  // Option -> START
    nuon_buttons |= (!(buttons & 0x00040)) ? 0x1000 : 0;  // Share -> SELECT
    nuon_buttons |= (!(buttons & 0x00004)) ? 0x0800 : 0;  // Dpad Down -> D-DOWN
    nuon_buttons |= (!(buttons & 0x00008)) ? 0x0400 : 0;  // Dpad Left -> D-LEFT
    nuon_buttons |= (!(buttons & 0x00001)) ? 0x0200 : 0;  // Dpad Up -> D-UP
    nuon_buttons |= (!(buttons & 0x00002)) ? 0x0100 : 0;  // Dpad Right -> D-RIGHT
    // Skipping the two buttons represented by 0x0080 and 0x0040 in the new format
    nuon_buttons |= (!(buttons & 0x04000)) ? 0x0020 : 0;  // L1 -> L
    nuon_buttons |= (!(buttons & 0x08000)) ? 0x0010 : 0;  // R1 -> R
    nuon_buttons |= (!(buttons & 0x02000)) ? 0x0008 : 0;  // Square -> B
    nuon_buttons |= (!(buttons & 0x01000)) ? 0x0004 : 0;  // Triangle -> C-LEFT
    nuon_buttons |= (!(buttons & 0x00100)) ? 0x0002 : 0;  // L2 -> C-UP
    nuon_buttons |= (!(buttons & 0x00200)) ? 0x0001 : 0;  // R2 -> C-RIGHT

    return nuon_buttons;
}

//
// post_globals - accumulate the many intermediate mouse scans (~1ms)
//                into an accumulator which will be reported back to PCE
//
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
  uint8_t quad_x)
{
  bool has6Btn = !(buttons & 0x0800);

  // for merging extra device instances into the root instance (ex: joycon charging grip)
  bool is_extra = (instance == -1);
  if (is_extra) instance = 0;

  int player_index = find_player_index(dev_addr, instance);
  uint16_t buttons_pressed = (~(buttons | 0x0800)) || keys;
  if (player_index < 0 && buttons_pressed) {
    printf("[add player] [%d, %d]\n", dev_addr, instance);
    player_index = add_player(dev_addr, instance);
  }

  // printf("[player_index] [%d] [%d, %d]\n", player_index, dev_addr, instance);

  if (player_index >= 0) {
#ifdef CONFIG_PCE
      // map analog to dpad movement here
      uint8_t dpad_offset = 32;
      if (analog_1x) {
          if (analog_1x > 128 + dpad_offset) buttons &= ~(0x02); // right
          else if (analog_1x < 128 - dpad_offset) buttons &= ~(0x08); // left
      }
      if (analog_1y) {
          if (analog_1y > 128 + dpad_offset) buttons &= ~(0x01); // up
          else if (analog_1y < 128 - dpad_offset) buttons &= ~(0x04); // down
      }
#endif
      // extra instance buttons to merge with root player
      if (is_extra) {
        players[0].altern_buttons = buttons;
      } else {
        players[player_index].global_buttons = buttons;
      }

#ifdef CONFIG_PCE
      // TODO: 
      //  - Map home button to S1 + S2

      // TODO:
      //  - May need to output_exclude on 6-button?

      // if (!output_exclude || !isMouse)
      // {
        players[player_index].output_buttons = players[player_index].global_buttons & players[player_index].altern_buttons;

        // basic socd (up priority, left+right neutral)
        if (((~players[player_index].output_buttons) & 0x01) && ((~players[player_index].output_buttons) & 0x04)) {
          players[player_index].output_buttons ^= 0x04;
        }
        if (((~players[player_index].output_buttons) & 0x02) && ((~players[player_index].output_buttons) & 0x08)) {
          players[player_index].output_buttons ^= 0x0a;
        }

        update_output();
      // }
#elif CONFIG_NGC
      // cache analog and button values to player object
      if (analog_1x) players[player_index].output_analog_1x = analog_1x;
      if (analog_1y) players[player_index].output_analog_1y = analog_1y;
      if (analog_2x) players[player_index].output_analog_2x = analog_2x;
      if (analog_2y) players[player_index].output_analog_2y = analog_2y;
      players[player_index].output_analog_l = analog_l;
      players[player_index].output_analog_r = analog_r;
      players[player_index].output_buttons = players[player_index].global_buttons & players[player_index].altern_buttons;

      players[player_index].keypress[0] = (keys) & 0xff;
      players[player_index].keypress[1] = (keys >> 8) & 0xff;
      players[player_index].keypress[2] = (keys >> 16) & 0xff;

      // full analog and digital L/R press always happen together
      if (!((players[player_index].output_buttons) & 0x8000)) {
        players[player_index].output_analog_r = 255;
      }
      else if (analog_r > 250) {
        players[player_index].output_buttons &= ~0x8000;
      }

      if (!((players[player_index].output_buttons) & 0x4000)) {
        players[player_index].output_analog_l = 255;
      }
      else if (analog_l > 250) {
        players[player_index].output_buttons &= ~0x4000;
      }

      // printf("X1: %d, Y1: %d   ", analog_1x, analog_1y);

      update_output();
#elif CONFIG_XB1
      // maps View + Menu + Up button combo to Guide button
      if (!((players[player_index].global_buttons) & 0xC1)) {
        players[player_index].global_buttons ^= 0x400;
        players[player_index].global_buttons |= 0xC1;
      }

      // cache analog and button values to player object
      if (analog_1x) players[player_index].output_analog_1x = analog_1x;
      if (analog_1y) players[player_index].output_analog_1y = analog_1y;
      if (analog_2x) players[player_index].output_analog_2x = analog_2x;
      if (analog_2y) players[player_index].output_analog_2y = analog_2y;
      players[player_index].output_analog_l = analog_l;
      players[player_index].output_analog_r = analog_r;
      players[player_index].output_buttons = players[player_index].global_buttons & players[player_index].altern_buttons;

      update_output();
#elif CONFIG_NUON
      uint32_t nuon_buttons = map_nuon_buttons(buttons);
      if (!instance) {
        players[player_index].output_buttons = nuon_buttons;
      } else {
        players[player_index].output_buttons_alt = nuon_buttons;
      }

      if (analog_1x) players[player_index].output_analog_1x = analog_1x;
      if (analog_1y) players[player_index].output_analog_1y = analog_1y;
      if (analog_2x) players[player_index].output_analog_2x = analog_2x;
      if (analog_2y) players[player_index].output_analog_2y = analog_2y;
      if (quad_x) players[player_index].output_quad_x = quad_x;
      update_output();
#endif

  }
}

//
// post_mouse_globals - accumulate the many intermediate mouse scans (~1ms)
//                into an accumulator which will be reported back to PCE
//
void __not_in_flash_func(post_mouse_globals)(
  uint8_t dev_addr,
  int8_t instance,
  uint16_t buttons,
  uint8_t delta_x,
  uint8_t delta_y,
  uint8_t quad_x)
{
  // for merging extra device instances into the root instance (ex: joycon charging grip)
  bool is_extra = (instance == -1);
  if (is_extra) instance = 0;

  int player_index = find_player_index(dev_addr, instance);
  uint16_t buttons_pressed = (~(buttons | 0x0f00));
  if (player_index < 0 && buttons_pressed) {
    printf("[add player] [%d, %d]\n", dev_addr, instance);
    player_index = add_player(dev_addr, instance);
  }

  // printf("[player_index] [%d] [%d, %d]\n", player_index, dev_addr, instance);

  if (player_index >= 0) {
#ifdef CONFIG_PCE
      players[player_index].global_buttons = buttons;

      if (delta_x >= 128)
        players[player_index].global_x = players[player_index].global_x - (256-delta_x);
      else
        players[player_index].global_x = players[player_index].global_x + delta_x;

      if (delta_y >= 128)
        players[player_index].global_y = players[player_index].global_y - (256-delta_y);
      else
        players[player_index].global_y = players[player_index].global_y + delta_y;

      if (!output_exclude)
      {
        players[player_index].output_analog_1x = players[player_index].global_x;
        players[player_index].output_analog_1y = players[player_index].global_y;
        players[player_index].output_buttons = players[player_index].global_buttons & players[player_index].altern_buttons;

        update_output();
      }
#elif CONFIG_NUON
      players[player_index].global_buttons = buttons;
      players[player_index].output_buttons = map_nuon_buttons(players[player_index].global_buttons & players[player_index].altern_buttons);
      players[player_index].output_analog_1x = 128;
      players[player_index].output_analog_1y = 128;
      players[player_index].output_analog_2x = 128;
      players[player_index].output_analog_2y = 128;
      players[player_index].output_analog_l = 0;
      players[player_index].output_analog_r = 0;
      if (quad_x) players[player_index].output_quad_x = quad_x;

      update_output();
#else //NGC
      // fixes out of range analog values (1-255)
      if (delta_x == 0) delta_x = 1;
      if (delta_y == 0) delta_y = 1;

      if (delta_x >= 128)
        players[player_index].global_x = players[player_index].global_x - (256-delta_x);
      else
        players[player_index].global_x = players[player_index].global_x + delta_x;

      if (players[player_index].global_x > 127) {
        delta_x = 0xff;
      } else if (players[player_index].global_x < -127) {
        delta_x = 1;
      } else {
        delta_x = 128 + players[player_index].global_x;
      }

      if (delta_y >= 128)
        players[player_index].global_y = players[player_index].global_y - (256-delta_y);
      else
        players[player_index].global_y = players[player_index].global_y + delta_y;

      if (players[player_index].global_y > 127) {
        delta_y = 0xff;
      } else if (players[player_index].global_y < -127) {
        delta_y = 1;
      } else {
        delta_y = 128 + players[player_index].global_y;
      }

      // printf("X: %d, Y: %d   ", players[player_index].global_x, players[player_index].global_y);
      // printf("X1: %d, Y1: %d   ", delta_x, delta_y);

      // cache analog and button values to player object
      players[player_index].output_analog_1x = delta_x;
      players[player_index].output_analog_1y = delta_y;
      // players[player_index].output_analog_2x = delta_x;
      // players[player_index].output_analog_2y = delta_y;
      players[player_index].output_buttons = buttons;

      update_output();
#endif

  }
}

//
// process_signals - inner-loop processing of events:
//                   - USB polling
//                   - event processing
//                   - detection of when a PCE scan is no longer in process (reset period)
//
static void __not_in_flash_func(process_signals)(void)
{
  while (1)
  {
    // tinyusb host task
    tuh_task();
    // neopixel task
    neopixel_task(playersCount);
#ifndef ADAFRUIT_QTPY_RP2040
    led_blinking_task();
#endif

    // xinput rumble task
    xinput_task();

#ifdef CONFIG_PCE
//
// check time offset in order to detect when a PCE scan is no longer
// in process (so that fresh values can be sent to the state machine)
//
    current_time = get_absolute_time();

    if (absolute_time_diff_us(init_time, current_time) > reset_period) {
      state = 3;
      update_output();
      output_exclude = false;
      init_time = get_absolute_time();
    }
#endif

#if CFG_TUH_HID
    hid_app_task(gc_rumble, gc_kb_led);
#endif

  }
}

#ifdef CONFIG_XB1
void mcp4728_write_dac(i2c_inst_t *i2c, uint8_t address, uint8_t channel, uint16_t value) {
    uint8_t buf[3];
    buf[0] = (channel << 1) | 0x40; // Select channel and set Write DAC command
    buf[1] = (value >> 8) & 0x0F; // Set upper 4 bits of value
    buf[2] = value & 0xFF; // Set lower 8 bits of value

    i2c_write_blocking(i2c, address, buf, 3, false);
}

void mcp4728_set_config(i2c_inst_t *i2c, uint8_t address, uint8_t channel, uint8_t gain, uint8_t power_down) {
    uint8_t buf[3];
    buf[0] = (channel << 1) | 0x60; // Select channel and set Write DAC and EEPROM command
    buf[1] = (gain << 4) | (power_down << 1);
    buf[2] = 0; // Dummy value

    i2c_write_blocking(i2c, address, buf, 3, false);
}

// Function to set the power-down mode for a channel on MCP4728
// channel: 0 to 3 for the DAC channels
// pd_mode: 0 to 3 for different power down modes
//          0 = No power-down mode (Normal operation)
//          1 = Power-down mode with 1kΩ to ground
//          2 = Power-down mode with 100kΩ to ground
//          3 = Power-down mode with 500kΩ to ground
void mcp4728_power_down(i2c_inst_t *i2c, uint8_t address, uint8_t channel, uint8_t pd_mode) {
    uint8_t command[3];

    // Construct command to set the power-down mode for the channel
    // The PD bits are the least significant bits of the first command byte
    command[0] = (0x40 | (channel << 1)) | (pd_mode & 0x03); // Upper command byte with channel and PD mode
    command[1] = 0x00; // Lower data byte (Don't care for power-down mode)
    command[2] = 0x00; // Upper data byte (Don't care for power-down mode)

    // Send the command to the MCP4728
    i2c_write_blocking(i2c, address, command, 3, false);
}
#endif

//
// core1_entry - inner-loop for the second core
//             - when the "CLR" line is de-asserted, set lock flag
//               protecting the output state machine from inconsistent data
//
static void __not_in_flash_func(core1_entry)(void)
{
#ifdef CONFIG_PCE
  static bool rx_bit = 0;
#elif CONFIG_NUON
uint64_t packet = 0;
  uint16_t state = 0;
  uint8_t channel = 0;
  uint8_t id = 0;
  bool alive = false;
  bool tagged = false;
  bool branded = false;
  int requestsB = 0;
#endif

  while (1)
  {

#ifdef CONFIG_PCE
     // wait for (and sync with) negedge of CLR signal; rx_data is throwaway
     rx_bit = pio_sm_get_blocking(pio, sm2);

     // Now we are in an update-sequence; set a lock
     // to prevent update during output transaction
     output_exclude = true;

     // assume data is already formatted in output_word and push it to the state machine
     pio_sm_put(pio, sm1, output_word_1);
     pio_sm_put(pio, sm1, output_word_0);

     // Sequence from state 3 down through state 0 (show different nybbles to PCE)
     //
     // Note that when state = zero, it doesn't transition to a next state; the reset to
     // state 3 will happen as part of a timed process on the second CPU & state machine
     //

     // Also note that staying in 'scan' (CLK = low, SEL = high), is not expected
     // last more than about a half of a millisecond
     //
     loop_time = get_absolute_time();
     while ((gpio_get(CLKIN_PIN) == 0) && (gpio_get(DATAIN_PIN) == 1))
     {
        if (absolute_time_diff_us(loop_time, get_absolute_time()) > 550) {
           state = 0;
           break;
        }
     }

     if (state != 0)
     {
        state--;
        update_output();

        // renew countdown timeframe
        init_time = get_absolute_time();
     }
     else
     {
        update_output();

        unsigned short int i;
        for (i = 0; i < MAX_PLAYERS; ++i) {
          // decrement outputs from globals
          players[i].global_x = (players[i].global_x - players[i].output_analog_1x);
          players[i].global_y = (players[i].global_y - players[i].output_analog_1y);

          players[i].output_analog_1x = 0;
          players[i].output_analog_1y = 0;
          players[i].output_buttons = players[i].global_buttons & players[i].altern_buttons;
        }

        output_exclude = true;            // continue to lock the output values (which are now zero)
     }
#elif CONFIG_NGC
    // Wait for GameCube console to poll controller
    gc_rumble = GamecubeConsole_WaitForPoll(&gc) ? 255 : 0;

    // Send GameCube controller button report
    GamecubeConsole_SendReport(&gc, &gc_report);
    update_pending = false;

    gc_kb_counter++;
    gc_kb_counter &= 15;

    unsigned short int i;
    for (i = 0; i < MAX_PLAYERS; ++i) {
      // decrement outputs from globals
      if (players[i].global_x != 0) {
        players[i].global_x = (players[i].global_x - (players[i].output_analog_1x - 128));
        // if (players[i].global_x > 128) players[i].global_x = 128;
        // if (players[i].global_x < -128) players[i].global_x = -128;
        players[i].output_analog_1x = 128;
      }
      if (players[i].global_y != 0) {
        players[i].global_y = (players[i].global_y - (players[i].output_analog_1y - 128));
        // if (players[i].global_y > 128) players[i].global_y = 128;
        // if (players[i].global_y < -128) players[i].global_y = -128;
        players[i].output_analog_1y = 128;
      }
    }
    update_output();

    // printf("MODE: %d\n", gc._reading_mode);
#elif CONFIG_XB1
    // Analog outputs
    uint16_t x1Val = ((players[0].output_analog_1x * 2047)/255);
    uint16_t y1Val = ((players[0].output_analog_1y * 2047)/255);
             y1Val = (y1Val - 2047) * -1;
    uint16_t x2Val = ((players[0].output_analog_2x * 2047)/255);
    uint16_t y2Val = ((players[0].output_analog_2y * 2047)/255);
             y2Val = (y2Val - 2047) * -1;
    uint16_t lVal = ((players[0].output_analog_l * 2047)/255);
             lVal = (lVal - 2047) * -1;
    uint16_t rVal = ((players[0].output_analog_r * 2047)/255);
             rVal = (rVal - 2047) * -1;

    mcp4728_write_dac(I2C_DAC_PORT, MCP4728_I2C_ADDR0, 0, x1Val);
    mcp4728_write_dac(I2C_DAC_PORT, MCP4728_I2C_ADDR0, 1, y1Val);
    mcp4728_write_dac(I2C_DAC_PORT, MCP4728_I2C_ADDR0, 2, x2Val);
    mcp4728_write_dac(I2C_DAC_PORT, MCP4728_I2C_ADDR0, 3, y2Val);
    mcp4728_write_dac(I2C_DAC_PORT, MCP4728_I2C_ADDR1, 0, lVal);
    mcp4728_write_dac(I2C_DAC_PORT, MCP4728_I2C_ADDR1, 1, rVal);

    // Individual buttons
    gpio_put(XBOX_B_BTN_PIN, ((players[0].output_buttons & 0x0010) == 0) ? 0 : 1);
    gpio_put(XBOX_GUIDE_PIN, ((players[0].output_buttons & 0x0400) == 0) ? 0 : 1);
    gpio_put(XBOX_R3_BTN_PIN, ((players[0].output_buttons & 0x20000) == 0) ? 0 : 1);
    gpio_put(XBOX_L3_BTN_PIN, ((players[0].output_buttons & 0x10000) == 0) ? 0 : 1);

    update_pending = false;

    unsigned short int i;
    for (i = 0; i < MAX_PLAYERS; ++i) {
      // decrement outputs from globals
      if (players[i].global_x != 0) {
        players[i].global_x = (players[i].global_x - (players[i].output_analog_1x - 128));
        // if (players[i].global_x > 128) players[i].global_x = 128;
        // if (players[i].global_x < -128) players[i].global_x = -128;
        players[i].output_analog_1x = 128;
      }
      if (players[i].global_y != 0) {
        players[i].global_y = (players[i].global_y - (players[i].output_analog_1y - 128));
        // if (players[i].global_y > 128) players[i].global_y = 128;
        // if (players[i].global_y < -128) players[i].global_y = -128;
        players[i].output_analog_1y = 128;
      }
    }
    update_output();
#elif CONFIG_NUON
    packet = 0;
    for (int i = 0; i < 2; ++i) {
      uint32_t rxdata = pio_sm_get_blocking(pio, sm2);
      packet = ((packet) << 32) | (rxdata & 0xFFFFFFFF);
    }

    // queue_try_add(&packet_queue, &packet);

    uint8_t dataA = ((packet>>17) & 0b11111111);
    uint8_t dataS = ((packet>>9) & 0b01111111);
    uint8_t dataC = ((packet>>1) & 0b01111111);
    uint8_t type0 = ((packet>>25) & 0b00000001);
    if (dataA == 0xb1 && dataS == 0x00 && dataC == 0x00) { // RESET
      id = 0;
      alive = false;
      tagged = false;
      branded = false;
      state = 0;
      channel = 0;
    }
    if (dataA == 0x80) { // ALIVE
      uint32_t word0 = 1;
      uint32_t word1 = __rev(0b01);
      if (alive) word1 = __rev(((id & 0b01111111) << 1));
      else alive = true;

      pio_sm_put_blocking(pio1, sm1, word1);
      pio_sm_put_blocking(pio1, sm1, word0);
    }
    else if (dataA == 0x88 && dataS == 0x04 && dataC == 0x40) { // ERROR
      uint32_t word0 = 1;
      uint32_t word1 = 0;
      pio_sm_put_blocking(pio1, sm1, word1);
      pio_sm_put_blocking(pio1, sm1, word0);
    }
    else if (dataA == 0x90 && !branded) { // MAGIC
      uint32_t word0 = 1;
      uint32_t word1 = __rev(MAGIC);
      pio_sm_put_blocking(pio1, sm1, word1);
      pio_sm_put_blocking(pio1, sm1, word0);
    }
    else if (dataA == 0x94) { // PROBE
      uint32_t word0 = 1; // default res from HPI controller
      uint32_t word1 = __rev(0b10001011000000110000000000000000);

      //DEFCFG VERSION     TYPE      MFG TAGGED BRANDED    ID P
      //   0b1 0001011 00000011 00000000      0       0 00000 0
      word1 = ((DEFCFG  & 1)<<31) |
              ((VERSION & 0b01111111)<<24) |
              ((TYPE    & 0b11111111)<<16) |
              ((MFG     & 0b11111111)<<8) |
              (((tagged ? 1:0) & 1)<<7) |
              (((branded? 1:0) & 1)<<6) |
              ((id      & 0b00011111)<<1);
      word1 = __rev(word1 | eparity(word1));

      pio_sm_put_blocking(pio1, sm1, word1);
      pio_sm_put_blocking(pio1, sm1, word0);
    }
    else if (dataA == 0x27 && dataS == 0x01 && dataC == 0x00) { // REQUEST (ADDRESS)
      uint32_t word0 = 1;
      uint32_t word1 = 0;

      if (channel == ATOD_CHANNEL_MODE) {
        // word1 = __rev(0b11000100100000101001101100000000); // 68
        word1 = __rev(crc_data_packet(0b11110100, 1)); // send & recv?
      } else {
        // word1 = __rev(0b11000110000000101001010000000000); // 70
        word1 = __rev(crc_data_packet(0b11110110, 1)); // send & recv?
      }

      pio_sm_put_blocking(pio1, sm1, word1);
      pio_sm_put_blocking(pio1, sm1, word0);
    }
    else if (dataA == 0x84 && dataS == 0x04 && dataC == 0x40) { // REQUEST (B)
      uint32_t word0 = 1;
      uint32_t word1 = 0;

      // 
      if ((0b101001001100 >> requestsB) & 0b01) {
        word1 = __rev(0b10);
      }

      pio_sm_put_blocking(pio1, sm1, word1);
      pio_sm_put_blocking(pio1, sm1, word0);

      requestsB++;
      if (requestsB == 12) requestsB = 7;
    }
    else if (dataA == 0x34 && dataS == 0x01) { // CHANNEL
      channel = dataC;
    }
    else if (dataA == 0x32 && dataS == 0x02 && dataC == 0x00) { // QUADX
      uint32_t word0 = 1;
      uint32_t word1 = __rev(0b10000000100000110000001100000000); //0

      word1 = __rev(output_quad_x);
      // TODO: solve how to set unique values to first two bytes plus checksum

      pio_sm_put_blocking(pio1, sm1, word1);
      pio_sm_put_blocking(pio1, sm1, word0);
    }
    else if (dataA == 0x35 && dataS == 0x01 && dataC == 0x00) { // ANALOG
      uint32_t word0 = 1;
      uint32_t word1 = __rev(0b10000000100000110000001100000000); //0

      // ALL_BUTTONS: CTRLR_STDBUTTONS & CTRLR_DPAD & CTRLR_SHOULDER & CTRLR_EXTBUTTONS
      // <= 23 - 0x51f CTRLR_TWIST & CTRLR_THROTTLE & CTRLR_ANALOG1 & ALL_BUTTONS
      // 29-47 - 0x83f CTRLR_MOUSE & CTRLR_ANALOG1 & CTRLR_ANALOG2 & ALL_BUTTONS
      // 48-69 - 0x01f CTRLR_ANALOG1 & ALL_BUTTONS
      // 70-92 - 0x808 CTRLR_MOUSE & CTRLR_EXTBUTTONS
      // >= 93 - ERROR?
 
      if (channel == ATOD_CHANNEL_NONE) {
        word1 = __rev(device_mode); // device mode packet?
      }
      // if (channel == ATOD_CHANNEL_MODE) {
      //   word1 = __rev(0b10000000100000110000001100000000);
      // }
      if (channel == ATOD_CHANNEL_X1) {
        word1 = __rev(output_analog_1x);
      }
      else if (channel == ATOD_CHANNEL_Y1) {
        word1 = __rev(output_analog_1y);
      }
      else if (channel == ATOD_CHANNEL_X2) {
        word1 = __rev(output_analog_2x);
      }
      else if (channel == ATOD_CHANNEL_Y2) {
        word1 = __rev(output_analog_2y);
      }

      pio_sm_put_blocking(pio1, sm1, word1);
      pio_sm_put_blocking(pio1, sm1, word0);
    }
    else if (dataA == 0x25 && dataS == 0x01 && dataC == 0x00) { // CONFIG
      uint32_t word0 = 1;
      uint32_t word1 = __rev(device_config); // device config packet?

      pio_sm_put_blocking(pio1, sm1, word1);
      pio_sm_put_blocking(pio1, sm1, word0);
    }
    else if (dataA == 0x31 && dataS == 0x01 && dataC == 0x00) { // {SWITCH[16:9]}
      uint32_t word0 = 1;
      uint32_t word1 = __rev(device_switch); // extra device config?

      pio_sm_put_blocking(pio1, sm1, word1);
      pio_sm_put_blocking(pio1, sm1, word0);
    }
    else if (dataA == 0x30 && dataS == 0x02 && dataC == 0x00) { // {SWITCH[8:1]}
      uint32_t word0 = 1;
      uint32_t word1 = __rev(output_buttons_0);

      pio_sm_put_blocking(pio1, sm1, word1);
      pio_sm_put_blocking(pio1, sm1, word0);
    }
    else if (dataA == 0x99 && dataS == 0x01) { // STATE
      if (type0 == PACKET_TYPE_READ) {
        uint32_t word0 = 1;
        uint32_t word1 = __rev(0b11000000000000101000000000000000);

        if (((state >> 8) & 0xff) == 0x41 && (state & 0xff) == 0x51) {
          word1 = __rev(0b11010001000000101110011000000000);
        }
        pio_sm_put_blocking(pio1, sm1, word1);
        pio_sm_put_blocking(pio1, sm1, word0);
      } else { // type0 == PACKET_TYPE_WRITE
        state = ((state) << 8) | (dataC & 0xff);
      }
    }
    else if (dataA == 0xb4 && dataS == 0x00) { // BRAND
      id = dataC;
      branded = true;
    }
#endif
  }
}

#ifdef CONFIG_PCE

void turbo_init() {
    cpu_frequency = clock_get_hz(clk_sys);
    turbo_frequency = 1000000; // Default turbo frequency
    timer_threshold_a = cpu_frequency / (turbo_frequency * 2);
    timer_threshold_b = cpu_frequency / (turbo_frequency * 20);
    timer_threshold = timer_threshold_a;
}

void pce_init() {
  // use turbo button feature with PCE
  turbo_init();

  // Load the plex (multiplex output) program, and configure a free state machine
  // to run the program.

  uint offset1 = pio_add_program(pio, &plex_program);
  sm1 = pio_claim_unused_sm(pio, true);
  plex_program_init(pio, sm1, offset1, DATAIN_PIN, CLKIN_PIN, OUTD0_PIN);

  // Load the clock/select (synchronizing input) programs, and configure a free state machines
  // to run the programs.

  uint offset2 = pio_add_program(pio, &clock_program);
  sm2 = pio_claim_unused_sm(pio, true);
  clock_program_init(pio, sm2, offset2, CLKIN_PIN, OUTD0_PIN);

  uint offset3 = pio_add_program(pio, &select_program);
  sm3 = pio_claim_unused_sm(pio, true);
  select_program_init(pio, sm3, offset3, DATAIN_PIN);
}
#endif

#ifdef CONFIG_NGC

void gc_kb_key_lookup_init() {
  // init hid key to gc key lookup table
  hid_to_gc_key[HID_KEY_A] = GC_KEY_A;
  hid_to_gc_key[HID_KEY_B] = GC_KEY_B;
  hid_to_gc_key[HID_KEY_C] = GC_KEY_C;
  hid_to_gc_key[HID_KEY_D] = GC_KEY_D;
  hid_to_gc_key[HID_KEY_E] = GC_KEY_E;
  hid_to_gc_key[HID_KEY_F] = GC_KEY_F;
  hid_to_gc_key[HID_KEY_G] = GC_KEY_G;
  hid_to_gc_key[HID_KEY_H] = GC_KEY_H;
  hid_to_gc_key[HID_KEY_I] = GC_KEY_I;
  hid_to_gc_key[HID_KEY_J] = GC_KEY_J;
  hid_to_gc_key[HID_KEY_K] = GC_KEY_K;
  hid_to_gc_key[HID_KEY_L] = GC_KEY_L;
  hid_to_gc_key[HID_KEY_M] = GC_KEY_M;
  hid_to_gc_key[HID_KEY_N] = GC_KEY_N;
  hid_to_gc_key[HID_KEY_O] = GC_KEY_O;
  hid_to_gc_key[HID_KEY_P] = GC_KEY_P;
  hid_to_gc_key[HID_KEY_Q] = GC_KEY_Q;
  hid_to_gc_key[HID_KEY_R] = GC_KEY_R;
  hid_to_gc_key[HID_KEY_S] = GC_KEY_S;
  hid_to_gc_key[HID_KEY_T] = GC_KEY_T;
  hid_to_gc_key[HID_KEY_U] = GC_KEY_U;
  hid_to_gc_key[HID_KEY_V] = GC_KEY_V;
  hid_to_gc_key[HID_KEY_W] = GC_KEY_W;
  hid_to_gc_key[HID_KEY_X] = GC_KEY_X;
  hid_to_gc_key[HID_KEY_Y] = GC_KEY_Y;
  hid_to_gc_key[HID_KEY_Z] = GC_KEY_Z;
  hid_to_gc_key[HID_KEY_1] = GC_KEY_1;
  hid_to_gc_key[HID_KEY_2] = GC_KEY_2;
  hid_to_gc_key[HID_KEY_3] = GC_KEY_3;
  hid_to_gc_key[HID_KEY_4] = GC_KEY_4;
  hid_to_gc_key[HID_KEY_5] = GC_KEY_5;
  hid_to_gc_key[HID_KEY_6] = GC_KEY_6;
  hid_to_gc_key[HID_KEY_7] = GC_KEY_7;
  hid_to_gc_key[HID_KEY_8] = GC_KEY_8;
  hid_to_gc_key[HID_KEY_9] = GC_KEY_9;
  hid_to_gc_key[HID_KEY_0] = GC_KEY_0;
  hid_to_gc_key[HID_KEY_MINUS] = GC_KEY_MINUS;
  hid_to_gc_key[HID_KEY_EQUAL] = GC_KEY_CARET;
  hid_to_gc_key[HID_KEY_GRAVE] = GC_KEY_YEN; // HID_KEY_KANJI3
  hid_to_gc_key[HID_KEY_PRINT_SCREEN] = GC_KEY_AT; // hankaku/zenkaku HID_KEY_LANG5
  hid_to_gc_key[HID_KEY_BRACKET_LEFT] = GC_KEY_LEFTBRACKET;
  hid_to_gc_key[HID_KEY_SEMICOLON] = GC_KEY_SEMICOLON;
  hid_to_gc_key[HID_KEY_APOSTROPHE] = GC_KEY_COLON;
  hid_to_gc_key[HID_KEY_BRACKET_RIGHT] = GC_KEY_RIGHTBRACKET;
  hid_to_gc_key[HID_KEY_COMMA] = GC_KEY_COMMA;
  hid_to_gc_key[HID_KEY_PERIOD] = GC_KEY_PERIOD;
  hid_to_gc_key[HID_KEY_SLASH] = GC_KEY_SLASH;
  hid_to_gc_key[HID_KEY_BACKSLASH] = GC_KEY_BACKSLASH;
  hid_to_gc_key[HID_KEY_F1] = GC_KEY_F1;
  hid_to_gc_key[HID_KEY_F2] = GC_KEY_F2;
  hid_to_gc_key[HID_KEY_F3] = GC_KEY_F3;
  hid_to_gc_key[HID_KEY_F4] = GC_KEY_F4;
  hid_to_gc_key[HID_KEY_F5] = GC_KEY_F5;
  hid_to_gc_key[HID_KEY_F6] = GC_KEY_F6;
  hid_to_gc_key[HID_KEY_F7] = GC_KEY_F7;
  hid_to_gc_key[HID_KEY_F8] = GC_KEY_F8;
  hid_to_gc_key[HID_KEY_F9] = GC_KEY_F9;
  hid_to_gc_key[HID_KEY_F10] = GC_KEY_F10;
  hid_to_gc_key[HID_KEY_F11] = GC_KEY_F11;
  hid_to_gc_key[HID_KEY_F12] = GC_KEY_F12;
  hid_to_gc_key[HID_KEY_ESCAPE] = GC_KEY_ESC;
  hid_to_gc_key[HID_KEY_INSERT] = GC_KEY_INSERT;
  hid_to_gc_key[HID_KEY_DELETE] = GC_KEY_DELETE;
  hid_to_gc_key[HID_KEY_GRAVE] = GC_KEY_GRAVE;
  hid_to_gc_key[HID_KEY_BACKSPACE] = GC_KEY_BACKSPACE;
  hid_to_gc_key[HID_KEY_TAB] = GC_KEY_TAB;
  hid_to_gc_key[HID_KEY_CAPS_LOCK] = GC_KEY_CAPSLOCK;
  hid_to_gc_key[HID_KEY_SHIFT_LEFT] = GC_KEY_LEFTSHIFT;
  hid_to_gc_key[HID_KEY_SHIFT_RIGHT] = GC_KEY_RIGHTSHIFT;
  hid_to_gc_key[HID_KEY_CONTROL_LEFT] = GC_KEY_LEFTCTRL;
  hid_to_gc_key[HID_KEY_ALT_LEFT] = GC_KEY_LEFTALT;
  hid_to_gc_key[HID_KEY_GUI_LEFT] = GC_KEY_LEFTUNK1; // muhenkan HID_KEY_KANJI5
  hid_to_gc_key[HID_KEY_SPACE] = GC_KEY_SPACE;
  hid_to_gc_key[HID_KEY_GUI_RIGHT] = GC_KEY_RIGHTUNK1; // henkan/zenkouho HID_KEY_KANJI4
  hid_to_gc_key[HID_KEY_APPLICATION] = GC_KEY_RIGHTUNK2; // hiragana/katakana HID_KEY_LANG4
  hid_to_gc_key[HID_KEY_ARROW_LEFT] = GC_KEY_LEFT;
  hid_to_gc_key[HID_KEY_ARROW_DOWN] = GC_KEY_DOWN;
  hid_to_gc_key[HID_KEY_ARROW_UP] = GC_KEY_UP;
  hid_to_gc_key[HID_KEY_ARROW_RIGHT] = GC_KEY_RIGHT;
  hid_to_gc_key[HID_KEY_ENTER] = GC_KEY_ENTER;
  hid_to_gc_key[HID_KEY_HOME] = GC_KEY_HOME; // fn + up
  hid_to_gc_key[HID_KEY_END] = GC_KEY_END; // fn + right
  hid_to_gc_key[HID_KEY_PAGE_DOWN] = GC_KEY_PAGEDOWN; // fn + left
  hid_to_gc_key[HID_KEY_PAGE_UP] = GC_KEY_PAGEUP; // fn + down
  // hid_to_gc_key[HID_KEY_SCROLL_LOCK] = GC_KEY_SCROLLLOCK; // fn + insert
}

void ngc_init() {
  // over clock CPU for correct timing with GC
  set_sys_clock_khz(130000, true);

  // corrects UART serial output after overclock
  stdio_init_all();

  // Ground gpio attatched to sheilding
  gpio_init(SHIELD_PIN_L);
  gpio_set_dir(SHIELD_PIN_L, GPIO_OUT);
  gpio_init(SHIELD_PIN_L+1);
  gpio_set_dir(SHIELD_PIN_L+1, GPIO_OUT);
  gpio_init(SHIELD_PIN_R);
  gpio_set_dir(SHIELD_PIN_R, GPIO_OUT);
  gpio_init(SHIELD_PIN_R+1);
  gpio_set_dir(SHIELD_PIN_R+1, GPIO_OUT);

  gpio_put(SHIELD_PIN_L, 0);
  gpio_put(SHIELD_PIN_L+1, 0);
  gpio_put(SHIELD_PIN_R, 0);
  gpio_put(SHIELD_PIN_R+1, 0);

  // Initialize the BOOTSEL_PIN as input
  gpio_init(BOOTSEL_PIN);
  gpio_set_dir(BOOTSEL_PIN, GPIO_IN);
  gpio_pull_up(BOOTSEL_PIN);

  // Reboot into bootsel mode if GC 3.3V not detected.
  gpio_init(GC_3V3_PIN);
  gpio_set_dir(GC_3V3_PIN, GPIO_IN);
  gpio_pull_down(GC_3V3_PIN);

  sleep_ms(200);
  if (!gpio_get(GC_3V3_PIN)) reset_usb_boot(0, 0);

  int sm = -1;
  int offset = -1;
  gc_kb_key_lookup_init();
  GamecubeConsole_init(&gc, GC_DATA_PIN, pio, sm, offset);
  gc_report = default_gc_report;
}

#endif

#ifdef CONFIG_XB1

// I2C interrupt handlers
static void i2c_slave_handler(i2c_inst_t *i2c, i2c_slave_event_t event) {
    int data;
    size_t bytes_available = 0;
    
    // Handle I2C events
    switch (event) {
        case I2C_SLAVE_RECEIVE:
            // Read data from master
            // printf("[RECEIVE]::");
            
            // determine how many bytes have been received
            bytes_available = i2c_get_read_available(i2c);
            if (bytes_available > 0) {
                // printf("bytes_available:%d data:", bytes_available);

                // read the bytes from the RX FIFO
                i2c_read_raw_blocking(i2c, i2c_slave_write_buffer, bytes_available);
                // process the received bytes as needed
                // for (int i = 0; i < bytes_available; ++i) {
                //   printf(" 0x%x", i2c_slave_write_buffer[i]);
                // }
            }
            break;

        case I2C_SLAVE_REQUEST:
            // Write data to master
            // printf("[REQUEST]:: 0x%x 0x%x", i2c_slave_read_buffer[0], i2c_slave_read_buffer[1]);

            i2c_write_raw_blocking(i2c, i2c_slave_read_buffer, sizeof(i2c_slave_read_buffer));
            break;

        default:
            // printf("[UNHANDLED]");
            break;
    }
    // printf("\n");
}

void xb1_init() {
  sleep_ms(1000);

  // corrects UART serial output after overclock
  stdio_init_all();

  gpio_init(XBOX_B_BTN_PIN);
  // gpio_disable_pulls(XBOX_B_BTN_PIN);
  gpio_set_dir(XBOX_B_BTN_PIN, GPIO_OUT);

  gpio_init(XBOX_GUIDE_PIN);
  gpio_set_dir(XBOX_GUIDE_PIN, GPIO_OUT);

  gpio_init(XBOX_R3_BTN_PIN);
  gpio_set_dir(XBOX_R3_BTN_PIN, GPIO_OUT);

  gpio_init(XBOX_L3_BTN_PIN);
  gpio_set_dir(XBOX_L3_BTN_PIN, GPIO_OUT);

  gpio_put(XBOX_B_BTN_PIN, 1);
  gpio_put(XBOX_GUIDE_PIN, 1);
  gpio_put(XBOX_R3_BTN_PIN, 1);
  gpio_put(XBOX_L3_BTN_PIN, 1);

#ifdef ADAFRUIT_QTPY_RP2040
  gpio_init(NEOPIXEL_POWER_PIN);
  gpio_set_dir(NEOPIXEL_POWER_PIN, GPIO_OUT);
  gpio_put(NEOPIXEL_POWER_PIN, 1);
#endif

  gpio_init(I2C_SLAVE_SDA_PIN);
  gpio_set_function(I2C_SLAVE_SDA_PIN, GPIO_FUNC_I2C);
  gpio_pull_up(I2C_SLAVE_SDA_PIN);

  gpio_init(I2C_SLAVE_SCL_PIN);
  gpio_set_function(I2C_SLAVE_SCL_PIN, GPIO_FUNC_I2C);
  gpio_pull_up(I2C_SLAVE_SCL_PIN);

  // Initialize Slave I2C for simulated XB1Slim GPIO expander
  i2c_init(I2C_SLAVE_PORT, 400 * 1000);
  i2c_slave_init(I2C_SLAVE_PORT, I2C_SLAVE_ADDRESS, &i2c_slave_handler);

  // Initialize DAC I2C for simulated analog sticks/triggers
  i2c_init(I2C_DAC_PORT, 400 * 1000);
  gpio_set_function(I2C_DAC_SDA_PIN, GPIO_FUNC_I2C);
  gpio_set_function(I2C_DAC_SCL_PIN, GPIO_FUNC_I2C);
  gpio_pull_up(I2C_DAC_SDA_PIN);
  gpio_pull_up(I2C_DAC_SCL_PIN);

  mcp4728_set_config(I2C_DAC_PORT, MCP4728_I2C_ADDR0, 0, 0, 0); // TP64 - LSX
  mcp4728_set_config(I2C_DAC_PORT, MCP4728_I2C_ADDR0, 1, 0, 0); // TP63 - LSY
  mcp4728_set_config(I2C_DAC_PORT, MCP4728_I2C_ADDR0, 2, 0, 0); // TP66 - RSX
  mcp4728_set_config(I2C_DAC_PORT, MCP4728_I2C_ADDR0, 3, 0, 0); // TP65 - RSY
  mcp4728_set_config(I2C_DAC_PORT, MCP4728_I2C_ADDR1, 0, 0, 0); // TP68 - LT
  mcp4728_set_config(I2C_DAC_PORT, MCP4728_I2C_ADDR1, 1, 0, 0); // TP67 - RT
}

#endif

#ifdef CONFIG_NUON

void nuon_init() {
  // init for nuon communication
  output_buttons_0 = 0b00000000100000001000001100000011; // no buttons pressed
  output_analog_1x = 0b10000000100000110000001100000000; // x1 = 0
  output_analog_1y = 0b10000000100000110000001100000000; // y1 = 0
  output_analog_2x = 0b10000000100000110000001100000000; // x2 = 0
  output_analog_2y = 0b10000000100000110000001100000000; // y2 = 0
  output_quad_x = 0b10000000000000000000000000000000; // quadx = 0

  // PROPERTIES DEV____MOD DEV___CONF DEV____EXT // CTRL_VALUES from SDK joystick.h
  // 0x0000001f 0b10111001 0b10000000 0b10000000 // ANALOG1, STDBUTTONS, DPAD, SHOULDER, EXTBUTTONS
  // 0x0000003f 0b10000000 0b01000000 0b01000000 // ANALOG1, ANALOG2, STDBUTTONS, DPAD, SHOULDER, EXTBUTTONS
  // 0x0000011d 0b11000000 0b00000000 0b10000000 // THROTTLE, ANALOG1, STDBUTTONS, SHOULDER, EXTBUTTONS
  // 0x0000011f 0b11000000 0b01000000 0b00010000 // THROTTLE, ANALOG1, STDBUTTONS, DPAD, SHOULDER, EXTBUTTONS
  // 0x0000014f 0b11010000 0b00000000 0b00000000 // THROTTLE, WHEEL|PADDLE, STDBUTTONS, DPAD, SHOULDER, EXTBUTTONS
  // 0x00000300 0b11000000 0b00000000 0b11000000 // BRAKE, THROTTLE
  // 0x00000341 0b11000000 0b00000000 0b00000000 // BRAKE, THROTTLE, WHEEL|PADDLE, STDBUTTONS
  // 0x0000034f 0b10111001 0b10000000 0b00000000 // BRAKE, THROTTLE, WHEEL|PADDLE, STDBUTTONS, DPAD, SHOULDER, EXTBUTTONS
  // 0x0000041d 0b11000000 0b11000000 0b00000000 // RUDDER|TWIST, ANALOG1, STDBUTTONS, DPAD, EXTBUTTONS
  // 0x00000513 0b10000000 0b00000000 0b00000000 // RUDDER|TWIST, THROTTLE, ANALOG1, DPAD, STDBUTTONS
  // 0x0000051f 0b10000000 0b10000000 0b10000000 // RUDDER|TWIST, THROTTLE, ANALOG1, STDBUTTONS, DPAD, SHOULDER, EXTBUTTONS
  // 0x00000800 0b11010000 0b00000000 0b10000000 // MOUSE|TRACKBALL
  // 0x00000808 0b11010000 0b10000000 0b10000000 // MOUSE|TRACKBALL, EXTBUTTONS
  // 0x00000811 0b11001000 0b00010000 0b00010000 // MOUSE|TRACKBALL, ANALOG1, STDBUTTONS
  // 0x00000815 0b11001000 0b11000000 0b00010000 // MOUSE|TRACKBALL, ANALOG1, STDBUTTONS, SHOULDER
  // 0x0000083f 0b10011101 0b10000000 0b10000000 // MOUSE|TRACKBALL, ANALOG1, ANALOG2, STDBUTTONS, DPAD, SHOULDER, EXTBUTTONS
  // 0x0000103f 0b10011101 0b11000000 0b11000000 // QUADSPINNER1, ANALOG1, ANALOG2, STDBUTTONS, DPAD, SHOULDER, EXTBUTTONS
  // 0x0000101f 0b10111001 0b10000000 0b01000000 // QUADSPINNER1, ANALOG1, STDBUTTONS, DPAD, SHOULDER, EXTBUTTONS
  // 0x00001301 0b11000000 0b11000000 0b11000000 // QUADSPINNER1, BRAKE, THROTTLE, STDBUTTONS
  // 0x0000401d 0b11010000 0b01000000 0b00010000 // THUMBWHEEL1, ANALOG1, STDBUTTONS, SHOULDER, EXTBUTTONS
  // 0x0000451b 0b10011101 0b00000000 0b00000000 // THUMBWHEEL1, RUDDER|TWIST, THROTTLE, STDBUTTONS, DPAD, EXTBUTTONS
  // 0x0000c011 0b10111001 0b11000000 0b01000000 // THUMBWHEEL1, THUMBWHEEL2, ANALOG1, STDBUTTONS
  // 0x0000c01f 0b11000000 0b00000000 0b01000000 // THUMBWHEEL1, THUMBWHEEL2, ANALOG1, STDBUTTONS, DPAD, SHOULDER, EXTBUTTONS
  // 0x0000c03f 0b10011101 0b01000000 0b01000000 // THUMBWHEEL1, THUMBWHEEL2, ANALOG1, ANALOG2, STDBUTTONS, DPAD, SHOULDER, EXTBUTTONS
  // 0x0000c51b 0b10000000 0b11000000 0b11000000 // THUMBWHEEL1, THUMBWHEEL2, RUDDER|TWIST, THROTTLE, ANALOG1, STDBUTTONS, DPAD, EXTBUTTONS
  // 0x0001001d 0b11000000 0b11000000 0b10000000 // FISHINGREEL, ANALOG1, STDBUTTONS, SHOULDER, EXTBUTTONS

  // Sets packets that define device properties
  device_mode   = crc_data_packet(0b10011101, 1);
  device_config = crc_data_packet(0b11000000, 1);
  device_switch = crc_data_packet(0b11000000, 1);

  // Load the clock/select (synchronizing input) programs, and configure a free state machines
  // to run the programs.

  uint offset2 = pio_add_program(pio, &polyface_read_program);
  sm2 = pio_claim_unused_sm(pio, true);
  polyface_read_program_init(pio, sm2, offset2, DATAIO_PIN);

  // Load the plex (multiplex output) program, and configure a free state machine
  // to run the program.

  uint offset1 = pio_add_program(pio1, &polyface_send_program);
  sm1 = pio_claim_unused_sm(pio1, true);
  polyface_send_program_init(pio1, sm1, offset1, DATAIO_PIN);

  queue_init(&packet_queue, sizeof(int64_t), 1000);
}

#endif

int main(void)
{
  board_init();

  printf("\nUSB_RETRO::");
#ifdef CONFIG_PCE
  printf("PCENGINE");
#elif CONFIG_NGC
  printf("GAMECUBE");
#elif CONFIG_XB1
  printf("XBOXONE");
#elif CONFIG_NUON
  printf("NUON");
#endif
  printf("\n\n");

  // Pause briefly for stability before starting activity
  sleep_ms(250);

  hid_app_init();

  tusb_init();

  neopixel_init();

  unsigned short int i;
  for (i = 0; i < MAX_PLAYERS; ++i) {
    players[i].global_buttons = 0xFFFFF;
    players[i].altern_buttons = 0xFFFFF;
    players[i].global_x = 0;
    players[i].global_y = 0;
    players[i].output_buttons = 0xFFFFF;
    players[i].output_analog_1x = 128;
    players[i].output_analog_1y = 128;
    players[i].output_analog_2x = 128;
    players[i].output_analog_2y = 128;
    players[i].output_analog_l = 0;
    players[i].output_analog_r = 0;
    players[i].prev_buttons = 0xFFFFF;
    players[i].button_mode = 0;
#ifdef CONFIG_NGC
    players[i].gc_report = default_gc_report;
#elif CONFIG_NUON
    players[i].global_buttons = 0x80;
    players[i].altern_buttons = 0x80;
    players[i].output_buttons = 0x80;
    players[i].output_buttons_alt = 0x80;
    players[i].output_quad_x = 0;
#endif
  }
  state = 3;

  output_word_0 = 0x00FFFFFFFF;  // no buttons pushed
  output_word_1 = 0x00000000FF;  // no buttons pushed

  init_time = get_absolute_time();

  // Both state machines can run on the same PIO processor
  pio = pio0;

#ifdef CONFIG_PCE
  pce_init();
#elif CONFIG_NGC
  ngc_init();
#elif CONFIG_XB1
  xb1_init();
#elif CONFIG_NUON
  nuon_init();
#endif

  multicore_launch_core1(core1_entry);

  process_signals();

  return 0;
}

//--------------------------------------------------------------------+
// Player Managment Functions
//--------------------------------------------------------------------+

// Function to remove all players with a certain device_address and shift the remaining players
void remove_players_by_address(int device_address, int instance) { // -1 instance removes all instances within device_address
    int i = 0;
    while(i < playersCount) {
        if((players[i].device_address == device_address && instance == -1) ||
           (players[i].device_address == device_address && players[i].instance_number == instance))
        {
            // Shift all the players after this one up in the array
            for(int j = i; j < playersCount - 1; j++) {
                players[j] = players[j+1];
            }
            // Decrement playersCount because a player was removed
            playersCount--;
        } else {
            i++;
        }
    }

    // Update the player numbers
    for(i = 0; i < playersCount; i++) {
        players[i].player_number = i + 1;
    }
}

//--------------------------------------------------------------------+
// TinyUSB Callbacks
//--------------------------------------------------------------------+
#if CFG_TUH_HID

void tuh_mount_cb(uint8_t dev_addr)
{
  // application set-up
  printf("A device with address %d is mounted\r\n", dev_addr);

}

void tuh_umount_cb(uint8_t dev_addr)
{
  // application tear-down
  printf("A device with address %d is unmounted \r\n", dev_addr);

  // if ((--playersCount) < 0) playersCount = 0;
  remove_players_by_address(dev_addr, -1);

  is_fun = false;
}

#endif

void xinput_task() {
  // rumble only if controller connected
  if (!playersCount) return;

  // rumble state update only on diff than last
  if (gc_last_rumble == gc_rumble && last_player_count == playersCount) return;
  gc_last_rumble = gc_rumble;
  last_player_count = playersCount;

  // update rumble state for xinput device 1.
  unsigned short int i;
  for (i = 0; i < playersCount; ++i) {
    // TODO: only fire this if device is xinput
    // if (players[i].xinput) {
      tuh_xinput_set_led(players[i].device_address, players[i].instance_number, i+1, true);
      tuh_xinput_set_rumble(players[i].device_address, players[i].instance_number, gc_rumble, gc_rumble, true);
    // } else {
    //   hid_set_rumble(players[i].device_address, players[i].instance_number, gc_rumble, gc_rumble);
    // }
  }
}

//--------------------------------------------------------------------+
// Blinking Task
//--------------------------------------------------------------------+
void led_blinking_task(void)
{
  const uint32_t interval_ms = 1000;
  static uint32_t start_ms = 0;

  static bool led_state = false;

  // Blink every interval ms
  if ( board_millis() - start_ms < interval_ms) return; // not enough time
  start_ms += interval_ms;

  board_led_write(led_state);
  led_state = 1 - led_state; // toggle
}
