// triple_adapter_v2.h
#ifndef TRIPLE_ADAPTER_V2_HEADER_H
#define TRIPLE_ADAPTER_V2_HEADER_H

#include "../../../core/device_interface.h"
#include "../../../core/device_utils.h"
#include "tusb.h"

extern DeviceInterface triple_adapter_v2_interface;

uint16_t tplctr_serial_v2[] = {0x0320, 'N', 'E', 'S', '-', 'N', 'T', 'T', '-', 'G', 'E', 'N', 'E', 'S', 'I', 'S'};
uint16_t tplctr_serial_v2_1[] = {0x031a, 'S', '-', 'N', 'E', 'S', '-', 'G', 'E', 'N', '-', 'V', '2'};

// TripleController v2 (Arduino based HID)
typedef struct TU_ATTR_PACKED
{
  struct {
    uint8_t b : 1;
    uint8_t a : 1;
    uint8_t y : 1;
    uint8_t x : 1;
    uint8_t l : 1;
    uint8_t r : 1;
    uint8_t select : 1;
    uint8_t start : 1;
  };

  struct {
    uint8_t ntt_0 : 1;
    uint8_t ntt_1 : 1;
    uint8_t ntt_2 : 1;
    uint8_t ntt_3 : 1;
    uint8_t ntt_4 : 1;
    uint8_t ntt_5 : 1;
    uint8_t ntt_6 : 1;
    uint8_t ntt_7 : 1;
  };

  struct {
    uint8_t ntt_8 : 1;
    uint8_t ntt_9 : 1;
    uint8_t ntt_star : 1;
    uint8_t ntt_hash : 1;
    uint8_t ntt_dot : 1;
    uint8_t ntt_clear : 1;
    uint8_t ntt_null : 1;
    uint8_t ntt_end : 1;
  };

  uint8_t axis_x;
  uint8_t axis_y;

} triple_adapter_v2_report_t;

#endif
