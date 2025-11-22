// hori_horipad.h
#ifndef HORIPAD_HEADER_H
#define HORIPAD_HEADER_H

#include "../../../core/device_interface.h"
#include "../../../core/device_utils.h"
#include "tusb.h"

extern DeviceInterface hori_horipad_interface;

// DragonRise Generic Gamepad (SNES/NES/GC/etc)
typedef struct TU_ATTR_PACKED
{
  uint8_t id, axis1_y, axis1_x, axis0_x, axis0_y;

  struct {
    uint8_t padding : 4;
    uint8_t x : 1;
    uint8_t a : 1;
    uint8_t b : 1;
    uint8_t y : 1;
  };

  struct {
    uint8_t c : 1;
    uint8_t z : 1;
    uint8_t l : 1;
    uint8_t r : 1;
    uint8_t select : 1;
    uint8_t start : 1;
  };

} dragonrise_report_t;

#endif
