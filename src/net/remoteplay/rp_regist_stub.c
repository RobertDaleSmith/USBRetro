// rp_regist_stub.c - fallback when the chiaki registration crypto isn't vendored
// SPDX-License-Identifier: Apache-2.0
//
// src/lib/chiaki is gitignored (AGPL, local-only), so CI / fresh checkouts build
// this stub instead of rp_regist.c. It reports RP_REGIST_UNAVAILABLE so the web
// config can say "pairing not built in" rather than silently doing nothing.

#include "rp_regist.h"

void rp_regist_init(void) {}

bool rp_regist_start(const char* ps5_ip, uint32_t pin)
{
    (void)ps5_ip; (void)pin;
    return false;
}

void rp_regist_task(void) {}

rp_regist_state_t rp_regist_get_state(void) { return RP_REGIST_UNAVAILABLE; }
const char* rp_regist_error(void) { return "pairing engine not built"; }

const char* rp_regist_state_str(void) { return "unavailable"; }
