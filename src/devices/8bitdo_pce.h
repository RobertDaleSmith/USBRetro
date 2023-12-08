// 8bitdo_pce.h
#ifndef BITDO_PCE_HEADER_H
#define BITDO_PCE_HEADER_H

#include "device_interface.h"
#include "device_utils.h"
#include "tusb.h"

extern DeviceInterface bitdo_pce_interface;

// 8BitDo USB Adapter for PC Engine 2.4g controllers
typedef struct TU_ATTR_PACKED
{
  struct {
    uint8_t three : 1;
    uint8_t two   : 1;
    uint8_t one   : 1;
    uint8_t four  : 1;
  };

  struct {
    uint8_t sel  : 1;
    uint8_t run  : 1;
  };

  struct {
    uint8_t dpad : 4; // (hat format, 0x08 is released, 0=N, 1=NE, 2=E, 3=SE, 4=S, 5=SW, 6=W, 7=NW)
  };

  uint8_t x1, y1, x2, y2;

} bitdo_pce_report_t;

uint32_t buttons;

#endif
