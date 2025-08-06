// nuon.c

#include "nuon.h"

PIO pio;
uint sm1, sm2;
int crc_lut[256]; // crc look up table

// Definition of global variables
uint32_t output_buttons_0 = 0;
uint32_t output_analog_1x = 0;
uint32_t output_analog_1y = 0;
uint32_t output_analog_2x = 0;
uint32_t output_analog_2y = 0;
uint32_t output_quad_x = 0;

uint32_t device_mode   = 0b10111001100000111001010100000000;
uint32_t device_config = 0b10000000100000110000001100000000;
uint32_t device_switch = 0b10000000100000110000001100000000;

bool softReset = false;
uint32_t pressTime = 0;
const uint32_t requiredHoldDuration = 2000; // Duration in milliseconds for which the button combination must be held

// init for nuon communication
void nuon_init(void)
{
  output_buttons_0 = 0b00000000100000001000001100000011; // no buttons pressed
  output_analog_1x = 0b10000000100000110000001100000000; // x1 = 0
  output_analog_1y = 0b10000000100000110000001100000000; // y1 = 0
  output_analog_2x = 0b10000000100000110000001100000000; // x2 = 0
  output_analog_2y = 0b10000000100000110000001100000000; // y2 = 0
  output_quad_x = 0b10000000000000000000000000000000; // quadx = 0

  // PROPERTIES DEV____MOD DEV___CONF DEV____EXT // CTRL_VALUES from SDK joystick.h
  // 0x0000001f 0b10111001 0b10000000 0b10000000 // ANALOG1, STDBUTTONS, DPAD, SHOULDER, EXTBUTTONS
  // 0x0000003f 0b10000000 0b01000000 0b01000000 // ANALOG1, ANALOG2, STDBUTTONS, DPAD, SHOULDER, EXTBUTTONS
  // 0x0000011d 0b11000000 0b00000000 0b10000000 // THROTTLE, ANALOG1, STDBUTTONS, SHOULDER, EXTBUTTONS
  // 0x0000011f 0b11000000 0b01000000 0b00010000 // THROTTLE, ANALOG1, STDBUTTONS, DPAD, SHOULDER, EXTBUTTONS
  // 0x0000014f 0b11010000 0b00000000 0b00000000 // THROTTLE, WHEEL|PADDLE, STDBUTTONS, DPAD, SHOULDER, EXTBUTTONS
  // 0x00000300 0b11000000 0b00000000 0b11000000 // BRAKE, THROTTLE
  // 0x00000341 0b11000000 0b00000000 0b00000000 // BRAKE, THROTTLE, WHEEL|PADDLE, STDBUTTONS
  // 0x0000034f 0b10111001 0b10000000 0b00000000 // BRAKE, THROTTLE, WHEEL|PADDLE, STDBUTTONS, DPAD, SHOULDER, EXTBUTTONS
  // 0x0000041d 0b11000000 0b11000000 0b00000000 // RUDDER|TWIST, ANALOG1, STDBUTTONS, DPAD, EXTBUTTONS
  // 0x00000513 0b10000000 0b00000000 0b00000000 // RUDDER|TWIST, THROTTLE, ANALOG1, DPAD, STDBUTTONS
  // 0x0000051f 0b10000000 0b10000000 0b10000000 // RUDDER|TWIST, THROTTLE, ANALOG1, STDBUTTONS, DPAD, SHOULDER, EXTBUTTONS
  // 0x00000800 0b11010000 0b00000000 0b10000000 // MOUSE|TRACKBALL
  // 0x00000808 0b11010000 0b10000000 0b10000000 // MOUSE|TRACKBALL, EXTBUTTONS
  // 0x00000811 0b11001000 0b00010000 0b00010000 // MOUSE|TRACKBALL, ANALOG1, STDBUTTONS
  // 0x00000815 0b11001000 0b11000000 0b00010000 // MOUSE|TRACKBALL, ANALOG1, STDBUTTONS, SHOULDER
  // 0x0000083f 0b10011101 0b10000000 0b10000000 // MOUSE|TRACKBALL, ANALOG1, ANALOG2, STDBUTTONS, DPAD, SHOULDER, EXTBUTTONS
  // 0x0000103f 0b10011101 0b11000000 0b11000000 // QUADSPINNER1, ANALOG1, ANALOG2, STDBUTTONS, DPAD, SHOULDER, EXTBUTTONS
  // 0x0000101f 0b10111001 0b10000000 0b01000000 // QUADSPINNER1, ANALOG1, STDBUTTONS, DPAD, SHOULDER, EXTBUTTONS
  // 0x00001301 0b11000000 0b11000000 0b11000000 // QUADSPINNER1, BRAKE, THROTTLE, STDBUTTONS
  // 0x0000401d 0b11010000 0b01000000 0b00010000 // THUMBWHEEL1, ANALOG1, STDBUTTONS, SHOULDER, EXTBUTTONS
  // 0x0000451b 0b10011101 0b00000000 0b00000000 // THUMBWHEEL1, RUDDER|TWIST, THROTTLE, STDBUTTONS, DPAD, EXTBUTTONS
  // 0x0000c011 0b10111001 0b11000000 0b01000000 // THUMBWHEEL1, THUMBWHEEL2, ANALOG1, STDBUTTONS
  // 0x0000c01f 0b11000000 0b00000000 0b01000000 // THUMBWHEEL1, THUMBWHEEL2, ANALOG1, STDBUTTONS, DPAD, SHOULDER, EXTBUTTONS
  // 0x0000c03f 0b10011101 0b01000000 0b01000000 // THUMBWHEEL1, THUMBWHEEL2, ANALOG1, ANALOG2, STDBUTTONS, DPAD, SHOULDER, EXTBUTTONS
  // 0x0000c51b 0b10000000 0b11000000 0b11000000 // THUMBWHEEL1, THUMBWHEEL2, RUDDER|TWIST, THROTTLE, ANALOG1, STDBUTTONS, DPAD, EXTBUTTONS
  // 0x0001001d 0b11000000 0b11000000 0b10000000 // FISHINGREEL, ANALOG1, STDBUTTONS, SHOULDER, EXTBUTTONS

  // Sets packets that define device properties
  device_mode   = crc_data_packet(0b10011101, 1);
  device_config = crc_data_packet(0b11000000, 1);
  device_switch = crc_data_packet(0b11000000, 1);

  pio = pio0; // Both state machines can run on the same PIO processor

  // Load the read and write programs, and configure a free state machines
  uint offset2 = pio_add_program(pio, &polyface_read_program);
  sm2 = pio_claim_unused_sm(pio, true);
  polyface_read_program_init(pio, sm2, offset2, DATAIO_PIN);

  uint offset1 = pio_add_program(pio1, &polyface_send_program);
  sm1 = pio_claim_unused_sm(pio1, true);
  polyface_send_program_init(pio1, sm1, offset1, DATAIO_PIN);

  // queue_init(&packet_queue, sizeof(int64_t), 1000);
}

// maps default usbretro button bit order to nuon's button packet data structure
uint32_t map_nuon_buttons(uint32_t buttons)
{
  uint32_t nuon_buttons = 0x0080;

  // Mapping the buttons from the old format to the new format, inverting the logic
  nuon_buttons |= (!(buttons & USBR_BUTTON_B2)) ? NUON_BUTTON_C_DOWN : 0;  // Circle -> C-DOWN
  nuon_buttons |= (!(buttons & USBR_BUTTON_B1)) ? NUON_BUTTON_A  : 0;  // Cross -> A
  nuon_buttons |= (!(buttons & USBR_BUTTON_S2)) ? NUON_BUTTON_START : 0;  // Option -> START
  nuon_buttons |= (!(buttons & USBR_BUTTON_S1)) ? NUON_BUTTON_NUON : 0;  // Share -> NUON/Z
  nuon_buttons |= (!(buttons & USBR_BUTTON_DD)) ? NUON_BUTTON_DOWN : 0;  // Dpad Down -> D-DOWN
  nuon_buttons |= (!(buttons & USBR_BUTTON_DL)) ? NUON_BUTTON_LEFT : 0;  // Dpad Left -> D-LEFT
  nuon_buttons |= (!(buttons & USBR_BUTTON_DU)) ? NUON_BUTTON_UP : 0;  // Dpad Up -> D-UP
  nuon_buttons |= (!(buttons & USBR_BUTTON_DR)) ? NUON_BUTTON_RIGHT : 0;  // Dpad Right -> D-RIGHT
  // Skipping the two buttons represented by 0x0080 and 0x0040 in the new format
  nuon_buttons |= (!(buttons & USBR_BUTTON_L1)) ? NUON_BUTTON_L : 0;  // L1 -> L
  nuon_buttons |= (!(buttons & USBR_BUTTON_R1)) ? NUON_BUTTON_R : 0;  // R1 -> R
  nuon_buttons |= (!(buttons & USBR_BUTTON_B3)) ? NUON_BUTTON_B : 0;  // Square -> B
  nuon_buttons |= (!(buttons & USBR_BUTTON_B4)) ? NUON_BUTTON_C_LEFT : 0;  // Triangle -> C-LEFT
  nuon_buttons |= (!(buttons & USBR_BUTTON_L2)) ? NUON_BUTTON_C_UP : 0;  // L2 -> C-UP
  nuon_buttons |= (!(buttons & USBR_BUTTON_R2)) ? NUON_BUTTON_C_RIGHT : 0;  // R2 -> C-RIGHT

  return nuon_buttons;
}

uint8_t eparity(uint32_t data)
{
  uint32_t eparity;
  eparity = (data>>16)^data;
  eparity ^= (eparity>>8);
  eparity ^= (eparity>>4);
  eparity ^= (eparity>>2);
  eparity ^= (eparity>>1);
  return ((eparity)&0x1);
}

// generates data response packet with crc check bytes
uint32_t crc_data_packet(int32_t value, int8_t size)
{
  uint32_t packet = 0;
  uint16_t crc = 0;

  // calculate crc and place bytes into packet position
  for (int i=0; i<size; i++)
  {
    uint8_t byte_val = (((value>>((size-i-1)*8)) & 0xff));
    crc = (crc_calc(byte_val, crc) & 0xffff);
    packet |= (byte_val << ((3-i)*8));
  }

  // place crc check bytes in packet position
  packet |= (crc << ((2-size)*8));

  return (packet);
}

int crc_build_lut()
{
	int i,j,k;
	for (i=0; i<256; i++)
  {
		for(j=i<<8,k=0; k<8; k++)
    {
			j=(j&0x8000) ? (j<<1)^CRC16 : (j<<1); crc_lut[i]=j;
		}
	}
	return(0);
}

int crc_calc(unsigned char data, int crc)
{
	if (crc_lut[1]==0) crc_build_lut();
	return(((crc_lut[((crc>>8)^data)&0xff])^(crc<<8))&0xffff);
}

void trigger_button_press(uint8_t pin)
{
  // Configure the button pin as output
  gpio_init(pin);
  gpio_set_dir(pin, GPIO_OUT);

  // Set the button pin to low
  gpio_put(pin, 0);

  // Wait for a brief moment
  sleep_ms(100); // Wait for 100 milliseconds

  // Reconfigure the button pin as an input
  gpio_set_dir(pin, GPIO_IN);
}

void nuon_task()
{
  // Calculate and set Nuon output packet values here.
  int32_t buttons = (players[0].output_buttons & 0xffff) |
                    (players[0].output_buttons_alt & 0xffff);

  // Check for button combination (Nuon + Start + L + R)
  if ((buttons & 0x3030) == 0x3030) {
    if (!softReset) {
      softReset = true;
      pressTime = to_ms_since_boot(get_absolute_time()); // Start timing when buttons are pressed
    } else {
      uint32_t holdDuration = to_ms_since_boot(get_absolute_time()) - pressTime;
      if (holdDuration >= requiredHoldDuration) {
        // long press and release
        trigger_button_press(POWER_PIN);
        softReset = false;
        pressTime = 0; // Reset pressTime for next button press
      }
    }
  } else if (softReset) {
    // quick press and release
    trigger_button_press(STOP_PIN);
    softReset = false;
  }
}

//
// core1_entry - inner-loop for the second core
void __not_in_flash_func(core1_entry)(void)
{
  uint64_t packet = 0;
  uint16_t state = 0;
  uint8_t channel = 0;
  uint8_t id = 0;
  bool alive = false;
  bool tagged = false;
  bool branded = false;
  int requestsB = 0;

  while (1)
  {
    packet = 0;
    for (int i = 0; i < 2; ++i)
    {
      uint32_t rxdata = pio_sm_get_blocking(pio, sm2);
      packet = ((packet) << 32) | (rxdata & 0xFFFFFFFF);
    }

    // queue_try_add(&packet_queue, &packet);

    uint8_t dataA = ((packet>>17) & 0b11111111);
    uint8_t dataS = ((packet>>9) & 0b01111111);
    uint8_t dataC = ((packet>>1) & 0b01111111);
    uint8_t type0 = ((packet>>25) & 0b00000001);
    uint32_t word0 = 1;
    uint32_t word1 = 0;

    if ((dataA == 0xb1 && dataS == 0x00 && dataC == 0x00) || // RESET
        (alive && !playersCount) // USB controller disconnected
    ) {
      id = 0;
      alive = false;
      tagged = false;
      branded = false;
      state = 0;
      channel = 0;
    }

    // No response unless USB controller connected
    if (!playersCount) continue;

    if (dataA == 0x80) // ALIVE
    {
      word0 = 1;
      word1 = __rev(0b01);
      if (alive) word1 = __rev(((id & 0b01111111) << 1));
      else alive = true;

      pio_sm_put_blocking(pio1, sm1, word1);
      pio_sm_put_blocking(pio1, sm1, word0);
    }
    else if (dataA == 0x88 && dataS == 0x04 && dataC == 0x40) // ERROR
    {
      word0 = 1;
      word1 = 0;
      pio_sm_put_blocking(pio1, sm1, word1);
      pio_sm_put_blocking(pio1, sm1, word0);
    }
    else if (dataA == 0x90 && !branded) // MAGIC
    {
      word0 = 1;
      word1 = __rev(MAGIC);
      pio_sm_put_blocking(pio1, sm1, word1);
      pio_sm_put_blocking(pio1, sm1, word0);
    }
    else if (dataA == 0x94) // PROBE
    {
      word0 = 1; // default res from HPI controller
      word1 = __rev(0b10001011000000110000000000000000);

      //DEFCFG VERSION     TYPE      MFG TAGGED BRANDED    ID P
      //   0b1 0001011 00000011 00000000      0       0 00000 0
      word1 = ((DEFCFG  & 1)<<31) |
              ((VERSION & 0b01111111)<<24) |
              ((TYPE    & 0b11111111)<<16) |
              ((MFG     & 0b11111111)<<8) |
              (((tagged ? 1:0) & 1)<<7) |
              (((branded? 1:0) & 1)<<6) |
              ((id      & 0b00011111)<<1);
      word1 = __rev(word1 | eparity(word1));

      pio_sm_put_blocking(pio1, sm1, word1);
      pio_sm_put_blocking(pio1, sm1, word0);
    }
    else if (dataA == 0x27 && dataS == 0x01 && dataC == 0x00) // REQUEST (ADDRESS)
    {
      word0 = 1;
      word1 = 0;

      if (channel == ATOD_CHANNEL_MODE)
      {
        // word1 = __rev(0b11000100100000101001101100000000); // 68
        word1 = __rev(crc_data_packet(0b11110100, 1)); // send & recv?
      } else {
        // word1 = __rev(0b11000110000000101001010000000000); // 70
        word1 = __rev(crc_data_packet(0b11110110, 1)); // send & recv?
      }

      pio_sm_put_blocking(pio1, sm1, word1);
      pio_sm_put_blocking(pio1, sm1, word0);
    }
    else if (dataA == 0x84 && dataS == 0x04 && dataC == 0x40) // REQUEST (B)
    {
      word0 = 1;
      word1 = 0;

      // 
      if ((0b101001001100 >> requestsB) & 0b01)
      {
        word1 = __rev(0b10);
      }

      pio_sm_put_blocking(pio1, sm1, word1);
      pio_sm_put_blocking(pio1, sm1, word0);

      requestsB++;
      if (requestsB == 12) requestsB = 7;
    }
    else if (dataA == 0x34 && dataS == 0x01) // CHANNEL
    {
      channel = dataC;
    }
    else if (dataA == 0x32 && dataS == 0x02 && dataC == 0x00) // QUADX
    {
      word0 = 1;
      word1 = __rev(0b10000000100000110000001100000000); //0

      word1 = __rev(output_quad_x);
      // TODO: solve how to set unique values to first two bytes plus checksum

      pio_sm_put_blocking(pio1, sm1, word1);
      pio_sm_put_blocking(pio1, sm1, word0);
    }
    else if (dataA == 0x35 && dataS == 0x01 && dataC == 0x00) // ANALOG
    {
      word0 = 1;
      word1 = __rev(0b10000000100000110000001100000000); //0

      // ALL_BUTTONS: CTRLR_STDBUTTONS & CTRLR_DPAD & CTRLR_SHOULDER & CTRLR_EXTBUTTONS
      // <= 23 - 0x51f CTRLR_TWIST & CTRLR_THROTTLE & CTRLR_ANALOG1 & ALL_BUTTONS
      // 29-47 - 0x83f CTRLR_MOUSE & CTRLR_ANALOG1 & CTRLR_ANALOG2 & ALL_BUTTONS
      // 48-69 - 0x01f CTRLR_ANALOG1 & ALL_BUTTONS
      // 70-92 - 0x808 CTRLR_MOUSE & CTRLR_EXTBUTTONS
      // >= 93 - ERROR?

      switch (channel)
      {
      case ATOD_CHANNEL_NONE:
        word1 = __rev(device_mode); // device mode packet?
        break;
      // case ATOD_CHANNEL_MODE:
      //   word1 = __rev(0b10000000100000110000001100000000);
      //   break;
      case ATOD_CHANNEL_X1:
        word1 = __rev(output_analog_1x);
        break;
      case ATOD_CHANNEL_Y1:
        word1 = __rev(output_analog_1y);
        break;
      case ATOD_CHANNEL_X2:
        word1 = __rev(output_analog_2x);
        break;
      case ATOD_CHANNEL_Y2:
        word1 = __rev(output_analog_2y);
        break;
      default:
        break;
      }

      pio_sm_put_blocking(pio1, sm1, word1);
      pio_sm_put_blocking(pio1, sm1, word0);
    }
    else if (dataA == 0x25 && dataS == 0x01 && dataC == 0x00) // CONFIG
    {
      word0 = 1;
      word1 = __rev(device_config); // device config packet?

      pio_sm_put_blocking(pio1, sm1, word1);
      pio_sm_put_blocking(pio1, sm1, word0);
    }
    else if (dataA == 0x31 && dataS == 0x01 && dataC == 0x00) // {SWITCH[16:9]}
    {
      word0 = 1;
      word1 = __rev(device_switch); // extra device config?

      pio_sm_put_blocking(pio1, sm1, word1);
      pio_sm_put_blocking(pio1, sm1, word0);
    }
    else if (dataA == 0x30 && dataS == 0x02 && dataC == 0x00) // {SWITCH[8:1]}
    {
      word0 = 1;
      word1 = __rev(output_buttons_0);

      pio_sm_put_blocking(pio1, sm1, word1);
      pio_sm_put_blocking(pio1, sm1, word0);
    }
    else if (dataA == 0x99 && dataS == 0x01) // STATE
    {
      switch (type0)
      {
      case PACKET_TYPE_READ:
        word0 = 1;
        word1 = __rev(0b11000000000000101000000000000000);

        if (((state >> 8) & 0xff) == 0x41 && (state & 0xff) == 0x51)
        {
          word1 = __rev(0b11010001000000101110011000000000);
        }
        pio_sm_put_blocking(pio1, sm1, word1);
        pio_sm_put_blocking(pio1, sm1, word0);
        break;
      // case PACKET_TYPE_WRITE:
      default:
        state = ((state) << 8) | (dataC & 0xff);
        break;
      }
    }
    else if (dataA == 0xb4 && dataS == 0x00) // BRAND
    {
      id = dataC;
      branded = true;
    }
  }
}

//
// update_output - updates output words with button/analog polyface packet
//
void __not_in_flash_func(update_output)(void)
{
  // Calculate and set Nuon output packet values here.
  int32_t buttons = (players[0].output_buttons & 0xffff) |
                    (players[0].output_buttons_alt & 0xffff);

  output_buttons_0 = crc_data_packet(buttons, 2);
  output_analog_1x = crc_data_packet(players[0].output_analog_1x, 1);
  output_analog_1y = crc_data_packet(players[0].output_analog_1y, 1);
  output_analog_2x = crc_data_packet(players[0].output_analog_2x, 1);
  output_analog_2y = crc_data_packet(players[0].output_analog_2y, 1);
  output_quad_x    = crc_data_packet(players[0].output_quad_x, 1);

  codes_task();

  update_pending = true;
}

//
// post_globals - accumulate button and analog values
//
void __not_in_flash_func(post_globals)(
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
  uint8_t quad_x)
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
    // extra instance buttons to merge with root player
    if (is_extra)
    {
      players[0].altern_buttons = buttons;
    }
    else
    {
      players[player_index].global_buttons = buttons;
    }

    uint32_t nuon_buttons = map_nuon_buttons(buttons);
    if (!instance)
    {
      players[player_index].output_buttons = nuon_buttons;
    }
    else
    {
      players[player_index].output_buttons_alt = nuon_buttons;
    }

    if (analog_1x) players[player_index].output_analog_1x = analog_1x;
    if (analog_1y) players[player_index].output_analog_1y = 256 - analog_1y;
    if (analog_2x) players[player_index].output_analog_2x = analog_2x;
    if (analog_2y) players[player_index].output_analog_2y = 256 - analog_2y;
    if (quad_x) players[player_index].output_quad_x = quad_x;
    update_output();
  }
}

//
// post_mouse_globals - accumulate the many intermediate mouse scans (~1ms)
//
void __not_in_flash_func(post_mouse_globals)(
  uint8_t dev_addr,
  int8_t instance,
  uint16_t buttons,
  uint8_t delta_x,
  uint8_t delta_y,
  uint8_t quad_x)
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

    // Swap B2 and S2
    if (!(buttons & USBR_BUTTON_B2)) {
      players[player_index].global_buttons |= USBR_BUTTON_B2;
      players[player_index].global_buttons &= ~USBR_BUTTON_S2;
    }
    if (!(buttons & USBR_BUTTON_S2)) {
      players[player_index].global_buttons |= USBR_BUTTON_S2;
      players[player_index].global_buttons &= ~USBR_BUTTON_B2;
    }

    players[player_index].output_buttons = map_nuon_buttons(players[player_index].global_buttons & players[player_index].altern_buttons);
    players[player_index].output_analog_1x = 128;
    players[player_index].output_analog_1y = 128;
    players[player_index].output_analog_2x = 128;
    players[player_index].output_analog_2y = 128;
    players[player_index].output_analog_l = 0;
    players[player_index].output_analog_r = 0;
    if (quad_x) players[player_index].output_quad_x = quad_x;

    update_output();
  }
}
