// rp_regist.h - On-device PS5 registration (pairing) for usb2wifi
// SPDX-License-Identifier: Apache-2.0
//
// Pairs the adapter with a console so it can later open a Remote Play session.
// The user puts the PS5 into "Link Device" (Settings > System > Remote Play >
// Link Device), which shows an 8-digit PIN. This runs the chiaki registration
// handshake to that console — a UDP "search" wake-up (SRC3/RES3) followed by an
// encrypted HTTP request to :9295 — and stores the returned RP-Key + Registration
// Key into rp_config (completing have_registration).
//
// Async, single-threaded, driven from the LWIP poll loop (NO_SYS=1). The crypto
// is chiaki's rpcrypt (compiled with mbedTLS); the networking is raw lwip here.
// When the chiaki sources aren't vendored, rp_regist_stub.c stands in and reports
// RP_REGIST_UNAVAILABLE.

#ifndef RP_REGIST_H
#define RP_REGIST_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    RP_REGIST_IDLE = 0,
    RP_REGIST_SEARCH,       // UDP SRC3 -> awaiting RES3 (wake the console)
    RP_REGIST_CONNECT,      // TCP connect to :9295
    RP_REGIST_SEND,         // sending the encrypted regist request
    RP_REGIST_RECV,         // awaiting the encrypted response
    RP_REGIST_DONE,         // RP-Key + Regist Key stored
    RP_REGIST_ERROR,
    RP_REGIST_UNAVAILABLE,  // built without the chiaki crypto (stub)
} rp_regist_state_t;

void              rp_regist_init(void);
// Begin pairing with the console at ps5_ip using the console's 8-digit PIN.
// Uses the PSN account id already stored in rp_config. Returns false if busy,
// unavailable, or missing prerequisites (account id / wifi).
bool              rp_regist_start(const char* ps5_ip, uint32_t pin);
void              rp_regist_task(void);
rp_regist_state_t rp_regist_get_state(void);
const char*       rp_regist_state_str(void);
const char*       rp_regist_error(void);   // "" if none

#endif // RP_REGIST_H
