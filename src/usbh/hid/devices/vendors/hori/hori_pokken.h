// hori_pokken.h
#ifndef HORI_POKKEN_HEADER_H
#define HORI_POKKEN_HEADER_H

#include "../../../hid_device.h"
#include "../../../device_utils.h"
#include "tusb.h"

extern DeviceInterface hori_pokken_interface;

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

  uint8_t x_axis, y_axis, z_axis, rz_axis;

} hori_pokken_report_t;

#endif
