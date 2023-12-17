// codes.c

#include "codes.h"

// Definition of global variables
uint32_t code_buffer[CODE_LENGTH] = {0};
uint32_t konami_code[CODE_LENGTH] = KONAMI_CODE;
bool is_fun = false;
unsigned char fun_inc = 0;
unsigned char fun_player = 1;

// shift button state into buffer and scan for matching codes
void codes_task()
{
  int32_t btns = (~players[0].output_buttons & 0xffff);
  int32_t prev_btns = (~players[0].prev_buttons & 0xffff);

  // Stash previous buttons to detect release
  if (!btns || btns != prev_btns)
  {
    players[0].prev_buttons = players[0].output_buttons;
  }

  // Check if code has been entered
#ifdef CONFIG_NUON
  if (btns != 0xff7f && btns != prev_btns)
  {
    shift_buffer_and_insert(~btns & 0xff7f);
    check_for_konami_code();
  }
#else
  if ((btns & 0xff) && btns != prev_btns)
  {
    shift_buffer_and_insert(btns & 0xff);
    check_for_konami_code();
  }
#endif
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
