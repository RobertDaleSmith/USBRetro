// players.c

#include "players.h"
#include "globals.h"

// Definition of global variables
int playersCount = 0;

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
    players[i].output_buttons_alt = 0x80;
    players[i].output_quad_x = 0;
#else
    players[i].global_buttons = 0xFFFFF;
    players[i].altern_buttons = 0xFFFFF;
    players[i].output_buttons = 0xFFFFF;
#endif
    players[i].global_x = 0;
    players[i].global_y = 0;
    players[i].output_analog_1x = 128;
    players[i].output_analog_1y = 128;
    players[i].output_analog_2x = 128;
    players[i].output_analog_2y = 128;
    players[i].output_analog_l = 0;
    players[i].output_analog_r = 0;
    players[i].prev_buttons = 0xFFFFF;
    players[i].button_mode = 0;
  }
}

// Function to find a player in the array based on their device_address and instance_number.
int __not_in_flash_func(find_player_index)(int device_address, int instance_number)
{
  for(int i = 0; i < playersCount; i++)
  {
    if(players[i].device_address == device_address && players[i].instance_number == instance_number)
    {
      return i;
    }
  }
  // If we reached here, the player was not found.
  return -1;
}

// An example function to add a player to the array.
int __not_in_flash_func(add_player)(int device_address, int instance_number)
{
    if(playersCount == MAX_PLAYERS) return -1;

    players[playersCount].device_address = device_address;
    players[playersCount].instance_number = instance_number;
    players[playersCount].player_number = playersCount + 1;

    players[playersCount].global_buttons = 0xFFFFF;
    players[playersCount].altern_buttons = 0xFFFFF;
    players[playersCount].global_x = 0;
    players[playersCount].global_y = 0;

    players[playersCount].output_buttons = 0xFFFFF;
    players[playersCount].output_analog_1x = 0;
    players[playersCount].output_analog_1y = 0;
    players[playersCount].button_mode = 0;
    players[playersCount].prev_buttons = 0xFFFFF;

    playersCount++;
    return playersCount-1; // returns player_index
}

// Function to remove all players with a certain device_address and shift the remaining players.
void remove_players_by_address(int device_address, int instance)
{
  int i = 0;
  while(i < playersCount)
  {
    // -1 instance removes all instances within device_address
    if((players[i].device_address == device_address && instance == -1) ||
       (players[i].device_address == device_address && players[i].instance_number == instance))
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
