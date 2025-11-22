// sony_ds3.h
#ifndef SONY_DS3_HEADER_H
#define SONY_DS3_HEADER_H

#include "../../../hid_device.h"
#include "../../../hid_utils.h"
#include "tusb.h"

extern DeviceInterface sony_ds3_interface;

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

typedef struct {
  uint8_t time_enabled; // the total time the led is active (0xff means forever)
  uint8_t duty_length; // how long a cycle is in deciseconds (0 means "really fast")
  uint8_t enabled;
  uint8_t duty_off; // % of duty_length the led is off (0xff means 100%)
  uint8_t duty_on; // % of duty_length the led is on (0xff mean 100%)
} sony_ds3_led_t;

typedef struct {
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

#endif
