// sony_psc.h
#ifndef SONY_PSC_HEADER_H
#define SONY_PSC_HEADER_H

#include "../../core/device_interface.h"
#include "../../core/device_utils.h"
#include "tusb.h"

extern DeviceInterface sony_psc_interface;

// Sony PS Classic controller (or 8BitDo USB Adapter for PS classic)
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

} sony_psc_report_t;

#endif
