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

#include "tusb.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "globals.h"

bool update_pending = false;
uint8_t gc_rumble = 0;
uint8_t gc_kb_led = 0;

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

extern void hid_init(void);
extern void hid_task(uint8_t rumble, uint8_t leds, uint8_t trigger_threshold, uint8_t test);
extern void xinput_task(uint8_t rumble);

extern void neopixel_init(void);
extern void neopixel_task(int pat);

extern void profile_indicator_init(void);
extern void profile_indicator_task(void);
extern uint8_t profile_indicator_get_rumble(void);
extern uint8_t profile_indicator_get_player_led(uint8_t player_count);

extern void players_init(void);

#ifdef CONFIG_NGC
extern void flash_settings_task(void);
#include "console/gamecube/gamecube_config.h"
extern gc_profile_t* get_active_profile(void);
#endif

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

    // profile indicator task (rumble and player LED patterns)
    profile_indicator_task();

#ifdef CONFIG_NGC
    // flash settings task (debounced flash writes for profile persistence)
    flash_settings_task();
#endif

    // Combine GameCube console rumble with profile indicator rumble
    uint8_t combined_rumble = gc_rumble | profile_indicator_get_rumble();

    // Get player LED value (combines keyboard mode LED with profile indicator)
    uint8_t player_led = profile_indicator_get_player_led(playersCount) | gc_kb_led;

    // Get adaptive trigger threshold from console profile/config
    uint8_t trigger_threshold = 0;
#ifdef CONFIG_NGC
    gc_profile_t* profile = get_active_profile();
    if (profile && profile->adaptive_triggers) {
      trigger_threshold = profile->l2_threshold;
    }
#endif

    // test pattern counter (managed by application layer)
    static uint8_t test_counter = 0;
    if (is_fun) test_counter++;

    // xinput rumble task
    xinput_task(combined_rumble);

#if CFG_TUH_HID
    // hid_device rumble/led task
    hid_task(combined_rumble, player_led, trigger_threshold, test_counter);

#endif
#ifdef CONFIG_PCE
    // detection of when a PCE scan is no longer in process (reset period)
    pce_task();

#elif CONFIG_NUON
    nuon_task();

#endif
  }
}

int main(void)
{
  stdio_init_all();

  printf("\nUSB_RETRO::");

  // board_init() removed in latest pico-sdk/TinyUSB

#ifndef CONFIG_LOOPY
  // pause briefly for stability before starting activity
  sleep_ms(250);
#endif

  hid_init(); // init hid device interfaces

  tusb_init(); // init tinyusb for usb host input

  neopixel_init(); // init multi-color status led

  profile_indicator_init(); // init profile indicator (rumble and player LED)

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
