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

// Output interface abstraction
#include "common/output_interface.h"
extern const OutputInterface* active_output;

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
#include "native/device/gamecube/gamecube_config.h"
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

    // Console-specific periodic task (if needed)
    if (active_output->task) {
      active_output->task();
    }
  }
}

int main(void)
{
  stdio_init_all();

  printf("\nUSB_RETRO::%s\n\n", active_output->name);

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

  // Initialize router system (Phase 2)
  router_config_t router_cfg = {
    #ifdef CONFIG_NGC
      .mode = ROUTING_MODE_MERGE,      // GameCube: merge all USB inputs (current behavior)
      .merge_mode = MERGE_ALL,
      .merge_all_inputs = true,
    #else
      .mode = ROUTING_MODE_SIMPLE,     // Other consoles: simple 1:1
      .merge_mode = MERGE_ALL,
      .merge_all_inputs = false,
    #endif
    .max_players_per_output = {4, 5, 8, 5, 1, 5, 4, 4},  // GC=4, PCE=5, 3DO=8, etc.

    // Phase 5: Input transformations
    #if defined(CONFIG_XB1)
      .transform_flags = TRANSFORM_MOUSE_TO_ANALOG,  // Xbox One: enable mouse-to-analog
      .mouse_drain_rate = 8,                          // Gradual drain (balance responsiveness/smoothness)
    #elif defined(CONFIG_PCE) || defined(CONFIG_LOOPY)
      .transform_flags = TRANSFORM_MOUSE_TO_ANALOG,  // PCEngine/Loopy: enable mouse support
      .mouse_drain_rate = 8,
    #else
      .transform_flags = TRANSFORM_NONE,             // Other consoles: no transformations
      .mouse_drain_rate = 8,
    #endif
  };
  router_init(&router_cfg);

  // Initialize app layer (Phase 5 - optional, only for app-based builds)
#ifdef CONFIG_NGC
  // GCUSB app initialization
  extern void app_init(void);
  app_init();
#endif

  // Initialize active output (console-specific or USB device)
  active_output->init();

  // Launch Core1 for console output protocol (if needed)
  if (active_output->core1_entry) {
    multicore_launch_core1(active_output->core1_entry);
  }

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
