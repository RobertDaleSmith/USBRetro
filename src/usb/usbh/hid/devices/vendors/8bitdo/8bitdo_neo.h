// 8bitdo_neo.h
#ifndef BITDO_NEO_HEADER_H
#define BITDO_NEO_HEADER_H

#include "../../../hid_device.h"
#include "../../../hid_utils.h"
#include "tusb.h"

extern DeviceInterface bitdo_neo_interface;

// 8BitDo NeoGeo 2.4g gamepad
typedef struct TU_ATTR_PACKED
{
  uint8_t unknown;

} bitdo_neo_report_t;

#endif
