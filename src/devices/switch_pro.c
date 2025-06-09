// switch_pro.c
#include "switch_pro.h"
#include "globals.h"
#include "pico/time.h"

// Switch instance state
typedef struct TU_ATTR_PACKED
{
  bool conn_ack;
  bool baud;
  bool baud_ack;
  bool handshake;
  bool handshake_ack;
  bool usb_enable;
  bool usb_enable_ack;
  bool home_led_set;
  bool full_report_enabled;
  bool imu_enabled;
  bool command_ack;
  uint8_t rumble;
  uint8_t player_led_set;
} switch_instance_t;

// Cached device report properties on mount
typedef struct TU_ATTR_PACKED
{
  switch_instance_t instances[CFG_TUH_HID];
  uint8_t instance_count;
  uint8_t instance_root;
  bool is_pro;
} switch_device_t;

static switch_device_t switch_devices[MAX_DEVICES] = { 0 };

// check if device is Nintendo Switch
static inline bool is_switch_pro(uint16_t vid, uint16_t pid)
{
  return ((vid == 0x057e && (
           pid == 0x2009 || // Nintendo Switch Pro
           pid == 0x200e    // JoyCon Charge Grip
  )));
}

// check if 2 reports are different enough
bool diff_report_switch_pro(switch_pro_report_t const* rpt1, switch_pro_report_t const* rpt2)
{
  bool result;

  // x, y, z, rz must different than 2 to be counted
  result = diff_than_n(rpt1->left_x, rpt2->left_x, 4) || diff_than_n(rpt1->left_y, rpt2->left_y, 4) ||
           diff_than_n(rpt1->right_x, rpt2->right_x, 4) || diff_than_n(rpt1->right_y, rpt2->right_y, 4);

  // check the reset with mem compare (everything but the sticks)
  result |= memcmp(&rpt1->battery_level_and_connection_info + 1, &rpt2->battery_level_and_connection_info + 1, 3);
  result |= memcmp(&rpt1->subcommand_ack, &rpt2->subcommand_ack, 36);

  return result;
}

// scales down switch analog value to a single byte
uint8_t scale_analog_switch_pro(uint16_t switch_val)
{
    // If the input is zero, then output min value of 1
    if (switch_val == 0) {
        return 1;
    }

    // Otherwise, scale the switch value from [1, 4095] to [1, 255]
    return 1 + ((switch_val - 1) * 255) / 4095;
}

// resets default values in case devices are hotswapped
void unmount_switch_pro(uint8_t dev_addr, uint8_t instance)
{
  TU_LOG1("SWITCH[%d|%d]: Unmount Reset\r\n", dev_addr, instance);
  switch_devices[dev_addr].instances[instance].conn_ack = false;
  switch_devices[dev_addr].instances[instance].baud = false;
  switch_devices[dev_addr].instances[instance].baud_ack = false;
  switch_devices[dev_addr].instances[instance].handshake = false;
  switch_devices[dev_addr].instances[instance].handshake_ack = false;
  switch_devices[dev_addr].instances[instance].usb_enable = false;
  switch_devices[dev_addr].instances[instance].usb_enable_ack = false;
  switch_devices[dev_addr].instances[instance].home_led_set = false;
  switch_devices[dev_addr].instances[instance].command_ack = true;
  switch_devices[dev_addr].instances[instance].full_report_enabled = false;
  switch_devices[dev_addr].instances[instance].imu_enabled = false;
  switch_devices[dev_addr].instances[instance].rumble = 0;
  switch_devices[dev_addr].instances[instance].player_led_set = 0xff;
  switch_devices[dev_addr].is_pro = false;

  if (switch_devices[dev_addr].instance_count > 1) {
    switch_devices[dev_addr].instance_count--;
  } else {
    switch_devices[dev_addr].instance_count = 0;
  }

  // if (switch_devices[dev_addr].instance_count == 1 &&
  //     switch_devices[dev_addr].instance_root == instance) {
  //   if (switch_devices[dev_addr].instance_root == 1) {
  //     switch_devices[dev_addr].instance_root = 0;
  //   } else {
  //     switch_devices[dev_addr].instance_root = 1;
  //   }
  // }
}

// prints raw switch pro input report byte data
void print_report_switch_pro(switch_pro_report_01_t* report, uint32_t length)
{
    TU_LOG1("Bytes: ");
    for(uint32_t i = 0; i < length; i++) {
        TU_LOG1("%02X ", report->buf[i]);
    }
    TU_LOG1("\n");
}

// process usb hid input reports
void input_report_switch_pro(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
  uint32_t buttons;
  // previous report used to compare for changes
  static switch_pro_report_t prev_report[5][5];

  switch_pro_report_t update_report;
  memcpy(&update_report, report, sizeof(update_report));

  if (update_report.report_id == 0x30) // Switch Controller Report
  {
    switch_devices[dev_addr].instances[instance].usb_enable_ack = true;

    update_report.left_x = (update_report.left_stick[0] & 0xFF) | ((update_report.left_stick[1] & 0x0F) << 8);
    update_report.left_y = ((update_report.left_stick[1] & 0xF0) >> 4) | ((update_report.left_stick[2] & 0xFF) << 4);
    update_report.right_x = (update_report.right_stick[0] & 0xFF) | ((update_report.right_stick[1] & 0x0F) << 8);
    update_report.right_y = ((update_report.right_stick[1] & 0xF0) >> 4) | ((update_report.right_stick[2] & 0xFF) << 4);

    if (diff_report_switch_pro(&prev_report[dev_addr-1][instance], &update_report))
    {
      TU_LOG1("SWITCH[%d|%d]: Report ID = 0x%x\r\n", dev_addr, instance, update_report.report_id);
      TU_LOG1("(lx, ly, rx, ry) = (%u, %u, %u, %u)\r\n", update_report.left_x, update_report.left_y, update_report.right_x, update_report.right_y);
      TU_LOG1("DPad = ");

      if (update_report.down) TU_LOG1("Down ");
      if (update_report.up) TU_LOG1("Up ");
      if (update_report.right) TU_LOG1("Right ");
      if (update_report.left ) TU_LOG1("Left ");

      TU_LOG1("; Buttons = ");
      if (update_report.y) TU_LOG1("Y ");
      if (update_report.b) TU_LOG1("B ");
      if (update_report.a) TU_LOG1("A ");
      if (update_report.x) TU_LOG1("X ");
      if (update_report.l) TU_LOG1("L ");
      if (update_report.r) TU_LOG1("R ");
      if (update_report.zl) TU_LOG1("ZL ");
      if (update_report.zr) TU_LOG1("ZR ");
      if (update_report.lstick) TU_LOG1("LStick ");
      if (update_report.rstick) TU_LOG1("RStick ");
      if (update_report.select) TU_LOG1("Select ");
      if (update_report.start) TU_LOG1("Start ");
      if (update_report.home) TU_LOG1("Home ");
      if (update_report.cap) TU_LOG1("Cap ");
      if (update_report.sr_r) TU_LOG1("sr_r ");
      if (update_report.sl_l) TU_LOG1("sl_l ");
      TU_LOG1("\r\n");

      int threshold = 256;
      bool dpad_up    = update_report.up;
      bool dpad_right = update_report.right;
      bool dpad_down  = update_report.down;
      bool dpad_left  = update_report.left;
      bool bttn_b1 = update_report.b;
      bool bttn_b2 = update_report.a;
      bool bttn_b3 = update_report.y;
      bool bttn_b4 = update_report.x;
      bool bttn_l1 = update_report.l;
      bool bttn_r1 = update_report.r;
      bool bttn_s1 = update_report.select || update_report.zl || update_report.zr;
      bool bttn_s2 = update_report.start;
      bool bttn_a1 = update_report.home;

      uint8_t leftX = 0;
      uint8_t leftY = 0;
      uint8_t rightX = 0;
      uint8_t rightY = 0;

      if (switch_devices[dev_addr].is_pro) {
        leftX = scale_analog_switch_pro(update_report.left_x);
        leftY = scale_analog_switch_pro(update_report.left_y);
        rightX = scale_analog_switch_pro(update_report.right_x);
        rightY = scale_analog_switch_pro(update_report.right_y);
      } else {
        bool is_left_joycon = (!update_report.right_x && !update_report.right_y);
        bool is_right_joycon = (!update_report.left_x && !update_report.left_y);
        if (is_left_joycon) {
          dpad_up    = update_report.up;
          dpad_right = update_report.right;
          dpad_down  = update_report.down;
          dpad_left  = update_report.left;
          bttn_l1 = update_report.l;
          bttn_s2 = false;

          leftX = scale_analog_switch_pro(update_report.left_x + 127);
          leftY = scale_analog_switch_pro(update_report.left_y - 127);
        }
        else if (is_right_joycon)
        {
          dpad_up    = false; // (right_stick_y > (2048 + threshold));
          dpad_right = false; // (right_stick_x > (2048 + threshold));
          dpad_down  = false; // (right_stick_y < (2048 - threshold));
          dpad_left  = false; // (right_stick_x < (2048 - threshold));
          bttn_a1 = false;

          rightX = scale_analog_switch_pro(update_report.right_x);
          rightY = scale_analog_switch_pro(update_report.right_y + 127);
        }
      }

      buttons = (((dpad_up)              ? 0x00 : USBR_BUTTON_DU) |
                 ((dpad_down)            ? 0x00 : USBR_BUTTON_DD) |
                 ((dpad_left)            ? 0x00 : USBR_BUTTON_DL) |
                 ((dpad_right)           ? 0x00 : USBR_BUTTON_DR) |
                 ((bttn_b1)              ? 0x00 : USBR_BUTTON_B1) |
                 ((bttn_b2)              ? 0x00 : USBR_BUTTON_B2) |
                 ((bttn_b3)              ? 0x00 : USBR_BUTTON_B3) |
                 ((bttn_b4)              ? 0x00 : USBR_BUTTON_B4) |
                 ((bttn_l1)              ? 0x00 : USBR_BUTTON_L1) |
                 ((bttn_r1)              ? 0x00 : USBR_BUTTON_R1) |
                 ((update_report.sr_l)   ? 0x00 : USBR_BUTTON_L2) |
                 ((update_report.sr_r)   ? 0x00 : USBR_BUTTON_R2) |
                 ((bttn_s1)              ? 0x00 : USBR_BUTTON_S1) |
                 ((bttn_s2)              ? 0x00 : USBR_BUTTON_S2) |
                 ((update_report.lstick) ? 0x00 : USBR_BUTTON_L3) |
                 ((update_report.rstick) ? 0x00 : USBR_BUTTON_R3) |
                 ((bttn_a1)              ? 0x00 : USBR_BUTTON_A1) |
                 ((1)/*has_6btns*/       ? 0x00 : 0x800));

      // add to accumulator and post to the state machine
      // if a scan from the host machine is ongoing, wait
      bool is_root = instance == switch_devices[dev_addr].instance_root;
      post_globals(dev_addr, is_root ? instance : -1, buttons, leftX, leftY, rightX, rightY, 0, 0, 0, 0);

      prev_report[dev_addr-1][instance] = update_report;

    }
  }
  else // process input reports for events and command acknowledgments
  {
    switch_pro_report_01_t state_report;
    memcpy(&state_report, report, sizeof(state_report));

    // JC_INPUT_USB_RESPONSE (connection events & command acknowledgments)
    if (state_report.buf[0] == 0x81 && state_report.buf[1] == 0x01) { // JC_USB_CMD_CONN_STATUS
      if (state_report.buf[2] == 0x00) { // connect
        switch_devices[dev_addr].instances[instance].conn_ack = true;
      } else if (state_report.buf[2] == 0x03) { // disconnect
        unmount_switch_pro(dev_addr, instance);
        remove_players_by_address(dev_addr, instance);
      }
    }
    else if (state_report.buf[0] == 0x81 && state_report.buf[1] == 0x02) { // JC_USB_CMD_HANDSHAKE
      switch_devices[dev_addr].instances[instance].handshake_ack = true;
    }
    else if (state_report.buf[0] == 0x81 && state_report.buf[1] == 0x03) { // JC_USB_CMD_BAUDRATE_3M
      switch_devices[dev_addr].instances[instance].baud_ack = true;
    }
    else if (state_report.buf[0] == 0x81 && state_report.buf[1] == 0x92) { // command ack
      switch_devices[dev_addr].instances[instance].command_ack = true;
    }
    else if (state_report.buf[0] == 0x21) {
      switch_devices[dev_addr].instances[instance].command_ack = true;
    }

    TU_LOG1("SWITCH[%d|%d]: Report ID = 0x%x\r\n", dev_addr, instance, state_report.data.report_id);

    uint32_t length = sizeof(state_report.buf) / sizeof(state_report.buf[0]);
    print_report_switch_pro(&state_report, length);
  }

  if (update_report.report_id == 0x81)
  {
    tuh_hid_receive_report(dev_addr, instance);
  }
}

// process usb hid output reports
void output_switch_pro(uint8_t dev_addr, uint8_t instance, int player_index, uint8_t rumble, uint8_t leds)
{
  static uint8_t output_sequence_counter = 0;
  // Nintendo Switch Pro/JoyCons Charging Grip initialization and subcommands (rumble|leds)
  // See: https://github.com/Dan611/hid-procon/
  //      https://github.com/felis/USB_Host_Shield_2.0/
  //      https://github.com/nicman23/dkms-hid-nintendo/
  //      https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering/blob/master/USB-HID-Notes.md

  if (true/*switch_devices[dev_addr].instances[instance].conn_ack*/) // bug fix for 3rd-party ctrls?
  {
    // set the faster baud rate
    // if (!switch_devices[dev_addr].instances[instance].baud) {
    //   TU_LOG1("SWITCH[%d|%d]: CMD_HID, USB_BAUD\r\n", dev_addr, instance);

    //   uint8_t baud_command[2] = {CMD_HID, SUBCMD_USB_BAUD};

    //   switch_devices[dev_addr].instances[instance].baud = 
    //    tuh_hid_send_report(dev_addr, instance, 0, baud_command, sizeof(baud_command));

    // // wait for baud ask and then send init handshake
    // } else if (!switch_devices[dev_addr].instances[instance].handshake && switch_devices[dev_addr].instances[instance].baud_ack) {
    if (!switch_devices[dev_addr].instances[instance].handshake) {
      TU_LOG1("SWITCH[%d|%d]: CMD_HID, HANDSHAKE\r\n", dev_addr, instance);

      uint8_t handshake_command[2] = {CMD_HID, SUBCMD_HANDSHAKE};

      switch_devices[dev_addr].instances[instance].handshake =
        tuh_hid_send_report(dev_addr, instance, 0, handshake_command, sizeof(handshake_command));

      tuh_hid_receive_report(dev_addr, instance);

    // wait for handshake ack and then send USB enable mode
    } else if (!switch_devices[dev_addr].instances[instance].usb_enable && switch_devices[dev_addr].instances[instance].handshake_ack) {
      TU_LOG1("SWITCH[%d|%d]: CMD_HID, DISABLE_TIMEOUT\r\n", dev_addr, instance);

      uint8_t disable_timeout_cmd[2] = {CMD_HID, SUBCMD_DISABLE_TIMEOUT};

      switch_devices[dev_addr].instances[instance].usb_enable =
        tuh_hid_send_report(dev_addr, instance, 0, disable_timeout_cmd, sizeof(disable_timeout_cmd));

      sleep_ms(100);
      tuh_hid_receive_report(dev_addr, instance);

    // wait for usb enabled acknowledgment
    } else if (switch_devices[dev_addr].instances[instance].usb_enable) {

      uint8_t report[14] = { 0 };
      uint8_t report_size = 10;

      report[0x00] = CMD_RUMBLE_ONLY; // COMMAND
       // Lowest 4-bit is a sequence number, which needs to be increased for every report

      if (!switch_devices[dev_addr].instances[instance].home_led_set) {
        TU_LOG1("SWITCH[%d|%d]: CMD_AND_RUMBLE, CMD_LED_HOME \r\n", dev_addr, instance);
        
        report_size = 14;

        report[0x01] = output_sequence_counter++;
        report[0x00] = CMD_AND_RUMBLE;   // COMMAND
        report[0x0A + 0] = CMD_LED_HOME; // SUB_COMMAND
        
        // SUB_COMMAND ARGS
        report[0x0A + 1] = (0 /* Number of cycles */ << 4) | (true ? 0xF : 0) /* Global mini cycle duration */;
        report[0x0A + 2] = (0x1 /* LED start intensity */ << 4) | 0x0 /* Number of full cycles */;
        report[0x0A + 3] = (0x0 /* Mini Cycle 1 LED intensity */ << 4) | 0x1 /* Mini Cycle 2 LED intensity */;

        // It is possible set up to 15 mini cycles, but we simply just set the LED constantly on after momentary off.
        // See: https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering/blob/master/bluetooth_hid_subcommands_notes.md#subcommand-0x38-set-home-light

        switch_devices[dev_addr].instances[instance].home_led_set = true;
        tuh_hid_send_report(dev_addr, instance, 0, report, report_size);
        sleep_ms(100);

      } else if (!switch_devices[dev_addr].instances[instance].full_report_enabled) {
        TU_LOG1("SWITCH[%d|%d]: CMD_AND_RUMBLE, CMD_MODE, FULL_REPORT_MODE \r\n", dev_addr, instance);

        report_size = 14;

        report[0x01] = output_sequence_counter++;
        report[0x00] = CMD_AND_RUMBLE;              // COMMAND
        report[0x0A + 0] = CMD_MODE;                // SUB_COMMAND
        report[0x0A + 1] = SUBCMD_FULL_REPORT_MODE; // SUB_COMMAND ARGS

        switch_devices[dev_addr].instances[instance].full_report_enabled = true;
        tuh_hid_send_report(dev_addr, instance, 0, report, report_size);
        sleep_ms(100);

      // } else if (!switch_devices[dev_addr].instances[instance].imu_enabled) {
      //   TU_LOG1("SWITCH[%d|%d]: CMD_AND_RUMBLE, CMD_GYRO, 1 \r\n", dev_addr, instance);

      //   report_size = 12;

      //   report[0x00] = CMD_AND_RUMBLE; // COMMAND
      //   report[0x0A + 0] = CMD_GYRO;   // SUB_COMMAND
      //   report[0x0A + 1] = 1 ? 1 : 0;  // SUB_COMMAND ARGS

      //   switch_devices[dev_addr].instances[instance].imu_enabled = true;
      //   tuh_hid_send_report(dev_addr, instance, 0, report, report_size);
      //   sleep_ms(100);

      // } else if (switch_devices[dev_addr].instances[instance].imu_enabled) {
      } else if (switch_devices[dev_addr].instances[instance].full_report_enabled) {
        uint8_t instance_count = switch_devices[dev_addr].instance_count;
        uint8_t instance_index = instance_count == 1 ? instance : switch_devices[dev_addr].instance_root;
        player_index = find_player_index(dev_addr, instance_index);

        if (is_fun || (player_index >= 0 &&
          switch_devices[dev_addr].instances[instance].player_led_set != player_index)
        ) {
          TU_LOG1("SWITCH[%d|%d]: CMD_AND_RUMBLE, CMD_LED, %d\r\n", dev_addr, instance, player_index+1);

          report_size = 12;

          report[0x01] = output_sequence_counter++;
          report[0x00] = CMD_AND_RUMBLE; // COMMAND
          report[0x0A + 0] = CMD_LED;    // SUB_COMMAND

          // SUB_COMMAND ARGS
          switch (player_index+1)
          {
          case 1:
          case 2:
          case 3:
          case 4:
          case 5:
            report[0x0A + 1] = PLAYER_LEDS[player_index+1];
            break;

          default: // unassigned - turn all leds on
            // 
            report[0x0A + 1] = 0x0f;
            break;
          }

          // fun
          if (player_index+1 && is_fun) {
            report[0x0A + 1] = (fun_inc & 0b00001111);
          }

          switch_devices[dev_addr].instances[instance].player_led_set = player_index;

          tuh_hid_send_report(dev_addr, instance, 0, report, report_size);
        }
        else if (switch_devices[dev_addr].instances[instance].rumble != rumble)
        {
          TU_LOG1("SWITCH[%d|%d]: CMD_RUMBLE_ONLY, %d\r\n", dev_addr, instance, rumble);

          report_size = 10;
          
          report[0x01] = output_sequence_counter++;
          report[0x00] = CMD_RUMBLE_ONLY; // COMMAND
          
          if (rumble) {
            // Left rumble ON data
            report[0x02 + 0] = 0x20;
            report[0x02 + 1] = 0x78;
            report[0x02 + 2] = 0x28;
            report[0x02 + 3] = 0x5e;

            // Right rumble ON data
            report[0x02 + 4] = 0x20;
            report[0x02 + 5] = 0x78;
            report[0x02 + 6] = 0x28;
            report[0x02 + 7] = 0x5e;
          } else {
            // Left rumble OFF data
            report[0x02 + 0] = 0x00;
            report[0x02 + 1] = 0x01;
            report[0x02 + 2] = 0x40;
            report[0x02 + 3] = 0x40;

            // Right rumble OFF data
            report[0x02 + 4] = 0x00;
            report[0x02 + 5] = 0x01;
            report[0x02 + 6] = 0x40;
            report[0x02 + 7] = 0x40;
          }

          switch_devices[dev_addr].instances[instance].rumble = rumble;

          tuh_hid_send_report(dev_addr, instance, 0, report, report_size);
        }
      }
    }
  }
}

// process usb hid output reports
void task_switch_pro(uint8_t dev_addr, uint8_t instance, int player_index, uint8_t rumble, uint8_t leds)
{
  const uint32_t interval_ms = 20;
  static uint32_t start_ms = 0;

  uint32_t current_time_ms = to_ms_since_boot(get_absolute_time());
  if (current_time_ms - start_ms >= interval_ms)
  {
    start_ms = current_time_ms;
    output_switch_pro(dev_addr, instance, player_index, rumble, leds);
  }
}

// initialize usb hid input
static inline bool init_switch_pro(uint8_t dev_addr, uint8_t instance)
{
  TU_LOG1("SWITCH[%d|%d]: Mounted\r\n", dev_addr, instance);

  switch_devices[dev_addr].instances[instance].command_ack = true;
  if ((++switch_devices[dev_addr].instance_count) == 1) {
    switch_devices[dev_addr].instance_root = instance; // save initial root instance to merge extras into
  }

  uint16_t vid, pid;
  tuh_vid_pid_get(dev_addr, &vid, &pid);
  if (pid == 0x2009) switch_devices[dev_addr].is_pro = true;
}

DeviceInterface switch_pro_interface = {
  .name = "Switch Pro",
  .is_device = is_switch_pro,
  .process = input_report_switch_pro,
  .task = output_switch_pro,
  .unmount = unmount_switch_pro,
  .init = init_switch_pro,
};
