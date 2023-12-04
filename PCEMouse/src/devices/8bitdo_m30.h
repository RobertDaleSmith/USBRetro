// 8bitdo_m30.h
#ifndef BITDO_M30_HEADER_H
#define BITDO_M30_HEADER_H

#include "device_interface.h"
#include "device_utils.h"
#include "tusb.h"

extern DeviceInterface bitdo_m30_interface;

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
    uint8_t l : 1; // z
    uint8_t r : 1; // c
  };

  struct {
    uint8_t l2 : 1;
    uint8_t r2 : 1;
    uint8_t select : 1;
    uint8_t start : 1;
    uint8_t padding2 : 1;
    uint8_t l3 : 1;
    uint8_t r3 : 1;
  };

  struct {
    uint8_t dpad   : 4;
    uint8_t cap : 1;
  };

  uint8_t x1, y1, x2, y2;

} bitdo_m30_report_t;

uint32_t buttons;

#endif
