// globals.h
#ifndef GLOBALS_H
#define GLOBALS_H

#include "codes.h"
#include "players.h"

#define UART_ID uart0
#define BAUD_RATE 115200

// Define the UART pins on your Stemma connector
#define UART_TX_PIN 12  // Replace with your TX pin number
#define UART_RX_PIN 13  // Replace with your RX pin number

#define MAX_DEVICES 6

// USBRetro Buttons -> Xinput/Switch/PlayStation
#define USBR_BUTTON_B1 0x00020 // A/B/Cross
#define USBR_BUTTON_B2 0x00010 // B/A/Circle
#define USBR_BUTTON_B3 0x02000 // X/Y/Square
#define USBR_BUTTON_B4 0x01000 // Y/X/Triangle
#define USBR_BUTTON_L1 0x04000 // LB/L/L1
#define USBR_BUTTON_R1 0x08000 // RB/R/R1
#define USBR_BUTTON_L2 0x00100 // LT/ZL/L2
#define USBR_BUTTON_R2 0x00200 // RT/ZR/R2
#define USBR_BUTTON_S1 0x00040 // Back/Minus/Share/Select
#define USBR_BUTTON_S2 0x00080 // Start/Plus/Options
#define USBR_BUTTON_L3 0x10000 // LS/L3
#define USBR_BUTTON_R3 0x20000 // RS/R3
#define USBR_BUTTON_A1 0x00400 // Guide/Home/PS
#define USBR_BUTTON_A2 0x00000 // Capture/Touchpad

// USBRetro Dpad Directions
#define USBR_BUTTON_DU 0x00001 // Dpad Up
#define USBR_BUTTON_DD 0x00004 // Dpad Down
#define USBR_BUTTON_DL 0x00008 // Dpad Left
#define USBR_BUTTON_DR 0x00002 // Dpad Right
// TODO: add A2 and make button values uniform

// handles state of multi-player inputs
extern bool update_pending;

// GameCube rumble and keyboard LED states
extern uint8_t gc_rumble;
extern uint8_t gc_kb_led;

// common console response for controller data
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
  uint8_t quad_x
);

// common console response for mouse data
void __not_in_flash_func(post_mouse_globals)(
  uint8_t dev_addr,
  int8_t instance,
  uint16_t buttons,
  uint8_t delta_x,
  uint8_t delta_y,
  uint8_t spinner
);

int __not_in_flash_func(find_player_index)(int dev_addr, int instance);
int __not_in_flash_func(add_player)(int dev_addr, int instance);
void remove_players_by_address(int dev_addr, int instance);

#endif // GLOBALS_H
