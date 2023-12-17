// dragonrise.c
#include "dragonrise.h"
#include "globals.h"

// check if device is generic NES USB Controller
static inline bool is_dragonrise(uint16_t vid, uint16_t pid) {
  return ((vid == 0x0079 && pid == 0x0011)); // Generic NES USB
}

// check if 2 reports are different enough
bool diff_report_pokken(dragonrise_report_t const* rpt1, dragonrise_report_t const* rpt2)
{
  bool result;

  // x, y, z, rz must different than 2 to be counted
  result = diff_than_n(rpt1->axis0_x, rpt2->axis0_x, 2) || diff_than_n(rpt1->axis0_y, rpt2->axis0_y, 2) ||
           diff_than_n(rpt1->axis1_x, rpt2->axis1_x, 2) || diff_than_n(rpt1->axis1_y, rpt2->axis1_y, 2);

  // check the rest with mem compare
  result |= memcmp(&rpt1->axis0_y + 1, &rpt2->axis0_y + 1, 2);

  return result;
}

// process usb hid input reports
void process_dragonrise(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
  // previous report used to compare for changes
  static dragonrise_report_t prev_report[5][5];

  dragonrise_report_t update_report;
  memcpy(&update_report, report, sizeof(update_report));

  if ( dragonrise_diff_report(&prev_report[dev_addr-1][instance], &update_report) )
  {
    TU_LOG1("(x1, y1, x2, y2) = (%u, %u, %u, %u)\r\n", update_report.axis0_x, update_report.axis0_y, update_report.axis1_x, update_report.axis1_y);
    // Y,X,L,R extra button data may or may not be used by similiar generic controller variants
    if (update_report.y) TU_LOG1("Y ");
    if (update_report.b) TU_LOG1("B ");
    if (update_report.a) TU_LOG1("A ");
    if (update_report.x) TU_LOG1("X ");
    if (update_report.l) TU_LOG1("L ");
    if (update_report.r) TU_LOG1("R ");
    if (update_report.z) TU_LOG1("Z ");
    if (update_report.c) TU_LOG1("C ");
    if (update_report.select) TU_LOG1("Select ");
    if (update_report.start) TU_LOG1("Start ");
    TU_LOG1("\r\n");

    bool dpad_left  = (update_report.axis0_x < 126);
    bool dpad_right  = (update_report.axis0_x > 128);
    bool dpad_up  = (update_report.axis0_y < 126);
    bool dpad_down  = (update_report.axis0_y > 128);
    bool has_6btns = true;

    buttons = (((false)                ? 0x00 : 0x20000) |
               ((false)                ? 0x00 : 0x10000) |
               ((update_report.z)      ? 0x00 : 0x8000) | // VI
               ((update_report.y)      ? 0x00 : 0x4000) | // V
               ((update_report.x)      ? 0x00 : 0x2000) | // IV
               ((update_report.a)      ? 0x00 : 0x1000) | // III
               ((has_6btns)            ? 0x00 : 0x0800) |
               ((false)                ? 0x00 : 0x0400) | // home
               ((false)                ? 0x00 : 0x0200) | // r2
               ((false)                ? 0x00 : 0x0100) | // l2
               ((dpad_left)            ? 0x00 : 0x0008) |
               ((dpad_down)            ? 0x00 : 0x0004) |
               ((dpad_right)           ? 0x00 : 0x0002) |
               ((dpad_up)              ? 0x00 : 0x0001) |
               ((update_report.start)  ? 0x00 : 0x0080) | // Run
               ((update_report.select) ? 0x00 : 0x0040) | // Select
               ((update_report.b || update_report.l) ? 0x00 : 0x0020) | // II
               ((update_report.c || update_report.r) ? 0x00 : 0x0010)); // I

    // invert vertical axis
    uint8_t axis_1x = update_report.axis0_x;
    uint8_t axis_1y = (update_report.axis0_y == 0) ? 255 : 256 - update_report.axis0_y;
    uint8_t axis_2x = update_report.axis1_x;
    uint8_t axis_2y = (update_report.axis1_y == 0) ? 255 : 256 - update_report.axis1_y;

    // keep analog within range [1-255]
    ensureAllNonZero(&axis_1x, &axis_1y, &axis_2x, &axis_2y);

    // add to accumulator and post to the state machine
    // if a scan from the host machine is ongoing, wait
    post_globals(dev_addr, instance, buttons, axis_1x, axis_1y, axis_2x, axis_2y, 0, 0, 0, 0);

    prev_report[dev_addr-1][instance] = update_report;
  }
}

DeviceInterface dragonrise_interface = {
  .name = "DragonRise Generic",
  .is_device = is_dragonrise,
  .process = process_dragonrise,
  .task = NULL,
  .init = NULL
};
