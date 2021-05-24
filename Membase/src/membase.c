/**
 * membase.c - PC Engine Memory Base 128 processor
 *
 * Copyright (c) 2021 Dave Shadoff
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "pico/stdlib.h"
#include "pico/time.h"
#include "pico/multicore.h"
#include "hardware/flash.h"
#include "hardware/pio.h"
#include "membase.pio.h"

// Set up a PIO state machine to shift in serial data, sampling with an
// external clock, and push the data to the RX FIFO, 1 bit at a time.


#ifdef ADAFRUIT_QTPY_RP2040	// if build for QTPY RP2040 board, use these pins

#define DATAIN_PIN	28
#define CLKIN_PIN	DATAIN_PIN + 1	// Note - must be consecutive for PIO code
#define DATAOUT_PIN	27
#define	IDENT_PIN	24
#define ACTIVE_PIN	6
#define WRSTAT_PIN	4
#define RDSTAT_PIN	3
#define	FLUSH_PIN	25

#else				// else assume build for RP Pico board, and use these pins

#define DATAIN_PIN	27
#define CLKIN_PIN	DATAIN_PIN + 1	// Note - must be consecutive for PIO code
#define DATAOUT_PIN	26
#define	IDENT_PIN	22
#define ACTIVE_PIN	6
#define WRSTAT_PIN	4
#define RDSTAT_PIN	3
#define	FLUSH_PIN	5

#endif

#define FLASH_OFFSET	(512 * 1024)	// How far into flash to store the memory data
#define FLASH_AMOUNT	(128 * 1024)
#define FLASH_SECTORS	(FLASH_AMOUNT / FLASH_SECTOR_SIZE)

#define SYNC_VALUE	0xA8	// bit signature to use as synchronizer on the joypad scan stream

#define CMD_WRITE	0	// command embedded into bitstream
#define CMD_READ	1


static uint8_t MemStore[FLASH_AMOUNT];
static bool in_transaction;

static uint8_t	sync_byte = 0;

static bool	rx_bit = false;

static int	page_num = 0;

static bool	rw_cmd = false;
static int	rw_addr = 0;
static int	bit_len = 0;
static int	byte_len = 0;

// Timestamping for delayed flush to flash memory
//
static absolute_time_t LastTransaction;
static int64_t idle_microseconds = 750000;
static bool DirtyPage[FLASH_SECTORS];
static bool AnyDirty;

PIO pio;
uint sm;


// Get the flash memory at initialization time, and place in SRAM
//
void __not_in_flash_func(ReadFlash)()
{
int i;
	memcpy(MemStore, (uint8_t *)XIP_BASE + FLASH_OFFSET, sizeof(MemStore));

	for (i = 0; i < FLASH_SECTORS; i ++) {
		DirtyPage[i] = false;
	}

	LastTransaction = at_the_end_of_time;	// Reset indicator for anything to writeback
	AnyDirty = false;
}

//
// Sequence through dirty pages and erase/rewrite them to flash
//
// Original intent was to have this run by the second processor so
// that the first processor could continue to read the input bitstream,
// but that hasn't been implemented yet
//
static void __not_in_flash_func(WriteFlash)()
{
int i;
uint SectorOffset;

	gpio_put(FLUSH_PIN, 1);

	AnyDirty = false;		// reset dirty flags before save; if data is updated again, there
					// should not be any moment when data dirty but flag is not set
	LastTransaction = at_the_end_of_time;

// Block commands are faster for large-scale erase
//	uint Interrupts = save_and_disable_interrupts();
//	flash_range_erase(FLASH_OFFSET, FLASH_AMOUNT);
//	flash_range_program(FLASH_OFFSET, &MemStore[0], FLASH_AMOUNT);
//	restore_interrupts(Interrupts);

	for (i = 0; i < FLASH_SECTORS; i ++) {
		if (DirtyPage[i] == true) {
			DirtyPage[i] = false;
			SectorOffset = i * FLASH_SECTOR_SIZE;
			uint Interrupts = save_and_disable_interrupts();
			flash_range_erase(FLASH_OFFSET + SectorOffset, FLASH_SECTOR_SIZE);
			flash_range_program(FLASH_OFFSET + SectorOffset, &MemStore[SectorOffset], FLASH_SECTOR_SIZE);
			restore_interrupts(Interrupts);
		}
	}
	gpio_put(FLUSH_PIN, 0);
}


static void __not_in_flash_func(process_signals)(void)
{
    while(1) {
	gpio_put(ACTIVE_PIN,  0);
	gpio_put(DATAOUT_PIN, 0);
	gpio_put(IDENT_PIN,   0);
	gpio_put(WRSTAT_PIN,  0);
	gpio_put(RDSTAT_PIN,  0);
	gpio_put(FLUSH_PIN,   0);

	// Get Sync byte (0xA8)
	//
	sync_byte = 0xFF;	// We don't want premature recognition of 0xA8 (which includes
				// 3 leading zeroes), force bits to all '1' to ensure at least
				// 8 bits have been read.

	in_transaction = false;

	while (sync_byte != SYNC_VALUE) {
		while (pio_sm_is_rx_fifo_empty(pio,sm))		// check if any bits to process...
		{						// while we're waiting, check if it's time to flush
			if (AnyDirty == true) {
				if (absolute_time_diff_us(LastTransaction, get_absolute_time()) > idle_microseconds) {
					// Note: if we flush to flash, we should not disable state machines
					//       in order to avoid a backlog of desynchronized bits which
					//       may cause problems
					pio_sm_set_enabled(pio,sm,false);
					WriteFlash();
					pio_sm_set_enabled(pio,sm,true);
				}
			}
		}
		rx_bit = pio_sm_get(pio,sm);

		sync_byte = (sync_byte >> 1) | (rx_bit << 7);
	}

	// We are now in an active session
	//
	gpio_put(ACTIVE_PIN, 1);
	in_transaction = true;

	// State A8_A1 - send IDENT - note that it is based on the bit sent in
	//
	rx_bit = pio_sm_get_blocking(pio,sm);
	gpio_put(IDENT_PIN, rx_bit);

	// State A8_A2 - send IDENT - note that it is based on the bit sent in
	//
	rx_bit = pio_sm_get_blocking(pio,sm);
	gpio_put(IDENT_PIN, rx_bit);

	// REQUEST type
	//
	rw_cmd = pio_sm_get_blocking(pio,sm);

	gpio_put(IDENT_PIN, 0);		// no more IDENT bit

	if (rw_cmd == CMD_WRITE)	// set appropriate LED output
		gpio_put(WRSTAT_PIN,1);
	else
		gpio_put(RDSTAT_PIN,1);

	// Get 10-bit address
	//
	rw_addr = 0;
	if (pio_sm_get_blocking(pio,sm)) rw_addr |= 0x00080;
	if (pio_sm_get_blocking(pio,sm)) rw_addr |= 0x00100;
	if (pio_sm_get_blocking(pio,sm)) rw_addr |= 0x00200;
	if (pio_sm_get_blocking(pio,sm)) rw_addr |= 0x00400;
	if (pio_sm_get_blocking(pio,sm)) rw_addr |= 0x00800;
	if (pio_sm_get_blocking(pio,sm)) rw_addr |= 0x01000;
	if (pio_sm_get_blocking(pio,sm)) rw_addr |= 0x02000;
	if (pio_sm_get_blocking(pio,sm)) rw_addr |= 0x04000;
	if (pio_sm_get_blocking(pio,sm)) rw_addr |= 0x08000;
	if (pio_sm_get_blocking(pio,sm)) rw_addr |= 0x10000;

	// Get 3-bit number of bits (less-than-byte transfer portion)
	//
	bit_len = 0;
	if (pio_sm_get_blocking(pio,sm)) bit_len |= 0x1;
	if (pio_sm_get_blocking(pio,sm)) bit_len |= 0x2;
	if (pio_sm_get_blocking(pio,sm)) bit_len |= 0x4;

	// Get 17-bit number of bytes
	//
	byte_len = 0;
	if (pio_sm_get_blocking(pio,sm)) byte_len |= 0x00001;
	if (pio_sm_get_blocking(pio,sm)) byte_len |= 0x00002;
	if (pio_sm_get_blocking(pio,sm)) byte_len |= 0x00004;
	if (pio_sm_get_blocking(pio,sm)) byte_len |= 0x00008;
	if (pio_sm_get_blocking(pio,sm)) byte_len |= 0x00010;
	if (pio_sm_get_blocking(pio,sm)) byte_len |= 0x00020;
	if (pio_sm_get_blocking(pio,sm)) byte_len |= 0x00040;
	if (pio_sm_get_blocking(pio,sm)) byte_len |= 0x00080;
	if (pio_sm_get_blocking(pio,sm)) byte_len |= 0x00100;
	if (pio_sm_get_blocking(pio,sm)) byte_len |= 0x00200;
	if (pio_sm_get_blocking(pio,sm)) byte_len |= 0x00400;
	if (pio_sm_get_blocking(pio,sm)) byte_len |= 0x00800;
	if (pio_sm_get_blocking(pio,sm)) byte_len |= 0x01000;
	if (pio_sm_get_blocking(pio,sm)) byte_len |= 0x02000;
	if (pio_sm_get_blocking(pio,sm)) byte_len |= 0x04000;
	if (pio_sm_get_blocking(pio,sm)) byte_len |= 0x08000;
	if (pio_sm_get_blocking(pio,sm)) byte_len |= 0x10000;

	// Transfer byte portion (read or write)
	//
	while (byte_len > 0) {
		if (rw_cmd == CMD_READ) {
			rx_bit = pio_sm_get_blocking(pio,sm); gpio_put(DATAOUT_PIN, ((MemStore[rw_addr] & 0x01) ? 1 : 0));
			rx_bit = pio_sm_get_blocking(pio,sm); gpio_put(DATAOUT_PIN, ((MemStore[rw_addr] & 0x02) ? 1 : 0));
			rx_bit = pio_sm_get_blocking(pio,sm); gpio_put(DATAOUT_PIN, ((MemStore[rw_addr] & 0x04) ? 1 : 0));
			rx_bit = pio_sm_get_blocking(pio,sm); gpio_put(DATAOUT_PIN, ((MemStore[rw_addr] & 0x08) ? 1 : 0));
			rx_bit = pio_sm_get_blocking(pio,sm); gpio_put(DATAOUT_PIN, ((MemStore[rw_addr] & 0x10) ? 1 : 0));
			rx_bit = pio_sm_get_blocking(pio,sm); gpio_put(DATAOUT_PIN, ((MemStore[rw_addr] & 0x20) ? 1 : 0));
			rx_bit = pio_sm_get_blocking(pio,sm); gpio_put(DATAOUT_PIN, ((MemStore[rw_addr] & 0x40) ? 1 : 0));
			rx_bit = pio_sm_get_blocking(pio,sm); gpio_put(DATAOUT_PIN, ((MemStore[rw_addr] & 0x80) ? 1 : 0));
		}
		else {
			AnyDirty = true;		// if we're writing, we will need to flush to flash later
			page_num = (rw_addr >> 12);
			DirtyPage[page_num] = true;

			MemStore[rw_addr] = 0;
			if (pio_sm_get_blocking(pio,sm)) MemStore[rw_addr] |= 0x01;
			if (pio_sm_get_blocking(pio,sm)) MemStore[rw_addr] |= 0x02;
			if (pio_sm_get_blocking(pio,sm)) MemStore[rw_addr] |= 0x04;
			if (pio_sm_get_blocking(pio,sm)) MemStore[rw_addr] |= 0x08;
			if (pio_sm_get_blocking(pio,sm)) MemStore[rw_addr] |= 0x10;
			if (pio_sm_get_blocking(pio,sm)) MemStore[rw_addr] |= 0x20;
			if (pio_sm_get_blocking(pio,sm)) MemStore[rw_addr] |= 0x40;
			if (pio_sm_get_blocking(pio,sm)) MemStore[rw_addr] |= 0x80;
		}
		rw_addr++;
		byte_len--;
	}

	// Transfer bit portion (read or write)
	//
	while (bit_len > 0) {
		if (rw_cmd == CMD_READ) {
			rx_bit = pio_sm_get_blocking(pio,sm); gpio_put(DATAOUT_PIN, ((MemStore[rw_addr] & 0x01) ? 1 : 0));
			bit_len--; if (bit_len == 0) break;
			rx_bit = pio_sm_get_blocking(pio,sm); gpio_put(DATAOUT_PIN, ((MemStore[rw_addr] & 0x02) ? 1 : 0));
			bit_len--; if (bit_len == 0) break;
			rx_bit = pio_sm_get_blocking(pio,sm); gpio_put(DATAOUT_PIN, ((MemStore[rw_addr] & 0x04) ? 1 : 0));
			bit_len--; if (bit_len == 0) break;
			rx_bit = pio_sm_get_blocking(pio,sm); gpio_put(DATAOUT_PIN, ((MemStore[rw_addr] & 0x08) ? 1 : 0));
			bit_len--; if (bit_len == 0) break;
			rx_bit = pio_sm_get_blocking(pio,sm); gpio_put(DATAOUT_PIN, ((MemStore[rw_addr] & 0x10) ? 1 : 0));
			bit_len--; if (bit_len == 0) break;
			rx_bit = pio_sm_get_blocking(pio,sm); gpio_put(DATAOUT_PIN, ((MemStore[rw_addr] & 0x20) ? 1 : 0));
			bit_len--; if (bit_len == 0) break;
			rx_bit = pio_sm_get_blocking(pio,sm); gpio_put(DATAOUT_PIN, ((MemStore[rw_addr] & 0x40) ? 1 : 0));
			bit_len--; if (bit_len == 0) break;
			rx_bit = pio_sm_get_blocking(pio,sm); gpio_put(DATAOUT_PIN, ((MemStore[rw_addr] & 0x80) ? 1 : 0));
			bit_len--; if (bit_len == 0) break;
			
		}
		else {
			AnyDirty = true;		// if we're writing, we will need to flush to flash later
			page_num = (rw_addr >> 12);
			DirtyPage[page_num] = true;

			MemStore[rw_addr] &= 0xFE; if (pio_sm_get_blocking(pio,sm)) MemStore[rw_addr] |= 0x01;
			bit_len--; if (bit_len == 0) break;
			MemStore[rw_addr] &= 0xFD; if (pio_sm_get_blocking(pio,sm)) MemStore[rw_addr] |= 0x02;
			bit_len--; if (bit_len == 0) break;
			MemStore[rw_addr] &= 0xFB; if (pio_sm_get_blocking(pio,sm)) MemStore[rw_addr] |= 0x04;
			bit_len--; if (bit_len == 0) break;
			MemStore[rw_addr] &= 0xF7; if (pio_sm_get_blocking(pio,sm)) MemStore[rw_addr] |= 0x08;
			bit_len--; if (bit_len == 0) break;
			MemStore[rw_addr] &= 0xEF; if (pio_sm_get_blocking(pio,sm)) MemStore[rw_addr] |= 0x10;
			bit_len--; if (bit_len == 0) break;
			MemStore[rw_addr] &= 0xDF; if (pio_sm_get_blocking(pio,sm)) MemStore[rw_addr] |= 0x20;
			bit_len--; if (bit_len == 0) break;
			MemStore[rw_addr] &= 0xBF; if (pio_sm_get_blocking(pio,sm)) MemStore[rw_addr] |= 0x40;
			bit_len--; if (bit_len == 0) break;
			MemStore[rw_addr] &= 0x7F; if (pio_sm_get_blocking(pio,sm)) MemStore[rw_addr] |= 0x80;
			bit_len--; if (bit_len == 0) break;
		}
	}

	// Trailing bits - extra 2 for write
	//
	if (rw_cmd == CMD_WRITE) {
		rx_bit = pio_sm_get_blocking(pio,sm);
		gpio_put(DATAOUT_PIN, 0);
		rx_bit = pio_sm_get_blocking(pio,sm);
	}

	// Trailing bits - final 3 for both read and write
	//
	rx_bit = pio_sm_get_blocking(pio,sm);
	gpio_put(DATAOUT_PIN, 0);
	rx_bit = pio_sm_get_blocking(pio,sm);
	rx_bit = pio_sm_get_blocking(pio,sm);

	// Timestamp last write (or read while dirty); flush to flash
	// should happen only after a certain amount of time without activity
	//
	if (AnyDirty)
		LastTransaction = get_absolute_time();

	in_transaction = false;

	while (gpio_get(CLKIN_PIN) != 0);	// sustain the final value until clock transitions low
						// then become inactive and reset all values to initial
    }

}

int main() {
    stdio_init_all();

    // set up all the GPIOs
    //
    gpio_init(ACTIVE_PIN);		// ACTIVE_PIN  is an indicator (yellow) and also sets 74HC157 to send this data instead of joypad)
    gpio_init(DATAOUT_PIN);		// DATAOUT_PIN is the D0 data to send back via joypad
    gpio_init(IDENT_PIN);		// IDENT_PIN   is the D2 data to send back via joypad - it identifies that the sync signal was detected
    gpio_init(WRSTAT_PIN);		// WRSTAT_PIN  is the 'write' indicator LED (red)
    gpio_init(RDSTAT_PIN);		// RDSTAT_PIN  is the 'read' indicator LED (green)
    gpio_init(FLUSH_PIN);		// FLUSH_PIN   is the 'writeback to flash' indicator LED (blue if possible)

    gpio_set_dir(ACTIVE_PIN,  GPIO_OUT);
    gpio_set_dir(DATAOUT_PIN, GPIO_OUT);
    gpio_set_dir(IDENT_PIN,   GPIO_OUT);
    gpio_set_dir(WRSTAT_PIN,  GPIO_OUT);
    gpio_set_dir(RDSTAT_PIN,  GPIO_OUT);
    gpio_set_dir(FLUSH_PIN,   GPIO_OUT);


    gpio_put(ACTIVE_PIN,  0);		// turn off all LEDs to start
    gpio_put(WRSTAT_PIN,  0);
    gpio_put(RDSTAT_PIN,  0);
    gpio_put(FLUSH_PIN,   0);

    sleep_ms(1000);			// startup delay

    // Load the membase program, and configure a free state machine
    // to run the program.
    pio = pio0;
    uint offset = pio_add_program(pio, &membase_program);
    sm = pio_claim_unused_sm(pio, true);
    membase_program_init(pio, sm, offset, DATAIN_PIN);

    gpio_put(ACTIVE_PIN,  1);		// initial startup indicator - turn on all LEDs briefly
    gpio_put(DATAOUT_PIN, 0);
    gpio_put(IDENT_PIN,   0);
    gpio_put(WRSTAT_PIN,  1);
    gpio_put(RDSTAT_PIN,  1);
    gpio_put(FLUSH_PIN,   1);

    sleep_ms(750);

    ReadFlash();				// initialize SRAM from Flash

    process_signals();
}
