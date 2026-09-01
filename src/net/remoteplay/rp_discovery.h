// rp_discovery.h - PS5/PS4 LAN discovery (raw-LWIP UDP broadcast) for usb2wifi
// SPDX-License-Identifier: Apache-2.0
//
// Self-contained "find my PS5" over the same UDP broadcast protocol chiaki uses
// (SRCH to :9302 for PS5, :987 for PS4). Works in the current poll-mode LWIP
// (NO_SYS=1) — no sockets/engine needed. Lets the web config auto-fill the PS5
// IP instead of typing it, and shows the console's wake state.

#ifndef RP_DISCOVERY_H
#define RP_DISCOVERY_H

#include <stdint.h>
#include <stdbool.h>

#define RP_DISCOVERY_MAX_HOSTS 4

typedef struct {
    char ip[16];
    char name[32];
    bool is_ps5;
    bool ready;      // true = awake/ready, false = standby
} rp_discovery_host_t;

// Kick off a scan (broadcasts probes). Safe to call repeatedly; requires WiFi up.
void rp_discovery_start(void);
// Pump timeouts/retransmit. Call from the output task.
void rp_discovery_task(void);
bool rp_discovery_in_progress(void);
// Copy found hosts into out[]; returns count.
uint8_t rp_discovery_get_hosts(rp_discovery_host_t* out, uint8_t max);

#endif // RP_DISCOVERY_H
