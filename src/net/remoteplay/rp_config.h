// rp_config.h - PS Remote Play provisioning config (usb2wifi app)
// SPDX-License-Identifier: Apache-2.0
//
// Everything the adapter needs to join WiFi and open a Remote Play session to a
// PS5: WiFi SSID/password, the PSN account id, the console IP, and the
// registration key (RP-Key) — all obtained on a PC (e.g. remote-play-lab/rp.py)
// and provisioned into the device over the web config (CDC WIFI.*/RP.* commands).
//
// NOTE: persisted in RAM for now; flash persistence is a TODO (provision each
// boot via web config until then). See .dev/docs/ps5-remoteplay-output.md.

#ifndef RP_CONFIG_H
#define RP_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

#define RP_SSID_MAX     33   // 32 chars + NUL
#define RP_PASS_MAX     64
#define RP_IP_MAX       16   // "255.255.255.255"
#define RP_ACCOUNT_LEN  8    // PSN account id, 8 raw bytes
#define RP_KEY_LEN      16   // RP-Key (morning), 16 bytes
#define RP_REGIST_LEN   16   // registration key

typedef struct {
    char    wifi_ssid[RP_SSID_MAX];
    char    wifi_pass[RP_PASS_MAX];
    char    ps5_ip[RP_IP_MAX];
    uint8_t account_id[RP_ACCOUNT_LEN];
    uint8_t rp_key[RP_KEY_LEN];         // "morning"
    uint8_t regist_key[RP_REGIST_LEN];
    bool    have_wifi;                   // ssid+pass set
    bool    have_registration;           // account+ip+keys set
} rp_config_t;

void rp_config_init(void);                 // load from flash (or defaults)
rp_config_t* rp_config_get(void);
void rp_config_save(void);                 // persist to flash
void rp_config_clear(void);                // wipe config + erase flash store

// Provisioning setters (from web config / CDC). Return true on valid input.
bool rp_config_set_wifi(const char* ssid, const char* pass);
bool rp_config_set_ps5_ip(const char* ip);
bool rp_config_set_account_id(const uint8_t* id8);
bool rp_config_set_keys(const uint8_t* rp_key16, const uint8_t* regist16);

#endif // RP_CONFIG_H
