// codes.h - Button sequence detection service
//
// Detects button sequences (cheat codes) and manages test mode state.

#ifndef CODES_H
#define CODES_H

#include <stdint.h>
#include <stdbool.h>

// Test mode API
bool codes_is_test_mode(void);
void codes_reset_test_mode(void);
uint8_t codes_get_test_counter(void);

// Called from console update_output() to detect button sequences
void codes_task(void);

#endif // CODES_H
