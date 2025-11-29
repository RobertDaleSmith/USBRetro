// cdc.h - USB CDC (Virtual Serial Port) interface
// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Robert Dale Smith
//
// Dual CDC implementation:
// - CDC 0: Data channel (commands, config, responses)
// - CDC 1: Debug channel (printf output)

#ifndef CDC_H
#define CDC_H

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

// CDC port indices
#define CDC_PORT_DATA   0
#define CDC_PORT_DEBUG  1

// Initialize CDC subsystem
void cdc_init(void);

// Process CDC tasks (call from main loop)
void cdc_task(void);

// ============================================================================
// DATA PORT (CDC 0) - Commands and responses
// ============================================================================

// Check if data port is connected
bool cdc_data_connected(void);

// Check bytes available to read
uint32_t cdc_data_available(void);

// Read from data port (returns bytes read)
uint32_t cdc_data_read(uint8_t* buffer, uint32_t bufsize);

// Read single byte (-1 if none available)
int32_t cdc_data_read_byte(void);

// Write to data port (returns bytes written)
uint32_t cdc_data_write(const uint8_t* buffer, uint32_t bufsize);

// Write string to data port
uint32_t cdc_data_write_str(const char* str);

// Flush data port
void cdc_data_flush(void);

// ============================================================================
// DEBUG PORT (CDC 1) - Debug/logging output
// ============================================================================

// Check if debug port is connected
bool cdc_debug_connected(void);

// Printf to debug port
int cdc_debug_printf(const char* format, ...);

// Write raw data to debug port
uint32_t cdc_debug_write(const uint8_t* buffer, uint32_t bufsize);

// Flush debug port
void cdc_debug_flush(void);

// Enable/disable debug output (runtime toggle)
void cdc_debug_set_enabled(bool enabled);
bool cdc_debug_is_enabled(void);

#endif // CDC_H
