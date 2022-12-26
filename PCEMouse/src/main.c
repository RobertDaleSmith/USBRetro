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

queue_t packet_queue;

uint32_t __rev(uint32_t);
void led_blinking_task(void);
uint8_t eparity(uint32_t);
uint8_t checkbit(uint8_t, uint8_t, bool);
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
  int16_t output_x1;
  int16_t output_y1;
  int16_t output_x2;
  int16_t output_y2;
} Player_t;

Player_t players[5] = { 0 };
int playersCount = 0;


// When Nuon reads, set interlock to ensure atomic update
//
volatile bool  output_exclude = false;

uint32_t output_buttons_0 = 0;
uint32_t output_analogx_0 = 0;
uint32_t output_analogy_0 = 0;
uint32_t output_analogx_1 = 0;
uint32_t output_analogy_1 = 0;

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
  int16_t buttons = (players[0].output_buttons & 0xffff);
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
  output_analogx_0 = genAnalogPacket(players[0].output_x1);
  output_analogy_0 = genAnalogPacket(players[0].output_y1);
  output_analogx_1 = genAnalogPacket(players[0].output_x2);
  output_analogy_1 = genAnalogPacket(players[0].output_y2);
}

//
// post_globals - accumulate the many intermediate mouse scans (~1ms)
//                into an accumulator which will be reported back to PCE
//
void __not_in_flash_func(post_globals)(uint8_t dev_addr, uint16_t buttons, uint8_t analog_x1, uint8_t analog_y1, uint8_t analog_x2, uint8_t analog_y2)
{
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
  players[dev_addr-1].output_buttons = buttons;
  players[dev_addr-1].output_x1 = analog_x1;
  players[dev_addr-1].output_y1 = analog_y1;
  players[dev_addr-1].output_x2 = analog_x2;
  players[dev_addr-1].output_y2 = analog_y2;

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
  uint8_t defcfg = 1;
  uint8_t version = 11;
  uint8_t type = 3;
  uint8_t mfg = 0;
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
    if (dataA == 0x80) { // ALIVE
      uint32_t word0 = 1;
      uint32_t word1 = __rev(0b01);
      if (alive) word1 = __rev(0b10);
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
    else if (dataA == 0x90) { // MAGIC
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
      word1 = ((defcfg  & 1)<<31) |
              ((version & 0b01111111)<<24) |
              ((type    & 0b11111111)<<16) |
              ((mfg     & 0b11111111)<<8) |
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
        word1 = __rev(0b11000100100000101001101100000000);
      } else {
        word1 = __rev(0b11000110000000101001010000000000);
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
    else if (dataA == 0x35 && dataS == 0x01 && dataC == 0x00) { // ANALOG
      uint32_t word0 = 1;
      uint32_t word1 = __rev(0b10000000100000110000001100000000); //0

      if (channel == ATOD_CHANNEL_MODE) {
        word1 = __rev(0b00000001000000000000000000000000);
      }
      if (channel == ATOD_CHANNEL_X1) {
        word1 = __rev(output_analogx_0);
      }
      else if (channel == ATOD_CHANNEL_Y1) {
        word1 = __rev(output_analogy_0);
      }
      else if (channel == ATOD_CHANNEL_X2) {
        word1 = __rev(output_analogx_1);
      }
      else if (channel == ATOD_CHANNEL_Y2) {
        word1 = __rev(output_analogy_1);
      }

      pio_sm_put_blocking(pio1, sm1, word1);
      pio_sm_put_blocking(pio1, sm1, word0);
    }
    else if (dataA == 0x25 && dataS == 0x01 && dataC == 0x00) { // CONFIG
      uint32_t word0 = 1;
      uint32_t word1 = __rev(0b10000000100000110000001100000000);

      pio_sm_put_blocking(pio1, sm1, word1);
      pio_sm_put_blocking(pio1, sm1, word0);
    }
    else if (dataA == 0x31 && dataS == 0x01 && dataC == 0x00) { // {SWITCH[16:9]}
      uint32_t word0 = 1;
      uint32_t word1 = __rev(0b10000000100000110000001100000000);

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
        uint32_t word1 = __rev(0b10000000000000000000000000000000);

        if (((state >> 8) | 0xff) == 0x41 && (state | 0xff) == 0x51) {
          word1 = __rev(0b11000000000000101000000000000000);
        }
        pio_sm_put_blocking(pio1, sm1, word1);
        pio_sm_put_blocking(pio1, sm1, word0);
      } else { // type0 == PACKET_TYPE_WRITE
        state = ((state) << 8) | (dataC & 0xff);
      }
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
    players[i].output_x1 = 0;
    players[i].output_y1 = 0;
    players[i].output_x2 = 0;
    players[i].output_y2 = 0;
  }

  output_buttons_0 = 0b00000000100000001000001100000011; // no buttons pressed
  output_analogx_0 = 0b10000000100000110000001100000000; // x1 = 0
  output_analogy_0 = 0b10000000100000110000001100000000; // y1 = 0
  output_analogx_1 = 0b10000000100000110000001100000000; // x2 = 0
  output_analogy_1 = 0b10000000100000110000001100000000; // y2 = 0

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
uint8_t checkbit(uint8_t value, uint8_t size, bool zero) {
  bool inSet = false;
  bool skip = !zero;

  int i = 0;
  do {
    if (!skip) {
      if (value >= i && value <= i+(size-1)) {
        inSet = true;
      }
    }

    i = i + ((!i) ? (size/2) : size);
    skip = !skip;
  } while (i < 128 && !inSet);

  return (inSet ? 1 : 0);
}

// checks whether value exist within every other subgroup of n(size).
uint32_t genAnalogPacket(int16_t value) { // 0 - 254
  value -= 127;
  // if (value < -127) value = -127;
  if (value > 127) value = 127;

  bool positive = (value >= 0);
  uint8_t delta = ((positive ? value : (-1 * value)) & 0b01111111);
  uint8_t value_byte = (((positive ? 1 : 0) & 1) << 7) |  // value is positive
                       ((positive ? delta : (~delta)) & 0b01111111); // value, ones' complement if negative

  return (value_byte << 24) |
         (((eparity(value_byte)) & 1) << 23) | // value_byte[7-0] is even parity
         ((((positive)?1:0) & 1) << 17) | // value is positive
       //(((checkbit(delta, 128, true)) & 1) << 16) | // 128, is zero
         ((((delta <= 63) ? 1:0) & 1) << 16) | // [-63 - 63] 128
         (((checkbit(delta, 64, false)) & 1) << 15) | // 64, not zero
         (((checkbit(delta, 32, false)) & 1) << 14) | // 32, not zero
         (((checkbit(delta, 16, false)) & 1) << 13) | // 16, not zero
         (((checkbit(delta, 8, false)) & 1) << 12) |  // 8, not zero
         (((checkbit(delta, 4, false)) & 1) << 11) |  // 4, not zero
         (((checkbit(delta, 2, false)) & 1) << 10) |  // 2, not zero
         (((eparity(value_byte & 0b11111110)) & 1) << 9) | // value_byte[7-1] is even parity
         (((eparity(value_byte)) & 1) << 8); // value_byte[7-0] is even parity
}

  
