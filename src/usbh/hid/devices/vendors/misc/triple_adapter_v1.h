// triple_adapter_v2.h
#ifndef TRIPLE_ADAPTER_V2_HEADER_H
#define TRIPLE_ADAPTER_V2_HEADER_H

#include "../../../hid_device.h"
#include "../../../device_utils.h"
#include "tusb.h"

extern DeviceInterface triple_adapter_v2_interface;

uint16_t tplctr_serial_v1[] = {0x031a, 'N', 'E', 'S', '-', 'S', 'N', 'E', 'S', '-', 'G', 'E', 'N', 'E', 'S', 'I', 'S'};


// TripleController v1 (Arduino based HID)
typedef struct TU_ATTR_PACKED
{
  struct {
    uint8_t b : 1; // A
    uint8_t a : 1; // B
    uint8_t y : 1; // C
    uint8_t x : 1; // X
    uint8_t l : 1; // Y
    uint8_t r : 1; // Z
    uint8_t select : 1; // Mode
    uint8_t start : 1;
  };

  struct {
    uint8_t home : 1;
    uint8_t null : 7;
  };

  uint8_t axis_x;
  uint8_t axis_y;

} triple_adapter_v1_report_t;

#endif
