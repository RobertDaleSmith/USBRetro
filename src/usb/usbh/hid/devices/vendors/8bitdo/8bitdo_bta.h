// 8bitdo_bta.h
#ifndef BITDO_BTA_HEADER_H
#define BITDO_BTA_HEADER_H

#include "../../../hid_device.h"
#include "../../../hid_utils.h"
#include "tusb.h"

extern DeviceInterface bitdo_bta_interface;

// 8BitDo Wireless Adapter
typedef struct TU_ATTR_PACKED
{
  uint8_t reportId; // 0x01 for HID data

  struct {
    uint8_t a : 1;
    uint8_t b : 1;
    uint8_t home : 1;
    uint8_t x : 1;
    uint8_t y : 1;
    uint8_t padding1 : 1;
    uint8_t l : 1;
    uint8_t r : 1;
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
    uint8_t dpad : 4;
    uint8_t cap : 1;
  };

  uint8_t x1, y1, x2, y2;

  uint8_t r2_trigger;
  uint8_t l2_trigger;

} bitdo_bta_report_t;

#endif
