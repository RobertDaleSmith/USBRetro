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

#include "pico/stdlib.h"
#include "pico/time.h"
#include "pico/multicore.h"
#include "hardware/pio.h"
#include "plex.pio.h"
#include "clock.pio.h"
#include "select.pio.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+


#ifdef ADAFRUIT_KB2040          // if build for Adafruit KB2040 board

#define DATAIN_PIN      18
#define CLKIN_PIN       DATAIN_PIN + 1  // Note - in pins must be a consecutive 'in' group
#define OUTD0_PIN       26              // Note - out pins must be a consecutive 'out' group
#define OUTD1_PIN       27
#define OUTD2_PIN       28
#define OUTD3_PIN       29

#else
#ifdef ADAFRUIT_QTPY_RP2040      // if build for QtPy RP2040 board

#define DATAIN_PIN      24
#define CLKIN_PIN       DATAIN_PIN + 1  // Note - in pins must be a consecutive 'in' group
#define OUTD0_PIN       26              // Note - out pins must be a consecutive 'out' group
#define OUTD1_PIN       27
#define OUTD2_PIN       28
#define OUTD3_PIN       29

#else
#ifdef SEEED_XIAO_RP2040         // else assignments for Seed XIAO RP2040 board - note: needs specific board

#define DATAIN_PIN      24
#define CLKIN_PIN       DATAIN_PIN + 1  // Note - in pins must be a consecutive 'in' group
#define OUTD0_PIN       26              // Note - out pins must be a consecutive 'out' group
#define OUTD1_PIN       27
#define OUTD2_PIN       28
#define OUTD3_PIN       29

#else                           // else assume build for RP Pico board

#define DATAIN_PIN      16
#define CLKIN_PIN       DATAIN_PIN + 1  // Note - in pins must be a consecutive 'in' group
#define OUTD0_PIN       18              // Note - out pins must be a consecutive 'out' group
#define OUTD1_PIN       19
#define OUTD2_PIN       20
#define OUTD3_PIN       21

#endif
#endif
#endif

void led_blinking_task(void);

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
  int16_t output_x;
  int16_t output_y;

  bool is6btn;
} Player_t;

Player_t players[5] = { 0 };
int playersCount = 0;


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

//
// update_output - updates output_word with multi-tap plex data that
//                 is sent to PCE based on state and device types
//
void __not_in_flash_func(update_output)(void)
{
  int8_t bytes[5] = { 0 };

  unsigned short int i;
  for (i = 0; i < 5; ++i) {
    bool has6Btn = !(players[i].output_buttons & 0x0f00);
    bool isMouse = !(players[i].output_buttons & 0x0f);

    // base controller/mouse buttons
    int8_t byte = (players[i].output_buttons & 0xff);

    // 6 button extra four buttons (III/IV/V/VI)
    if (has6Btn && players[i].is6btn && (state == 2)) {
      byte = ((players[i].output_buttons>>8) & 0xf0);
    }

    // mouse x/y states
    if (isMouse) {
      switch (state) {
        case 3: // state 3: x most significant nybble
          byte |= (((players[i].output_x>>1) & 0xf0) >> 4);
          break;
        case 2: // state 2: x least significant nybble
          byte |= (((players[i].output_x>>1) & 0x0f));
          break;
        case 1: // state 1: y most significant nybble
          byte |= (((players[i].output_y>>1) & 0xf0) >> 4);
          break;
        case 0: // state 0: y least significant nybble
          byte |= (((players[i].output_y>>1) & 0x0f));
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
}


//
// post_globals - accumulate the many intermediate mouse scans (~1ms)
//                into an accumulator which will be reported back to PCE
//
void __not_in_flash_func(post_globals)(uint8_t dev_addr, uint16_t buttons, uint8_t delta_x, uint8_t delta_y)
{
  bool has6Btn = !(buttons & 0x0f00);
  bool isMouse = !(buttons & 0x0f); // dpad least significant nybble only zero for usb mice

  if (delta_x >= 128) 
    players[dev_addr-1].global_x = players[dev_addr-1].global_x - (256-delta_x);
  else
    players[dev_addr-1].global_x = players[dev_addr-1].global_x + delta_x;

  if (delta_y >= 128) 
    players[dev_addr-1].global_y = players[dev_addr-1].global_y - (256-delta_y);
  else
    players[dev_addr-1].global_y = players[dev_addr-1].global_y + delta_y;

  players[dev_addr-1].global_buttons = buttons;

  if (has6Btn && !(buttons & 0b0000000010000001)) {
    players[dev_addr-1].is6btn = true;
  }
  else if (has6Btn && !(buttons & 0b0000000010000100)) {
    players[dev_addr-1].is6btn = false;
  }

  if (!output_exclude || !isMouse)
  {
     players[dev_addr-1].output_x = players[dev_addr-1].global_x;
     players[dev_addr-1].output_y = players[dev_addr-1].global_y;
     players[dev_addr-1].output_buttons = players[dev_addr-1].global_buttons;

     update_output();
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

#if CFG_TUH_CDC
    cdc_task();
#endif

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

#if CFG_TUH_HID
    hid_app_task();
#endif

  }
}


//
// core1_entry - inner-loop for the second core
//             - when the "CLR" line is de-asserted, set lock flag
//               protecting the output state machine from inconsistent data
//
static void __not_in_flash_func(core1_entry)(void)
{
static bool rx_bit = 0;

  while (1)
  {
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
        for (i = 0; i < 5; ++i) {
          // decrement outputs from globals
          players[i].global_x = (players[i].global_x - players[i].output_x);
          players[i].global_y = (players[i].global_y - players[i].output_y);

          players[i].output_x = 0;
          players[i].output_y = 0;
          players[i].output_buttons = players[i].global_buttons;
        }

        output_exclude = true;            // continue to lock the output values (which are now zero)
     }

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
    players[i].global_buttons = 0xFFFF;
    players[i].global_x = 0;
    players[i].global_y = 0;
    players[i].output_buttons = 0xFFFF;
    players[i].output_x = 0;
    players[i].output_y = 0;
    players[i].is6btn = false;
  }
  state = 3;

  output_word_0 = 0x00FFFFFFFF;  // no buttons pushed
  output_word_1 = 0x00000000FF;  // no buttons pushed


  init_time = get_absolute_time();

  // Both state machines can run on the same PIO processor
  pio = pio0;

  // Load the plex (multiplex output) program, and configure a free state machine
  // to run the program.

  uint offset1 = pio_add_program(pio, &plex_program);
  sm1 = pio_claim_unused_sm(pio, true);
  plex_program_init(pio, sm1, offset1, DATAIN_PIN, CLKIN_PIN, OUTD0_PIN);


  // Load the clock/select (synchronizing input) programs, and configure a free state machines
  // to run the programs.

  uint offset2 = pio_add_program(pio, &clock_program);
  sm2 = pio_claim_unused_sm(pio, true);
  clock_program_init(pio, sm2, offset2, CLKIN_PIN);

  uint offset3 = pio_add_program(pio, &select_program);
  sm3 = pio_claim_unused_sm(pio, true);
  select_program_init(pio, sm3, offset3, DATAIN_PIN);

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

  playersCount--;
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

//--------------------------------------------------------------------+
// TinyUSB Callbacks
//--------------------------------------------------------------------+

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
