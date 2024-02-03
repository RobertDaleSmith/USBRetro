// loopy.c

#include "loopy.h"

uint32_t output_word = 0;

// init for casio loopy communication
void loopy_init()
{
  stdio_init_all();

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
    bool isMouse = (!(player_1 & 0x000f)) || (!(player_2 & 0x000f)) ||
                   (!(player_3 & 0x000f)) || (!(player_4 & 0x000f));

    if (!isMouse) {
      // Gamepad output
      //
      //        bit0        bit1        bit2        bit3
      // ROW0   Presence    Start       Trigger-L   Trigger-R
      // ROW1   A           D           C           B
      // ROW2   Dpad-Up     Dpad-Down   Dpad-Left   Dpad-Right
      //

      if (gpio_get(ROW0_PIN)) {
        // Player 1 - ROW0
        gpio_put(BIT0_PIN, 1); // Presence
        gpio_put(BIT1_PIN, ((player_1 & 0x0080) == 0) ? 1 : 0); // Start
        gpio_put(BIT2_PIN, ((player_1 & 0x4000) == 0) ? 1 : 0); // L
        gpio_put(BIT3_PIN, ((player_1 & 0x8000) == 0) ? 1 : 0); // R

        // Player 2 - ROW0
        gpio_put(BIT4_PIN, 1); // Presence
        gpio_put(BIT5_PIN, ((player_2 & 0x0080) == 0) ? 1 : 0); // Start
        gpio_put(BIT6_PIN, ((player_2 & 0x4000) == 0) ? 1 : 0); // L
        gpio_put(BIT7_PIN, ((player_2 & 0x8000) == 0) ? 1 : 0); // R

      } else if (gpio_get(ROW1_PIN)) {
        // Player 1 - ROW1
        gpio_put(BIT0_PIN, ((player_1 & 0x0020) == 0) ? 1 : 0); // A
        gpio_put(BIT1_PIN, ((player_1 & 0x2000) == 0) ? 1 : 0); // D
        gpio_put(BIT2_PIN, ((player_1 & 0x1000) == 0) ? 1 : 0); // C
        gpio_put(BIT3_PIN, ((player_1 & 0x0010) == 0) ? 1 : 0); // B

        // Player 2 - ROW1
        gpio_put(BIT4_PIN, ((player_2 & 0x0020) == 0) ? 1 : 0); // A
        gpio_put(BIT5_PIN, ((player_2 & 0x2000) == 0) ? 1 : 0); // D
        gpio_put(BIT6_PIN, ((player_2 & 0x1000) == 0) ? 1 : 0); // C
        gpio_put(BIT7_PIN, ((player_2 & 0x0010) == 0) ? 1 : 0); // B

      } else if (gpio_get(ROW2_PIN)) {
        // Player 1 - ROW2
        gpio_put(BIT0_PIN, ((player_1 & 0x0001) == 0) ? 1 : 0); // Up
        gpio_put(BIT1_PIN, ((player_1 & 0x0004) == 0) ? 1 : 0); // Down
        gpio_put(BIT2_PIN, ((player_1 & 0x0008) == 0) ? 1 : 0); // Left
        gpio_put(BIT3_PIN, ((player_1 & 0x0002) == 0) ? 1 : 0); // Right

        // Player 2 - ROW2
        gpio_put(BIT4_PIN, ((player_2 & 0x0001) == 0) ? 1 : 0); // Up
        gpio_put(BIT5_PIN, ((player_2 & 0x0004) == 0) ? 1 : 0); // Down
        gpio_put(BIT6_PIN, ((player_2 & 0x0008) == 0) ? 1 : 0); // Left
        gpio_put(BIT7_PIN, ((player_2 & 0x0002) == 0) ? 1 : 0); // Right

      } else if (gpio_get(ROW3_PIN)) {
        // Player 3 - ROW0
        gpio_put(BIT0_PIN, 1); // Presence
        gpio_put(BIT1_PIN, ((player_3 & 0x0080) == 0) ? 1 : 0); // Start
        gpio_put(BIT2_PIN, ((player_3 & 0x4000) == 0) ? 1 : 0); // L
        gpio_put(BIT3_PIN, ((player_3 & 0x8000) == 0) ? 1 : 0); // R

        // Player 4 - ROW0
        gpio_put(BIT4_PIN, 1); // Presence
        gpio_put(BIT5_PIN, ((player_4 & 0x0080) == 0) ? 1 : 0); // Start
        gpio_put(BIT6_PIN, ((player_4 & 0x4000) == 0) ? 1 : 0); // L
        gpio_put(BIT7_PIN, ((player_4 & 0x8000) == 0) ? 1 : 0); // R

      } else if (gpio_get(ROW4_PIN)) {
        // Player 3 - ROW1
        gpio_put(BIT0_PIN, ((player_3 & 0x0020) == 0) ? 1 : 0); // A
        gpio_put(BIT1_PIN, ((player_3 & 0x2000) == 0) ? 1 : 0); // D
        gpio_put(BIT2_PIN, ((player_3 & 0x1000) == 0) ? 1 : 0); // C
        gpio_put(BIT3_PIN, ((player_3 & 0x0010) == 0) ? 1 : 0); // B

        // Player 4 - ROW1
        gpio_put(BIT4_PIN, ((player_4 & 0x0020) == 0) ? 1 : 0); // A
        gpio_put(BIT5_PIN, ((player_4 & 0x2000) == 0) ? 1 : 0); // D
        gpio_put(BIT6_PIN, ((player_4 & 0x1000) == 0) ? 1 : 0); // C
        gpio_put(BIT7_PIN, ((player_4 & 0x0010) == 0) ? 1 : 0); // B

      } else if (gpio_get(ROW5_PIN)) {
        // Player 3 - ROW2
        gpio_put(BIT0_PIN, ((player_3 & 0x0001) == 0) ? 1 : 0); // Up
        gpio_put(BIT1_PIN, ((player_3 & 0x0004) == 0) ? 1 : 0); // Down
        gpio_put(BIT2_PIN, ((player_3 & 0x0008) == 0) ? 1 : 0); // Left
        gpio_put(BIT3_PIN, ((player_3 & 0x0002) == 0) ? 1 : 0); // Right

        // Player 4 - ROW2
        gpio_put(BIT4_PIN, ((player_4 & 0x0001) == 0) ? 1 : 0); // Up
        gpio_put(BIT5_PIN, ((player_4 & 0x0004) == 0) ? 1 : 0); // Down
        gpio_put(BIT6_PIN, ((player_4 & 0x0008) == 0) ? 1 : 0); // Left
        gpio_put(BIT7_PIN, ((player_4 & 0x0002) == 0) ? 1 : 0); // Right
      } else {
        // Time between ROWs
        gpio_put(BIT0_PIN, 0);
        gpio_put(BIT1_PIN, 0);
        gpio_put(BIT2_PIN, 0);
        gpio_put(BIT3_PIN, 0);

        gpio_put(BIT4_PIN, 0);
        gpio_put(BIT5_PIN, 0);
        gpio_put(BIT6_PIN, 0);
        gpio_put(BIT7_PIN, 0);
      }

      // TODO: construct 48-bit output_word with this logic to reduce steps at this phase.
    } else {
      // Mouse output
      //
      // bit0     bit1     bit2     bit3     bit4     bit5     bit6     bit7
      // [X encoder raw]   [Y encoder raw]   Left     N/C      Right    Presence
      //

      gpio_put(BIT0_PIN, ((player_1 & 0x0008) == 0) ? 1 : 0); // X
      gpio_put(BIT1_PIN, ((player_1 & 0x0004) == 0) ? 1 : 0); // X
      gpio_put(BIT2_PIN, ((player_1 & 0x0001) == 0) ? 1 : 0); // Y
      gpio_put(BIT3_PIN, ((player_1 & 0x0004) == 0) ? 1 : 0); // Y

      gpio_put(BIT4_PIN, ((player_1 & 0x0020) == 0) ? 0 : 1); // Left 
      gpio_put(BIT5_PIN, 0);
      gpio_put(BIT6_PIN, ((player_1 & 0x0010) == 0) ? 0 : 1); // Right 
      gpio_put(BIT7_PIN, 1); // Presence

      // TODO: handle multiple mice + controllers.
    }

    update_output();

    unsigned short int i;
    for (i = 0; i < MAX_PLAYERS; ++i) {
      // decrement outputs from globals
      players[i].global_x = (players[i].global_x - players[i].output_analog_1x);
      players[i].global_y = (players[i].global_y - players[i].output_analog_1y);

      players[i].output_analog_1x = 0;
      players[i].output_analog_1y = 0;
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

  if (playersCount < 1)
  {
    bytes[0] = 0x00;
    bytes[1] = 0x00;
    bytes[2] = 0x00;
  } else {
    bytes[0] = 0x01;
    bytes[1] = players[0].output_buttons & 0x0f;
    bytes[2] = players[0].output_buttons & 0xf0;
  }

  bool isMouse = !(players[0].output_buttons & 0x000f);

  // mouse x/y states
  if (isMouse)
  {
    // TODO:
    int8_t mouse_byte = 0b10000000;

    bytes[0] = mouse_byte;
    bytes[1] = mouse_byte;
    bytes[2] = mouse_byte;
  }

  output_word = ((bytes[0] & 0xff))      | // ROW0
                ((bytes[1] & 0xff) << 8) | // ROW1
                ((bytes[2] & 0xff) << 16); // ROW2

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
  uint16_t buttons_pressed = (~(buttons | 0x0800)) || keys;
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

    if (delta_x >= 128)
      players[player_index].global_x = players[player_index].global_x - (256-delta_x);
    else
      players[player_index].global_x = players[player_index].global_x + delta_x;

    if (delta_y >= 128)
      players[player_index].global_y = players[player_index].global_y - (256-delta_y);
    else
      players[player_index].global_y = players[player_index].global_y + delta_y;

    players[player_index].output_analog_1x = players[player_index].global_x;
    players[player_index].output_analog_1y = players[player_index].global_y;
    players[player_index].output_buttons = players[player_index].global_buttons & players[player_index].altern_buttons;

    update_output();
  }
}
