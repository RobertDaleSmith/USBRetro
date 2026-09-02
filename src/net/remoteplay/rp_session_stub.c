// rp_session_stub.c - placeholder Remote Play session engine
// SPDX-License-Identifier: Apache-2.0
//
// Validates the usb2wifi plumbing (config, WiFi station, output interface,
// input forwarding) WITHOUT a real Remote Play session. The real engine is a
// staged chiaki-ng port; see rp_session.h + .dev/docs/ps5-remoteplay-output.md.
//
// Swap this file for rp_session_chiaki.c (same rp_session.h interface) when the
// engine lands. It intentionally rate-limits a log line so you can confirm on
// UART that input is reaching the output.

#include "rp_session.h"
#include "rp_config.h"
#include "wifi_station.h"
#include "platform/platform.h"
#include <stdio.h>
#include <string.h>

static rp_session_state_t s_state = RP_SESS_IDLE;
static uint32_t s_last_log = 0;
static uint32_t s_input_count = 0;

void rp_session_init(void)
{
    s_state = RP_SESS_IDLE;
    s_input_count = 0;
}

void rp_session_start(void)
{
    rp_config_t* cfg = rp_config_get();
    if (!wifi_station_is_connected()) { s_state = RP_SESS_IDLE; return; }
    if (!cfg->have_registration)      { s_state = RP_SESS_IDLE; return; }
    // A real engine would open the Takion session to cfg->ps5_ip here.
    printf("[rp_session] STUB: would open Remote Play session to %s "
           "(engine not built in — see rp_session.h)\n", cfg->ps5_ip);
    s_state = RP_SESS_UNIMPLEMENTED;
}

void rp_session_stop(void) { s_state = RP_SESS_IDLE; }

void rp_session_set_enabled(bool enabled) { (void)enabled; }
bool rp_session_is_enabled(void) { return false; }

void rp_session_task(void)
{
    // Auto-start once wifi + registration are ready.
    if (s_state == RP_SESS_IDLE) {
        rp_config_t* cfg = rp_config_get();
        if (wifi_station_is_connected() && cfg->have_registration) rp_session_start();
    }
}

rp_session_state_t rp_session_get_state(void) { return s_state; }

const char* rp_session_state_str(void)
{
    switch (s_state) {
        case RP_SESS_IDLE:          return "idle";
        case RP_SESS_CONNECTING:    return "connecting";
        case RP_SESS_READY:         return "ready";
        case RP_SESS_ERROR:         return "error";
        case RP_SESS_UNIMPLEMENTED: return "engine-not-built";
        default:                    return "?";
    }
}

bool rp_session_is_ready(void) { return s_state == RP_SESS_READY; }

void rp_session_set_controller_state(const input_event_t* ev, uint32_t buttons)
{
    (void)ev;
    s_input_count++;
    uint32_t now = platform_time_ms();
    if (now - s_last_log >= 1000) {
        s_last_log = now;
        printf("[rp_session] STUB: %lu input frames buffered, buttons=0x%06lx "
               "(no real session)\n", (unsigned long)s_input_count,
               (unsigned long)buttons);
    }
}

bool rp_session_get_feedback(output_feedback_t* fb)
{
    (void)fb;
    return false;  // no rumble from a stub session
}
