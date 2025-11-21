// hid_mouse.c
#include "hid_mouse.h"
#include "globals.h"
#include "input_event.h"

static uint8_t local_x;
static uint8_t local_y;

// Button swap functionality
// -------------------------
#ifdef MID_BUTTON_SWAPPABLE
const bool buttons_swappable = true;
#else
const bool buttons_swappable = false;
#endif

static bool buttons_swapped = false;

void cursor_movement(int8_t x, int8_t y, int8_t wheel, uint8_t spinner)
{
  uint8_t x1, y1;

#if USE_ANSI_ESCAPE
  // Move X using ansi escape
  if ( x < 0)
  {
    TU_LOG1(ANSI_CURSOR_BACKWARD(%d), (-x)); // move left
  }else if ( x > 0)
  {
    TU_LOG1(ANSI_CURSOR_FORWARD(%d), x); // move right
  }

  // Move Y using ansi escape
  if ( y < 0)
  {
    TU_LOG1(ANSI_CURSOR_UP(%d), (-y)); // move up
  }else if ( y > 0)
  {
    TU_LOG1(ANSI_CURSOR_DOWN(%d), y); // move down
  }

  // Scroll using ansi escape
  if (wheel < 0)
  {
    TU_LOG1(ANSI_SCROLL_UP(%d), (-wheel)); // scroll up
  }else if (wheel > 0)
  {
    TU_LOG1(ANSI_SCROLL_DOWN(%d), wheel); // scroll down
  }

  TU_LOG1("\r\n");
#else
  TU_LOG1("(%d %d %d %d)\r\n", x, y, wheel, spinner);
#endif
}

// process usb hid input reports
void process_hid_mouse(uint8_t dev_addr, uint8_t instance, uint8_t const* mouse_report, uint16_t len) {
  uint32_t buttons;
  hid_mouse_report_t const* report = (hid_mouse_report_t const*)mouse_report;
  static hid_mouse_report_t prev_report = { 0 };

  static bool previous_middle_button = false;

  //------------- button state  -------------//
  uint8_t button_changed_mask = report->buttons ^ prev_report.buttons;
  if (button_changed_mask & report->buttons) {
    TU_LOG1(" %c%c%c%c%c ",
       report->buttons & MOUSE_BUTTON_BACKWARD  ? 'R' : '-',
       report->buttons & MOUSE_BUTTON_FORWARD   ? 'S' : '-',
       report->buttons & MOUSE_BUTTON_LEFT      ? '2' : '-',
       report->buttons & MOUSE_BUTTON_MIDDLE    ? 'M' : '-',
       report->buttons & MOUSE_BUTTON_RIGHT     ? '1' : '-');

    if (buttons_swappable && (report->buttons & MOUSE_BUTTON_MIDDLE) &&
        (previous_middle_button == false))
       buttons_swapped = (buttons_swapped ? false : true);

    previous_middle_button = (report->buttons & MOUSE_BUTTON_MIDDLE);
  }

  if (buttons_swapped) {
     buttons = (((0xfff00)) | // no six button controller byte
                ((0x0000f)) | // no dpad button presses (isMouse)
                ((report->buttons & MOUSE_BUTTON_RIGHT)   ? 0x00 : USBR_BUTTON_B1) |
                ((report->buttons & MOUSE_BUTTON_LEFT)    ? 0x00 : USBR_BUTTON_B2) |
                ((report->buttons & MOUSE_BUTTON_BACKWARD)? 0x00 : USBR_BUTTON_B3) |
                ((report->buttons & MOUSE_BUTTON_FORWARD) ? 0x00 : USBR_BUTTON_S1) |
                ((report->buttons & MOUSE_BUTTON_MIDDLE)  ? 0x00 : USBR_BUTTON_S2));
  } else {
     buttons = (((0xfff00)) |
                ((0x0000f)) |
                ((report->buttons & MOUSE_BUTTON_LEFT)    ? 0x00 : USBR_BUTTON_B1) |
                ((report->buttons & MOUSE_BUTTON_RIGHT)   ? 0x00 : USBR_BUTTON_B2) |
                ((report->buttons & MOUSE_BUTTON_BACKWARD)? 0x00 : USBR_BUTTON_B3) |
                ((report->buttons & MOUSE_BUTTON_FORWARD) ? 0x00 : USBR_BUTTON_S1) |
                ((report->buttons & MOUSE_BUTTON_MIDDLE)  ? 0x00 : USBR_BUTTON_S2));
  }

#ifdef CONFIG_PCE // mice translation
  local_x = (0 - report->x);
  local_y = (0 - report->y);
#else // controllers
  local_x = report->x;
  local_y = ((~report->y) & 0xff);
#endif

#ifdef CONFIG_NUON
  // mouse wheel to spinner rotation conversion
  if (report->wheel != 0) {
    if (report->wheel < 0) { // clockwise
      spinner += ((-1 * report->wheel) + 3);
      if (spinner > 255) spinner -= 255;
    } else { // counter-clockwise
      if (spinner >= ((report->wheel) + 3)) {
        spinner += report->wheel;
        spinner -= 3;
      } else {
        spinner = 255 - ((report->wheel) - spinner) - 3;
      }
    }
  }

  int16_t delta = (report->x * -1);

  // check max/min delta value
  if (delta > 15) delta = 15;
  if (delta < -15) delta = -15;

  // mouse x-axis to spinner rotation conversion
  if (delta != 0) {
    if (delta < 0) { // clockwise
      spinner += delta;
      if (spinner > 255) spinner -= 255;
    } else { // counter-clockwise
      if (spinner >= ((delta))) {
        spinner += delta;
      } else {
        spinner = 255 - delta - spinner;
      }
    }
  }

#endif
  // add to accumulator and post to the state machine
  // if a scan from the host machine is ongoing, wait
  input_event_t event = {
    .dev_addr = dev_addr,
    .instance = instance,
    .type = INPUT_TYPE_MOUSE,
    .buttons = buttons,
    .analog = {128, 128, 128, 128, 128, 0, 0, 128},
    .delta_x = local_x,
    .delta_y = local_y,
    .quad_x = spinner,
    .keys = 0
  };
  post_input_event(&event);

  //------------- cursor movement -------------//
  cursor_movement(report->x, report->y, report->wheel, spinner);
}

DeviceInterface hid_mouse_interface = {
  .name = "HID Mouse",
  .is_device = NULL,
  .init = NULL,
  .task = NULL,
  .process = process_hid_mouse,
};
