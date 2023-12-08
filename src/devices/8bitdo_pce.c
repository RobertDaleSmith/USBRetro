// 8bitdo_pce.c
#include "8bitdo_pce.h"
#include "globals.h"

// check if device is 8BitDo PCE 2.4g controllers
bool is_8bitdo_pce(uint16_t vid, uint16_t pid) {
  return ((vid == 0x0f0d && pid == 0x0138)); // 8BitDo PCE 2.4g
}

// check if 2 reports are different enough
bool diff_report_pce(bitdo_pce_report_t const* rpt1, bitdo_pce_report_t const* rpt2) {
  bool result;

  // x1, y1, x2, y2 must different than 2 to be counted
  result = diff_than_n(rpt1->x1, rpt2->x1, 2) || diff_than_n(rpt1->y1, rpt2->y1, 2) ||
           diff_than_n(rpt1->x2, rpt2->x2, 2) || diff_than_n(rpt1->y2, rpt2->y2, 2);

  // Compare the first 3 bytes of the reports
  result |= memcmp(rpt1, rpt2, 3) != 0;

  return result;
}

// process usb hid input reports
void process_8bitdo_pce(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
  // previous report used to compare for changes
  static bitdo_pce_report_t prev_report[5] = { 0 };

  bitdo_pce_report_t pce_report;
  memcpy(&pce_report, report, sizeof(pce_report));

  if (diff_report_pce(&prev_report[dev_addr-1], &pce_report)) {
    printf("(x1, y1, x2, y2) = (%u, %u, %u, %u)\r\n", pce_report.x1, pce_report.y1, pce_report.x2, pce_report.y2);
    printf("DPad = %d ", pce_report.dpad);

    if (pce_report.sel) printf("Select ");
    if (pce_report.run) printf("Run ");
    if (pce_report.one) printf("I ");
    if (pce_report.two) printf("II ");

    printf("\r\n");

    bool dpad_up    = (pce_report.dpad == 0 || pce_report.dpad == 1 || pce_report.dpad == 7);
    bool dpad_right = (pce_report.dpad >= 1 && pce_report.dpad <= 3);
    bool dpad_down  = (pce_report.dpad >= 3 && pce_report.dpad <= 5);
    bool dpad_left  = (pce_report.dpad >= 5 && pce_report.dpad <= 7);
    bool has_6btns = false;

    buttons = (((false)          ? 0x00 : 0x20000) |
               ((false)          ? 0x00 : 0x10000) |
               ((false)          ? 0x00 : 0x08000) |
               ((false)          ? 0x00 : 0x4000) |
               ((false)          ? 0x00 : 0x2000) |
               ((false)          ? 0x00 : 0x1000) |
               ((has_6btns)      ? 0x00 : 0x0800) |
               ((false)          ? 0x00 : 0x0400) | // home
               ((false)          ? 0x00 : 0x0200) | // r2
               ((false)          ? 0x00 : 0x0100) | // l2
               ((dpad_left)      ? 0x00 : 0x08) |
               ((dpad_down)      ? 0x00 : 0x04) |
               ((dpad_right)     ? 0x00 : 0x02) |
               ((dpad_up)        ? 0x00 : 0x01) |
               ((pce_report.run) ? 0x00 : 0x80) |
               ((pce_report.sel) ? 0x00 : 0x40) |
               ((pce_report.two) ? 0x00 : 0x20) |
               ((pce_report.one) ? 0x00 : 0x10));

    // add to accumulator and post to the state machine
    // if a scan from the host machine is ongoing, wait
    post_globals(dev_addr, instance, buttons, 128, 128, 128, 128, 0, 0, 0, 0);

    prev_report[dev_addr-1] = pce_report;
  }
}

DeviceInterface bitdo_pce_interface = {
  .name = "8BitDo PCE 2.4g",
  .is_device = is_8bitdo_pce,
  .process = process_8bitdo_pce,
  .task = NULL,
  .init = NULL
};
