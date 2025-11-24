// manager.h
// USBRetro Core - Player Management System
//
// Configurable player slot management supporting both SHIFT and FIXED modes.
// SHIFT mode: Players shift up when one disconnects (3DO, PCEngine)
// FIXED mode: Players stay in assigned slots (GameCube 4-port)

#ifndef PLAYER_MANAGER_H
#define PLAYER_MANAGER_H

#include <stdint.h>
#include "tusb.h"
#include "input_event.h"
#ifdef CONFIG_NGC
#include "lib/joybus-pio/include/gamecube_definitions.h"
#endif

#ifndef MAX_PLAYERS
#define MAX_PLAYERS 5
#endif

// ============================================================================
// PLAYER SLOT MODES
// ============================================================================

typedef enum {
    PLAYER_SLOT_SHIFT,      // Shift players up when one disconnects (3DO, PCE)
    PLAYER_SLOT_FIXED,      // Keep players in assigned slots (GameCube 4-port)
} player_slot_mode_t;

// ============================================================================
// PLAYER CONFIGURATION
// ============================================================================

typedef struct {
    player_slot_mode_t slot_mode;
    uint8_t max_slots;              // Maximum player slots (1-8)
    bool auto_assign_on_press;      // Assign slot on first button press
} player_config_t;

// ============================================================================
// PLAYER DATA STRUCTURE
// ============================================================================

typedef struct TU_ATTR_PACKED
{
  // Device identification
  int dev_addr;
  int instance;
  int player_number;
  input_device_type_t device_type;  // Device type (gamepad, flightstick, mouse, etc.)

  // Digital inputs
  int32_t global_buttons;
  int32_t altern_buttons;
  int32_t output_buttons;
  int32_t prev_buttons;

  // Analog inputs (unified array)
  // [0]=LX, [1]=LY, [2]=RX, [3]=RY, [4]=LT, [5]=RT, [6]=Extra1, [7]=Extra2
  int16_t analog[8];

  // Mouse/relative motion accumulators
  int16_t global_x;
  int16_t global_y;

  // Keyboard
  uint8_t keypress[3];

  // Mode
  int button_mode;

  // Console-specific extensions
#ifdef CONFIG_NGC
  gc_report_t gc_report;
#endif
} Player_t;

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

// Player array (MAX_PLAYERS slots)
extern Player_t players[MAX_PLAYERS];

// Player count (highest occupied slot + 1)
extern int playersCount;

// LED patterns for PS3/Switch controllers
extern const uint8_t PLAYER_LEDS[11];

// ============================================================================
// PLAYER MANAGEMENT API
// ============================================================================

// Initialize player system with default configuration (SHIFT mode)
void players_init(void);

// Initialize player system with custom configuration
void players_init_with_config(const player_config_t* config);

// Get/set slot mode (for runtime changes)
void players_set_slot_mode(player_slot_mode_t mode);
player_slot_mode_t players_get_slot_mode(void);

// Find player by dev_addr and instance
// Returns player index (0-based), or -1 if not found
int __not_in_flash_func(find_player_index)(int dev_addr, int instance);

// Add player to array
// SHIFT mode: Adds to end (playersCount++)
// FIXED mode: Finds first empty slot (dev_addr == -1)
// Returns player index (0-based), or -1 if full
int __not_in_flash_func(add_player)(int dev_addr, int instance);

// Remove player(s) by address
// SHIFT mode: Shifts remaining players up, renumbers all
// FIXED mode: Marks slot as empty (dev_addr = -1), preserves positions
// instance = -1 removes all instances of dev_addr
void remove_players_by_address(int dev_addr, int instance);

#endif // PLAYER_MANAGER_H
