// switch_spi.h - emulated Switch Pro Controller SPI flash reads
// SPDX-License-Identifier: Apache-2.0
//
// Clean-room from dekuNukem/Nintendo_Switch_Reverse_Engineering spi_flash_notes.md.
// The Switch reads factory calibration + pairing/host-capability bytes from the
// controller's "SPI flash" via subcommand 0x10; we synthesize plausible factory
// values so the console accepts us as a Pro Controller. Cross-checked against HOJA
// for byte-correctness; no code copied.

#ifndef SWITCH_SPI_H
#define SWITCH_SPI_H

#include <stdint.h>

// Return the emulated flash byte at (region<<8 | addr), where `region` is the high
// byte of the 32-bit SPI address (0x60 = 0x60xx factory block, 0x80 = user cal, etc).
uint8_t switch_spi_byte(uint8_t region, uint8_t addr);

// Fill `len` bytes starting at (region<<8 | addr) into out.
void switch_spi_read(uint8_t region, uint8_t addr, uint8_t len, uint8_t* out);

#endif // SWITCH_SPI_H
