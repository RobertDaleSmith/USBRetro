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

const char* dpad_str[] = { "N", "NE", "E", "SE", "S", "SW", "W", "NW", "none" };

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

  // comment out since not used by this example
  uint8_t l2_trigger; // 0 released, 0xff fully pressed
  uint8_t r2_trigger; // as above

  uint16_t timestamp;
  uint8_t  battery;
  int16_t  gyro[3];  // x, y, z;
  int16_t  accel[3]; // x, y, z
  int8_t   unknown_a[5]; // who knows?
  uint8_t  headset;
  int8_t   unknown_b[2]; // future use?

  struct {
    uint8_t tpad_event : 4; // track pad event 0x01 = 2 finger tap; 0x02 last on edge?
    uint8_t unknown_c  : 4; // future use?
  };

  uint8_t  tpad_counter;

  struct {
    uint8_t tpad_f1_count : 7;
    uint8_t tpad_f1_down  : 1;
  };

  int8_t tpad_f1_pos[3];

  // struct {
  //   uint8_t tpad_f2_count : 7;
  //   uint8_t tpad_f2_down  : 1;
  // };

  // int8_t tpad_f2_pos[3];

  // struct {
  //   uint8_t tpad_f1_count_prev : 7;
  //   uint8_t tpad_f1_down_prev  : 1;
  // };

  // int8_t [3];

} sony_ds4_report_t;

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
    uint8_t counter : 5; // +1 each report
  };

  int16_t  gyro[3];  // x, y, z;
  int16_t  accel[3]; // x, y, z
  int8_t   unknown_a[5]; // who knows?
  uint8_t  headset;
  int8_t   unknown_b[2]; // future use?

  struct {
    uint8_t tpad_event : 4; // track pad event 0x01 = 2 finger tap; 0x02 last on edge?
    uint8_t unknown_c  : 4; // future use?
  };

  uint8_t  tpad_counter;

  struct {
    uint8_t tpad_f1_count : 7;
    uint8_t tpad_f1_down  : 1;
  };

  int8_t tpad_f1_pos[3];

} sony_ds5_report_t;

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

} bitdo_psc_report_t;

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

// check if device is Sony DualShock 4
static inline bool is_sony_ds4(uint8_t dev_addr)
{
  uint16_t vid, pid;
  tuh_vid_pid_get(dev_addr, &vid, &pid);

  return ( (vid == 0x054c && (pid == 0x09cc || pid == 0x05c4)) // Sony DualShock4 
           || (vid == 0x0f0d && pid == 0x005e)                 // Hori FC4 
           || (vid == 0x0f0d && pid == 0x00ee)                 // Hori PS4 Mini (PS4-099U) 
           || (vid == 0x1f4f && pid == 0x1002)                 // ASW GG xrd controller
         );
}

// check if device is 8BitDo PCE Controller
static inline bool is_8bit_pce(uint8_t dev_addr)
{
  uint16_t vid, pid;
  tuh_vid_pid_get(dev_addr, &vid, &pid);

  return ((vid == 0x0f0d && pid == 0x0138)); // 8BitDo PCE (wireless)
}

// check if device is PlayStation Classic Controller
static inline bool is_8bit_psc(uint8_t dev_addr)
{
  uint16_t vid, pid;
  tuh_vid_pid_get(dev_addr, &vid, &pid);

  return ((vid == 0x054c && pid == 0x0cda)); // PSClassic Controller
}

// check if device is Sega Genesis mini controller
static inline bool is_sega_mini(uint8_t dev_addr)
{
  uint16_t vid, pid;
  tuh_vid_pid_get(dev_addr, &vid, &pid);

  return ((vid == 0x0f0d && pid == 0x00c1)); // Sega Genesis mini controller
}

// check if device is Astro City mini controller
static inline bool is_astro_city(uint8_t dev_addr)
{
  uint16_t vid, pid;
  tuh_vid_pid_get(dev_addr, &vid, &pid);

  return ((vid == 0x0ca3 && (
           pid == 0x0027 ||  // Astro City mini controller
           pid == 0x0024    // 8BitDo M30 6-button controller
         )));
}

// check if device is Sony DS5 controller
static inline bool is_sony_ds5(uint8_t dev_addr)
{
  uint16_t vid, pid;
  tuh_vid_pid_get(dev_addr, &vid, &pid);

  return ((vid == 0x054c && pid == 0x0ce6)); // Sony DS5 controller
}

// check if device is Logitech WingMan Action controller
static inline bool is_wing_man(uint8_t dev_addr)
{
  uint16_t vid, pid;
  tuh_vid_pid_get(dev_addr, &vid, &pid);

  return ((vid == 0x046d && pid == 0xc20b)); // Logitech WingMan Action controller
}

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+

// If your host terminal support ansi escape code such as TeraTerm
// it can be use to simulate mouse cursor movement within terminal
#define USE_ANSI_ESCAPE   0

#define MAX_REPORT  4

// Uncomment the following line if you desire button-swap when middle button is clicked:
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

int16_t spinner = 0;
uint16_t tpadLastPos = 0;
bool tpadDragging = false;

// Each HID instance can has multiple reports
static struct
{
  uint8_t report_count;
  tuh_hid_report_info_t report_info[MAX_REPORT];
}hid_info[CFG_TUH_HID];

static void process_kbd_report(uint8_t dev_addr, uint8_t instance, hid_keyboard_report_t const *report);
static void process_mouse_report(uint8_t dev_addr, uint8_t instance, hid_mouse_report_t const * report);
static void process_generic_report(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len);

extern void __not_in_flash_func(post_globals)(
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
);

void hid_app_task(void)
{
  // nothing to do
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
  printf("HID device address = %d, instance = %d is mounted\r\n", dev_addr, instance);

  // Interface protocol (hid_interface_protocol_enum_t)
  const char* protocol_str[] = { "None", "Keyboard", "Mouse" };
  uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

  printf("HID Interface Protocol = %s\r\n", protocol_str[itf_protocol]);

  // By default host stack will use activate boot protocol on supported interface.
  // Therefore for this simple example, we only need to parse generic report descriptor (with built-in parser)
  bool isController = is_sony_ds4(dev_addr)
                   || is_sony_ds5(dev_addr)
                   || is_8bit_pce(dev_addr)
                   || is_8bit_psc(dev_addr)
                   || is_astro_city(dev_addr)
                   || is_sega_mini(dev_addr)
                   || is_wing_man(dev_addr)
                   ;
  if ( !isController && itf_protocol == HID_ITF_PROTOCOL_NONE )
  {
    hid_info[instance].report_count = tuh_hid_parse_report_descriptor(hid_info[instance].report_info, MAX_REPORT, desc_report, desc_len);
    printf("HID has %u reports \r\n", hid_info[instance].report_count);
  }

  // request to receive report
  // tuh_hid_report_received_cb() will be invoked when report is available
  if ( !tuh_hid_receive_report(dev_addr, instance) )
  {
    printf("Error: cannot request to receive report\r\n");
  }
}

// Invoked when device with hid interface is un-mounted
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
{
  printf("HID device address = %d, instance = %d is unmounted\r\n", dev_addr, instance);
}

// check if different than 2
bool diff_than_2(uint8_t x, uint8_t y)
{
  return (x - y > 2) || (y - x > 2);
}

// check if 2 reports are different enough
bool ds4_diff_report(sony_ds4_report_t const* rpt1, sony_ds4_report_t const* rpt2)
{
  bool result;

  // x, y, z, rz must different than 2 to be counted
  result = diff_than_2(rpt1->x, rpt2->x) || diff_than_2(rpt1->y , rpt2->y ) ||
           diff_than_2(rpt1->z, rpt2->z) || diff_than_2(rpt1->rz, rpt2->rz);

  // check the reset with mem compare
  result |= memcmp(&rpt1->rz + 1, &rpt2->rz + 1, sizeof(sony_ds4_report_t)-4);

  return result;
}

bool ds5_diff_report(sony_ds5_report_t const* rpt1, sony_ds5_report_t const* rpt2)
{
  bool result;

  // x1, y1, x2, y2, rx, ry must different than 2 to be counted
  result = diff_than_2(rpt1->x1, rpt2->x1) || diff_than_2(rpt1->y1, rpt2->y1) ||
           diff_than_2(rpt1->x2, rpt2->x2) || diff_than_2(rpt1->y2, rpt2->y2) ||
           diff_than_2(rpt1->rx, rpt2->rx) || diff_than_2(rpt1->ry, rpt2->ry);

  // check the reset with mem compare
  result |= memcmp(&rpt1->rz + 1, &rpt2->rz + 1, sizeof(sony_ds5_report_t)-7);

  return result;
}

bool psc_diff_report(bitdo_psc_report_t const* rpt1, bitdo_psc_report_t const* rpt2)
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

      if (!ds4_report.tpad_f1_down) printf("F1 ");

      uint16_t tx = (((ds4_report.tpad_f1_pos[1] & 0x0f) << 8)) | ((ds4_report.tpad_f1_pos[0] & 0xff) << 0);
      uint16_t ty = (((ds4_report.tpad_f1_pos[1] & 0xf0) >> 4)) | ((ds4_report.tpad_f1_pos[2] & 0xff) << 4);
      printf(" (tx, ty) = (%u, %u)\r\n", tx, ty);
      printf("\r\n");

      int threshold = 28;
      bool dpad_up    = (ds4_report.dpad == 0 || ds4_report.dpad == 1 || ds4_report.dpad == 7);
      bool dpad_right = ((ds4_report.dpad >= 1 && ds4_report.dpad <= 3));
      bool dpad_down  = ((ds4_report.dpad >= 3 && ds4_report.dpad <= 5));
      bool dpad_left  = ((ds4_report.dpad >= 5 && ds4_report.dpad <= 7));
      bool buttons_a  = (ds4_report.cross || ds4_report.tpad);

      buttons = (((ds4_report.circle)   ? 0x8000 : 0x00) | //C-DOWN
                 ((buttons_a)           ? 0x4000 : 0x00) | //A
                 ((ds4_report.option)   ? 0x2000 : 0x00) | //START
                 ((ds4_report.share)    ? 0x1000 : 0x00) | //NUON
                 ((dpad_down)           ? 0x0800 : 0x00) | //D-DOWN
                 ((dpad_left)           ? 0x0400 : 0x00) | //D-LEFT
                 ((dpad_up)             ? 0x0200 : 0x00) | //D-UP
                 ((dpad_right)          ? 0x0100 : 0x00) | //D-RIGHT
                 ((1)                   ? 0x0080 : 0x00) |
                 ((0)                   ? 0x0040 : 0x00) |
                 ((ds4_report.l1)       ? 0x0020 : 0x00) | //L
                 ((ds4_report.r1)       ? 0x0010 : 0x00) | //R
                 ((ds4_report.square)   ? 0x0008 : 0x00) | //B
                 ((ds4_report.triangle) ? 0x0004 : 0x00) | //C-LEFT
                 ((ds4_report.l2)       ? 0x0002 : 0x00) | //C-UP
                 ((ds4_report.r2)       ? 0x0001 : 0x00)); //C-RIGHT

      // Touch Pad - Atari50 Tempest like spinner input
      if (!ds4_report.tpad_f1_down) {
        // scroll spinner value while swipping
        if (tpadDragging) {
          // get directional difference delta
          int16_t delta = 0;
          if (tx >= tpadLastPos) delta = tx - tpadLastPos;
          else delta = (-1) * (tpadLastPos - tx);

          // check max/min delta value
          if (delta > 12) delta = 12;
          if (delta < -12) delta = -12;

          // inc global spinner value by delta
          spinner += delta;

          // check max/min spinner value
          if (spinner > 255) spinner -= 255;
          if (spinner < 0) spinner = 256 - (-1 * spinner);
        }

        tpadLastPos = tx;
        tpadDragging = true;
      } else {
        tpadDragging = false;
      }
      // printf(" (spinner) = (%u)\r\n", spinner);

      uint8_t analog_1x = ds4_report.x+1;
      uint8_t analog_1y = ds4_report.y+1;
      uint8_t analog_2x = ds4_report.z+1;
      uint8_t analog_2y = ds4_report.rz+1;
      if (analog_1x == 0) analog_1x = 255;
      if (analog_1y == 0) analog_1y = 255;
      if (analog_2x == 0) analog_2x = 255;
      if (analog_2y == 0) analog_2y = 255;

      // add to accumulator and post to the state machine
      // if a scan from the host machine is ongoing, wait
      post_globals(
        dev_addr,
        instance,
        buttons,
        true,      // analog_1 enabled
        analog_1x, // analog_1x
        analog_1y, // analog_1y
        true,      // analog_2 enabled
        analog_2x, // analog_2x
        analog_2y, // analog_2y
        true,      // quad enabled
        spinner    // quad_x
      );
    }

    prev_report[dev_addr-1] = ds4_report;
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

      if (!ds5_report.tpad_f1_down) printf("F1 ");

      uint16_t tx = (((ds5_report.tpad_f1_pos[1] & 0x0f) << 8)) | ((ds5_report.tpad_f1_pos[0] & 0xff) << 0);
      uint16_t ty = (((ds5_report.tpad_f1_pos[1] & 0xf0) >> 4)) | ((ds5_report.tpad_f1_pos[2] & 0xff) << 4);
      printf(" (tx, ty) = (%u, %u)\r\n", tx, ty);
      printf("\r\n");

      int threshold = 28;
      bool dpad_up    = (ds5_report.dpad == 0 || ds5_report.dpad == 1 || ds5_report.dpad == 7);
      bool dpad_right = ((ds5_report.dpad >= 1 && ds5_report.dpad <= 3));
      bool dpad_down  = ((ds5_report.dpad >= 3 && ds5_report.dpad <= 5));
      bool dpad_left  = ((ds5_report.dpad >= 5 && ds5_report.dpad <= 7));

      buttons = (((ds5_report.circle)   ? 0x8000 : 0x00) | //C-DOWN
                 ((ds5_report.cross)    ? 0x4000 : 0x00) | //A
                 ((ds5_report.option)   ? 0x2000 : 0x00) | //START
                 ((ds5_report.share || ds5_report.ps) ? 0x1000 : 0x00) | //NUON
                 ((dpad_down)           ? 0x0800 : 0x00) | //D-DOWN
                 ((dpad_left)           ? 0x0400 : 0x00) | //D-LEFT
                 ((dpad_up)             ? 0x0200 : 0x00) | //D-UP
                 ((dpad_right)          ? 0x0100 : 0x00) | //D-RIGHT
                 ((1)                   ? 0x0080 : 0x00) |
                 ((0)                   ? 0x0040 : 0x00) |
                 ((ds5_report.l1)       ? 0x0020 : 0x00) | //L
                 ((ds5_report.r1)       ? 0x0010 : 0x00) | //R
                 ((ds5_report.square)   ? 0x0008 : 0x00) | //B
                 ((ds5_report.triangle) ? 0x0004 : 0x00) | //C-LEFT
                 ((ds5_report.l2)       ? 0x0002 : 0x00) | //C-UP
                 ((ds5_report.r2)       ? 0x0001 : 0x00)); //C-RIGHT

      // Touch Pad - Atari50 Tempest like spinner input
      if (!ds5_report.tpad_f1_down) {
        // scroll spinner value while swipping
        if (tpadDragging) {
          // get directional difference delta
          int16_t delta = 0;
          if (tx >= tpadLastPos) delta = tx - tpadLastPos;
          else delta = (-1) * (tpadLastPos - tx);

          // check max/min delta value
          if (delta > 12) delta = 12;
          if (delta < -12) delta = -12;

          // inc global spinner value by delta
          spinner += delta;

          // check max/min spinner value
          if (spinner > 255) spinner -= 255;
          if (spinner < 0) spinner = 256 - (-1 * spinner);
        }

        tpadLastPos = tx;
        tpadDragging = true;
      } else {
        tpadDragging = false;
      }
      // printf(" (spinner) = (%u)\r\n", spinner);

      uint8_t analog_1x = ds5_report.x1+1;
      uint8_t analog_1y = ds5_report.y1+1;
      uint8_t analog_2x = ds5_report.x2+1;
      uint8_t analog_2y = ds5_report.y2+1;
      if (analog_1x == 0) analog_1x = 255;
      if (analog_1y == 0) analog_1y = 255;
      if (analog_2x == 0) analog_2x = 255;
      if (analog_2y == 0) analog_2y = 255;

      // add to accumulator and post to the state machine
      // if a scan from the host machine is ongoing, wait
      post_globals(
        dev_addr,
        instance,
        buttons,
        true,      // analog_1 enabled
        analog_1x, // analog_1x
        analog_1y, // analog_1y
        true,      // analog_2 enabled
        analog_2x, // analog_2x
        analog_2y, // analog_2y
        true,      // quad enabled
        spinner    // quad_x
      );
    }

    prev_report[dev_addr-1] = ds5_report;
  }
}
void process_8bit_psc(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
  // previous report used to compare for changes
  static bitdo_psc_report_t prev_report[5] = { 0 };

  bitdo_psc_report_t psc_report;
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

    buttons = (((psc_report.circle)   ? 0x8000 : 0x00) | //C-DOWN
               ((psc_report.cross)    ? 0x4000 : 0x00) | //A
               ((psc_report.option)   ? 0x2000 : 0x00) | //START
               ((psc_report.share)    ? 0x1000 : 0x00) | //NUON
               ((dpad_down)           ? 0x0800 : 0x00) | //D-DOWN
               ((dpad_left)           ? 0x0400 : 0x00) | //D-LEFT
               ((dpad_up)             ? 0x0200 : 0x00) | //D-UP
               ((dpad_right)          ? 0x0100 : 0x00) | //D-RIGHT
               ((1)                   ? 0x0080 : 0x00) |
               ((0)                   ? 0x0040 : 0x00) |
               ((psc_report.l1)       ? 0x0020 : 0x00) | //L
               ((psc_report.r1)       ? 0x0010 : 0x00) | //R
               ((psc_report.square)   ? 0x0008 : 0x00) | //B
               ((psc_report.triangle) ? 0x0004 : 0x00) | //C-LEFT
               ((psc_report.l2)       ? 0x0002 : 0x00) | //C-UP
               ((psc_report.r2)       ? 0x0001 : 0x00)); //C-RIGHT

    // add to accumulator and post to the state machine
    // if a scan from the host machine is ongoing, wait
    post_globals(
      dev_addr,
      instance,
      buttons,
      false, // analog_1 enabled
      0,     // analog_1x
      0,     // analog_1y
      false, // analog_2 enabled
      0,     // analog_2x
      0,     // analog_2y
      false, // quad enabled
      0      // quad_x
    );
  }

  prev_report[dev_addr-1] = psc_report;
}

void process_8bit_pce(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
  // previous report used to compare for changes
  static bitdo_pce_report_t prev_report[5] = { 0 };

  bitdo_pce_report_t pce_report;
  memcpy(&pce_report, report, sizeof(pce_report));

  if ( pce_diff_report(&prev_report[dev_addr-1], &pce_report) )
  {
    printf("DPad = %s ", dpad_str[pce_report.dpad]);

    if (pce_report.sel) printf("Select ");
    if (pce_report.run) printf("Run ");
    if (pce_report.one) printf("I ");
    if (pce_report.two) printf("II ");

    printf("\r\n");

    bool dpad_up    = (pce_report.dpad == 0 || pce_report.dpad == 1 || pce_report.dpad == 7);
    bool dpad_right = (pce_report.dpad >= 1 && pce_report.dpad <= 3);
    bool dpad_down  = (pce_report.dpad >= 3 && pce_report.dpad <= 5);
    bool dpad_left  = (pce_report.dpad >= 5 && pce_report.dpad <= 7);

    buttons = (((pce_report.two) ? 0x4000 : 0x00) | //A
               ((pce_report.run) ? 0x2000 : 0x00) | //START
               ((pce_report.sel) ? 0x1000 : 0x00) | //NUON
               ((dpad_down)      ? 0x0800 : 0x00) | //D-DOWN
               ((dpad_left)      ? 0x0400 : 0x00) | //D-LEFT
               ((dpad_up)        ? 0x0200 : 0x00) | //D-UP
               ((dpad_right)     ? 0x0100 : 0x00) | //D-RIGHT
               ((1)              ? 0x0080 : 0x00) |
               ((0)              ? 0x0040 : 0x00) |
               ((pce_report.one) ? 0x0008 : 0x00)) ; //B

    // add to accumulator and post to the state machine
    // if a scan from the host machine is ongoing, wait
    post_globals(
      dev_addr,
      instance,
      buttons,
      false, // analog_1 enabled
      0,     // analog_1x
      0,     // analog_1y
      false, // analog_2 enabled
      0,     // analog_2x
      0,     // analog_2y
      false, // quad enabled
      0      // quad_x
    );
  }

  prev_report[dev_addr-1] = pce_report;
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

    buttons = (((sega_report.b)     ? 0x8000 : 0x00) | //C-DOWN
               ((sega_report.x)     ? 0x4000 : 0x00) | //A
               ((sega_report.start) ? 0x2000 : 0x00) | //START
               ((sega_report.mode)  ? 0x1000 : 0x00) | //NUON
               ((dpad_down)         ? 0x0800 : 0x00) | //D-DOWN
               ((dpad_left)         ? 0x0400 : 0x00) | //D-LEFT
               ((dpad_up)           ? 0x0200 : 0x00) | //D-UP
               ((dpad_right)        ? 0x0100 : 0x00) | //D-RIGHT
               ((1)                 ? 0x0080 : 0x00) |
               ((0)                 ? 0x0040 : 0x00) |
               ((sega_report.a)     ? 0x0008 : 0x00) | //B
               ((sega_report.y)     ? 0x0004 : 0x00) | //C-LEFT
               ((sega_report.z)     ? 0x0002 : 0x00) | //C-UP
               ((sega_report.c)     ? 0x0001 : 0x00)); //C-RIGHT

    // add to accumulator and post to the state machine
    // if a scan from the host machine is ongoing, wait
    post_globals(
      dev_addr,
      instance,
      buttons,
      false, // analog_1 enabled
      0,     // analog_1x
      0,     // analog_1y
      false, // analog_2 enabled
      0,     // analog_2x
      0,     // analog_2y
      false, // quad enabled
      0      // quad_x
    );
  }

  prev_report[dev_addr-1] = sega_report;
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
    if (astro_report.a) printf("A ");
    if (astro_report.b) printf("B ");
    if (astro_report.c) printf("C ");
    if (astro_report.d) printf("D ");
    if (astro_report.e) printf("E ");
    if (astro_report.f) printf("F ");
    if (astro_report.l) printf("L ");
    if (astro_report.r) printf("R ");
    if (astro_report.credit) printf("Credit ");
    if (astro_report.start) printf("Start ");
    printf("\r\n");

    bool dpad_up    = (astro_report.y < 127);
    bool dpad_right = (astro_report.x > 127);
    bool dpad_down  = (astro_report.y > 127);
    bool dpad_left  = (astro_report.x < 127);

    buttons = (((astro_report.b)      ? 0x8000 : 0x00) | //C-DOWN
               ((astro_report.f)      ? 0x4000 : 0x00) | //A
               ((astro_report.start)  ? 0x2000 : 0x00) | //START
               ((astro_report.credit) ? 0x1000 : 0x00) | //NUON
               ((dpad_down)           ? 0x0800 : 0x00) | //D-DOWN
               ((dpad_left)           ? 0x0400 : 0x00) | //D-LEFT
               ((dpad_up)             ? 0x0200 : 0x00) | //D-UP
               ((dpad_right)          ? 0x0100 : 0x00) | //D-RIGHT
               ((1)                   ? 0x0080 : 0x00) |
               ((0)                   ? 0x0040 : 0x00) |
               ((astro_report.l)      ? 0x0020 : 0x00) | //L
               ((astro_report.r)      ? 0x0010 : 0x00) | //R
               ((astro_report.c)      ? 0x0008 : 0x00) | //B
               ((astro_report.e)      ? 0x0004 : 0x00) | //C-LEFT
               ((astro_report.d)      ? 0x0002 : 0x00) | //C-UP
               ((astro_report.a)      ? 0x0001 : 0x00)); //C-RIGHT

    // add to accumulator and post to the state machine
    // if a scan from the host machine is ongoing, wait
    post_globals(
      dev_addr,
      instance,
      buttons,
      false,  // analog_1 enabled
      0,      // analog_1x
      0,      // analog_1y
      false,  // analog_2 enabled
      0,      // analog_2x
      0,      // analog_2y
      false,  // quad enabled
      0       // quad_x
    );
  }

  prev_report[dev_addr-1] = astro_report;
}

void process_wing_man(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
  // previous report used to compare for changes
  static wing_man_report_t prev_report[5] = { 0 };

  wing_man_report_t wingman_report;
  memcpy(&wingman_report, report, sizeof(wingman_report));

  if ( wingman_diff_report(&prev_report[dev_addr-1], &wingman_report) )
  {
    printf("(x, y, z) = (%u, %u, %u)\r\n", wingman_report.analog_x, wingman_report.analog_y, wingman_report.analog_z);
    printf("DPad = %d ", wingman_report.dpad);
    if (wingman_report.a) printf("A ");
    if (wingman_report.b) printf("B ");
    if (wingman_report.c) printf("C ");
    if (wingman_report.x) printf("X ");
    if (wingman_report.y) printf("Y ");
    if (wingman_report.z) printf("Z ");
    if (wingman_report.l) printf("L ");
    if (wingman_report.r) printf("R ");
    if (wingman_report.mode) printf("Mode ");
    if (wingman_report.s) printf("S ");
    printf("\r\n");

    bool dpad_up    = (wingman_report.dpad == 0 || wingman_report.dpad == 1 || wingman_report.dpad == 7);
    bool dpad_right = ((wingman_report.dpad >= 1 && wingman_report.dpad <= 3));
    bool dpad_down  = ((wingman_report.dpad >= 3 && wingman_report.dpad <= 5));
    bool dpad_left  = ((wingman_report.dpad >= 5 && wingman_report.dpad <= 7));

    buttons = (((wingman_report.b)    ? 0x8000 : 0x00) | //C-DOWN
               ((wingman_report.a)    ? 0x4000 : 0x00) | //A
               ((wingman_report.s)    ? 0x2000 : 0x00) | //START
               ((wingman_report.mode) ? 0x1000 : 0x00) | //NUON
               ((dpad_down)           ? 0x0800 : 0x00) | //D-DOWN
               ((dpad_left)           ? 0x0400 : 0x00) | //D-LEFT
               ((dpad_up)             ? 0x0200 : 0x00) | //D-UP
               ((dpad_right)          ? 0x0100 : 0x00) | //D-RIGHT
               ((1)                   ? 0x0080 : 0x00) |
               ((0)                   ? 0x0040 : 0x00) |
               ((wingman_report.l)    ? 0x0020 : 0x00) | //L
               ((wingman_report.r)    ? 0x0010 : 0x00) | //R
               ((wingman_report.x)    ? 0x0008 : 0x00) | //B
               ((wingman_report.y)    ? 0x0004 : 0x00) | //C-LEFT
               ((wingman_report.z)    ? 0x0002 : 0x00) | //C-UP
               ((wingman_report.c)    ? 0x0001 : 0x00)); //C-RIGHT

    uint8_t analog_1x = wingman_report.analog_x+1;
    uint8_t analog_1y = wingman_report.analog_y+1;
    if (analog_1x == 0) analog_1x = 255;
    if (analog_1y == 0) analog_1y = 255;

    // add to accumulator and post to the state machine
    // if a scan from the host machine is ongoing, wait
    post_globals(
      dev_addr,
      instance,
      buttons,
      true,                   // analog_1 enabled
      analog_1x,              // analog_1x
      analog_1y,              // analog_1y
      false,                  // analog_2 enabled
      0,                      // analog_2x
      0,                      // analog_2y
      true,                   // quad enabled
      wingman_report.analog_z // quad_x
    );
  }

  prev_report[dev_addr-1] = wingman_report;
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
      if      ( is_sony_ds4(dev_addr) ) process_sony_ds4(dev_addr, instance, report, len);
      else if ( is_sony_ds5(dev_addr) ) process_sony_ds5(dev_addr, instance, report, len);
      else if ( is_8bit_pce(dev_addr) ) process_8bit_pce(dev_addr, instance, report, len);
      else if ( is_8bit_psc(dev_addr) ) process_8bit_psc(dev_addr, instance, report, len);
      else if ( is_sega_mini(dev_addr) ) process_sega_mini(dev_addr, instance, report, len);
      else if ( is_astro_city(dev_addr) ) process_astro_city(dev_addr, instance, report, len);
      else if ( is_wing_man(dev_addr) ) process_wing_man(dev_addr, instance, report, len);
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

  bool btns_cd, btns_a, btns_nuon, btns_start, btns_l, btns_r, btns_b,
  btns_cl, btns_cu, btns_cr, dpad_left, dpad_down, dpad_right, dpad_up;

  //------------- example code ignore control (non-printable) key affects -------------//
  for(uint8_t i=0; i<6; i++)
  {
    if ( report->keycode[i] )
    {
      // btns_enter = (report->keycode[i] == 40); // Enter
      // btns_esc = (report->keycode[i] == 41); // ESC
      dpad_up = (report->keycode[i] == 26 || report->keycode[i] == 82); // W or Arrow
      dpad_left = (report->keycode[i] == 4  || report->keycode[i] == 80); // A or Arrow
      dpad_down = (report->keycode[i] == 22 || report->keycode[i] == 81); // S or Arrow
      dpad_right = (report->keycode[i] == 7  || report->keycode[i] == 79); // D or Arrow

      btns_cl = (report->keycode[i] == 89); // 1
      btns_cd = (report->keycode[i] == 90); // 2
      btns_cl = (report->keycode[i] == 91); // 3
      btns_cu = (report->keycode[i] == 92); // 4

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

  buttons = (((btns_cd)    ? 0x8000 : 0x00) | //C-DOWN  (2)
             ((btns_a)     ? 0x4000 : 0x00) | //A       (f)
             ((btns_nuon)  ? 0x2000 : 0x00) | //START   (n)
             ((btns_start) ? 0x1000 : 0x00) | //NUON    (m)
             ((dpad_down)  ? 0x0800 : 0x00) | //D-DOWN
             ((dpad_left)  ? 0x0400 : 0x00) | //D-LEFT
             ((dpad_up)    ? 0x0200 : 0x00) | //D-UP
             ((dpad_right) ? 0x0100 : 0x00) | //D-RIGHT
             ((1)          ? 0x0080 : 0x00) |
             ((0)          ? 0x0040 : 0x00) |
             ((btns_l)     ? 0x0020 : 0x00) | //L       (q)
             ((btns_r)     ? 0x0010 : 0x00) | //R       (e)
             ((btns_b)     ? 0x0008 : 0x00) | //B       (b)
             ((btns_cl)    ? 0x0004 : 0x00) | //C-LEFT  (1)
             ((btns_cu)    ? 0x0002 : 0x00) | //C-UP    (4)
             ((btns_cr)    ? 0x0001 : 0x00)); //C-RIGHT (3)

  post_globals(
    dev_addr,
    instance,
    buttons,
    false,  // analog_1 enabled
    0,      // analog_1x
    0,      // analog_1y
    false,  // analog_2 enabled
    0,      // analog_2x
    0,      // analog_2y
    false,  // quad enabled
    0       // quad_x
  );

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
    TU_LOG1(" %c%c%c%c%c ",
       report->buttons & MOUSE_BUTTON_BACKWARD  ? 'R' : '-',
       report->buttons & MOUSE_BUTTON_FORWARD   ? 'S' : '-',
       report->buttons & MOUSE_BUTTON_LEFT      ? '2' : '-',
       report->buttons & MOUSE_BUTTON_MIDDLE    ? 'M' : '-',
       report->buttons & MOUSE_BUTTON_RIGHT     ? '1' : '-');

    // if (buttons_swappable && (report->buttons & MOUSE_BUTTON_MIDDLE) &&
    //     (previous_middle_button == false))
    //    buttons_swapped = (buttons_swapped ? false : true);

    // previous_middle_button = (report->buttons & MOUSE_BUTTON_MIDDLE);
  }

  // if (buttons_swapped)
  // {
  //    buttons = (((0xFF00)) | // no six button controller byte
  //               ((report->buttons & MOUSE_BUTTON_BACKWARD) ? 0x00 : 0x80) |
  //               ((report->buttons & MOUSE_BUTTON_FORWARD ) ? 0x00 : 0x40) |
  //               ((report->buttons & MOUSE_BUTTON_RIGHT)    ? 0x00 : 0x20) |
  //               ((report->buttons & MOUSE_BUTTON_LEFT)     ? 0x00 : 0x10));
  // }
  // else
  // {
  //    buttons = (((0xFF00)) |
  //               ((report->buttons & MOUSE_BUTTON_BACKWARD) ? 0x00 : 0x80) |
  //               ((report->buttons & MOUSE_BUTTON_FORWARD ) ? 0x00 : 0x40) |
  //               ((report->buttons & MOUSE_BUTTON_LEFT)     ? 0x00 : 0x20) |
  //               ((report->buttons & MOUSE_BUTTON_RIGHT)    ? 0x00 : 0x10));
  // }


  buttons = (((0)                   ? 0x8000 : 0x00) | //C-DOWN
             ((report->buttons & MOUSE_BUTTON_LEFT)     ? 0x4000 : 0x00) | //A
             ((report->buttons & MOUSE_BUTTON_MIDDLE)   ? 0x2000 : 0x00) | //START
             ((report->buttons & MOUSE_BUTTON_BACKWARD) ? 0x1000 : 0x00) | //NUON
             ((0)                   ? 0x0800 : 0x00) | //D-DOWN
             ((0)                   ? 0x0400 : 0x00) | //D-LEFT
             ((0)                   ? 0x0200 : 0x00) | //D-UP
             ((0)                   ? 0x0100 : 0x00) | //D-RIGHT
             ((1)                   ? 0x0080 : 0x00) |
             ((0)                   ? 0x0040 : 0x00) |
             ((0)                   ? 0x0020 : 0x00) | //L
             ((0)                   ? 0x0010 : 0x00) | //R
             ((report->buttons & MOUSE_BUTTON_RIGHT) ? 0x0008 : 0x00) | //B
             ((0)                   ? 0x0004 : 0x00) | //C-LEFT
             ((0)                   ? 0x0002 : 0x00) | //C-UP
             ((0)                   ? 0x0001 : 0x00)); //C-RIGHT

  local_x = (0 - report->x);
  local_y = (0 - report->y);

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

  int16_t delta = report->x;

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

  // add to accumulator and post to the state machine
  // if a scan from the host machine is ongoing, wait
  post_globals(
    dev_addr,
    1, // instance,
    buttons,
    false,  // analog_1 enabled
    0,      // analog_1x
    0,      // analog_1y
    false,  // analog_2 enabled
    0,      // analog_2x
    0,      // analog_2y
    true,   // quad enabled
    spinner // quad_x
  );

  //------------- cursor movement -------------//
  // cursor_movement(report->x, report->y, report->wheel, spinner);
}

//--------------------------------------------------------------------+
// Generic Report
//--------------------------------------------------------------------+
static void process_generic_report(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
  (void) dev_addr;

  uint8_t const rpt_count = hid_info[instance].report_count;
  tuh_hid_report_info_t* rpt_info_arr = hid_info[instance].report_info;
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

      default: break;
    }
  }
}
