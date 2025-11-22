// gamecube_adapter.h
#ifndef GAMECUBE_ADAPTER_HEADER_H
#define GAMECUBE_ADAPTER_HEADER_H

#include "../../../hid_device.h"
#include "../../../device_utils.h"
#include "tusb.h"

extern DeviceInterface gamecube_adapter_interface;

// GameCube Adapter for WiiU/Switch Individual Port
typedef struct TU_ATTR_PACKED
{
  struct {
    uint8_t type : 4;
    uint8_t connected : 4;
  };

  struct {
    uint8_t a : 1;
    uint8_t b : 1;
    uint8_t x : 1;
    uint8_t y : 1;
    uint8_t left : 1;
    uint8_t right : 1;
    uint8_t down : 1;
    uint8_t up : 1;
  };

  struct {
    uint8_t start : 1;
    uint8_t z : 1;
    uint8_t r : 1;
    uint8_t l : 1;
  };

  uint8_t x1, y1, x2, y2, zl, zr;

} gamecube_adapter_port_report_t;

// GameCube Adapter for WiiU/Switch
typedef struct TU_ATTR_PACKED
{
  uint8_t report_id;
  gamecube_adapter_port_report_t port[4];

} gamecube_adapter_report_t;

#endif
