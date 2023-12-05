// sony_ds3.c
#include "sony_ds3.h"
#include "globals.h"

// check if device is Sony PlayStation 3 controllers
bool is_sony_ds3(uint16_t vid, uint16_t pid) {
  return ((vid == 0x054c && pid == 0x0268)); // Sony DualShock3
}

// check if 2 reports are different enough
bool diff_report_ds3(sony_ds3_report_t const* rpt1, sony_ds3_report_t const* rpt2)
{
  bool result;

  // x, y, z, rz must different than 2 to be counted
  result = diff_than_n(rpt1->lx, rpt2->lx, 2) || diff_than_n(rpt1->ly, rpt2->ly, 2) ||
           diff_than_n(rpt1->rx, rpt2->rx, 2) || diff_than_n(rpt1->ry, rpt2->ry, 2);

  // check the rest with mem compare
  result |= memcmp(&rpt1->reportId + 1, &rpt2->reportId + 1, 3);

  return result;
}

// process usb hid input reports
void process_sony_ds3(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
  // previous report used to compare for changes
  static sony_ds3_report_t prev_report[5] = { 0 };

  uint8_t const report_id = report[0];
  report++;
  len--;

  // all buttons state is stored in ID 1
  if (report_id == 1)
  {
    sony_ds3_report_t ds3_report;
    memcpy(&ds3_report, report, sizeof(ds3_report));

    // counter is +1, assign to make it easier to compare 2 report
    prev_report[dev_addr-1].counter = ds3_report.counter;

    // only print if changes since it is polled ~ 5ms
    // Since count+1 after each report and  x, y, z, rz fluctuate within 1 or 2
    // We need more than memcmp to check if report is different enough
    if ( diff_report_ds3(&prev_report[dev_addr-1], &ds3_report) )
    {
      //TODO: parse left and right analog trigger pressure values
      printf("(lx, ly, rx, ry) = (%u, %u, %u, %u)\r\n", ds3_report.lx, ds3_report.ly, ds3_report.rx, ds3_report.ry);
      printf("DPad = ");

      if (ds3_report.up       ) printf("Up ");
      if (ds3_report.down     ) printf("Down ");
      if (ds3_report.left     ) printf("Left ");
      if (ds3_report.right    ) printf("Right ");

      if (ds3_report.square   ) printf("Square ");
      if (ds3_report.cross    ) printf("Cross ");
      if (ds3_report.circle   ) printf("Circle ");
      if (ds3_report.triangle ) printf("Triangle ");

      if (ds3_report.l1       ) printf("L1 ");
      if (ds3_report.r1       ) printf("R1 ");
      if (ds3_report.l2       ) printf("L2 ");
      if (ds3_report.r2       ) printf("R2 ");

      if (ds3_report.select   ) printf("Select ");
      if (ds3_report.start    ) printf("Start ");
      if (ds3_report.l3       ) printf("L3 ");
      if (ds3_report.r3       ) printf("R3 ");

      if (ds3_report.ps       ) printf("PS ");

      printf("\r\n");

      bool has_6btns = true;

      buttons = (((ds3_report.r3)       ? 0x00 : 0x20000) |
                 ((ds3_report.l3)       ? 0x00 : 0x10000) |
                 ((ds3_report.r1)       ? 0x00 : 0x08000) |
                 ((ds3_report.l1)       ? 0x00 : 0x04000) |
                 ((ds3_report.square)   ? 0x00 : 0x02000) |
                 ((ds3_report.triangle) ? 0x00 : 0x01000) |
                 ((has_6btns)           ? 0x00 : 0x00800) |
                 ((ds3_report.ps)       ? 0x00 : 0x00400) |
                 ((ds3_report.r2)       ? 0x00 : 0x00200) |
                 ((ds3_report.l2)       ? 0x00 : 0x00100) |
                 ((ds3_report.left)     ? 0x00 : 0x00008) |
                 ((ds3_report.down)     ? 0x00 : 0x00004) |
                 ((ds3_report.right)    ? 0x00 : 0x00002) |
                 ((ds3_report.up)       ? 0x00 : 0x00001) |
                 ((ds3_report.start)    ? 0x00 : 0x00080) |
                 ((ds3_report.select)   ? 0x00 : 0x00040) |
                 ((ds3_report.cross)    ? 0x00 : 0x00020) |
                 ((ds3_report.circle)   ? 0x00 : 0x00010));

      uint8_t analog_1x = ds3_report.lx;
      uint8_t analog_1y = 255 - ds3_report.ly;
      uint8_t analog_2x = ds3_report.rx;
      uint8_t analog_2y = 255 - ds3_report.ry;

      // keep analog within range [1-255]
      ensureAllNonZero(&analog_1x, &analog_1y, &analog_2x, &analog_2y);

      // add to accumulator and post to the state machine
      // if a scan from the host machine is ongoing, wait
      post_globals(dev_addr, instance, buttons, analog_1x, analog_1y, analog_2x, analog_2y, 0, 0, 0, 0);

      prev_report[dev_addr-1] = ds3_report;
    }
  }
}

// process usb hid output reports
void task_sony_ds3(uint8_t dev_addr, uint8_t instance, uint8_t player_index, uint8_t rumble) {
  sony_ds3_output_report_01_t output_report = {
    .buf = {
      0x01,
      0x00, 0xff, 0x00, 0xff, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00,
      0xff, 0x27, 0x10, 0x00, 0x32,
      0xff, 0x27, 0x10, 0x00, 0x32,
      0xff, 0x27, 0x10, 0x00, 0x32,
      0xff, 0x27, 0x10, 0x00, 0x32,
      0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    }
  };
  static uint8_t last_rumble = 0;

  // led player indicator
  switch (player_index+1)
  {
  case 1:
  case 2:
  case 3:
  case 4:
  case 5:
    output_report.data.leds_bitmap = (PLAYER_LEDS[player_index+1] << 1);
    break;

  default: // unassigned
    // turn all leds on
    output_report.data.leds_bitmap = (PLAYER_LEDS[10] << 1);

    // make all leds dim
    for (int n = 0; n < 4; n++) {
      output_report.data.led[n].duty_length = 0;
      output_report.data.led[n].duty_on = 32;
      output_report.data.led[n].duty_off = 223;
    }
    break;
  }

  // fun
  if (player_index+1 && is_fun) {
    output_report.data.leds_bitmap = (fun_inc & 0b00011110);

    // led brightness
    for (int n = 0; n < 4; n++) {
      output_report.data.led[n].duty_length = (fun_inc & 0x07);
      output_report.data.led[n].duty_on = fun_inc;
      output_report.data.led[n].duty_off = 255 - fun_inc;
    }
  }

  if (rumble != last_rumble) {
    if (rumble) {
      output_report.data.rumble.right_motor_on = 1;
      output_report.data.rumble.left_motor_force = 128;
      output_report.data.rumble.left_duration = 128;
      output_report.data.rumble.right_duration = 128;
    }
    last_rumble = rumble;
  }

  // Send report without the report ID, start at index 1 instead of 0
  tuh_hid_send_report(dev_addr, instance, output_report.data.report_id, &(output_report.buf[1]), sizeof(output_report) - 1);
}

// initialize usb hid input
bool init_sony_ds3(uint8_t dev_addr, uint8_t instance) {
  /*
  * The Sony Sixaxis does not handle HID Output Reports on the
  * Interrupt EP like it could, so we need to force HID Output
  * Reports to use tuh_hid_set_report on the Control EP.
  *
  * There is also another issue about HID Output Reports via USB,
  * the Sixaxis does not want the report_id as part of the data
  * packet, so we have to discard buf[0] when sending the actual
  * control message, even for numbered reports, humpf!
  */
  printf("PS3 Init..\n");

  uint8_t cmd_buf[4];
  cmd_buf[0] = 0x42; // Special PS3 Controller enable commands
  cmd_buf[1] = 0x0c;
  cmd_buf[2] = 0x00;
  cmd_buf[3] = 0x00;

  // Send a Set Report request to the control endpoint
  return tuh_hid_set_report(dev_addr, instance, 0xF4, HID_REPORT_TYPE_FEATURE, &(cmd_buf), sizeof(cmd_buf));
}

DeviceInterface sony_ds3_interface = {
  .name = "Sony DualShock 3",
  .is_device = is_sony_ds3,
  .process = process_sony_ds3,
  .task = task_sony_ds3,
  .init = init_sony_ds3
};
