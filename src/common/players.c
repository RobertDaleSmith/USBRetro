// players.c

#include "players.h"
#include "globals.h"
#include "input_event.h"

// Definition of global variables
Player_t players[MAX_PLAYERS];
int playersCount = 0;

// Used to set the LEDs on the controllers
const uint8_t PLAYER_LEDS[] = {
  0x00, // OFF
  0x01, // LED1  0001
  0x02, // LED2  0010
  0x04, // LED3  0100
  0x08, // LED4  1000
  0x09, // LED5  1001
  0x0A, // LED6  1010
  0x0C, // LED7  1100
  0x0D, // LED8  1101
  0x0E, // LED9  1110
  0x0F, // LED10 1111
};

// init data structure for multi-player management
void players_init()
{
  unsigned short int i;
  for (i = 0; i < MAX_PLAYERS; ++i)
  {
#ifdef CONFIG_NGC
    players[i].gc_report = default_gc_report;
#elif CONFIG_NUON
    players[i].global_buttons = 0x80;
    players[i].altern_buttons = 0x80;
    players[i].output_buttons = 0x80;
    // Note: output_buttons_alt and output_quad_x removed (Phase 5 cleanup)
#else
    players[i].global_buttons = 0xFFFFF;
    players[i].altern_buttons = 0xFFFFF;
    players[i].output_buttons = 0xFFFFF;
#endif
    players[i].global_x = 0;
    players[i].global_y = 0;

    // Initialize all analog axes to centered position (128 = neutral)
    for (int j = 0; j < 8; j++) {
      players[i].analog[j] = 128;
    }

    players[i].device_type = INPUT_TYPE_NONE;
    players[i].prev_buttons = 0xFFFFF;
    players[i].button_mode = 0;
  }
}

// Function to find a player in the array based on their dev_addr and instance.
int __not_in_flash_func(find_player_index)(int dev_addr, int instance)
{
  for(int i = 0; i < playersCount; i++)
  {
    if(players[i].dev_addr == dev_addr && players[i].instance == instance)
    {
      return i;
    }
  }
  // If we reached here, the player was not found.
  return -1;
}

// Function to add a player to the players array.
int __not_in_flash_func(add_player)(int dev_addr, int instance)
{
    if(playersCount == MAX_PLAYERS) return -1;

    players[playersCount].dev_addr = dev_addr;
    players[playersCount].instance = instance;
    players[playersCount].player_number = playersCount + 1;

    players[playersCount].global_buttons = 0xFFFFF;
    players[playersCount].altern_buttons = 0xFFFFF;
    players[playersCount].global_x = 0;
    players[playersCount].global_y = 0;

    players[playersCount].output_buttons = 0xFFFFF;

    // Initialize all analog axes to centered position
    for (int j = 0; j < 8; j++) {
      players[playersCount].analog[j] = 128;
    }

    players[playersCount].device_type = INPUT_TYPE_NONE;
    players[playersCount].button_mode = 0;
    players[playersCount].prev_buttons = 0xFFFFF;

    playersCount++;
    return playersCount-1; // returns player_index
}

// Function to remove all players with a certain dev_addr and shift the remaining players.
void remove_players_by_address(int dev_addr, int instance)
{
  int i = 0;
  while(i < playersCount)
  {
    // -1 instance removes all instances within dev_addr
    if((players[i].dev_addr == dev_addr && instance == -1) ||
       (players[i].dev_addr == dev_addr && players[i].instance == instance))
    {
      // Shift all the players after this one up in the array
      for(int j = i; j < playersCount - 1; j++)
      {
        players[j] = players[j+1];
      }
      // Decrement playersCount because a player was removed
      playersCount--;
    } else {
      i++;
    }
  }

  // Update the player numbers
  for(i = 0; i < playersCount; i++)
  {
    players[i].player_number = i + 1;
  }
}
