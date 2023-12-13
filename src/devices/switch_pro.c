// switch_pro.c
#include "switch_pro.h"
#include "globals.h"
#include "bsp/board_api.h"

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
  bool home_led;
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
  printf("SWITCH[%d|%d]: Unmount Reset\r\n", dev_addr, instance);
  switch_devices[dev_addr].instances[instance].conn_ack = false;
  switch_devices[dev_addr].instances[instance].baud = false;
  switch_devices[dev_addr].instances[instance].baud_ack = false;
  switch_devices[dev_addr].instances[instance].handshake = false;
  switch_devices[dev_addr].instances[instance].handshake_ack = false;
  switch_devices[dev_addr].instances[instance].usb_enable = false;
  switch_devices[dev_addr].instances[instance].usb_enable_ack = false;
  switch_devices[dev_addr].instances[instance].home_led = false;
  switch_devices[dev_addr].instances[instance].command_ack = false;
  switch_devices[dev_addr].instances[instance].rumble = 0;
  switch_devices[dev_addr].instances[instance].player_led_set = 0xff;

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
    printf("Bytes: ");
    for(uint32_t i = 0; i < length; i++) {
        printf("%02X ", report->buf[i]);
    }
    printf("\n");
}

// process usb hid input reports
void input_report_switch_pro(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
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
      printf("SWITCH[%d|%d]: Report ID = 0x%x\r\n", dev_addr, instance, update_report.report_id);
      printf("(lx, ly, rx, ry) = (%u, %u, %u, %u)\r\n", update_report.left_x, update_report.left_y, update_report.right_x, update_report.right_y);
      printf("DPad = ");

      if (update_report.down) printf("Down ");
      if (update_report.up) printf("Up ");
      if (update_report.right) printf("Right ");
      if (update_report.left ) printf("Left ");

      printf("; Buttons = ");
      if (update_report.y) printf("Y ");
      if (update_report.b) printf("B ");
      if (update_report.a) printf("A ");
      if (update_report.x) printf("X ");
      if (update_report.l) printf("L ");
      if (update_report.r) printf("R ");
      if (update_report.zl) printf("ZL ");
      if (update_report.zr) printf("ZR ");
      if (update_report.lstick) printf("LStick ");
      if (update_report.rstick) printf("RStick ");
      if (update_report.select) printf("Select ");
      if (update_report.start) printf("Start ");
      if (update_report.home) printf("Home ");
      if (update_report.cap) printf("Cap ");
      if (update_report.sr_r) printf("sr_r ");
      if (update_report.sl_l) printf("sl_l ");
      printf("\r\n");

      bool has_6btns = true;
      int threshold = 256;
      bool dpad_up    = update_report.up;
      bool dpad_right = update_report.right;
      bool dpad_down  = update_report.down;
      bool dpad_left  = update_report.left;
      bool bttn_1 = update_report.a;
      bool bttn_2 = update_report.b;
      bool bttn_3 = update_report.x;
      bool bttn_4 = update_report.y;
      bool bttn_5 = update_report.l;
      bool bttn_6 = update_report.r;
      bool bttn_run = update_report.start;
      bool bttn_sel = update_report.select || update_report.zl || update_report.zr;
      bool bttn_home = update_report.home;

      uint8_t leftX = 0;
      uint8_t leftY = 0;
      uint8_t rightX = 0;
      uint8_t rightY = 0;

      bool is_left_joycon = (!update_report.right_x && !update_report.right_y);
      bool is_right_joycon = (!update_report.left_x && !update_report.left_y);
      if (is_left_joycon) {
        dpad_up    = update_report.up;
        dpad_right = update_report.right;
        dpad_down  = update_report.down;
        dpad_left  = update_report.left;
        bttn_5 = update_report.l;
        bttn_run = false;

        leftX = scale_analog_switch_pro(update_report.left_x + 127);
        leftY = scale_analog_switch_pro(update_report.left_y - 127);
      }
      else if (is_right_joycon)
      {
        dpad_up    = false; // (right_stick_y > (2048 + threshold));
        dpad_right = false; // (right_stick_x > (2048 + threshold));
        dpad_down  = false; // (right_stick_y < (2048 - threshold));
        dpad_left  = false; // (right_stick_x < (2048 - threshold));
        bttn_home = false;

        rightX = scale_analog_switch_pro(update_report.right_x);
        rightY = scale_analog_switch_pro(update_report.right_y + 127);
      }
      else
      {
        leftX = scale_analog_switch_pro(update_report.left_x);
        leftY = scale_analog_switch_pro(update_report.left_y);
        rightX = scale_analog_switch_pro(update_report.right_x);
        rightY = scale_analog_switch_pro(update_report.right_y);
      }

      buttons = (
        ((update_report.rstick) ? 0x00 : 0x20000) |
        ((update_report.lstick) ? 0x00 : 0x10000) |
        ((bttn_6)     ? 0x00 : 0x8000) | // VI
        ((bttn_5)     ? 0x00 : 0x4000) | // V
        ((bttn_4)     ? 0x00 : 0x2000) | // IV
        ((bttn_3)     ? 0x00 : 0x1000) | // III
        ((has_6btns)  ? 0x00 : 0x0800) |
        ((bttn_home)  ? 0x00 : 0x0400) | // home
        ((update_report.sr_r) ? 0x00 : 0x0200) | // r2
        ((update_report.sr_l) ? 0x00 : 0x0100) | // l2
        ((dpad_left)  ? 0x00 : 0x0008) |
        ((dpad_down)  ? 0x00 : 0x0004) |
        ((dpad_right) ? 0x00 : 0x0002) |
        ((dpad_up)    ? 0x00 : 0x0001) |
        ((bttn_run)   ? 0x00 : 0x0080) | // Run
        ((bttn_sel)   ? 0x00 : 0x0040) | // Select
        ((bttn_2)     ? 0x00 : 0x0020) | // II
        ((bttn_1)     ? 0x00 : 0x0010)   // I
      );

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

    printf("SWITCH[%d|%d]: Report ID = 0x%x\r\n", dev_addr, instance, state_report.data.report_id);

    uint32_t length = sizeof(state_report.buf) / sizeof(state_report.buf[0]);
    print_report_switch_pro(&state_report, length);
  }
}

// sends commands to switch pro to init/set led/rumble
bool send_command_switch_pro(uint8_t dev_addr, uint8_t instance, uint8_t *data, uint8_t len)
{
  uint8_t buf[8 + len];
  buf[0] = 0x80; // PROCON_REPORT_SEND_USB
  buf[1] = 0x92; // PROCON_USB_DO_CMD
  buf[2] = 0x00;
  buf[3] = 0x31;
  buf[4] = 0x00;
  buf[5] = 0x00;
  buf[6] = 0x00;
  buf[7] = 0x00;

  memcpy(buf + 8, data, len);

  tuh_hid_send_report(dev_addr, instance, buf[0], &(buf[0])+1, sizeof(buf) - 1);
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
  if (switch_devices[dev_addr].instances[instance].conn_ack)
  {
    // set the faster baud rate
    // if (!switch_devices[dev_addr].instances[instance].baud) {
    //   switch_devices[dev_addr].instances[instance].baud = true;

    //   printf("SWITCH[%d|%d]: Baud\r\n", dev_addr, instance);
    //   uint8_t buf2[1] = { 0x03 /* PROCON_USB_BAUD */ };
    //   tuh_hid_send_report(dev_addr, instance, 0x80, buf2, sizeof(buf2));

    // // wait for baud ask and then send init handshake
    // } else
    if (!switch_devices[dev_addr].instances[instance].handshake/* && switch_devices[dev_addr].instances[instance].baud_ack*/) {
      switch_devices[dev_addr].instances[instance].handshake = true;

      printf("SWITCH[%d|%d]: Handshake\r\n", dev_addr, instance);
      uint8_t buf1[1] = { 0x02 /* PROCON_USB_HANDSHAKE */ };
      tuh_hid_send_report(dev_addr, instance, 0x80, buf1, sizeof(buf1));

    // wait for handshake ack and then send USB enable mode
    } else if (!switch_devices[dev_addr].instances[instance].usb_enable && switch_devices[dev_addr].instances[instance].handshake_ack) {
      switch_devices[dev_addr].instances[instance].usb_enable = true;

      printf("SWITCH[%d|%d]: Enable USB\r\n", dev_addr, instance);
      uint8_t buf3[1] = { 0x04 /* PROCON_USB_ENABLE */ };
      tuh_hid_send_report(dev_addr, instance, 0x80, buf3, sizeof(buf3));

    // wait for usb enabled acknowledgment
    } else if (switch_devices[dev_addr].instances[instance].usb_enable_ack) {
      // SWITCH SUB-COMMANDS
      //
      // Based on: https://github.com/Dan611/hid-procon
      //           https://github.com/nicman23/dkms-hid-nintendo
      //
      uint8_t data[14] = { 0 };
      data[0x00] = 0x01; // Report ID - PROCON_CMD_AND_RUMBLE

      if (!switch_devices[dev_addr].instances[instance].home_led) {
        switch_devices[dev_addr].instances[instance].home_led = true;

        // It is possible set up to 15 mini cycles, but we simply just set the LED constantly on after momentary off.
        // See: https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering/blob/master/bluetooth_hid_subcommands_notes.md#subcommand-0x38-set-home-light
        data[0x01] = output_sequence_counter++; // Lowest 4-bit is a sequence number, which needs to be increased for every report

        data[0x0A + 0] = 0x38; // PROCON_CMD_LED_HOME
        data[0x0A + 1] = (0 /* Number of cycles */ << 4) | (true ? 0xF : 0) /* Global mini cycle duration */;
        data[0x0A + 2] = (0x1 /* LED start intensity */ << 4) | 0x0 /* Number of full cycles */;
        data[0x0A + 3] = (0x0 /* Mini Cycle 1 LED intensity */ << 4) | 0x1 /* Mini Cycle 2 LED intensity */;

        send_command_switch_pro(dev_addr, instance, data, 10 + 4);

      } else if (switch_devices[dev_addr].instances[instance].command_ack) {
        player_index = find_player_index(dev_addr, switch_devices[dev_addr].instance_count == 1 ? instance : switch_devices[dev_addr].instance_root);

        if (switch_devices[dev_addr].instances[instance].player_led_set != player_index || is_fun)
        {
          switch_devices[dev_addr].instances[instance].player_led_set = player_index;

          // See: https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering/blob/master/bluetooth_hid_subcommands_notes.md#subcommand-0x30-set-player-lights
          data[0x01] = output_sequence_counter++; // Lowest 4-bit is a sequence number, which needs to be increased for every report

          data[0x0A + 0] = 0x30; // PROCON_CMD_LED

          // led player indicator
          switch (player_index+1)
          {
          case 1:
          case 2:
          case 3:
          case 4:
          case 5:
            data[0x0A + 1] = PLAYER_LEDS[player_index+1];
            break;

          default: // unassigned
            // turn all leds on
            data[0x0A + 1] = 0x0f;
            break;
          }

          // fun
          if (player_index+1 && is_fun) {
            data[0x0A + 1] = (fun_inc & 0b00001111);
          }

          switch_devices[dev_addr].instances[instance].command_ack = false;
          send_command_switch_pro(dev_addr, instance, data, 10 + 2);
        }
        else if (switch_devices[dev_addr].instances[instance].rumble != rumble)
        {
          switch_devices[dev_addr].instances[instance].rumble = rumble;

          uint8_t buf[10] = { 0 };
          buf[0x00] = 0x10; // Report ID - PROCON_CMD_RUMBLE_ONLY
          buf[0x01] = output_sequence_counter++; // Lowest 4-bit is a sequence number, which needs to be increased for every report
          
          // // Snippet values from https://github.com/DanielOgorchock/linux/blob/7811b8f1f00ee9f195b035951749c57498105d52/drivers/hid/hid-nintendo.c#L197
          // // joycon_rumble_frequencies.freq = { 0x2000, 0x28,   95 }
          // uint16_t freq_data_high_high = 0x2000;
          // uint8_t freq_data_low_low = 0x28;
          // // joycon_rumble_amplitudes.amp = { 0x78, 0x005e,  422 }
          // uint8_t amp_data_high = 0x78;
          // uint16_t amp_data_low = 0x005e;
          // printf("0x%x 0x%x 0x%x 0x%x\n\n", (freq_data_high_high >> 8) & 0xFF, (freq_data_high_high & 0xFF) + amp_data_high, freq_data_low_low + ((amp_data_low >> 8) & 0xFF), amp_data_low & 0xFF);

          if (rumble) {
            // Left rumble ON data
            buf[0x02 + 0] = 0x20;
            buf[0x02 + 1] = 0x78;
            buf[0x02 + 2] = 0x28;
            buf[0x02 + 3] = 0x5e;
            // buf[0x02 + 0] = (freq_data_high_high >> 8) & 0xFF;
            // buf[0x02 + 1] = (freq_data_high_high & 0xFF) + amp_data_high;
            // buf[0x02 + 2] = freq_data_low_low + ((amp_data_low >> 8) & 0xFF);
            // buf[0x02 + 3] = amp_data_low & 0xFF;

            // Right rumble ON data
            buf[0x02 + 4] = 0x20;
            buf[0x02 + 5] = 0x78;
            buf[0x02 + 6] = 0x28;
            buf[0x02 + 7] = 0x5e;
            // buf[0x02 + 4] = (freq_data_high_high >> 8) & 0xFF;
            // buf[0x02 + 5] = (freq_data_high_high & 0xFF) + amp_data_high;
            // buf[0x02 + 6] = freq_data_low_low + ((amp_data_low >> 8) & 0xFF);
            // buf[0x02 + 7] = amp_data_low & 0xFF;
          } else {
            // Left rumble OFF data
            buf[0x02 + 0] = 0x00;
            buf[0x02 + 1] = 0x01;
            buf[0x02 + 2] = 0x40;
            buf[0x02 + 3] = 0x40;

            // Right rumble OFF data
            buf[0x02 + 4] = 0x00;
            buf[0x02 + 5] = 0x01;
            buf[0x02 + 6] = 0x40;
            buf[0x02 + 7] = 0x40;
          }
          switch_devices[dev_addr].instances[instance].command_ack = false;
          send_command_switch_pro(dev_addr, instance, buf, 10);
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

  uint32_t current_time_ms = board_millis();
  if (current_time_ms - start_ms >= interval_ms)
  {
    start_ms = current_time_ms;
    output_switch_pro(dev_addr, instance, player_index, rumble, leds);
  }
}

// initialize usb hid input
static inline bool init_switch_pro(uint8_t dev_addr, uint8_t instance)
{
  printf("SWITCH[%d|%d]: Mounted\r\n", dev_addr, instance);

  if ((++switch_devices[dev_addr].instance_count) == 1) {
    switch_devices[dev_addr].instance_root = instance; // save initial root instance to merge extras into
  }
}

DeviceInterface switch_pro_interface = {
  .name = "Switch Pro",
  .is_device = is_switch_pro,
  .process = input_report_switch_pro,
  .task = output_switch_pro,
  .unmount = unmount_switch_pro,
  .init = init_switch_pro,
};
