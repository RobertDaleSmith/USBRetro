// switch_spi.c - emulated Switch Pro Controller SPI flash (see switch_spi.h)
// SPDX-License-Identifier: Apache-2.0
//
// Address map and constants per dekuNukem spi_flash_notes.md. Only the regions the
// Switch actually reads during setup are synthesized; everything else reads 0xFF
// (matching an erased/unset flash so the console falls back to defaults).

#include "switch_spi.h"

// Neutral stick calibration triplet: center = 2048 (0x800), ±2048 delta = full
// 12-bit range. encode(x=2048,y=2048): b0=x&0xFF=0x00, b1=((x>>8)&0xF)|((y&0xF)<<4)
// =0x08, b2=(y>>4)&0xFF=0x80. Factory cal is 3 triplets (max/center/min deltas); all
// identical here => a controller centered at 2048 with full travel each way.
static const uint8_t STICK_CAL[9] = { 0x00,0x08,0x80, 0x00,0x08,0x80, 0x00,0x08,0x80 };

// Factory 6-axis + stick "config" block at 0x6020..0x6037 (documented defaults).
static const uint8_t SIXAXIS_CFG[0x18] = {
    35,0,185,255,26,1, 0,64,0,64,0,64, 1,0,1,0,1,0, 0x3B,0x34,0x3B,0x34,0x3B,0x34
};
// Accelerometer origin at 0x6080..0x6085.
static const uint8_t ACCEL_ORIGIN[6] = { 80,253,0,0,198,15 };
// Stick device parameters at 0x6086..0x6097 (mirrored at 0x6098..0x60A9).
static const uint8_t STICK_PARAMS[0x12] = {
    15,48,97,174,144,217, 212,20,84,65,21,84, 199,121,156,51,54,99
};
// Controller body / button / grip colors (RGB). Dark grey, classic Pro look.
static const uint8_t COLOR_BODY[3]    = { 0x32,0x32,0x32 };
static const uint8_t COLOR_BUTTONS[3] = { 0xE6,0xE6,0xE6 };
static const uint8_t COLOR_LGRIP[3]   = { 0x46,0x46,0x46 };
static const uint8_t COLOR_RGRIP[3]   = { 0x46,0x46,0x46 };

uint8_t switch_spi_byte(uint8_t region, uint8_t addr)
{
    switch (region) {
    // --- pairing block (0x2000..0x4000): the bytes that mark us "paired to a Switch"
    case 0x20 ... 0x40:
        switch (addr) {
        case 0x00: case 0x26: return 0x95;  // pairing-data-present magic
        case 0x01: case 0x27: return 0x22;  // pairing payload size
        case 0x24: case 0x4A: return 0x68;  // host capability: 0x68 = Switch (0x08 = PC)
        default:              return 0x00;  // host addr / LTK areas: zeroed placeholder
        }

    // --- factory config / calibration / colors (0x6000..0x60FF) -------------------
    case 0x60:
        if (addr <= 0x0F)              return 0xFF;                 // serial: disabled
        if (addr == 0x12)             return 0x03;                 // ProCon type hi
        if (addr == 0x13)             return 0x02;                 // ProCon type lo
        if (addr == 0x1B)             return 0x01;                 // colors present
        if (addr >= 0x20 && addr <= 0x37) return SIXAXIS_CFG[addr - 0x20];
        if (addr >= 0x3D && addr <= 0x45) return STICK_CAL[addr - 0x3D]; // left stick cal
        if (addr >= 0x46 && addr <= 0x4E) return STICK_CAL[addr - 0x46]; // right stick cal
        if (addr == 0x4F)             return 0xFF;
        if (addr >= 0x50 && addr <= 0x52) return COLOR_BODY[addr - 0x50];
        if (addr >= 0x53 && addr <= 0x55) return COLOR_BUTTONS[addr - 0x53];
        if (addr >= 0x56 && addr <= 0x58) return COLOR_LGRIP[addr - 0x56];
        if (addr >= 0x59 && addr <= 0x5B) return COLOR_RGRIP[addr - 0x59];
        if (addr >= 0x80 && addr <= 0x85) return ACCEL_ORIGIN[addr - 0x80];
        if (addr >= 0x86 && addr <= 0x97) return STICK_PARAMS[addr - 0x86];
        if (addr >= 0x98 && addr <= 0xA9) return STICK_PARAMS[addr - 0x98]; // mirror
        return 0x00;

    // --- user calibration (0x8000): unset => 0xFF, console uses factory ----------
    case 0x80:
        return 0xFF;

    default:
        return 0xFF;  // patch ROM / failsafe / everything else
    }
}

void switch_spi_read(uint8_t region, uint8_t addr, uint8_t len, uint8_t* out)
{
    for (uint8_t i = 0; i < len; i++)
        out[i] = switch_spi_byte(region, (uint8_t)(addr + i));
}
