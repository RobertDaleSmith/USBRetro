// cdc.c - USB CDC (Virtual Serial Port) implementation
// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Robert Dale Smith

#include "cdc.h"
#include "tusb.h"
#include "pico/stdio.h"
#include "pico/stdio/driver.h"
#include <stdio.h>
#include <string.h>

#if CFG_TUD_CDC > 0

// Debug output enabled flag (runtime toggle)
static bool debug_enabled = true;

// ============================================================================
// STDIO DRIVER (routes printf to CDC debug port)
// ============================================================================

#if CFG_TUD_CDC >= 2

static void cdc_stdio_out_chars(const char *buf, int len)
{
    if (!debug_enabled) return;
    if (!tud_cdc_n_connected(CDC_PORT_DEBUG)) return;

    // Write in chunks to avoid blocking
    int remaining = len;
    while (remaining > 0) {
        int available = (int)tud_cdc_n_write_available(CDC_PORT_DEBUG);
        if (available == 0) {
            tud_cdc_n_write_flush(CDC_PORT_DEBUG);
            break;  // Don't block waiting for space
        }
        int to_write = remaining < available ? remaining : available;
        int written = (int)tud_cdc_n_write(CDC_PORT_DEBUG, buf, to_write);
        buf += written;
        remaining -= written;
    }
    tud_cdc_n_write_flush(CDC_PORT_DEBUG);
}

static void cdc_stdio_out_flush(void)
{
    if (tud_cdc_n_connected(CDC_PORT_DEBUG)) {
        tud_cdc_n_write_flush(CDC_PORT_DEBUG);
    }
}

static stdio_driver_t cdc_stdio_driver = {
    .out_chars = cdc_stdio_out_chars,
    .out_flush = cdc_stdio_out_flush,
    .in_chars = NULL,  // No input on debug port
    .set_chars_available_callback = NULL,
    .next = NULL,
#if PICO_STDIO_ENABLE_CRLF_SUPPORT
    .crlf_enabled = PICO_STDIO_DEFAULT_CRLF,
#endif
};

#endif // CFG_TUD_CDC >= 2

// ============================================================================
// INITIALIZATION
// ============================================================================

void cdc_init(void)
{
    debug_enabled = true;

#if CFG_TUD_CDC >= 2
    // Register CDC debug port as stdio output
    stdio_set_driver_enabled(&cdc_stdio_driver, true);
#endif
}

void cdc_task(void)
{
    // TinyUSB handles CDC internally via tud_task()
    // This function reserved for future rx processing
}

// ============================================================================
// DATA PORT (CDC 0)
// ============================================================================

bool cdc_data_connected(void)
{
    return tud_cdc_n_connected(CDC_PORT_DATA);
}

uint32_t cdc_data_available(void)
{
    return tud_cdc_n_available(CDC_PORT_DATA);
}

uint32_t cdc_data_read(uint8_t* buffer, uint32_t bufsize)
{
    return tud_cdc_n_read(CDC_PORT_DATA, buffer, bufsize);
}

int32_t cdc_data_read_byte(void)
{
    uint8_t ch;
    if (tud_cdc_n_read(CDC_PORT_DATA, &ch, 1) == 1) {
        return ch;
    }
    return -1;
}

uint32_t cdc_data_write(const uint8_t* buffer, uint32_t bufsize)
{
    if (!tud_cdc_n_connected(CDC_PORT_DATA)) {
        return 0;
    }
    uint32_t written = tud_cdc_n_write(CDC_PORT_DATA, buffer, bufsize);
    tud_cdc_n_write_flush(CDC_PORT_DATA);
    return written;
}

uint32_t cdc_data_write_str(const char* str)
{
    return cdc_data_write((const uint8_t*)str, strlen(str));
}

void cdc_data_flush(void)
{
    tud_cdc_n_write_flush(CDC_PORT_DATA);
}

// ============================================================================
// DEBUG PORT (CDC 1)
// ============================================================================

bool cdc_debug_connected(void)
{
#if CFG_TUD_CDC >= 2
    return tud_cdc_n_connected(CDC_PORT_DEBUG);
#else
    return false;
#endif
}

int cdc_debug_printf(const char* format, ...)
{
#if CFG_TUD_CDC >= 2
    if (!debug_enabled || !tud_cdc_n_connected(CDC_PORT_DEBUG)) {
        return 0;
    }

    char buffer[256];
    va_list args;
    va_start(args, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (len > 0) {
        uint32_t written = tud_cdc_n_write(CDC_PORT_DEBUG, buffer, len);
        tud_cdc_n_write_flush(CDC_PORT_DEBUG);
        return written;
    }
    return 0;
#else
    (void)format;
    return 0;
#endif
}

uint32_t cdc_debug_write(const uint8_t* buffer, uint32_t bufsize)
{
#if CFG_TUD_CDC >= 2
    if (!debug_enabled || !tud_cdc_n_connected(CDC_PORT_DEBUG)) {
        return 0;
    }
    uint32_t written = tud_cdc_n_write(CDC_PORT_DEBUG, buffer, bufsize);
    tud_cdc_n_write_flush(CDC_PORT_DEBUG);
    return written;
#else
    (void)buffer;
    (void)bufsize;
    return 0;
#endif
}

void cdc_debug_flush(void)
{
#if CFG_TUD_CDC >= 2
    tud_cdc_n_write_flush(CDC_PORT_DEBUG);
#endif
}

void cdc_debug_set_enabled(bool enabled)
{
    debug_enabled = enabled;
}

bool cdc_debug_is_enabled(void)
{
    return debug_enabled;
}

// ============================================================================
// TINYUSB CDC CALLBACKS
// ============================================================================

// Invoked when CDC interface received data from host
void tud_cdc_rx_cb(uint8_t itf)
{
    (void)itf;
    // Data available - will be read via cdc_data_read()
}

// Invoked when CDC TX is complete
void tud_cdc_tx_complete_cb(uint8_t itf)
{
    (void)itf;
}

// Invoked when CDC line state changed (DTR/RTS)
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{
    (void)itf;
    (void)dtr;
    (void)rts;
}

// Invoked when CDC line coding changed (baud, parity, etc)
void tud_cdc_line_coding_cb(uint8_t itf, cdc_line_coding_t const* p_line_coding)
{
    (void)itf;
    (void)p_line_coding;
}

#else // CFG_TUD_CDC == 0

// Stub implementations when CDC is disabled
void cdc_init(void) {}
void cdc_task(void) {}
bool cdc_data_connected(void) { return false; }
uint32_t cdc_data_available(void) { return 0; }
uint32_t cdc_data_read(uint8_t* buffer, uint32_t bufsize) { (void)buffer; (void)bufsize; return 0; }
int32_t cdc_data_read_byte(void) { return -1; }
uint32_t cdc_data_write(const uint8_t* buffer, uint32_t bufsize) { (void)buffer; (void)bufsize; return 0; }
uint32_t cdc_data_write_str(const char* str) { (void)str; return 0; }
void cdc_data_flush(void) {}
bool cdc_debug_connected(void) { return false; }
int cdc_debug_printf(const char* format, ...) { (void)format; return 0; }
uint32_t cdc_debug_write(const uint8_t* buffer, uint32_t bufsize) { (void)buffer; (void)bufsize; return 0; }
void cdc_debug_flush(void) {}
void cdc_debug_set_enabled(bool enabled) { (void)enabled; }
bool cdc_debug_is_enabled(void) { return false; }

#endif // CFG_TUD_CDC
