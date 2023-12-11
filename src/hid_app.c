// hid_app.c
#include "bsp/board_api.h"
#include "tusb.h"
#include "devices/device_utils.h"
#include "devices/device_registry.h"

const char* dpad_str[] = { "N", "NE", "E", "SE", "S", "SW", "W", "NW", "none" };

uint8_t last_rumble = 0;
uint8_t last_leds = 0;

/** Used to set the LEDs on the controllers */
const uint8_t PLAYER_LEDS[] = {
  0x00, // OFF
  0x01, // LED1  0001
  0x02, // LED2  0010
  0x04, // LED3  0100
  0x08, // LED4  1000
  0x09, // LED5  1001
  0x0A, // LED6  1010
  0x0C, // LED7  1100
  0x0D, // LED8  1101
  0x0E, // LED9  1110
  0x0F, // LED10 1111
};

#define LANGUAGE_ID 0x0409
#define MAX_DEVICES 6
#define MAX_REPORTS 5

// Each HID instance can have multiple reports
typedef struct TU_ATTR_PACKED
{
  dev_type_t type;
  uint8_t report_count;
  tuh_hid_report_info_t report_info[MAX_REPORTS];
  //
  bool kbd_init;
  bool kbd_ready;
} instance_t;

// Cached device report properties on mount
typedef struct TU_ATTR_PACKED
{
  // uint16_t serial[20];
  uint16_t vid, pid;
  instance_t instances[CFG_TUH_HID];
} device_t;

static device_t devices[MAX_DEVICES] = { 0 };

// Keyboard LED control
static uint8_t kbd_leds = 0;
static uint8_t prev_kbd_leds = 0xFF;

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+

// If your host terminal support ansi escape code such as TeraTerm
// it can be use to simulate mouse cursor movement within terminal
#define USE_ANSI_ESCAPE   0

// Uncomment the following line if you desire button-swap when middle button is clicd:
// #define MID_BUTTON_SWAPPABLE  true

// Button swap functionality
// -------------------------
#ifdef MID_BUTTON_SWAPPABLE
const bool buttons_swappable = true;
#else
const bool buttons_swappable = false;
#endif

static bool buttons_swapped = false;

// Core functionality
// ------------------
static uint8_t const keycode2ascii[128][2] =  { HID_KEYCODE_TO_ASCII };

int16_t spinner = 0;
uint32_t buttons;
uint8_t local_x;
uint8_t local_y;

static void process_kbd_report(uint8_t dev_addr, uint8_t instance, hid_keyboard_report_t const *report);
static void process_mouse_report(uint8_t dev_addr, uint8_t instance, hid_mouse_report_t const * report);
static void process_generic_report(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len);

extern void __not_in_flash_func(post_globals)(
  uint8_t dev_addr,
  int8_t instance,
  uint32_t buttons,
  uint8_t analog_1x,
  uint8_t analog_1y,
  uint8_t analog_2x,
  uint8_t analog_2y,
  uint8_t analog_l,
  uint8_t analog_r,
  uint32_t keys,
  uint8_t quad_x
);
extern void __not_in_flash_func(post_mouse_globals)(uint8_t dev_addr, int8_t instance, uint16_t buttons, uint8_t delta_x, uint8_t delta_y, uint8_t spinner);
extern int __not_in_flash_func(find_player_index)(int device_address, int instance_number);
extern void remove_players_by_address(int device_address, int instance);

extern bool is_fun;
extern unsigned char fun_inc;
extern unsigned char fun_player;

void hid_app_init()
{
  register_devices();
}

void hid_app_task(uint8_t rumble, uint8_t leds)
{
  const uint32_t interval_ms = 200;
  static uint32_t start_ms_fun = 0;

  // TODO: throttle this by time
  if (is_fun) {
    fun_inc++;
    if (!fun_inc) {
      fun_player = ++fun_player%0x20;
    }
  }

  // iterate devices and instances that can receive responses
  for(uint8_t dev_addr=1; dev_addr<MAX_DEVICES; dev_addr++)
  {
    for(uint8_t instance=0; instance<CFG_TUH_HID; instance++)
    {
      int8_t player_index = find_player_index(dev_addr, instance);
      int8_t ctrl_type = devices[dev_addr].instances[instance].type;
      switch (ctrl_type)
      {
      case CONTROLLER_DUALSHOCK3: // send DS3 Init, LED and rumble responses
      case CONTROLLER_DUALSHOCK4: // send DS4 LED and rumble response
      case CONTROLLER_DUALSENSE: // send DS5 LED and rumble response
      case CONTROLLER_GAMECUBE: // send GameCube WiiU/Switch Adapter rumble response
      case CONTROLLER_SWITCH: // Switch Pro home LED and rumble response
        device_interfaces[ctrl_type]->task(dev_addr, instance, player_index, rumble);
        break;
      default:
        break;
      }

      // keyboard LED
      if (devices[dev_addr].instances[instance].type == CONTROLLER_KEYBOARD)
      {
        if (!devices[dev_addr].instances[instance].kbd_init && devices[dev_addr].instances[instance].kbd_ready) {
          devices[dev_addr].instances[instance].kbd_init = true;

          // kbd_leds = KEYBOARD_LED_NUMLOCK;
          tuh_hid_set_report(dev_addr, instance, 0, HID_REPORT_TYPE_OUTPUT, &kbd_leds, sizeof(kbd_leds));
        }
        else if (leds != last_leds) {
          if (leds & 0x1) kbd_leds |= KEYBOARD_LED_NUMLOCK;
          else kbd_leds &= ~KEYBOARD_LED_NUMLOCK;
          if (leds & 0x2) kbd_leds |= KEYBOARD_LED_CAPSLOCK;
          else kbd_leds &= ~KEYBOARD_LED_CAPSLOCK;
          if (leds & 0x4) kbd_leds |= KEYBOARD_LED_SCROLLLOCK;
          else kbd_leds &= ~KEYBOARD_LED_SCROLLLOCK;
          tuh_hid_set_report(dev_addr, instance, 0, HID_REPORT_TYPE_OUTPUT, &kbd_leds, sizeof(kbd_leds));
          last_leds = leds;
        }
        if (rumble != last_rumble) {
          if (rumble)
          {
            kbd_leds |= KEYBOARD_LED_CAPSLOCK | KEYBOARD_LED_SCROLLLOCK | KEYBOARD_LED_NUMLOCK;
          } else {
            kbd_leds = 0; // kbd_leds &= ~KEYBOARD_LED_CAPSLOCK;
          }
          last_rumble = rumble;

          if (kbd_leds != prev_kbd_leds) {
            tuh_hid_set_report(dev_addr, instance, 0, HID_REPORT_TYPE_OUTPUT, &kbd_leds, sizeof(kbd_leds));
            prev_kbd_leds = kbd_leds;
          }
        }
      }
    }
  }
}

dev_type_t get_dev_type(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len)
{
  uint16_t vid, pid;
  tuh_vid_pid_get(dev_addr, &vid, &pid);
  printf("VID = %04x, PID = %04x\r\n", vid, pid);

  for (int i = 0; i < CONTROLLER_TYPE_COUNT-1; i++) {
    if (device_interfaces[i] &&
        device_interfaces[i]->is_device(vid, pid)) {
      printf("DEVICE:[%s]\n", device_interfaces[i]->name);
      return (dev_type_t)i;
    }
  }

  // Interface protocol (hid_interface_protocol_enum_t)
  const char* protocol_str[] = { "None", "Keyboard", "Mouse" };
  uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);
  printf("DEVICE:[HID Interface Protocol = %s]\r\n", protocol_str[itf_protocol]);

  switch (itf_protocol)
  {
  case HID_ITF_PROTOCOL_KEYBOARD:
    return CONTROLLER_KEYBOARD;
    break;
  case HID_ITF_PROTOCOL_MOUSE:
    return CONTROLLER_MOUSE;
    break;
  default:
    break;
  }

  if (device_interfaces[CONTROLLER_DINPUT]->check_descriptor(dev_addr, instance, desc_report, desc_len))
  {
    printf("DEVICE:[%s]\n", device_interfaces[CONTROLLER_DINPUT]->name);
    return CONTROLLER_DINPUT;
  }

  printf("DEVICE:[UKNOWN]\n");
  return CONTROLLER_UNKNOWN;
}

//--------------------------------------------------------------------+
// TinyUSB Callbacks
//--------------------------------------------------------------------+

// Invoked when device with hid interface is mounted
// Report descriptor is also available for use. tuh_hid_parse_report_descriptor()
// can be used to parse common/simple enough descriptor.
// Note: if report descriptor length > CFG_TUH_ENUMERATION_BUFSIZE, it will be skipped
// therefore report_desc = NULL, desc_len = 0
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len)
{
  printf("HID device address = %d, instance = %d is mounted\r\n", dev_addr, instance);

  dev_type_t dev_type = get_dev_type(dev_addr, instance, desc_report, desc_len);
  devices[dev_addr].instances[instance].type = dev_type;

  // Set device type and defaults
  switch (dev_type)
  {
  case CONTROLLER_DUALSHOCK3:
  case CONTROLLER_SWITCH:
    device_interfaces[dev_type]->init(dev_addr, instance);
    break;
  case CONTROLLER_KEYBOARD:
    devices[dev_addr].instances[instance].type = CONTROLLER_KEYBOARD;
    devices[dev_addr].instances[instance].kbd_ready = false;
    devices[dev_addr].instances[instance].kbd_init = false;
  case CONTROLLER_MOUSE:
    devices[dev_addr].instances[instance].type = CONTROLLER_MOUSE;
    break;
  default:
    break;
  }

  if (dev_type == CONTROLLER_UNKNOWN)
  {
    devices[dev_addr].instances[instance].report_count =
      tuh_hid_parse_report_descriptor(devices[dev_addr].instances[instance].report_info, MAX_REPORTS, desc_report, desc_len);
    printf("HID has %u reports \r\n", devices[dev_addr].instances[instance].report_count);
  }

  // gets serial for discovering some devices
  // uint16_t temp_buf[128];
  // if (0 == tuh_descriptor_get_serial_string_sync(dev_addr, LANGUAGE_ID, temp_buf, sizeof(temp_buf)))
  // {
  //   for(int i=0; i<20; i++){
  //     devices[dev_addr].serial[i] = temp_buf[i];
  //   }
  // }

  // request to receive report
  // tuh_hid_report_received_cb() will be invoked when report is available
  if ( !tuh_hid_receive_report(dev_addr, instance) )
  {
    printf("Error: cannot request to receive report\r\n");
  }
}

// Invoked when device with hid interface is un-mounted
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
{
  printf("HID device address = %d, instance = %d is unmounted\r\n", dev_addr, instance);

  // Reset device states
  dev_type_t dev_type = devices[dev_addr].instances[instance].type;
  switch (dev_type)
  {
  case CONTROLLER_DUALSENSE:
  case CONTROLLER_DUALSHOCK3:
  case CONTROLLER_DUALSHOCK4:
  case CONTROLLER_SWITCH:
  case CONTROLLER_DINPUT:
    device_interfaces[dev_type]->unmount(dev_addr, instance);
  default:
    break;
  }

  devices[dev_addr].instances[instance].type = CONTROLLER_UNKNOWN;
}

// Invoked when received report from device via interrupt endpoint
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
  dev_type_t dev_type = devices[dev_addr].instances[instance].type;
  if (dev_type == CONTROLLER_UNKNOWN)
  {
    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);
    switch (itf_protocol)
    {
      case HID_ITF_PROTOCOL_KEYBOARD:
        TU_LOG2("HID receive boot keyboard report\r\n");
        process_kbd_report(dev_addr, instance, (hid_keyboard_report_t const*) report );
      break;

      case HID_ITF_PROTOCOL_MOUSE:
        TU_LOG2("HID receive boot mouse report\r\n");
        process_mouse_report(dev_addr, instance, (hid_mouse_report_t const*) report );
      break;

      default:
        TU_LOG2("HID receive generic report\r\n");
        process_generic_report(dev_addr, instance, report, len);
      break;
    }
  }
  else
  {
    // process known device interface reports
    device_interfaces[dev_type]->process(dev_addr, instance, report, len);
  }

  // continue to request to receive report
  if ( !tuh_hid_receive_report(dev_addr, instance) )
  {
    printf("Error: cannot request to receive report\r\n");
  }
}

//--------------------------------------------------------------------+
// Keyboard
//--------------------------------------------------------------------+

void calculate_coordinates(uint32_t stick_keys, int intensity, uint8_t *x_value, uint8_t *y_value) {
  uint16_t angle_degrees = 0;
  uint8_t offset = (127.0 - ((intensity/100.0) * 127.0));

  if (stick_keys && intensity) {
    if (stick_keys <= 0x000f) {
      switch (stick_keys)
      {
      case 0x01: // W
          angle_degrees = 0;
          break;
      case 0x02: // S
          angle_degrees = 180;
          break;
      case 0x04: // A
          angle_degrees = 270;
          break;
      case 0x08: // D
          angle_degrees = 90;
          break;
      default:
          break;
      }
    } else if (stick_keys <= 0x00ff) {
      switch (stick_keys)
      {
      case 0x12: // S ⇾ W
          angle_degrees = 0;
          break;
      case 0x81: // W ⇾ D
      case 0x18: // D ⇾ W
          angle_degrees = 45;
          break;
      case 0x84: // A ⇾ D
          angle_degrees = 90;
          break;
      case 0x82: // S ⇾ D
      case 0x28: // D ⇾ S
          angle_degrees = 135;
          break;
      case 0x21: // W ⇾ S
          angle_degrees = 180;
          break;
      case 0x42: // S ⇾ A
      case 0x24: // A ⇾ S
          angle_degrees = 225;
          break;
      case 0x41: // W ⇾ A
      case 0x14: // A ⇾ W
          angle_degrees = 315;
          break;
      case 0x48: // D ⇾ A
          angle_degrees = 270;
          break;
      default:
          break;
      }
    } else if (stick_keys <= 0x0fff) {
      switch (stick_keys)
      {
      case 0x841: // W ⇾ A ⇾ D
      case 0x812: // S ⇾ W ⇾ D
      case 0x182: // S ⇾ D ⇾ W
      case 0x814: // A ⇾ W ⇾ D
      case 0x184: // A ⇾ D ⇾ W
      case 0x128: // D ⇾ S ⇾ W
          angle_degrees = 45;
          break;
      case 0x821: // W ⇾ S ⇾ D
      case 0x281: // W ⇾ D ⇾ S
      case 0x842: // S ⇾ A ⇾ D
      case 0x824: // A ⇾ S ⇾ D
      case 0x284: // A ⇾ D ⇾ S
      case 0x218: // D ⇾ W ⇾ S
          angle_degrees = 135;
          break;
      case 0x421: // W ⇾ S ⇾ A
      case 0x241: // W ⇾ A ⇾ S
      case 0x482: // S ⇾ D ⇾ A
      case 0x214: // A ⇾ W ⇾ S
      case 0x248: // D ⇾ A ⇾ S
          angle_degrees = 225;
          break;
      case 0x124: // A ⇾ S ⇾ W
      case 0x418: // D ⇾ W ⇾ A
      case 0x148: // D ⇾ A ⇾ W
      case 0x481: // W ⇾ D ⇾ A
      case 0x412: // S ⇾ W ⇾ A
      case 0x142: // S ⇾ A ⇾ W
          angle_degrees = 315;
          break;
      default:
          break;
      }
    } else if (stick_keys <= 0xffff) {
      switch (stick_keys)
      {
      case 0x8412: // S ⇾ W ⇾ A ⇾ D
      case 0x8142: // S ⇾ A ⇾ W ⇾ D
      case 0x1842: // S ⇾ A ⇾ D ⇾ W
      case 0x8124: // A ⇾ S ⇾ W ⇾ D
      case 0x1824: // A ⇾ S ⇾ D ⇾ W
      case 0x1284: // A ⇾ D ⇾ S ⇾ W
          angle_degrees = 45;
          break;
      case 0x8421: // W ⇾ S ⇾ A ⇾ D
      case 0x8241: // W ⇾ A ⇾ S ⇾ D
      case 0x2841: // W ⇾ A ⇾ D ⇾ S
      case 0x8214: // A ⇾ W ⇾ S ⇾ D
      case 0x2814: // A ⇾ W ⇾ D ⇾ S
      case 0x2184: // A ⇾ D ⇾ W ⇾ S
          angle_degrees = 135;
          break;
      case 0x2148: // D ⇾ A ⇾ W ⇾ S
      case 0x4821: // W ⇾ S ⇾ D ⇾ A
      case 0x4281: // W ⇾ D ⇾ S ⇾ A
      case 0x2481: // W ⇾ D ⇾ A ⇾ S
      case 0x4218: // D ⇾ W ⇾ S ⇾ A
      case 0x2418: // D ⇾ W ⇾ A ⇾ S
          angle_degrees = 225;
          break;
      case 0x4812: // S ⇾ W ⇾ D ⇾ A
      case 0x4182: // S ⇾ D ⇾ W ⇾ A
      case 0x1482: // S ⇾ D ⇾ A ⇾ W
      case 0x4128: // D ⇾ S ⇾ W ⇾ A
      case 0x1428: // D ⇾ S ⇾ A ⇾ W
      case 0x1248: // D ⇾ A ⇾ S ⇾ W
          angle_degrees = 315;
          break;
      default:
          break;
      }
    }
  }

  switch (angle_degrees)
  {
  case 0: // Up
    *x_value = 128;
    *y_value = 255 - offset;
    break;

  case 45: // Up + Right
    *x_value = 245 - offset;
    *y_value = 245 - offset;
    break;

  case 90: // Right
    *x_value = 255 - offset;
    *y_value = 128;
    break;

  case 135: // Down + Right
    *x_value = 245 - offset;
    *y_value = 11 + offset;
    break;

  case 180: // Down
    *x_value = 128;
    *y_value = 1 + offset;
    break;

  case 225: // Down + Left
    *x_value = 11 + offset;
    *y_value = 11 + offset;
    break;

  case 270: // Left
    *x_value = 1 + offset;
    *y_value = 128;
    break;

  case 315: // Up + Left
    *x_value = 11 + offset;
    *y_value = 245 - offset;
    break;

  default:
    break;
  }

  // printf("in: %d° %d%, x:%d, y:%d, keys: %x\n", angle_degrees, intensity, *x_value, *y_value, stick_keys);
  return;
}

// look up new key in previous keys
static inline bool find_key_in_report(hid_keyboard_report_t const *report, uint8_t keycode)
{
  for(uint8_t i=0; i<6; i++)
  {
    if (report->keycode[i] == keycode)  return true;
  }

  return false;
}

static void process_kbd_report(uint8_t dev_addr, uint8_t instance, hid_keyboard_report_t const *report)
{
  static hid_keyboard_report_t prev_report = { 0, 0, {0} }; // previous report to check key released

  uint8_t analog_left_x = 128;
  uint8_t analog_left_y = 128;
  uint8_t analog_right_x = 128;
  uint8_t analog_right_y = 128;
  uint8_t analog_l = 0;
  uint8_t analog_r = 0;
  bool has_6btns = true;
  bool dpad_left = false, dpad_down = false, dpad_right = false, dpad_up = false;
  bool btns_run = false, btns_sel = false, btns_one = false, btns_two = false,
       btns_three = false, btns_four = false, btns_five = false, btns_six = false,
       btns_home = false;

  uint32_t hatSwitchKeys = 0x0;
  uint32_t leftStickKeys = 0x0;
  uint32_t rightStickKeys = 0x0;
  uint8_t hatIndex = 0;
  uint8_t leftIndex = 0;
  uint8_t rightIndex = 0;

  bool const is_shift = report->modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT);
  bool const is_ctrl = report->modifier & (KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_RIGHTCTRL);
  bool const is_alt = report->modifier & (KEYBOARD_MODIFIER_LEFTALT | KEYBOARD_MODIFIER_RIGHTALT);

  // parse 3 keycode bytes into single word to return
  uint32_t reportKeys = report->keycode[0] | (report->keycode[1] << 8) | (report->keycode[2] << 16);
  if (report->modifier & (KEYBOARD_MODIFIER_LEFTSHIFT)) {
    reportKeys = reportKeys << 8 | HID_KEY_SHIFT_LEFT;
  } else if (report->modifier & (KEYBOARD_MODIFIER_RIGHTSHIFT)) {
    reportKeys = reportKeys << 8 | HID_KEY_SHIFT_RIGHT;
  }
  if (is_ctrl) {
    reportKeys = reportKeys << 8 | HID_KEY_CONTROL_LEFT;
  }
  if (is_alt) {
    reportKeys = reportKeys << 8 | HID_KEY_ALT_LEFT;
  }
  if (report->modifier & (KEYBOARD_MODIFIER_LEFTGUI)) {
    reportKeys = reportKeys << 8 | HID_KEY_GUI_LEFT;
  } else if (report->modifier & (KEYBOARD_MODIFIER_RIGHTGUI)) {
    reportKeys = reportKeys << 8 | HID_KEY_GUI_RIGHT;
  }

  // wait until first report before sending init led output report
  if (!devices[dev_addr].instances[instance].kbd_ready) {
    devices[dev_addr].instances[instance].kbd_ready = true;
  }

  //------------- example code ignore control (non-printable) key affects -------------//
  for(uint8_t i=0; i<6; i++)
  {
    if ( report->keycode[i] )
    {
      if (report->keycode[i] == HID_KEY_ESCAPE || report->keycode[i] == HID_KEY_EQUAL) btns_run = true; // Start
      if (report->keycode[i] == HID_KEY_P || report->keycode[i] == HID_KEY_MINUS) btns_sel = true; // Select / Z
#ifdef CONFIG_PCE
      // more ideal PCE enter button for SuperSD3 Menu
      if (report->keycode[i] == HID_KEY_J || report->keycode[i] == HID_KEY_ENTER) btns_two = true; // II
      if (report->keycode[i] == HID_KEY_K || report->keycode[i] == HID_KEY_BACKSPACE) btns_one = true; // I
#else
      if (report->keycode[i] == HID_KEY_J || report->keycode[i] == HID_KEY_ENTER) btns_one = true; // A
      if (report->keycode[i] == HID_KEY_K || report->keycode[i] == HID_KEY_BACKSPACE) btns_two = true; // B
#endif
      if (report->keycode[i] == HID_KEY_L) btns_three = true; // X
      if (report->keycode[i] == HID_KEY_SEMICOLON) btns_four = true; // Y
      if (report->keycode[i] == HID_KEY_U || report->keycode[i] == HID_KEY_PAGE_UP) btns_five = true; // L
      if (report->keycode[i] == HID_KEY_I || report->keycode[i] == HID_KEY_PAGE_DOWN) btns_six = true; // R

      // HAT SWITCH
      switch (report->keycode[i])
      {
      case HID_KEY_1:
      case HID_KEY_ARROW_UP:
          hatSwitchKeys |= (0x1 << (4 * hatIndex));
          hatIndex++;
          break;
      case HID_KEY_3:
      case HID_KEY_ARROW_DOWN:
          hatSwitchKeys |= (0x2 << (4 * hatIndex));
          hatIndex++;
          break;
      case HID_KEY_2:
      case HID_KEY_ARROW_LEFT:
          hatSwitchKeys |= (0x4 << (4 * hatIndex));
          hatIndex++;
          break;
      case HID_KEY_4:
      case HID_KEY_ARROW_RIGHT:
          hatSwitchKeys |= (0x8 << (4 * hatIndex));
          hatIndex++;
          break;
      default:
          break;
      }

      // LEFT STICK
      switch (report->keycode[i])
      {
      case HID_KEY_W:
          leftStickKeys |= (0x1 << (4 * leftIndex));
          leftIndex++;
          break;
      case HID_KEY_S:
          leftStickKeys |= (0x2 << (4 * leftIndex));
          leftIndex++;
          break;
      case HID_KEY_A:
          leftStickKeys |= (0x4 << (4 * leftIndex));
          leftIndex++;
          break;
      case HID_KEY_D:
          leftStickKeys |= (0x8 << (4 * leftIndex));
          leftIndex++;
          break;
      default:
          break;
      }

      // RIGHT STICK
      switch (report->keycode[i])
      {
      case HID_KEY_M:
          rightStickKeys |= (0x1 << (4 * rightIndex));
          rightIndex++;
          break;
      case HID_KEY_PERIOD:
          rightStickKeys |= (0x2 << (4 * rightIndex));
          rightIndex++;
          break;
      case HID_KEY_COMMA:
          rightStickKeys |= (0x4 << (4 * rightIndex));
          rightIndex++;
          break;
      case HID_KEY_SLASH:
          rightStickKeys |= (0x8 << (4 * rightIndex));
          rightIndex++;
          break;
      default:
          break;
      }

      if (is_ctrl && is_alt && report->keycode[i] == HID_KEY_DELETE)
      {
      #ifdef CONFIG_XB1
        btns_home = true;
      #elif CONFIG_NGC
        // gc-swiss irg
        btns_sel = true;
        dpad_down = true;
        btns_two = true;
        btns_six = true;
      #elif CONFIG_PCE
        // SSDS3 igr
        btns_sel = true;
        btns_run = true;
      #endif
      }

      if ( find_key_in_report(&prev_report, report->keycode[i]) )
      {
        // exist in previous report means the current key is holding
      }else
      {
        // printf("keycode(%d)\r\n", report->keycode[i]);
        // not existed in previous report means the current key is pressed
        // bool const is_shift = report->modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT);
        // uint8_t ch = keycode2ascii[report->keycode[i]][is_shift ? 1 : 0];
        // putchar(ch);
        // if ( ch == '\r' ) putchar('\n'); // added new line for enter key

        // fflush(stdout); // flush right away, else nanolib will wait for newline
      }
    }
  }

  // calculate left stick angle degrees
  if (leftStickKeys) {
    int leftIntensity = leftStickKeys ? (is_shift ? 28 : 78) : 0;
    calculate_coordinates(leftStickKeys, leftIntensity, &analog_left_x, &analog_left_y);
  }

  if (rightStickKeys) {
    int rightIntensity = rightStickKeys ? (is_shift ? 28 : 78) : 0;
    calculate_coordinates(rightStickKeys, rightIntensity, &analog_right_x, &analog_right_y);
  }

  if (hatSwitchKeys) {
    uint8_t hat_switch_x, hat_switch_y;
    calculate_coordinates(hatSwitchKeys, 100, &hat_switch_x, &hat_switch_y);
    dpad_up = hat_switch_y > 128;
    dpad_down = hat_switch_y < 128;
    dpad_left = hat_switch_x < 128;
    dpad_right = hat_switch_x > 128;
  }

  buttons = (((false)      ? 0x00 : 0x20000) | // r3
             ((false)      ? 0x00 : 0x10000) | // l3
             ((btns_six)   ? 0x00 : 0x8000) |
             ((btns_five)  ? 0x00 : 0x4000) |
             ((btns_four)  ? 0x00 : 0x2000) |
             ((btns_three) ? 0x00 : 0x1000) |
             ((has_6btns)  ? 0x00 : 0x0800) |
             ((btns_home)  ? 0x00 : 0x0400) | // home
             ((false)      ? 0x00 : 0x0200) | // r2
             ((false)      ? 0x00 : 0x0100) | // l2
             ((dpad_left)  ? 0x00 : 0x0008) |
             ((dpad_down)  ? 0x00 : 0x0004) |
             ((dpad_right) ? 0x00 : 0x0002) |
             ((dpad_up)    ? 0x00 : 0x0001) |
             ((btns_run)   ? 0x00 : 0x0080) |
             ((btns_sel)   ? 0x00 : 0x0040) |
             ((btns_two)   ? 0x00 : 0x0020) |
             ((btns_one)   ? 0x00 : 0x0010));

  post_globals(dev_addr, instance, buttons, analog_left_x, analog_left_y, analog_right_x, analog_right_y, analog_l, analog_r, reportKeys, 0);

  prev_report = *report;
}

//--------------------------------------------------------------------+
// Mouse
//--------------------------------------------------------------------+

void cursor_movement(int8_t x, int8_t y, int8_t wheel, uint8_t spinner)
{

uint8_t x1, y1;

#if USE_ANSI_ESCAPE
  // Move X using ansi escape
  if ( x < 0)
  {
    printf(ANSI_CURSOR_BACKWARD(%d), (-x)); // move left
  }else if ( x > 0)
  {
    printf(ANSI_CURSOR_FORWARD(%d), x); // move right
  }

  // Move Y using ansi escape
  if ( y < 0)
  {
    printf(ANSI_CURSOR_UP(%d), (-y)); // move up
  }else if ( y > 0)
  {
    printf(ANSI_CURSOR_DOWN(%d), y); // move down
  }

  // Scroll using ansi escape
  if (wheel < 0)
  {
    printf(ANSI_SCROLL_UP(%d), (-wheel)); // scroll up
  }else if (wheel > 0)
  {
    printf(ANSI_SCROLL_DOWN(%d), wheel); // scroll down
  }

  printf("\r\n");
#else
  printf("(%d %d %d %d)\r\n", x, y, wheel, spinner);
#endif
}

static void process_mouse_report(uint8_t dev_addr, uint8_t instance, hid_mouse_report_t const * report)
{
  static hid_mouse_report_t prev_report = { 0 };

  static bool previous_middle_button = false;

  //------------- button state  -------------//
  uint8_t button_changed_mask = report->buttons ^ prev_report.buttons;
  if ( button_changed_mask & report->buttons)
  {
    printf(" %c%c%c%c%c ",
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

  if (buttons_swapped)
  {
     buttons = (((0xfff00)) | // no six button controller byte
                ((0x0000f)) | // no dpad button presses
                ((report->buttons & MOUSE_BUTTON_MIDDLE)   ? 0x00 : 0x80) |
                ((report->buttons & MOUSE_BUTTON_FORWARD ) ? 0x00 : 0x40) |
                ((report->buttons & MOUSE_BUTTON_RIGHT)    ? 0x00 : 0x20) |
                ((report->buttons & MOUSE_BUTTON_LEFT)     ? 0x00 : 0x10));
  }
  else
  {
     buttons = (((0xfff00)) |
                ((0x0000f)) |
                ((report->buttons & MOUSE_BUTTON_MIDDLE)   ? 0x00 : 0x80) |
                ((report->buttons & MOUSE_BUTTON_FORWARD ) ? 0x00 : 0x40) |
                ((report->buttons & MOUSE_BUTTON_LEFT)     ? 0x00 : 0x20) |
                ((report->buttons & MOUSE_BUTTON_RIGHT)    ? 0x00 : 0x10));
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
  post_mouse_globals(dev_addr, instance, buttons, local_x, local_y, spinner);

  //------------- cursor movement -------------//
  cursor_movement(report->x, report->y, report->wheel, spinner);
}

//--------------------------------------------------------------------+
// Generic Report
//--------------------------------------------------------------------+

static void process_generic_report(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
  (void) dev_addr;

  uint8_t const rpt_count = devices[dev_addr].instances[instance].report_count;
  tuh_hid_report_info_t* rpt_info_arr = devices[dev_addr].instances[instance].report_info;
  tuh_hid_report_info_t* rpt_info = NULL;

  if ( rpt_count == 1 && rpt_info_arr[0].report_id == 0)
  {
    // Simple report without report ID as 1st byte
    rpt_info = &rpt_info_arr[0];
  }
  else
  {
    // Composite report, 1st byte is report ID, data starts from 2nd byte
    uint8_t const rpt_id = report[0];

    // Find report id in the arrray
    for(uint8_t i=0; i<rpt_count; i++)
    {
      if (rpt_id == rpt_info_arr[i].report_id )
      {
        rpt_info = &rpt_info_arr[i];
        break;
      }
    }

    report++;
    len--;
  }

  if (!rpt_info)
  {
    printf("Couldn't find the report info for this report !\r\n");
    return;
  }

  // For complete list of Usage Page & Usage checkout src/class/hid/hid.h. For examples:
  // - Keyboard                     : Desktop, Keyboard
  // - Mouse                        : Desktop, Mouse
  // - Gamepad                      : Desktop, Gamepad
  // - Consumer Control (Media Key) : Consumer, Consumer Control
  // - System Control (Power key)   : Desktop, System Control
  // - Generic (vendor)             : 0xFFxx, xx
  if ( rpt_info->usage_page == HID_USAGE_PAGE_DESKTOP )
  {
    switch (rpt_info->usage)
    {
      case HID_USAGE_DESKTOP_KEYBOARD:
        TU_LOG1("HID receive keyboard report\r\n");
        // Assume keyboard follow boot report layout
        process_kbd_report(dev_addr, instance, (hid_keyboard_report_t const*) report );
      break;

      case HID_USAGE_DESKTOP_MOUSE:
        TU_LOG1("HID receive mouse report\r\n");
        // Assume mouse follow boot report layout
        process_mouse_report(dev_addr, instance, (hid_mouse_report_t const*) report );
      break;

      default: break;
    }
  }
}
