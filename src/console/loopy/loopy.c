// loopy.c

#include "loopy.h"

#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"

PIO pio;
uint sm1, sm2, sm3;

uint32_t output_word = 0;

// init for casio loopy communication
void loopy_init()
{
  stdio_init_all();

  // Initialize chosen UART
  uart_init(UART_ID, BAUD_RATE);

  // Set the GPIO function for the UART pins
  gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
  gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

  // Initialize stdio (redirects printf to UART)
  stdio_uart_init();

  pio = pio0; // Both state machines can run on the same PIO processor

  gpio_init(ROW0_PIN);
  gpio_init(ROW1_PIN);
  gpio_init(ROW2_PIN);
  gpio_init(ROW3_PIN);
  gpio_init(ROW4_PIN);
  gpio_init(ROW5_PIN);

  gpio_set_dir(ROW0_PIN, GPIO_IN);
  gpio_set_dir(ROW1_PIN, GPIO_IN);
  gpio_set_dir(ROW2_PIN, GPIO_IN);
  gpio_set_dir(ROW3_PIN, GPIO_IN);
  gpio_set_dir(ROW4_PIN, GPIO_IN);
  gpio_set_dir(ROW5_PIN, GPIO_IN);

  unsigned short int i;
  for (i = 0; i < 8; ++i) {
    gpio_init(BIT0_PIN + i);
    gpio_set_dir(BIT0_PIN + i, GPIO_OUT);
    gpio_put(BIT0_PIN + i, 0);
  }

  // Load the plex (multiplex output) program, and configure a free state machine
  // to run the program.

  // uint offset = pio_add_program(pio1, &loopy_program);
  // sm1 = pio_claim_unused_sm(pio, true);
  // loopy_program_init(pio, sm1, offset, ROW0_PIN, BIT0_PIN);

  // uint offset2 = pio_add_program(pio, &loopy_program);
  // sm2 = pio_claim_unused_sm(pio, true);
  // loopy_program_init(pio, sm2, offset2, ROW1_PIN, BIT0_PIN);

  // uint offset3 = pio_add_program(pio, &loopy_program);
  // sm3 = pio_claim_unused_sm(pio, true);
  // loopy_program_init(pio, sm3, offset3, ROW2_PIN, BIT0_PIN);

  output_word = 0;  // no buttons pushed
}

//
// core1_entry - inner-loop for the second core
//
void __not_in_flash_func(core1_entry)(void)
{
  static bool rx_bit = 0;

  while (1)
  {
    // 
    // rx_bit = pio_sm_get(pio, sm1);

    // assume data is already formatted in output_word and push it to the state machine
    // pio_sm_put(pio, sm1, output_word & 0x0f);
    // pio_sm_put(pio, sm2, (output_word << 4) & 0x0f);
    // pio_sm_put(pio, sm3, (output_word << 8) & 0x0f);
    // TODO: implement gamepad, mouse, multi-tap output with PIO.

    int16_t player_1 = (players[0].output_buttons & 0xffff);
    int16_t player_2 = (players[1].output_buttons & 0xffff);
    int16_t player_3 = (players[2].output_buttons & 0xffff);
    int16_t player_4 = (players[3].output_buttons & 0xffff);
    bool is_mouse = !(player_1 & 0x0f);
    // TODO: properly handle mouse detection at boot

    uint8_t loopy_byte = 0;

    // TODO: construct 48-bit output_word with this logic to reduce steps at this phase.
    if (!is_mouse) {
      // Gamepad output
      //
      //        bit0        bit1        bit2        bit3
      // ROW0   Presence    Start       Trigger-L   Trigger-R
      // ROW1   A           D           C           B
      // ROW2   Dpad-Up     Dpad-Down   Dpad-Left   Dpad-Right
      //

      if (gpio_get(ROW0_PIN)) {
        // Player 1 - ROW0
        loopy_byte |= LOOPY_BIT0; // Presence
        loopy_byte |= ((player_1 & USBR_BUTTON_S2) == 0) ? LOOPY_BIT1 : 0; // Start
        loopy_byte |= ((player_1 & USBR_BUTTON_L1) == 0) ? LOOPY_BIT2 : 0; // L
        loopy_byte |= ((player_1 & USBR_BUTTON_R1) == 0) ? LOOPY_BIT3 : 0; // R

        // Player 2 - ROW0
        loopy_byte |= (playersCount >= 2               ) ? LOOPY_BIT4 : 0; // Presence
        loopy_byte |= ((player_2 & USBR_BUTTON_S2) == 0) ? LOOPY_BIT5 : 0; // Start
        loopy_byte |= ((player_2 & USBR_BUTTON_L1) == 0) ? LOOPY_BIT6 : 0; // L
        loopy_byte |= ((player_2 & USBR_BUTTON_R1) == 0) ? LOOPY_BIT7 : 0; // R

      } else if (gpio_get(ROW1_PIN)) {
        // Player 1 - ROW1
        loopy_byte |= ((player_1 & USBR_BUTTON_B1) == 0) ? LOOPY_BIT0 : 0; // A
        loopy_byte |= ((player_1 & USBR_BUTTON_B4) == 0) ? LOOPY_BIT1 : 0; // D
        loopy_byte |= ((player_1 & USBR_BUTTON_B3) == 0) ? LOOPY_BIT2 : 0; // C
        loopy_byte |= ((player_1 & USBR_BUTTON_B2) == 0) ? LOOPY_BIT3 : 0; // B

        // Player 2 - ROW1
        loopy_byte |= ((player_2 & USBR_BUTTON_B1) == 0) ? LOOPY_BIT4 : 0; // A
        loopy_byte |= ((player_2 & USBR_BUTTON_B4) == 0) ? LOOPY_BIT5 : 0; // D
        loopy_byte |= ((player_2 & USBR_BUTTON_B3) == 0) ? LOOPY_BIT6 : 0; // C
        loopy_byte |= ((player_2 & USBR_BUTTON_B2) == 0) ? LOOPY_BIT7 : 0; // B

      } else if (gpio_get(ROW2_PIN)) {
        // Player 1 - ROW2
        loopy_byte |= ((player_1 & USBR_BUTTON_DU) == 0) ? LOOPY_BIT0 : 0; // Up
        loopy_byte |= ((player_1 & USBR_BUTTON_DD) == 0) ? LOOPY_BIT1 : 0; // Down
        loopy_byte |= ((player_1 & USBR_BUTTON_DL) == 0) ? LOOPY_BIT2 : 0; // Left
        loopy_byte |= ((player_1 & USBR_BUTTON_DR) == 0) ? LOOPY_BIT3 : 0; // Right

        // Player 2 - ROW2
        loopy_byte |= ((player_2 & USBR_BUTTON_DU) == 0) ? LOOPY_BIT4 : 0; // Up
        loopy_byte |= ((player_2 & USBR_BUTTON_DD) == 0) ? LOOPY_BIT5 : 0; // Down
        loopy_byte |= ((player_2 & USBR_BUTTON_DL) == 0) ? LOOPY_BIT6 : 0; // Left
        loopy_byte |= ((player_2 & USBR_BUTTON_DR) == 0) ? LOOPY_BIT7 : 0; // Right

      } else if (gpio_get(ROW3_PIN)) {
        // Player 3 - ROW0
        loopy_byte |= (playersCount >= 3               ) ? LOOPY_BIT0 : 0; // Presence
        loopy_byte |= ((player_3 & USBR_BUTTON_S2) == 0) ? LOOPY_BIT1 : 0; // Start
        loopy_byte |= ((player_3 & USBR_BUTTON_L1) == 0) ? LOOPY_BIT2 : 0; // L
        loopy_byte |= ((player_3 & USBR_BUTTON_R1) == 0) ? LOOPY_BIT3 : 0; // R

        // Player 4 - ROW0
        loopy_byte |= (playersCount >= 4               ) ? LOOPY_BIT4 : 0; // Presence
        loopy_byte |= ((player_4 & USBR_BUTTON_S2) == 0) ? LOOPY_BIT5 : 0; // Start
        loopy_byte |= ((player_4 & USBR_BUTTON_L1) == 0) ? LOOPY_BIT6 : 0; // L
        loopy_byte |= ((player_4 & USBR_BUTTON_R1) == 0) ? LOOPY_BIT7 : 0; // R

      } else if (gpio_get(ROW4_PIN)) {
        // Player 3 - ROW1
        loopy_byte |= ((player_3 & USBR_BUTTON_B1) == 0) ? LOOPY_BIT0 : 0; // A
        loopy_byte |= ((player_3 & USBR_BUTTON_B4) == 0) ? LOOPY_BIT1 : 0; // D
        loopy_byte |= ((player_3 & USBR_BUTTON_B3) == 0) ? LOOPY_BIT2 : 0; // C
        loopy_byte |= ((player_3 & USBR_BUTTON_B2) == 0) ? LOOPY_BIT3 : 0; // B

        // Player 4 - ROW1
        loopy_byte |= ((player_4 & USBR_BUTTON_B1) == 0) ? LOOPY_BIT4 : 0; // A
        loopy_byte |= ((player_4 & USBR_BUTTON_B4) == 0) ? LOOPY_BIT5 : 0; // D
        loopy_byte |= ((player_4 & USBR_BUTTON_B3) == 0) ? LOOPY_BIT6 : 0; // C
        loopy_byte |= ((player_4 & USBR_BUTTON_B2) == 0) ? LOOPY_BIT7 : 0; // B

      } else if (gpio_get(ROW5_PIN)) {
        // Player 3 - ROW2
        loopy_byte |= ((player_3 & USBR_BUTTON_DU) == 0) ? LOOPY_BIT0 : 0; // Up
        loopy_byte |= ((player_3 & USBR_BUTTON_DD) == 0) ? LOOPY_BIT1 : 0; // Down
        loopy_byte |= ((player_3 & USBR_BUTTON_DL) == 0) ? LOOPY_BIT2 : 0; // Left
        loopy_byte |= ((player_3 & USBR_BUTTON_DR) == 0) ? LOOPY_BIT3 : 0; // Right

        // Player 4 - ROW2
        loopy_byte |= ((player_4 & USBR_BUTTON_DU) == 0) ? LOOPY_BIT4 : 0; // Up
        loopy_byte |= ((player_4 & USBR_BUTTON_DD) == 0) ? LOOPY_BIT5 : 0; // Down
        loopy_byte |= ((player_4 & USBR_BUTTON_DL) == 0) ? LOOPY_BIT6 : 0; // Left
        loopy_byte |= ((player_4 & USBR_BUTTON_DR) == 0) ? LOOPY_BIT7 : 0; // Right
      }

    } else {
      // Mouse output
      //
      // bit0     bit1     bit2     bit3     bit4     bit5     bit6     bit7
      // [X encoder raw]   [Y encoder raw]   Left     N/C      Right    Presence
      //

      uint8_t x_gray = players[0].output_analog_1x;
      uint8_t y_gray = players[0].output_analog_1y;
      // printf("[raw_gray_code] [%d, %d]\n", x_gray, y_gray);

      loopy_byte |= ((x_gray)      & 0x1             ) ? LOOPY_BIT0 : 0; // X
      loopy_byte |= ((x_gray >> 1) & 0x1             ) ? LOOPY_BIT1 : 0; // X
      loopy_byte |= ((y_gray)      & 0x1             ) ? LOOPY_BIT2 : 0; // Y
      loopy_byte |= ((y_gray >> 1) & 0x1             ) ? LOOPY_BIT3 : 0; // Y
      loopy_byte |= ((player_1 & USBR_BUTTON_B1) == 0) ? LOOPY_BIT4 : 0; // Left
      loopy_byte |= ((0)                             ) ? LOOPY_BIT5 : 0; // N/C
      loopy_byte |= ((player_1 & USBR_BUTTON_B2) == 0) ? LOOPY_BIT6 : 0; // Right
      loopy_byte |= ((1)                             ) ? LOOPY_BIT7 : 0; // Presence
    }

    gpio_put(BIT0_PIN, (loopy_byte & LOOPY_BIT0) ? 1 : 0);
    gpio_put(BIT1_PIN, (loopy_byte & LOOPY_BIT1) ? 1 : 0);
    gpio_put(BIT2_PIN, (loopy_byte & LOOPY_BIT2) ? 1 : 0);
    gpio_put(BIT3_PIN, (loopy_byte & LOOPY_BIT3) ? 1 : 0);
    gpio_put(BIT4_PIN, (loopy_byte & LOOPY_BIT4) ? 1 : 0);
    gpio_put(BIT5_PIN, (loopy_byte & LOOPY_BIT5) ? 1 : 0);
    gpio_put(BIT6_PIN, (loopy_byte & LOOPY_BIT6) ? 1 : 0);
    gpio_put(BIT7_PIN, (loopy_byte & LOOPY_BIT7) ? 1 : 0);

    update_output();

    unsigned short int i;
    for (i = 0; i < MAX_PLAYERS; ++i) {
      // decrement outputs from globals
      // players[i].global_x = (players[i].global_x - players[i].output_analog_1x);
      // players[i].global_y = (players[i].global_y - players[i].output_analog_1y);

      // players[i].output_analog_1x = 0;
      // players[i].output_analog_1y = 0;
      players[i].output_buttons = players[i].global_buttons & players[i].altern_buttons;
    }
  }
}

//
// update_output - updates output_word with loopy data that is sent
//                 to the console based on ROW state and device type
//
void __not_in_flash_func(update_output)(void)
{
  int8_t bytes[4] = { 0 };

  // unsigned short int i;
  // for (i = 0; i < MAX_PLAYERS; ++i)
  // {
  // }

  // TODO: parse USBRetro button state to output_word here
  // if (playersCount < 1)
  // {
  //   bytes[0] = 0x00;
  //   bytes[1] = 0x00;
  //   bytes[2] = 0x00;
  // } else {
  //   bytes[0] = 0x01;
  //   bytes[1] = players[0].output_buttons & 0x0f;
  //   bytes[2] = players[0].output_buttons & 0xf0;
  // }

  // bool isMouse = !(players[0].output_buttons & 0x0f);

  // // mouse x/y states
  // if (isMouse)
  // {
  //   // TODO:
  //   int8_t mouse_byte = 0b10000000;

  //   bytes[0] = mouse_byte;
  //   bytes[1] = mouse_byte;
  //   bytes[2] = mouse_byte;
  // }

  // output_word = ((bytes[0] & 0xff))      | // ROW0
  //               ((bytes[1] & 0xff) << 8) | // ROW1
  //               ((bytes[2] & 0xff) << 16); // ROW2

  codes_task();
}

//
// post_globals - accumulate the many intermediate mouse scans (~1ms)
//                into an accumulator which will be reported back to loopy
//
void __not_in_flash_func(post_globals)(
  uint8_t dev_addr, int8_t instance, uint32_t buttons,
  uint8_t analog_1x, uint8_t analog_1y, uint8_t analog_2x,
  uint8_t analog_2y, uint8_t analog_l, uint8_t analog_r,
  uint32_t keys, uint8_t quad_x)
{
  // for merging extra device instances into the root instance (ex: joycon charging grip)
  bool is_extra = (instance == -1);
  if (is_extra) instance = 0;

  int player_index = find_player_index(dev_addr, instance);
  uint16_t buttons_pressed = (~(buttons | 0x800)) || keys;
  if (player_index < 0 && buttons_pressed)
  {
    printf("[add player] [%d, %d]\n", dev_addr, instance);
    player_index = add_player(dev_addr, instance);
  }

  // printf("[player_index] [%d] [%d, %d]\n", player_index, dev_addr, instance);

  if (player_index >= 0)
  {
    // map analog to dpad movement here
    uint8_t dpad_offset = 32;
    if (analog_1x)
    {
      if (analog_1x > 128 + dpad_offset) buttons &= ~(0x02); // right
      else if (analog_1x < 128 - dpad_offset) buttons &= ~(0x08); // left
    }
    if (analog_1y)
    {
      if (analog_1y > 128 + dpad_offset) buttons &= ~(0x01); // up
      else if (analog_1y < 128 - dpad_offset) buttons &= ~(0x04); // down
    }

    // extra instance buttons to merge with root player
    if (is_extra)
    {
      players[0].altern_buttons = buttons;
    }
    else
    {
      players[player_index].global_buttons = buttons;
    }

    players[player_index].output_buttons = players[player_index].global_buttons & players[player_index].altern_buttons;

    // basic socd (up priority, left+right neutral)
    if (((~players[player_index].output_buttons) & 0x01) && ((~players[player_index].output_buttons) & 0x04)) {
      players[player_index].output_buttons ^= 0x04;
    }
    if (((~players[player_index].output_buttons) & 0x02) && ((~players[player_index].output_buttons) & 0x08)) {
      players[player_index].output_buttons ^= 0x0a;
    }

    update_output();
  }
}

//
// post_mouse_globals - accumulate the many intermediate mouse scans (~1ms)
//                into an accumulator which will be reported back to loopy
//
void __not_in_flash_func(post_mouse_globals)(
  uint8_t dev_addr, int8_t instance, uint16_t buttons,
  uint8_t delta_x, uint8_t delta_y, uint8_t quad_x)
{
  // for merging extra device instances into the root instance (ex: joycon charging grip)
  bool is_extra = (instance == -1);
  if (is_extra) instance = 0;

  int player_index = find_player_index(dev_addr, instance);
  uint16_t buttons_pressed = (~(buttons | 0x0f00));
  if (player_index < 0 && buttons_pressed)
  {
    printf("[add player] [%d, %d]\n", dev_addr, instance);
    player_index = add_player(dev_addr, instance);
  }

  // printf("[player_index] [%d] [%d, %d]\n", player_index, dev_addr, instance);

  if (player_index >= 0)
  {
    players[player_index].global_buttons = buttons;

    // TODO: continue to parse mouse movement into RAW gray code correctly.
    //      [0, 1, 3, 2 ... for one direction and 2, 3, 1, 0 ... for other]
    if (delta_x >= 128) {
      // x-axis moving left
      switch (players[player_index].output_analog_1x)
      {
      case 2:
        players[player_index].output_analog_1x = 3;
        break;
      case 3:
        players[player_index].output_analog_1x = 1;
        break;
      case 1:
        players[player_index].output_analog_1x = 0;
        break;
      case 0:
        players[player_index].output_analog_1x = 2;
        break;
      default:
        break;
      }
    } else {
      // x-axis moving right
      switch (players[player_index].output_analog_1x)
      {
      case 0:
        players[player_index].output_analog_1x = 1;
        break;
      case 1:
        players[player_index].output_analog_1x = 3;
        break;
      case 3:
        players[player_index].output_analog_1x = 2;
        break;
      case 2:
        players[player_index].output_analog_1x = 0;
        break;
      default:
        break;
      }
    }

    if (delta_y >= 128) {
      // y-axis moving up
      switch (players[player_index].output_analog_1y)
      {
      case 2:
        players[player_index].output_analog_1y = 3;
        break;
      case 3:
        players[player_index].output_analog_1y = 1;
        break;
      case 1:
        players[player_index].output_analog_1y = 0;
        break;
      case 0:
        players[player_index].output_analog_1y = 2;
        break;
      default:
        break;
      }
    } else {
      // y-axis moving down
      switch (players[player_index].output_analog_1y)
      {
      case 0:
        players[player_index].output_analog_1y = 1;
        break;
      case 1:
        players[player_index].output_analog_1y = 3;
        break;
      case 3:
        players[player_index].output_analog_1y = 2;
        break;
      case 2:
        players[player_index].output_analog_1y = 0;
        break;
      default:
        break;
      }
    }

    players[player_index].output_buttons = players[player_index].global_buttons & players[player_index].altern_buttons;

    update_output();
  }
}
