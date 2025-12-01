// sony_ds4.c
#include "sony_ds4.h"
#include "core/buttons.h"
#include "core/router/router.h"
#include "core/input_event.h"
#include "pico/time.h"
#include "app_config.h"
#include <string.h>

static uint16_t tpadLastPos;
static bool tpadDragging;

// DualSense instance state
typedef struct TU_ATTR_PACKED
{
  uint8_t rumble;
  uint8_t player;
} ds4_instance_t;

// Cached device report properties on mount
typedef struct TU_ATTR_PACKED
{
  ds4_instance_t instances[CFG_TUH_HID];
} ds4_device_t;

static ds4_device_t ds4_devices[MAX_DEVICES] = { 0 };

// ============================================================================
// PS4 AUTH PASSTHROUGH STATE
// ============================================================================

// Auth buffer sizes
#define DS4_AUTH_NONCE_SIZE      56   // Nonce data size (without report ID)
#define DS4_AUTH_SIGNATURE_SIZE  56   // Signature data size
#define DS4_AUTH_STATUS_SIZE     16   // Status report size
#define DS4_AUTH_REPORT_SIZE     64   // Full report size with report ID

// Auth passthrough state
static struct {
    ds4_auth_state_t state;
    uint8_t dev_addr;           // DS4 device address for auth
    uint8_t instance;           // DS4 instance for auth
    bool ds4_available;         // Is a DS4 connected for auth?

    // Nonce from console (to forward to DS4)
    uint8_t nonce_buffer[DS4_AUTH_REPORT_SIZE];
    uint16_t nonce_len;
    bool nonce_pending;         // Nonce waiting to be sent
    bool nonce_sent;            // Nonce has been sent to DS4

    // Signature from DS4 (to return to console)
    uint8_t signature_buffer[DS4_AUTH_REPORT_SIZE];
    uint16_t signature_len;
    bool signature_ready;       // Signature fetched from DS4

    // Status from DS4
    uint8_t status_buffer[DS4_AUTH_STATUS_SIZE];
    bool status_pending;        // Status request pending
    bool status_ready;          // Status fetched from DS4

    // Timing
    uint32_t last_poll_ms;      // Last time we polled DS4
    uint8_t poll_count;         // Number of status polls
    uint8_t nonce_page;         // Current nonce page (0-4, 5 pages total)
    uint8_t signature_page;     // Current signature page being fetched
} ds4_auth = { 0 };

// check if device is Sony PlayStation 4 controllers
bool is_sony_ds4(uint16_t vid, uint16_t pid) {
  return ( (vid == 0x054c && (pid == 0x09cc || pid == 0x05c4)) // Sony DualShock4 
    || (vid == 0x0f0d && pid == 0x005e) // Hori FC4 
    || (vid == 0x0f0d && pid == 0x00ee) // Hori PS4 Mini (PS4-099U) 
    || (vid == 0x1f4f && pid == 0x1002) // ASW GG xrd controller
    || (vid == 0x1532 && pid == 0x0401) // Razer Panthera PS4 Controller (GP2040-CE PS4 Mode)
  );
}

// check if 2 reports are different enough
bool diff_report_ds4(sony_ds4_report_t const* rpt1, sony_ds4_report_t const* rpt2)
{
  bool result;

  // x, y, z, rz must different than 2 to be counted
  result = diff_than_n(rpt1->x, rpt2->x, 2) || diff_than_n(rpt1->y, rpt2->y, 2) ||
           diff_than_n(rpt1->z, rpt2->z, 2) || diff_than_n(rpt1->rz, rpt2->rz, 2) ||
           diff_than_n(rpt1->l2_trigger, rpt2->l2_trigger, 2) ||
           diff_than_n(rpt1->r2_trigger, rpt2->r2_trigger, 2);

  // check the reset with mem compare
  result |= memcmp(&rpt1->rz + 1, &rpt2->rz + 1, 2);
  result |= (rpt1->ps != rpt2->ps);
  result |= (rpt1->tpad != rpt2->tpad);
  result |= memcmp(&rpt1->tpad_f1_pos, &rpt2->tpad_f1_pos, 3);

  return result;
}

// process usb hid input reports
void input_sony_ds4(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
  uint32_t buttons;
  // previous report used to compare for changes
  static sony_ds4_report_t prev_report[5] = { 0 };

  uint8_t const report_id = report[0];
  report++;
  len--;

  // all buttons state is stored in ID 1
  if (report_id == 1)
  {
    sony_ds4_report_t ds4_report;
    memcpy(&ds4_report, report, sizeof(ds4_report));

    // counter is +1, assign to make it easier to compare 2 report
    prev_report[dev_addr-1].counter = ds4_report.counter;

    // only print if changes since it is polled ~ 5ms
    // Since count+1 after each report and  x, y, z, rz fluctuate within 1 or 2
    // We need more than memcmp to check if report is different enough
    if ( diff_report_ds4(&prev_report[dev_addr-1], &ds4_report) )
    {
      TU_LOG1("(x, y, z, rz, l, r) = (%u, %u, %u, %u, %u, %u)\r\n", ds4_report.x, ds4_report.y, ds4_report.z, ds4_report.rz, ds4_report.r2_trigger, ds4_report.l2_trigger);
      TU_LOG1("DPad = %s ", ds4_report.dpad);

      if (ds4_report.square   ) TU_LOG1("Square ");
      if (ds4_report.cross    ) TU_LOG1("Cross ");
      if (ds4_report.circle   ) TU_LOG1("Circle ");
      if (ds4_report.triangle ) TU_LOG1("Triangle ");

      if (ds4_report.l1       ) TU_LOG1("L1 ");
      if (ds4_report.r1       ) TU_LOG1("R1 ");
      if (ds4_report.l2       ) TU_LOG1("L2 ");
      if (ds4_report.r2       ) TU_LOG1("R2 ");

      if (ds4_report.share    ) TU_LOG1("Share ");
      if (ds4_report.option   ) TU_LOG1("Option ");
      if (ds4_report.l3       ) TU_LOG1("L3 ");
      if (ds4_report.r3       ) TU_LOG1("R3 ");

      if (ds4_report.ps       ) TU_LOG1("PS ");
      if (ds4_report.tpad     ) TU_LOG1("TPad ");

      if (!ds4_report.tpad_f1_down) TU_LOG1("F1 ");

      uint16_t tx = (((ds4_report.tpad_f1_pos[1] & 0x0f) << 8)) | ((ds4_report.tpad_f1_pos[0] & 0xff) << 0);
      uint16_t ty = (((ds4_report.tpad_f1_pos[1] & 0xf0) >> 4)) | ((ds4_report.tpad_f1_pos[2] & 0xff) << 4);
      // TU_LOG1(" (tx, ty) = (%u, %u)\r\n", tx, ty);
      // TU_LOG1("\r\n");

      bool dpad_up    = (ds4_report.dpad == 0 || ds4_report.dpad == 1 || ds4_report.dpad == 7);
      bool dpad_right = ((ds4_report.dpad >= 1 && ds4_report.dpad <= 3));
      bool dpad_down  = ((ds4_report.dpad >= 3 && ds4_report.dpad <= 5));
      bool dpad_left  = ((ds4_report.dpad >= 5 && ds4_report.dpad <= 7));

      buttons = (((dpad_up)             ? USBR_BUTTON_DU : 0) |
                 ((dpad_down)           ? USBR_BUTTON_DD : 0) |
                 ((dpad_left)           ? USBR_BUTTON_DL : 0) |
                 ((dpad_right)          ? USBR_BUTTON_DR : 0) |
                 ((ds4_report.cross)    ? USBR_BUTTON_B1 : 0) |
                 ((ds4_report.circle)   ? USBR_BUTTON_B2 : 0) |
                 ((ds4_report.square)   ? USBR_BUTTON_B3 : 0) |
                 ((ds4_report.triangle) ? USBR_BUTTON_B4 : 0) |
                 ((ds4_report.l1)       ? USBR_BUTTON_L1 : 0) |
                 ((ds4_report.r1)       ? USBR_BUTTON_R1 : 0) |
                 ((ds4_report.l2)       ? USBR_BUTTON_L2 : 0) |
                 ((ds4_report.r2)       ? USBR_BUTTON_R2 : 0) |
                 ((ds4_report.share)    ? USBR_BUTTON_S1 : 0) |
                 ((ds4_report.option)   ? USBR_BUTTON_S2 : 0) |
                 ((ds4_report.l3)       ? USBR_BUTTON_L3 : 0) |
                 ((ds4_report.r3)       ? USBR_BUTTON_R3 : 0) |
                 ((ds4_report.ps)       ? USBR_BUTTON_A1 : 0) |
                 ((ds4_report.tpad)     ? USBR_BUTTON_A2 : 0));

      uint8_t analog_1x = ds4_report.x;
      uint8_t analog_1y = 255 - ds4_report.y;
      uint8_t analog_2x = ds4_report.z;
      uint8_t analog_2y = 255 - ds4_report.rz;
      uint8_t analog_l = ds4_report.l2_trigger;
      uint8_t analog_r = ds4_report.r2_trigger;

      // Touch Pad - provides mouse-like delta for horizontal swipes
      // Can be used for spinners, camera control, etc. (platform-agnostic)
      int8_t touchpad_delta_x = 0;
      if (!ds4_report.tpad_f1_down) {
        // Calculate horizontal swipe delta while finger is down
        if (tpadDragging) {
          int16_t delta = 0;
          if (tx >= tpadLastPos) delta = tx - tpadLastPos;
          else delta = (-1) * (tpadLastPos - tx);

          // Clamp delta to reasonable range
          if (delta > 12) delta = 12;
          if (delta < -12) delta = -12;

          touchpad_delta_x = (int8_t)delta;
        }

        tpadLastPos = tx;
        tpadDragging = true;
      } else {
        tpadDragging = false;
      }

      // keep analog within range [1-255]
      ensureAllNonZero(&analog_1x, &analog_1y, &analog_2x, &analog_2y);

      // adds deadzone
      uint8_t deadzone = 40;
      if (analog_1x > (128-(deadzone/2)) && analog_1x < (128+(deadzone/2))) analog_1x = 128;
      if (analog_1y > (128-(deadzone/2)) && analog_1y < (128+(deadzone/2))) analog_1y = 128;
      if (analog_2x > (128-(deadzone/2)) && analog_2x < (128+(deadzone/2))) analog_2x = 128;
      if (analog_2y > (128-(deadzone/2)) && analog_2y < (128+(deadzone/2))) analog_2y = 128;

      // add to accumulator and post to the state machine
      // if a scan from the host machine is ongoing, wait
      input_event_t event = {
        .dev_addr = dev_addr,
        .instance = instance,
        .type = INPUT_TYPE_GAMEPAD,
        .buttons = buttons,
        .button_count = 10,  // PS4: Cross, Circle, Square, Triangle, L1, R1, L2, R2, L3, R3
        .analog = {analog_1x, analog_1y, analog_2x, analog_2y, 128, analog_l, analog_r, 128},
        .delta_x = touchpad_delta_x,  // Touchpad horizontal swipe as mouse-like delta
        .keys = 0
      };
      router_submit_input(&event);

      prev_report[dev_addr-1] = ds4_report;
    }
  }
}

// process usb hid output reports
void output_sony_ds4(uint8_t dev_addr, uint8_t instance, device_output_config_t* config) {
  sony_ds4_output_report_t output_report = {0};
  output_report.set_led = 1;

  // Console-specific LED colors from led_config.h
  switch (config->player_index+1)
  {
  case 1:
    output_report.lightbar_red = LED_P1_R;
    output_report.lightbar_green = LED_P1_G;
    output_report.lightbar_blue = LED_P1_B;
    break;

  case 2:
    output_report.lightbar_red = LED_P2_R;
    output_report.lightbar_green = LED_P2_G;
    output_report.lightbar_blue = LED_P2_B;
    break;

  case 3:
    output_report.lightbar_red = LED_P3_R;
    output_report.lightbar_green = LED_P3_G;
    output_report.lightbar_blue = LED_P3_B;
    break;

  case 4:
    output_report.lightbar_red = LED_P4_R;
    output_report.lightbar_green = LED_P4_G;
    output_report.lightbar_blue = LED_P4_B;
    break;

  case 5:
    output_report.lightbar_red = LED_P5_R;
    output_report.lightbar_green = LED_P5_G;
    output_report.lightbar_blue = LED_P5_B;
    break;

  default:
    output_report.lightbar_red = LED_DEFAULT_R;
    output_report.lightbar_green = LED_DEFAULT_G;
    output_report.lightbar_blue = LED_DEFAULT_B;
    break;
  }

  // fun
  if (config->player_index+1 && config->test) {
    output_report.lightbar_red = config->test;
    output_report.lightbar_green = (config->test%2 == 0) ? config->test+64 : 0;
    output_report.lightbar_blue = (config->test%2 == 0) ? 0 : config->test+128;
  }

  output_report.set_rumble = 1;
  if (config->rumble) {
    output_report.motor_left = 192;
    output_report.motor_right = 192;
  } else {
    output_report.motor_left = 0;
    output_report.motor_right = 0;
  }

  if (ds4_devices[dev_addr].instances[instance].rumble != config->rumble ||
      ds4_devices[dev_addr].instances[instance].player != config->player_index+1 ||
      config->test)
  {
    ds4_devices[dev_addr].instances[instance].rumble = config->rumble;
    ds4_devices[dev_addr].instances[instance].player = config->test ? config->test : config->player_index+1;
    tuh_hid_send_report(dev_addr, instance, 5, &output_report, sizeof(output_report));
  }
}

// process usb hid output reports
void task_sony_ds4(uint8_t dev_addr, uint8_t instance, device_output_config_t* config) {
  const uint32_t interval_ms = 20;
  static uint32_t start_ms = 0;

  uint32_t current_time_ms = to_ms_since_boot(get_absolute_time());
  if (current_time_ms - start_ms >= interval_ms) {
    start_ms = current_time_ms;
    output_sony_ds4(dev_addr, instance, config);
  }
}

// resets default values in case devices are hotswapped
void unmount_sony_ds4(uint8_t dev_addr, uint8_t instance)
{
  ds4_devices[dev_addr].instances[instance].rumble = 0;
  ds4_devices[dev_addr].instances[instance].player = 0xff;
}

DeviceInterface sony_ds4_interface = {
  .name = "Sony DualShock 4",
  .is_device = is_sony_ds4,
  .process = input_sony_ds4,
  .task = task_sony_ds4,
  .unmount = unmount_sony_ds4,
};

// ============================================================================
// PS4 AUTH PASSTHROUGH IMPLEMENTATION
// ============================================================================

// Called when a DS4 is mounted - register it for auth
void ds4_auth_register(uint8_t dev_addr, uint8_t instance) {
    if (!ds4_auth.ds4_available) {
        ds4_auth.dev_addr = dev_addr;
        ds4_auth.instance = instance;
        ds4_auth.ds4_available = true;
        ds4_auth.state = DS4_AUTH_STATE_IDLE;
        TU_LOG1("[DS4 Auth] Registered DS4 at %d:%d for auth passthrough\r\n", dev_addr, instance);
    }
}

// Called when a DS4 is unmounted - unregister it from auth
void ds4_auth_unregister(uint8_t dev_addr, uint8_t instance) {
    if (ds4_auth.ds4_available &&
        ds4_auth.dev_addr == dev_addr &&
        ds4_auth.instance == instance) {
        ds4_auth.ds4_available = false;
        ds4_auth.state = DS4_AUTH_STATE_IDLE;
        TU_LOG1("[DS4 Auth] Unregistered DS4 from auth passthrough\r\n");
    }
}

// Check if a DS4 is available for auth passthrough
bool ds4_auth_is_available(void) {
    return ds4_auth.ds4_available;
}

// Get the current auth state
ds4_auth_state_t ds4_auth_get_state(void) {
    return ds4_auth.state;
}

// Forward nonce from PS4 console to connected DS4
bool ds4_auth_send_nonce(const uint8_t* data, uint16_t len) {
    if (!ds4_auth.ds4_available) {
        TU_LOG1("[DS4 Auth] No DS4 available for auth\r\n");
        return false;
    }

    // Store nonce data for async sending
    if (len > DS4_AUTH_REPORT_SIZE) len = DS4_AUTH_REPORT_SIZE;
    memcpy(ds4_auth.nonce_buffer, data, len);
    ds4_auth.nonce_len = len;
    ds4_auth.nonce_pending = true;
    ds4_auth.nonce_sent = false;
    ds4_auth.signature_ready = false;
    ds4_auth.status_ready = false;
    ds4_auth.state = DS4_AUTH_STATE_NONCE_PENDING;

    TU_LOG1("[DS4 Auth] Nonce received (%d bytes), queued for DS4\r\n", len);
    return true;
}

// Get cached signature response (0xF1)
uint16_t ds4_auth_get_signature(uint8_t* buffer, uint16_t max_len) {
    if (!ds4_auth.signature_ready) {
        // Return zeros if signature not ready
        memset(buffer, 0, max_len);
        return max_len;
    }

    uint16_t copy_len = ds4_auth.signature_len;
    if (copy_len > max_len) copy_len = max_len;
    memcpy(buffer, ds4_auth.signature_buffer, copy_len);
    return copy_len;
}

// Get auth status (0xF2)
uint16_t ds4_auth_get_status(uint8_t* buffer, uint16_t max_len) {
    // Status report format:
    // Byte 0: Sequence number
    // Byte 1: Status (0x00 = busy, 0x10 = ready)
    // Bytes 2-9: CRC or padding
    // Rest: zeros

    uint16_t copy_len = DS4_AUTH_STATUS_SIZE;
    if (copy_len > max_len) copy_len = max_len;

    if (ds4_auth.status_ready) {
        // Return cached status from DS4
        memcpy(buffer, ds4_auth.status_buffer, copy_len);
    } else {
        // Return "busy" status
        memset(buffer, 0, copy_len);
        buffer[1] = 0x00;  // Busy/not ready
    }

    return copy_len;
}

// Reset auth state (0xF3)
void ds4_auth_reset(void) {
    ds4_auth.state = DS4_AUTH_STATE_IDLE;
    ds4_auth.nonce_pending = false;
    ds4_auth.nonce_sent = false;
    ds4_auth.signature_ready = false;
    ds4_auth.status_ready = false;
    ds4_auth.poll_count = 0;

    if (ds4_auth.ds4_available) {
        // Send reset to DS4
        uint8_t reset_buf[8] = { 0 };
        tuh_hid_set_report(ds4_auth.dev_addr, ds4_auth.instance,
                           DS4_AUTH_REPORT_RESET, HID_REPORT_TYPE_FEATURE,
                           reset_buf, sizeof(reset_buf));
    }

    TU_LOG1("[DS4 Auth] Auth state reset\r\n");
}

// TinyUSB callback for get_report completion
void tuh_hid_get_report_complete_cb(uint8_t dev_addr, uint8_t idx,
                                    uint8_t report_id, uint8_t report_type,
                                    uint16_t len) {
    // Check if this is for our auth DS4
    if (!ds4_auth.ds4_available ||
        dev_addr != ds4_auth.dev_addr ||
        idx != ds4_auth.instance) {
        return;
    }

    if (report_type != HID_REPORT_TYPE_FEATURE) return;

    if (report_id == DS4_AUTH_REPORT_SIGNATURE) {
        // Signature response received
        ds4_auth.signature_len = len;
        ds4_auth.signature_ready = true;
        ds4_auth.state = DS4_AUTH_STATE_READY;
        TU_LOG1("[DS4 Auth] Signature received (%d bytes)\r\n", len);
    }
    else if (report_id == DS4_AUTH_REPORT_STATUS) {
        // Status response received
        ds4_auth.status_ready = true;

        // Check if signing is complete (status byte 1 == 0x10 means ready)
        if (ds4_auth.status_buffer[1] == 0x10) {
            TU_LOG1("[DS4 Auth] DS4 signing complete, fetching signature\r\n");
            // Request signature
            tuh_hid_get_report(ds4_auth.dev_addr, ds4_auth.instance,
                               DS4_AUTH_REPORT_SIGNATURE, HID_REPORT_TYPE_FEATURE,
                               ds4_auth.signature_buffer, DS4_AUTH_REPORT_SIZE);
        } else {
            TU_LOG1("[DS4 Auth] DS4 still signing (status=0x%02X)\r\n",
                    ds4_auth.status_buffer[1]);
        }
    }
}

// TinyUSB callback for set_report completion
void tuh_hid_set_report_complete_cb(uint8_t dev_addr, uint8_t idx,
                                    uint8_t report_id, uint8_t report_type,
                                    uint16_t len) {
    // Check if this is for our auth DS4
    if (!ds4_auth.ds4_available ||
        dev_addr != ds4_auth.dev_addr ||
        idx != ds4_auth.instance) {
        return;
    }

    if (report_type != HID_REPORT_TYPE_FEATURE) return;

    if (report_id == DS4_AUTH_REPORT_NONCE) {
        // Nonce was sent successfully
        ds4_auth.nonce_sent = true;
        ds4_auth.nonce_pending = false;
        ds4_auth.state = DS4_AUTH_STATE_SIGNING;
        ds4_auth.poll_count = 0;
        ds4_auth.last_poll_ms = to_ms_since_boot(get_absolute_time());
        TU_LOG1("[DS4 Auth] Nonce sent to DS4, waiting for signing\r\n");
    }
}

// Auth task - called from main loop to handle async operations
void ds4_auth_task(void) {
    if (!ds4_auth.ds4_available) return;

    uint32_t now = to_ms_since_boot(get_absolute_time());

    // Send pending nonce
    if (ds4_auth.nonce_pending && !ds4_auth.nonce_sent) {
        bool result = tuh_hid_set_report(ds4_auth.dev_addr, ds4_auth.instance,
                                         DS4_AUTH_REPORT_NONCE, HID_REPORT_TYPE_FEATURE,
                                         ds4_auth.nonce_buffer, ds4_auth.nonce_len);
        if (result) {
            TU_LOG1("[DS4 Auth] Sending nonce to DS4...\r\n");
        } else {
            TU_LOG1("[DS4 Auth] Failed to send nonce\r\n");
        }
        // Mark as not pending even if failed to avoid retry spam
        ds4_auth.nonce_pending = false;
    }

    // Poll for signing status
    if (ds4_auth.state == DS4_AUTH_STATE_SIGNING) {
        // Poll every 50ms
        if (now - ds4_auth.last_poll_ms >= 50) {
            ds4_auth.last_poll_ms = now;
            ds4_auth.poll_count++;

            // Request status from DS4
            ds4_auth.status_ready = false;
            tuh_hid_get_report(ds4_auth.dev_addr, ds4_auth.instance,
                               DS4_AUTH_REPORT_STATUS, HID_REPORT_TYPE_FEATURE,
                               ds4_auth.status_buffer, DS4_AUTH_STATUS_SIZE);

            // Timeout after ~5 seconds
            if (ds4_auth.poll_count > 100) {
                TU_LOG1("[DS4 Auth] Signing timeout\r\n");
                ds4_auth.state = DS4_AUTH_STATE_ERROR;
            }
        }
    }
}
