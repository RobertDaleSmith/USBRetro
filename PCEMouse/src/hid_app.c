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
 * Copyright (c) 2021, Ha Thach (tinyusb.org)
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

#include "bsp/board_api.h"
#include "tusb.h"
#include "hid_parser.h"
#include "devices/device_utils.h"
#include "devices/device_registry.h"

#define HID_DEBUG 0
#define LANGUAGE_ID 0x0409

// SWITCH PRO
#define PROCON_REPORT_SEND_USB 0x80
#define PROCON_USB_HANDSHAKE   0x02
#define PROCON_USB_BAUD        0x03
#define PROCON_USB_ENABLE      0x04
#define PROCON_USB_DO_CMD      0x92
#define PROCON_CMD_AND_RUMBLE  0x01
#define PROCON_CMD_MODE        0x03
#define PROCON_CMD_LED_HOME    0x38
#define PROCON_ARG_INPUT_FULL  0x30

const char* dpad_str[] = { "N", "NE", "E", "SE", "S", "SW", "W", "NW", "none" };

uint8_t output_sequence_counter = 0;
uint8_t last_rumble = 0;
uint8_t last_leds = 0;

/** Used to set the LEDs on the controllers */
const uint8_t PLAYER_LEDS[] = {
  0x00, // OFF
  0x01, // LED1  0001
  0x02, // LED2  0010
  0x04, // LED3  0100
  0x08, // LED4  1000
  0x09, // LED5  1001
  0x0A, // LED6  1010
  0x0C, // LED7  1100
  0x0D, // LED8  1101
  0x0E, // LED9  1110
  0x0F, // LED10 1111
};

////////////////
// hid_parser //
////////////////
#define INVALID_REPORT_ID -1
// means 1/X of half range of analog would be dead zone
#define DEAD_ZONE 4U
static const char *const BUTTON_NAMES[] = {"NONE", "^", ">", "\\/", "<", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N"};
//(hat format, 8 is released, 0=N, 1=NE, 2=E, 3=SE, 4=S, 5=SW, 6=W, 7=NW)
static const uint8_t HAT_SWITCH_TO_DIRECTION_BUTTONS[] = {0b0001, 0b0011, 0b0010, 0b0110, 0b0100, 0b1100, 0b1000, 0b1001, 0b0000};

typedef union
{
  struct
  {
    bool up : 1;
    bool right : 1;
    bool down : 1;
    bool left : 1;
    bool button1 : 1;
    bool button2 : 1;
    bool button3 : 1;
    bool button4 : 1;

    bool button5 : 1;
    bool button6 : 1;
    bool button7 : 1;
    bool button8 : 1;
    bool button9 : 1;
    bool button10 : 1;
    bool button11 : 1;
    bool button12 : 1;

    uint8_t x, y, z, rz; // joystick
  };
  struct
  {
    uint8_t all_direction : 4;
    uint16_t all_buttons : 12;
    uint32_t analog_sticks : 32;
  };
  uint64_t value : 56;
} pad_buttons;

// Nintedo Switch Pro USB Controller
typedef struct {
    uint8_t  report_id;   // The first byte is always the report ID
    uint8_t  timer;       // Timer tick (1 tick = 5ms)
    uint8_t  battery_level_and_connection_info; // Battery level and connection info
    struct {
      uint8_t y    : 1;
      uint8_t x    : 1;
      uint8_t b    : 1;
      uint8_t a    : 1;
      uint8_t sr_r : 1;
      uint8_t sl_r : 1;
      uint8_t r    : 1;
      uint8_t zr   : 1;
    };

    struct {
      uint8_t select  : 1;
      uint8_t start   : 1;
      uint8_t rstick  : 1;
      uint8_t lstick  : 1;
      uint8_t home    : 1;
      uint8_t cap     : 1;
      uint8_t padding : 2;
    };

    struct {
      uint8_t down  : 1;
      uint8_t up    : 1;
      uint8_t right : 1;
      uint8_t left  : 1;
      uint8_t sr_l  : 1;
      uint8_t sl_l  : 1;
      uint8_t l     : 1;
      uint8_t zl    : 1;
    };
    uint8_t  left_stick[3]; // 12 bits for X and Y each (little endian)
    uint8_t  right_stick[3]; // 12 bits for X and Y each (little endian)
    uint8_t  vibration_ack; // Acknowledge output reports that trigger vibration
    uint8_t  subcommand_ack; // Acknowledge if a subcommand was executed
    uint8_t  subcommand_reply_data[35]; // Reply data for executed subcommands

    uint16_t left_x, left_y, right_x, right_y;
} switch_report_t;

typedef union
{
  switch_report_t data;
  uint8_t buf[sizeof(switch_report_t)];
} switch_report_01_t;

#define MAX_BUTTONS 12 // max generic HID buttons to map
#define MAX_DEVICES 6
#define MAX_REPORT  5

typedef struct {
    uint8_t byteIndex;
    uint16_t bitMask;
    uint32_t mid;
} InputLocation;

// Each HID instance can has multiple reports
typedef struct TU_ATTR_PACKED
{
  uint8_t type;
  uint8_t report_count;
  tuh_hid_report_info_t report_info[MAX_REPORT];
  //
  bool kbd_init;
  bool kbd_ready;
  bool switch_conn_ack;
  bool switch_baud;
  bool switch_baud_ack;
  bool switch_handshake;
  bool switch_handshake_ack;
  bool switch_usb_enable;
  bool switch_usb_enable_ack;
  bool switch_home_led;
  bool switch_command_ack;
  int switch_player_led_set;
  uint8_t motor_left;
  uint8_t motor_right;
  uint8_t analog_l;
  uint8_t analog_r;

  InputLocation xLoc;
  InputLocation yLoc;
  InputLocation zLoc;
  InputLocation rzLoc;
  InputLocation hatLoc;
  InputLocation buttonLoc[MAX_BUTTONS]; // assuming a maximum of 12 buttons
  uint8_t buttonCnt;

} instance_t;

// hid_parser
HID_ReportInfo_t *info;

// Cached device report properties on mount
typedef struct TU_ATTR_PACKED
{
  uint16_t serial[20];
  uint16_t vid, pid;
  instance_t instances[CFG_TUH_HID];
  uint8_t instance_count;
  uint8_t instance_root;
} device_t;

static device_t devices[MAX_DEVICES] = { 0 };

// Keyboard LED control
static uint8_t kbd_leds = 0;
static uint8_t prev_kbd_leds = 0xFF;

// check if device is Nintendo Switch
static inline bool is_switch(uint8_t dev_addr)
{
  uint16_t vid = devices[dev_addr].vid;
  uint16_t pid = devices[dev_addr].pid;

  return ((vid == 0x057e && (
           pid == 0x2009 || // Nintendo Switch Pro
           pid == 0x200e    // JoyCon Charge Grip
         )));
}

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+

// If your host terminal support ansi escape code such as TeraTerm
// it can be use to simulate mouse cursor movement within terminal
#define USE_ANSI_ESCAPE   0

// Uncomment the following line if you desire button-swap when middle button is clicd:
// #define MID_BUTTON_SWAPPABLE  true

// Button swap functionality
// -------------------------
#ifdef MID_BUTTON_SWAPPABLE
const bool buttons_swappable = true;
#else
const bool buttons_swappable = false;
#endif

static bool buttons_swapped = false;

// Core functionality
// ------------------
static uint8_t const keycode2ascii[128][2] =  { HID_KEYCODE_TO_ASCII };

uint32_t buttons;
uint8_t local_x;
uint8_t local_y;

int16_t spinner = 0;
uint16_t tpadLastPos = 0;
bool tpadDragging = false;

static void process_kbd_report(uint8_t dev_addr, uint8_t instance, hid_keyboard_report_t const *report);
static void process_mouse_report(uint8_t dev_addr, uint8_t instance, hid_mouse_report_t const * report);
static void process_gamepad_report(uint8_t dev_addr, uint8_t instance, hid_gamepad_report_t const *report);
static void process_generic_report(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len);

extern void __not_in_flash_func(post_globals)(
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
extern void __not_in_flash_func(post_mouse_globals)(uint8_t dev_addr, int8_t instance, uint16_t buttons, uint8_t delta_x, uint8_t delta_y, uint8_t spinner);
extern int __not_in_flash_func(find_player_index)(int device_address, int instance_number);
extern void remove_players_by_address(int device_address, int instance);

extern bool is_fun;
extern unsigned char fun_inc;
extern unsigned char fun_player;

bool switch_send_command(uint8_t dev_addr, uint8_t instance, uint8_t *data, uint8_t len) {
  uint8_t buf[8 + len];
  buf[0] = 0x80; // PROCON_REPORT_SEND_USB
  buf[1] = 0x92; // PROCON_USB_DO_CMD
  buf[2] = 0x00;
  buf[3] = 0x31;
  buf[4] = 0x00;
  buf[5] = 0x00;
  buf[6] = 0x00;
  buf[7] = 0x00;

  memcpy(buf + 8, data, len);

  tuh_hid_send_report(dev_addr, instance, buf[0], &(buf[0])+1, sizeof(buf) - 1);
}

void hid_app_init() {
  register_devices();
}

void hid_app_task(uint8_t rumble, uint8_t leds)
{
  const uint32_t interval_ms = 200;
  static uint32_t start_ms_ds5 = 0;
  static uint32_t start_ms_nsw = 0;

  if (is_fun) {
    fun_inc++;
    if (!fun_inc) {
      fun_player = ++fun_player%0x20;
    }
  }

  // iterate devices and instances that can receive responses
  for(uint8_t dev_addr=1; dev_addr<MAX_DEVICES; dev_addr++)
  {
    for(uint8_t instance=0; instance<CFG_TUH_HID; instance++)
    {
      int player_index = find_player_index(dev_addr, instance);
      if (player_index >= 0)
      {
        switch (devices[dev_addr].instances[instance].type)
        {
        case CONTROLLER_DUALSHOCK3: // send DS3 Init, LED and rumble responses
          device_interfaces[CONTROLLER_DUALSHOCK3]->task(dev_addr, instance, player_index, rumble);
          break;
        case CONTROLLER_DUALSHOCK4: // send DS4 LED and rumble response
          device_interfaces[CONTROLLER_DUALSHOCK4]->task(dev_addr, instance, player_index, rumble);
          break;
        case CONTROLLER_DUALSENSE: // send DS5 LED and rumble response
          device_interfaces[CONTROLLER_DUALSENSE]->task(dev_addr, instance, player_index, rumble);
          break;
        default:
          break;
        }
      }

      // Nintendo Switch Pro/JoyCons Charging Grip initialization and subcommands (rumble|leds)
      // See: https://github.com/Dan611/hid-procon/
      //      https://github.com/felis/USB_Host_Shield_2.0/
      //      https://github.com/nicman23/dkms-hid-nintendo/
      //      https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering/blob/master/USB-HID-Notes.md
      if (devices[dev_addr].instances[instance].type == CONTROLLER_SWITCH &&
          devices[dev_addr].instances[instance].switch_conn_ack)
      {
        // set the faster baud rate
        // if (!devices[dev_addr].instances[instance].switch_baud) {
        //   devices[dev_addr].instances[instance].switch_baud = true;

        //   printf("SWITCH[%d|%d]: Baud\r\n", dev_addr, instance);
        //   uint8_t buf2[1] = { 0x03 /* PROCON_USB_BAUD */ };
        //   tuh_hid_send_report(dev_addr, instance, 0x80, buf2, sizeof(buf2));

        // // wait for baud ask and then send init handshake
        // } else
        if (!devices[dev_addr].instances[instance].switch_handshake/* && devices[dev_addr].instances[instance].switch_baud_ack*/) {
          devices[dev_addr].instances[instance].switch_handshake = true;

          printf("SWITCH[%d|%d]: Handshake\r\n", dev_addr, instance);
          uint8_t buf1[1] = { 0x02 /* PROCON_USB_HANDSHAKE */ };
          tuh_hid_send_report(dev_addr, instance, 0x80, buf1, sizeof(buf1));

        // wait for handshake ack and then send USB enable mode
        } else if (!devices[dev_addr].instances[instance].switch_usb_enable && devices[dev_addr].instances[instance].switch_handshake_ack) {
          devices[dev_addr].instances[instance].switch_usb_enable = true;

          printf("SWITCH[%d|%d]: Enable USB\r\n", dev_addr, instance);
          uint8_t buf3[1] = { 0x04 /* PROCON_USB_ENABLE */ };
          tuh_hid_send_report(dev_addr, instance, 0x80, buf3, sizeof(buf3));

        // wait for usb enabled acknowledgment
        } else if (devices[dev_addr].instances[instance].switch_usb_enable_ack) {
          // SWITCH SUB-COMMANDS
          //
          // Based on: https://github.com/Dan611/hid-procon
          //           https://github.com/nicman23/dkms-hid-nintendo
          //
          uint8_t data[14] = { 0 };
          data[0x00] = 0x01; // Report ID - PROCON_CMD_AND_RUMBLE

          if (!devices[dev_addr].instances[instance].switch_home_led) {
            devices[dev_addr].instances[instance].switch_home_led = true;

            // It is possible set up to 15 mini cycles, but we simply just set the LED constantly on after momentary off.
            // See: https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering/blob/master/bluetooth_hid_subcommands_notes.md#subcommand-0x38-set-home-light
            data[0x01] = output_sequence_counter++; // Lowest 4-bit is a sequence number, which needs to be increased for every report

            data[0x0A + 0] = 0x38; // PROCON_CMD_LED_HOME
            data[0x0A + 1] = (0 /* Number of cycles */ << 4) | (true ? 0xF : 0) /* Global mini cycle duration */;
            data[0x0A + 2] = (0x1 /* LED start intensity */ << 4) | 0x0 /* Number of full cycles */;
            data[0x0A + 3] = (0x0 /* Mini Cycle 1 LED intensity */ << 4) | 0x1 /* Mini Cycle 2 LED intensity */;

            switch_send_command(dev_addr, instance, data, 10 + 4);

          } else if (devices[dev_addr].instances[instance].switch_command_ack) {
            player_index = find_player_index(dev_addr, devices[dev_addr].instance_count == 1 ? instance : devices[dev_addr].instance_root);

            if (devices[dev_addr].instances[instance].switch_player_led_set != player_index || is_fun) {
              devices[dev_addr].instances[instance].switch_player_led_set = player_index;

              // See: https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering/blob/master/bluetooth_hid_subcommands_notes.md#subcommand-0x30-set-player-lights
              data[0x01] = output_sequence_counter++; // Lowest 4-bit is a sequence number, which needs to be increased for every report

              data[0x0A + 0] = 0x30; // PROCON_CMD_LED

              // led player indicator
              switch (player_index+1)
              {
              case 1:
              case 2:
              case 3:
              case 4:
              case 5:
                data[0x0A + 1] = PLAYER_LEDS[player_index+1];
                break;

              default: // unassigned
                // turn all leds on
                data[0x0A + 1] = 0x0f;
                break;
              }

              // fun
              if (player_index+1 && is_fun) {
                data[0x0A + 1] = (fun_inc & 0b00001111);
              }

              devices[dev_addr].instances[instance].switch_command_ack = false;
              switch_send_command(dev_addr, instance, data, 10 + 2);
            } else if (rumble != last_rumble) {
              uint8_t buf[10] = { 0 };
              buf[0x00] = 0x10; // Report ID - PROCON_CMD_RUMBLE_ONLY
              buf[0x01] = output_sequence_counter++; // Lowest 4-bit is a sequence number, which needs to be increased for every report
              
              // // Snippet values from https://github.com/DanielOgorchock/linux/blob/7811b8f1f00ee9f195b035951749c57498105d52/drivers/hid/hid-nintendo.c#L197
              // // joycon_rumble_frequencies.freq = { 0x2000, 0x28,   95 }
              // uint16_t freq_data_high_high = 0x2000;
              // uint8_t freq_data_low_low = 0x28;
              // // joycon_rumble_amplitudes.amp = { 0x78, 0x005e,  422 }
              // uint8_t amp_data_high = 0x78;
              // uint16_t amp_data_low = 0x005e;
              // printf("0x%x 0x%x 0x%x 0x%x\n\n", (freq_data_high_high >> 8) & 0xFF, (freq_data_high_high & 0xFF) + amp_data_high, freq_data_low_low + ((amp_data_low >> 8) & 0xFF), amp_data_low & 0xFF);

              if (rumble) {
                // Left rumble ON data
                buf[0x02 + 0] = 0x20;
                buf[0x02 + 1] = 0x78;
                buf[0x02 + 2] = 0x28;
                buf[0x02 + 3] = 0x5e;
                // buf[0x02 + 0] = (freq_data_high_high >> 8) & 0xFF;
                // buf[0x02 + 1] = (freq_data_high_high & 0xFF) + amp_data_high;
                // buf[0x02 + 2] = freq_data_low_low + ((amp_data_low >> 8) & 0xFF);
                // buf[0x02 + 3] = amp_data_low & 0xFF;

                // Right rumble ON data
                buf[0x02 + 4] = 0x20;
                buf[0x02 + 5] = 0x78;
                buf[0x02 + 6] = 0x28;
                buf[0x02 + 7] = 0x5e;
                // buf[0x02 + 4] = (freq_data_high_high >> 8) & 0xFF;
                // buf[0x02 + 5] = (freq_data_high_high & 0xFF) + amp_data_high;
                // buf[0x02 + 6] = freq_data_low_low + ((amp_data_low >> 8) & 0xFF);
                // buf[0x02 + 7] = amp_data_low & 0xFF;
              } else {
                // Left rumble OFF data
                buf[0x02 + 0] = 0x00;
                buf[0x02 + 1] = 0x01;
                buf[0x02 + 2] = 0x40;
                buf[0x02 + 3] = 0x40;

                // Right rumble OFF data
                buf[0x02 + 4] = 0x00;
                buf[0x02 + 5] = 0x01;
                buf[0x02 + 6] = 0x40;
                buf[0x02 + 7] = 0x40;
              }
              last_rumble = rumble;
              switch_send_command(dev_addr, instance, buf, 10);
            }
          }
        }
      }

      // keyboard LED
      if (devices[dev_addr].instances[instance].type == CONTROLLER_KEYBOARD) {
        if (!devices[dev_addr].instances[instance].kbd_init && devices[dev_addr].instances[instance].kbd_ready) {
          devices[dev_addr].instances[instance].kbd_init = true;

          // kbd_leds = KEYBOARD_LED_NUMLOCK;
          tuh_hid_set_report(dev_addr, instance, 0, HID_REPORT_TYPE_OUTPUT, &kbd_leds, sizeof(kbd_leds));
        }
        else if (leds != last_leds) {
          if (leds & 0x1) kbd_leds |= KEYBOARD_LED_NUMLOCK;
          else kbd_leds &= ~KEYBOARD_LED_NUMLOCK;
          if (leds & 0x2) kbd_leds |= KEYBOARD_LED_CAPSLOCK;
          else kbd_leds &= ~KEYBOARD_LED_CAPSLOCK;
          if (leds & 0x4) kbd_leds |= KEYBOARD_LED_SCROLLLOCK;
          else kbd_leds &= ~KEYBOARD_LED_SCROLLLOCK;
          tuh_hid_set_report(dev_addr, instance, 0, HID_REPORT_TYPE_OUTPUT, &kbd_leds, sizeof(kbd_leds));
          last_leds = leds;
        }
        if (rumble != last_rumble) {
          if (rumble)
          {
            kbd_leds |= KEYBOARD_LED_CAPSLOCK | KEYBOARD_LED_SCROLLLOCK | KEYBOARD_LED_NUMLOCK;
          } else {
            kbd_leds = 0; // kbd_leds &= ~KEYBOARD_LED_CAPSLOCK;
          }
          last_rumble = rumble;

          if (kbd_leds != prev_kbd_leds) {
            tuh_hid_set_report(dev_addr, instance, 0, HID_REPORT_TYPE_OUTPUT, &kbd_leds, sizeof(kbd_leds));
            prev_kbd_leds = kbd_leds;
          }
        }
      }

      // GameCube WiiU Adapter Rumble
      if (devices[dev_addr].instances[instance].type == CONTROLLER_GAMECUBE) {
        device_interfaces[CONTROLLER_GAMECUBE]->task(dev_addr, instance, player_index, rumble);
      }
    }
  }
}

// Gets HID descriptor report item for specific ReportID
static inline bool USB_GetHIDReportItemInfoWithReportId(const uint8_t *ReportData, HID_ReportItem_t *const ReportItem)
{
  if (HID_DEBUG) printf("ReportID: %d ", ReportItem->ReportID);
  if (ReportItem->ReportID)
  {
    if (ReportItem->ReportID != ReportData[0])
      return false;

    ReportData++;
  }
  return USB_GetHIDReportItemInfo(ReportItem->ReportID, ReportData, ReportItem);
}

// Parses HID descriptor into byteIndex/buttonMasks
void parse_hid_descriptor(uint8_t dev_addr, uint8_t instance)
{
  HID_ReportItem_t *item = info->FirstReportItem;
  //iterate filtered reports info to match report from data
  uint8_t btns_count = 0;
  while (item)
  {
    uint8_t midValue = (item->Attributes.Logical.Maximum - item->Attributes.Logical.Minimum) / 2;
    uint8_t bitSize = item->Attributes.BitSize ? item->Attributes.BitSize : 0; // bits per usage
    uint8_t bitOffset = item->BitOffset ? item->BitOffset : 0; // bits offset from start
    uint16_t bitMask = ((0xFFFF >> (16 - bitSize)) << bitOffset % 8); // usage bits byte mask
    uint8_t byteIndex = (int)(bitOffset / 8); // usage start byte

    if (HID_DEBUG) {
      printf("minimum: %d ", item->Attributes.Logical.Minimum);
      printf("mid: %d ", midValue);
      printf("maximum: %d ", item->Attributes.Logical.Maximum);
      printf("bitSize: %d ", bitSize);
      printf("bitOffset: %d ", bitOffset);
      printf("bitMask: 0x%x ", bitMask);
      printf("byteIndex: %d ", byteIndex);
    }
    // TODO: this is limiting to repordId 0..
    // Need to parse reportId and match later with received reports.
    // Also helpful if multiple reportId maps can be saved per instance and report as individual
    // players for single instance HID reports that contain multiple reportIds.
    //
    uint8_t report[1] = {0}; // reportId = 0; original ex maps report to descriptor data structure
    if (USB_GetHIDReportItemInfoWithReportId(report, item))
    {
      if (HID_DEBUG) printf("PAGE: %d ", item->Attributes.Usage.Page);
      switch (item->Attributes.Usage.Page)
      {
      case HID_USAGE_PAGE_DESKTOP:
        switch (item->Attributes.Usage.Usage)
        {
        case HID_USAGE_DESKTOP_X: // Left Analog X
        {
          if (HID_DEBUG) printf(" HID_USAGE_DESKTOP_X ");
          devices[dev_addr].instances[instance].xLoc.byteIndex = byteIndex;
          devices[dev_addr].instances[instance].xLoc.bitMask = bitMask;
          devices[dev_addr].instances[instance].xLoc.mid = midValue;
          break;
        }
        case HID_USAGE_DESKTOP_Y: // Left Analog Y
        {
          if (HID_DEBUG) printf(" HID_USAGE_DESKTOP_Y ");
          devices[dev_addr].instances[instance].yLoc.byteIndex = byteIndex;
          devices[dev_addr].instances[instance].yLoc.bitMask = bitMask;
          devices[dev_addr].instances[instance].yLoc.mid = midValue;
          break;
        }
        case HID_USAGE_DESKTOP_Z: // Right Analog X
        {
          if (HID_DEBUG) printf(" HID_USAGE_DESKTOP_Z ");
          devices[dev_addr].instances[instance].zLoc.byteIndex = byteIndex;
          devices[dev_addr].instances[instance].zLoc.bitMask = bitMask;
          devices[dev_addr].instances[instance].zLoc.mid = midValue;
          break;
        }
        case HID_USAGE_DESKTOP_RZ: // Right Analog Y
        {
          if (HID_DEBUG) printf(" HID_USAGE_DESKTOP_RZ ");
          devices[dev_addr].instances[instance].rzLoc.byteIndex = byteIndex;
          devices[dev_addr].instances[instance].rzLoc.bitMask = bitMask;
          devices[dev_addr].instances[instance].rzLoc.mid = midValue;
          break;
        }
        case HID_USAGE_DESKTOP_HAT_SWITCH:
          if (HID_DEBUG) printf(" HID_USAGE_DESKTOP_HAT_SWITCH ");
          devices[dev_addr].instances[instance].hatLoc.byteIndex = byteIndex;
          devices[dev_addr].instances[instance].hatLoc.bitMask = bitMask;

          break;
        // case HID_USAGE_DESKTOP_DPAD_UP:
        //   current.up |= 1;
        //   break;
        // case HID_USAGE_DESKTOP_DPAD_RIGHT:
        //   current.right |= 1;
        //   break;
        // case HID_USAGE_DESKTOP_DPAD_DOWN:
        //   current.down |= 1;
        //   break;
        // case HID_USAGE_DESKTOP_DPAD_LEFT:
        //   current.left |= 1;
        //   break;
        }
        break;
      case HID_USAGE_PAGE_BUTTON:
      {
        if (HID_DEBUG) printf(" HID_USAGE_PAGE_BUTTON ");
        uint8_t usage = item->Attributes.Usage.Usage;

        if (usage >= 1 && usage <= MAX_BUTTONS) {
          devices[dev_addr].instances[instance].buttonLoc[usage - 1].byteIndex = byteIndex;
          devices[dev_addr].instances[instance].buttonLoc[usage - 1].bitMask = bitMask;
        }
        btns_count++;
      }
      break;
      }
    }
    item = item->Next;
    if (HID_DEBUG) printf("\n\n");
  }

  devices[dev_addr].instances[instance].buttonCnt = btns_count;
}

bool isKnownController(uint8_t dev_addr) {

  if (is_switch(dev_addr)    ) {
    if (devices[dev_addr].pid == 0x200e) {
      printf("DEVICE:[Switch JoyCon Charging Grip]\n");
    } else {
      printf("DEVICE:[Switch Pro Controller]\n");
    }
    return true;
  }
  else {
    for (int i = 0; i < CONTROLLER_TYPE_COUNT; i++) {
      if (device_interfaces[i] &&
          device_interfaces[i]->is_device(devices[dev_addr].vid, devices[dev_addr].pid)) {
        printf("DEVICE:[%s]\n", device_interfaces[i]->name);
        return true;
      }
    }    
  }

  printf("DEVICE:[UKNOWN]\n");
  return false;
}

//--------------------------------------------------------------------+
// TinyUSB Callbacks
//--------------------------------------------------------------------+

// Invoked when device with hid interface is mounted
// Report descriptor is also available for use. tuh_hid_parse_report_descriptor()
// can be used to parse common/simple enough descriptor.
// Note: if report descriptor length > CFG_TUH_ENUMERATION_BUFSIZE, it will be skipped
// therefore report_desc = NULL, desc_len = 0
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len)
{
  (void)desc_report;
  (void)desc_len;
  uint16_t vid, pid;
  tuh_vid_pid_get(dev_addr, &vid, &pid);

  printf("HID device address = %d, instance = %d is mounted\r\n", dev_addr, instance);
  printf("VID = %04x, PID = %04x\r\n", vid, pid);

  // Stash device vid/pid/serial device type detection
  devices[dev_addr].vid = vid;
  devices[dev_addr].pid = pid;
  if ((++devices[dev_addr].instance_count) == 1) {
    devices[dev_addr].instance_root = instance; // save initial root instance to merge extras into
  }

  // Interface protocol (hid_interface_protocol_enum_t)
  const char* protocol_str[] = { "None", "Keyboard", "Mouse" };
  uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

  printf("HID Interface Protocol = %s\r\n", protocol_str[itf_protocol]);

  // By default host stack will use activate boot protocol on supported interface.
  // Therefore for this simple example, we only need to parse generic report descriptor (with built-in parser)
  bool isKnownCtrlr = isKnownController(dev_addr);
  printf("mapped: %d, dev: %d, instance: %d\n", isKnownCtrlr?1:0, dev_addr, instance);

  // Set device type and defaults
  if (device_interfaces[CONTROLLER_DUALSHOCK3]->is_device(vid, pid))
  {
    device_interfaces[CONTROLLER_DUALSHOCK3]->init(dev_addr, instance);
    devices[dev_addr].instances[instance].type = CONTROLLER_DUALSHOCK3;
  }
  else if (device_interfaces[CONTROLLER_DUALSHOCK4]->is_device(vid, pid))
  {
    devices[dev_addr].instances[instance].type = CONTROLLER_DUALSHOCK4;
    devices[dev_addr].instances[instance].motor_left = 0;
    devices[dev_addr].instances[instance].motor_right = 0;
  }
  else if (device_interfaces[CONTROLLER_DUALSENSE]->is_device(vid, pid))
  {
    devices[dev_addr].instances[instance].type = CONTROLLER_DUALSENSE;
    devices[dev_addr].instances[instance].motor_left = 0;
    devices[dev_addr].instances[instance].motor_right = 0;
  }
  else if (is_switch(dev_addr))
  {
    devices[dev_addr].instances[instance].type = CONTROLLER_SWITCH;
    printf("SWITCH[%d|%d]: Mounted\r\n", dev_addr, instance);
  }
  else if (device_interfaces[CONTROLLER_GAMECUBE]->is_device(vid, pid))
  {
    devices[dev_addr].instances[instance].type = CONTROLLER_GAMECUBE;
    devices[dev_addr].instances[instance].motor_left = 0;
    devices[dev_addr].instances[instance].motor_right = 0;
  }
  else if (itf_protocol == HID_ITF_PROTOCOL_KEYBOARD)
  {
    devices[dev_addr].instances[instance].type = CONTROLLER_KEYBOARD;
    devices[dev_addr].instances[instance].kbd_ready = false;
    devices[dev_addr].instances[instance].kbd_init = false;
  }
  else {
    devices[dev_addr].instances[instance].type = CONTROLLER_DINPUT;
    devices[dev_addr].instances[instance].kbd_ready = false;
    devices[dev_addr].instances[instance].kbd_init = false;
  }

  if (!isKnownCtrlr && itf_protocol != HID_ITF_PROTOCOL_KEYBOARD)
  {
    if (itf_protocol == HID_ITF_PROTOCOL_NONE) {
      devices[dev_addr].instances[instance].report_count = tuh_hid_parse_report_descriptor(devices[dev_addr].instances[instance].report_info, MAX_REPORT, desc_report, desc_len);
      printf("HID has %u reports \r\n", devices[dev_addr].instances[instance].report_count);
    }

    // hid_parser
    uint8_t ret = USB_ProcessHIDReport(dev_addr, instance, desc_report, desc_len, &(info));
    if(ret == HID_PARSE_Successful)
    {
      parse_hid_descriptor(dev_addr, instance);
    }
    else
    {
      printf("Error: USB_ProcessHIDReport failed: %d\r\n", ret);
    }
    USB_FreeReportInfo(info);
    info = NULL;
  }

  // gets serial for discovering some devices
  // uint16_t temp_buf[128];
  // if (0 == tuh_descriptor_get_serial_string_sync(dev_addr, LANGUAGE_ID, temp_buf, sizeof(temp_buf)))
  // {
  //   for(int i=0; i<20; i++){
  //     devices[dev_addr].serial[i] = temp_buf[i];
  //   }
  // }

  // request to receive report
  // tuh_hid_report_received_cb() will be invoked when report is available
  if ( !tuh_hid_receive_report(dev_addr, instance) )
  {
    printf("Error: cannot request to receive report\r\n");
  }
}

// resets default values in case devices are hotswapped
void switch_reset(uint8_t dev_addr, uint8_t instance)
{
  printf("SWITCH[%d|%d]: Unmount Reset\r\n", dev_addr, instance);
  devices[dev_addr].instances[instance].switch_conn_ack = false;
  devices[dev_addr].instances[instance].switch_baud = false;
  devices[dev_addr].instances[instance].switch_baud_ack = false;
  devices[dev_addr].instances[instance].switch_handshake = false;
  devices[dev_addr].instances[instance].switch_handshake_ack = false;
  devices[dev_addr].instances[instance].switch_usb_enable = false;
  devices[dev_addr].instances[instance].switch_usb_enable_ack = false;
  devices[dev_addr].instances[instance].switch_home_led = false;
  devices[dev_addr].instances[instance].switch_command_ack = false;
  devices[dev_addr].instances[instance].switch_player_led_set = 0;
}

// resets default values in case devices are hotswapped
void hid_reset(uint8_t dev_addr, uint8_t instance)
{
  printf("HID[%d|%d]: Unmount Reset\r\n", dev_addr, instance);
  devices[dev_addr].instances[instance].xLoc.byteIndex = 0;
  devices[dev_addr].instances[instance].xLoc.bitMask = 0;
  devices[dev_addr].instances[instance].xLoc.mid = 0;
  devices[dev_addr].instances[instance].yLoc.byteIndex = 0;
  devices[dev_addr].instances[instance].yLoc.bitMask = 0;
  devices[dev_addr].instances[instance].yLoc.mid = 0;
  devices[dev_addr].instances[instance].zLoc.byteIndex = 0;
  devices[dev_addr].instances[instance].zLoc.bitMask = 0;
  devices[dev_addr].instances[instance].zLoc.mid = 0;
  devices[dev_addr].instances[instance].rzLoc.byteIndex = 0;
  devices[dev_addr].instances[instance].rzLoc.bitMask = 0;
  devices[dev_addr].instances[instance].rzLoc.mid = 0;
  devices[dev_addr].instances[instance].hatLoc.byteIndex = 0;
  devices[dev_addr].instances[instance].hatLoc.bitMask = 0;
  devices[dev_addr].instances[instance].buttonCnt = 0;
  for (int i = 0; i < MAX_BUTTONS; i++) {
    devices[dev_addr].instances[instance].buttonLoc[i].byteIndex = 0;
    devices[dev_addr].instances[instance].buttonLoc[i].bitMask = 0;
  }
}

// Invoked when device with hid interface is un-mounted
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
{
  printf("HID device address = %d, instance = %d is unmounted\r\n", dev_addr, instance);

  // Reset Switch Config
  if (devices[dev_addr].instances[instance].type == CONTROLLER_SWITCH) {
    switch_reset(dev_addr, instance);
  }
  // Reset HID Config
  else if (devices[dev_addr].instances[instance].type == CONTROLLER_DINPUT) {
    hid_reset(dev_addr, instance);
  }

  if (devices[dev_addr].instance_count > 0) {
    devices[dev_addr].instance_count--;
  } else {
    devices[dev_addr].instance_count = 0;
  }

  devices[dev_addr].instances[instance].type = CONTROLLER_DINPUT;
}

bool switch_diff_report(switch_report_t const* rpt1, switch_report_t const* rpt2)
{
  bool result;

  // x, y, z, rz must different than 2 to be counted
  result = diff_than_n(rpt1->left_x, rpt2->left_x, 4) || diff_than_n(rpt1->left_y, rpt2->left_y, 4) ||
           diff_than_n(rpt1->right_x, rpt2->right_x, 4) || diff_than_n(rpt1->right_y, rpt2->right_y, 4);

  // check the reset with mem compare (everything but the sticks)
  result |= memcmp(&rpt1->battery_level_and_connection_info + 1, &rpt2->battery_level_and_connection_info + 1, 3);
  result |= memcmp(&rpt1->subcommand_ack, &rpt2->subcommand_ack, 36);

  return result;
}

void print_report(switch_report_01_t* report, uint32_t length) {
    printf("Bytes: ");
    for(uint32_t i = 0; i < length; i++) {
        printf("%02X ", report->buf[i]);
    }
    printf("\n");
}

uint8_t byteScaleSwitchAnalog(uint16_t switch_val) {
    // If the input is zero, then output min value of 1
    if (switch_val == 0) {
        return 1;
    }

    // Otherwise, scale the switch value from [1, 4095] to [1, 255]
    return 1 + ((switch_val - 1) * 255) / 4095;
}

void process_switch(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
  // previous report used to compare for changes
  static switch_report_t prev_report[5][5];

  switch_report_t update_report;
  memcpy(&update_report, report, sizeof(update_report));

  if (update_report.report_id == 0x30) { // Switch Controller Report

    devices[dev_addr].instances[instance].switch_usb_enable_ack = true;

    update_report.left_x = (update_report.left_stick[0] & 0xFF) | ((update_report.left_stick[1] & 0x0F) << 8);
    update_report.left_y = ((update_report.left_stick[1] & 0xF0) >> 4) | ((update_report.left_stick[2] & 0xFF) << 4);
    update_report.right_x = (update_report.right_stick[0] & 0xFF) | ((update_report.right_stick[1] & 0x0F) << 8);
    update_report.right_y = ((update_report.right_stick[1] & 0xF0) >> 4) | ((update_report.right_stick[2] & 0xFF) << 4);

    if (switch_diff_report(&prev_report[dev_addr-1][instance], &update_report)) {

      printf("SWITCH[%d|%d]: Report ID = 0x%x\r\n", dev_addr, instance, update_report.report_id);
      printf("(lx, ly, rx, ry) = (%u, %u, %u, %u)\r\n", update_report.left_x, update_report.left_y, update_report.right_x, update_report.right_y);
      printf("DPad = ");

      if (update_report.down) printf("Down ");
      if (update_report.up) printf("Up ");
      if (update_report.right) printf("Right ");
      if (update_report.left ) printf("Left ");

      printf("; Buttons = ");
      if (update_report.y) printf("Y ");
      if (update_report.b) printf("B ");
      if (update_report.a) printf("A ");
      if (update_report.x) printf("X ");
      if (update_report.l) printf("L ");
      if (update_report.r) printf("R ");
      if (update_report.zl) printf("ZL ");
      if (update_report.zr) printf("ZR ");
      if (update_report.lstick) printf("LStick ");
      if (update_report.rstick) printf("RStick ");
      if (update_report.select) printf("Select ");
      if (update_report.start) printf("Start ");
      if (update_report.home) printf("Home ");
      if (update_report.cap) printf("Cap ");
      if (update_report.sr_r) printf("sr_r ");
      if (update_report.sl_l) printf("sl_l ");
      printf("\r\n");

      bool has_6btns = true;
      int threshold = 256;
      bool dpad_up    = update_report.up;
      bool dpad_right = update_report.right;
      bool dpad_down  = update_report.down;
      bool dpad_left  = update_report.left;
      bool bttn_1 = update_report.a;
      bool bttn_2 = update_report.b;
      bool bttn_3 = update_report.x;
      bool bttn_4 = update_report.y;
      bool bttn_5 = update_report.l;
      bool bttn_6 = update_report.r;
      bool bttn_run = update_report.start;
      bool bttn_sel = update_report.select || update_report.zl || update_report.zr;
      bool bttn_home = update_report.home;

      uint8_t leftX = 0;
      uint8_t leftY = 0;
      uint8_t rightX = 0;
      uint8_t rightY = 0;

      if (devices[dev_addr].pid == 0x200e) { // is_joycon_grip
        bool is_left_joycon = (!update_report.right_x && !update_report.right_y);
        bool is_right_joycon = (!update_report.left_x && !update_report.left_y);
        if (is_left_joycon) {
          dpad_up    = update_report.up;
          dpad_right = update_report.right;
          dpad_down  = update_report.down;
          dpad_left  = update_report.left;
          bttn_5 = update_report.l;
          bttn_run = false;

          leftX = byteScaleSwitchAnalog(update_report.left_x + 127);
          leftY = byteScaleSwitchAnalog(update_report.left_y - 127);
        }
        else if (is_right_joycon) {
          dpad_up    = false; // (right_stick_y > (2048 + threshold));
          dpad_right = false; // (right_stick_x > (2048 + threshold));
          dpad_down  = false; // (right_stick_y < (2048 - threshold));
          dpad_left  = false; // (right_stick_x < (2048 - threshold));
          bttn_home = false;

          rightX = byteScaleSwitchAnalog(update_report.right_x);
          rightY = byteScaleSwitchAnalog(update_report.right_y + 127);
        }
      } else {
        leftX = byteScaleSwitchAnalog(update_report.left_x);
        leftY = byteScaleSwitchAnalog(update_report.left_y);
        rightX = byteScaleSwitchAnalog(update_report.right_x);
        rightY = byteScaleSwitchAnalog(update_report.right_y);
      }

      buttons = (
        ((update_report.rstick) ? 0x00 : 0x20000) |
        ((update_report.lstick) ? 0x00 : 0x10000) |
        ((bttn_6)     ? 0x00 : 0x8000) | // VI
        ((bttn_5)     ? 0x00 : 0x4000) | // V
        ((bttn_4)     ? 0x00 : 0x2000) | // IV
        ((bttn_3)     ? 0x00 : 0x1000) | // III
        ((has_6btns)  ? 0x00 : 0x0800) |
        ((bttn_home)  ? 0x00 : 0x0400) | // home
        ((update_report.sr_r) ? 0x00 : 0x0200) | // r2
        ((update_report.sr_l) ? 0x00 : 0x0100) | // l2
        ((dpad_left)  ? 0x00 : 0x0008) |
        ((dpad_down)  ? 0x00 : 0x0004) |
        ((dpad_right) ? 0x00 : 0x0002) |
        ((dpad_up)    ? 0x00 : 0x0001) |
        ((bttn_run)   ? 0x00 : 0x0080) | // Run
        ((bttn_sel)   ? 0x00 : 0x0040) | // Select
        ((bttn_2)     ? 0x00 : 0x0020) | // II
        ((bttn_1)     ? 0x00 : 0x0010)   // I
      );

      // add to accumulator and post to the state machine
      // if a scan from the host machine is ongoing, wait
      bool is_root = instance == devices[dev_addr].instance_root;
      post_globals(dev_addr, is_root ? instance : -1, buttons, leftX, leftY, rightX, rightY, 0, 0, 0, 0);

      prev_report[dev_addr-1][instance] = update_report;
    }

  // process input reports for events and command acknowledgments
  } else {

    switch_report_01_t state_report;
    memcpy(&state_report, report, sizeof(state_report));

    // JC_INPUT_USB_RESPONSE (connection events & command acknowledgments)
    if (state_report.buf[0] == 0x81 && state_report.buf[1] == 0x01) { // JC_USB_CMD_CONN_STATUS
      if (state_report.buf[2] == 0x00) { // connect
        devices[dev_addr].instances[instance].switch_conn_ack = true;
      } else if (state_report.buf[2] == 0x03) { // disconnect
        switch_reset(dev_addr, instance);
        remove_players_by_address(dev_addr, instance);
      }
    }
    else if (state_report.buf[0] == 0x81 && state_report.buf[1] == 0x02) { // JC_USB_CMD_HANDSHAKE
      devices[dev_addr].instances[instance].switch_handshake_ack = true;
    }
    else if (state_report.buf[0] == 0x81 && state_report.buf[1] == 0x03) { // JC_USB_CMD_BAUDRATE_3M
      devices[dev_addr].instances[instance].switch_baud_ack = true;
    }
    else if (state_report.buf[0] == 0x81 && state_report.buf[1] == 0x92) { // command ack
      devices[dev_addr].instances[instance].switch_command_ack = true;
    }

    printf("SWITCH[%d|%d]: Report ID = 0x%x\r\n", dev_addr, instance, state_report.data.report_id);

    uint32_t length = sizeof(state_report.buf) / sizeof(state_report.buf[0]);
    print_report(&state_report, length);
  }
}

// Invoked when received report from device via interrupt endpoint
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
  uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);
  uint16_t vid = devices[dev_addr].vid;
  uint16_t pid = devices[dev_addr].pid;
  bool known = false;

  switch (itf_protocol)
  {
    case HID_ITF_PROTOCOL_KEYBOARD:
      TU_LOG2("HID receive boot keyboard report\r\n");
      process_kbd_report(dev_addr, instance, (hid_keyboard_report_t const*) report );
    break;

    case HID_ITF_PROTOCOL_MOUSE:
      TU_LOG2("HID receive boot mouse report\r\n");
      process_mouse_report(dev_addr, instance, (hid_mouse_report_t const*) report );
    break;

    default:
      if ( is_switch(dev_addr) ) {
        process_switch(dev_addr, instance, report, len);
        known = true;
      } else {
        for (int i = 0; i < CONTROLLER_TYPE_COUNT; i++) {
          if (device_interfaces[i] && device_interfaces[i]->is_device(vid, pid)) {
            device_interfaces[i]->process(dev_addr, instance, report, len);
            known = true;
            break;
          }
        }
      }

      if (!known) {
        // Generic report requires matching ReportID and contents with previous parsed report info
        process_generic_report(dev_addr, instance, report, len);
      }
      break;
  }

  // continue to request to receive report
  if ( !tuh_hid_receive_report(dev_addr, instance) )
  {
    printf("Error: cannot request to receive report\r\n");
  }
}

//--------------------------------------------------------------------+
// Keyboard
//--------------------------------------------------------------------+

void calculate_coordinates(uint32_t stick_keys, int intensity, uint8_t *x_value, uint8_t *y_value) {
  uint16_t angle_degrees = 0;
  uint8_t offset = (127.0 - ((intensity/100.0) * 127.0));

  if (stick_keys && intensity) {
    if (stick_keys <= 0x000f) {
      switch (stick_keys)
      {
      case 0x01: // W
          angle_degrees = 0;
          break;
      case 0x02: // S
          angle_degrees = 180;
          break;
      case 0x04: // A
          angle_degrees = 270;
          break;
      case 0x08: // D
          angle_degrees = 90;
          break;
      default:
          break;
      }
    } else if (stick_keys <= 0x00ff) {
      switch (stick_keys)
      {
      case 0x12: // S ⇾ W
          angle_degrees = 0;
          break;
      case 0x81: // W ⇾ D
      case 0x18: // D ⇾ W
          angle_degrees = 45;
          break;
      case 0x84: // A ⇾ D
          angle_degrees = 90;
          break;
      case 0x82: // S ⇾ D
      case 0x28: // D ⇾ S
          angle_degrees = 135;
          break;
      case 0x21: // W ⇾ S
          angle_degrees = 180;
          break;
      case 0x42: // S ⇾ A
      case 0x24: // A ⇾ S
          angle_degrees = 225;
          break;
      case 0x41: // W ⇾ A
      case 0x14: // A ⇾ W
          angle_degrees = 315;
          break;
      case 0x48: // D ⇾ A
          angle_degrees = 270;
          break;
      default:
          break;
      }
    } else if (stick_keys <= 0x0fff) {
      switch (stick_keys)
      {
      case 0x841: // W ⇾ A ⇾ D
      case 0x812: // S ⇾ W ⇾ D
      case 0x182: // S ⇾ D ⇾ W
      case 0x814: // A ⇾ W ⇾ D
      case 0x184: // A ⇾ D ⇾ W
      case 0x128: // D ⇾ S ⇾ W
          angle_degrees = 45;
          break;
      case 0x821: // W ⇾ S ⇾ D
      case 0x281: // W ⇾ D ⇾ S
      case 0x842: // S ⇾ A ⇾ D
      case 0x824: // A ⇾ S ⇾ D
      case 0x284: // A ⇾ D ⇾ S
      case 0x218: // D ⇾ W ⇾ S
          angle_degrees = 135;
          break;
      case 0x421: // W ⇾ S ⇾ A
      case 0x241: // W ⇾ A ⇾ S
      case 0x482: // S ⇾ D ⇾ A
      case 0x214: // A ⇾ W ⇾ S
      case 0x248: // D ⇾ A ⇾ S
          angle_degrees = 225;
          break;
      case 0x124: // A ⇾ S ⇾ W
      case 0x418: // D ⇾ W ⇾ A
      case 0x148: // D ⇾ A ⇾ W
      case 0x481: // W ⇾ D ⇾ A
      case 0x412: // S ⇾ W ⇾ A
      case 0x142: // S ⇾ A ⇾ W
          angle_degrees = 315;
          break;
      default:
          break;
      }
    } else if (stick_keys <= 0xffff) {
      switch (stick_keys)
      {
      case 0x8412: // S ⇾ W ⇾ A ⇾ D
      case 0x8142: // S ⇾ A ⇾ W ⇾ D
      case 0x1842: // S ⇾ A ⇾ D ⇾ W
      case 0x8124: // A ⇾ S ⇾ W ⇾ D
      case 0x1824: // A ⇾ S ⇾ D ⇾ W
      case 0x1284: // A ⇾ D ⇾ S ⇾ W
          angle_degrees = 45;
          break;
      case 0x8421: // W ⇾ S ⇾ A ⇾ D
      case 0x8241: // W ⇾ A ⇾ S ⇾ D
      case 0x2841: // W ⇾ A ⇾ D ⇾ S
      case 0x8214: // A ⇾ W ⇾ S ⇾ D
      case 0x2814: // A ⇾ W ⇾ D ⇾ S
      case 0x2184: // A ⇾ D ⇾ W ⇾ S
          angle_degrees = 135;
          break;
      case 0x2148: // D ⇾ A ⇾ W ⇾ S
      case 0x4821: // W ⇾ S ⇾ D ⇾ A
      case 0x4281: // W ⇾ D ⇾ S ⇾ A
      case 0x2481: // W ⇾ D ⇾ A ⇾ S
      case 0x4218: // D ⇾ W ⇾ S ⇾ A
      case 0x2418: // D ⇾ W ⇾ A ⇾ S
          angle_degrees = 225;
          break;
      case 0x4812: // S ⇾ W ⇾ D ⇾ A
      case 0x4182: // S ⇾ D ⇾ W ⇾ A
      case 0x1482: // S ⇾ D ⇾ A ⇾ W
      case 0x4128: // D ⇾ S ⇾ W ⇾ A
      case 0x1428: // D ⇾ S ⇾ A ⇾ W
      case 0x1248: // D ⇾ A ⇾ S ⇾ W
          angle_degrees = 315;
          break;
      default:
          break;
      }
    }
  }

  switch (angle_degrees)
  {
  case 0: // Up
    *x_value = 128;
    *y_value = 255 - offset;
    break;

  case 45: // Up + Right
    *x_value = 245 - offset;
    *y_value = 245 - offset;
    break;

  case 90: // Right
    *x_value = 255 - offset;
    *y_value = 128;
    break;

  case 135: // Down + Right
    *x_value = 245 - offset;
    *y_value = 11 + offset;
    break;

  case 180: // Down
    *x_value = 128;
    *y_value = 1 + offset;
    break;

  case 225: // Down + Left
    *x_value = 11 + offset;
    *y_value = 11 + offset;
    break;

  case 270: // Left
    *x_value = 1 + offset;
    *y_value = 128;
    break;

  case 315: // Up + Left
    *x_value = 11 + offset;
    *y_value = 245 - offset;
    break;

  default:
    break;
  }

  // printf("in: %d° %d%, x:%d, y:%d, keys: %x\n", angle_degrees, intensity, *x_value, *y_value, stick_keys);
  return;
}

// look up new key in previous keys
static inline bool find_key_in_report(hid_keyboard_report_t const *report, uint8_t keycode)
{
  for(uint8_t i=0; i<6; i++)
  {
    if (report->keycode[i] == keycode)  return true;
  }

  return false;
}

static void process_kbd_report(uint8_t dev_addr, uint8_t instance, hid_keyboard_report_t const *report)
{
  static hid_keyboard_report_t prev_report = { 0, 0, {0} }; // previous report to check key released

  uint8_t analog_left_x = 128;
  uint8_t analog_left_y = 128;
  uint8_t analog_right_x = 128;
  uint8_t analog_right_y = 128;
  uint8_t analog_l = 0;
  uint8_t analog_r = 0;
  bool has_6btns = true;
  bool dpad_left = false, dpad_down = false, dpad_right = false, dpad_up = false;
  bool btns_run = false, btns_sel = false, btns_one = false, btns_two = false,
       btns_three = false, btns_four = false, btns_five = false, btns_six = false,
       btns_home = false;

  uint32_t hatSwitchKeys = 0x0;
  uint32_t leftStickKeys = 0x0;
  uint32_t rightStickKeys = 0x0;
  uint8_t hatIndex = 0;
  uint8_t leftIndex = 0;
  uint8_t rightIndex = 0;

  bool const is_shift = report->modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT);
  bool const is_ctrl = report->modifier & (KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_RIGHTCTRL);
  bool const is_alt = report->modifier & (KEYBOARD_MODIFIER_LEFTALT | KEYBOARD_MODIFIER_RIGHTALT);

  // parse 3 keycode bytes into single word to return
  uint32_t reportKeys = report->keycode[0] | (report->keycode[1] << 8) | (report->keycode[2] << 16);
  if (report->modifier & (KEYBOARD_MODIFIER_LEFTSHIFT)) {
    reportKeys = reportKeys << 8 | HID_KEY_SHIFT_LEFT;
  } else if (report->modifier & (KEYBOARD_MODIFIER_RIGHTSHIFT)) {
    reportKeys = reportKeys << 8 | HID_KEY_SHIFT_RIGHT;
  }
  if (is_ctrl) {
    reportKeys = reportKeys << 8 | HID_KEY_CONTROL_LEFT;
  }
  if (is_alt) {
    reportKeys = reportKeys << 8 | HID_KEY_ALT_LEFT;
  }
  if (report->modifier & (KEYBOARD_MODIFIER_LEFTGUI)) {
    reportKeys = reportKeys << 8 | HID_KEY_GUI_LEFT;
  } else if (report->modifier & (KEYBOARD_MODIFIER_RIGHTGUI)) {
    reportKeys = reportKeys << 8 | HID_KEY_GUI_RIGHT;
  }

  // wait until first report before sending init led output report
  if (!devices[dev_addr].instances[instance].kbd_ready) {
    devices[dev_addr].instances[instance].kbd_ready = true;
  }

  //------------- example code ignore control (non-printable) key affects -------------//
  for(uint8_t i=0; i<6; i++)
  {
    if ( report->keycode[i] )
    {
      if (report->keycode[i] == HID_KEY_ESCAPE || report->keycode[i] == HID_KEY_EQUAL) btns_run = true; // Start
      if (report->keycode[i] == HID_KEY_P || report->keycode[i] == HID_KEY_MINUS) btns_sel = true; // Select / Z
#ifdef CONFIG_PCE
      // more ideal PCE enter button for SuperSD3 Menu
      if (report->keycode[i] == HID_KEY_J || report->keycode[i] == HID_KEY_ENTER) btns_two = true; // II
      if (report->keycode[i] == HID_KEY_K || report->keycode[i] == HID_KEY_BACKSPACE) btns_one = true; // I
#else
      if (report->keycode[i] == HID_KEY_J || report->keycode[i] == HID_KEY_ENTER) btns_one = true; // A
      if (report->keycode[i] == HID_KEY_K || report->keycode[i] == HID_KEY_BACKSPACE) btns_two = true; // B
#endif
      if (report->keycode[i] == HID_KEY_L) btns_three = true; // X
      if (report->keycode[i] == HID_KEY_SEMICOLON) btns_four = true; // Y
      if (report->keycode[i] == HID_KEY_U || report->keycode[i] == HID_KEY_PAGE_UP) btns_five = true; // L
      if (report->keycode[i] == HID_KEY_I || report->keycode[i] == HID_KEY_PAGE_DOWN) btns_six = true; // R

      // HAT SWITCH
      switch (report->keycode[i])
      {
      case HID_KEY_1:
      case HID_KEY_ARROW_UP:
          hatSwitchKeys |= (0x1 << (4 * hatIndex));
          hatIndex++;
          break;
      case HID_KEY_3:
      case HID_KEY_ARROW_DOWN:
          hatSwitchKeys |= (0x2 << (4 * hatIndex));
          hatIndex++;
          break;
      case HID_KEY_2:
      case HID_KEY_ARROW_LEFT:
          hatSwitchKeys |= (0x4 << (4 * hatIndex));
          hatIndex++;
          break;
      case HID_KEY_4:
      case HID_KEY_ARROW_RIGHT:
          hatSwitchKeys |= (0x8 << (4 * hatIndex));
          hatIndex++;
          break;
      default:
          break;
      }

      // LEFT STICK
      switch (report->keycode[i])
      {
      case HID_KEY_W:
          leftStickKeys |= (0x1 << (4 * leftIndex));
          leftIndex++;
          break;
      case HID_KEY_S:
          leftStickKeys |= (0x2 << (4 * leftIndex));
          leftIndex++;
          break;
      case HID_KEY_A:
          leftStickKeys |= (0x4 << (4 * leftIndex));
          leftIndex++;
          break;
      case HID_KEY_D:
          leftStickKeys |= (0x8 << (4 * leftIndex));
          leftIndex++;
          break;
      default:
          break;
      }

      // RIGHT STICK
      switch (report->keycode[i])
      {
      case HID_KEY_M:
          rightStickKeys |= (0x1 << (4 * rightIndex));
          rightIndex++;
          break;
      case HID_KEY_PERIOD:
          rightStickKeys |= (0x2 << (4 * rightIndex));
          rightIndex++;
          break;
      case HID_KEY_COMMA:
          rightStickKeys |= (0x4 << (4 * rightIndex));
          rightIndex++;
          break;
      case HID_KEY_SLASH:
          rightStickKeys |= (0x8 << (4 * rightIndex));
          rightIndex++;
          break;
      default:
          break;
      }

      if (is_ctrl && is_alt && report->keycode[i] == HID_KEY_DELETE)
      {
      #ifdef CONFIG_XB1
        btns_home = true;
      #elif CONFIG_NGC
        // gc-swiss irg
        btns_sel = true;
        dpad_down = true;
        btns_two = true;
        btns_six = true;
      #elif CONFIG_PCE
        // SSDS3 igr
        btns_sel = true;
        btns_run = true;
      #endif
      }

      if ( find_key_in_report(&prev_report, report->keycode[i]) )
      {
        // exist in previous report means the current key is holding
      }else
      {
        // printf("keycode(%d)\r\n", report->keycode[i]);
        // not existed in previous report means the current key is pressed
        // bool const is_shift = report->modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT);
        // uint8_t ch = keycode2ascii[report->keycode[i]][is_shift ? 1 : 0];
        // putchar(ch);
        // if ( ch == '\r' ) putchar('\n'); // added new line for enter key

        // fflush(stdout); // flush right away, else nanolib will wait for newline
      }
    }
  }

  // calculate left stick angle degrees
  if (leftStickKeys) {
    int leftIntensity = leftStickKeys ? (is_shift ? 28 : 78) : 0;
    calculate_coordinates(leftStickKeys, leftIntensity, &analog_left_x, &analog_left_y);
  }

  if (rightStickKeys) {
    int rightIntensity = rightStickKeys ? (is_shift ? 28 : 78) : 0;
    calculate_coordinates(rightStickKeys, rightIntensity, &analog_right_x, &analog_right_y);
  }

  if (hatSwitchKeys) {
    uint8_t hat_switch_x, hat_switch_y;
    calculate_coordinates(hatSwitchKeys, 100, &hat_switch_x, &hat_switch_y);
    dpad_up = hat_switch_y > 128;
    dpad_down = hat_switch_y < 128;
    dpad_left = hat_switch_x < 128;
    dpad_right = hat_switch_x > 128;
  }

  buttons = (((false)      ? 0x00 : 0x20000) | // r3
             ((false)      ? 0x00 : 0x10000) | // l3
             ((btns_six)   ? 0x00 : 0x8000) |
             ((btns_five)  ? 0x00 : 0x4000) |
             ((btns_four)  ? 0x00 : 0x2000) |
             ((btns_three) ? 0x00 : 0x1000) |
             ((has_6btns)  ? 0x00 : 0x0800) |
             ((btns_home)  ? 0x00 : 0x0400) | // home
             ((false)      ? 0x00 : 0x0200) | // r2
             ((false)      ? 0x00 : 0x0100) | // l2
             ((dpad_left)  ? 0x00 : 0x0008) |
             ((dpad_down)  ? 0x00 : 0x0004) |
             ((dpad_right) ? 0x00 : 0x0002) |
             ((dpad_up)    ? 0x00 : 0x0001) |
             ((btns_run)   ? 0x00 : 0x0080) |
             ((btns_sel)   ? 0x00 : 0x0040) |
             ((btns_two)   ? 0x00 : 0x0020) |
             ((btns_one)   ? 0x00 : 0x0010));

  post_globals(dev_addr, instance, buttons, analog_left_x, analog_left_y, analog_right_x, analog_right_y, analog_l, analog_r, reportKeys, 0);

  prev_report = *report;
}

//--------------------------------------------------------------------+
// Mouse
//--------------------------------------------------------------------+

void cursor_movement(int8_t x, int8_t y, int8_t wheel, uint8_t spinner)
{

uint8_t x1, y1;

#if USE_ANSI_ESCAPE
  // Move X using ansi escape
  if ( x < 0)
  {
    printf(ANSI_CURSOR_BACKWARD(%d), (-x)); // move left
  }else if ( x > 0)
  {
    printf(ANSI_CURSOR_FORWARD(%d), x); // move right
  }

  // Move Y using ansi escape
  if ( y < 0)
  {
    printf(ANSI_CURSOR_UP(%d), (-y)); // move up
  }else if ( y > 0)
  {
    printf(ANSI_CURSOR_DOWN(%d), y); // move down
  }

  // Scroll using ansi escape
  if (wheel < 0)
  {
    printf(ANSI_SCROLL_UP(%d), (-wheel)); // scroll up
  }else if (wheel > 0)
  {
    printf(ANSI_SCROLL_DOWN(%d), wheel); // scroll down
  }

  printf("\r\n");
#else
  printf("(%d %d %d %d)\r\n", x, y, wheel, spinner);
#endif
}

static void process_mouse_report(uint8_t dev_addr, uint8_t instance, hid_mouse_report_t const * report)
{
  static hid_mouse_report_t prev_report = { 0 };

  static bool previous_middle_button = false;

  //------------- button state  -------------//
  uint8_t button_changed_mask = report->buttons ^ prev_report.buttons;
  if ( button_changed_mask & report->buttons)
  {
    printf(" %c%c%c%c%c ",
       report->buttons & MOUSE_BUTTON_BACKWARD  ? 'R' : '-',
       report->buttons & MOUSE_BUTTON_FORWARD   ? 'S' : '-',
       report->buttons & MOUSE_BUTTON_LEFT      ? '2' : '-',
       report->buttons & MOUSE_BUTTON_MIDDLE    ? 'M' : '-',
       report->buttons & MOUSE_BUTTON_RIGHT     ? '1' : '-');

    if (buttons_swappable && (report->buttons & MOUSE_BUTTON_MIDDLE) &&
        (previous_middle_button == false))
       buttons_swapped = (buttons_swapped ? false : true);

    previous_middle_button = (report->buttons & MOUSE_BUTTON_MIDDLE);
  }

  if (buttons_swapped)
  {
     buttons = (((0xfff00)) | // no six button controller byte
                ((0x0000f)) | // no dpad button presses
                ((report->buttons & MOUSE_BUTTON_MIDDLE)   ? 0x00 : 0x80) |
                ((report->buttons & MOUSE_BUTTON_FORWARD ) ? 0x00 : 0x40) |
                ((report->buttons & MOUSE_BUTTON_RIGHT)    ? 0x00 : 0x20) |
                ((report->buttons & MOUSE_BUTTON_LEFT)     ? 0x00 : 0x10));
  }
  else
  {
     buttons = (((0xfff00)) |
                ((0x0000f)) |
                ((report->buttons & MOUSE_BUTTON_MIDDLE)   ? 0x00 : 0x80) |
                ((report->buttons & MOUSE_BUTTON_FORWARD ) ? 0x00 : 0x40) |
                ((report->buttons & MOUSE_BUTTON_LEFT)     ? 0x00 : 0x20) |
                ((report->buttons & MOUSE_BUTTON_RIGHT)    ? 0x00 : 0x10));
  }

#ifdef CONFIG_PCE // mice translation
  local_x = (0 - report->x);
  local_y = (0 - report->y);
#else // controllers
  local_x = report->x;
  local_y = ((~report->y) & 0xff);
#endif

#ifdef CONFIG_NUON
  // mouse wheel to spinner rotation conversion
  if (report->wheel != 0) {
    if (report->wheel < 0) { // clockwise
      spinner += ((-1 * report->wheel) + 3);
      if (spinner > 255) spinner -= 255;
    } else { // counter-clockwise
      if (spinner >= ((report->wheel) + 3)) {
        spinner += report->wheel;
        spinner -= 3;
      } else {
        spinner = 255 - ((report->wheel) - spinner) - 3;
      }
    }
  }

  int16_t delta = (report->x * -1);

  // check max/min delta value
  if (delta > 15) delta = 15;
  if (delta < -15) delta = -15;

  // mouse x-axis to spinner rotation conversion
  if (delta != 0) {
    if (delta < 0) { // clockwise
      spinner += delta;
      if (spinner > 255) spinner -= 255;
    } else { // counter-clockwise
      if (spinner >= ((delta))) {
        spinner += delta;
      } else {
        spinner = 255 - delta - spinner;
      }
    }
  }

#endif
  // add to accumulator and post to the state machine
  // if a scan from the host machine is ongoing, wait
  post_mouse_globals(dev_addr, instance, buttons, local_x, local_y, spinner);

  //------------- cursor movement -------------//
  cursor_movement(report->x, report->y, report->wheel, spinner);
}

//--------------------------------------------------------------------+
// Gamepad Report
//--------------------------------------------------------------------+

void print_bits(uint32_t num) {
    for(int bit = 7; bit >= 0; bit--) {
        printf("%d", (num >> bit) & 1);
    }
    printf("\n");
}

//called from parser for filtering report items
bool CALLBACK_HIDParser_FilterHIDReportItem(uint8_t dev_addr, uint8_t instance, HID_ReportItem_t *const CurrentItem)
{
  if (CurrentItem->ItemType != HID_REPORT_ITEM_In)
    return false;

  // if (devices[dev_addr].instances[instance].reportID == INVALID_REPORT_ID)
  // {
  //   devices[dev_addr].instances[instance].reportID = CurrentItem->ReportID;
  // }
  switch (CurrentItem->Attributes.Usage.Page)
  {
    case HID_USAGE_PAGE_DESKTOP:
      // printf("HID_USAGE: 0x%x\n", CurrentItem->Attributes.Usage.Usage);
      switch (CurrentItem->Attributes.Usage.Usage)
      {
        case HID_USAGE_DESKTOP_X:
        case HID_USAGE_DESKTOP_Y:
        case HID_USAGE_DESKTOP_Z:
        case HID_USAGE_DESKTOP_RZ:
        case HID_USAGE_DESKTOP_HAT_SWITCH:
        case HID_USAGE_DESKTOP_DPAD_UP:
        case HID_USAGE_DESKTOP_DPAD_DOWN:
        case HID_USAGE_DESKTOP_DPAD_LEFT:
        case HID_USAGE_DESKTOP_DPAD_RIGHT:
          return true;
      }
      return false;
    case HID_USAGE_PAGE_BUTTON:
      return true;
  }
  return false;
}

static void process_gamepad_report(uint8_t dev_addr, uint8_t instance, hid_gamepad_report_t const *report)
{
  static hid_gamepad_report_t prev_report = { 0 };

  bool has_6btns = true;
  bool dpad_left = false, dpad_down = false, dpad_right = false, dpad_up = false,
    btns_run = false, btns_sel = false, btns_one = false, btns_two = false,
    btns_three = false, btns_four = false, btns_five = false, btns_six = false,
    btns_home = false;

  printf("X: %d ", report->x);
  print_bits(report->x);
  printf("Y: %d ", report->y);
  print_bits(report->y);
  printf("Z: %d ", report->z);
  print_bits(report->z);
  printf("Rz: %d ", report->rz);
  print_bits(report->rz);
  printf("Rx: %d ", report->rx);
  print_bits(report->rx);
  printf("Ry: %d ", report->ry);
  print_bits(report->ry);
  printf("Hat: ");
  print_bits(report->hat);

  printf("Buttons: ");
  for(int i = 3; i >= 0; i--) {
      print_bits(report->buttons >> (i * 8));
  }
  printf("\n");

  buttons = (((false)      ? 0x00 : 0x20000) | // r3
             ((false)      ? 0x00 : 0x10000) | // l3
             ((btns_six)   ? 0x00 : 0x8000) |
             ((btns_five)  ? 0x00 : 0x4000) |
             ((btns_four)  ? 0x00 : 0x2000) |
             ((btns_three) ? 0x00 : 0x1000) |
             ((has_6btns)  ? 0x00 : 0x0800) |
             ((btns_home)  ? 0x00 : 0x0400) | // home
             ((false)      ? 0x00 : 0x0200) | // r2
             ((false)      ? 0x00 : 0x0100) | // l2
             ((dpad_left)  ? 0x00 : 0x0008) |
             ((dpad_down)  ? 0x00 : 0x0004) |
             ((dpad_right) ? 0x00 : 0x0002) |
             ((dpad_up)    ? 0x00 : 0x0001) |
             ((btns_run)   ? 0x00 : 0x0080) |
             ((btns_sel)   ? 0x00 : 0x0040) |
             ((btns_two)   ? 0x00 : 0x0020) |
             ((btns_one)   ? 0x00 : 0x0010));
  post_globals(dev_addr, instance, buttons, 128, 128, 128, 128, 0, 0, 0, 0);

  prev_report = *report;
}

// Parses report with parsed HID descriptor byteIndexes & bitMasks
void parse_hid_report(uint8_t dev_addr, uint8_t instance, uint8_t const *report, uint16_t len)
{
  static pad_buttons previous[5][5];
  pad_buttons current = {0};
  current.value = 0;

  uint16_t xValue, yValue, zValue, rzValue;

  if (devices[dev_addr].instances[instance].xLoc.bitMask > 0xFF) { // if bitmask is larger than 8 bits
    // Combine the current byte and the next byte into a 16-bit value
    uint16_t combinedBytes = ((uint16_t)report[devices[dev_addr].instances[instance].xLoc.byteIndex] << 8) | report[devices[dev_addr].instances[instance].xLoc.byteIndex + 1];
    // Apply the bitmask to the combined 16-bit value
    xValue = (combinedBytes & devices[dev_addr].instances[instance].xLoc.bitMask) >> (__builtin_ctz(devices[dev_addr].instances[instance].xLoc.bitMask));
  } else {
    // Apply the bitmask to the single byte value (your existing implementation)
    xValue = report[devices[dev_addr].instances[instance].xLoc.byteIndex] & devices[dev_addr].instances[instance].xLoc.bitMask;
  }

  if (devices[dev_addr].instances[instance].yLoc.bitMask > 0xFF) { // if bitmask is larger than 8 bits
    // Combine the current byte and the next byte into a 16-bit value
    uint16_t combinedBytes = ((uint16_t)report[devices[dev_addr].instances[instance].yLoc.byteIndex] << 8) | report[devices[dev_addr].instances[instance].yLoc.byteIndex + 1];
    // Apply the bitmask to the combined 16-bit value
    yValue = (combinedBytes & devices[dev_addr].instances[instance].yLoc.bitMask) >> (__builtin_ctz(devices[dev_addr].instances[instance].yLoc.bitMask));
  } else {
    // Apply the bitmask to the single byte value (your existing implementation)
    yValue = report[devices[dev_addr].instances[instance].yLoc.byteIndex] & devices[dev_addr].instances[instance].yLoc.bitMask;
  }

  if (devices[dev_addr].instances[instance].zLoc.bitMask > 0xFF) { // if bitmask is larger than 8 bits
    // Combine the current byte and the next byte into a 16-bit value
    uint16_t combinedBytes = ((uint16_t)report[devices[dev_addr].instances[instance].zLoc.byteIndex] << 8) | report[devices[dev_addr].instances[instance].zLoc.byteIndex + 1];
    // Apply the bitmask to the combined 16-bit value
    zValue = (combinedBytes & devices[dev_addr].instances[instance].zLoc.bitMask) >> (__builtin_ctz(devices[dev_addr].instances[instance].zLoc.bitMask));
  } else {
    // Apply the bitmask to the single byte value (your existing implementation)
    zValue = report[devices[dev_addr].instances[instance].zLoc.byteIndex] & devices[dev_addr].instances[instance].zLoc.bitMask;
  }

  if (devices[dev_addr].instances[instance].rzLoc.bitMask > 0xFF) { // if bitmask is larger than 8 bits
    // Combine the current byte and the next byte into a 16-bit value
    uint16_t combinedBytes = ((uint16_t)report[devices[dev_addr].instances[instance].rzLoc.byteIndex] << 8) | report[devices[dev_addr].instances[instance].rzLoc.byteIndex + 1];
    // Apply the bitmask to the combined 16-bit value
    rzValue = (combinedBytes & devices[dev_addr].instances[instance].rzLoc.bitMask) >> (__builtin_ctz(devices[dev_addr].instances[instance].rzLoc.bitMask));
  } else {
    // Apply the bitmask to the single byte value (your existing implementation)
    rzValue = report[devices[dev_addr].instances[instance].rzLoc.byteIndex] & devices[dev_addr].instances[instance].rzLoc.bitMask;
  }

  uint8_t hatValue = report[devices[dev_addr].instances[instance].hatLoc.byteIndex] & devices[dev_addr].instances[instance].hatLoc.bitMask;

  // parse hat from report
  if (devices[dev_addr].instances[instance].hatLoc.bitMask) {
    uint8_t direction = hatValue <= 8 ? hatValue : 8; // fix for hats with pressed state greater than 8
    current.all_direction |= HAT_SWITCH_TO_DIRECTION_BUTTONS[direction];
  } else {
    hatValue = 8;
  }

  // parse buttons from report
  current.all_buttons = 0;
  for (int i = 0; i < 12; i++) {
    if (report[devices[dev_addr].instances[instance].buttonLoc[i].byteIndex] & devices[dev_addr].instances[instance].buttonLoc[i].bitMask) {
      current.all_buttons |= (0x01 << i);
    }
  }

  // TODO:
  //    - parse and scale analog value by xLoc.mid*2
  //    - add support for second analog stick
  //
  // parse analog from report
  current.x = xValue;
  current.y = yValue;
  current.z = zValue;
  current.rz = rzValue;
  // if (devices[dev_addr].instances[instance].xLoc.bitMask && devices[dev_addr].instances[instance].yLoc.bitMask) {
  //   // parse x-axis from report
  //   uint32_t range_half = devices[dev_addr].instances[instance].xLoc.mid;
  //   uint32_t dead_zone_range = range_half / DEAD_ZONE;
  //   if (xValue < (range_half - dead_zone_range))
  //   {
  //     current.left |= 1;
  //   }
  //   else if (xValue > (range_half + dead_zone_range))
  //   {
  //     current.right |= 1;
  //   }

  //   // parse y-axis from report
  //   range_half = devices[dev_addr].instances[instance].yLoc.mid;
  //   dead_zone_range = range_half / DEAD_ZONE;
  //   if (yValue < (range_half - dead_zone_range))
  //   {
  //     current.up |= 1;
  //   }
  //   else if (yValue > (range_half + dead_zone_range))
  //   {
  //     current.down |= 1;
  //   }
  // }

  // TODO: based on diff report rather than current's datastructure in order to get subtle analog changes
  if (previous[dev_addr-1][instance].value != current.value)
  {
    previous[dev_addr-1][instance] = current;

    if (HID_DEBUG) {
      printf("Super HID Report: ");
      printf("Button Count: %d\n", devices[dev_addr].instances[instance].buttonCnt);
      printf(" xValue:%d yValue:%d dPad:%d \n",xValue, yValue, hatValue);
      for (int i = 0; i < 12; i++) {
        printf(" B%d:%d", i + 1, (report[devices[dev_addr].instances[instance].buttonLoc[i].byteIndex] & devices[dev_addr].instances[instance].buttonLoc[i].bitMask) ? 1 : 0 );
      }
      printf("\n");
    }

    uint8_t buttonCount = devices[dev_addr].instances[instance].buttonCnt;
    if (buttonCount > 12) buttonCount = 12;
    bool buttonSelect = current.all_buttons & (0x01 << (buttonCount-2));
    bool buttonStart = current.all_buttons & (0x01 << (buttonCount-1));
    bool buttonI = current.button1;
    bool buttonIII = current.button3;
    bool buttonIV = current.button4;
    bool buttonV = buttonCount >=7 ? current.button5 : 0;
    bool buttonVI = buttonCount >=8 ? current.button6 : 0;
    bool buttonVIII = buttonCount >=9 ? current.button7 : 0;
    bool buttonXI = buttonCount >=10 ? current.button8 : 0;
    bool has_6btns = buttonCount >= 6;

    // assume DirectInput mapping
    if (buttonCount >= 10) {
      buttonSelect = current.button9;
      buttonStart = current.button10;
      buttonI = current.button3;
      buttonIII = current.button4;
      buttonIV = current.button1;
    }

    buttons = (((buttonXI)        ? 0x00 : 0x20000) | // r3
               ((buttonVIII)      ? 0x00 : 0x10000) | // l3
               ((buttonVI)        ? 0x00 : 0x8000) |
               ((buttonV)         ? 0x00 : 0x4000) |
               ((buttonIV)        ? 0x00 : 0x2000) |
               ((buttonIII)       ? 0x00 : 0x1000) |
               ((has_6btns)       ? 0x00 : 0x0800) |
               ((false)           ? 0x00 : 0x0400) | // home
               ((false)           ? 0x00 : 0x0200) | // r2
               ((false)           ? 0x00 : 0x0100) | // l2
               ((current.left)    ? 0x00 : 0x0008) |
               ((current.down)    ? 0x00 : 0x0004) |
               ((current.right)   ? 0x00 : 0x0002) |
               ((current.up)      ? 0x00 : 0x0001) |
               ((buttonStart)     ? 0x00 : 0x0080) |
               ((buttonSelect)    ? 0x00 : 0x0040) |
               ((current.button2) ? 0x00 : 0x0020) |
               ((buttonI)         ? 0x00 : 0x0010));

    // invert vertical axis
    uint8_t axis_x = (current.x == 255) ? 255 : current.x + 1;
    uint8_t axis_y = (current.y == 0) ? 255 : 255 - current.y;
    uint8_t axis_z = (current.z == 255) ? 255 : current.z + 1;
    uint8_t axis_rz = (current.rz == 0) ? 255 : 255 - current.rz;

    // keep analog within range [1-255]
    ensureAllNonZero(&axis_x, &axis_y, &axis_z, &axis_rz);

    post_globals(dev_addr, instance, buttons, axis_x, axis_y, axis_z, axis_rz, 0, 0, 0, 0);
  }
}

//--------------------------------------------------------------------+
// Generic Report
//--------------------------------------------------------------------+
static void process_generic_report(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
  (void) dev_addr;

  uint8_t const rpt_count = devices[dev_addr].instances[instance].report_count;
  tuh_hid_report_info_t* rpt_info_arr = devices[dev_addr].instances[instance].report_info;
  tuh_hid_report_info_t* rpt_info = NULL;

  if ( rpt_count == 1 && rpt_info_arr[0].report_id == 0)
  {
    // Simple report without report ID as 1st byte
    rpt_info = &rpt_info_arr[0];
  }else
  {
    // Composite report, 1st byte is report ID, data starts from 2nd byte
    uint8_t const rpt_id = report[0];

    // Find report id in the arrray
    for(uint8_t i=0; i<rpt_count; i++)
    {
      if (rpt_id == rpt_info_arr[i].report_id )
      {
        rpt_info = &rpt_info_arr[i];
        break;
      }
    }

    report++;
    len--;
  }

  if (!rpt_info)
  {
    printf("Couldn't find the report info for this report !\r\n");
    return;
  }

  // For complete list of Usage Page & Usage checkout src/class/hid/hid.h. For examples:
  // - Keyboard                     : Desktop, Keyboard
  // - Mouse                        : Desktop, Mouse
  // - Gamepad                      : Desktop, Gamepad
  // - Consumer Control (Media Key) : Consumer, Consumer Control
  // - System Control (Power key)   : Desktop, System Control
  // - Generic (vendor)             : 0xFFxx, xx
  if ( rpt_info->usage_page == HID_USAGE_PAGE_DESKTOP )
  {
    switch (rpt_info->usage)
    {
      case HID_USAGE_DESKTOP_KEYBOARD:
        TU_LOG1("HID receive keyboard report\r\n");
        // Assume keyboard follow boot report layout
        process_kbd_report(dev_addr, instance, (hid_keyboard_report_t const*) report );
      break;

      case HID_USAGE_DESKTOP_MOUSE:
        TU_LOG1("HID receive mouse report\r\n");
        // Assume mouse follow boot report layout
        process_mouse_report(dev_addr, instance, (hid_mouse_report_t const*) report );
      break;

      // case HID_USAGE_DESKTOP_GAMEPAD:
      default:
        TU_LOG1("HID receive gamepad report\r\n");
        // Assume gamepad follow boot report layout
        // process_gamepad_report(dev_addr, instance, (hid_gamepad_report_t const*) report );
        parse_hid_report(dev_addr, instance, report, len);
      break;

      // default: break;
    }
  }
}
