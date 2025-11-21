// 3do.c - 3DO console output implementation for USBRetro
// Based on FCare/USBTo3DO protocol implementation
// Adapted to USBRetro architecture

#include "3do.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/structs/bus_ctrl.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "pico/multicore.h"
#include <string.h>

#if CFG_TUSB_DEBUG >= 1
#include "hardware/uart.h"
#endif

// PIO state machines
PIO pio;
uint sm_sampling = 0;
uint sm_output = 0;

// Report buffers (initialize with 0xFF = all buttons not pressed in active-low logic)
uint8_t current_reports[MAX_PLAYERS][9];
uint8_t report_sizes[MAX_PLAYERS] = {0};
volatile bool device_attached[MAX_PLAYERS] = {false};
uint8_t controller_buffer[201];

// DMA channels
typedef enum {
  CHAN_OUTPUT = 0,
  CHAN_INPUT,
  CHAN_MAX
} DMA_chan_t;

int dma_channels[CHAN_MAX];
uint instr_jmp[CHAN_MAX];
dma_channel_config dma_config[CHAN_MAX];

uint8_t max_usb_controller = 0;
volatile bool update_report_flag = false;
volatile uint32_t pio_irq_count = 0;  // Track PIO IRQ calls (incremented in IRQ, read from task)

// Forward declarations
static void start_dma_transfer(uint8_t channel, uint8_t *buffer, uint32_t count);
static void report_done(uint8_t instance);

//-----------------------------------------------------------------------------
// DMA Setup Functions
//-----------------------------------------------------------------------------

void setup_3do_dma_output(void) {
  dma_channels[CHAN_OUTPUT] = dma_claim_unused_channel(true);
  dma_config[CHAN_OUTPUT] = dma_channel_get_default_config(dma_channels[CHAN_OUTPUT]);

  channel_config_set_transfer_data_size(&dma_config[CHAN_OUTPUT], DMA_SIZE_8);
  channel_config_set_read_increment(&dma_config[CHAN_OUTPUT], true);
  channel_config_set_write_increment(&dma_config[CHAN_OUTPUT], false);
  channel_config_set_irq_quiet(&dma_config[CHAN_OUTPUT], true);
  channel_config_set_dreq(&dma_config[CHAN_OUTPUT], DREQ_PIO1_TX0 + sm_output);

  dma_channel_set_write_addr(dma_channels[CHAN_OUTPUT], &pio1->txf[sm_output], false);
  dma_channel_set_config(dma_channels[CHAN_OUTPUT], &dma_config[CHAN_OUTPUT], false);

  // Set DMA bus priority
  bus_ctrl_hw->priority = BUSCTRL_BUS_PRIORITY_DMA_W_BITS | BUSCTRL_BUS_PRIORITY_DMA_R_BITS;
}

void setup_3do_dma_input(void) {
  dma_channels[CHAN_INPUT] = dma_claim_unused_channel(true);
  dma_config[CHAN_INPUT] = dma_channel_get_default_config(dma_channels[CHAN_INPUT]);

  channel_config_set_transfer_data_size(&dma_config[CHAN_INPUT], DMA_SIZE_8);
  channel_config_set_read_increment(&dma_config[CHAN_INPUT], false);
  channel_config_set_write_increment(&dma_config[CHAN_INPUT], true);
  channel_config_set_irq_quiet(&dma_config[CHAN_INPUT], true);
  channel_config_set_dreq(&dma_config[CHAN_INPUT], DREQ_PIO1_RX0 + sm_output);

  dma_channel_set_read_addr(dma_channels[CHAN_INPUT], &pio1->rxf[sm_output], false);
  dma_channel_set_config(dma_channels[CHAN_INPUT], &dma_config[CHAN_INPUT], false);

  // Set DMA bus priority
  bus_ctrl_hw->priority = BUSCTRL_BUS_PRIORITY_DMA_W_BITS | BUSCTRL_BUS_PRIORITY_DMA_R_BITS;
}

static void start_dma_transfer(uint8_t channel, uint8_t *buffer, uint32_t count) {
  if (channel == CHAN_OUTPUT) {
    dma_channel_transfer_from_buffer_now(dma_channels[channel], buffer, count);
  } else if (channel == CHAN_INPUT) {
    dma_channel_transfer_to_buffer_now(dma_channels[channel], buffer, count);
  }
}

//-----------------------------------------------------------------------------
// Report Management Functions
//-----------------------------------------------------------------------------

// Called after report is sent to clear relative data (e.g., mouse delta)
static void report_done(uint8_t instance) {
  if (instance >= MAX_PLAYERS) return;

  if (current_reports[instance][0] == 0x49) {
    // Mouse report - clear relative displacement to avoid continuous movement
    current_reports[instance][1] &= 0xF0;  // Keep buttons, clear dy_up
    current_reports[instance][2] = 0x00;    // Clear dx_up and dy_low
    current_reports[instance][3] = 0x00;    // Clear dx_low
  }
}

// PIO interrupt handler - triggered when CLK is high for 32 consecutive cycles
//
// NOTE: Current implementation uses buffered passthrough relay with one-poll delay (~16ms).
// Passthrough data read during this poll is stored and sent on the NEXT poll.
//
// Future enhancement: Implement real-time passthrough relay (zero latency) to match
// SNES23DO AVR implementation. See Phase 4, item #2 in 3DO_INTEGRATION_PLAN.md.
// Reference: /Users/robert/git/SNES23DO/code/SNES23DO/main.asm (lines 520-768)
//
void on_pio0_irq(void) {
  update_report_flag = true;
  pio_irq_count++;  // Fast counter increment (safe, no timing impact)

  // NOTE: Do NOT printf here! It breaks timing and kills passthrough.
  // Counter is printed from _3do_task() instead.

  // Abort any ongoing DMA transfers
  dma_channel_abort(dma_channels[CHAN_OUTPUT]);
  dma_channel_abort(dma_channels[CHAN_INPUT]);

  // Drain PIO FIFOs
  pio_sm_drain_tx_fifo(pio1, sm_output);
  while (!pio_sm_is_rx_fifo_empty(pio1, sm_output)) {
    pio_sm_get(pio1, sm_output);
  }

  // Restart PIO state machine
  pio_sm_restart(pio1, sm_output);
  pio_sm_exec(pio1, sm_output, instr_jmp[sm_output]);

  // Copy all USB controller reports to DMA buffer
  int total_report_size = 0;
  for (int i = 0; i < max_usb_controller; i++) {
    memcpy(&controller_buffer[total_report_size], &current_reports[i][0], report_sizes[i]);
    report_done(i);
    total_report_size += report_sizes[i];
  }

  // NOTE: Do NOT printf here! It breaks timing.
  // total_report_size can be logged from _3do_task() instead.

  // Start DMA transfers
  // OUTPUT: Sends USB controllers + buffered passthrough from previous poll
  start_dma_transfer(CHAN_OUTPUT, &controller_buffer[0], 201);
  pio_sm_set_enabled(pio1, sm_output, true);
  // INPUT: Reads new passthrough data (will be sent on NEXT poll)
  start_dma_transfer(CHAN_INPUT, &controller_buffer[total_report_size], 201 - total_report_size);

  // Clear PIO interrupt
  pio_interrupt_clear(pio1, 0);
  irq_clear(PIO1_IRQ_0);
}

//-----------------------------------------------------------------------------
// Report Constructor Functions
//-----------------------------------------------------------------------------

_3do_joypad_report new_3do_joypad_report(void) {
  _3do_joypad_report report;
  // Initialize to 0 to ensure clean state for bitfield struct
  memset(&report, 0, sizeof(report));
  report.id = 0b100;   // Standard gamepad ID
  report.tail = 0b00;  // Tail bits always 0
  // All buttons default to 1 (not pressed in active-low logic)
  report.A = 1; report.B = 1; report.C = 1; report.X = 1;
  report.L = 1; report.R = 1; report.P = 1;
  report.left = 1; report.right = 1; report.up = 1; report.down = 1;
  return report;
}

_3do_joystick_report new_3do_joystick_report(void) {
  _3do_joystick_report report;
  // Initialize to 0 to ensure clean state
  memset(&report, 0, sizeof(report));
  report.id_0 = 0x01;
  report.id_1 = 0x7B;
  report.id_2 = 0x08;
  report.tail = 0x00;
  // Initialize analog to center and buttons to not pressed (active-low = 1)
  report.analog1 = 128; report.analog2 = 128;
  report.analog3 = 128; report.analog4 = 128;
  report.A = 1; report.B = 1; report.C = 1; report.X = 1;
  report.L = 1; report.R = 1; report.P = 1; report.FIRE = 1;
  report.left = 1; report.right = 1; report.up = 1; report.down = 1;
  return report;
}

_3do_mouse_report new_3do_mouse_report(void) {
  _3do_mouse_report report;
  // Initialize to 0 to ensure clean state
  memset(&report, 0, sizeof(report));
  report.id = 0x49;
  return report;
}

//-----------------------------------------------------------------------------
// Report Update Functions
//-----------------------------------------------------------------------------

void update_3do_joypad(_3do_joypad_report report, uint8_t instance) {
  if (instance >= MAX_PLAYERS) return;

  memcpy(&current_reports[instance][0], &report, sizeof(_3do_joypad_report));
  report_sizes[instance] = 2;
  device_attached[instance] = true;

  max_usb_controller = (max_usb_controller < (instance + 1)) ? (instance + 1) : max_usb_controller;
}

void update_3do_joystick(_3do_joystick_report report, uint8_t instance) {
  if (instance >= MAX_PLAYERS) return;

  memcpy(&current_reports[instance][0], &report, sizeof(_3do_joystick_report));
  report_sizes[instance] = 9;
  device_attached[instance] = true;

  max_usb_controller = (max_usb_controller < (instance + 1)) ? (instance + 1) : max_usb_controller;
}

void update_3do_mouse(_3do_mouse_report report, uint8_t instance) {
  if (instance >= MAX_PLAYERS) return;

  memcpy(&current_reports[instance][0], &report, sizeof(_3do_mouse_report));
  report_sizes[instance] = 4;
  device_attached[instance] = true;

  max_usb_controller = (max_usb_controller < (instance + 1)) ? (instance + 1) : max_usb_controller;
}

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------

void _3do_init(void) {
  #if CFG_TUSB_DEBUG >= 1
  // Initialize UART for debugging
  uart_init(UART_ID, BAUD_RATE);
  gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
  gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
  printf("3DO protocol initializing...\n");
  #endif

  // Initialize report buffers with 0xFF (all buttons not pressed in active-low logic)
  memset(current_reports, 0xFF, sizeof(current_reports));
  memset(controller_buffer, 0xFF, sizeof(controller_buffer));

  // Use PIO1 to isolate 3DO protocol from ws2812 on PIO0
  pio = pio1;

  // Initialize CLK pin as input
  gpio_init(CLK_PIN);
  gpio_set_dir(CLK_PIN, GPIO_IN);

  // Set up PIO interrupt
  pio_set_irq0_source_enabled(pio1, pis_interrupt0, true);
  irq_set_exclusive_handler(PIO1_IRQ_0, on_pio0_irq);
  irq_set_enabled(PIO1_IRQ_0, true);

  // Load and initialize sampling program (CLK monitor)
  // Use dynamic claiming to avoid conflicts
  sm_sampling = pio_claim_unused_sm(pio1, true);
  uint offset_sampling = pio_add_program(pio1, &sampling_program);
  sampling_program_init(pio1, sm_sampling, offset_sampling);

  // Load and initialize output program (serial data output)
  sm_output = pio_claim_unused_sm(pio1, true);
  uint offset_output = pio_add_program(pio1, &output_program);
  output_program_init(pio1, sm_output, offset_output);

  instr_jmp[sm_output] = pio_encode_jmp(offset_output);

  // Setup DMA channels
  setup_3do_dma_output();
  setup_3do_dma_input();

  // Initialize GPIO pins for PIO
  pio_gpio_init(pio1, DATA_IN_PIN);
  gpio_pull_up(DATA_IN_PIN);
  pio_sm_set_consecutive_pindirs(pio1, sm_output, DATA_IN_PIN, 1, false);

  pio_gpio_init(pio1, DATA_OUT_PIN);
  pio_sm_set_consecutive_pindirs(pio1, sm_output, DATA_OUT_PIN, 1, true);

  #if CFG_TUSB_DEBUG >= 1
  printf("3DO protocol initialized successfully.\n");
  #endif

  // Note: Core1 is launched by main.c, not here
}

//-----------------------------------------------------------------------------
// Task Processing
//-----------------------------------------------------------------------------

// task process for 3DO (called from main loop)
void _3do_task() {
  // Periodic debug logging (safe to printf here, not in IRQ)
  #if CFG_TUSB_DEBUG >= 1
  static uint32_t last_log_time = 0;
  static uint32_t last_irq_count = 0;
  uint32_t now = to_ms_since_boot(get_absolute_time());

  if (now - last_log_time > 5000) {  // Log every 5 seconds
    uint32_t irq_delta = pio_irq_count - last_irq_count;
    printf("[3DO] IRQs: %lu (+%lu/5s), max_usb=%d, attached=[",
           pio_irq_count, irq_delta, max_usb_controller);
    for (int i = 0; i < MAX_PLAYERS; i++) {
      printf("%d", device_attached[i] ? 1 : 0);
    }
    printf("], sizes=[");
    for (int i = 0; i < MAX_PLAYERS; i++) {
      printf("%d,", report_sizes[i]);
    }
    printf("]\n");

    last_log_time = now;
    last_irq_count = pio_irq_count;
  }
  #endif
}

//-----------------------------------------------------------------------------
// Core1 Entry Point
//-----------------------------------------------------------------------------

void __not_in_flash_func(core1_entry)(void) {
  // Core1 can be used for periodic tasks if needed
  // For 3DO, most work is done in the interrupt handler
  while (1) {
    sleep_ms(100);
  }
}

//-----------------------------------------------------------------------------
// USB Input Integration - post_globals()
//-----------------------------------------------------------------------------

void __not_in_flash_func(update_3do_report)(uint8_t player_index) {
  if (player_index >= MAX_PLAYERS) return;

  uint32_t buttons = players[player_index].global_buttons;
  uint8_t ax = players[player_index].output_analog_1x;
  uint8_t ay = players[player_index].output_analog_1y;
  uint8_t az = players[player_index].output_analog_2x;
  uint8_t at = players[player_index].output_analog_2y;

  // DEBUG: Log button state when S1 or B4 is pressed
  static uint32_t debug_counter = 0;
  static uint32_t last_buttons = 0;

  // Only log when button state changes to reduce spam
  if (buttons != last_buttons && ((buttons & USBR_BUTTON_S1) || (buttons & USBR_BUTTON_B4))) {
    uint32_t s1_val = (buttons & USBR_BUTTON_S1);
    uint32_t b4_val = (buttons & USBR_BUTTON_B4);
    bool or_result = (s1_val || b4_val);

    printf("[DEBUG] buttons=0x%08X | S1=0x%08X(%d) | B4=0x%08X(%d) | OR=%d\n",
      buttons,
      s1_val, s1_val ? 1 : 0,
      b4_val, b4_val ? 1 : 0,
      or_result ? 1 : 0
    );
    last_buttons = buttons;
  } else if (!(buttons & (USBR_BUTTON_S1 | USBR_BUTTON_B4))) {
    last_buttons = buttons;  // Reset tracking when both buttons released
  }

  // Determine if this is a flight stick (has significant analog input)
  // TODO: Add better heuristics or device-type detection
  bool is_joystick = false;

  if (is_joystick) {
    // Send joystick report
    _3do_joystick_report report = new_3do_joystick_report();

    // Map analog axes
    report.analog1 = ax;
    report.analog2 = ay;
    report.analog3 = az;
    report.analog4 = at;

    // Map digital buttons (check == 0 because xinput sends inverted)
    // 3DO uses active-low: 0 = pressed, 1 = not pressed
    // Button mapping matches SNES23DO layout
    report.A = (buttons & USBR_BUTTON_B3) == 0 ? 1 : 0;  // B3 -> A
    report.B = (buttons & USBR_BUTTON_B1) == 0 ? 1 : 0;  // B1 -> B
    report.C = (buttons & USBR_BUTTON_B2) == 0 ? 1 : 0;  // B2 -> C
    report.X = ((buttons & USBR_BUTTON_B4) == 0 || (buttons & USBR_BUTTON_S1) == 0) ? 1 : 0;  // B4 OR S1 -> X
    report.L = ((buttons & USBR_BUTTON_L1) == 0 || (buttons & USBR_BUTTON_L2) == 0) ? 1 : 0;  // L1 OR L2 -> L
    report.R = ((buttons & USBR_BUTTON_R1) == 0 || (buttons & USBR_BUTTON_R2) == 0) ? 1 : 0;  // R1 OR R2 -> R
    report.P = (buttons & USBR_BUTTON_S2) == 0 ? 1 : 0;  // S2 -> P
    report.FIRE = (buttons & USBR_BUTTON_B1) == 0 ? 1 : 0;  // FIRE = B

    // Map D-pad (check == 0 because xinput sends inverted)
    report.left = (buttons & USBR_BUTTON_DL) == 0 ? 1 : 0;
    report.right = (buttons & USBR_BUTTON_DR) == 0 ? 1 : 0;
    report.up = (buttons & USBR_BUTTON_DU) == 0 ? 1 : 0;
    report.down = (buttons & USBR_BUTTON_DD) == 0 ? 1 : 0;

    update_3do_joystick(report, player_index);
  } else {
    // Send joypad report
    _3do_joypad_report report = new_3do_joypad_report();

    // Map buttons (check == 0 because xinput sends inverted: bit clear = pressed)
    // 3DO uses active-low: 0 = pressed, 1 = not pressed
    // Button mapping matches SNES23DO layout
    report.A = (buttons & USBR_BUTTON_B3) == 0 ? 1 : 0;  // B3 -> A
    report.B = (buttons & USBR_BUTTON_B1) == 0 ? 1 : 0;  // B1 -> B
    report.C = (buttons & USBR_BUTTON_B2) == 0 ? 1 : 0;  // B2 -> C
    report.X = ((buttons & USBR_BUTTON_B4) == 0 || (buttons & USBR_BUTTON_S1) == 0) ? 1 : 0;  // B4 OR S1 -> X
    report.L = ((buttons & USBR_BUTTON_L1) == 0 || (buttons & USBR_BUTTON_L2) == 0) ? 1 : 0;  // L1 OR L2 -> L
    report.R = ((buttons & USBR_BUTTON_R1) == 0 || (buttons & USBR_BUTTON_R2) == 0) ? 1 : 0;  // R1 OR R2 -> R
    report.P = (buttons & USBR_BUTTON_S2) == 0 ? 1 : 0;  // S2 -> P

    // Map D-pad (check == 0 because xinput sends inverted)
    report.left = (buttons & USBR_BUTTON_DL) == 0 ? 1 : 0;
    report.right = (buttons & USBR_BUTTON_DR) == 0 ? 1 : 0;
    report.up = (buttons & USBR_BUTTON_DU) == 0 ? 1 : 0;
    report.down = (buttons & USBR_BUTTON_DD) == 0 ? 1 : 0;

    // If no digital D-pad pressed, use analog stick (active-low)
    if (!(buttons & (USBR_BUTTON_DL | USBR_BUTTON_DR | USBR_BUTTON_DU | USBR_BUTTON_DD))) {
      report.left &= (ax < 64) ? 0 : 1;
      report.right &= (ax > 192) ? 0 : 1;
      report.up &= (ay < 64) ? 0 : 1;
      report.down &= (ay > 192) ? 0 : 1;
    }

    update_3do_joypad(report, player_index);
  }
}

void __not_in_flash_func(post_globals)(uint8_t dev_addr, int8_t instance,
  uint32_t buttons, uint8_t analog_1x, uint8_t analog_1y,
  uint8_t analog_2x, uint8_t analog_2y, uint8_t analog_l,
  uint8_t analog_r, uint32_t keys, uint8_t quad_x)
{
  int player_index = find_player_index(dev_addr, instance);

  // Invert buttons ONLY for checking if any button is pressed (for player registration)
  uint32_t buttons_pressed = (~(buttons | 0x800)) || keys;

  if (player_index == -1) {
    // First button press - register player
    if (buttons_pressed) {
      player_index = add_player(dev_addr, instance);
      #if CFG_TUSB_DEBUG >= 1
      printf("[3DO] New device: addr=%d inst=%d -> player %d\n", dev_addr, instance, player_index);
      #endif
    } else {
      return; // No buttons pressed, ignore
    }
  }

  if (player_index >= MAX_PLAYERS) {
    #if CFG_TUSB_DEBUG >= 1
    printf("[3DO] WARNING: Player index %d exceeds MAX_PLAYERS %d\n", player_index, MAX_PLAYERS);
    #endif
    return;
  }

  // Store RAW inverted buttons (like GameCube does)
  players[player_index].global_buttons = buttons;
  players[player_index].output_analog_1x = analog_1x;
  players[player_index].output_analog_1y = analog_1y;
  players[player_index].output_analog_2x = analog_2x;
  players[player_index].output_analog_2y = analog_2y;
  players[player_index].output_analog_l = analog_l;
  players[player_index].output_analog_r = analog_r;

  // Trigger report update
  update_3do_report(player_index);
}

void __not_in_flash_func(post_mouse_globals)(uint8_t dev_addr, int8_t instance,
  uint16_t buttons, uint8_t delta_x, uint8_t delta_y, uint8_t quad_x)
{
  int player_index = find_player_index(dev_addr, instance);

  if (player_index == -1) {
    player_index = add_player(dev_addr, instance);
  }

  if (player_index >= MAX_PLAYERS) return;

  // Accumulate deltas (similar to PCEngine mouse handling)
  int8_t dx = (int8_t)delta_x;
  int8_t dy = (int8_t)delta_y;

  if (dx >= 128) {
    players[player_index].global_x -= (256 - dx);
  } else {
    players[player_index].global_x += dx;
  }

  if (dy >= 128) {
    players[player_index].global_y -= (256 - dy);
  } else {
    players[player_index].global_y += dy;
  }

  // Build 3DO mouse report
  _3do_mouse_report mouse_report = new_3do_mouse_report();

  // Map buttons
  mouse_report.left = (buttons & USBR_BUTTON_B1) ? 1 : 0;
  mouse_report.right = (buttons & USBR_BUTTON_B2) ? 1 : 0;
  mouse_report.middle = (buttons & USBR_BUTTON_B3) ? 1 : 0;
  mouse_report.shift = (buttons & USBR_BUTTON_B4) ? 1 : 0;

  // Get accumulated deltas
  int16_t accumulated_dx = players[player_index].global_x;
  int16_t accumulated_dy = players[player_index].global_y;

  // Convert to 3DO format (10-bit split)
  if (accumulated_dx < 0) accumulated_dx |= 0x300;
  if (accumulated_dy < 0) accumulated_dy |= 0x300;

  mouse_report.dx_up = (accumulated_dx >> 8) & 0x3;
  mouse_report.dx_low = accumulated_dx & 0xFF;
  mouse_report.dy_up = (accumulated_dy >> 4) & 0xF;
  mouse_report.dy_low = accumulated_dy & 0x3F;

  update_3do_mouse(mouse_report, player_index);

  // Clear accumulated deltas after sending
  players[player_index].global_x = 0;
  players[player_index].global_y = 0;
}
