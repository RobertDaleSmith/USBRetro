// pcengine.c

#include "pcengine.h"
#include "hardware/clocks.h"

// Definition of global variables
uint32_t output_analog_1x = 0;
uint32_t output_analog_1y = 0;
uint32_t output_analog_2x = 0;
uint32_t output_analog_2y = 0;

// When PCE reads, set interlock to ensure atomic update
//
volatile bool  output_exclude = false;

// output_word -> is the word sent to the state machine for output
//
// Structure of the word sent to the FIFO from the ARM:
// |  word_1|                             word_0
// |PLAYER_5|PLAYER_4|PLAYER_3|PLAYER_2|PLAYER_1
//
// 2-button mode byte: [Left, Down, Right, Up, Run, Select, II, I]
//  - all player button bytes are sent every cycle.
// 6-button mode byte: [III, IV, V, VI, 0, 0, 0, 0]
//  - every other cycle alternates between default
//    2-button byte and extended button byte.
// pce-mouse mode bytes:
//  - when mouse present, player buttons [Run, Select, II, I] are sent
//    as the most significant nybble. the least significant nybble holds
//    the x-axis and y-axis broken into nyybles sent over four cycles.
//    |CYCLE__4|CYCLE__3|CYCLE__2|CYCLE__1
//    |bbbbXXXX|bbbbxxxx|bbbbYYYY|bbbbyyyy
// where:
//  - b = button values, arranged in Run/Sel/II/I sequence for PC Engine use
//  - Xx = mouse 'x' movement; left is {1 - 0x7F} ; right is {0xFF - 0x80 }
//  - Yy = mouse 'y' movement;  up  is {1 - 0x7F} ; down  is {0xFF - 0x80 }
//
uint32_t output_word_0 = 0;
uint32_t output_word_1 = 0;

int state = 0; // countdown sequence for shift-register position

static absolute_time_t init_time;
static absolute_time_t current_time;
static absolute_time_t loop_time;
static const int64_t reset_period = 600; // at 600us, reset the scan exclude flag

// init for pcengine communication
void pce_init()
{
  // use turbo button feature with PCE
  turbo_init();

  pio = pio0; // Both state machines can run on the same PIO processor

  // Load the plex (multiplex output) program, and configure a free state machine
  // to run the program.

  uint offset1 = pio_add_program(pio, &plex_program);
  sm1 = pio_claim_unused_sm(pio, true);
  plex_program_init(pio, sm1, offset1, DATAIN_PIN, CLKIN_PIN, OUTD0_PIN);

  // Load the clock/select (synchronizing input) programs, and configure a free state machines
  // to run the programs.

  uint offset2 = pio_add_program(pio, &clock_program);
  sm2 = pio_claim_unused_sm(pio, true);
  clock_program_init(pio, sm2, offset2, CLKIN_PIN, OUTD0_PIN);

  uint offset3 = pio_add_program(pio, &select_program);
  sm3 = pio_claim_unused_sm(pio, true);
  select_program_init(pio, sm3, offset3, DATAIN_PIN);

  state = 3;

  output_word_0 = 0x00FFFFFFFF;  // no buttons pushed
  output_word_1 = 0x00000000FF;  // no buttons pushed

  init_time = get_absolute_time();
}

// init turbo button timings
void turbo_init()
{
    cpu_frequency = clock_get_hz(clk_sys);
    turbo_frequency = 1000000; // Default turbo frequency
    timer_threshold_a = cpu_frequency / (turbo_frequency * 2);
    timer_threshold_b = cpu_frequency / (turbo_frequency * 20);
    timer_threshold = timer_threshold_a;
}

// task process for checking pcengine polling cycles
void pce_task()
{
  //
  // check time offset in order to detect when a PCE scan is no longer
  // in process (so that fresh values can be sent to the state machine)
  //
  current_time = get_absolute_time();

  if (absolute_time_diff_us(init_time, current_time) > reset_period) {
    state = 3;
    update_output();
    output_exclude = false;
    init_time = get_absolute_time();
  }
}

//

//
// core1_entry - inner-loop for the second core
//             - when the "CLR" line is de-asserted, set lock flag
//               protecting the output state machine from inconsistent data
//
void __not_in_flash_func(core1_entry)(void)
{
  static bool rx_bit = 0;

  while (1)
  {
    // wait for (and sync with) negedge of CLR signal; rx_data is throwaway
    rx_bit = pio_sm_get_blocking(pio, sm2);

    // Now we are in an update-sequence; set a lock
    // to prevent update during output transaction
    output_exclude = true;

    // assume data is already formatted in output_word and push it to the state machine
    pio_sm_put(pio, sm1, output_word_1);
    pio_sm_put(pio, sm1, output_word_0);

    // Sequence from state 3 down through state 0 (show different nybbles to PCE)
    //
    // Note that when state = zero, it doesn't transition to a next state; the reset to
    // state 3 will happen as part of a timed process on the second CPU & state machine
    //

    // Also note that staying in 'scan' (CLK = low, SEL = high), is not expected
    // last more than about a half of a millisecond
    //
    loop_time = get_absolute_time();
    while ((gpio_get(CLKIN_PIN) == 0) && (gpio_get(DATAIN_PIN) == 1))
    {
      if (absolute_time_diff_us(loop_time, get_absolute_time()) > 550)
      {
        state = 0;
        break;
      }
    }

    if (state != 0)
    {
      state--;
      update_output();

      // renew countdown timeframe
      init_time = get_absolute_time();
    }
    else
    {
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

      output_exclude = true;            // continue to lock the output values (which are now zero)
    }
  }
}

//
// update_output - updates output_word with multi-tap plex data that
//                 is sent to PCE based on state and device types
//
void __not_in_flash_func(update_output)(void)
{

  static uint32_t turbo_timer = 0;
  static bool turbo_state = false;
  int8_t bytes[5] = { 0 };
  int16_t hotkey = 0;

  // Increment the timer and check if it reaches the threshold
  turbo_timer++;
  if (turbo_timer >= timer_threshold)
  {
    turbo_timer = 0;
    turbo_state = !turbo_state;
  }

  unsigned short int i;
  for (i = 0; i < MAX_PLAYERS; ++i)
  {
    // base controller/mouse buttons
    int8_t byte = (players[i].output_buttons & 0xff);

    if (i >= playersCount && !hotkey)
    {
      bytes[i] = 0xff;
      continue;
    }

    // check for 6-button enable/disable hotkeys
    if (!(players[i].output_buttons & (USBR_BUTTON_S2 | USBR_BUTTON_DU)))
      players[i].button_mode = BUTTON_MODE_6;
    else if (!(players[i].output_buttons & (USBR_BUTTON_S2 | USBR_BUTTON_DD)))
      players[i].button_mode = BUTTON_MODE_2;
    else if (!(players[i].output_buttons & (USBR_BUTTON_S2 | USBR_BUTTON_DR)))
      players[i].button_mode = BUTTON_MODE_3_SEL;
    else if (!(players[i].output_buttons & (USBR_BUTTON_S2 | USBR_BUTTON_DL)))
      players[i].button_mode = BUTTON_MODE_3_RUN;

    // Turbo EverDrive Pro hot-key fix
    if (hotkey)
    {
      byte &= hotkey;
    }
    else if (i == 0)
    {
      int16_t btns= (~players[i].output_buttons & 0xff);
      if     (btns == 0x82) hotkey = ~0x82; // RUN + RIGHT
      else if(btns == 0x88) hotkey = ~0x88; // RUN + LEFT
      else if(btns == 0x84) hotkey = ~0x84; // RUN + DOWN
    }

    bool has6Btn = !(players[i].output_buttons & 0x800);
    bool isMouse = !(players[i].output_buttons & 0x0f);
    bool is6btn = has6Btn && players[i].button_mode == BUTTON_MODE_6;
    bool is3btnSel = has6Btn && players[i].button_mode == BUTTON_MODE_3_SEL;
    bool is3btnRun = has6Btn && players[i].button_mode == BUTTON_MODE_3_RUN;

    // 6 button extra four buttons (III/IV/V/VI)
    if (is6btn)
    {
      if (state == 2)
      {
        byte = ((players[i].output_buttons>>8) & 0xf0);
      }
    }

    //
    else if (is3btnSel)
    {
      if ((~(players[i].output_buttons>>8)) & 0x30)
      {
        byte &= 0b01111111;
      }
    }

    //
    else if (is3btnRun)
    {
      if ((~(players[i].output_buttons>>8)) & 0x30)
      {
        byte &= 0b10111111;
      }
    }

    // Simulated Turbo buttons X/Y for II/I and L/R for speeds 1/2
    else {
      // Update the button state based on the turbo_state
      if (turbo_state)
      {
        // Set the button state as pressed
        if ((~(players[i].output_buttons>>8)) & 0x20) byte &= 0b11011111;
        if ((~(players[i].output_buttons>>8)) & 0x10) byte &= 0b11101111;
      }
      else
      {
        // Set the button state as released
      }

      if ((~(players[i].output_buttons>>8)) & 0x40) timer_threshold = timer_threshold_a;
      if ((~(players[i].output_buttons>>8)) & 0x80) timer_threshold = timer_threshold_b;
    }

    // mouse x/y states
    if (isMouse)
    {
      switch (state)
      {
        case 3: // state 3: x most significant nybble
          byte |= (((players[i].output_analog_1x>>1) & 0xf0) >> 4);
        break;
        case 2: // state 2: x least significant nybble
          byte |= (((players[i].output_analog_1x>>1) & 0x0f));
        break;
        case 1: // state 1: y most significant nybble
          byte |= (((players[i].output_analog_1y>>1) & 0xf0) >> 4);
        break;
        case 0: // state 0: y least significant nybble
          byte |= (((players[i].output_analog_1y>>1) & 0x0f));
        break;
      }
    }

    bytes[i] = byte;
  }

  output_word_0 = ((bytes[0] & 0xff))      | // player 1
                  ((bytes[1] & 0xff) << 8) | // player 2
                  ((bytes[2] & 0xff) << 16)| // player 3
                  ((bytes[3] & 0xff) << 24); // player 4
  output_word_1 = ((bytes[4] & 0xff));       // player 5

  codes_task();

  update_pending = true;
}


//
// post_globals - accumulate the many intermediate mouse scans (~1ms)
//                into an accumulator which will be reported back to PCE
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

    // TODO: 
    //  - Map home button to S1 + S2

    // TODO:
    //  - May need to output_exclude on 6-button?

    // if (!output_exclude || !isMouse)
    // {
      players[player_index].output_buttons = players[player_index].global_buttons & players[player_index].altern_buttons;

      // basic socd (up priority, left+right neutral)
      if (((~players[player_index].output_buttons) & 0x01) && ((~players[player_index].output_buttons) & 0x04)) {
        players[player_index].output_buttons ^= 0x04;
      }
      if (((~players[player_index].output_buttons) & 0x02) && ((~players[player_index].output_buttons) & 0x08)) {
        players[player_index].output_buttons ^= 0x0a;
      }

      update_output();
    // }
  }
}

//
// post_mouse_globals - accumulate the many intermediate mouse scans (~1ms)
//                into an accumulator which will be reported back to PCE
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

    if (!output_exclude)
    {
      players[player_index].output_analog_1x = players[player_index].global_x;
      players[player_index].output_analog_1y = players[player_index].global_y;
      players[player_index].output_buttons = players[player_index].global_buttons & players[player_index].altern_buttons;

      update_output();
    }
  }
}
