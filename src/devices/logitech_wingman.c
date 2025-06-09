// logitech_wingman.c
#include "logitech_wingman.h"
#include "globals.h"

// check if device is Logitech WingMan Action controller
static inline bool is_logitech_wingman(uint16_t vid, uint16_t pid) {
  return ((vid == 0x046d && pid == 0xc20b)); // Logitech WingMan Action controller
}

// check if 2 reports are different enough
bool diff_report_logitech_wingman(logitech_wingman_report_t const* rpt1, logitech_wingman_report_t const* rpt2) {
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
  uint32_t buttons;
  // previous report used to compare for changes
  static logitech_wingman_report_t prev_report[5] = { 0 };

  logitech_wingman_report_t wingman_report;
  memcpy(&wingman_report, report, sizeof(wingman_report));

  if (diff_report_logitech_wingman(&prev_report[dev_addr-1], &wingman_report)) {
    TU_LOG1("(x, y, z) = (%u, %u, %u)\r\n", wingman_report.analog_x, wingman_report.analog_y, wingman_report.analog_z);
    TU_LOG1("DPad = %d ", wingman_report.dpad);
    if (wingman_report.a) TU_LOG1("A ");
    if (wingman_report.b) TU_LOG1("B ");
    if (wingman_report.c) TU_LOG1("C ");
    if (wingman_report.x) TU_LOG1("X ");
    if (wingman_report.y) TU_LOG1("Y ");
    if (wingman_report.z) TU_LOG1("Z ");
    if (wingman_report.l) TU_LOG1("L ");
    if (wingman_report.r) TU_LOG1("R ");
    if (wingman_report.mode) TU_LOG1("Mode ");
    if (wingman_report.s) TU_LOG1("S ");
    TU_LOG1("\r\n");

    uint8_t analog_x1 = (wingman_report.analog_x == 255) ? 255 : wingman_report.analog_x + 1;
    uint8_t analog_y1 = (wingman_report.analog_y == 0) ? 255 : 255 - wingman_report.analog_y;
    uint8_t analog_x2 = ~wingman_report.analog_z;
    uint8_t analog_y2 = 128;

    bool dpad_up    = (wingman_report.dpad == 0 || wingman_report.dpad == 1 || wingman_report.dpad == 7);
    bool dpad_right = ((wingman_report.dpad >= 1 && wingman_report.dpad <= 3));
    bool dpad_down  = ((wingman_report.dpad >= 3 && wingman_report.dpad <= 5));
    bool dpad_left  = ((wingman_report.dpad >= 5 && wingman_report.dpad <= 7));

#ifdef CONFIG_PCE
    buttons = (((dpad_up)          ? 0x00 : USBR_BUTTON_DU) |
               ((dpad_down)        ? 0x00 : USBR_BUTTON_DD) |
               ((dpad_left)        ? 0x00 : USBR_BUTTON_DL) |
               ((dpad_right)       ? 0x00 : USBR_BUTTON_DR) |
               ((wingman_report.b) ? 0x00 : USBR_BUTTON_B1) | // II
               ((wingman_report.c) ? 0x00 : USBR_BUTTON_B2) | // I
               ((wingman_report.x) ? 0x00 : USBR_BUTTON_B3) | // IV
               ((wingman_report.a) ? 0x00 : USBR_BUTTON_B4) | // III
               ((wingman_report.y) ? 0x00 : USBR_BUTTON_L1) | // V
               ((wingman_report.z) ? 0x00 : USBR_BUTTON_R1) | // VI
               ((0)                ? 0x00 : USBR_BUTTON_L2) |
               ((0)                ? 0x00 : USBR_BUTTON_R2) |
               ((wingman_report.r) ? 0x00 : USBR_BUTTON_S1) | // Sel
               ((wingman_report.s) ? 0x00 : USBR_BUTTON_S2) | // Run
               ((0)                ? 0x00 : USBR_BUTTON_L3) |
               ((0)                ? 0x00 : USBR_BUTTON_R3) |
               ((0)                ? 0x00 : USBR_BUTTON_A1) |
               ((1)/*has_6btns*/   ? 0x00 : 0x800));
#else
    buttons = (((dpad_up)          ? 0x00 : USBR_BUTTON_DU) |
               ((dpad_down)        ? 0x00 : USBR_BUTTON_DD) |
               ((dpad_left)        ? 0x00 : USBR_BUTTON_DL) |
               ((dpad_right)       ? 0x00 : USBR_BUTTON_DR) |
               ((wingman_report.b) ? 0x00 : USBR_BUTTON_B1) |
               ((wingman_report.a) ? 0x00 : USBR_BUTTON_B2) |
               ((wingman_report.y) ? 0x00 : USBR_BUTTON_B3) |
               ((wingman_report.x) ? 0x00 : USBR_BUTTON_B4) |
               ((wingman_report.l) ? 0x00 : USBR_BUTTON_L1) |
               ((wingman_report.r) ? 0x00 : USBR_BUTTON_R1) |
               ((0)                ? 0x00 : USBR_BUTTON_L2) |
               ((0)                ? 0x00 : USBR_BUTTON_R2) |
               ((wingman_report.s) ? 0x00 : USBR_BUTTON_S2) |
               ((wingman_report.z) ? 0x00 : USBR_BUTTON_S1) |
               ((0)                ? 0x00 : USBR_BUTTON_L3) |
               ((0)                ? 0x00 : USBR_BUTTON_R3) |
               ((0)                ? 0x00 : USBR_BUTTON_A1) |
               ((1)/*has_6btns*/   ? 0x00 : 0x800));

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
