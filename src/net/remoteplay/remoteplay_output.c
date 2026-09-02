// remoteplay_output.c - PS Remote Play OutputInterface (usb2wifi)
// SPDX-License-Identifier: Apache-2.0
#include "remoteplay_output.h"
#include "rp_config.h"
#include "wifi_station.h"
#include "rp_session.h"
#include "rp_discovery.h"
#include "rp_oauth.h"
#include "rp_regist.h"
#include "core/router/router.h"
#include "platform/platform.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// --- tiny JSON field extractor: copies "key":"value" string into out ---------
static bool json_str(const char* json, const char* key, char* out, int outlen)
{
    char pat[48];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char* p = strstr(json, pat);
    if (!p) return false;
    p = strchr(p + strlen(pat), ':');
    if (!p) return false;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '"') return false;
    p++;
    int i = 0;
    while (*p && *p != '"' && i < outlen - 1) out[i++] = *p++;
    out[i] = '\0';
    return true;
}

// hex string -> bytes; returns true if exactly `nbytes` decoded.
static bool hex_bytes(const char* hex, uint8_t* out, int nbytes)
{
    if ((int)strlen(hex) != nbytes * 2) return false;
    for (int i = 0; i < nbytes; i++) {
        char b[3] = { hex[i * 2], hex[i * 2 + 1], 0 };
        char* end = NULL;
        long v = strtol(b, &end, 16);
        if (end != b + 2) return false;
        out[i] = (uint8_t)v;
    }
    return true;
}

// ---------------------------------------------------------------------------
// WiFi/CYW43 bring-up is DEFERRED out of init() into the task, gated a couple
// seconds after boot. main.c runs output init() BEFORE stdio + the main loop,
// so a slow/blocking cyw43_arch_init() here would stall USB enumeration (no CDC,
// no UART). Bringing it up in the loop lets the USB device enumerate first.
static bool wifi_started = false;

static void rp_out_init(void)
{
    rp_config_init();
    rp_session_init();
    rp_oauth_init();
    rp_regist_init();
}

static void rp_out_task(void)
{
    if (!wifi_started && platform_time_ms() > 2000) {
        wifi_started = true;
        if (wifi_station_init()) wifi_station_connect();
    }
    if (wifi_started) wifi_station_task();
    rp_discovery_task();
    rp_oauth_task();
    rp_regist_task();
    rp_session_task();

    // Forward the merged controller state to the session.
    const input_event_t* ev = router_get_output(OUTPUT_TARGET_REMOTE_PLAY, 0);
    if (ev) {
        rp_session_set_controller_state(ev, ev->buttons);
    }
}

static bool rp_out_get_feedback(output_feedback_t* fb)
{
    return rp_session_get_feedback(fb);
}

// GET: report status (for the web config page).
static uint16_t rp_out_get_native_config(char* buf, uint16_t buf_size)
{
    rp_config_t* cfg = rp_config_get();
    char ip[16]; wifi_station_get_ip(ip, sizeof(ip));
    const char* wstate =
        wifi_station_is_connected() ? "connected" :
        (wifi_station_get_state() == WIFI_STA_CONNECTING ? "connecting" :
         (wifi_station_get_state() == WIFI_STA_FAILED ? "failed" : "idle"));
    bool have_account = false;
    for (int i = 0; i < RP_ACCOUNT_LEN; i++)
        if (cfg->account_id[i]) { have_account = true; break; }
    int n = snprintf(buf, buf_size,
        "\"type\":\"remoteplay\",\"wifi_ssid\":\"%s\",\"wifi_state\":\"%s\","
        "\"ip\":\"%s\",\"ps5_ip\":\"%s\",\"have_wifi\":%s,"
        "\"have_account\":%s,\"psn_online_id\":\"%s\",\"oauth\":\"%s\",\"oauth_error\":\"%s\","
        "\"regist\":\"%s\",\"regist_error\":\"%s\",\"streaming\":%s,"
        "\"have_registration\":%s,\"session\":\"%s\",\"scanning\":%s,\"hosts\":[",
        cfg->wifi_ssid, wstate, ip, cfg->ps5_ip,
        cfg->have_wifi ? "true" : "false",
        have_account ? "true" : "false",
        rp_oauth_online_id(), rp_oauth_state_str(), rp_oauth_error(),
        rp_regist_state_str(), rp_regist_error(),
        rp_session_is_enabled() ? "true" : "false",
        cfg->have_registration ? "true" : "false",
        rp_session_state_str(),
        rp_discovery_in_progress() ? "true" : "false");
    if (n < 0) return 0;

    // Append discovered consoles (auto-fill candidates).
    rp_discovery_host_t hs[RP_DISCOVERY_MAX_HOSTS];
    uint8_t hc = rp_discovery_get_hosts(hs, RP_DISCOVERY_MAX_HOSTS);
    for (uint8_t i = 0; i < hc && n < (int)buf_size - 96; i++) {
        n += snprintf(buf + n, buf_size - n,
            "%s{\"ip\":\"%s\",\"name\":\"%s\",\"ps5\":%s,\"ready\":%s}",
            i ? "," : "", hs[i].ip, hs[i].name,
            hs[i].is_ps5 ? "true" : "false", hs[i].ready ? "true" : "false");
    }
    n += snprintf(buf + n, buf_size - n, "],\"wifiscanning\":%s,\"aps\":[",
                  wifi_ap_scan_in_progress() ? "true" : "false");

    // Append scanned WiFi APs (network picker).
    wifi_ap_t ap[WIFI_AP_MAX];
    uint8_t ac = wifi_ap_get_results(ap, WIFI_AP_MAX);
    for (uint8_t i = 0; i < ac && n < (int)buf_size - 64; i++) {
        // escape " and \ in SSIDs for valid JSON
        char esc[40]; int e = 0;
        for (const char* s = ap[i].ssid; *s && e < (int)sizeof(esc) - 2; s++) {
            if (*s == '"' || *s == '\\') esc[e++] = '\\';
            esc[e++] = *s;
        }
        esc[e] = '\0';
        n += snprintf(buf + n, buf_size - n, "%s{\"ssid\":\"%s\",\"rssi\":%d,\"secure\":%s}",
                      i ? "," : "", esc, ap[i].rssi, ap[i].secure ? "true" : "false");
    }
    n += snprintf(buf + n, buf_size - n, "]");
    return (uint16_t)n;
}

// SET: provision WiFi creds + PSN account + PS5 IP + RP-Key (all optional per
// call). account_id (8B), rp_key (16B), regist_key (16B) are hex strings.
static bool rp_out_set_native_config(const char* json, char* resp, uint16_t resp_size)
{
    // Action: scan for WiFi APs (network picker). Not a config change.
    if (strstr(json, "\"wifiscan\"")) {
        wifi_ap_scan_start();
        snprintf(resp, resp_size, "{\"status\":\"wifiscanning\"}");
        return true;
    }
    // Action: scan the LAN for consoles (auto-fill IP). Not a config change.
    if (strstr(json, "\"scan\"")) {
        rp_discovery_start();
        snprintf(resp, resp_size, "{\"status\":\"scanning\"}");
        return true;
    }
    // Action: enable/disable streaming (opt-in — connecting puts the PS5 into
    // Remote Play and blanks its local TV, so it must be explicitly started).
    if (strstr(json, "\"stream\"")) {
        bool en = strstr(json, "\"stream\":1") || strstr(json, "\"stream\": 1") ||
                  strstr(json, "\"stream\":true");
        rp_session_set_enabled(en);
        snprintf(resp, resp_size, "{\"status\":\"%s\"}", en ? "streaming-enabled" : "streaming-stopped");
        return true;
    }
    // Action: unlink / factory-reset the Remote Play provisioning.
    if (strstr(json, "\"rp_reset\"")) {
        rp_config_clear();
        snprintf(resp, resp_size, "{\"status\":\"cleared\"}");
        return true;
    }
    // Action: pair with the console using its 8-digit "Link Device" PIN. Uses the
    // stored ps5_ip + PSN account id; on success stores RP-Key + Regist Key.
    {
        char pinstr[16];
        if (json_str(json, "pair_pin", pinstr, sizeof(pinstr))) {
            rp_config_t* c = rp_config_get();
            uint32_t pin = (uint32_t)strtoul(pinstr, NULL, 10);
            bool ok = rp_regist_start(c->ps5_ip, pin);
            const char* e = rp_regist_error();
            snprintf(resp, resp_size, "{\"status\":\"%s\"}",
                     ok ? "pairing" : (e[0] ? e : "busy"));
            return ok;
        }
    }
    // Action: PSN sign-in. Browser hands us the authorization code from Sony's
    // post-login redirect; the device does the HTTPS token exchange on-chip and
    // derives the 8-byte account id (browser can't — CORS + Sony anti-bot edge).
    {
        char code[260];
        if (json_str(json, "psn_code", code, sizeof(code))) {
            if (!wifi_station_is_connected()) {
                snprintf(resp, resp_size, "{\"status\":\"error\",\"error\":\"wifi not connected\"}");
                return false;
            }
            bool ok = rp_oauth_start(code);
            snprintf(resp, resp_size, "{\"status\":\"%s\"}", ok ? "signing-in" : "busy");
            return ok;
        }
    }

    bool changed = false;
    char ssid[RP_SSID_MAX], pass[RP_PASS_MAX], ip[RP_IP_MAX], hex[80];

    if (json_str(json, "wifi_ssid", ssid, sizeof(ssid))) {
        if (!json_str(json, "wifi_pass", pass, sizeof(pass))) pass[0] = '\0';
        if (rp_config_set_wifi(ssid, pass)) { changed = true; wifi_station_connect(); }
    }
    if (json_str(json, "ps5_ip", ip, sizeof(ip))) {
        if (rp_config_set_ps5_ip(ip)) changed = true;
    }
    if (json_str(json, "account_id", hex, sizeof(hex))) {
        uint8_t id[RP_ACCOUNT_LEN];
        if (hex_bytes(hex, id, RP_ACCOUNT_LEN)) { rp_config_set_account_id(id); changed = true; }
    }
    if (json_str(json, "rp_key", hex, sizeof(hex))) {
        uint8_t k[RP_KEY_LEN], rk[RP_REGIST_LEN] = {0};
        char rhex[80];
        if (json_str(json, "regist_key", rhex, sizeof(rhex))) hex_bytes(rhex, rk, RP_REGIST_LEN);
        if (hex_bytes(hex, k, RP_KEY_LEN)) { rp_config_set_keys(k, rk); changed = true; }
    }
    if (changed) rp_config_save();
    snprintf(resp, resp_size, "{\"status\":\"%s\"}", changed ? "ok" : "no-change");
    return changed;
}

const OutputInterface remoteplay_output_interface = {
    .name = "PS Remote Play",
    .target = OUTPUT_TARGET_REMOTE_PLAY,
    .init = rp_out_init,
    .task = rp_out_task,
    .core1_task = NULL,
    .get_feedback = rp_out_get_feedback,
    .get_rumble = NULL,
    .get_player_led = NULL,
    .get_profile_count = NULL,
    .get_active_profile = NULL,
    .set_active_profile = NULL,
    .get_profile_name = NULL,
    .get_trigger_threshold = NULL,
    .get_native_config = rp_out_get_native_config,
    .set_native_config = rp_out_set_native_config,
};
