// rp_session.h - PS Remote Play session engine interface (usb2wifi)
// SPDX-License-Identifier: Apache-2.0
//
// This is the boundary for the chiaki-based Remote Play protocol engine
// (registration + Takion + senkusha + GKCrypt + controller feedback). The
// current implementation (rp_session_stub.c) is a STUB that validates the
// plumbing but does not open a real session — the real engine is a staged port
// of chiaki-ng (needs FreeRTOS + LWIP sockets + mbedTLS, and a Pico 2 W's RAM;
// a plain Pico W is too small). See .dev/docs/ps5-remoteplay-output.md.

#ifndef RP_SESSION_H
#define RP_SESSION_H

#include <stdbool.h>
#include <stdint.h>
#include "core/input_event.h"
#include "core/output_interface.h"

typedef enum {
    RP_SESS_IDLE = 0,     // not started / no config
    RP_SESS_CONNECTING,
    RP_SESS_READY,        // session up, forwarding input
    RP_SESS_ERROR,
    RP_SESS_UNIMPLEMENTED, // stub: engine not built in
} rp_session_state_t;

void rp_session_init(void);
// Begin connecting (requires wifi up + a complete registration in rp_config).
void rp_session_start(void);
void rp_session_stop(void);
void rp_session_task(void);
rp_session_state_t rp_session_get_state(void);
const char* rp_session_state_str(void);
bool rp_session_is_ready(void);

// Forward the latest controller state to the console.
void rp_session_set_controller_state(const input_event_t* ev, uint32_t buttons);
// Rumble/feedback from the console (returns false if none/unsupported).
bool rp_session_get_feedback(output_feedback_t* fb);

#endif // RP_SESSION_H
