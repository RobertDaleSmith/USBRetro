// wifi_station.h - CYW43 WiFi station-mode bring-up for usb2wifi
// SPDX-License-Identifier: Apache-2.0
//
// Unlike jocp/wifi_transport.c (AP mode, controllers connect TO us), this joins
// an existing network (station mode) so the adapter can reach a PS5 for Remote
// Play. Poll-mode CYW43 (PICO_CYW43_ARCH_POLL): call wifi_station_task() often.

#ifndef WIFI_STATION_H
#define WIFI_STATION_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    WIFI_STA_IDLE = 0,      // no creds configured
    WIFI_STA_CONNECTING,
    WIFI_STA_CONNECTED,
    WIFI_STA_FAILED,
} wifi_station_state_t;

// Bring up CYW43 in station mode. Safe to call once at boot. Returns false if
// the CYW43 could not be initialised.
bool wifi_station_init(void);

// Start (or restart) connecting to the SSID/pass currently in rp_config.
void wifi_station_connect(void);

// Pump CYW43 + manage connection/retry. Call every main-loop iteration.
void wifi_station_task(void);

wifi_station_state_t wifi_station_get_state(void);
bool wifi_station_is_connected(void);
// Fills a dotted-quad string of our IP (empty if not connected). buf >= 16.
void wifi_station_get_ip(char* buf, int buflen);

#endif // WIFI_STATION_H
