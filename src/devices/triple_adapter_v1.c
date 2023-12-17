// triple_adapter_v1.c
#include "triple_adapter_v1.h"
#include "globals.h"

// check if device is TripleController (Arduino based HID)
static inline bool is_triple_adapter_v1(uint16_t vid, uint16_t pid) {
  bool serial_match = false;
  bool vidpid_match = (vid == 0x2341 && pid == 0x8036); // Arduino Leonardo

  if (!vidpid_match) return false;

  // Compare the the fetched serial with "NES-SNES-GENESIS"
  // if(memcmp(devices[dev_addr].serial, tplctr_serial_v1, sizeof(tplctr_serial_v1)) == 0)
  // {
  //   serial_match = true;
  // }

  return serial_match;
}

// check if 2 reports are different enough
bool diff_report_triple_adapter_v1(triple_adapter_v1_report_t const* rpt1, triple_adapter_v1_report_t const* rpt2) {
  bool result;

  result |= rpt1->axis_x != rpt2->axis_x;
  result |= rpt1->axis_y != rpt2->axis_y;
  result |= rpt1->b != rpt2->b;
  result |= rpt1->a != rpt2->a;
  result |= rpt1->y != rpt2->y;
  result |= rpt1->x != rpt2->x;
  result |= rpt1->l != rpt2->l;
  result |= rpt1->r != rpt2->r;
  result |= rpt1->select != rpt2->select;
  result |= rpt1->start != rpt2->start;
  result |= rpt1->home != rpt2->home;

  return result;
}

// process usb hid input reports
void process_triple_adapter_v1(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
  // previous report used to compare for changes
  static triple_adapter_v1_report_t prev_report[5][5];

  triple_adapter_v1_report_t update_report;
  memcpy(&update_report, report, sizeof(update_report));

  if (diff_report_triple_adapter_v1(&prev_report[dev_addr-1][instance], &update_report) )
  {
    printf("(x, y) = (%u, %u)\r\n", update_report.axis_x, update_report.axis_y);
    if (update_report.b) printf("B ");
    if (update_report.a) printf("A ");
    if (update_report.y) printf("Y ");
    if (update_report.x) printf("X ");
    if (update_report.l) printf("L ");
    if (update_report.r) printf("R ");
    if (update_report.select) printf("Select ");
    if (update_report.start) printf("Start ");
    printf("\r\n");

    int threshold = 28;
    bool dpad_up    = update_report.axis_y ? (update_report.axis_y > (128 - threshold)) : 0;
    bool dpad_right = update_report.axis_x ? (update_report.axis_x < (128 + threshold)) : 0;
    bool dpad_down  = update_report.axis_y ? (update_report.axis_y < (128 + threshold)) : 0;
    bool dpad_left  = update_report.axis_x ? (update_report.axis_x > (128 - threshold)) : 0;
    bool has_6btns = true;

    buttons = (((false)                ? 0x00 : 0x20000) |
               ((false)                ? 0x00 : 0x10000) |
               ((update_report.r)      ? 0x00 : 0x8000) | // VI
               ((update_report.l)      ? 0x00 : 0x4000) | // V
               ((update_report.y)      ? 0x00 : 0x2000) | // IV
               ((update_report.x)      ? 0x00 : 0x1000) | // III
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
               ((update_report.b)      ? 0x00 : 0x0020) | // II
               ((update_report.a)      ? 0x00 : 0x0010)); // I

    // add to accumulator and post to the state machine
    // if a scan from the host machine is ongoing, wait
    post_globals(dev_addr, instance, buttons, 128, 128, 128, 128, 0, 0, 0, 0);

    prev_report[dev_addr-1][instance] = update_report;
  }
}

DeviceInterface triple_adapter_v1_interface = {
  .name = "TripleController Adapter v1",
  .is_device = is_triple_adapter_v1,
  .process = process_triple_adapter_v1,
  .task = NULL,
  .init = NULL
};
