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

#include "bsp/board.h"
#include "tusb.h"
#include "pico.h"

#include "pico/stdlib.h"
#include "pico/time.h"
#include "pico/multicore.h"
#include "pico/util/queue.h"
#include "hardware/pio.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "hardware/structs/ioqspi.h"
#include "hardware/structs/sio.h"
#include "polyface_read.pio.h"
#include "polyface_send.pio.h"


//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+


#ifdef ADAFRUIT_KB2040          // if build for Adafruit KB2040 board

#define DATAIO_PIN      2
#define CLKIN_PIN       DATAIO_PIN + 1  // Note - in pins must be a consecutive 'in' group

#else
#ifdef ADAFRUIT_QTPY_RP2040      // if build for QtPy RP2040 board

#define DATAIO_PIN      24
#define CLKIN_PIN       DATAIO_PIN + 1  // Note - in pins must be a consecutive 'in' group

#else
#ifdef SEEED_XIAO_RP2040         // else assignments for Seed XIAO RP2040 board - note: needs specific board

#define DATAIO_PIN      24
#define CLKIN_PIN       DATAIO_PIN + 1  // Note - in pins must be a consecutive 'in' group

#else                           // else assume build for RP Pico board

#define DATAIO_PIN      16
#define CLKIN_PIN       DATAIO_PIN + 1  // Note - in pins must be a consecutive 'in' group

#endif
#endif
#endif

#define PACKET_TYPE_READ 1
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

int crc_calc(unsigned char data,int crc);
static int crc_lut[256]; // crc look up table

queue_t packet_queue;

uint32_t __rev(uint32_t);
void led_blinking_task(void);
uint8_t eparity(uint32_t);
uint32_t genAnalogPacket(int16_t);

extern void cdc_task(void);
extern void hid_app_task(void);

extern void neopixel_init(void);
extern void neopixel_task(int pat);

typedef struct TU_ATTR_PACKED
{
  int16_t global_buttons;
  int16_t global_x;
  int16_t global_y;

  int16_t output_buttons;
  int16_t output_buttons_alt;
  int16_t output_x1;
  int16_t output_y1;
  int16_t output_x2;
  int16_t output_y2;
  int16_t output_qx;
} Player_t;

Player_t players[5] = { 0 };
int playersCount = 0;

// When Nuon reads, set interlock to ensure atomic update
//
volatile bool  output_exclude = false;

uint32_t output_buttons_0 = 0;
uint32_t output_analog_1x = 0;
uint32_t output_analog_1y = 0;
uint32_t output_analog_2x = 0;
uint32_t output_analog_2y = 0;
uint32_t output_quadx = 0;

uint32_t device_mode = 0b10111001100000111001010100000000;
uint32_t device_config = 0b10000000100000110000001100000000;
uint32_t device_switch = 0b10000000100000110000001100000000;

PIO pio;
uint sm1, sm2;   // sm1 = clocked_out; sm2 = clocked_in

/*------------- MAIN -------------*/

// note that "__not_in_flash_func" functions are loaded
// and "pinned" in SRAM - not paged in/out from XIP flash
//

//
// update_output - updates output_word with multi-tap plex data that
//                 is sent to PCE based on state and device types
//
void __not_in_flash_func(update_output)(void)
{
  int16_t buttons = (players[0].output_buttons & 0xffff) |
                    (players[0].output_buttons_alt & 0xffff);
  int16_t checksum = (eparity(buttons & 0b1101111111111111) << 15) |
                     (eparity(buttons & 0b0011000000000000) << 14) |
                     (eparity(buttons & 0b0001100000000000) << 13) |
                     (eparity(buttons & 0b0000110000000000) << 12) |
                     (eparity(buttons & 0b0000011000000000) << 11) |
                     (eparity(buttons & 0b0000001100000000) << 10) |
                     (eparity(buttons & 0b0000000110000000) << 9) |
                     (eparity(buttons & 0b0000000011000000) << 8) |

                     (eparity(buttons & 0b0000000001100000) << 7) |
                     (eparity(buttons & 0b0000000000110000) << 6) |
                     (eparity(buttons & 0b0000000000011000) << 5) |
                     (eparity(buttons & 0b0000000000001100) << 4) |
                     (eparity(buttons & 0b1000000000000110) << 3) |
                     (eparity(buttons & 0b0100000000000011) << 2) |
                     (eparity(buttons & 0b0111111111111110) << 1) |
                     (eparity(buttons & 0b1011111111111111) << 0) ;

  output_buttons_0 = (buttons << 16) | (checksum & 0xffff);
  output_analog_1x = genAnalogPacket(players[0].output_x1);
  output_analog_1y = genAnalogPacket(players[0].output_y1);
  output_analog_2x = genAnalogPacket(players[0].output_x2);
  output_analog_2y = genAnalogPacket(players[0].output_y2);
  output_quadx     = genAnalogPacket(players[0].output_qx);
}

//
// post_globals - accumulate the many intermediate mouse scans (~1ms)
//                into an accumulator which will be reported back to PCE
//
void __not_in_flash_func(post_globals)(
  uint8_t dev_addr,
  uint8_t instance,
  uint16_t buttons,
  bool analog_1,
  uint8_t analog_1x,
  uint8_t analog_1y,
  bool analog_2,
  uint8_t analog_2x,
  uint8_t analog_2y,
  bool quad,
  uint8_t quad_x
) {
  // TODO: Mouse stuffs
  // if (delta_x >= 128) 
  //   players[dev_addr-1].global_x = players[dev_addr-1].global_x - (256-delta_x);
  // else
  //   players[dev_addr-1].global_x = players[dev_addr-1].global_x + delta_x;
  // if (delta_y >= 128) 
  //   players[dev_addr-1].global_y = players[dev_addr-1].global_y - (256-delta_y);
  // else
  //   players[dev_addr-1].global_y = players[dev_addr-1].global_y + delta_y;
  // players[dev_addr-1].global_x = delta_x;
  // players[dev_addr-1].global_y = delta_y;
  // players[dev_addr-1].global_buttons = buttons;
  // players[dev_addr-1].output_x = players[dev_addr-1].global_x;
  // players[dev_addr-1].output_y = players[dev_addr-1].global_y;
  // players[dev_addr-1].output_buttons = players[dev_addr-1].global_buttons;

  // Controller with analog processing
  if (!instance) {
    players[dev_addr-1].output_buttons = buttons;
  } else {
    players[dev_addr-1].output_buttons_alt = buttons;
  }

  if (analog_1) players[dev_addr-1].output_x1 = analog_1x;
  if (analog_1) players[dev_addr-1].output_y1 = analog_1y;
  if (analog_2) players[dev_addr-1].output_x2 = analog_2x;
  if (analog_2) players[dev_addr-1].output_y2 = analog_2y;
  if (quad) players[dev_addr-1].output_qx = quad_x;

  update_output();
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
    // led_blinking_task();
#endif

#if CFG_TUH_CDC
    cdc_task();
#endif

//
// check time offset in order to detect when a PCE scan is no longer
// in process (so that fresh values can be sent to the state machine)
//
    // current_time = get_absolute_time();

    // if (absolute_time_diff_us(init_time, current_time) > reset_period) {
    //   state = 3;
    //   update_output();
      // output_exclude = false;
    //   init_time = get_absolute_time();
    // }

#if CFG_TUH_HID
    hid_app_task();
#endif

  }
}

static bool __not_in_flash_func(get_bootsel_btn)() {
    const uint CS_PIN_INDEX = 1;

    // Must disable interrupts, as interrupt handlers may be in flash, and we
    // are about to temporarily disable flash access!
    uint32_t flags = save_and_disable_interrupts();

    // Set chip select to Hi-Z
    hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
                    GPIO_OVERRIDE_LOW << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);

    // Note we can't call into any sleep functions in flash right now
    for (volatile int i = 0; i < 1000; ++i);

    // The HI GPIO registers in SIO can observe and control the 6 QSPI pins.
    // Note the button pulls the pin *low* when pressed.
    bool button_state = !(sio_hw->gpio_hi_in & (1u << CS_PIN_INDEX));

    // Need to restore the state of chip select, else we are going to have a
    // bad time when we return to code in flash!
    hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
                    GPIO_OVERRIDE_NORMAL << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);

    restore_interrupts(flags);

    return button_state;
}

//
// core1_entry - inner-loop for the second core
//             - when the "CLR" line is de-asserted, set lock flag
//               protecting the output state machine from inconsistent data
//
static void __not_in_flash_func(core1_entry)(void)
{
  uint64_t packet = 0;
  uint16_t state = 0;
  uint8_t channel = 0;
  uint8_t id = 0;
  bool alive = false;
  bool tagged = false;
  bool branded = false;
  int requestsB = 0;
  while (1)
  {
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
      uint32_t word1 = __rev(0b01001010010101010100010001000101);
              
      pio_sm_put_blocking(pio1, sm1, word1);
      pio_sm_put_blocking(pio1, sm1, word0);
    }
    else if (dataA == 0x94) { // PROBE
      uint32_t word0 = 1;
      uint32_t word1 = __rev(0b10001011000000110000000000000000); // res from HPI controller

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
        word1 = __rev(0b11000100100000101001101100000000); // 68
        // word1 = __rev(genAnalogPacket((116)+127)); // send & recv?
      } else {
        word1 = __rev(0b11000110000000101001010000000000); // 70
        // word1 = __rev(genAnalogPacket((118)+127)); // send & recv?
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

      word1 = __rev(output_quadx);
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
      uint32_t word1 = __rev(0b10000000100000110000001100000000);

      word1 = __rev(device_config); // device config packet?

      pio_sm_put_blocking(pio1, sm1, word1);
      pio_sm_put_blocking(pio1, sm1, word0);
    }
    else if (dataA == 0x31 && dataS == 0x01 && dataC == 0x00) { // {SWITCH[16:9]}
      uint32_t word0 = 1;
      uint32_t word1 = __rev(0b10000000100000110000001100000000);

      word1 = __rev(device_switch); // device switch packet?

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

    // output_exclude = true;

    // update_output();

    // unsigned short int i;
    // for (i = 0; i < 5; ++i) {
    //   // decrement outputs from globals
    //   players[i].global_x = (players[i].global_x - players[i].output_x);
    //   players[i].global_y = (players[i].global_y - players[i].output_y);

    //   players[i].output_x = 0;
    //   players[i].output_y = 0;
    //   players[i].output_buttons = players[i].global_buttons;
    // }

  }
}

int main(void)
{
  board_init();

  // Pause briefly for stability before starting activity
  sleep_ms(1000);

  printf("TinyUSB Host CDC MSC HID Example\r\n");

  tusb_init();

  neopixel_init();

  unsigned short int i;
  for (i = 0; i < 5; ++i) {
    players[i].global_buttons = 0x80;
    players[i].global_x = 0;
    players[i].global_y = 0;
    players[i].output_buttons = 0x80;
    players[i].output_buttons_alt = 0x80;
    players[i].output_x1 = 127;
    players[i].output_y1 = 127;
    players[i].output_x2 = 127;
    players[i].output_y2 = 127;
    players[i].output_qx = 0;
  }

  output_buttons_0 = 0b00000000100000001000001100000011; // no buttons pressed
  output_analog_1x = 0b10000000100000110000001100000000; // x1 = 0
  output_analog_1y = 0b10000000100000110000001100000000; // y1 = 0
  output_analog_2x = 0b10000000100000110000001100000000; // x2 = 0
  output_analog_2y = 0b10000000100000110000001100000000; // y2 = 0
  output_quadx = 0b10000000100000110000001100000000; // quadx = 0

  // ANALOG        [0x0000001f]
  // ANALOG1, STDBUTTONS, DPAD, SHOULDER, EXTBUTTONS
  // device_mode   = 0b10111001100000111001010100000000; // 57
  // device_config = 0b10000000100000110000001100000000; // 0
  // device_switch = 0b10000000100000110000001100000000; // 0

  // ANALOG2       [0x0000083f]
  // MOUSE|TRACKBALL, ANALOG1, ANALOG2, STDBUTTONS, DPAD, SHOULDER, EXTBUTTONS
  // device_mode   = 0b10011101100000110100110100000000; // 29
  // device_config = 0b10000000100000110000001100000000; // 0
  // device_switch = 0b10000000100000110000001100000000; // 0

  // FLIGHT?      [0x0000051f]
  // RUDDER, THROTTLE, ANALOG1, STDBUTTONS, DPAD, SHOULDER, EXTBUTTONS
  // device_mode   = 0b10000000100000110000001100000000; // 0
  // device_config = 0b10000000100000110000001100000000; // 0
  // device_switch = 0b10000000100000110000001100000000; // 0

  // MOUSE        [0x00000808]
  // MOUSE|TRACKBALL, EXTBUTTONS
  // device_mode   = 0b11000110000000101001010000000000; // 70
  // device_config = 0b10000000100000110000001100000000; // 0
  // device_switch = 0b10000000100000110000001100000000; // 0

  // FISHINGREEL  [0x0001001d]
  // FISHINGREEL, ANALOG1, STDBUTTONS, DPAD, EXTBUTTONS
  // device_mode   = 0b11000000000000101000000000000000; // 64
  // device_config = 0b11000000000000101000000000000000; // 64
  // device_switch = 0b10000000100000110000001100000000; // 0

  // DUALWHEEL    [0x0000c51b]
  // THUMBWHEEL1, THUMBWHEEL2, RUDDER, THROTTLE, ANALOG1, STDBUTTONS, EXTBUTTONS
  // device_mode   = 0b10000000100000110000001100000000; // 0
  // device_config = 0b11000000000000101000000000000000; // 64
  // device_switch = 0b11000000000000101000000000000000; // 64

  // SPINANALOG   [0x0000103f]
  // QUADSPINNER1, ANALOG1, ANALOG2, STDBUTTONS, DPAD, SHOULDER, EXTBUTTONS
  // device_mode   = 0b10011101100000110100110100000000; // 29
  // device_config = 0b11000000000000101000000000000000; // 64
  // device_switch = 0b11000000000000101000000000000000; // 64

  // WHEEL        [0x00001301]
  // QUADSPINNER1, BRAKE, THROTTLE, STDBUTTONS
  // device_mode   = 0b11000000000000101000000000000000; // 64
  // device_config = 0b11000000000000101000000000000000; // 64
  // device_switch = 0b11000000000000101000000000000000; // 64

  // TESTING
  device_mode   = genAnalogPacket((29)+127);
  device_config = genAnalogPacket((64)+127);
  device_switch = genAnalogPacket((64)+127);

  // Both state machines can run on the same PIO processor
  pio = pio0;

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

  multicore_launch_core1(core1_entry);

  process_signals();

  return 0;
}


//--------------------------------------------------------------------+
// USB CDC
//--------------------------------------------------------------------+
#if CFG_TUH_CDC
CFG_TUSB_MEM_SECTION static char serial_in_buffer[64] = { 0 };

void tuh_mount_cb(uint8_t dev_addr)
{
  // application set-up
  printf("A device with address %d is mounted\r\n", dev_addr);

  tuh_cdc_receive(dev_addr, serial_in_buffer, sizeof(serial_in_buffer), true); // schedule first transfer

  playersCount++;
}

void tuh_umount_cb(uint8_t dev_addr)
{
  // application tear-down
  printf("A device with address %d is unmounted \r\n", dev_addr);

  if ((--playersCount) < 0) playersCount = 0;
}

// invoked ISR context
void tuh_cdc_xfer_isr(uint8_t dev_addr, xfer_result_t event, cdc_pipeid_t pipe_id, uint32_t xferred_bytes)
{
  (void) event;
  (void) pipe_id;
  (void) xferred_bytes;

  printf(serial_in_buffer);
  tu_memclr(serial_in_buffer, sizeof(serial_in_buffer));

  tuh_cdc_receive(dev_addr, serial_in_buffer, sizeof(serial_in_buffer), true); // waiting for next data
}

void cdc_task(void)
{

}

#endif

uint8_t eparity(uint32_t data) {
  uint32_t eparity;
  eparity = (data>>16)^data;
  eparity ^= (eparity>>8);
  eparity ^= (eparity>>4);
  eparity ^= (eparity>>2);
  eparity ^= (eparity>>1);
  return ((eparity)&0x1);
}

// checks whether value exist within every other subgroup of n(size).
uint32_t genAnalogPacket(int16_t value) { // 0 - 254
  value += 1;
  return (((value & 0xff) << 24) | ((crc_calc(value, 0) & 0xffff) << 8));
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
