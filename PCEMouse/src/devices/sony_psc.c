// sony_psc.c
#include "sony_psc.h"
#include "globals.h"

// check if device is PlayStation Classic controller
bool is_sony_psc(uint16_t vid, uint16_t pid)
{
  return ((vid == 0x054c && pid == 0x0cda)); // Sony PSClassic
}

// check if 2 reports are different enough
bool diff_report_psc(sony_psc_report_t const* rpt1, sony_psc_report_t const* rpt2)
{
    // Compare the first 2 bytes of the reports
    return memcmp(rpt1, rpt2, sizeof(sony_psc_report_t)-1) != 0;
}


// process usb hid input reports
void process_sony_psc(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
  // previous report used to compare for changes
  static sony_psc_report_t prev_report[5] = { 0 };

  sony_psc_report_t psc_report;
  memcpy(&psc_report, report, sizeof(psc_report));

  // counter is +1, assign to make it easier to compare 2 report
  prev_report[dev_addr-1].counter = psc_report.counter;

  if (diff_report_psc(&prev_report[dev_addr-1], &psc_report)) {
    printf("DPad = %d ", psc_report.dpad);

    if (psc_report.square   ) printf("Square ");
    if (psc_report.cross    ) printf("Cross ");
    if (psc_report.circle   ) printf("Circle ");
    if (psc_report.triangle ) printf("Triangle ");
    if (psc_report.l1       ) printf("L1 ");
    if (psc_report.r1       ) printf("R1 ");
    if (psc_report.l2       ) printf("L2 ");
    if (psc_report.r2       ) printf("R2 ");
    if (psc_report.share    ) printf("Share ");
    if (psc_report.option   ) printf("Option ");
    if (psc_report.ps       ) printf("PS ");

    printf("\r\n");

    bool dpad_up    = (psc_report.dpad >= 0 && psc_report.dpad <= 2);
    bool dpad_right = (psc_report.dpad == 2 || psc_report.dpad == 6 || psc_report.dpad == 10);
    bool dpad_down  = (psc_report.dpad >= 8 && psc_report.dpad <= 10);
    bool dpad_left  = (psc_report.dpad == 0 || psc_report.dpad == 4 || psc_report.dpad == 8);
    bool has_6btns = true;

    buttons = (((false)               ? 0x00 : 0x20000) |
               ((false)               ? 0x00 : 0x10000) |
               ((psc_report.r1)       ? 0x00 : 0x08000) |
               ((psc_report.l1)       ? 0x00 : 0x4000) |
               ((psc_report.square)   ? 0x00 : 0x2000) |
               ((psc_report.triangle) ? 0x00 : 0x1000) |
               ((has_6btns)           ? 0x00 : 0x0800) |
               ((psc_report.ps)       ? 0x00 : 0x0400) |
               ((psc_report.r2)       ? 0x00 : 0x0200) |
               ((psc_report.l2)       ? 0x00 : 0x0100) |
               ((dpad_left)           ? 0x00 : 0x08) |
               ((dpad_down)           ? 0x00 : 0x04) |
               ((dpad_right)          ? 0x00 : 0x02) |
               ((dpad_up)             ? 0x00 : 0x01) |
               ((psc_report.option)   ? 0x00 : 0x80) |
               ((psc_report.share)    ? 0x00 : 0x40) |
               ((psc_report.cross)    ? 0x00 : 0x20) |
               ((psc_report.circle)   ? 0x00 : 0x10));

    // add to accumulator and post to the state machine
    // if a scan from the host machine is ongoing, wait
    post_globals(dev_addr, instance, buttons, 128, 128, 128, 128, 0, 0, 0, 0);

    prev_report[dev_addr-1] = psc_report;
  }
}

DeviceInterface sony_psc_interface = {
  .name = "Sony PlayStation Classic",
  .is_device = is_sony_psc,
  .process = process_sony_psc,
  .task = NULL,
  .init = NULL
};
