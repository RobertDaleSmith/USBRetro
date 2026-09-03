// 8bitdo_sn30.c
#include "8bitdo_sn30.h"
#include "core/buttons.h"
#include "core/router/router.h"
#include "core/input_event.h"

// check if device is 8BitDo SN30/SF30 Pro family in USB D-Input mode
bool is_8bitdo_sn30(uint16_t vid, uint16_t pid) {
  return (vid == 0x2dc8 && pid == 0x6001); // 8BitDo SN30 Pro / SF30 Pro (D-Input)
}

// check if 2 reports are different enough (buttons + dpad + sticks = 7 bytes)
bool diff_report_sn30(bitdo_sn30_report_t const* rpt1, bitdo_sn30_report_t const* rpt2) {
  return memcmp(rpt1, rpt2, sizeof(bitdo_sn30_report_t)) != 0;
}

// process usb hid input reports
void process_8bitdo_sn30(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
  uint32_t buttons;
  // previous report used to compare for changes
  static bitdo_sn30_report_t prev_report[MAX_DEVICES] = { 0 };

  bitdo_sn30_report_t input_report;
  memcpy(&input_report, report, sizeof(input_report));

  if (diff_report_sn30(&prev_report[dev_addr-1], &input_report)) {
    // hat: low nibble, 0x0F = released; 0=N,1=NE,2=E,3=SE,4=S,5=SW,6=W,7=NW
    uint8_t hat = input_report.dpad & 0x0F;
    bool dpad_up    = (hat == 0 || hat == 1 || hat == 7);
    bool dpad_right = (hat >= 1 && hat <= 3);
    bool dpad_down  = (hat >= 3 && hat <= 5);
    bool dpad_left  = (hat >= 5 && hat <= 7);

    // Mapping straight from the joypad-web training capture (2dc8:6001):
    // physical B/A/Y/X face buttons -> W3C B2/B1/B4/B3.
    buttons = (((dpad_up)             ? JP_BUTTON_DU : 0) |
               ((dpad_down)           ? JP_BUTTON_DD : 0) |
               ((dpad_left)           ? JP_BUTTON_DL : 0) |
               ((dpad_right)          ? JP_BUTTON_DR : 0) |
               ((input_report.a)      ? JP_BUTTON_B1 : 0) |  // A (south)
               ((input_report.b)      ? JP_BUTTON_B2 : 0) |  // B (east)
               ((input_report.x)      ? JP_BUTTON_B3 : 0) |  // X (west)
               ((input_report.y)      ? JP_BUTTON_B4 : 0) |  // Y (north)
               ((input_report.l1)     ? JP_BUTTON_L1 : 0) |
               ((input_report.r1)     ? JP_BUTTON_R1 : 0) |
               ((input_report.l2)     ? JP_BUTTON_L2 : 0) |
               ((input_report.r2)     ? JP_BUTTON_R2 : 0) |
               ((input_report.select) ? JP_BUTTON_S1 : 0) |
               ((input_report.start)  ? JP_BUTTON_S2 : 0) |
               ((input_report.l3)     ? JP_BUTTON_L3 : 0) |
               ((input_report.r3)     ? JP_BUTTON_R3 : 0) |
               ((input_report.home)   ? JP_BUTTON_A1 : 0));

    // HID convention: 0=up, 255=down (no inversion needed)
    uint8_t analog_lx = input_report.x1;
    uint8_t analog_ly = input_report.y1;
    uint8_t analog_rx = input_report.x2;
    uint8_t analog_ry = input_report.y2;

    // keep analog within range [1-255]
    ensureAllNonZero(&analog_lx, &analog_ly, &analog_rx, &analog_ry);

    input_event_t event = {
      .dev_addr = dev_addr,
      .instance = instance,
      .type = INPUT_TYPE_GAMEPAD,
      .transport = INPUT_TRANSPORT_USB,
      .layout = LAYOUT_NINTENDO_4FACE,  // SNES BAYX face style
      .buttons = buttons,
      .button_count = 11,  // 4 face + L1/R1/L2/R2 + L3/R3 + Home
      .analog = {analog_lx, analog_ly, analog_rx, analog_ry, 0, 0},
      .keys = 0,
    };
    router_submit_input(&event);

    prev_report[dev_addr-1] = input_report;
  }
}

DeviceInterface bitdo_sn30_interface = {
  .name = "8BitDo SN30/SF30 Pro (D-Input)",
  .is_device = is_8bitdo_sn30,
  .process = process_8bitdo_sn30,
  .task = NULL,
  .init = NULL
};
