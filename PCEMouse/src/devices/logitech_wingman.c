// logitech_wingman.c
#include "logitech_wingman.h"
#include "globals.h"

// check if device is Logitech WingMan Action controller
static inline bool is_logitech_wingman(uint16_t vid, uint16_t pid)
{
  return ((vid == 0x046d && pid == 0xc20b)); // Logitech WingMan Action controller
}

// check if 2 reports are different enough
bool wingman_diff_report(logitech_wingman_report_t const* rpt1, logitech_wingman_report_t const* rpt2)
{
  bool result;

  result |= rpt1->analog_x != rpt2->analog_x;
  result |= rpt1->analog_y != rpt2->analog_y;
  result |= rpt1->analog_z != rpt2->analog_z;
  result |= rpt1->dpad != rpt2->dpad;
  result |= rpt1->a != rpt2->a;
  result |= rpt1->b != rpt2->b;
  result |= rpt1->c != rpt2->c;
  result |= rpt1->x != rpt2->x;
  result |= rpt1->y != rpt2->y;
  result |= rpt1->z != rpt2->z;
  result |= rpt1->l != rpt2->l;
  result |= rpt1->r != rpt2->r;
  result |= rpt1->mode != rpt2->mode;
  result |= rpt1->s != rpt2->s;

  return result;
}

// process usb hid input reports
void process_logitech_wingman(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
  // previous report used to compare for changes
  static logitech_wingman_report_t prev_report[5] = { 0 };

  logitech_wingman_report_t wingman_report;
  memcpy(&wingman_report, report, sizeof(wingman_report));

  if ( wingman_diff_report(&prev_report[dev_addr-1], &wingman_report) )
  {
    // printf("(x, y, z) = (%u, %u, %u)\r\n", wingman_report.analog_x, wingman_report.analog_y, wingman_report.analog_z);
    // printf("DPad = %d ", wingman_report.dpad);
    // if (wingman_report.a) printf("A ");
    // if (wingman_report.b) printf("B ");
    // if (wingman_report.c) printf("C ");
    // if (wingman_report.x) printf("X ");
    // if (wingman_report.y) printf("Y ");
    // if (wingman_report.z) printf("Z ");
    // if (wingman_report.l) printf("L ");
    // if (wingman_report.r) printf("R ");
    // if (wingman_report.mode) printf("Mode ");
    // if (wingman_report.s) printf("S ");
    // printf("\r\n");

    uint8_t analog_x1 = (wingman_report.analog_x == 255) ? 255 : wingman_report.analog_x + 1;
    uint8_t analog_y1 = (wingman_report.analog_y == 0) ? 255 : 255 - wingman_report.analog_y;
    uint8_t analog_x2 = ~wingman_report.analog_z;
    uint8_t analog_y2 = 128;

    bool dpad_up    = (wingman_report.dpad == 0 || wingman_report.dpad == 1 || wingman_report.dpad == 7);
    bool dpad_right = ((wingman_report.dpad >= 1 && wingman_report.dpad <= 3));
    bool dpad_down  = ((wingman_report.dpad >= 3 && wingman_report.dpad <= 5));
    bool dpad_left  = ((wingman_report.dpad >= 5 && wingman_report.dpad <= 7));
    bool has_6btns = true;

#ifdef CONFIG_PCE
    buttons = (((false)            ? 0x00 : 0x20000) |
               ((false)            ? 0x00 : 0x10000) |
               ((wingman_report.z) ? 0x00 : 0x8000) |  // VI
               ((wingman_report.y) ? 0x00 : 0x4000) |  // V
               ((wingman_report.x) ? 0x00 : 0x2000) |  // IV
               ((wingman_report.a) ? 0x00 : 0x1000) |  // III
               ((has_6btns)        ? 0x00 : 0x0800) |
               ((false)            ? 0x00 : 0x0400) | // home
               ((false)            ? 0x00 : 0x0200) | // r2
               ((false)            ? 0x00 : 0x0100) | // l2
               ((dpad_left)        ? 0x00 : 0x08) |
               ((dpad_down)        ? 0x00 : 0x04) |
               ((dpad_right)       ? 0x00 : 0x02) |
               ((dpad_up)          ? 0x00 : 0x01) |
               ((wingman_report.s) ? 0x00 : 0x80) |  // Run
               ((wingman_report.r) ? 0x00 : 0x40) |  // Select
               ((wingman_report.b) ? 0x00 : 0x20) |  // II
               ((wingman_report.c) ? 0x00 : 0x10));  // I
#else
    buttons = (((false)            ? 0x00 : 0x20000) |
               ((false)            ? 0x00 : 0x10000) |
               ((wingman_report.r) ? 0x00 : 0x8000) |  // R
               ((wingman_report.l) ? 0x00 : 0x4000) |  // L
               ((wingman_report.y) ? 0x00 : 0x2000) |  // Y
               ((wingman_report.x) ? 0x00 : 0x1000) |  // X
               ((has_6btns)        ? 0x00 : 0x0800) |
               ((false)            ? 0x00 : 0x0400) | // home
               ((false)            ? 0x00 : 0x0200) | // r2
               ((false)            ? 0x00 : 0x0100) | // l2
               ((dpad_left)        ? 0x00 : 0x08) |
               ((dpad_down)        ? 0x00 : 0x04) |
               ((dpad_right)       ? 0x00 : 0x02) |
               ((dpad_up)          ? 0x00 : 0x01) |
               ((wingman_report.s) ? 0x00 : 0x80) |  // Start
               ((wingman_report.z) ? 0x00 : 0x40) |  // Z
               ((wingman_report.b) ? 0x00 : 0x20) |  // B
               ((wingman_report.a) ? 0x00 : 0x10));  // A

    // C button hold swaps slider axis from horizontal to vertical
    if (wingman_report.c) {
        analog_x2 = 128;
        analog_y2 = wingman_report.analog_z;
    }
#endif

    // keep analog within range [1-255]
    ensureAllNonZero(&analog_x1, &analog_y1, &analog_x2, &analog_y2);

    // add to accumulator and post to the state machine
    // if a scan from the host machine is ongoing, wait
    post_globals(dev_addr, instance, buttons, analog_x1, analog_y1, analog_x2, analog_y2, 0, 0, 0, 0);

    prev_report[dev_addr-1] = wingman_report;
  }
}

DeviceInterface logitech_wingman_interface = {
  .name = "Logitech WingMan Action",
  .is_device = is_logitech_wingman,
  .process = process_logitech_wingman,
  .task = NULL,
  .init = NULL
};
