// hori_horipad.c
#include "hori_horipad.h"
#include "globals.h"

// check if device is HORIPAD for Nintendo Switch 
//                (or Sega Genesis mini controllers)
bool is_hori_horipad(uint16_t vid, uint16_t pid)
{
  return ((vid == 0x0f0d && pid == 0x00c1)); // Switch HORI HORIPAD
}

// check if 2 reports are different enough
bool diff_report_horipad(hori_horipad_report_t const* rpt1, hori_horipad_report_t const* rpt2) {
  bool result = memcmp(rpt1, rpt2, 3) != 0;

  // x, y, z, rz must different than 2 to be counted
  result |= diff_than_n(rpt1->axis_x, rpt2->axis_x, 2) ||
            diff_than_n(rpt1->axis_y, rpt2->axis_y, 2) ||
            diff_than_n(rpt1->axis_z, rpt2->axis_z, 2) ||
            diff_than_n(rpt1->axis_rz, rpt2->axis_rz, 2);

  return result;
}

// process usb hid input reports
void process_hori_horipad(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
  // previous report used to compare for changes
  static hori_horipad_report_t prev_report[5] = { 0 };

  hori_horipad_report_t input_report;
  memcpy(&input_report, report, sizeof(input_report));

  if (diff_report_horipad(&prev_report[dev_addr-1], &input_report)) {
    printf("(x, y, z, rz) = (%d, %d, %d, %d) ", input_report.axis_x, input_report.axis_y, input_report.axis_z, input_report.axis_rz);
    printf("DPad = %d ", input_report.dpad);

    if (input_report.b) printf("B ");
    if (input_report.a) printf("A ");
    if (input_report.y) printf("Y ");
    if (input_report.x) printf("X ");
    if (input_report.l1) printf("L1 ");
    if (input_report.r1) printf("R1 ");
    if (input_report.l2) printf("L2(Z) ");
    if (input_report.r2) printf("R2(C) ");
    if (input_report.l3) printf("L3 ");
    if (input_report.r3) printf("R3 ");
    if (input_report.s1) printf("Select ");
    if (input_report.s2) printf("Start ");
    if (input_report.a1) printf("Home ");
    if (input_report.a2) printf("Capture ");
    printf("\r\n");

    bool dpad_up    = (input_report.dpad == 0 || input_report.dpad == 1 || input_report.dpad == 7);
    bool dpad_right = (input_report.dpad >= 1 && input_report.dpad <= 3);
    bool dpad_down  = (input_report.dpad >= 3 && input_report.dpad <= 5);
    bool dpad_left  = (input_report.dpad >= 5 && input_report.dpad <= 7);
    bool has_6btns = true;
#ifdef CONFIG_PCE
    buttons = (((false)           ? 0x00 : 0x20000) |
               ((false)           ? 0x00 : 0x10000) |
               ((input_report.l2 || input_report.l1)? 0x00 : 0x8000) |
               ((input_report.y)  ? 0x00 : 0x4000) |
               ((input_report.x || input_report.r1) ? 0x00 : 0x2000) |
               ((input_report.a)  ? 0x00 : 0x1000) |
               ((has_6btns)       ? 0x00 : 0x0800) |
               ((input_report.a1) ? 0x00 : 0x0400) |
               ((false)           ? 0x00 : 0x0200) |
               ((false)           ? 0x00 : 0x0100) |
               ((dpad_left)       ? 0x00 : 0x08) |
               ((dpad_down)       ? 0x00 : 0x04) |
               ((dpad_right)      ? 0x00 : 0x02) |
               ((dpad_up)         ? 0x00 : 0x01) |
               ((input_report.s2) ? 0x00 : 0x80) |
               ((input_report.s1) ? 0x00 : 0x40) |
               ((input_report.b)  ? 0x00 : 0x20) |
               ((input_report.r2) ? 0x00 : 0x10));
#else
    buttons = (((input_report.r3) ? 0x00 : 0x20000) |
               ((input_report.l3) ? 0x00 : 0x10000) |
               ((input_report.r1) ? 0x00 : 0x8000) |
               ((input_report.l1) ? 0x00 : 0x4000) |
               ((input_report.y)  ? 0x00 : 0x2000) |
               ((input_report.x)  ? 0x00 : 0x1000) |
               ((has_6btns)       ? 0x00 : 0x0800) |
               ((input_report.a1) ? 0x00 : 0x0400) |
               ((false)           ? 0x00 : 0x0200) |
               ((false)           ? 0x00 : 0x0100) |
               ((dpad_left)       ? 0x00 : 0x08) |
               ((dpad_down)       ? 0x00 : 0x04) |
               ((dpad_right)      ? 0x00 : 0x02) |
               ((dpad_up)         ? 0x00 : 0x01) |
               ((input_report.s2) ? 0x00 : 0x80) |
               ((input_report.s1 || input_report.r2 || input_report.l2) ? 0x00 : 0x40) |
               ((input_report.b)  ? 0x00 : 0x20) |
               ((input_report.a)  ? 0x00 : 0x10));
#endif
    // invert vertical axis
    uint8_t axis_x = input_report.axis_x;
    uint8_t axis_y = (input_report.axis_y == 0) ? 255 : 256 - input_report.axis_y;
    uint8_t axis_z = input_report.axis_z;
    uint8_t axis_rz = (input_report.axis_rz == 0) ? 255 : 256 - input_report.axis_rz;

    // keep analog within range [1-255]
    ensureAllNonZero(&axis_x, &axis_y, &axis_z, &axis_rz);

    // add to accumulator and post to the state machine
    // if a scan from the host machine is ongoing, wait
    post_globals(dev_addr, instance, buttons, axis_x, axis_y, axis_z, axis_rz, 0, 0, 0, 0);

    prev_report[dev_addr-1] = input_report;
  }
}

DeviceInterface hori_horipad_interface = {
  .name = "HORI HORIPAD (or Genesis/MD Mini)",
  .is_device = is_hori_horipad,
  .process = process_hori_horipad,
  .task = NULL,
  .init = NULL
};
