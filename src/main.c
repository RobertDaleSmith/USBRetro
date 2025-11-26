/*
 * USBRetro - Modular controller firmware for RP2040-based devices
 *
 * A flexible foundation for building controller adapters, arcade sticks,
 * custom controllers, and any device that routes inputs to outputs.
 * Apps define the product behavior while the core handles the complexity.
 *
 * Inputs:  USB host (HID, X-input), Native (console controllers), BLE*, UART
 * Outputs: Native (GameCube, PCEngine, etc.), USB device*, BLE*, UART
 * Core:    Router, players, profiles, feedback, storage, LEDs
 *
 * Whether you're building a simple adapter or a full custom controller,
 * configure an app and let the firmware handle the rest.
 *
 * (* planned)
 *
 * Copyright (c) 2022-2025 Robert Dale Smith
 * https://github.com/RobertDaleSmith/USBRetro
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"

#include "core/output_interface.h"
#include "core/services/players/manager.h"
#include "core/services/leds/leds.h"
#include "core/services/storage/storage.h"
#include "usb/usbh/usbh.h"

// App layer (linked per-product)
extern void app_init(void);
extern const OutputInterface* app_get_output_interface(void);

// Cached output interface (set once at startup)
static const OutputInterface* output = NULL;

// Core 0 main loop - pinned in SRAM for consistent timing
static void __not_in_flash_func(core0_main)(void)
{
  while (1)
  {
    leds_task();
    players_task();
    storage_task();
    usbh_task();

    if (output->task) {
      output->task();
    }
  }
}

int main(void)
{
  stdio_init_all();

  printf("\n[usbretro] Starting...\n");

  sleep_ms(250);  // Brief pause for stability

  usbh_init();
  leds_init();
  storage_init();
  players_init();
  app_init();

  output = app_get_output_interface();
  output->init();

  if (output->core1_task) {
    multicore_launch_core1(output->core1_task);
  }

  core0_main();

  return 0;
}
