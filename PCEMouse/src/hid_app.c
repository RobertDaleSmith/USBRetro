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

#include "bsp/board.h"
#include "tusb.h"
#include "hid_parser.h"

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

// Controller Types
#define CONTROLLER_GENERIC 0x00
#define CONTROLLER_HID 0x01
#define CONTROLLER_DS3 0x02
#define CONTROLLER_DS4 0x03
#define CONTROLLER_DS5 0x04
#define CONTROLLER_SWITCH 0x05

const char* dpad_str[] = { "N", "NE", "E", "SE", "S", "SW", "W", "NW", "none" };
uint16_t tplctr_serial_v1[] = {0x031a, 'N', 'E', 'S', '-', 'S', 'N', 'E', 'S', '-', 'G', 'E', 'N', 'E', 'S', 'I', 'S'};
uint16_t tplctr_serial_v2[] = {0x0320, 'N', 'E', 'S', '-', 'N', 'T', 'T', '-', 'G', 'E', 'N', 'E', 'S', 'I', 'S'};
uint16_t tplctr_serial_v2_1[] = {0x031a, 'S', '-', 'N', 'E', 'S', '-', 'G', 'E', 'N', '-', 'V', '2'};

uint8_t output_sequence_counter = 0;

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
  };
  struct
  {
    uint16_t all_direction : 4;
    uint32_t all_buttons : 12;
  };
  uint32_t value : 24;
} pad_buttons;

// Sony DS3/SIXAXIS https://github.com/torvalds/linux/blob/master/drivers/hid/hid-sony.c
typedef struct TU_ATTR_PACKED {
  uint8_t reportId; // 0x01 for HID data

  struct {
    uint8_t select  : 1;
    uint8_t l3      : 1;
    uint8_t r3      : 1;
    uint8_t start   : 1;
    uint8_t up      : 1;
    uint8_t right   : 1;
    uint8_t down    : 1;
    uint8_t left    : 1;
  };

  struct {
    uint8_t l2      : 1;
    uint8_t r2      : 1;
    uint8_t l1      : 1;
    uint8_t r1      : 1;
    uint8_t triangle: 1;
    uint8_t circle  : 1;
    uint8_t cross   : 1;
    uint8_t square  : 1;
  };

  uint8_t ps;

  uint8_t notUsed;
  uint8_t lx, ly, rx, ry; // joystick data
  uint8_t pressure[12]; // pressure levels for select, L3, R3, start, up, right, down, left, L2, R2, L1, R1, triangle, circle, cross, square
  uint8_t unused[36];
  uint8_t charge;       // battery level
  uint8_t connection;   // connection state, 0x02: connected
  uint8_t power_rating; // unknown
  uint8_t communication_rating; // unknown
  uint8_t pad[5];       // padding

  uint8_t counter; // +1 each report
} sony_ds3_report_t;

typedef struct
{
  uint8_t time_enabled; // the total time the led is active (0xff means forever)
  uint8_t duty_length; // how long a cycle is in deciseconds (0 means "really fast")
  uint8_t enabled;
  uint8_t duty_off; // % of duty_length the led is off (0xff means 100%)
  uint8_t duty_on; // % of duty_length the led is on (0xff mean 100%)
} sony_ds3_led_t;

typedef struct
{
  uint8_t padding;
  uint8_t right_duration; // Right motor duration (0xff means forever)
  uint8_t right_motor_on; // Right (small) motor on/off, only supports values of 0 or 1 (off/on) */
  uint8_t left_duration; // Left motor duration (0xff means forever)
  uint8_t left_motor_force; // Left (large) motor, supports force values from 0 to 255
} sony_ds3_rumble_t;

typedef struct
{
  uint8_t report_id;
  sony_ds3_rumble_t rumble;
  uint8_t padding[4];
  uint8_t leds_bitmap; // bitmap of enabled LEDs: LED_1 = 0x02, LED_2 = 0x04, ...
  sony_ds3_led_t led[4]; // LEDx at (4 - x)
  sony_ds3_led_t _reserved; // LED5, not actually soldered
} sony_ds3_output_report_t;

typedef union
{
  sony_ds3_output_report_t data;
  uint8_t buf[49];
} sony_ds3_output_report_01_t;

// Sony DS4 report layout detail https://www.psdevwiki.com/ps4/DS4-USB
typedef struct TU_ATTR_PACKED
{
  uint8_t x, y, z, rz; // joystick

  struct {
    uint8_t dpad     : 4; // (hat format, 0x08 is released, 0=N, 1=NE, 2=E, 3=SE, 4=S, 5=SW, 6=W, 7=NW)
    uint8_t square   : 1; // west
    uint8_t cross    : 1; // south
    uint8_t circle   : 1; // east
    uint8_t triangle : 1; // north
  };

  struct {
    uint8_t l1     : 1;
    uint8_t r1     : 1;
    uint8_t l2     : 1;
    uint8_t r2     : 1;
    uint8_t share  : 1;
    uint8_t option : 1;
    uint8_t l3     : 1;
    uint8_t r3     : 1;
  };

  struct {
    uint8_t ps      : 1; // playstation button
    uint8_t tpad    : 1; // track pad click
    uint8_t counter : 6; // +1 each report
  };

  uint8_t l2_trigger; // 0 released, 0xff fully pressed
  uint8_t r2_trigger; // as above

   uint16_t timestamp;
   uint8_t  battery;

   int16_t gyro[3];  // x, y, z;
   int16_t accel[3]; // x, y, z

  // there is still lots more info

} sony_ds4_report_t;

typedef struct TU_ATTR_PACKED {
  // First 16 bits set what data is pertinent in this structure (1 = set; 0 = not set)
  uint8_t set_rumble : 1;
  uint8_t set_led : 1;
  uint8_t set_led_blink : 1;
  uint8_t set_ext_write : 1;
  uint8_t set_left_volume : 1;
  uint8_t set_right_volume : 1;
  uint8_t set_mic_volume : 1;
  uint8_t set_speaker_volume : 1;
  uint8_t set_flags2;

  uint8_t reserved;

  uint8_t motor_right;
  uint8_t motor_left;

  uint8_t lightbar_red;
  uint8_t lightbar_green;
  uint8_t lightbar_blue;
  uint8_t lightbar_blink_on;
  uint8_t lightbar_blink_off;

  uint8_t ext_data[8];

  uint8_t volume_left;
  uint8_t volume_right;
  uint8_t volume_mic;
  uint8_t volume_speaker;

  uint8_t other[9];
} sony_ds4_output_report_t;

// Sony DS5 controller
typedef struct TU_ATTR_PACKED
{
  uint8_t x1, y1, x2, y2, rx, ry, rz;

  struct {
    uint8_t dpad     : 4; // (hat format, 0x08 is released, 0=N, 1=NE, 2=E, 3=SE, 4=S, 5=SW, 6=W, 7=NW)
    uint8_t square   : 1; // west
    uint8_t cross    : 1; // south
    uint8_t circle   : 1; // east
    uint8_t triangle : 1; // north
  };

  struct {
    uint8_t l1     : 1;
    uint8_t r1     : 1;
    uint8_t l2     : 1;
    uint8_t r2     : 1;
    uint8_t share  : 1;
    uint8_t option : 1;
    uint8_t l3     : 1;
    uint8_t r3     : 1;
  };

  struct {
    uint8_t ps      : 1; // playstation button
    uint8_t tpad    : 1; // track pad click
    uint8_t mute    : 1; // mute button
    uint8_t padding : 5;
  };

  uint8_t counter; // +1 each report

} sony_ds5_report_t;

typedef struct {
    // Screw this I'll leave it to hid-sony maintainer's internal doc S:
    // No he won't implement it. So maybe use DS4Windows docs for now.
    uint8_t type; // TODO. 0x6: vibrating, 0x23: 2step
    uint8_t params[10]; // 0x6: 0: frequency (1-255), 1: off time (1-255). 0x23: 0: step1 resistance (0-15), 1: step2 resistance (0-15)
} ds5_trigger_t;

typedef struct {
    uint16_t flags; // @ 0-1. bitfield fedcba9876543210. 012: rumble emulation (seems that the lowest nibble has to be 0x7 (????????????0111) in order to trigger this), 2: trigger_r, 3: trigger_l, 8: mic_led, a: lightbar, c: player_led
    uint8_t rumble_r; // @ 2
    uint8_t rumble_l; // @ 3
    uint8_t unk3[4]; // @ 4-7
    uint8_t mic_led; // @ 8. 0: off, 1: on, 2: pulse
    uint8_t unk9; // @ 9
    ds5_trigger_t trigger_r;// 10-20
    ds5_trigger_t trigger_l; // 21-31
    uint8_t unk28[11]; // @ 32-42
    uint8_t player_led; // @ 43. 5-bit. LSB is left.
    union {
        uint8_t lightbar_rgb[3];
        struct {
            uint8_t lightbar_r;
            uint8_t lightbar_g;
            uint8_t lightbar_b;
        };
    }; // @ 44-46
} ds5_feedback_t;

// 8BitDo USB Adapter for PS classic
typedef struct TU_ATTR_PACKED
{
  struct {
    uint8_t triangle : 1;
    uint8_t circle   : 1;
    uint8_t cross    : 1;
    uint8_t square   : 1;
    uint8_t l2       : 1;
    uint8_t r2       : 1;
    uint8_t l1       : 1;
    uint8_t r1       : 1;
  };

  struct {
    uint8_t share  : 1;
    uint8_t option : 1;
    uint8_t dpad   : 4;
    uint8_t ps     : 2;
  };

  uint8_t counter; // +1 each report

} sony_psc_report_t;

// 8BitDo USB Adapter for PC Engine 2.4g controllers
typedef struct TU_ATTR_PACKED
{
  struct {
    uint8_t two  : 2;
    uint8_t one  : 2;
  };

  struct {
    uint8_t sel  : 1;
    uint8_t run  : 1;
  };

  struct {
    uint8_t dpad : 4; // (hat format, 0x08 is released, 0=N, 1=NE, 2=E, 3=SE, 4=S, 5=SW, 6=W, 7=NW)
  };

} bitdo_pce_report_t;

// 8BitDo M30 Bluetooth gamepad
typedef struct TU_ATTR_PACKED
{
  struct {
    uint8_t a : 1;
    uint8_t b : 1;
    uint8_t home : 1;
    uint8_t x : 1;
    uint8_t y : 1;
    uint8_t padding1 : 1;
    uint8_t z : 1;
    uint8_t c : 1;
  };

  struct {
    uint8_t l : 1;
    uint8_t r : 1;
    uint8_t minus : 1;
    uint8_t start : 1;
    uint8_t padding2 : 4;
  };

  struct {
    uint8_t dpad   : 4;
    uint8_t padding3 : 4;
  };

} bitdo_m30_report_t;

// Sega Genesis mini controller
typedef struct TU_ATTR_PACKED
{
  struct {
    uint8_t y : 1;
    uint8_t b : 1;
    uint8_t a : 1;
    uint8_t x : 1;
    uint8_t l : 1;
    uint8_t r : 1;
    uint8_t z : 1;
    uint8_t c : 1;
  };

  struct {
    uint8_t mode  : 1;
    uint8_t start : 7;
  };

  uint8_t id;

  uint8_t dpad_x;
  uint8_t dpad_y;

} sega_mini_report_t;

// Sega Astro City mini controller
typedef struct TU_ATTR_PACKED
{
  uint8_t id;
  uint8_t id2;
  uint8_t id3;
  uint8_t x;
  uint8_t y;

  struct {
    uint8_t null : 4;
    uint8_t b : 1;
    uint8_t e : 1;
    uint8_t d : 1;
    uint8_t a : 1;
  };

  struct {
    uint8_t c : 1;
    uint8_t f : 1;
    uint8_t l : 1;
    uint8_t r : 1;
    uint8_t credit : 1;
    uint8_t start  : 3;
  };

} astro_city_report_t;

// Logitech WingMan controller
typedef struct TU_ATTR_PACKED
{
  uint8_t analog_x;
  uint8_t analog_y;
  uint8_t analog_z;

  struct {
    uint8_t dpad : 4;
    uint8_t a : 1;
    uint8_t b : 1;
    uint8_t c : 1;
    uint8_t x : 1;
  };

  struct {
    uint8_t y : 1;
    uint8_t z : 1;
    uint8_t l : 1;
    uint8_t r : 1;
    uint8_t s : 1;
    uint8_t mode : 1;
    uint8_t null : 2;
  };

} wing_man_report_t;

// TripleController v2 (Arduino based HID)
typedef struct TU_ATTR_PACKED
{
  struct {
    uint8_t b : 1;
    uint8_t a : 1;
    uint8_t y : 1;
    uint8_t x : 1;
    uint8_t l : 1;
    uint8_t r : 1;
    uint8_t select : 1;
    uint8_t start : 1;
  };

  struct {
    uint8_t ntt_0 : 1;
    uint8_t ntt_1 : 1;
    uint8_t ntt_2 : 1;
    uint8_t ntt_3 : 1;
    uint8_t ntt_4 : 1;
    uint8_t ntt_5 : 1;
    uint8_t ntt_6 : 1;
    uint8_t ntt_7 : 1;
  };

  struct {
    uint8_t ntt_8 : 1;
    uint8_t ntt_9 : 1;
    uint8_t ntt_star : 1;
    uint8_t ntt_hash : 1;
    uint8_t ntt_dot : 1;
    uint8_t ntt_clear : 1;
    uint8_t ntt_null : 1;
    uint8_t ntt_end : 1;
  };

  uint8_t axis_x;
  uint8_t axis_y;

} triple_v2_report_t;

// TripleController v1 (Arduino based HID)
typedef struct TU_ATTR_PACKED
{
  struct {
    uint8_t b : 1; // A
    uint8_t a : 1; // B
    uint8_t y : 1; // C
    uint8_t x : 1; // X
    uint8_t l : 1; // Y
    uint8_t r : 1; // Z
    uint8_t select : 1; // Mode
    uint8_t start : 1;
  };

  struct {
    uint8_t home : 1;
    uint8_t null : 7;
  };

  uint8_t axis_x;
  uint8_t axis_y;

} triple_v1_report_t;

// Pokken Wii U USB Controller
typedef struct TU_ATTR_PACKED
{
  struct {
    uint8_t y : 1;
    uint8_t b : 1;
    uint8_t a : 1;
    uint8_t x : 1;
    uint8_t l : 1;
    uint8_t r : 1;
    uint8_t zl : 1;
    uint8_t zr : 1;
  };

  struct {
    uint8_t select : 1;
    uint8_t start  : 1;
    uint8_t padding1 : 6;
  };

  struct {
    uint8_t dpad   : 4;
    uint8_t padding2 : 4;
  };

} pokken_report_t;

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
} switch_report_t;

typedef union
{
  switch_report_t data;
  uint8_t buf[sizeof(switch_report_t)];
} switch_report_01_t;

// Generic NES USB Controller
typedef struct TU_ATTR_PACKED
{
  uint8_t id, axis1_y, axis1_x, axis0_x, axis0_y;

  struct {
    uint8_t high : 4;
    uint8_t x : 1;
    uint8_t a : 1;
    uint8_t b : 1;
    uint8_t y : 1;
  };

  struct {
    uint8_t low : 4;
    uint8_t select : 1;
    uint8_t start : 1;
    uint8_t r : 1;
    uint8_t l : 1;
  };

} nes_usb_report_t;

#define MAX_DEVICES 10
#define MAX_REPORT  5

typedef struct {
    uint8_t byteIndex;
    uint8_t bitMask;
    uint32_t mid;
} InputLocation;

// Each HID instance can has multiple reports
typedef struct TU_ATTR_PACKED
{
  uint8_t type;
  uint8_t report_count;
  tuh_hid_report_info_t report_info[MAX_REPORT];
  //
  bool ds3_init;
  bool ds3_led_set;
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
  // uint8_t motor_left;
  // uint8_t motor_right;

  InputLocation xLoc;
  InputLocation yLoc;
  InputLocation hatLoc;
  InputLocation buttonLoc[12]; // assuming a maximum of 12 buttons
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

// check if device is Sony DualShock 3
static inline bool is_sony_ds3(uint8_t dev_addr)
{
  uint16_t vid = devices[dev_addr].vid;
  uint16_t pid = devices[dev_addr].pid;

  return ((vid == 0x054c && pid == 0x0268)); // Sony DualShock3
}

// check if device is Sony DualShock 4
static inline bool is_sony_ds4(uint8_t dev_addr)
{
  uint16_t vid = devices[dev_addr].vid;
  uint16_t pid = devices[dev_addr].pid;

  return ( (vid == 0x054c && (pid == 0x09cc || pid == 0x05c4)) // Sony DualShock4 
           || (vid == 0x0f0d && pid == 0x005e)                 // Hori FC4 
           || (vid == 0x0f0d && pid == 0x00ee)                 // Hori PS4 Mini (PS4-099U) 
           || (vid == 0x1f4f && pid == 0x1002)                 // ASW GG xrd controller
          //  || (vid == 0x1532 && pid == 0x0401)                 // GP2040-CE PS4 mode
         );
}

// check if device is Wii U Pokken USB Controller
static inline bool is_pokken(uint8_t dev_addr)
{
  uint16_t vid = devices[dev_addr].vid;
  uint16_t pid = devices[dev_addr].pid;

  return ((vid == 0x0f0d && pid == 0x0092)); // Wii U Pokken
}

// check if device is 8BitDo Ultimate C Wired Controller
static inline bool is_switch(uint8_t dev_addr)
{
  uint16_t vid = devices[dev_addr].vid;
  uint16_t pid = devices[dev_addr].pid;

  return ((vid == 0x057e && (pid == 0x2009 || pid == 0x200e)));

  return ((vid == 0x057e && (
           pid == 0x2009 || // Nintendo Switch Pro
           pid == 0x200e    // JoyCon Charge Grip
         )));
}

// check if device is generic NES USB Controller
static inline bool is_nes_usb(uint8_t dev_addr)
{
  uint16_t vid = devices[dev_addr].vid;
  uint16_t pid = devices[dev_addr].pid;

  return ((vid == 0x0079 && pid == 0x0011)); // Generic NES USB
}

// check if device is 8BitDo PCE Controller
static inline bool is_8bit_pce(uint8_t dev_addr)
{
  uint16_t vid = devices[dev_addr].vid;
  uint16_t pid = devices[dev_addr].pid;

  return ((vid == 0x0f0d && pid == 0x0138)); // 8BitDo PCE (wireless)
}

// check if device is PlayStation Classic Controller
static inline bool is_sony_psc(uint8_t dev_addr)
{
  uint16_t vid = devices[dev_addr].vid;
  uint16_t pid = devices[dev_addr].pid;

  return ((vid == 0x054c && pid == 0x0cda)); // PSClassic Controller
}

// check if device is 8BitDo Bluetooth gamepad
static inline bool is_8bit_m30(uint8_t dev_addr)
{
  uint16_t vid = devices[dev_addr].vid;
  uint16_t pid = devices[dev_addr].pid;

  return ((vid == 0x2dc8 && pid == 0x5006)); // 8BitDo M30 BT (Android Mode)
}

// check if device is Sega Genesis mini controller
static inline bool is_sega_mini(uint8_t dev_addr)
{
  uint16_t vid = devices[dev_addr].vid;
  uint16_t pid = devices[dev_addr].pid;

  return ((vid == 0x0f0d && pid == 0x00c1)); // Sega Genesis mini controller
}

// check if device is Astro City mini controller
static inline bool is_astro_city(uint8_t dev_addr)
{
  uint16_t vid = devices[dev_addr].vid;
  uint16_t pid = devices[dev_addr].pid;

  return ((vid == 0x0ca3 && (
           pid == 0x0028 || // Astro City mini joystick
           pid == 0x0027 || // Astro City mini controller
           pid == 0x0024    // 8BitDo M30 6-button controller (2.4g)
         )));
}

// check if device is Sony DS5 controller
static inline bool is_sony_ds5(uint8_t dev_addr)
{
  uint16_t vid = devices[dev_addr].vid;
  uint16_t pid = devices[dev_addr].pid;

  return ((vid == 0x054c && pid == 0x0ce6)); // Sony DS5 controller
}

// check if device is Logitech WingMan Action controller
static inline bool is_wing_man(uint8_t dev_addr)
{
  uint16_t vid = devices[dev_addr].vid;
  uint16_t pid = devices[dev_addr].pid;

  return ((vid == 0x046d && pid == 0xc20b)); // Logitech WingMan Action controller
}

bool compare_utf16(uint16_t* s1, uint16_t* s2, size_t n) {
    for(size_t i = 0; i < n; i++) {
        if(s1[i] != s2[i]) {
            return false;
        }
    }
    return true;
}

// check if device is TripleController (Arduino based HID)
static inline bool is_triple_v2(uint8_t dev_addr)
{
  uint16_t vid = devices[dev_addr].vid;
  uint16_t pid = devices[dev_addr].pid;

  bool serial_match = false;
  bool vidpid_match = (vid == 0x2341 && pid == 0x8036); // Arduino Leonardo

  if (!vidpid_match) return false;

  // Compare the the fetched serial with "S-NES-GEN-V2" or "NES-NTT-GENESIS"
  if(memcmp(devices[dev_addr].serial, tplctr_serial_v2, sizeof(tplctr_serial_v2)) == 0 ||
     memcmp(devices[dev_addr].serial, tplctr_serial_v2_1, sizeof(tplctr_serial_v2_1)) == 0)
  {
    serial_match = true;
  }

  return serial_match;
}

// check if device is TripleController (Arduino based HID)
static inline bool is_triple_v1(uint8_t dev_addr)
{
  uint16_t vid = devices[dev_addr].vid;
  uint16_t pid = devices[dev_addr].pid;

  bool serial_match = false;
  bool vidpid_match = (vid == 0x2341 && pid == 0x8036); // Arduino Leonardo

  if (!vidpid_match) return false;

  // Compare the the fetched serial with "NES-SNES-GENESIS"
  if(memcmp(devices[dev_addr].serial, tplctr_serial_v1, sizeof(tplctr_serial_v1)) == 0)
  {
    serial_match = true;
  }

  return serial_match;
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

uint16_t buttons;
uint8_t local_x;
uint8_t local_y;

static void process_kbd_report(uint8_t dev_addr, uint8_t instance, hid_keyboard_report_t const *report);
static void process_mouse_report(uint8_t dev_addr, uint8_t instance, hid_mouse_report_t const * report);
static void process_gamepad_report(uint8_t dev_addr, uint8_t instance, hid_gamepad_report_t const *report);
static void process_generic_report(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len);

extern void __not_in_flash_func(post_globals)(uint8_t dev_addr, int8_t instance, uint16_t buttons, uint8_t delta_x, uint8_t delta_y);
extern int __not_in_flash_func(find_player_index)(int device_address, int instance_number);
extern void remove_players_by_address(int device_address, int instance);

extern bool is_fun;
unsigned char fun_inc = 0;
unsigned char fun_player = 1;
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

void hid_app_task(void)
{
  const uint32_t interval_ms = 200;
  static uint32_t start_ms_ds3 = 0;
  static uint32_t start_ms_ds4 = 0;
  static uint32_t start_ms_ds5 = 0;
  static uint32_t start_ms_nsw = 0;

  if (is_fun) {
    fun_inc++;
    if (!fun_inc) {
      fun_player = ++fun_player%0x20;
    }
  }

  // iterate devices and instances that can receive responses
  for(uint8_t dev_addr=1; dev_addr<MAX_DEVICES; dev_addr++){
    for(uint8_t instance=0; instance<CFG_TUH_HID; instance++){
      // send DS3 Init, LED and rumble responses
      if (devices[dev_addr].instances[instance].type == CONTROLLER_DS3) {
        if (!devices[dev_addr].instances[instance].ds3_init) {
          /*
          * The Sony Sixaxis does not handle HID Output Reports on the
          * Interrupt EP like it could, so we need to force HID Output
          * Reports to use tuh_hid_set_report on the Control EP.
          *
          * There is also another issue about HID Output Reports via USB,
          * the Sixaxis does not want the report_id as part of the data
          * packet, so we have to discard buf[0] when sending the actual
          * control message, even for numbered reports, humpf!
          */
          printf("PS3 Init..\n");

          uint8_t cmd_buf[4];
          cmd_buf[0] = 0x42; // Special PS3 Controller enable commands
          cmd_buf[1] = 0x0c;
          cmd_buf[2] = 0x00;
          cmd_buf[3] = 0x00;

          // Send a Set Report request to the control endpoint
          tuh_hid_set_report(dev_addr, instance, 0xF4, HID_REPORT_TYPE_FEATURE, &(cmd_buf), sizeof(cmd_buf));

          devices[dev_addr].instances[instance].ds3_init = true;
        } else if (!devices[dev_addr].instances[instance].ds3_led_set) {
          int player_index = find_player_index(dev_addr, instance);

          uint32_t current_time_ms = board_millis();
          if ( current_time_ms - start_ms_ds3 >= interval_ms)
          {
            start_ms_ds3 = current_time_ms;

            sony_ds3_output_report_01_t output_report = {
              .buf = {
                0x01,
                0x00, 0xff, 0x00, 0xff, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00,
                0xff, 0x27, 0x10, 0x00, 0x32,
                0xff, 0x27, 0x10, 0x00, 0x32,
                0xff, 0x27, 0x10, 0x00, 0x32,
                0xff, 0x27, 0x10, 0x00, 0x32,
                0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
              }
            };

            // led player indicator
            switch (player_index+1)
            {
            case 1:
            case 2:
            case 3:
            case 4:
            case 5:
              output_report.data.leds_bitmap = (PLAYER_LEDS[player_index+1] << 1);
              break;

            default: // unassigned
              // turn all leds on
              output_report.data.leds_bitmap = (PLAYER_LEDS[10] << 1);

              // make all leds dim
              for (int n = 0; n < 4; n++) {
                output_report.data.led[n].duty_length = 0;
                output_report.data.led[n].duty_on = 32;
                output_report.data.led[n].duty_off = 223;
              }
              break;
            }

            // fun
            if (player_index+1 && is_fun) {
              output_report.data.leds_bitmap = (fun_inc & 0b00011110);

              // led brightness
              for (int n = 0; n < 4; n++) {
                output_report.data.led[n].duty_length = (fun_inc & 0x07);
                output_report.data.led[n].duty_on = fun_inc;
                output_report.data.led[n].duty_off = 255 - fun_inc;
              }
            }

            // output_report.data.rumble.right_motor_on = 1;
            // output_report.data.rumble.left_motor_force = 1;
            // output_report.data.rumble.left_duration = 16;
            // output_report.data.rumble.right_duration = 16;
            // Send report without the report ID, start at index 1 instead of 0
            tuh_hid_send_report(dev_addr, instance, output_report.data.report_id, &(output_report.buf[1]), sizeof(output_report) - 1);
          }
          // devices[dev_addr].instances[instance].ds3_led_set = true;
        }
      }

      // send DS4 LED and rumble response
      if (devices[dev_addr].instances[instance].type == CONTROLLER_DS4) {
        uint32_t current_time_ms = board_millis();
        if ( current_time_ms - start_ms_ds4 >= interval_ms)
        {
          int player_index = find_player_index(dev_addr, instance);
          start_ms_ds4 = current_time_ms;

          sony_ds4_output_report_t output_report = {0};
          output_report.set_led = 1;
          switch (player_index+1)
          {
          case 1:
            output_report.lightbar_blue = 64;
            break;

          case 2:
            output_report.lightbar_red = 64;
            break;

          case 3:
            output_report.lightbar_green = 64;
            break;

          case 4: // purple
            output_report.lightbar_red = 20;
            output_report.lightbar_blue = 40;
            break;

          case 5: // yellow
            output_report.lightbar_red = 64;
            output_report.lightbar_green = 64;
            break;

          default: // white
            output_report.lightbar_blue = 32;
            output_report.lightbar_green = 32;
            output_report.lightbar_red = 32;
            break;
          }

          // fun
          if (player_index+1 && is_fun) {
            output_report.lightbar_red = fun_inc;
            output_report.lightbar_green = (fun_inc%2 == 0) ? fun_inc+64 : 0;
            output_report.lightbar_blue = (fun_inc%2 == 0) ? 0 : fun_inc+128;
          }

          // output_report.set_rumble = 1;
          // output_report.motor_left = devices[dev_addr].instances[instance].motor_left;
          // output_report.motor_right = devices[dev_addr].instances[instance].motor_right;
          tuh_hid_send_report(dev_addr, instance, 5, &output_report, sizeof(output_report));
        }
      }

      // send DS5 LED and rumble response
      if (devices[dev_addr].instances[instance].type == CONTROLLER_DS5) {

        uint32_t current_time_ms = board_millis();
        if ( current_time_ms - start_ms_ds5 >= interval_ms)
        {
          int player_index = find_player_index(dev_addr, instance);
          start_ms_ds5 = current_time_ms;

          ds5_feedback_t ds5_fb = {0};

          // set flags for trigger_r, trigger_l, lightbar, and player_led
          // ds5_fb.flags |= (1 << 0 | 1 << 1); // haptics
          // ds5_fb.flags |= (1 << 2); // trigger_r
          // ds5_fb.flags |= (1 << 3); // trigger_l
          ds5_fb.flags |= (1 << 10); // lightbar
          ds5_fb.flags |= (1 << 12); // player_led

          // haptic feedback example
          // ds5_fb.trigger_r.type = 2; // Set type
          // ds5_fb.trigger_r.params[0] = 0x5f;
          // ds5_fb.trigger_r.params[1] = 0xff;

          // left trigger with similar effect as the PS5 demo
          // ds5_fb.trigger_l.type = (fun_inc % 32 < 16) ? 0 : 2; // Set type
          // ds5_fb.trigger_l.params[0] = 0x00;
          // ds5_fb.trigger_l.params[1] = 0xff;
          // ds5_fb.trigger_l.params[2] = 0xff;

          // for(int i = 0; i < 10; i++) {
          //     ds5_fb.trigger_r.params[i] = 255;
          // }

          switch (player_index+1)
          {
          case 1:
            ds5_fb.player_led = 0b00100;
            ds5_fb.lightbar_b = 64;
            break;

          case 2:
            ds5_fb.player_led = 0b01010;
            ds5_fb.lightbar_r = 64;
            break;

          case 3:
            ds5_fb.player_led = 0b10101;
            ds5_fb.lightbar_g = 64;
            break;

          case 4: // purple
            ds5_fb.player_led = 0b11011;
            ds5_fb.lightbar_r = 20;
            ds5_fb.lightbar_b = 40;
            break;

          case 5: // yellow
            ds5_fb.player_led = 0b11111;
            ds5_fb.lightbar_r = 64;
            ds5_fb.lightbar_g = 64;
            break;

          default: // white
            ds5_fb.player_led = 0;
            ds5_fb.lightbar_b = 32;
            ds5_fb.lightbar_g = 32;
            ds5_fb.lightbar_r = 32;
            break;
          }

          // fun
          if (player_index+1 && is_fun) {
            ds5_fb.player_led = fun_player;
            ds5_fb.lightbar_r = fun_inc;
            ds5_fb.lightbar_g = fun_inc+64;
            ds5_fb.lightbar_b = fun_inc+128;
          }

          tuh_hid_send_report(dev_addr, instance, 5, &ds5_fb, sizeof(ds5_fb));
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
        if (!devices[dev_addr].instances[instance].switch_baud) {
          devices[dev_addr].instances[instance].switch_baud = true;

          printf("SWITCH[%d|%d]: Baud\r\n", dev_addr, instance);
          uint8_t buf2[1] = { 0x03 /* PROCON_USB_BAUD */ };
          tuh_hid_send_report(dev_addr, instance, 0x80, buf2, sizeof(buf2));

        // wait for baud ask and then send init handshake
        } else if (!devices[dev_addr].instances[instance].switch_handshake && devices[dev_addr].instances[instance].switch_baud_ack) {
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
            int player_index = find_player_index(dev_addr, devices[dev_addr].instance_count == 1 ? instance : devices[dev_addr].instance_root);

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
            }
          }
        }
      }
    }
  }
}

// Gets HID descriptor report item for specific ReportID
static inline bool USB_GetHIDReportItemInfoWithReportId(const uint8_t *ReportData, HID_ReportItem_t *const ReportItem)
{
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
  bool hid_debug = false;
  while (item)
  {
    uint8_t midValue = (item->Attributes.Logical.Maximum - item->Attributes.Logical.Minimum) / 2;
    uint8_t bitSize = item->Attributes.BitSize ? item->Attributes.BitSize : 0; // bits per usage
    uint8_t bitOffset = item->BitOffset ? item->BitOffset : 0; // bits offset from start
    uint8_t bitMask = ((0xFF >> (8 - bitSize)) << bitOffset % 8); // usage bits byte mask
    uint8_t byteIndex = (int)(bitOffset / 8); // usage start byte

    if (hid_debug) {
      printf("midValue: %d ", midValue);
      printf("bitSize: %d ", bitSize);
      printf("bitOffset: %d ", bitOffset);
      printf("bitMask: 0x%x ", bitMask);
      printf("byteIndex: %d", byteIndex);
    }
    uint8_t report[1] = {0}; // reportId = 0; original ex maps report to descriptor data structure
    if (USB_GetHIDReportItemInfoWithReportId(report, item))
    {
      switch (item->Attributes.Usage.Page)
      {
      case HID_USAGE_PAGE_DESKTOP:
        switch (item->Attributes.Usage.Usage)
        {
        case HID_USAGE_DESKTOP_X:
        {
          if (hid_debug) printf(" HID_USAGE_DESKTOP_X ");
          devices[dev_addr].instances[instance].xLoc.byteIndex = byteIndex;
          devices[dev_addr].instances[instance].xLoc.bitMask = bitMask;
          devices[dev_addr].instances[instance].xLoc.mid = midValue;

        }
        break;
        case HID_USAGE_DESKTOP_Y:
        {
          if (hid_debug) printf(" HID_USAGE_DESKTOP_Y ");
          devices[dev_addr].instances[instance].yLoc.byteIndex = byteIndex;
          devices[dev_addr].instances[instance].yLoc.bitMask = bitMask;
          devices[dev_addr].instances[instance].yLoc.mid = midValue;
        }
        break;
        // case HID_USAGE_DESKTOP_Z:
        // {
        //   if (hid_debug) printf(" HID_USAGE_DESKTOP_Z ");
        //   devices[dev_addr].instances[instance].zLoc.byteIndex = byteIndex;
        //   devices[dev_addr].instances[instance].zLoc.bitMask = bitMask;
        //  devices[dev_addr].instances[instance].zLoc.mid = midValue;
        // }
        break;
        case HID_USAGE_DESKTOP_HAT_SWITCH:
          if (hid_debug) printf(" HID_USAGE_DESKTOP_HAT_SWITCH ");
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
        if (hid_debug) printf(" HID_USAGE_PAGE_BUTTON ");
        uint8_t usage = item->Attributes.Usage.Usage;

        if (usage >= 1 && usage <= 12) {
          devices[dev_addr].instances[instance].buttonLoc[usage - 1].byteIndex = byteIndex;
          devices[dev_addr].instances[instance].buttonLoc[usage - 1].bitMask = bitMask;
        }
        btns_count++;
      }
      break;
      }
    }
    item = item->Next;
    if (hid_debug) printf("\n\n");
  }

  devices[dev_addr].instances[instance].buttonCnt = btns_count;
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

  // hid_parser
  uint8_t ret = USB_ProcessHIDReport(dev_addr, instance, desc_report, desc_len, &(info));
  if(ret == HID_PARSE_Successful)
  {
    // g_dev_addr = dev_addr;
    // g_instance = instance;
    parse_hid_descriptor(dev_addr, instance);
  }
  else
  {
    printf("Error: USB_ProcessHIDReport failed: %d\r\n", ret);
  }
  USB_FreeReportInfo(info);
  info = NULL;

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
  bool isController = false;
  if      (is_sony_ds3(dev_addr)  ) isController = true;
  else if (is_sony_ds4(dev_addr)  ) isController = true;
  else if (is_sony_ds5(dev_addr)  ) isController = true;
  else if (is_sony_psc(dev_addr)  ) isController = true;
  else if (is_8bit_pce(dev_addr)  ) isController = true;
  else if (is_sega_mini(dev_addr) ) isController = true;
  else if (is_astro_city(dev_addr)) isController = true;
  else if (is_wing_man(dev_addr)  ) isController = true;
  else if (is_triple_v2(dev_addr) ) isController = true;
  else if (is_triple_v1(dev_addr) ) isController = true;
  else if (is_pokken(dev_addr)    ) isController = true;
  else if (is_switch(dev_addr)    ) isController = true;
  else if (is_nes_usb(dev_addr)   ) isController = true;

  printf("isController: %d, dev: %d, instance: %d\n", isController?1:0, dev_addr, instance);

  if ( !isController && itf_protocol == HID_ITF_PROTOCOL_NONE )
  {
    devices[dev_addr].instances[instance].report_count = tuh_hid_parse_report_descriptor(devices[dev_addr].instances[instance].report_info, MAX_REPORT, desc_report, desc_len);
    printf("HID has %u reports \r\n", devices[dev_addr].instances[instance].report_count);
  }

  uint16_t temp_buf[128];
  if (0 == tuh_descriptor_get_serial_string_sync(dev_addr, LANGUAGE_ID, temp_buf, sizeof(temp_buf)))
  {
    for(int i=0; i<20; i++){
      devices[dev_addr].serial[i] = temp_buf[i];
    }
  }

  if (is_sony_ds3(dev_addr))
  {
    devices[dev_addr].instances[instance].type = CONTROLLER_DS3;
    devices[dev_addr].instances[instance].ds3_init = false;
    devices[dev_addr].instances[instance].ds3_led_set = false;
    // devices[dev_addr].instances[instance].motor_left = 0;
    // devices[dev_addr].instances[instance].motor_right = 0;
  }
  else if (is_sony_ds4(dev_addr))
  {
    devices[dev_addr].instances[instance].type = CONTROLLER_DS4;
    // devices[dev_addr].instances[instance].motor_left = 0;
    // devices[dev_addr].instances[instance].motor_right = 0;
  }
  else if (is_sony_ds5(dev_addr))
  {
    devices[dev_addr].instances[instance].type = CONTROLLER_DS5;
    // devices[dev_addr].instances[instance].motor_left = 0;
    // devices[dev_addr].instances[instance].motor_right = 0;
  }
  else if (is_switch(dev_addr))
  {
    devices[dev_addr].instances[instance].type = CONTROLLER_SWITCH;
    // devices[dev_addr].instances[instance].motor_left = 0;
    // devices[dev_addr].instances[instance].motor_right = 0;
    printf("SWITCH[%d|%d]: Mounted\r\n", dev_addr, instance);
  }
  else {
    devices[dev_addr].instances[instance].type = CONTROLLER_GENERIC;
  }

  // request to receive report
  // tuh_hid_report_received_cb() will be invoked when report is available
  if ( !tuh_hid_receive_report(dev_addr, instance) )
  {
    printf("Error: cannot request to receive report\r\n");
  }
}

void switch_reset(uint8_t dev_addr, uint8_t instance)
{
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

// Invoked when device with hid interface is un-mounted
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
{
  printf("HID device address = %d, instance = %d is unmounted\r\n", dev_addr, instance);

  // Reset Switch Config
  if (devices[dev_addr].instances[instance].type == CONTROLLER_SWITCH) {
    switch_reset(dev_addr, instance);
  }
  // Reset HID Config
  else if (devices[dev_addr].instances[instance].type == CONTROLLER_HID) {
    // hid_reset(dev_addr, instance);
  }

  devices[dev_addr].instance_count--;
  devices[dev_addr].instances[instance].type = CONTROLLER_GENERIC;
}

// check if different than 2
bool diff_than_n(uint8_t x, uint8_t y, uint8_t n)
{
  return (x - y > n) || (y - x > n);
}

// check if 2 reports are different enough
bool ds3_diff_report(sony_ds3_report_t const* rpt1, sony_ds3_report_t const* rpt2)
{
  bool result;

  // x, y, z, rz must different than 2 to be counted
  result = diff_than_n(rpt1->lx, rpt2->lx, 2) || diff_than_n(rpt1->ly, rpt2->ly, 2) ||
           diff_than_n(rpt1->rx, rpt2->rx, 2) || diff_than_n(rpt1->ry, rpt2->ry, 2);

  // check the rest with mem compare
  result |= memcmp(&rpt1->reportId + 1, &rpt2->reportId + 1, 3);

  return result;
}

// check if 2 reports are different enough
bool ds4_diff_report(sony_ds4_report_t const* rpt1, sony_ds4_report_t const* rpt2)
{
  bool result;

  // x, y, z, rz must different than 2 to be counted
  result = diff_than_n(rpt1->x, rpt2->x, 2) || diff_than_n(rpt1->y, rpt2->y, 2) ||
           diff_than_n(rpt1->z, rpt2->z, 2) || diff_than_n(rpt1->rz, rpt2->rz, 2);

  // check the rest with mem compare
  result |= memcmp(&rpt1->rz + 1, &rpt2->rz + 1, 5);

  return result;
}

bool ds5_diff_report(sony_ds5_report_t const* rpt1, sony_ds5_report_t const* rpt2)
{
  bool result;

  // x1, y1, x2, y2, rx, ry must different than 2 to be counted
  result = diff_than_n(rpt1->x1, rpt2->x1, 2) || diff_than_n(rpt1->y1, rpt2->y1, 2) ||
           diff_than_n(rpt1->x2, rpt2->x2, 2) || diff_than_n(rpt1->y2, rpt2->y2, 2) ||
           diff_than_n(rpt1->rx, rpt2->rx, 2) || diff_than_n(rpt1->ry, rpt2->ry, 2);

  // check the rest with mem compare
  result |= memcmp(&rpt1->rz + 1, &rpt2->rz + 1, 3);

  return result;
}

bool psc_diff_report(sony_psc_report_t const* rpt1, sony_psc_report_t const* rpt2)
{
  bool result;

  result = rpt1->dpad != rpt2->dpad;
  result |= rpt1->triangle != rpt2->triangle;
  result |= rpt1->circle != rpt2->circle;
  result |= rpt1->square != rpt2->square;
  result |= rpt1->cross != rpt2->cross;
  result |= rpt1->r1 != rpt2->r1;
  result |= rpt1->l1 != rpt2->l1;
  result |= rpt1->r2 != rpt2->r2;
  result |= rpt1->l2 != rpt2->l2;
  result |= rpt1->option != rpt2->option;
  result |= rpt1->share != rpt2->share;
  result |= rpt1->ps != rpt2->ps;

  return result;
}

bool pce_diff_report(bitdo_pce_report_t const* rpt1, bitdo_pce_report_t const* rpt2)
{
  bool result;

  result = rpt1->dpad != rpt2->dpad;
  result |= rpt1->sel != rpt2->sel;
  result |= rpt1->run != rpt2->run;
  result |= rpt1->one != rpt2->one;
  result |= rpt1->two != rpt2->two;

  return result;
}

bool m30_diff_report(bitdo_m30_report_t const* rpt1, bitdo_m30_report_t const* rpt2)
{
  bool result;

  // check the all with mem compare
  result |= memcmp(&rpt1, &rpt2, 3);

  return result;
}

bool sega_diff_report(sega_mini_report_t const* rpt1, sega_mini_report_t const* rpt2)
{
  bool result;

  result |= rpt1->a != rpt2->a;
  result |= rpt1->b != rpt2->b;
  result |= rpt1->c != rpt2->c;
  result |= rpt1->x != rpt2->x;
  result |= rpt1->y != rpt2->y;
  result |= rpt1->z != rpt2->z;
  result |= rpt1->l != rpt2->l;
  result |= rpt1->r != rpt2->r;
  result |= rpt1->start != rpt2->start;
  result |= rpt1->mode != rpt2->mode;
  result |= rpt1->dpad_x != rpt2->dpad_x;
  result |= rpt1->dpad_y != rpt2->dpad_y;

  return result;
}

bool astro_diff_report(astro_city_report_t const* rpt1, astro_city_report_t const* rpt2)
{
  bool result;

  result |= rpt1->x != rpt2->x;
  result |= rpt1->y != rpt2->y;
  result |= rpt1->a != rpt2->a;
  result |= rpt1->b != rpt2->b;
  result |= rpt1->c != rpt2->c;
  result |= rpt1->d != rpt2->d;
  result |= rpt1->e != rpt2->e;
  result |= rpt1->f != rpt2->f;
  result |= rpt1->l != rpt2->l;
  result |= rpt1->r != rpt2->r;
  result |= rpt1->credit != rpt2->credit;
  result |= rpt1->start != rpt2->start;

  return result;
}

bool wingman_diff_report(wing_man_report_t const* rpt1, wing_man_report_t const* rpt2)
{
  bool result;

  result |= rpt1->analog_x != rpt2->analog_x;
  result |= rpt1->analog_y != rpt2->analog_y;
  result |= rpt1->analog_z != rpt2->analog_z;
  result |= rpt1->dpad != rpt2->dpad;
  result |= rpt1->a != rpt2->a;
  result |= rpt1->b != rpt2->b;
  result |= rpt1->c != rpt2->c;
  result |= rpt1->x != rpt2->x;
  result |= rpt1->y != rpt2->y;
  result |= rpt1->z != rpt2->z;
  result |= rpt1->l != rpt2->l;
  result |= rpt1->r != rpt2->r;
  result |= rpt1->mode != rpt2->mode;
  result |= rpt1->s != rpt2->s;

  return result;
}

bool triple_v2_diff_report(triple_v2_report_t const* rpt1, triple_v2_report_t const* rpt2)
{
  bool result;

  result |= rpt1->axis_x != rpt2->axis_x;
  result |= rpt1->axis_y != rpt2->axis_y;
  result |= rpt1->b != rpt2->b;
  result |= rpt1->a != rpt2->a;
  result |= rpt1->y != rpt2->y;
  result |= rpt1->x != rpt2->x;
  result |= rpt1->l != rpt2->l;
  result |= rpt1->r != rpt2->r;
  result |= rpt1->select != rpt2->select;
  result |= rpt1->start != rpt2->start;
  result |= rpt1->ntt_0 != rpt2->ntt_0;

  return result;
}

bool triple_v1_diff_report(triple_v1_report_t const* rpt1, triple_v1_report_t const* rpt2)
{
  bool result;

  result |= rpt1->axis_x != rpt2->axis_x;
  result |= rpt1->axis_y != rpt2->axis_y;
  result |= rpt1->b != rpt2->b;
  result |= rpt1->a != rpt2->a;
  result |= rpt1->y != rpt2->y;
  result |= rpt1->x != rpt2->x;
  result |= rpt1->l != rpt2->l;
  result |= rpt1->r != rpt2->r;
  result |= rpt1->select != rpt2->select;
  result |= rpt1->start != rpt2->start;
  result |= rpt1->home != rpt2->home;

  return result;
}

bool pokken_diff_report(pokken_report_t const* rpt1, pokken_report_t const* rpt2)
{
  bool result;

  result |= rpt1->dpad != rpt2->dpad;
  result |= rpt1->b != rpt2->b;
  result |= rpt1->a != rpt2->a;
  result |= rpt1->y != rpt2->y;
  result |= rpt1->x != rpt2->x;
  result |= rpt1->l != rpt2->l;
  result |= rpt1->r != rpt2->r;
  result |= rpt1->zl != rpt2->zl;
  result |= rpt1->zr != rpt2->zr;
  result |= rpt1->select != rpt2->select;
  result |= rpt1->start != rpt2->start;

  return result;
}

bool switch_diff_report(switch_report_t const* rpt1, switch_report_t const* rpt2)
{
  bool result;

  uint16_t rpt1_left_stick_x = rpt1->left_stick[0] | ((rpt1->left_stick[1] & 0x0F) << 8);
  uint16_t rpt1_left_stick_y = (rpt1->left_stick[1] >> 4) | (rpt1->left_stick[2] << 4);
  uint16_t rpt2_left_stick_x = rpt2->left_stick[0] | ((rpt2->left_stick[1] & 0x0F) << 8);
  uint16_t rpt2_left_stick_y = (rpt2->left_stick[1] >> 4) | (rpt2->left_stick[2] << 4);

  uint16_t rpt1_right_stick_x = rpt1->right_stick[0] | ((rpt1->right_stick[1] & 0x0F) << 8);
  uint16_t rpt1_right_stick_y = (rpt1->right_stick[1] >> 4) | (rpt1->right_stick[2] << 4);
  uint16_t rpt2_right_stick_x = rpt2->right_stick[0] | ((rpt2->right_stick[1] & 0x0F) << 8);
  uint16_t rpt2_right_stick_y = (rpt2->right_stick[1] >> 4) | (rpt2->right_stick[2] << 4);

  // x, y, z, rz must different than 2 to be counted
  result = diff_than_n(rpt1_left_stick_x, rpt2_left_stick_x, 4) || diff_than_n(rpt1_left_stick_y, rpt2_left_stick_y, 4) ||
           diff_than_n(rpt1_right_stick_x, rpt2_right_stick_x, 4) || diff_than_n(rpt1_right_stick_y, rpt2_right_stick_y, 4);

  // check the reset with mem compare (everything but the sticks)
  result |= memcmp(&rpt1->report_id + 3, &rpt2->report_id + 3, 3);
  result |= memcmp(&rpt1->vibration_ack, &rpt2->vibration_ack, 37);

  return result;
}

bool nes_usb_diff_report(nes_usb_report_t const* rpt1, nes_usb_report_t const* rpt2)
{
  bool result;

  result |= rpt1->axis0_y != rpt2->axis0_y;
  result |= rpt1->axis0_x != rpt2->axis0_x;
  result |= rpt1->a != rpt2->a;
  result |= rpt1->b != rpt2->b;
  result |= rpt1->x != rpt2->x;
  result |= rpt1->y != rpt2->y;
  result |= rpt1->l != rpt2->l;
  result |= rpt1->r != rpt2->r;
  result |= rpt1->select != rpt2->select;
  result |= rpt1->start != rpt2->start;

  return result;
}

void process_sony_ds3(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
  // previous report used to compare for changes
  static sony_ds3_report_t prev_report[5] = { 0 };

  uint8_t const report_id = report[0];
  report++;
  len--;

  // all buttons state is stored in ID 1
  if (report_id == 1)
  {
    sony_ds3_report_t ds3_report;
    memcpy(&ds3_report, report, sizeof(ds3_report));

    // counter is +1, assign to make it easier to compare 2 report
    prev_report[dev_addr-1].counter = ds3_report.counter;

    // only print if changes since it is polled ~ 5ms
    // Since count+1 after each report and  x, y, z, rz fluctuate within 1 or 2
    // We need more than memcmp to check if report is different enough
    if ( ds3_diff_report(&prev_report[dev_addr-1], &ds3_report) )
    {
      printf("(lx, ly, rx, ry) = (%u, %u, %u, %u)\r\n", ds3_report.lx, ds3_report.ly, ds3_report.rx, ds3_report.ry);
      printf("DPad = ");

      if (ds3_report.up       ) printf("Up ");
      if (ds3_report.down     ) printf("Down ");
      if (ds3_report.left     ) printf("Left ");
      if (ds3_report.right    ) printf("Right ");

      if (ds3_report.square   ) printf("Square ");
      if (ds3_report.cross    ) printf("Cross ");
      if (ds3_report.circle   ) printf("Circle ");
      if (ds3_report.triangle ) printf("Triangle ");

      if (ds3_report.l1       ) printf("L1 ");
      if (ds3_report.r1       ) printf("R1 ");
      if (ds3_report.l2       ) printf("L2 ");
      if (ds3_report.r2       ) printf("R2 ");

      if (ds3_report.select   ) printf("Select ");
      if (ds3_report.start    ) printf("Start ");
      if (ds3_report.l3       ) printf("L3 ");
      if (ds3_report.r3       ) printf("R3 ");

      if (ds3_report.ps       ) printf("PS ");

      printf("\r\n");

      int threshold = 28;
      bool dpad_up    = (ds3_report.up || ds3_report.ly < (128 - threshold));
      bool dpad_right = (ds3_report.right || ds3_report.lx > (128 + threshold));
      bool dpad_down  = (ds3_report.down || ds3_report.ly > (128 + threshold));
      bool dpad_left  = (ds3_report.left || ds3_report.lx < (128 - threshold));
      bool has_6btns = true;

      buttons = (((ds3_report.r1 || ds3_report.l2) ? 0x00 : 0x8000) |
                 ((ds3_report.l1 || ds3_report.r2) ? 0x00 : 0x4000) |
                 ((ds3_report.square)   ? 0x00 : 0x2000) |
                 ((ds3_report.triangle) ? 0x00 : 0x1000) |
                 ((has_6btns)           ? 0x00 : 0xFF00) |
                 ((dpad_left)           ? 0x00 : 0x08) |
                 ((dpad_down)           ? 0x00 : 0x04) |
                 ((dpad_right)          ? 0x00 : 0x02) |
                 ((dpad_up)             ? 0x00 : 0x01) |
                 ((ds3_report.start || ds3_report.ps)    ? 0x00 : 0x80) |
                 ((ds3_report.select || ds3_report.ps)   ? 0x00 : 0x40) |
                 ((ds3_report.cross  || (!has_6btns && ds3_report.triangle)) ? 0x00 : 0x20) |
                 ((ds3_report.circle || (!has_6btns && ds3_report.square))   ? 0x00 : 0x10));

      // add to accumulator and post to the state machine
      // if a scan from the host machine is ongoing, wait
      post_globals(dev_addr, instance, buttons, 0, 0);

      // The left and right triggers control the intensity of the left and right rumble motors
      // motor_left = ds3_report.l2_trigger;
      // motor_right = ds3_report.r2_trigger;

      prev_report[dev_addr-1] = ds3_report;
    }
  }
}

void process_sony_ds4(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
  // previous report used to compare for changes
  static sony_ds4_report_t prev_report[5] = { 0 };

  uint8_t const report_id = report[0];
  report++;
  len--;

  // all buttons state is stored in ID 1
  if (report_id == 1)
  {
    sony_ds4_report_t ds4_report;
    memcpy(&ds4_report, report, sizeof(ds4_report));

    // counter is +1, assign to make it easier to compare 2 report
    prev_report[dev_addr-1].counter = ds4_report.counter;

    // only print if changes since it is polled ~ 5ms
    // Since count+1 after each report and  x, y, z, rz fluctuate within 1 or 2
    // We need more than memcmp to check if report is different enough
    if ( ds4_diff_report(&prev_report[dev_addr-1], &ds4_report) )
    {
      printf("(x, y, z, rz) = (%u, %u, %u, %u)\r\n", ds4_report.x, ds4_report.y, ds4_report.z, ds4_report.rz);
      printf("DPad = %s ", dpad_str[ds4_report.dpad]);

      if (ds4_report.square   ) printf("Square ");
      if (ds4_report.cross    ) printf("Cross ");
      if (ds4_report.circle   ) printf("Circle ");
      if (ds4_report.triangle ) printf("Triangle ");

      if (ds4_report.l1       ) printf("L1 ");
      if (ds4_report.r1       ) printf("R1 ");
      if (ds4_report.l2       ) printf("L2 ");
      if (ds4_report.r2       ) printf("R2 ");

      if (ds4_report.share    ) printf("Share ");
      if (ds4_report.option   ) printf("Option ");
      if (ds4_report.l3       ) printf("L3 ");
      if (ds4_report.r3       ) printf("R3 ");

      if (ds4_report.ps       ) printf("PS ");
      if (ds4_report.tpad     ) printf("TPad ");

      printf("\r\n");

      int threshold = 28;
      bool dpad_up    = (ds4_report.dpad == 0 || ds4_report.dpad == 1 ||
                         ds4_report.dpad == 7 || ds4_report.y < (128 - threshold));
      bool dpad_right = ((ds4_report.dpad >= 1 && ds4_report.dpad <= 3) || ds4_report.x > (128 + threshold));
      bool dpad_down  = ((ds4_report.dpad >= 3 && ds4_report.dpad <= 5) || ds4_report.y > (128 + threshold));
      bool dpad_left  = ((ds4_report.dpad >= 5 && ds4_report.dpad <= 7) || ds4_report.x < (128 - threshold));
      bool has_6btns = true;

      buttons = (((ds4_report.r1 || ds4_report.l2) ? 0x00 : 0x8000) |
                 ((ds4_report.l1 || ds4_report.r2) ? 0x00 : 0x4000) |
                 ((ds4_report.square)   ? 0x00 : 0x2000) |
                 ((ds4_report.triangle) ? 0x00 : 0x1000) |
                 ((has_6btns)           ? 0x00 : 0xFF00) |
                 ((dpad_left)           ? 0x00 : 0x08) |
                 ((dpad_down)           ? 0x00 : 0x04) |
                 ((dpad_right)          ? 0x00 : 0x02) |
                 ((dpad_up)             ? 0x00 : 0x01) |
                 ((ds4_report.option || ds4_report.ps) ? 0x00 : 0x80) |
                 ((ds4_report.share  || ds4_report.ps) ? 0x00 : 0x40) |
                 ((ds4_report.cross  || (!has_6btns && ds4_report.triangle)) ? 0x00 : 0x20) |
                 ((ds4_report.circle || (!has_6btns && ds4_report.square))   ? 0x00 : 0x10));

      // add to accumulator and post to the state machine
      // if a scan from the host machine is ongoing, wait
      post_globals(dev_addr, instance, buttons, 0, 0);

      // The left and right triggers control the intensity of the left and right rumble motors
      // motor_left = ds4_report.l2_trigger;
      // motor_right = ds4_report.r2_trigger;

      prev_report[dev_addr-1] = ds4_report;
    }
  }
}

void process_sony_ds5(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
  // previous report used to compare for changes
  static sony_ds5_report_t prev_report[5] = { 0 };

  uint8_t const report_id = report[0];
  report++;
  len--;

  // all buttons state is stored in ID 1
  if (report_id == 1)
  {
    sony_ds5_report_t ds5_report;
    memcpy(&ds5_report, report, sizeof(ds5_report));

    // counter is +1, assign to make it easier to compare 2 report
    prev_report[dev_addr-1].counter = ds5_report.counter;

    if ( ds5_diff_report(&prev_report[dev_addr-1], &ds5_report) )
    {
      printf("(x1, y1, x2, y2, rx, ry) = (%u, %u, %u, %u, %u, %u)\r\n", ds5_report.x1, ds5_report.y1, ds5_report.x2, ds5_report.y2, ds5_report.rx, ds5_report.ry);
      printf("DPad = %s ", dpad_str[ds5_report.dpad]);

      if (ds5_report.square   ) printf("Square ");
      if (ds5_report.cross    ) printf("Cross ");
      if (ds5_report.circle   ) printf("Circle ");
      if (ds5_report.triangle ) printf("Triangle ");

      if (ds5_report.l1       ) printf("L1 ");
      if (ds5_report.r1       ) printf("R1 ");
      if (ds5_report.l2       ) printf("L2 ");
      if (ds5_report.r2       ) printf("R2 ");

      if (ds5_report.share    ) printf("Share ");
      if (ds5_report.option   ) printf("Option ");
      if (ds5_report.l3       ) printf("L3 ");
      if (ds5_report.r3       ) printf("R3 ");

      if (ds5_report.ps       ) printf("PS ");
      if (ds5_report.tpad     ) printf("TPad ");
      if (ds5_report.mute     ) printf("Mute ");

      printf("\r\n");

      int threshold = 28;
      bool dpad_up    = (ds5_report.dpad == 0 || ds5_report.dpad == 1 ||
                         ds5_report.dpad == 7 || ds5_report.y1 < (128 - threshold));
      bool dpad_right = ((ds5_report.dpad >= 1 && ds5_report.dpad <= 3) || ds5_report.x1 > (128 + threshold));
      bool dpad_down  = ((ds5_report.dpad >= 3 && ds5_report.dpad <= 5) || ds5_report.y1 > (128 + threshold));
      bool dpad_left  = ((ds5_report.dpad >= 5 && ds5_report.dpad <= 7) || ds5_report.x1 < (128 - threshold));
      bool has_6btns = true;

      buttons = (((ds5_report.r1 || ds5_report.l2) ? 0x00 : 0x8000) |
                 ((ds5_report.l1 || ds5_report.r2) ? 0x00 : 0x4000) |
                 ((ds5_report.square)   ? 0x00 : 0x2000) |
                 ((ds5_report.triangle) ? 0x00 : 0x1000) |
                 ((has_6btns)           ? 0x00 : 0xFF00) |
                 ((dpad_left)           ? 0x00 : 0x08) |
                 ((dpad_down)           ? 0x00 : 0x04) |
                 ((dpad_right)          ? 0x00 : 0x02) |
                 ((dpad_up)             ? 0x00 : 0x01) |
                 ((ds5_report.option || ds5_report.ps|| ds5_report.mute) ? 0x00 : 0x80) |
                 ((ds5_report.share  || ds5_report.ps || ds5_report.mute) ? 0x00 : 0x40) |
                 ((ds5_report.cross  || (!has_6btns && ds5_report.triangle)) ? 0x00 : 0x20) |
                 ((ds5_report.circle || (!has_6btns && ds5_report.square))   ? 0x00 : 0x10));

      // add to accumulator and post to the state machine
      // if a scan from the host machine is ongoing, wait
      post_globals(dev_addr, instance, buttons, 0, 0);

      prev_report[dev_addr-1] = ds5_report;
    }

  }
}
void process_sony_psc(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
  // previous report used to compare for changes
  static sony_psc_report_t prev_report[5] = { 0 };

  sony_psc_report_t psc_report;
  memcpy(&psc_report, report, sizeof(psc_report));

  // counter is +1, assign to make it easier to compare 2 report
  prev_report[dev_addr-1].counter = psc_report.counter;

  if ( psc_diff_report(&prev_report[dev_addr-1], &psc_report) )
  {
    printf("DPad = %d ", psc_report.dpad);

    if (psc_report.square   ) printf("Square ");
    if (psc_report.cross    ) printf("Cross ");
    if (psc_report.circle   ) printf("Circle ");
    if (psc_report.triangle ) printf("Triangle ");
    if (psc_report.l1       ) printf("L1 ");
    if (psc_report.r1       ) printf("R1 ");
    if (psc_report.l2       ) printf("L2 ");
    if (psc_report.r2       ) printf("R2 ");
    if (psc_report.share    ) printf("Share ");
    if (psc_report.option   ) printf("Option ");
    if (psc_report.ps       ) printf("PS ");

    printf("\r\n");

    bool dpad_up    = (psc_report.dpad >= 0 && psc_report.dpad <= 2);
    bool dpad_right = (psc_report.dpad == 2 || psc_report.dpad == 6 || psc_report.dpad == 10);
    bool dpad_down  = (psc_report.dpad >= 8 && psc_report.dpad <= 10);
    bool dpad_left  = (psc_report.dpad == 0 || psc_report.dpad == 4 || psc_report.dpad == 8);
    bool has_6btns = true;

    buttons = (((psc_report.r1 || psc_report.l2) ? 0x00 : 0x8000) |
               ((psc_report.l1 || psc_report.r2) ? 0x00 : 0x4000) |
               ((psc_report.square)   ? 0x00 : 0x2000) |
               ((psc_report.triangle) ? 0x00 : 0x1000) |
               ((has_6btns)           ? 0x00 : 0xFF00) |
               ((dpad_left)           ? 0x00 : 0x08) |
               ((dpad_down)           ? 0x00 : 0x04) |
               ((dpad_right)          ? 0x00 : 0x02) |
               ((dpad_up)             ? 0x00 : 0x01) |
               ((psc_report.option || psc_report.ps) ? 0x00 : 0x80) |
               ((psc_report.share  || psc_report.ps) ? 0x00 : 0x40) |
               ((psc_report.cross  || (!has_6btns && psc_report.triangle && !psc_report.ps)) ? 0x00 : 0x20) |
               ((psc_report.circle || (!has_6btns && psc_report.square)) ? 0x00 : 0x10));

    // add to accumulator and post to the state machine
    // if a scan from the host machine is ongoing, wait
    post_globals(dev_addr, instance, buttons, 0, 0);

    prev_report[dev_addr-1] = psc_report;
  }
}

void process_8bit_pce(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
  // previous report used to compare for changes
  static bitdo_pce_report_t prev_report[5] = { 0 };

  bitdo_pce_report_t pce_report;
  memcpy(&pce_report, report, sizeof(pce_report));

  if ( pce_diff_report(&prev_report[dev_addr-1], &pce_report) )
  {
    printf("DPad = %d ", pce_report.dpad);

    if (pce_report.sel) printf("Select ");
    if (pce_report.run) printf("Run ");
    if (pce_report.one) printf("I ");
    if (pce_report.two) printf("II ");

    printf("\r\n");

    bool dpad_up    = (pce_report.dpad == 0 || pce_report.dpad == 1 || pce_report.dpad == 7);
    bool dpad_right = (pce_report.dpad >= 1 && pce_report.dpad <= 3);
    bool dpad_down  = (pce_report.dpad >= 3 && pce_report.dpad <= 5);
    bool dpad_left  = (pce_report.dpad >= 5 && pce_report.dpad <= 7);
    bool has_6btns = false;

    buttons = (((has_6btns)      ? 0x00 : 0xFF00) |
               ((dpad_left)      ? 0x00 : 0x08) |
               ((dpad_down)      ? 0x00 : 0x04) |
               ((dpad_right)     ? 0x00 : 0x02) |
               ((dpad_up)        ? 0x00 : 0x01) |
               ((pce_report.run) ? 0x00 : 0x80) |
               ((pce_report.sel) ? 0x00 : 0x40) |
               ((pce_report.two) ? 0x00 : 0x20) |
               ((pce_report.one) ? 0x00 : 0x10));

    // add to accumulator and post to the state machine
    // if a scan from the host machine is ongoing, wait
    post_globals(dev_addr, instance, buttons, 0, 0);

    prev_report[dev_addr-1] = pce_report;
  }
}

void process_8bit_m30(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
  // previous report used to compare for changes
  static bitdo_m30_report_t prev_report[5] = { 0 };

  bitdo_m30_report_t input_report;
  memcpy(&input_report, report, sizeof(input_report));

  if ( m30_diff_report(&prev_report[dev_addr-1], &input_report) )
  {
    printf("DPad = %d ", input_report.dpad);

    if (input_report.a) printf("A ");
    if (input_report.b) printf("B ");
    if (input_report.c) printf("C ");
    if (input_report.x) printf("X ");
    if (input_report.y) printf("Y ");
    if (input_report.z) printf("Z ");
    if (input_report.l) printf("L ");
    if (input_report.r) printf("R ");
    if (input_report.start) printf("Start ");
    if (input_report.minus) printf("Minus ");
    if (input_report.home) printf("Home ");

    printf("\r\n");

    bool dpad_up    = (input_report.dpad == 0 || input_report.dpad == 1 || input_report.dpad == 7);
    bool dpad_right = (input_report.dpad >= 1 && input_report.dpad <= 3);
    bool dpad_down  = (input_report.dpad >= 3 && input_report.dpad <= 5);
    bool dpad_left  = (input_report.dpad >= 5 && input_report.dpad <= 7);
    bool has_6btns = false;

    buttons = (((input_report.z || input_report.l) ? 0x00 : 0x8000) |
               ((input_report.y || input_report.r) ? 0x00 : 0x4000) |
               ((input_report.x)     ? 0x00 : 0x2000) |
               ((input_report.a)     ? 0x00 : 0x1000) |
               ((dpad_left)          ? 0x00 : 0x08) |
               ((dpad_down)          ? 0x00 : 0x04) |
               ((dpad_right)         ? 0x00 : 0x02) |
               ((dpad_up)            ? 0x00 : 0x01) |
               ((input_report.start || input_report.home) ? 0x00 : 0x80) |
               ((input_report.minus || input_report.home) ? 0x00 : 0x40) |
               ((input_report.b)     ? 0x00 : 0x20) |
               ((input_report.c)     ? 0x00 : 0x10));

    // add to accumulator and post to the state machine
    // if a scan from the host machine is ongoing, wait
    post_globals(dev_addr, instance, buttons, 0, 0);

    prev_report[dev_addr-1] = input_report;
  }
}

void process_sega_mini(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
  // previous report used to compare for changes
  static sega_mini_report_t prev_report[5] = { 0 };

  sega_mini_report_t sega_report;
  memcpy(&sega_report, report, sizeof(sega_report));

  if ( sega_diff_report(&prev_report[dev_addr-1], &sega_report) )
  {
    printf("DPad = x:%d, y:%d ", sega_report.dpad_x, sega_report.dpad_y);
    if (sega_report.a) printf("A ");
    if (sega_report.b) printf("B ");
    if (sega_report.c) printf("C ");
    if (sega_report.x) printf("X ");
    if (sega_report.y) printf("Y ");
    if (sega_report.z) printf("Z ");
    if (sega_report.l) printf("L ");
    if (sega_report.r) printf("R ");
    if (sega_report.start) printf("Start ");
    if (sega_report.mode)  printf("Mode ");
    printf("\r\n");

    bool dpad_up    = (sega_report.dpad_y < 128);
    bool dpad_right = (sega_report.dpad_x > 128);
    bool dpad_down  = (sega_report.dpad_y > 128);
    bool dpad_left  = (sega_report.dpad_x < 128);
    bool has_6btns = true;

    buttons = (((sega_report.z || sega_report.l) ? 0x00 : 0x8000) |
               ((sega_report.y) ? 0x00 : 0x4000) |
               ((sega_report.x || sega_report.r) ? 0x00 : 0x2000) |
               ((sega_report.a) ? 0x00 : 0x1000) |
               ((has_6btns)      ? 0x00 : 0xFF00) |
               ((dpad_left)      ? 0x00 : 0x08) |
               ((dpad_down)      ? 0x00 : 0x04) |
               ((dpad_right)     ? 0x00 : 0x02) |
               ((dpad_up)        ? 0x00 : 0x01) |
               ((sega_report.start) ? 0x00 : 0x80) |
               ((sega_report.mode)  ? 0x00 : 0x40) |
               ((sega_report.b) ? 0x00 : 0x20) |
               ((sega_report.c) ? 0x00 : 0x10));

    // add to accumulator and post to the state machine
    // if a scan from the host machine is ongoing, wait
    post_globals(dev_addr, instance, buttons, 0, 0);

    prev_report[dev_addr-1] = sega_report;
  }

}

void process_astro_city(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
  // previous report used to compare for changes
  static astro_city_report_t prev_report[5] = { 0 };

  astro_city_report_t astro_report;
  memcpy(&astro_report, report, sizeof(astro_report));

  if ( astro_diff_report(&prev_report[dev_addr-1], &astro_report) )
  {
    printf("DPad = x:%d, y:%d ", astro_report.x, astro_report.y);
    if (astro_report.a) printf("A "); // X   <-M30 buttons
    if (astro_report.b) printf("B "); // Y
    if (astro_report.c) printf("C "); // Z
    if (astro_report.d) printf("D "); // A
    if (astro_report.e) printf("E "); // B
    if (astro_report.f) printf("F "); // C
    if (astro_report.l) printf("L ");
    if (astro_report.r) printf("R ");
    if (astro_report.credit) printf("Credit "); // Select
    if (astro_report.start) printf("Start ");
    printf("\r\n");

    bool dpad_up    = (astro_report.y < 127);
    bool dpad_right = (astro_report.x > 127);
    bool dpad_down  = (astro_report.y > 127);
    bool dpad_left  = (astro_report.x < 127);
    bool has_6btns = true;

    buttons = (((astro_report.c) ? 0x00 : 0x8000) | // VI
               ((astro_report.b) ? 0x00 : 0x4000) | // V
               ((astro_report.a) ? 0x00 : 0x2000) | // IV
               ((astro_report.d) ? 0x00 : 0x1000) | // III
               ((has_6btns)      ? 0x00 : 0xFF00) |
               ((dpad_left)      ? 0x00 : 0x08) |
               ((dpad_down)      ? 0x00 : 0x04) |
               ((dpad_right)     ? 0x00 : 0x02) |
               ((dpad_up)        ? 0x00 : 0x01) |
               ((astro_report.start)  ? 0x00 : 0x80) | // RUN
               ((astro_report.credit) ? 0x00 : 0x40) | // SEL
               ((astro_report.e || astro_report.l) ? 0x00 : 0x20) | // II
               ((astro_report.f || astro_report.r) ? 0x00 : 0x10)); // I

    // add to accumulator and post to the state machine
    // if a scan from the host machine is ongoing, wait
    post_globals(dev_addr, instance, buttons, 0, 0);

    prev_report[dev_addr-1] = astro_report;
  }
}

void process_wing_man(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
  // previous report used to compare for changes
  static wing_man_report_t prev_report[5] = { 0 };

  wing_man_report_t wingman_report;
  memcpy(&wingman_report, report, sizeof(wingman_report));

  if ( wingman_diff_report(&prev_report[dev_addr-1], &wingman_report) )
  {
    // printf("(x, y, z) = (%u, %u, %u)\r\n", wingman_report.analog_x, wingman_report.analog_y, wingman_report.analog_z);
    // printf("DPad = %d ", wingman_report.dpad);
    // if (wingman_report.a) printf("A ");
    // if (wingman_report.b) printf("B ");
    // if (wingman_report.c) printf("C ");
    // if (wingman_report.x) printf("X ");
    // if (wingman_report.y) printf("Y ");
    // if (wingman_report.z) printf("Z ");
    // if (wingman_report.l) printf("L ");
    // if (wingman_report.r) printf("R ");
    // if (wingman_report.mode) printf("Mode ");
    // if (wingman_report.s) printf("S ");
    // printf("\r\n");

    int threshold = 28;
    bool dpad_up    = (wingman_report.dpad == 0 || wingman_report.dpad == 1 ||
                        wingman_report.dpad == 7 || wingman_report.analog_y < (128 - threshold));
    bool dpad_right = ((wingman_report.dpad >= 1 && wingman_report.dpad <= 3) || wingman_report.analog_x > (128 + threshold));
    bool dpad_down  = ((wingman_report.dpad >= 3 && wingman_report.dpad <= 5) || wingman_report.analog_y > (128 + threshold));
    bool dpad_left  = ((wingman_report.dpad >= 5 && wingman_report.dpad <= 7) || wingman_report.analog_x < (128 - threshold));
    bool has_6btns = true;

    buttons = (((wingman_report.z) ? 0x00 : 0x8000) |  // VI
               ((wingman_report.y) ? 0x00 : 0x4000) |  // V
               ((wingman_report.x) ? 0x00 : 0x2000) |  // IV
               ((wingman_report.a) ? 0x00 : 0x1000) |  // III
               ((has_6btns)        ? 0x00 : 0xFF00) |
               ((dpad_left)        ? 0x00 : 0x08) |
               ((dpad_down)        ? 0x00 : 0x04) |
               ((dpad_right)       ? 0x00 : 0x02) |
               ((dpad_up)          ? 0x00 : 0x01) |
               ((wingman_report.s) ? 0x00 : 0x80) |    // Run
               ((wingman_report.mode) ? 0x00 : 0x40) | // Select
               ((wingman_report.b) ? 0x00 : 0x20) |    // II
               ((wingman_report.c) ? 0x00 : 0x10));    // I

    // add to accumulator and post to the state machine
    // if a scan from the host machine is ongoing, wait
    post_globals(dev_addr, instance, buttons, 0, 0);

    prev_report[dev_addr-1] = wingman_report;
  }
}

void process_triple_v2(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
  // previous report used to compare for changes
  static triple_v2_report_t prev_report[5][5];

  triple_v2_report_t update_report;
  memcpy(&update_report, report, sizeof(update_report));

  if ( triple_v2_diff_report(&prev_report[dev_addr-1][instance], &update_report) )
  {
    printf("(x, y) = (%u, %u)\r\n", update_report.axis_x, update_report.axis_y);
    if (update_report.b) printf("B ");
    if (update_report.a) printf("A ");
    if (update_report.y) printf("Y ");
    if (update_report.x) printf("X ");
    if (update_report.l) printf("L ");
    if (update_report.r) printf("R ");
    if (update_report.select) printf("Select ");
    if (update_report.start) printf("Start ");
    printf("\r\n");

    int threshold = 28;
    bool dpad_up    = update_report.axis_y ? (update_report.axis_y > (128 - threshold)) : 0;
    bool dpad_right = update_report.axis_x ? (update_report.axis_x < (128 + threshold)) : 0;
    bool dpad_down  = update_report.axis_y ? (update_report.axis_y < (128 + threshold)) : 0;
    bool dpad_left  = update_report.axis_x ? (update_report.axis_x > (128 - threshold)) : 0;
    bool has_6btns = true;

    buttons = (((update_report.r)      ? 0x00 : 0x8000) | // VI
               ((update_report.l)      ? 0x00 : 0x4000) | // V
               ((update_report.y)      ? 0x00 : 0x2000) | // IV
               ((update_report.x)      ? 0x00 : 0x1000) | // III
               ((has_6btns)            ? 0x00 : 0xFF00) |
               ((dpad_left)            ? 0x00 : 0x0008) |
               ((dpad_down)            ? 0x00 : 0x0004) |
               ((dpad_right)           ? 0x00 : 0x0002) |
               ((dpad_up)              ? 0x00 : 0x0001) |
               ((update_report.start)  ? 0x00 : 0x0080) | // Run
               ((update_report.select) ? 0x00 : 0x0040) | // Select
               ((update_report.b)      ? 0x00 : 0x0020) | // II
               ((update_report.a)      ? 0x00 : 0x0010)); // I

    // add to accumulator and post to the state machine
    // if a scan from the host machine is ongoing, wait
    post_globals(dev_addr, instance, buttons, 0, 0);

    prev_report[dev_addr-1][instance] = update_report;
  }
}

void process_triple_v1(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
  // previous report used to compare for changes
  static triple_v1_report_t prev_report[5][5];

  triple_v1_report_t update_report;
  memcpy(&update_report, report, sizeof(update_report));

  if ( triple_v1_diff_report(&prev_report[dev_addr-1][instance], &update_report) )
  {
    printf("(x, y) = (%u, %u)\r\n", update_report.axis_x, update_report.axis_y);
    if (update_report.b) printf("B ");
    if (update_report.a) printf("A ");
    if (update_report.y) printf("Y ");
    if (update_report.x) printf("X ");
    if (update_report.l) printf("L ");
    if (update_report.r) printf("R ");
    if (update_report.select) printf("Select ");
    if (update_report.start) printf("Start ");
    printf("\r\n");

    int threshold = 28;
    bool dpad_up    = update_report.axis_y ? (update_report.axis_y > (128 - threshold)) : 0;
    bool dpad_right = update_report.axis_x ? (update_report.axis_x < (128 + threshold)) : 0;
    bool dpad_down  = update_report.axis_y ? (update_report.axis_y < (128 + threshold)) : 0;
    bool dpad_left  = update_report.axis_x ? (update_report.axis_x > (128 - threshold)) : 0;
    bool has_6btns = true;

    buttons = (((update_report.r)      ? 0x00 : 0x8000) | // VI
               ((update_report.l)      ? 0x00 : 0x4000) | // V
               ((update_report.y)      ? 0x00 : 0x2000) | // IV
               ((update_report.x)      ? 0x00 : 0x1000) | // III
               ((has_6btns)            ? 0x00 : 0xFF00) |
               ((dpad_left)            ? 0x00 : 0x0008) |
               ((dpad_down)            ? 0x00 : 0x0004) |
               ((dpad_right)           ? 0x00 : 0x0002) |
               ((dpad_up)              ? 0x00 : 0x0001) |
               ((update_report.start)  ? 0x00 : 0x0080) | // Run
               ((update_report.select) ? 0x00 : 0x0040) | // Select
               ((update_report.b)      ? 0x00 : 0x0020) | // II
               ((update_report.a)      ? 0x00 : 0x0010)); // I

    // add to accumulator and post to the state machine
    // if a scan from the host machine is ongoing, wait
    post_globals(dev_addr, instance, buttons, 0, 0);

    prev_report[dev_addr-1][instance] = update_report;
  }
}

void process_pokken(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
  // previous report used to compare for changes
  static pokken_report_t prev_report[5][5];

  pokken_report_t update_report;
  memcpy(&update_report, report, sizeof(update_report));

  if ( pokken_diff_report(&prev_report[dev_addr-1][instance], &update_report) )
  {
    printf("DPad = %d ", update_report.dpad);
    if (update_report.y) printf("Y ");
    if (update_report.b) printf("B ");
    if (update_report.a) printf("A ");
    if (update_report.x) printf("X ");
    if (update_report.l) printf("L ");
    if (update_report.r) printf("R ");
    if (update_report.zl) printf("ZL ");
    if (update_report.zr) printf("ZR ");
    if (update_report.select) printf("Select ");
    if (update_report.start) printf("Start ");
    printf("\r\n");

    bool dpad_up    = (update_report.dpad == 0 || update_report.dpad == 1 || update_report.dpad == 7);
    bool dpad_right = (update_report.dpad >= 1 && update_report.dpad <= 3);
    bool dpad_down  = (update_report.dpad >= 3 && update_report.dpad <= 5);
    bool dpad_left  = (update_report.dpad >= 5 && update_report.dpad <= 7);
    bool has_6btns = true;

    buttons = (((update_report.r || update_report.zr) ? 0x00 : 0x8000) | // VI
               ((update_report.l || update_report.zl) ? 0x00 : 0x4000) | // V
               ((update_report.y)      ? 0x00 : 0x2000) | // IV
               ((update_report.x)      ? 0x00 : 0x1000) | // III
               ((has_6btns)            ? 0x00 : 0xFF00) |
               ((dpad_left)            ? 0x00 : 0x0008) |
               ((dpad_down)            ? 0x00 : 0x0004) |
               ((dpad_right)           ? 0x00 : 0x0002) |
               ((dpad_up)              ? 0x00 : 0x0001) |
               ((update_report.start)  ? 0x00 : 0x0080) | // Run
               ((update_report.select) ? 0x00 : 0x0040) | // Select
               ((update_report.b)      ? 0x00 : 0x0020) | // II
               ((update_report.a)      ? 0x00 : 0x0010)); // I

    // add to accumulator and post to the state machine
    // if a scan from the host machine is ongoing, wait
    post_globals(dev_addr, instance, buttons, 0, 0);

    prev_report[dev_addr-1][instance] = update_report;
  }
}

void print_report(switch_report_01_t* report, uint32_t length) {
    printf("Bytes: ");
    for(uint32_t i = 0; i < length; i++) {
        printf("%02X ", report->buf[i]);
    }
    printf("\n");
}

void process_switch(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
  // previous report used to compare for changes
  static switch_report_t prev_report[5][5];

  switch_report_t update_report;
  memcpy(&update_report, report, sizeof(update_report));

  if (update_report.report_id == 0x30) {
    devices[dev_addr].instances[instance].switch_usb_enable_ack = true;

    if (switch_diff_report(&prev_report[dev_addr-1][instance], &update_report)) {
      uint16_t left_stick_x = update_report.left_stick[0] | ((update_report.left_stick[1] & 0x0F) << 8);
      uint16_t left_stick_y = (update_report.left_stick[1] >> 4) | (update_report.left_stick[2] << 4);
      uint16_t right_stick_x = update_report.right_stick[0] | ((update_report.right_stick[1] & 0x0F) << 8);
      uint16_t right_stick_y = (update_report.right_stick[1] >> 4) | (update_report.right_stick[2] << 4);

      printf("SWITCH[%d|%d]: Report ID = 0x%x\r\n", dev_addr, instance, update_report.report_id);
      printf("(lx, ly, rx, ry) = (%u, %u, %u, %u)\r\n", left_stick_x, left_stick_y, right_stick_x, right_stick_y);
      printf("DPad = ");

      if (update_report.down) printf("Down ");
      if (update_report.up) printf("Up ");
      if (update_report.right) printf("Right ");
      if (update_report.left ) printf("Left ");
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
      if (update_report.sl_r) printf("sl_r ");
      if (update_report.sr_l) printf("sr_l ");
      if (update_report.sl_l) printf("sl_l ");
      printf("\r\n");

      bool has_6btns = true;
      int threshold = 256;
      bool dpad_up    = (update_report.up) || left_stick_y > (2048 + threshold);
      bool dpad_right = (update_report.right) || left_stick_x > (2048 + threshold);
      bool dpad_down  = (update_report.down) || left_stick_y < (2048 - threshold);
      bool dpad_left  = (update_report.left) || left_stick_x < (2048 - threshold);
      bool bttn_1 = update_report.a;
      bool bttn_2 = update_report.b;
      bool bttn_3 = update_report.x;
      bool bttn_4 = update_report.y;
      bool bttn_5 = update_report.l || update_report.zl;
      bool bttn_6 = update_report.r || update_report.zr;
      bool bttn_sel = update_report.select || update_report.home;
      bool bttn_run = update_report.start || update_report.home;

      bool is_left_joycon = (!right_stick_x && !right_stick_y);
      bool is_right_joycon = (!left_stick_x && !left_stick_y);

      if (is_left_joycon) {
        dpad_up    = update_report.up || (left_stick_y > (2048 + threshold));
        dpad_right = update_report.right || (left_stick_x > (2048 + threshold));
        dpad_down  = update_report.down || (left_stick_y < (2048 - threshold));
        dpad_left  = update_report.left || (left_stick_x < (2048 - threshold));
        // bttn_1 = update_report.right;
        // bttn_2 = update_report.down;
        // bttn_3 = update_report.up;
        // bttn_4 = update_report.left;
        bttn_5 = update_report.l;
        bttn_6 = update_report.zl;
        bttn_sel = update_report.select || update_report.cap;
        bttn_run = false; // update_report.select;
      }
      else if (is_right_joycon) {
        dpad_up    = false; // (right_stick_y > (2048 + threshold));
        dpad_right = false; // (right_stick_x > (2048 + threshold));
        dpad_down  = false; // (right_stick_y < (2048 - threshold));
        dpad_left  = false; // (right_stick_x < (2048 - threshold));
        bttn_sel = update_report.home;
        // bttn_run = update_report.start;
      }

      buttons = (
        ((bttn_6)     ? 0x00 : 0x8000) | // VI
        ((bttn_5)     ? 0x00 : 0x4000) | // V
        ((bttn_4)     ? 0x00 : 0x2000) | // IV
        ((bttn_3)     ? 0x00 : 0x1000) | // III
        ((has_6btns)  ? 0x00 : 0xFF00) |
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
      post_globals(dev_addr, is_root ? instance : -1, buttons, 0, 0);

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

void process_nes_usb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
  // previous report used to compare for changes
  static nes_usb_report_t prev_report[5][5];

  nes_usb_report_t update_report;
  memcpy(&update_report, report, sizeof(update_report));

  if ( nes_usb_diff_report(&prev_report[dev_addr-1][instance], &update_report) )
  {
    printf("(x, y) = (%u, %u)\r\n", update_report.axis0_x, update_report.axis0_y);
    // Y,X,L,R extra button data may or may not be used by similiar generic controller variants
    if (update_report.y) printf("Y ");
    if (update_report.b) printf("B ");
    if (update_report.a) printf("A ");
    if (update_report.x) printf("X ");
    if (update_report.l) printf("L ");
    if (update_report.r) printf("R ");
    if (update_report.select) printf("Select ");
    if (update_report.start) printf("Start ");
    printf("\r\n");

    bool dpad_left  = (update_report.axis0_x < 127);
    bool dpad_right  = (update_report.axis0_x > 127);
    bool dpad_up  = (update_report.axis0_y < 127);
    bool dpad_down  = (update_report.axis0_y > 127);
    bool has_6btns = false;

    buttons = (((update_report.r)      ? 0x00 : 0x8000) | // VI
               ((update_report.l)      ? 0x00 : 0x4000) | // V
               ((update_report.y)      ? 0x00 : 0x2000) | // IV
               ((update_report.x)      ? 0x00 : 0x1000) | // III
               ((has_6btns)            ? 0x00 : 0xFF00) |
               ((dpad_left)            ? 0x00 : 0x0008) |
               ((dpad_down)            ? 0x00 : 0x0004) |
               ((dpad_right)           ? 0x00 : 0x0002) |
               ((dpad_up)              ? 0x00 : 0x0001) |
               ((update_report.start)  ? 0x00 : 0x0080) | // Run
               ((update_report.select) ? 0x00 : 0x0040) | // Select
               ((update_report.b)      ? 0x00 : 0x0020) | // II
               ((update_report.a)      ? 0x00 : 0x0010)); // I

    // add to accumulator and post to the state machine
    // if a scan from the host machine is ongoing, wait
    post_globals(dev_addr, instance, buttons, 0, 0);

    prev_report[dev_addr-1][instance] = update_report;
  }
}
// Invoked when received report from device via interrupt endpoint
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
  uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

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
      if      ( is_sony_ds3(dev_addr) ) process_sony_ds3(dev_addr, instance, report, len);
      else if ( is_sony_ds4(dev_addr) ) process_sony_ds4(dev_addr, instance, report, len);
      else if ( is_sony_ds5(dev_addr) ) process_sony_ds5(dev_addr, instance, report, len);
      else if ( is_sony_psc(dev_addr) ) process_sony_psc(dev_addr, instance, report, len);
      else if ( is_8bit_pce(dev_addr) ) process_8bit_pce(dev_addr, instance, report, len);
      else if ( is_8bit_m30(dev_addr) ) process_8bit_m30(dev_addr, instance, report, len);
      else if ( is_sega_mini(dev_addr) ) process_sega_mini(dev_addr, instance, report, len);
      else if ( is_astro_city(dev_addr) ) process_astro_city(dev_addr, instance, report, len);
      else if ( is_wing_man(dev_addr) ) process_wing_man(dev_addr, instance, report, len);
      else if ( is_triple_v2(dev_addr) ) process_triple_v2(dev_addr, instance, report, len);
      else if ( is_triple_v1(dev_addr) ) process_triple_v1(dev_addr, instance, report, len);
      else if ( is_pokken(dev_addr) ) process_pokken(dev_addr, instance, report, len);
      else if ( is_switch(dev_addr) ) process_switch(dev_addr, instance, report, len);
      else if ( is_nes_usb(dev_addr) ) process_nes_usb(dev_addr, instance, report, len);
      else {
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

  bool has_6btns = true;
  bool dpad_left = false, dpad_down = false, dpad_right = false, dpad_up = false,
    btns_run = false, btns_sel = false, btns_one = false, btns_two = false,
    btns_three = false, btns_four = false, btns_five = false, btns_six = false;

  //------------- example code ignore control (non-printable) key affects -------------//
  for(uint8_t i=0; i<6; i++)
  {
    if ( report->keycode[i] )
    {
      if (report->keycode[i] == 40) btns_run = true; // Enter
      if (report->keycode[i] == 41) btns_sel = true; // ESC
      if (report->keycode[i] == 26 || report->keycode[i] == 82) dpad_up = true; // W or Arrow
      if (report->keycode[i] == 4  || report->keycode[i] == 80) dpad_left = true; // A or Arrow
      if (report->keycode[i] == 22 || report->keycode[i] == 81) dpad_down = true; // S or Arrow
      if (report->keycode[i] == 7  || report->keycode[i] == 79) dpad_right = true; // D or Arrow
      if (report->keycode[i] == 89) btns_one = true;
      if (report->keycode[i] == 90) btns_two = true;
      if (report->keycode[i] == 91) btns_three = true;
      if (report->keycode[i] == 92) btns_four = true;
      if (report->keycode[i] == 93) btns_five = true;
      if (report->keycode[i] == 94) btns_six = true;

      if ( find_key_in_report(&prev_report, report->keycode[i]) )
      {
        // exist in previous report means the current key is holding
      }else
      {
        // printf("keycode(%d)\r\n", report->keycode[i]);
        // not existed in previous report means the current key is pressed
        bool const is_shift = report->modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT);
        uint8_t ch = keycode2ascii[report->keycode[i]][is_shift ? 1 : 0];
        putchar(ch);
        if ( ch == '\r' ) putchar('\n'); // added new line for enter key

        fflush(stdout); // flush right away, else nanolib will wait for newline
      }
    }
    // TODO example skips key released
  }

  buttons = (((btns_six)   ? 0x00 : 0x8000) |
             ((btns_five)  ? 0x00 : 0x4000) |
             ((btns_four)  ? 0x00 : 0x2000) |
             ((btns_three) ? 0x00 : 0x1000) |
             ((has_6btns)  ? 0x00 : 0xFF00) |
             ((dpad_left)  ? 0x00 : 0x0008) |
             ((dpad_down)  ? 0x00 : 0x0004) |
             ((dpad_right) ? 0x00 : 0x0002) |
             ((dpad_up)    ? 0x00 : 0x0001) |
             ((btns_run)   ? 0x00 : 0x0080) |
             ((btns_sel)   ? 0x00 : 0x0040) |
             ((btns_two)   ? 0x00 : 0x0020) |
             ((btns_one)   ? 0x00 : 0x0010));
  post_globals(dev_addr, instance, buttons, 0, 0);

  prev_report = *report;
}

//--------------------------------------------------------------------+
// Mouse
//--------------------------------------------------------------------+

void cursor_movement(int8_t x, int8_t y, int8_t wheel)
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
  printf("(%d %d %d)\r\n", x, y, wheel);
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
     buttons = (((0xFF00)) | // no six button controller byte
                ((report->buttons & MOUSE_BUTTON_BACKWARD) ? 0x00 : 0x80) |
                ((report->buttons & MOUSE_BUTTON_FORWARD ) ? 0x00 : 0x40) |
                ((report->buttons & MOUSE_BUTTON_RIGHT)    ? 0x00 : 0x20) |
                ((report->buttons & MOUSE_BUTTON_LEFT)     ? 0x00 : 0x10));
  }
  else
  {
     buttons = (((0xFF00)) |
                ((report->buttons & MOUSE_BUTTON_BACKWARD) ? 0x00 : 0x80) |
                ((report->buttons & MOUSE_BUTTON_FORWARD ) ? 0x00 : 0x40) |
                ((report->buttons & MOUSE_BUTTON_LEFT)     ? 0x00 : 0x20) |
                ((report->buttons & MOUSE_BUTTON_RIGHT)    ? 0x00 : 0x10));
  }

  local_x = (0 - report->x);
  local_y = (0 - report->y);

  // add to accumulator and post to the state machine
  // if a scan from the host machine is ongoing, wait
  post_globals(dev_addr, instance, buttons, local_x, local_y);

  //------------- cursor movement -------------//
  cursor_movement(report->x, report->y, report->wheel);
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
    btns_three = false, btns_four = false, btns_five = false, btns_six = false;

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

  buttons = (((btns_six)   ? 0x00 : 0x8000) |
             ((btns_five)  ? 0x00 : 0x4000) |
             ((btns_four)  ? 0x00 : 0x2000) |
             ((btns_three) ? 0x00 : 0x1000) |
             ((has_6btns)  ? 0x00 : 0xFF00) |
             ((dpad_left)  ? 0x00 : 0x0008) |
             ((dpad_down)  ? 0x00 : 0x0004) |
             ((dpad_right) ? 0x00 : 0x0002) |
             ((dpad_up)    ? 0x00 : 0x0001) |
             ((btns_run)   ? 0x00 : 0x0080) |
             ((btns_sel)   ? 0x00 : 0x0040) |
             ((btns_two)   ? 0x00 : 0x0020) |
             ((btns_one)   ? 0x00 : 0x0010));
  post_globals(dev_addr, instance, buttons, 0, 0);

  prev_report = *report;
}

// Parses report with parsed HID descriptor byteIndexes & bitMasks
void parse_hid_report(uint8_t dev_addr, uint8_t instance, uint8_t const *report, uint16_t len)
{
  static pad_buttons previous[5][5];
  pad_buttons current = {0};
  current.value = 0;

  uint8_t xValue = report[devices[dev_addr].instances[instance].xLoc.byteIndex] & devices[dev_addr].instances[instance].xLoc.bitMask;
  uint8_t yValue = report[devices[dev_addr].instances[instance].yLoc.byteIndex] & devices[dev_addr].instances[instance].yLoc.bitMask;
  uint8_t hatValue = report[devices[dev_addr].instances[instance].hatLoc.byteIndex] & devices[dev_addr].instances[instance].hatLoc.bitMask;

  // parse hat from report
  if (devices[dev_addr].instances[instance].hatLoc.bitMask) {
    current.all_direction |= HAT_SWITCH_TO_DIRECTION_BUTTONS[hatValue];
  }

  // parse buttons from report
  current.all_buttons = 0;
  for (int i = 0; i < 12; i++) {
    if (report[devices[dev_addr].instances[instance].buttonLoc[i].byteIndex] & devices[dev_addr].instances[instance].buttonLoc[i].bitMask) {
      current.all_buttons |= (0x01 << i);
    }
  }

  // parse analog from report
  if (devices[dev_addr].instances[instance].xLoc.bitMask && devices[dev_addr].instances[instance].yLoc.bitMask) {
    // parse x-axis from report
    uint32_t range_half = devices[dev_addr].instances[instance].xLoc.mid;
    uint32_t dead_zone_range = range_half / DEAD_ZONE;
    if (xValue < (range_half - dead_zone_range) && xValue != 0x01)
    {
      current.left |= 1;
    }
    else if (xValue > (range_half + dead_zone_range))
    {
      current.right |= 1;
    }

    // parse y-axis from report
    range_half = devices[dev_addr].instances[instance].yLoc.mid;
    dead_zone_range = range_half / DEAD_ZONE;
    if (yValue < (range_half - dead_zone_range))
    {
      current.up |= 1;
    }
    else if (yValue > (range_half + dead_zone_range))
    {
      current.down |= 1;
    }
  }

  if (previous[dev_addr-1][instance].value != current.value)
  {
    previous[dev_addr-1][instance] = current;

    // printf("Generic HID Report: ");
    // printf("Button Count: %d\n", devices[dev_addr].instances[instance].buttonCnt);
    // printf(" xValue:%d yValue:%d dPad:%d \n",xValue, yValue, hatValue);
    // for (int i = 0; i < 12; i++) {
    //   printf(" B%d:%d", i + 1, (report[devices[dev_addr].instances[instance].buttonLoc[i].byteIndex] & devices[dev_addr].instances[instance].buttonLoc[i].bitMask) ? 1 : 0 );
    // }
    // printf("\n");

    uint8_t buttonCount = devices[dev_addr].instances[instance].buttonCnt;
    if (buttonCount > 12) buttonCount = 12;
    bool buttonSelect = current.all_buttons & (0x01 << (buttonCount-2));
    bool buttonStart = current.all_buttons & (0x01 << (buttonCount-1));
    bool button5 = buttonCount >=7 ? current.button5 : 0;
    bool button6 = buttonCount >=8 ? current.button6 : 0;
    bool has_6btns = buttonCount >= 6;

    buttons = (((button6)         ? 0x00 : 0x8000) |
               ((button5)         ? 0x00 : 0x4000) |
               ((current.button4) ? 0x00 : 0x2000) |
               ((current.button3) ? 0x00 : 0x1000) |
               ((has_6btns)       ? 0x00 : 0x0F00) |
               ((current.left)    ? 0x00 : 0x0008) |
               ((current.down)    ? 0x00 : 0x0004) |
               ((current.right)   ? 0x00 : 0x0002) |
               ((current.up)      ? 0x00 : 0x0001) |
               ((buttonStart)     ? 0x00 : 0x0080) |
               ((buttonSelect)    ? 0x00 : 0x0040) |
               ((current.button2) ? 0x00 : 0x0020) |
               ((current.button1) ? 0x00 : 0x0010));
    post_globals(dev_addr, instance, buttons, 0, 0);
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
