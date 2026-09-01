// rp_oauth.h - On-device PSN OAuth token exchange (usb2wifi)
// SPDX-License-Identifier: Apache-2.0
//
// The web config opens Sony's login page in a browser tab; after the user signs
// in, Sony redirects to the Remote Play redirect URL carrying an authorization
// `code`. The browser can't complete the exchange (CORS + Sony's anti-bot edge
// block cross-origin fetches), so the DEVICE does it: this module opens an HTTPS
// (mbedTLS/altcp) connection to Sony, POSTs the code for an access token, GETs
// the account info, and derives the 8-byte PSN account id into rp_config.
//
// Async, single-threaded, driven from the LWIP poll loop (NO_SYS=1). Call
// rp_oauth_start(code) once, then poll rp_oauth_get_state()/rp_oauth_task().

#ifndef RP_OAUTH_H
#define RP_OAUTH_H

#include <stdbool.h>

typedef enum {
    RP_OAUTH_IDLE = 0,
    RP_OAUTH_RESOLVING,   // DNS lookup for the auth host
    RP_OAUTH_CONNECTING,  // TLS handshake
    RP_OAUTH_TOKEN,       // POST /oauth/token, awaiting access_token
    RP_OAUTH_INFO,        // GET /oauth/token/<token>, awaiting user_id
    RP_OAUTH_DONE,        // account id stored in rp_config
    RP_OAUTH_ERROR,
} rp_oauth_state_t;

void             rp_oauth_init(void);
bool             rp_oauth_start(const char* code); // false if busy or code invalid
void             rp_oauth_task(void);              // drive timeouts
rp_oauth_state_t rp_oauth_get_state(void);
const char*      rp_oauth_state_str(void);
const char*      rp_oauth_error(void);             // last error ("" if none)
const char*      rp_oauth_online_id(void);         // resolved PSN online id ("")

#endif // RP_OAUTH_H
