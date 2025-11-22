// sega_astrocity.h
#ifndef SEGA_ASTROCITY_HEADER_H
#define SEGA_ASTROCITY_HEADER_H

#include "../../../hid_device.h"
#include "../../../device_utils.h"
#include "tusb.h"

extern DeviceInterface sega_astrocity_interface;

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
    uint8_t f : 1;
    uint8_t l : 1;
    uint8_t r : 1;
    uint8_t credit : 1;
    uint8_t start  : 3;
  };

} sega_astrocity_report_t;

#endif
