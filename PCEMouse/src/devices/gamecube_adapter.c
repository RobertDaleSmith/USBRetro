// gamecube_adapter.c
#include "gamecube_adapter.h"
#include "globals.h"

// check if device is GameCube Adapter for WiiU/Switch
static inline bool is_gamecube_adapter(uint16_t vid, uint16_t pid) {
  return (vid == 0x057e && pid == 0x0337); // GameCube Adapter
}

// check if 2 reports are different enough
bool diff_report_gamecube_adapter(gamecube_adapter_report_t const* rpt1, gamecube_adapter_report_t const* rpt2, uint8_t player) {
  bool result;

  // x, y must different than 2 to be counted
  result = diff_than_n(rpt1->port[player].x1, rpt2->port[player].x1, 2) || diff_than_n(rpt1->port[player].y1, rpt2->port[player].y1, 2) ||
           diff_than_n(rpt1->port[player].x2, rpt2->port[player].x2, 2) || diff_than_n(rpt1->port[player].y2, rpt2->port[player].y2, 2) ||
           diff_than_n(rpt1->port[player].zl, rpt2->port[player].zl, 2) || diff_than_n(rpt1->port[player].zr, rpt2->port[player].zr, 2);

  // check the all with mem compare (after report_id players are spaced 9 bytes apart)
  result |= memcmp(&rpt1->report_id + 1 + (player*9), &rpt2->report_id + 1 + (player*9), 3);

  return result;
}

// process usb hid input reports
void process_gamecube_adapter(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
  // previous report used to compare for changes
  static gamecube_adapter_report_t prev_report[5][4];

  gamecube_adapter_report_t gamecube_report;
  memcpy(&gamecube_report, report, sizeof(gamecube_report));

  if (gamecube_report.report_id == 0x21) { // GameCube Controller Report
    for(int i = 0; i < 4; i++) {
      if (gamecube_report.port[i].connected) {
        if (diff_report_gamecube_adapter(&prev_report[dev_addr-1][instance + i], &gamecube_report, i)) {
          printf("GAMECUBE[%d|%d]: Report ID = 0x%x\r\n", dev_addr, (instance + i), gamecube_report.report_id);
          printf("(x, y, cx, cy, zl, zr) = (%u, %u, %u, %u, %u, %u)\r\n",
            gamecube_report.port[i].x1,
            gamecube_report.port[i].y1,
            gamecube_report.port[i].x2,
            gamecube_report.port[i].y2,
            gamecube_report.port[i].zl,
            gamecube_report.port[i].zr);
          printf("DPad = ");

          if (gamecube_report.port[i].down) printf("Down ");
          if (gamecube_report.port[i].up) printf("Up ");
          if (gamecube_report.port[i].right) printf("Right ");
          if (gamecube_report.port[i].left) printf("Left ");
          if (gamecube_report.port[i].a) printf("A ");
          if (gamecube_report.port[i].b) printf("B ");
          if (gamecube_report.port[i].x) printf("X ");
          if (gamecube_report.port[i].y) printf("Y ");
          if (gamecube_report.port[i].z) printf("Z ");
          if (gamecube_report.port[i].l) printf("L ");
          if (gamecube_report.port[i].r) printf("R ");
          if (gamecube_report.port[i].start) printf("Start ");
          printf("\n");

          bool dpad_left  = gamecube_report.port[i].left;
          bool dpad_right = gamecube_report.port[i].right;
          bool dpad_up    = gamecube_report.port[i].up;
          bool dpad_down  = gamecube_report.port[i].down;
          bool has_6btns  = true;

          buttons = (
            ((false)                         ? 0x00 : 0x20000) |
            ((false)                         ? 0x00 : 0x10000) |
            ((gamecube_report.port[i].r)     ? 0x00 : 0x8000) | // VI
            ((gamecube_report.port[i].l)     ? 0x00 : 0x4000) | // V
            ((gamecube_report.port[i].y)     ? 0x00 : 0x2000) | // IV
            ((gamecube_report.port[i].x)     ? 0x00 : 0x1000) | // III
            ((has_6btns)                     ? 0x00 : 0x0800) |
            ((false)                         ? 0x00 : 0x0400) | // home
            ((false)                         ? 0x00 : 0x0200) | // r2
            ((false)                         ? 0x00 : 0x0100) | // l2
            ((dpad_left)                     ? 0x00 : 0x0008) |
            ((dpad_down)                     ? 0x00 : 0x0004) |
            ((dpad_right)                    ? 0x00 : 0x0002) |
            ((dpad_up)                       ? 0x00 : 0x0001) |
            ((gamecube_report.port[i].start) ? 0x00 : 0x0080) | // Run
            ((gamecube_report.port[i].z)     ? 0x00 : 0x0040) | // Select
            ((gamecube_report.port[i].b)     ? 0x00 : 0x0020) | // II
            ((gamecube_report.port[i].a)     ? 0x00 : 0x0010)   // I
          );

          uint8_t zl_axis = gamecube_report.port[i].zl;
          zl_axis = zl_axis > 38 ? zl_axis - 38 : 0;
          uint8_t zr_axis = gamecube_report.port[i].zr;
          zr_axis = zr_axis > 38 ? zr_axis - 38 : 0;

          post_globals(dev_addr, i, buttons,
            gamecube_report.port[i].x1,
            gamecube_report.port[i].y1,
            gamecube_report.port[i].x2,
            gamecube_report.port[i].y2,
            zl_axis,
            zr_axis,
            0,
            0
          );

          prev_report[dev_addr-1][instance + i] = gamecube_report;
        }
      } else if (prev_report[dev_addr-1][instance + i].port[i].connected) { // disconnected
        remove_players_by_address(dev_addr, instance + i);
        prev_report[dev_addr-1][instance + i] = gamecube_report;
      }
    }
  }
}

// process usb hid output reports
void task_gamecube_adapter(uint8_t dev_addr, uint8_t instance, uint8_t player_index, uint8_t rumble) {
  static uint8_t last_rumble = 0;
  if (rumble != last_rumble) {
    uint8_t buf4[5] = { 0x11, /* GC_CMD_RUMBLE */ };
    for(int i = 0; i < 4; i++) {
      buf4[i+1] = rumble ? 1 : 0;
    }
    tuh_hid_send_report(dev_addr, instance, buf4[0], &(buf4[0])+1, sizeof(buf4) - 1);
    last_rumble = rumble;
  }
}

DeviceInterface gamecube_adapter_interface = {
  .name = "GameCube Adapter for WiiU/Switch",
  .is_device = is_gamecube_adapter,
  .process = process_gamecube_adapter,
  .task = task_gamecube_adapter,
  .init = NULL
};
