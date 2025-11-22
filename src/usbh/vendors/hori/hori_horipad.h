// hori_horipad.h
#ifndef HORIPAD_HEADER_H
#define HORIPAD_HEADER_H

#include "../../core/device_interface.h"
#include "../../core/device_utils.h"
#include "tusb.h"

extern DeviceInterface hori_horipad_interface;

// HORI HORIPAD (or Sega Genesis mini compatible controllers)
typedef struct TU_ATTR_PACKED
{
  struct {
    uint8_t y : 1;
    uint8_t b : 1;
    uint8_t a : 1;
    uint8_t x : 1;
    uint8_t l1 : 1;
    uint8_t r1 : 1;
    uint8_t l2 : 1; // sega-z
    uint8_t r2 : 1; // sega-c
  };

  struct {
    uint8_t s1 : 1; // select / sega-mode
    uint8_t s2 : 1; // start
    uint8_t l3 : 1;
    uint8_t r3 : 1;
    uint8_t a1 : 1; // home
    uint8_t a2 : 1; // capture
    uint8_t padding1 : 2;
  };

  struct {
    uint8_t dpad : 4;
    uint8_t padding2 : 4;
  };

  uint8_t axis_x;
  uint8_t axis_y;
  uint8_t axis_z;
  uint8_t axis_rz;

} hori_horipad_report_t;

#endif
