// xboxone.c

#include "xboxone.h"
#include "pico/stdlib.h"
#include "tusb.h"

// init for xboxone communication
void xb1_init()
{
  sleep_ms(1000);

  // corrects UART serial output after overclock
  stdio_init_all();

  gpio_init(XBOX_B_BTN_PIN);
  // gpio_disable_pulls(XBOX_B_BTN_PIN);
  gpio_set_dir(XBOX_B_BTN_PIN, GPIO_OUT);

  gpio_init(XBOX_GUIDE_PIN);
  gpio_set_dir(XBOX_GUIDE_PIN, GPIO_OUT);

  gpio_init(XBOX_R3_BTN_PIN);
  gpio_set_dir(XBOX_R3_BTN_PIN, GPIO_OUT);

  gpio_init(XBOX_L3_BTN_PIN);
  gpio_set_dir(XBOX_L3_BTN_PIN, GPIO_OUT);

  gpio_put(XBOX_B_BTN_PIN, 1);
  gpio_put(XBOX_GUIDE_PIN, 1);
  gpio_put(XBOX_R3_BTN_PIN, 1);
  gpio_put(XBOX_L3_BTN_PIN, 1);

#ifdef ADAFRUIT_QTPY_RP2040
  gpio_init(NEOPIXEL_POWER_PIN);
  gpio_set_dir(NEOPIXEL_POWER_PIN, GPIO_OUT);
  gpio_put(NEOPIXEL_POWER_PIN, 1);
#endif

  gpio_init(I2C_SLAVE_SDA_PIN);
  gpio_set_function(I2C_SLAVE_SDA_PIN, GPIO_FUNC_I2C);
  gpio_pull_up(I2C_SLAVE_SDA_PIN);

  gpio_init(I2C_SLAVE_SCL_PIN);
  gpio_set_function(I2C_SLAVE_SCL_PIN, GPIO_FUNC_I2C);
  gpio_pull_up(I2C_SLAVE_SCL_PIN);

  // Initialize Slave I2C for simulated XB1Slim GPIO expander
  i2c_init(I2C_SLAVE_PORT, 400 * 1000);
  i2c_slave_init(I2C_SLAVE_PORT, I2C_SLAVE_ADDRESS, &i2c_slave_handler);

  // Initialize DAC I2C for simulated analog sticks/triggers
  i2c_init(I2C_DAC_PORT, 400 * 1000);
  gpio_set_function(I2C_DAC_SDA_PIN, GPIO_FUNC_I2C);
  gpio_set_function(I2C_DAC_SCL_PIN, GPIO_FUNC_I2C);
  gpio_pull_up(I2C_DAC_SDA_PIN);
  gpio_pull_up(I2C_DAC_SCL_PIN);

  mcp4728_set_config(I2C_DAC_PORT, MCP4728_I2C_ADDR0, 0, 0, 0); // TP64 - LSX
  mcp4728_set_config(I2C_DAC_PORT, MCP4728_I2C_ADDR0, 1, 0, 0); // TP63 - LSY
  mcp4728_set_config(I2C_DAC_PORT, MCP4728_I2C_ADDR0, 2, 0, 0); // TP66 - RSX
  mcp4728_set_config(I2C_DAC_PORT, MCP4728_I2C_ADDR0, 3, 0, 0); // TP65 - RSY
  mcp4728_set_config(I2C_DAC_PORT, MCP4728_I2C_ADDR1, 0, 0, 0); // TP68 - LT
  mcp4728_set_config(I2C_DAC_PORT, MCP4728_I2C_ADDR1, 1, 0, 0); // TP67 - RT
}

// I2C interrupt handlers
void i2c_slave_handler(i2c_inst_t *i2c, i2c_slave_event_t event)
{
  int data;
  size_t bytes_available = 0;
  
  // Handle I2C events
  switch (event) {
    case I2C_SLAVE_RECEIVE:
      // Read data from master
      // printf("[RECEIVE]::");
      
      // determine how many bytes have been received
      bytes_available = i2c_get_read_available(i2c);
      if (bytes_available > 0) {
        // printf("bytes_available:%d data:", bytes_available);

        // read the bytes from the RX FIFO
        i2c_read_raw_blocking(i2c, i2c_slave_write_buffer, bytes_available);
        // process the received bytes as needed
        // for (int i = 0; i < bytes_available; ++i) {
        //   printf(" 0x%x", i2c_slave_write_buffer[i]);
        // }
      }
    break;

    case I2C_SLAVE_REQUEST:
      // Write data to master
      // printf("[REQUEST]:: 0x%x 0x%x", i2c_slave_read_buffer[0], i2c_slave_read_buffer[1]);

      i2c_write_raw_blocking(i2c, i2c_slave_read_buffer, sizeof(i2c_slave_read_buffer));
    break;

    default:
      // printf("[UNHANDLED]");
    break;
  }
  // printf("\n");
}

//
void mcp4728_write_dac(i2c_inst_t *i2c, uint8_t address, uint8_t channel, uint16_t value)
{
  uint8_t buf[3];
  buf[0] = (channel << 1) | 0x40; // Select channel and set Write DAC command
  buf[1] = (value >> 8) & 0x0F; // Set upper 4 bits of value
  buf[2] = value & 0xFF; // Set lower 8 bits of value

  i2c_write_blocking(i2c, address, buf, 3, false);
}

//
void mcp4728_set_config(i2c_inst_t *i2c, uint8_t address, uint8_t channel, uint8_t gain, uint8_t power_down)
{
  uint8_t buf[3];
  buf[0] = (channel << 1) | 0x60; // Select channel and set Write DAC and EEPROM command
  buf[1] = (gain << 4) | (power_down << 1);
  buf[2] = 0; // Dummy value

  i2c_write_blocking(i2c, address, buf, 3, false);
}

// Function to set the power-down mode for a channel on MCP4728
// channel: 0 to 3 for the DAC channels
// pd_mode: 0 to 3 for different power down modes
//          0 = No power-down mode (Normal operation)
//          1 = Power-down mode with 1kΩ to ground
//          2 = Power-down mode with 100kΩ to ground
//          3 = Power-down mode with 500kΩ to ground
void mcp4728_power_down(i2c_inst_t *i2c, uint8_t address, uint8_t channel, uint8_t pd_mode)
{
  uint8_t command[3];

  // Construct command to set the power-down mode for the channel
  // The PD bits are the least significant bits of the first command byte
  command[0] = (0x40 | (channel << 1)) | (pd_mode & 0x03); // Upper command byte with channel and PD mode
  command[1] = 0x00; // Lower data byte (Don't care for power-down mode)
  command[2] = 0x00; // Upper data byte (Don't care for power-down mode)

  // Send the command to the MCP4728
  i2c_write_blocking(i2c, address, command, 3, false);
}

//
// core1_entry - inner-loop for the second core
void __not_in_flash_func(core1_entry)(void)
{
  while (1)
  {
    // Analog outputs
    uint16_t x1Val = ((players[0].analog[0] * 2047)/255);  // ANALOG_X
    uint16_t y1Val = ((players[0].analog[1] * 2047)/255);  // ANALOG_Y
             y1Val = (y1Val - 2047) * -1;
    uint16_t x2Val = ((players[0].analog[2] * 2047)/255);  // ANALOG_Z
    uint16_t y2Val = ((players[0].analog[3] * 2047)/255);  // ANALOG_RX
             y2Val = (y2Val - 2047) * -1;
    uint16_t lVal = ((players[0].analog[5] * 2047)/255);   // ANALOG_RZ
             lVal = (lVal - 2047) * -1;
    uint16_t rVal = ((players[0].analog[6] * 2047)/255);   // ANALOG_SLIDER
             rVal = (rVal - 2047) * -1;

    mcp4728_write_dac(I2C_DAC_PORT, MCP4728_I2C_ADDR0, 0, x1Val);
    mcp4728_write_dac(I2C_DAC_PORT, MCP4728_I2C_ADDR0, 1, y1Val);
    mcp4728_write_dac(I2C_DAC_PORT, MCP4728_I2C_ADDR0, 2, x2Val);
    mcp4728_write_dac(I2C_DAC_PORT, MCP4728_I2C_ADDR0, 3, y2Val);
    mcp4728_write_dac(I2C_DAC_PORT, MCP4728_I2C_ADDR1, 0, lVal);
    mcp4728_write_dac(I2C_DAC_PORT, MCP4728_I2C_ADDR1, 1, rVal);

    // Individual buttons
    gpio_put(XBOX_B_BTN_PIN, ((players[0].output_buttons & USBR_BUTTON_B2) == 0) ? 0 : 1);
    gpio_put(XBOX_GUIDE_PIN, ((players[0].output_buttons & USBR_BUTTON_A1) == 0) ? 0 : 1);
    gpio_put(XBOX_R3_BTN_PIN,((players[0].output_buttons & USBR_BUTTON_R3) == 0) ? 0 : 1);
    gpio_put(XBOX_L3_BTN_PIN,((players[0].output_buttons & USBR_BUTTON_L3) == 0) ? 0 : 1);

    update_pending = false;

    unsigned short int i;
    for (i = 0; i < MAX_PLAYERS; ++i)
    {
      // decrement outputs from globals
      if (players[i].global_x != 0)
      {
        players[i].global_x = (players[i].global_x - (players[i].analog[0] - 128));  // ANALOG_X
        // if (players[i].global_x > 128) players[i].global_x = 128;
        // if (players[i].global_x < -128) players[i].global_x = -128;
        players[i].analog[0] = 128;  // ANALOG_X
      }
      if (players[i].global_y != 0)
      {
        players[i].global_y = (players[i].global_y - (players[i].analog[1] - 128));  // ANALOG_Y
        // if (players[i].global_y > 128) players[i].global_y = 128;
        // if (players[i].global_y < -128) players[i].global_y = -128;
        players[i].analog[1] = 128;  // ANALOG_Y
      }
    }
    update_output();
  }
}

//
// update_output - updates i2c slave buffer with GPIO expander button bits
void __not_in_flash_func(update_output)(void)
{
  unsigned short int i;
  for (i = 0; i < playersCount; ++i)
  {
    // base controller buttons
    int16_t byte = (players[i].output_buttons & 0xffff);
    i2c_slave_read_buffer[0] = 0xFA;
    i2c_slave_read_buffer[0] ^= ((byte & USBR_BUTTON_B3) == 0) ? 0x02 : 0; // X
    i2c_slave_read_buffer[0] ^= ((byte & USBR_BUTTON_B4) == 0) ? 0x08 : 0; // Y
    i2c_slave_read_buffer[0] ^= ((byte & USBR_BUTTON_R1) == 0) ? 0x10 : 0; // R
    i2c_slave_read_buffer[0] ^= ((byte & USBR_BUTTON_L1) == 0) ? 0x20 : 0; // L
    i2c_slave_read_buffer[0] ^= ((byte & USBR_BUTTON_S2) == 0) ? 0x80 : 0; // MENU

    i2c_slave_read_buffer[1] = 0xFF;
    i2c_slave_read_buffer[1] ^= ((byte & USBR_BUTTON_DU) == 0) ? 0x02 : 0; // UP
    i2c_slave_read_buffer[1] ^= ((byte & USBR_BUTTON_DR) == 0) ? 0x04 : 0; // RIGHT
    i2c_slave_read_buffer[1] ^= ((byte & USBR_BUTTON_DD) == 0) ? 0x10 : 0; // DOWN
    i2c_slave_read_buffer[1] ^= ((byte & USBR_BUTTON_DL) == 0) ? 0x08 : 0; // LEFT
    i2c_slave_read_buffer[1] ^= ((byte & USBR_BUTTON_S1) == 0) ? 0x20 : 0; // VIEW
    i2c_slave_read_buffer[1] ^= ((byte & USBR_BUTTON_B1) == 0) ? 0x80 : 0; // A
  }

  codes_task();

  update_pending = true;
}

//
// post_input_event - NEW unified input event handler
//
void __not_in_flash_func(post_input_event)(const input_event_t* event)
{
  if (!event) return;

  int8_t instance = event->instance;
  bool is_extra = (instance == -1);
  if (is_extra) instance = 0;

  int player_index = find_player_index(event->dev_addr, instance);

  if (event->type == INPUT_TYPE_MOUSE) {
    uint16_t buttons_pressed = (~(event->buttons | 0x0f00));
    if (player_index < 0 && buttons_pressed) {
      printf("[add player] [%d, %d]\n", event->dev_addr, instance);
      player_index = add_player(event->dev_addr, instance);
    }

    if (player_index >= 0) {
      players[player_index].device_type = event->type;
      uint8_t delta_x = event->delta_x;
      uint8_t delta_y = event->delta_y;
      if (delta_x == 0) delta_x = 1;
      if (delta_y == 0) delta_y = 1;

      if (delta_x >= 128) {
        players[player_index].global_x -= (256 - delta_x);
      } else {
        players[player_index].global_x += delta_x;
      }
      delta_x = (players[player_index].global_x > 127) ? 0xff :
                (players[player_index].global_x < -127) ? 1 :
                128 + players[player_index].global_x;

      if (delta_y >= 128) {
        players[player_index].global_y -= (256 - delta_y);
      } else {
        players[player_index].global_y += delta_y;
      }
      delta_y = (players[player_index].global_y > 127) ? 0xff :
                (players[player_index].global_y < -127) ? 1 :
                128 + players[player_index].global_y;

      players[player_index].analog[0] = delta_x;
      players[player_index].analog[1] = delta_y;
      players[player_index].output_buttons = event->buttons;
      update_output();
    }
  } else {
    uint16_t buttons_pressed = (~(event->buttons | 0x800)) || event->keys;
    if (player_index < 0 && buttons_pressed) {
      printf("[add player] [%d, %d]\n", event->dev_addr, instance);
      player_index = add_player(event->dev_addr, instance);
    }

    if (player_index >= 0) {
      players[player_index].device_type = event->type;

      if (is_extra) {
        players[0].altern_buttons = event->buttons;
      } else {
        players[player_index].global_buttons = event->buttons;
      }

      // Maps View + Menu + Up combo to Guide button
      if (!((players[player_index].global_buttons) & XBOX_GUIDE_COMBO)) {
        players[player_index].global_buttons ^= USBR_BUTTON_A1;
        players[player_index].global_buttons |= XBOX_GUIDE_COMBO;
      }

      if (event->analog[0]) players[player_index].analog[0] = event->analog[0];
      if (event->analog[1]) players[player_index].analog[1] = event->analog[1];
      if (event->analog[2]) players[player_index].analog[2] = event->analog[2];
      if (event->analog[3]) players[player_index].analog[3] = event->analog[3];
      players[player_index].analog[5] = event->analog[5];
      players[player_index].analog[6] = event->analog[6];
      players[player_index].output_buttons = players[player_index].global_buttons & players[player_index].altern_buttons;

      if (!(players[player_index].output_buttons & USBR_BUTTON_R2)) {
        players[player_index].analog[6] = 255;
      }
      if (!(players[player_index].output_buttons & USBR_BUTTON_L2)) {
        players[player_index].analog[5] = 255;
      }
      update_output();
    }
  }
}
