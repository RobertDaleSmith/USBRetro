// sony_ds5.c
#include "sony_ds5.h"
#include "globals.h"
#include "bsp/board_api.h"

// DualSense instance state
typedef struct TU_ATTR_PACKED
{
  uint8_t rumble;
  uint8_t player;
} ds5_instance_t;

// Cached device report properties on mount
typedef struct TU_ATTR_PACKED
{
  ds5_instance_t instances[CFG_TUH_HID];
} ds5_device_t;

static ds5_device_t ds5_devices[MAX_DEVICES] = { 0 };

// check if device is Sony PlayStation 5 controllers
bool is_sony_ds5(uint16_t vid, uint16_t pid) {
  return ((vid == 0x054c && pid == 0x0ce6)); // Sony DualSense
}

// check if 2 reports are different enough
bool diff_report_ds5(sony_ds5_report_t const* rpt1, sony_ds5_report_t const* rpt2) {
  // Check x1 to ry with a threshold
  if (diff_than_n(rpt1->x1, rpt2->x1, 2) || diff_than_n(rpt1->y1, rpt2->y1, 2) ||
      diff_than_n(rpt1->x2, rpt2->x2, 2) || diff_than_n(rpt1->y2, rpt2->y2, 2) ||
      diff_than_n(rpt1->rx, rpt2->rx, 2) || diff_than_n(rpt1->ry, rpt2->ry, 2)) {
    return true;
  }

  // check the base buttons dpad -> r3 then
  // manually check fields up to 'counter'
  if (memcmp(&rpt1->rz + 1, &rpt2->rz + 1, 2) ||
      rpt1->ps != rpt2->ps ||
      rpt1->tpad != rpt2->tpad ||
      rpt1->mute != rpt2->mute) {
    return true;
  }

  // Check tpad_f1_down and tpad_f1_pos
  if (rpt1->tpad_f1_down != rpt2->tpad_f1_down ||
    memcmp(rpt1->tpad_f1_pos, rpt2->tpad_f1_pos, sizeof(rpt1->tpad_f1_pos)) != 0) {
    return true;
  }

  return false;
}

// process usb hid input reports
void input_sony_ds5(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
  // previous report used to compare for changes
  static sony_ds5_report_t prev_report[5] = { 0 };

  uint8_t const report_id = report[0];
  report++;
  len--;

  // all buttons state is stored in ID 1
  if (report_id == 1)
  {
    sony_ds5_report_t ds5_report;
    memcpy(&ds5_report, report, sizeof(ds5_report));

    // counter is +1, assign to make it easier to compare 2 report
    prev_report[dev_addr-1].counter = ds5_report.counter;

    if ( diff_report_ds5(&prev_report[dev_addr-1], &ds5_report) )
    {
      TU_LOG1("(x1, y1, x2, y2, rx, ry) = (%u, %u, %u, %u, %u, %u)\r\n", ds5_report.x1, ds5_report.y1, ds5_report.x2, ds5_report.y2, ds5_report.rx, ds5_report.ry);
      TU_LOG1("DPad = %s ", dpad_str[ds5_report.dpad]);

      if (ds5_report.square   ) TU_LOG1("Square ");
      if (ds5_report.cross    ) TU_LOG1("Cross ");
      if (ds5_report.circle   ) TU_LOG1("Circle ");
      if (ds5_report.triangle ) TU_LOG1("Triangle ");

      if (ds5_report.l1       ) TU_LOG1("L1 ");
      if (ds5_report.r1       ) TU_LOG1("R1 ");
      if (ds5_report.l2       ) TU_LOG1("L2 ");
      if (ds5_report.r2       ) TU_LOG1("R2 ");

      if (ds5_report.share    ) TU_LOG1("Share ");
      if (ds5_report.option   ) TU_LOG1("Option ");
      if (ds5_report.l3       ) TU_LOG1("L3 ");
      if (ds5_report.r3       ) TU_LOG1("R3 ");

      if (ds5_report.ps       ) TU_LOG1("PS ");
      if (ds5_report.tpad     ) TU_LOG1("TPad ");
      if (ds5_report.mute     ) TU_LOG1("Mute ");

      if (!ds5_report.tpad_f1_down) TU_LOG1("F1 ");

      uint16_t tx = (((ds5_report.tpad_f1_pos[1] & 0x0f) << 8)) | ((ds5_report.tpad_f1_pos[0] & 0xff) << 0);
      uint16_t ty = (((ds5_report.tpad_f1_pos[1] & 0xf0) >> 4)) | ((ds5_report.tpad_f1_pos[2] & 0xff) << 4);
      // TU_LOG1(" (tx, ty) = (%u, %u)\r\n", tx, ty);
      TU_LOG1("\r\n");

      bool dpad_up    = (ds5_report.dpad == 0 || ds5_report.dpad == 1 || ds5_report.dpad == 7);
      bool dpad_right = (ds5_report.dpad >= 1 && ds5_report.dpad <= 3);
      bool dpad_down  = (ds5_report.dpad >= 3 && ds5_report.dpad <= 5);
      bool dpad_left  = (ds5_report.dpad >= 5 && ds5_report.dpad <= 7);
      bool button_z = ds5_report.share || ds5_report.tpad;
      bool has_6btns = true;

      buttons = (((ds5_report.r3)       ? 0x00 : 0x20000) |
                 ((ds5_report.l3)       ? 0x00 : 0x10000) |
                 ((ds5_report.r1)       ? 0x00 : 0x08000) |
                 ((ds5_report.l1)       ? 0x00 : 0x04000) |
                 ((ds5_report.square)   ? 0x00 : 0x02000) |
                 ((ds5_report.triangle) ? 0x00 : 0x01000) |
                 ((has_6btns)           ? 0x00 : 0x00800) |
                 ((ds5_report.ps)       ? 0x00 : 0x00400) |
                 ((ds5_report.r2)       ? 0x00 : 0x00200) |
                 ((ds5_report.l2)       ? 0x00 : 0x00100) |
                 ((dpad_left)           ? 0x00 : 0x00008) |
                 ((dpad_down)           ? 0x00 : 0x00004) |
                 ((dpad_right)          ? 0x00 : 0x00002) |
                 ((dpad_up)             ? 0x00 : 0x00001) |
                 ((ds5_report.option)   ? 0x00 : 0x00080) |
                 ((button_z)            ? 0x00 : 0x00040) |
                 ((ds5_report.cross)    ? 0x00 : 0x00020) |
                 ((ds5_report.circle)   ? 0x00 : 0x00010));

#ifdef CONFIG_NUON
      // Touch Pad - Atari50 Tempest like spinner input
      if (!ds5_report.tpad_f1_down) {
        // scroll spinner value while swipping
        if (tpadDragging) {
          // get directional difference delta
          int16_t delta = 0;
          if (tx >= tpadLastPos) delta = tx - tpadLastPos;
          else delta = (-1) * (tpadLastPos - tx);

          // check max/min delta value
          if (delta > 12) delta = 12;
          if (delta < -12) delta = -12;

          // inc global spinner value by delta
          spinner += delta;

          // check max/min spinner value
          if (spinner > 255) spinner -= 255;
          if (spinner < 0) spinner = 256 - (-1 * spinner);
        }

        tpadLastPos = tx;
        tpadDragging = true;
      } else {
        tpadDragging = false;
      }
      // TU_LOG1(" (spinner) = (%u)\r\n", spinner);
#endif
      uint8_t analog_1x = ds5_report.x1;
      uint8_t analog_1y = 255 - ds5_report.y1;
      uint8_t analog_2x = ds5_report.x2;
      uint8_t analog_2y = 255 - ds5_report.y2;
      uint8_t analog_l = ds5_report.rx;
      uint8_t analog_r = ds5_report.ry;

      // keep analog within range [1-255]
      ensureAllNonZero(&analog_1x, &analog_1y, &analog_2x, &analog_2y);

      // add to accumulator and post to the state machine
      // if a scan from the host machine is ongoing, wait
      post_globals(
        dev_addr,
        instance,
        buttons,
        analog_1x, // Left Analog X
        analog_1y, // Left Analog Y
        analog_2x, // Right Analog X
        analog_2y, // Right Analog Y
        analog_l,  // Left Trigger
        analog_r,  // Right Trigger
        0,
        spinner    // Spinner Quad X
      );

      prev_report[dev_addr-1] = ds5_report;
    }
  }
}

// process usb hid output reports
void output_sony_ds5(uint8_t dev_addr, uint8_t instance, int player_index, uint8_t rumble) {
  ds5_feedback_t ds5_fb = {0};
  int32_t perc_threshold_l = -1;
  int32_t perc_threshold_r = -1;

  // set flags for trigger_r, trigger_l, lightbar, and player_led
  ds5_fb.flags |= (1 << 0 | 1 << 1); // haptics
  ds5_fb.flags |= (1 << 10); // lightbar
  ds5_fb.flags |= (1 << 12); // player_led
#ifdef CONFIG_NGC
  // gamecube simulated triggers
  ds5_fb.flags |= (1 << 2); // trigger_r
  ds5_fb.flags |= (1 << 3); // trigger_l

  if (GC_TRIGGER_THRESHOLD > perc_threshold_l) {
    perc_threshold_l = GC_TRIGGER_THRESHOLD;
  }

  if (GC_TRIGGER_THRESHOLD > perc_threshold_r) {
    perc_threshold_r = GC_TRIGGER_THRESHOLD;
  }

  // gamecube simulated analog/digital click
  uint8_t l2_start_resistance_value = (perc_threshold_l * 255) / 100;
  uint8_t r2_start_resistance_value = (perc_threshold_r * 255) / 100;

  uint8_t l2_trigger_start_resistance = (uint8_t)(0x94 * (l2_start_resistance_value / 255.0));
  uint8_t l2_trigger_effect_force =
    (uint8_t)((0xb4 - l2_trigger_start_resistance) * (l2_start_resistance_value / 255.0) + l2_trigger_start_resistance);

  uint8_t r2_trigger_start_resistance = (uint8_t)(0x94 * (r2_start_resistance_value / 255.0));
  uint8_t r2_trigger_effect_force =
    (uint8_t)((0xb4 - r2_trigger_start_resistance) * (r2_start_resistance_value / 255.0) + r2_trigger_start_resistance);

  // gamecube trigger left click
  ds5_fb.trigger_l.motor_mode = perc_threshold_r > -1 ? 0x02 : 0x00; // Set type
  ds5_fb.trigger_l.start_resistance = l2_trigger_start_resistance;
  ds5_fb.trigger_l.effect_force = l2_trigger_effect_force;
  ds5_fb.trigger_l.range_force = 0xff;

  // gamecube trigger right click
  ds5_fb.trigger_r.motor_mode = perc_threshold_r > -1 ? 0x02 : 0x00; // Set type
  ds5_fb.trigger_r.start_resistance = r2_trigger_start_resistance;
  ds5_fb.trigger_r.effect_force = r2_trigger_effect_force;
  ds5_fb.trigger_r.range_force = 0xff;

  switch (player_index+1)
  {
  case 1: // purple
    ds5_fb.player_led = 0b00100;
    ds5_fb.lightbar_r = 20;
    ds5_fb.lightbar_b = 40;
    break;

  case 2: // blue
    ds5_fb.player_led = 0b01010;
    ds5_fb.lightbar_b = 64;
    break;

  case 3: // red
    ds5_fb.player_led = 0b10101;
    ds5_fb.lightbar_r = 64;
    break;

  case 4: // green
    ds5_fb.player_led = 0b11011;
    ds5_fb.lightbar_g = 64;
    break;

  case 5: // yellow
    ds5_fb.player_led = 0b11111;
    ds5_fb.lightbar_r = 64;
    ds5_fb.lightbar_g = 64;
    break;

  default: // white
    ds5_fb.player_led = 0;
    ds5_fb.lightbar_b = 32;
    ds5_fb.lightbar_g = 32;
    ds5_fb.lightbar_r = 32;
    break;
  }
#elif CONFIG_XB1
  switch (player_index+1)
  {
  case 1: // green
    ds5_fb.player_led = 0b00100;
    ds5_fb.lightbar_g = 64;
    break;

  case 2: // blue
    ds5_fb.player_led = 0b01010;
    ds5_fb.lightbar_b = 64;
    break;

  case 3: // red
    ds5_fb.player_led = 0b10101;
    ds5_fb.lightbar_r = 64;
    break;

  case 4: // purple
    ds5_fb.player_led = 0b11011;
    ds5_fb.lightbar_r = 20;
    ds5_fb.lightbar_b = 40;
    break;

  case 5: // yellow
    ds5_fb.player_led = 0b11111;
    ds5_fb.lightbar_r = 64;
    ds5_fb.lightbar_g = 64;
    break;

  default: // white
    ds5_fb.player_led = 0;
    ds5_fb.lightbar_b = 32;
    ds5_fb.lightbar_g = 32;
    ds5_fb.lightbar_r = 32;
    break;
  }
#elif CONFIG_NUON
  switch (player_index+1)
  {
  case 1: // red
    ds5_fb.player_led = 0b00100;
    ds5_fb.lightbar_r = 64;
    break;

  case 2: // blue
    ds5_fb.player_led = 0b01010;
    ds5_fb.lightbar_b = 64;
    break;

  case 3: // green
    ds5_fb.player_led = 0b10101;
    ds5_fb.lightbar_g = 64;
    break;

  case 4: // purple
    ds5_fb.player_led = 0b11011;
    ds5_fb.lightbar_r = 20;
    ds5_fb.lightbar_b = 40;
    break;

  case 5: // yellow
    ds5_fb.player_led = 0b11111;
    ds5_fb.lightbar_r = 64;
    ds5_fb.lightbar_g = 64;
    break;

  default: // white
    ds5_fb.player_led = 0;
    ds5_fb.lightbar_b = 32;
    ds5_fb.lightbar_g = 32;
    ds5_fb.lightbar_r = 32;
    break;
  }
#elif CONFIG_PCE
  switch (player_index+1)
  {
  case 1: // blue
    ds5_fb.player_led = 0b00100;
    ds5_fb.lightbar_b = 64;
    break;

  case 2: // red
    ds5_fb.player_led = 0b01010;
    ds5_fb.lightbar_r = 64;
    break;

  case 3: // green
    ds5_fb.player_led = 0b10101;
    ds5_fb.lightbar_g = 64;
    break;

  case 4: // purple
    ds5_fb.player_led = 0b11011;
    ds5_fb.lightbar_r = 20;
    ds5_fb.lightbar_b = 40;
    break;

  case 5: // yellow
    ds5_fb.player_led = 0b11111;
    ds5_fb.lightbar_r = 64;
    ds5_fb.lightbar_g = 64;
    break;

  default: // white
    ds5_fb.player_led = 0;
    ds5_fb.lightbar_b = 32;
    ds5_fb.lightbar_g = 32;
    ds5_fb.lightbar_r = 32;
    break;
  }
#endif
  // fun
  if (player_index+1 && is_fun) {
    ds5_fb.player_led = fun_player;
    ds5_fb.lightbar_r = fun_inc;
    ds5_fb.lightbar_g = fun_inc+64;
    ds5_fb.lightbar_b = fun_inc+128;
  }

  if (rumble) {
    ds5_fb.rumble_l = 192;
    ds5_fb.rumble_r = 192;
  } else {
    ds5_fb.rumble_l = 0;
    ds5_fb.rumble_r = 0;
  }

  if (ds5_devices[dev_addr].instances[instance].rumble != rumble ||
      ds5_devices[dev_addr].instances[instance].player != ds5_fb.player_led ||
      is_fun)
  {
    ds5_devices[dev_addr].instances[instance].rumble = rumble;
    ds5_devices[dev_addr].instances[instance].player = ds5_fb.player_led & 0xff;
    tuh_hid_send_report(dev_addr, instance, 5, &ds5_fb, sizeof(ds5_fb));
  }
}

// process usb hid output reports
void task_sony_ds5(uint8_t dev_addr, uint8_t instance, int player_index, uint8_t rumble, uint8_t leds) {
  const uint32_t interval_ms = 20;
  static uint32_t start_ms = 0;

  uint32_t current_time_ms = board_millis();
  if (current_time_ms - start_ms >= interval_ms) {
    start_ms = current_time_ms;
    output_sony_ds5(dev_addr, instance, player_index, rumble);
  }
}

// resets default values in case devices are hotswapped
void unmount_sony_ds5(uint8_t dev_addr, uint8_t instance)
{
  ds5_devices[dev_addr].instances[instance].rumble = 0;
  ds5_devices[dev_addr].instances[instance].player = 0xff;
}

DeviceInterface sony_ds5_interface = {
  .name = "Sony DualSense",
  .is_device = is_sony_ds5,
  .process = input_sony_ds5,
  .task = task_sony_ds5,
  .unmount = unmount_sony_ds5,
};
