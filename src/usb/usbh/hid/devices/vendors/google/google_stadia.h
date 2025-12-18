// google_stadia.h - Google Stadia Controller driver
#ifndef GOOGLE_STADIA_H
#define GOOGLE_STADIA_H

#include "../../../hid_device.h"
#include "../../../hid_utils.h"
#include "tusb.h"

// Google Stadia Controller
// VID: 0x18D1 (Google)
// PID: 0x9400

// Input Report 0x03 (10 bytes)
typedef struct TU_ATTR_PACKED
{
    uint8_t dpad;           // 0=N, 1=NE, 2=E, 3=SE, 4=S, 5=SW, 6=W, 7=NW, 8=neutral
    uint8_t buttons1;       // A3=0x01, A2=0x02, L2=0x04, R2=0x08, A1=0x10, S2=0x20, S1=0x40, R3=0x80
    uint8_t buttons2;       // L3=0x01, R1=0x02, L1=0x04, B4=0x08, B3=0x10, B2=0x20, B1=0x40
    uint8_t left_x;         // 0-255, center 128
    uint8_t left_y;         // 0-255, center 128
    uint8_t right_x;        // 0-255, center 128
    uint8_t right_y;        // 0-255, center 128
    uint8_t l2_trigger;     // 0-255
    uint8_t r2_trigger;     // 0-255
    uint8_t consumer;       // Volume, play/pause (unused)
} stadia_report_t;

// Output Report 0x05 (Rumble, 4 bytes)
typedef struct TU_ATTR_PACKED
{
    uint16_t left_motor;    // 16-bit LE, 0-65535
    uint16_t right_motor;   // 16-bit LE, 0-65535
} stadia_output_report_t;

// Button masks for buttons1
#define STADIA_BTN1_A3      0x01  // Assistant/Capture button
#define STADIA_BTN1_A2      0x02  // Google Assistant button
#define STADIA_BTN1_L2      0x04
#define STADIA_BTN1_R2      0x08
#define STADIA_BTN1_A1      0x10  // Stadia button
#define STADIA_BTN1_S2      0x20  // Menu/Start
#define STADIA_BTN1_S1      0x40  // Options/Select
#define STADIA_BTN1_R3      0x80

// Button masks for buttons2
#define STADIA_BTN2_L3      0x01
#define STADIA_BTN2_R1      0x02
#define STADIA_BTN2_L1      0x04
#define STADIA_BTN2_B4      0x08  // Y
#define STADIA_BTN2_B3      0x10  // X
#define STADIA_BTN2_B2      0x20  // B
#define STADIA_BTN2_B1      0x40  // A

extern DeviceInterface google_stadia_interface;

#endif // GOOGLE_STADIA_H
