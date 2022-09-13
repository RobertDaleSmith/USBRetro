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

} sony_ds4_report_t;

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
    uint8_t f : 3;
    uint8_t credit : 1;
    uint8_t start  : 3;
  };

} astro_city_report_t;

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

// check if device is Astro City mini controller
static inline bool is_astro_city(uint8_t dev_addr)
{
  uint16_t vid, pid;
  tuh_vid_pid_get(dev_addr, &vid, &pid);

  return ((vid == 0x0ca3 && pid == 0x0027)); // Astro City mini controller
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

// Each HID instance can has multiple reports
static struct
{
  uint8_t report_count;
  tuh_hid_report_info_t report_info[MAX_REPORT];
}hid_info[CFG_TUH_HID];

static void process_kbd_report(uint8_t dev_addr, hid_keyboard_report_t const *report);
static void process_mouse_report(uint8_t dev_addr, hid_mouse_report_t const * report);
static void process_generic_report(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len);

extern void __not_in_flash_func(post_globals)(uint8_t dev_addr, uint16_t buttons, uint8_t delta_x, uint8_t delta_y);

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
  bool isController = is_sony_ds4(dev_addr) || is_8bit_pce(dev_addr) || is_8bit_psc(dev_addr) || is_astro_city(dev_addr);
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
  result |= rpt1->credit != rpt2->credit;
  result |= rpt1->start != rpt2->start;

  return result;
}

void process_sony_ds4(uint8_t dev_addr, uint8_t const* report, uint16_t len)
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

      buttons = (((ds4_report.r1)       ? 0x00 : 0x8000) |
                 ((ds4_report.l1)       ? 0x00 : 0x4000) |
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
      post_globals(dev_addr, buttons, 0, 0);
    }

    prev_report[dev_addr-1] = ds4_report;
  }
}

void process_8bit_psc(uint8_t dev_addr, uint8_t const* report, uint16_t len)
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
    bool has_6btns = true;

    buttons = (((psc_report.r1)       ? 0x00 : 0x8000) |
               ((psc_report.l1)       ? 0x00 : 0x4000) |
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
    post_globals(dev_addr, buttons, 0, 0);
  }

  prev_report[dev_addr-1] = psc_report;
}

void process_8bit_pce(uint8_t dev_addr, uint8_t const* report, uint16_t len)
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
    post_globals(dev_addr, buttons, 0, 0);
  }

  prev_report[dev_addr-1] = pce_report;
}

void process_astro_city(uint8_t dev_addr, uint8_t const* report, uint16_t len)
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
    if (astro_report.credit) printf("Credit ");
    if (astro_report.start) printf("Start ");
    printf("\r\n");

    bool dpad_up    = (astro_report.y < 127);
    bool dpad_right = (astro_report.x > 127);
    bool dpad_down  = (astro_report.y > 127);
    bool dpad_left  = (astro_report.x < 127);
    bool has_6btns = true;

    buttons = (((astro_report.a) ? 0x00 : 0x8000) |
               ((astro_report.b) ? 0x00 : 0x4000) |
               ((astro_report.c) ? 0x00 : 0x2000) |
               ((astro_report.d) ? 0x00 : 0x1000) |
               ((has_6btns)      ? 0x00 : 0xFF00) |
               ((dpad_left)      ? 0x00 : 0x08) |
               ((dpad_down)      ? 0x00 : 0x04) |
               ((dpad_right)     ? 0x00 : 0x02) |
               ((dpad_up)        ? 0x00 : 0x01) |
               ((astro_report.start)  ? 0x00 : 0x80) |
               ((astro_report.credit) ? 0x00 : 0x40) |
               ((astro_report.e) ? 0x00 : 0x20) |
               ((astro_report.f) ? 0x00 : 0x10));

    // add to accumulator and post to the state machine
    // if a scan from the host machine is ongoing, wait
    post_globals(dev_addr, buttons, 0, 0);
  }

  prev_report[dev_addr-1] = astro_report;
}

// Invoked when received report from device via interrupt endpoint
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
  uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

  switch (itf_protocol)
  {
    case HID_ITF_PROTOCOL_KEYBOARD:
      TU_LOG2("HID receive boot keyboard report\r\n");
      process_kbd_report(dev_addr, (hid_keyboard_report_t const*) report );
    break;

    case HID_ITF_PROTOCOL_MOUSE:
      TU_LOG2("HID receive boot mouse report\r\n");
      process_mouse_report(dev_addr, (hid_mouse_report_t const*) report );
    break;

    default:
      if      ( is_sony_ds4(dev_addr) ) process_sony_ds4(dev_addr, report, len);
      else if ( is_8bit_pce(dev_addr) ) process_8bit_pce(dev_addr, report, len);
      else if ( is_8bit_psc(dev_addr) ) process_8bit_psc(dev_addr, report, len);
      else if ( is_astro_city(dev_addr) ) process_astro_city(dev_addr, report, len);
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

static void process_kbd_report(uint8_t dev_addr, hid_keyboard_report_t const *report)
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
  post_globals(dev_addr, buttons, 0, 0);

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

static void process_mouse_report(uint8_t dev_addr, hid_mouse_report_t const * report)
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
  post_globals(dev_addr, buttons, local_x, local_y);

  //------------- cursor movement -------------//
  cursor_movement(report->x, report->y, report->wheel);
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
        process_kbd_report(dev_addr, (hid_keyboard_report_t const*) report );
      break;

      case HID_USAGE_DESKTOP_MOUSE:
        TU_LOG1("HID receive mouse report\r\n");
        // Assume mouse follow boot report layout
        process_mouse_report(dev_addr, (hid_mouse_report_t const*) report );
      break;

      default: break;
    }
  }
}
