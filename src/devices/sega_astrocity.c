// sega_astrocity.c
#include "sega_astrocity.h"
#include "globals.h"

// check if device is Sega Astro City mini controller
static inline bool is_sega_astrocity(uint16_t vid, uint16_t pid) {
  return ((vid == 0x0ca3 && (
           pid == 0x0028 || // Astro City mini joystick
           pid == 0x0027 || // Astro City mini controller
           pid == 0x0024    // 8BitDo M30 6-button controller (2.4g)
         )));
}

// check if 2 reports are different enough
bool diff_report_sega_astrocity(sega_astrocity_report_t const* rpt1, sega_astrocity_report_t const* rpt2) {
  bool result;

  result |= rpt1->x != rpt2->x;
  result |= rpt1->y != rpt2->y;
  result |= rpt1->a != rpt2->a;
  result |= rpt1->b != rpt2->b;
  result |= rpt1->c != rpt2->c;
  result |= rpt1->d != rpt2->d;
  result |= rpt1->e != rpt2->e;
  result |= rpt1->f != rpt2->f;
  result |= rpt1->l != rpt2->l;
  result |= rpt1->r != rpt2->r;
  result |= rpt1->credit != rpt2->credit;
  result |= rpt1->start != rpt2->start;

  return result;
}

// process usb hid input reports
void process_sega_astrocity(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
  uint32_t buttons;
  // previous report used to compare for changes
  static sega_astrocity_report_t prev_report[5] = { 0 };

  sega_astrocity_report_t astro_report;
  memcpy(&astro_report, report, sizeof(astro_report));

  if (diff_report_sega_astrocity(&prev_report[dev_addr-1], &astro_report)) {
    TU_LOG1("DPad = x:%d, y:%d ", astro_report.x, astro_report.y);
    if (astro_report.a) TU_LOG1("A "); // X   <-M30 buttons
    if (astro_report.b) TU_LOG1("B "); // Y
    if (astro_report.c) TU_LOG1("C "); // Z
    if (astro_report.d) TU_LOG1("D "); // A
    if (astro_report.e) TU_LOG1("E "); // B
    if (astro_report.f) TU_LOG1("F "); // C
    if (astro_report.l) TU_LOG1("L ");
    if (astro_report.r) TU_LOG1("R ");
    if (astro_report.credit) TU_LOG1("Credit "); // Select
    if (astro_report.start) TU_LOG1("Start ");
    TU_LOG1("\r\n");

    bool dpad_up    = (astro_report.y < 127);
    bool dpad_right = (astro_report.x > 127);
    bool dpad_down  = (astro_report.y > 127);
    bool dpad_left  = (astro_report.x < 127);

#ifdef CONFIG_PCE
    buttons = (((dpad_up)             ? 0x00 : USBR_BUTTON_DU) |
               ((dpad_down)           ? 0x00 : USBR_BUTTON_DD) |
               ((dpad_left)           ? 0x00 : USBR_BUTTON_DL) |
               ((dpad_right)          ? 0x00 : USBR_BUTTON_DR) |
               ((astro_report.e)      ? 0x00 : USBR_BUTTON_B1) |
               ((astro_report.f)      ? 0x00 : USBR_BUTTON_B2) |
               ((astro_report.a)      ? 0x00 : USBR_BUTTON_B3) |
               ((astro_report.d)      ? 0x00 : USBR_BUTTON_B4) |
               ((astro_report.b)      ? 0x00 : USBR_BUTTON_L1) |
               ((astro_report.c)      ? 0x00 : USBR_BUTTON_R1) |
               ((astro_report.l)      ? 0x00 : USBR_BUTTON_L2) |
               ((astro_report.r)      ? 0x00 : USBR_BUTTON_R2) |
               ((astro_report.credit) ? 0x00 : USBR_BUTTON_S1) |
               ((astro_report.start)  ? 0x00 : USBR_BUTTON_S2) |
               ((0)                   ? 0x00 : USBR_BUTTON_L3) |
               ((0)                   ? 0x00 : USBR_BUTTON_R3) |
               ((0)                   ? 0x00 : USBR_BUTTON_A1) |
               ((1)/*has_6btns*/      ? 0x00 : 0x800));
#else
    buttons = (((dpad_up)             ? 0x00 : USBR_BUTTON_DU) |
               ((dpad_down)           ? 0x00 : USBR_BUTTON_DD) |
               ((dpad_left)           ? 0x00 : USBR_BUTTON_DL) |
               ((dpad_right)          ? 0x00 : USBR_BUTTON_DR) |
               ((astro_report.d)      ? 0x00 : USBR_BUTTON_B1) |
               ((astro_report.e)      ? 0x00 : USBR_BUTTON_B2) |
               ((astro_report.a)      ? 0x00 : USBR_BUTTON_B3) |
               ((astro_report.b)      ? 0x00 : USBR_BUTTON_B4) |
               ((astro_report.c)      ? 0x00 : USBR_BUTTON_L1) |
               ((astro_report.f)      ? 0x00 : USBR_BUTTON_R1) |
               ((astro_report.l)      ? 0x00 : USBR_BUTTON_L2) |
               ((astro_report.r)      ? 0x00 : USBR_BUTTON_R2) |
               ((astro_report.credit) ? 0x00 : USBR_BUTTON_S1) |
               ((astro_report.start)  ? 0x00 : USBR_BUTTON_S2) |
               ((0)                   ? 0x00 : USBR_BUTTON_L3) |
               ((0)                   ? 0x00 : USBR_BUTTON_R3) |
               ((0)                   ? 0x00 : USBR_BUTTON_A1) |
               ((1)/*has_6btns*/      ? 0x00 : 0x800));
#endif

    // add to accumulator and post to the state machine
    // if a scan from the host machine is ongoing, wait
    post_globals(dev_addr, instance, buttons, 128, 128, 128, 128, 0, 0, 0, 0);

    prev_report[dev_addr-1] = astro_report;
  }
}

DeviceInterface sega_astrocity_interface = {
  .name = "Sega Astro City Mini",
  .is_device = is_sega_astrocity,
  .process = process_sega_astrocity,
  .task = NULL,
  .init = NULL
};
