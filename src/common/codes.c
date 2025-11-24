// codes.c

#include "codes.h"
#include "core/router/router.h"

// Definition of global variables
uint32_t code_buffer[CODE_LENGTH] = {0};
uint32_t konami_code[CODE_LENGTH] = KONAMI_CODE;
bool is_fun = false;
unsigned char fun_inc = 0;
unsigned char fun_player = 1;

// Previous button state for edge detection (local to codes.c)
static uint32_t codes_prev_buttons = 0xFFFFF;

// shift button state into buffer and scan for matching codes
// Called by console update_output() after sending data to console
// Reads button state from router (player 0) for cheat code detection
void codes_task()
{
  // Get current button state from router (player 0)
  // Note: Caller (console device) must have already called router_get_output()
  // This reads from the same output state the console just used
  const input_event_t* event = router_get_output(OUTPUT_TARGET_GAMECUBE, 0);

  // Fallback to other outputs if GameCube returns NULL
  // (codes_task is called by multiple consoles)
  if (!event) event = router_get_output(OUTPUT_TARGET_PCENGINE, 0);
  if (!event) event = router_get_output(OUTPUT_TARGET_NUON, 0);
  if (!event) event = router_get_output(OUTPUT_TARGET_XBOXONE, 0);
  if (!event) event = router_get_output(OUTPUT_TARGET_LOOPY, 0);
  if (!event) return;  // No input available

  // USBR buttons use inverted logic (0 = pressed, 1 = released)
  // Invert to get positive logic (1 = pressed)
  uint32_t btns = ~event->buttons & 0x3f;  // D-pad (0x0F) + B1/B2 (0x30)
  uint32_t prev_btns = ~codes_prev_buttons & 0x3f;

  // Detect button press edge (new press that wasn't pressed before)
  if (btns && btns != prev_btns)
  {
    // Find which single button was just pressed
    // Konami code expects individual button presses, not combos
    uint32_t new_presses = btns & ~prev_btns;
    if (new_presses)
    {
      shift_buffer_and_insert(new_presses);
      check_for_konami_code();
    }
    codes_prev_buttons = event->buttons;
  }
  else if (!btns && prev_btns)
  {
    // All buttons released
    codes_prev_buttons = event->buttons;
  }
}

// shift button presses into buffer
void __not_in_flash_func(shift_buffer_and_insert)(uint32_t new_value)
{
  // Shift all elements to the left by 1
  for (int i = 0; i < CODE_LENGTH - 1; i++)
  {
    code_buffer[i] = code_buffer[i + 1];
  }

  // Insert the new value at the end
  code_buffer[CODE_LENGTH - 1] = new_value;
}

// check buffer for konami code match
void __not_in_flash_func(check_for_konami_code)(void)
{
  // DEBUG LOGGING
  // printf("Buffer content: ");
  // for (int i = 0; i < CODE_LENGTH; i++)
  // {
  //     printf("%x ", code_buffer[i]);
  // }
  // printf("\n");

  for (int i = 0; i < CODE_LENGTH; i++)
  {
    if (code_buffer[i] != konami_code[i])
    {
      return;
    }
  }

  // The Konami Code has been entered
  printf("is_fun!\n");
  is_fun = !is_fun;
}
