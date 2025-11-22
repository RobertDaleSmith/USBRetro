// sony_ds4.h
#ifndef SONY_DS4_HEADER_H
#define SONY_DS4_HEADER_H

#include "../../../hid_device.h"
#include "../../../hid_utils.h"
#include "tusb.h"

extern DeviceInterface sony_ds4_interface;

// Sony DS4 report layout detail https://www.psdevwiki.com/ps4/DS4-USB
typedef struct TU_ATTR_PACKED {
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

extern int16_t spinner;

#endif
