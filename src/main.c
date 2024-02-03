/* 
 * USBRetro - Adapts most USB controllers/mice/keyboards for use with
 *            various retro consoles and computers. Built specifically
 *            for the Raspberry Pi Pico or other RP2040 based MCUs.
 *
 * Copyright (c) 2022-2023 Robert Dale Smith for Controller Adapter
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bsp/board_api.h"
#include "tusb.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "globals.h"

// include console specific handling
#ifdef CONFIG_NGC
#include "console/gamecube/gamecube.h"
#elif CONFIG_LOOPY
#include "console/loopy/loopy.h"
#elif CONFIG_NUON
#include "console/nuon/nuon.h"
#elif CONFIG_PCE
#include "console/pcengine/pcengine.h"
#elif CONFIG_XB1
#include "console/xboxone/xboxone.h"
#endif

extern void hid_app_init(void);
extern void hid_app_task(uint8_t rumble, uint8_t leds);
extern void xinput_task(uint8_t rumble);

extern void neopixel_init(void);
extern void neopixel_task(int pat);

extern void players_init(void);

/*------------- MAIN -------------*/

// note that "__not_in_flash_func" functions are loaded
// and "pinned" in SRAM - not paged in/out from XIP flash
//

//
// process_signals - inner-loop processing of events:
//                   - USB polling
//                   - event processing
//
static void __not_in_flash_func(process_signals)(void)
{
  while (1)
  {
    // tinyusb host task
    tuh_task();

    // neopixel task
    neopixel_task(playersCount);

    // xinput rumble task
    xinput_task(gc_rumble);

#if CFG_TUH_HID
    // hid_device rumble/led task
    hid_app_task(gc_rumble, gc_kb_led);

#endif
#ifdef CONFIG_PCE
    // detection of when a PCE scan is no longer in process (reset period)
    pce_task();

#endif
  }
}

int main(void)
{
  printf("\nUSB_RETRO::");

  board_init();

#ifndef CONFIG_LOOPY
  // pause briefly for stability before starting activity
  sleep_ms(250);
#endif

  hid_app_init(); // init hid device interfaces

  tusb_init(); // init tinyusb for usb host input

  neopixel_init(); // init multi-color status led

  players_init(); // init multi-player management

#ifdef CONFIG_NGC
  printf("GAMECUBE");
  ngc_init();
#elif CONFIG_LOOPY
  printf("LOOPY");
  loopy_init();
#elif CONFIG_NUON
  printf("NUON");
  nuon_init();
#elif CONFIG_PCE
  printf("PCENGINE");
  pce_init();
#elif CONFIG_XB1
  printf("XBOXONE");
  xb1_init();
#endif
  printf("\n\n");

  multicore_launch_core1(core1_entry);

  process_signals();

  return 0;
}

//--------------------------------------------------------------------+
// TinyUSB Callbacks
//--------------------------------------------------------------------+
#if CFG_TUH_HID

void tuh_mount_cb(uint8_t dev_addr)
{
  // application set-up
  printf("A device with address %d is mounted\r\n", dev_addr);
}

void tuh_umount_cb(uint8_t dev_addr)
{
  // application tear-down
  printf("A device with address %d is unmounted \r\n", dev_addr);

  // if ((--playersCount) < 0) playersCount = 0;
  remove_players_by_address(dev_addr, -1);

  // resets fun
  is_fun = false;
}

#endif
