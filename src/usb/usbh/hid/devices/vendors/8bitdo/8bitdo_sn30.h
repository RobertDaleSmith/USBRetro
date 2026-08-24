// 8bitdo_sn30.h
#ifndef BITDO_SN30_HEADER_H
#define BITDO_SN30_HEADER_H

#include "../../../hid_device.h"
#include "../../../hid_utils.h"
#include "tusb.h"

extern DeviceInterface bitdo_sn30_interface;

// 8BitDo SN30 Pro / SF30 Pro (and Pro+ / Pro 2) in USB D-Input mode (2dc8:6001).
//
// 9-byte report, reportId 0. The pad declares 15 HID buttons (usages 1-15) but
// only 13 are physical — bits 2 and 5 of byte 0 are dead. The generic parser
// assumes contiguous buttons and (with its 12-button cap) mismaps the dead bits
// onto face slots, which is why one face button vanished and the rest shifted.
// Layout captured via joypad-web training (2dc8:6001):
//   byte0: b0 B2, b1 B1, b2 (dead), b3 B4, b4 B3, b5 (dead), b6 L1, b7 R1
//   byte1: b0 L2, b1 R2, b2 Select, b3 Start, b4 Home, b5 L3, b6 R3, b7 (unused)
//   byte2: hat/d-pad in low nibble (0x0F = released)
//   byte3..6: LX, LY, RX, RY (0-255, 128 center, HID orientation)
typedef struct TU_ATTR_PACKED
{
  struct {
    uint8_t b : 1;     // byte0 bit0 -> B2
    uint8_t a : 1;     // byte0 bit1 -> B1
    uint8_t pad1 : 1;  // byte0 bit2 (dead)
    uint8_t y : 1;     // byte0 bit3 -> B4
    uint8_t x : 1;     // byte0 bit4 -> B3
    uint8_t pad2 : 1;  // byte0 bit5 (dead)
    uint8_t l1 : 1;    // byte0 bit6 -> L1
    uint8_t r1 : 1;    // byte0 bit7 -> R1
  };

  struct {
    uint8_t l2 : 1;     // byte1 bit0 -> L2
    uint8_t r2 : 1;     // byte1 bit1 -> R2
    uint8_t select : 1; // byte1 bit2 -> S1
    uint8_t start : 1;  // byte1 bit3 -> S2
    uint8_t home : 1;   // byte1 bit4 -> A1
    uint8_t l3 : 1;     // byte1 bit5 -> L3
    uint8_t r3 : 1;     // byte1 bit6 -> R3
    uint8_t pad3 : 1;   // byte1 bit7 (unused)
  };

  uint8_t dpad;         // byte2: hat in low nibble (0x0F = released)
  uint8_t x1, y1, x2, y2; // byte3..6: LX, LY, RX, RY

} bitdo_sn30_report_t;

#endif
