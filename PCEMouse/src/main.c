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
#include "pico/util/queue.h"
#include "hardware/pio.h"
#include "hardware/gpio.h"
#include "polyface_read.pio.h"
#include "polyface_send.pio.h"

#define BYTE_TO_BINARY_PATTERN_DAT "%c%c %c%c%c%c%c%c%c%c %c%c%c%c%c%c%c%c %c%c%c%c%c%c%c%c %c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY_PATTERN_CMD "%c%c %c%c%c%c%c %c%c %c %c%c%c%c%c%c%c %c %c%c%c%c%c%c%c %c %c%c%c%c%c%c%c %c"
#define BYTE_TO_BINARY(byte)  \
  (byte & 0x200000000 ? '1' : '0'), \
  (byte & 0x100000000 ? '1' : '0'), \
  (byte & 0x080000000 ? '1' : '0'), \
  (byte & 0x040000000 ? '1' : '0'), \
  (byte & 0x020000000 ? '1' : '0'), \
  (byte & 0x010000000 ? '1' : '0'), \
  (byte & 0x008000000 ? '1' : '0'), \
  (byte & 0x004000000 ? '1' : '0'), \
  (byte & 0x002000000 ? '1' : '0'), \
  (byte & 0x001000000 ? '1' : '0'), \
  (byte & 0x000800000 ? '1' : '0'), \
  (byte & 0x000400000 ? '1' : '0'), \
  (byte & 0x000200000 ? '1' : '0'), \
  (byte & 0x000100000 ? '1' : '0'), \
  (byte & 0x000080000 ? '1' : '0'), \
  (byte & 0x000040000 ? '1' : '0'), \
  (byte & 0x000020000 ? '1' : '0'), \
  (byte & 0x000010000 ? '1' : '0'), \
  (byte & 0x000008000 ? '1' : '0'), \
  (byte & 0x000004000 ? '1' : '0'), \
  (byte & 0x000002000 ? '1' : '0'), \
  (byte & 0x000001000 ? '1' : '0'), \
  (byte & 0x000000800 ? '1' : '0'), \
  (byte & 0x000000400 ? '1' : '0'), \
  (byte & 0x000000200 ? '1' : '0'), \
  (byte & 0x000000100 ? '1' : '0'), \
  (byte & 0x000000080 ? '1' : '0'), \
  (byte & 0x000000040 ? '1' : '0'), \
  (byte & 0x000000020 ? '1' : '0'), \
  (byte & 0x000000010 ? '1' : '0'), \
  (byte & 0x000000008 ? '1' : '0'), \
  (byte & 0x000000004 ? '1' : '0'), \
  (byte & 0x000000002 ? '1' : '0'), \
  (byte & 0x000000001 ? '1' : '0') 

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

queue_t packet_queue;

void led_blinking_task(void);
uint8_t eparity(uint32_t);

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

// static absolute_time_t init_time;
// static absolute_time_t current_time;
// static absolute_time_t loop_time;
// static const int64_t reset_period = 600;  // at 600us, reset the scan exclude flag

PIO pio;
uint sm1, sm2;   // sm1 = clock_out; sm2 = clock_in

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
  int switchA = 0;
  
  while (1)
  {
    if (!queue_is_empty(&packet_queue)) {
      uint64_t packet;
      queue_try_remove(&packet_queue, &packet);

      uint8_t ctrlBit = ((packet>>32) & 0b00000001);
      uint8_t _id = ((packet>>27) & 0b00011111);
      uint8_t type1 = ((packet>>26) & 0b00000001);
      uint8_t type0 = ((packet>>25) & 0b00000001);
      uint8_t cmdat = ((packet>>24) & 0b00000001);
      uint8_t incad = ((packet>>16) & 0b00000001);
      uint8_t crc = ((packet>>8) & 0b00000001);
      uint8_t dataA = ((packet>>17) & 0b11111111);
      uint8_t dataS = ((packet>>9) & 0b01111111);
      uint8_t dataC = ((packet>>1) & 0b01111111);
      // uint8_t eParity = eparity((packet & 0xFFFFFFFF));

      if (!ctrlBit) {
        if (switchA) {
          printf(""BYTE_TO_BINARY_PATTERN_DAT" | ", BYTE_TO_BINARY(packet));
          printf(" - %d \r\n", switchA);
          switchA++;
          if (switchA > 5) {
            switchA = 0;
            printf("\r\n");
          }
        }
      } else {
        if (pio_sm_is_tx_fifo_full(pio1, sm1)) printf("FULL.");

        // if (dataA == 0x84) {
          switchA = 1;
          printf(""BYTE_TO_BINARY_PATTERN_CMD" | ", BYTE_TO_BINARY(packet));
          printf("ID: "); printf(_id<16 ? "0x0%x " : "0x%x ", _id);
          printf(type1 ? "DIRECT   " : "INDIRECT ");
          printf(type0 ? "READ  " : "WRITE ");
          printf(cmdat ? "CMD  " : "DATA ");
          // printf("IA: %d ", incad);
          // printf("CRC: %d ", crc);
          printf("A: "); printf(dataA<16 ? "0x0%x " : "0x%x ", dataA);
          printf("S: "); printf(dataS<16 ? "0x0%x " : "0x%x ", dataS);
          printf("C: "); printf(dataC<16 ? "0x0%x " : "0x%x ", dataC);
          // printf("Parity: "); printf((eParity ? "PASS " : "FAIL "));
          if (dataA == 0xb0 && dataS == 0x00 && dataC == 0x01) printf("[FOCUS] ");
          if (dataA == 0xb0 && dataS == 0x00 && dataC == 0x02) printf("[BLUR] ");
          if (dataA == 0xb1) printf("[RESET] ");
          if (dataA == 0xb2) printf("[TAG] ");
          if (dataA == 0xb3) printf("[UNBRAND] ");
          if (dataA == 0xb4) printf("[BRAND] ");
          if (dataA == 0x94 && dataS == 0x04 && dataC == 0x00) printf("[PROBE] ");
          if (dataA == 0xb1 && dataS == 0x04 && dataC == 0x00) printf("[MAGIC] ");
          if (dataA == 0x90) printf("[MAGIC] ");
          if (dataA == 0x9a) printf("[CRC] ");
          if (dataA == 0x99) printf("[STATE] ");
          if (dataA == 0x80 && dataS == 0x04 && dataC == 0x40) printf("[ALIVE] ");
          else if (dataA == 0x80) printf("[CRC] ");
          if (dataA == 0x84) printf("[REQUEST] ");
          if (dataA == 0x85 || dataA == 0x88 || dataA == 0x98) printf("[ERROR] ");
          if (dataA == 0xa0) printf("[NOP] ");
          if (dataA == 0x30) printf("[{SWITCH[8:1]}] ");
          if (dataA == 0x31) printf("[{SWITCH[16:9]}] ");
          if (dataA == 0x32) printf("[QUADX] ");
          if (dataA == 0x33) printf("[QUADY] ");
          if (dataA == 0x34) printf("[CHANNEL] ");
          if (dataA == 0x35) printf("[ANALOG] ");
          if (dataA == 0x40) printf("[BAUD] ");
          if (dataA == 0x41) printf("[FLAGS0] ");
          if (dataA == 0x42) printf("[FLAGS1] ");
          if (dataA == 0x43) printf("[SDATA] ");
          if (dataA == 0x44) printf("[SSTATUS] ");
          if (dataA == 0x45) printf("[RSTATUS] ");
          if (dataA == 0x20) printf("[A0 (A[7:0])] ");
          if (dataA == 0x21) printf("[A1 (A[15:8])] ");
          if (dataA == 0x22) printf("[A2 (sticky_cs,A[23:16])] ");
          if (dataA == 0x23) printf("[STROBE] ");
          if (dataA == 0x24) printf("[PINOUT] ");
          if (dataA == 0x25) printf("[CONFIG] ");
          if (dataA == 0x26) printf("[INPUTA] ");
          if (dataA == 0x27) printf("[REQUEST] ");
          if (dataA == 0x28) printf("[INPUTB] ");
          printf("\r\n");
        // } else {
        //   switchA = 0;
        // }
      }
    }

    // tinyusb host task
    // tuh_task();
    // neopixel task
    // neopixel_task(playersCount);
#ifndef ADAFRUIT_QTPY_RP2040
    // led_blinking_task();
#endif

#if CFG_TUH_CDC
    // cdc_task();
#endif

//
// check time offset in order to detect when a PCE scan is no longer
// in process (so that fresh values can be sent to the state machine)
//
    // current_time = get_absolute_time();

    // if (absolute_time_diff_us(init_time, current_time) > reset_period) {
    //   state = 3;
    //   update_output();
    //   output_exclude = false;
    //   init_time = get_absolute_time();
    // }

#if CFG_TUH_HID
    // hid_app_task();
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
  bool alive = false;
  static uint64_t packet = 0;
  while (1)
  {
    for (int i = 0; i < 2; ++i) {
      uint32_t rxdata = pio_sm_get_blocking(pio, sm2);

      // printf(""BYTE_TO_BINARY_PATTERN"", BYTE_TO_BINARY(rxdata));
      packet = ((packet) << 32) | (rxdata & 0xFFFFFFFF);
    }

    queue_try_add(&packet_queue, &packet);

    uint8_t dataA = ((packet>>17) & 0b11111111);
    uint8_t dataS = ((packet>>9) & 0b01111111);
    uint8_t dataC = ((packet>>1) & 0b01111111);
    if (dataA == 0x80) { // ALIVE
      uint32_t word0 = 0b1111111111111111111111111111111;
      uint32_t word1 = 0b1000000000000000000000000000000;
      if (alive) word1 = 0b01000000000000000000000000000000;
      else alive = true;

      pio_sm_put_blocking(pio1, sm1, word1);
      pio_sm_put_blocking(pio1, sm1, word0);
    }
    // if (dataA == 0x88 && dataS == 0x04 && dataC == 0x40) { // ERROR
    //   u_int32_t word0 = 0b01;
    //   u_int32_t word1 = 0b00;
    //   pio_sm_put_blocking(pio1, sm1, word1);
    //   pio_sm_put_blocking(pio1, sm1, word0);
    // }
    // if (dataA == 0x90) { // MAGIC
    //   u_int32_t word0 = 0b01;
    //   u_int32_t word1 = 0b10100010001000101010101001010010;
              
    //   pio_sm_put_blocking(pio1, sm1, word1);
    //   pio_sm_put_blocking(pio1, sm1, word0);
    // }
    // if (dataA == 0x94) { // PROBE
    //   uint32_t word0 = 0b01;
    //   uint32_t word1 = 0b10000000000000000000000000000000;

    //   pio_sm_put_blocking(pio1, sm1, word1);
    //   pio_sm_put_blocking(pio1, sm1, word0);
    // }
    // if (dataA == 0xb1) { // RESET
    //   uint32_t word0 = 0b00000000000000000000000000000001;
    //   uint32_t word1 = 0b10000000000000000000000000000000;

    //   pio_sm_put_blocking(pio1, sm1, word1);
    //   pio_sm_put_blocking(pio1, sm1, word0);
    // }

     // Now we are in an update-sequence; set a lock
     // to prevent update during output transaction
    //  output_exclude = true;

     // assume data is already formatted in output_word and push it to the state machine
    //  pio_sm_put(pio, sm1, output_word_1);
    //  pio_sm_put(pio, sm1, output_word_0);

     // Sequence from state 3 down through state 0 (show different nybbles to PCE)
     //
     // Note that when state = zero, it doesn't transition to a next state; the reset to
     // state 3 will happen as part of a timed process on the second CPU & state machine
     //

     // Also note that staying in 'scan' (CLK = low, SEL = high), is not expected
     // last more than about a half of a millisecond
     //
    //  loop_time = get_absolute_time();
    //  while ((gpio_get(CLKIN_PIN) == 0) && (gpio_get(DATAIO_PIN) == 1))
    //  {
    //     if (absolute_time_diff_us(loop_time, get_absolute_time()) > 550) {
    //        state = 0;
    //        break;
    //     }
    //  }

    //  if (state != 0)
    //  {
    //     state--;
    //     update_output();

    //     // renew countdown timeframe
    //     init_time = get_absolute_time();
    //  }
    //  else
    //  {
    //     update_output();

    //     unsigned short int i;
    //     for (i = 0; i < 5; ++i) {
    //       // decrement outputs from globals
    //       players[i].global_x = (players[i].global_x - players[i].output_x);
    //       players[i].global_y = (players[i].global_y - players[i].output_y);

    //       players[i].output_x = 0;
    //       players[i].output_y = 0;
    //       players[i].output_buttons = players[i].global_buttons;
    //     }

    //     output_exclude = true;            // continue to lock the output values (which are now zero)
    //  }

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

  // init_time = get_absolute_time();

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

//--------------------------------------------------------------------+
// TinyUSB Callbacks
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// Blinking Task
//--------------------------------------------------------------------+
void led_blinking_task(void)
{
  const uint32_t interval_ms = 10;
  static uint32_t start_ms = 0;

  static bool led_state = false;

  // Blink every interval ms
  if ( board_millis() - start_ms < interval_ms) return; // not enough time
  start_ms += interval_ms;

  board_led_write(led_state);
  led_state = 1 - led_state; // toggle
}

uint8_t eparity(uint32_t data) {
  uint32_t eparity;
  eparity = (data>>16)^data;
  eparity ^= (eparity>>8);
  eparity ^= (eparity>>4);
  eparity ^= (eparity>>2);
  eparity ^= (eparity>>1);
  return((~eparity)&0x1);
}
