// sony_ds4.c
#include "sony_ds4.h"
#include "globals.h"
#include "pico/time.h"

// DualSense instance state
typedef struct TU_ATTR_PACKED
{
  uint8_t rumble;
  uint8_t player;
} ds4_instance_t;

// Cached device report properties on mount
typedef struct TU_ATTR_PACKED
{
  ds4_instance_t instances[CFG_TUH_HID];
} ds4_device_t;

static ds4_device_t ds4_devices[MAX_DEVICES] = { 0 };

// check if device is Sony PlayStation 4 controllers
bool is_sony_ds4(uint16_t vid, uint16_t pid) {
  return ( (vid == 0x054c && (pid == 0x09cc || pid == 0x05c4)) // Sony DualShock4 
    || (vid == 0x0f0d && pid == 0x005e) // Hori FC4 
    || (vid == 0x0f0d && pid == 0x00ee) // Hori PS4 Mini (PS4-099U) 
    || (vid == 0x1f4f && pid == 0x1002) // ASW GG xrd controller
    || (vid == 0x1532 && pid == 0x0401) // Razer Panthera PS4 Controller (GP2040-CE PS4 Mode)
  );
}

// check if 2 reports are different enough
bool diff_report_ds4(sony_ds4_report_t const* rpt1, sony_ds4_report_t const* rpt2)
{
  bool result;

  // x, y, z, rz must different than 2 to be counted
  result = diff_than_n(rpt1->x, rpt2->x, 2) || diff_than_n(rpt1->y, rpt2->y, 2) ||
           diff_than_n(rpt1->z, rpt2->z, 2) || diff_than_n(rpt1->rz, rpt2->rz, 2) ||
           diff_than_n(rpt1->l2_trigger, rpt2->l2_trigger, 2) ||
           diff_than_n(rpt1->r2_trigger, rpt2->r2_trigger, 2);

  // check the reset with mem compare
  result |= memcmp(&rpt1->rz + 1, &rpt2->rz + 1, 2);
  result |= (rpt1->ps != rpt2->ps);
  result |= (rpt1->tpad != rpt2->tpad);
  result |= memcmp(&rpt1->tpad_f1_pos, &rpt2->tpad_f1_pos, 3);

  return result;
}

// process usb hid input reports
void input_sony_ds4(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
  uint32_t buttons;
  // previous report used to compare for changes
  static sony_ds4_report_t prev_report[5] = { 0 };

  uint8_t const report_id = report[0];
  report++;
  len--;

  // all buttons state is stored in ID 1
  if (report_id == 1)
  {
    sony_ds4_report_t ds4_report;
    memcpy(&ds4_report, report, sizeof(ds4_report));

    // counter is +1, assign to make it easier to compare 2 report
    prev_report[dev_addr-1].counter = ds4_report.counter;

    // only print if changes since it is polled ~ 5ms
    // Since count+1 after each report and  x, y, z, rz fluctuate within 1 or 2
    // We need more than memcmp to check if report is different enough
    if ( diff_report_ds4(&prev_report[dev_addr-1], &ds4_report) )
    {
      TU_LOG1("(x, y, z, rz, l, r) = (%u, %u, %u, %u, %u, %u)\r\n", ds4_report.x, ds4_report.y, ds4_report.z, ds4_report.rz, ds4_report.r2_trigger, ds4_report.l2_trigger);
      TU_LOG1("DPad = %s ", ds4_report.dpad);

      if (ds4_report.square   ) TU_LOG1("Square ");
      if (ds4_report.cross    ) TU_LOG1("Cross ");
      if (ds4_report.circle   ) TU_LOG1("Circle ");
      if (ds4_report.triangle ) TU_LOG1("Triangle ");

      if (ds4_report.l1       ) TU_LOG1("L1 ");
      if (ds4_report.r1       ) TU_LOG1("R1 ");
      if (ds4_report.l2       ) TU_LOG1("L2 ");
      if (ds4_report.r2       ) TU_LOG1("R2 ");

      if (ds4_report.share    ) TU_LOG1("Share ");
      if (ds4_report.option   ) TU_LOG1("Option ");
      if (ds4_report.l3       ) TU_LOG1("L3 ");
      if (ds4_report.r3       ) TU_LOG1("R3 ");

      if (ds4_report.ps       ) TU_LOG1("PS ");
      if (ds4_report.tpad     ) TU_LOG1("TPad ");

      if (!ds4_report.tpad_f1_down) TU_LOG1("F1 ");

      uint16_t tx = (((ds4_report.tpad_f1_pos[1] & 0x0f) << 8)) | ((ds4_report.tpad_f1_pos[0] & 0xff) << 0);
      uint16_t ty = (((ds4_report.tpad_f1_pos[1] & 0xf0) >> 4)) | ((ds4_report.tpad_f1_pos[2] & 0xff) << 4);
      // TU_LOG1(" (tx, ty) = (%u, %u)\r\n", tx, ty);
      // TU_LOG1("\r\n");

      bool dpad_up    = (ds4_report.dpad == 0 || ds4_report.dpad == 1 || ds4_report.dpad == 7);
      bool dpad_right = ((ds4_report.dpad >= 1 && ds4_report.dpad <= 3));
      bool dpad_down  = ((ds4_report.dpad >= 3 && ds4_report.dpad <= 5));
      bool dpad_left  = ((ds4_report.dpad >= 5 && ds4_report.dpad <= 7));

      buttons = (((dpad_up)             ? 0x00 : USBR_BUTTON_DU) |
                 ((dpad_down)           ? 0x00 : USBR_BUTTON_DD) |
                 ((dpad_left)           ? 0x00 : USBR_BUTTON_DL) |
                 ((dpad_right)          ? 0x00 : USBR_BUTTON_DR) |
                 ((ds4_report.cross)    ? 0x00 : USBR_BUTTON_B1) |
                 ((ds4_report.circle)   ? 0x00 : USBR_BUTTON_B2) |
                 ((ds4_report.square)   ? 0x00 : USBR_BUTTON_B3) |
                 ((ds4_report.triangle) ? 0x00 : USBR_BUTTON_B4) |
                 ((ds4_report.l1)       ? 0x00 : USBR_BUTTON_L1) |
                 ((ds4_report.r1)       ? 0x00 : USBR_BUTTON_R1) |
                 ((ds4_report.l2)       ? 0x00 : USBR_BUTTON_L2) |
                 ((ds4_report.r2)       ? 0x00 : USBR_BUTTON_R2) |
                 ((ds4_report.share)    ? 0x00 : USBR_BUTTON_S1) |
                 ((ds4_report.option)   ? 0x00 : USBR_BUTTON_S2) |
                 ((ds4_report.l3)       ? 0x00 : USBR_BUTTON_L3) |
                 ((ds4_report.r3)       ? 0x00 : USBR_BUTTON_R3) |
                 ((ds4_report.ps)       ? 0x00 : USBR_BUTTON_A1) |
                 ((ds4_report.tpad)     ? 0x00 : USBR_BUTTON_A2) |
                 ((1)/*has_6btns*/      ? 0x00 : 0x800));

      uint8_t analog_1x = ds4_report.x;
      uint8_t analog_1y = 255 - ds4_report.y;
      uint8_t analog_2x = ds4_report.z;
      uint8_t analog_2y = 255 - ds4_report.rz;
      uint8_t analog_l = ds4_report.l2_trigger;
      uint8_t analog_r = ds4_report.r2_trigger;

#ifdef CONFIG_NUON
      // Touch Pad - Atari50 Tempest like spinner input
      if (!ds4_report.tpad_f1_down) {
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
      // keep analog within range [1-255]
      ensureAllNonZero(&analog_1x, &analog_1y, &analog_2x, &analog_2y);

      // adds deadzone
      uint8_t deadzone = 40;
      if (analog_1x > (128-(deadzone/2)) && analog_1x < (128+(deadzone/2))) analog_1x = 128;
      if (analog_1y > (128-(deadzone/2)) && analog_1y < (128+(deadzone/2))) analog_1y = 128;
      if (analog_2x > (128-(deadzone/2)) && analog_2x < (128+(deadzone/2))) analog_2x = 128;
      if (analog_2y > (128-(deadzone/2)) && analog_2y < (128+(deadzone/2))) analog_2y = 128;

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

      prev_report[dev_addr-1] = ds4_report;
    }
  }
}

// process usb hid output reports
void output_sony_ds4(uint8_t dev_addr, uint8_t instance, int player_index, uint8_t rumble) {
  sony_ds4_output_report_t output_report = {0};
  output_report.set_led = 1;

#ifdef CONFIG_NGC
  switch (player_index+1) {
    case 1: // purple
      output_report.lightbar_red = 20; // purple
      output_report.lightbar_blue = 40;//
      break;

    case 2: // blue
      output_report.lightbar_blue = 64;
      break;

    case 3: // red
      output_report.lightbar_red = 64;
      break;

    case 4: // green
      output_report.lightbar_green = 64;
      break;

    case 5: // yellow
      output_report.lightbar_red = 64;
      output_report.lightbar_green = 64;
      break;

    default: // white
      output_report.lightbar_blue = 32;
      output_report.lightbar_green = 32;
      output_report.lightbar_red = 32;
      break;
  }
#elif CONFIG_XB1
  switch (player_index+1) {
    case 1: // green
      output_report.lightbar_green = 64;
      break;

    case 2: // blue
      output_report.lightbar_blue = 64;
      break;

    case 3: // red
      output_report.lightbar_red = 64;
      break;

    case 4: // purple
      output_report.lightbar_red = 20; // purple
      output_report.lightbar_blue = 40;//
      break;

    case 5: // yellow
      output_report.lightbar_red = 64;
      output_report.lightbar_green = 64;
      break;

    default: // white
      output_report.lightbar_blue = 32;
      output_report.lightbar_green = 32;
      output_report.lightbar_red = 32;
      break;
  }
#elif CONFIG_NUON
  switch (player_index+1) {
    case 1: // red
      output_report.lightbar_red = 64;
      break;

    case 2: // blue
      output_report.lightbar_blue = 64;
      break;

    case 3: // green
      output_report.lightbar_green = 64;
      break;

    case 4: // purple
      output_report.lightbar_red = 20; // purple
      output_report.lightbar_blue = 40;//
      break;

    case 5: // yellow
      output_report.lightbar_red = 64;
      output_report.lightbar_green = 64;
      break;

    default: // white
      output_report.lightbar_blue = 32;
      output_report.lightbar_green = 32;
      output_report.lightbar_red = 32;
      break;
  }
#elif CONFIG_PCE
  switch (player_index+1) {
    case 1: // blue
      output_report.lightbar_blue = 64;
      break;

    case 2: // red
      output_report.lightbar_red = 64;
      break;

    case 3: // green
      output_report.lightbar_green = 64;
      break;

    case 4: // purple
      output_report.lightbar_red = 20; // purple
      output_report.lightbar_blue = 40;//
      break;

    case 5: // yellow
      output_report.lightbar_red = 64;
      output_report.lightbar_green = 64;
      break;

    default: // white
      output_report.lightbar_blue = 32;
      output_report.lightbar_green = 32;
      output_report.lightbar_red = 32;
      break;
  }
#endif

  // fun
  if (player_index+1 && is_fun) {
    output_report.lightbar_red = fun_inc;
    output_report.lightbar_green = (fun_inc%2 == 0) ? fun_inc+64 : 0;
    output_report.lightbar_blue = (fun_inc%2 == 0) ? 0 : fun_inc+128;
  }

  output_report.set_rumble = 1;
  if (rumble) {
    output_report.motor_left = 192;
    output_report.motor_right = 192;
  } else {
    output_report.motor_left = 0;
    output_report.motor_right = 0;
  }

  if (ds4_devices[dev_addr].instances[instance].rumble != rumble ||
      ds4_devices[dev_addr].instances[instance].player != player_index+1 ||
      is_fun)
  {
    ds4_devices[dev_addr].instances[instance].rumble = rumble;
    ds4_devices[dev_addr].instances[instance].player = is_fun ? fun_inc : player_index+1;
    tuh_hid_send_report(dev_addr, instance, 5, &output_report, sizeof(output_report));
  }
}

// process usb hid output reports
void task_sony_ds4(uint8_t dev_addr, uint8_t instance, int player_index, uint8_t rumble, uint8_t leds) {
  const uint32_t interval_ms = 20;
  static uint32_t start_ms = 0;

  uint32_t current_time_ms = to_ms_since_boot(get_absolute_time());
  if (current_time_ms - start_ms >= interval_ms) {
    start_ms = current_time_ms;
    output_sony_ds4(dev_addr, instance, player_index, rumble);
  }
}

// resets default values in case devices are hotswapped
void unmount_sony_ds4(uint8_t dev_addr, uint8_t instance)
{
  ds4_devices[dev_addr].instances[instance].rumble = 0;
  ds4_devices[dev_addr].instances[instance].player = 0xff;
}

DeviceInterface sony_ds4_interface = {
  .name = "Sony DualShock 4",
  .is_device = is_sony_ds4,
  .process = input_sony_ds4,
  .task = task_sony_ds4,
  .unmount = unmount_sony_ds4,
};
