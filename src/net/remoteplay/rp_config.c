// rp_config.c - PS Remote Play provisioning config (RAM-backed; flash TODO)
// SPDX-License-Identifier: Apache-2.0
#include "rp_config.h"
#include <string.h>
#include <stdio.h>

static rp_config_t cfg;

void rp_config_init(void)
{
    memset(&cfg, 0, sizeof(cfg));
    // TODO: load from flash. RAM-only for now — provision via web config each boot.
}

rp_config_t* rp_config_get(void) { return &cfg; }

void rp_config_save(void)
{
    // TODO: persist to flash (add an rp_config region to the storage service).
}

bool rp_config_set_wifi(const char* ssid, const char* pass)
{
    if (!ssid || ssid[0] == '\0' || strlen(ssid) >= RP_SSID_MAX) return false;
    if (!pass || strlen(pass) >= RP_PASS_MAX) return false;
    strncpy(cfg.wifi_ssid, ssid, RP_SSID_MAX - 1); cfg.wifi_ssid[RP_SSID_MAX - 1] = '\0';
    strncpy(cfg.wifi_pass, pass, RP_PASS_MAX - 1); cfg.wifi_pass[RP_PASS_MAX - 1] = '\0';
    cfg.have_wifi = true;
    printf("[rp_config] wifi set: ssid=%s\n", cfg.wifi_ssid);
    return true;
}

bool rp_config_set_ps5_ip(const char* ip)
{
    if (!ip || ip[0] == '\0' || strlen(ip) >= RP_IP_MAX) return false;
    strncpy(cfg.ps5_ip, ip, RP_IP_MAX - 1); cfg.ps5_ip[RP_IP_MAX - 1] = '\0';
    printf("[rp_config] ps5 ip set: %s\n", cfg.ps5_ip);
    return true;
}

bool rp_config_set_account_id(const uint8_t* id8)
{
    if (!id8) return false;
    memcpy(cfg.account_id, id8, RP_ACCOUNT_LEN);
    return true;
}

bool rp_config_set_keys(const uint8_t* rp_key16, const uint8_t* regist16)
{
    if (!rp_key16 || !regist16) return false;
    memcpy(cfg.rp_key, rp_key16, RP_KEY_LEN);
    memcpy(cfg.regist_key, regist16, RP_REGIST_LEN);
    // registration is "complete" once account, ip and keys are all present
    bool have_ip = cfg.ps5_ip[0] != '\0';
    bool have_acct = false;
    for (int i = 0; i < RP_ACCOUNT_LEN; i++) if (cfg.account_id[i]) { have_acct = true; break; }
    cfg.have_registration = have_ip && have_acct;
    return true;
}
