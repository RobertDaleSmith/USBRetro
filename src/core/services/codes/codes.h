// codes.h - Button sequence detection service
//
// Detects button sequences (cheat codes) and manages test mode state.

#ifndef CODES_H
#define CODES_H

#include <stdint.h>
#include <stdbool.h>
#include "core/router/router.h"

// Code detected callback type
typedef void (*codes_callback_t)(const char* code_name);

// Set callback for when a code is detected
void codes_set_callback(codes_callback_t callback);

// Test mode API
bool codes_is_test_mode(void);
void codes_reset_test_mode(void);
uint8_t codes_get_test_counter(void);

// Called from console update_output() to detect button sequences
void codes_task(void);

// Task with explicit output target (for controller app)
void codes_task_for_output(output_target_t output);

#endif // CODES_H
