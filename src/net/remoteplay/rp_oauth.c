// rp_oauth.c - On-device PSN OAuth token exchange over HTTPS (mbedTLS/altcp)
// SPDX-License-Identifier: Apache-2.0
//
// Async state machine (NO_SYS=1, poll mode). Two sequential HTTPS requests to
// Sony's auth endpoint:
//   1. POST /2.0/oauth/token          (authorization_code -> access_token)
//   2. GET  /2.0/oauth/token/<token>  (access_token -> user_id/online_id)
// then derive the 8-byte little-endian PSN account id into rp_config.
//
// The exact endpoints, client credentials, and the full 4-scope set are lifted
// from a validated native implementation (mouthpad-utility RPOAuth.swift). The
// single-scope `psn:clientapp` request that older Python clients use now returns
// Sony's "Something went wrong." page, so the full set is required here too.

#include "rp_oauth.h"
#include "rp_config.h"

#include "pico/cyw43_arch.h"
#include "lwip/altcp.h"
#include "lwip/altcp_tls.h"
#include "lwip/dns.h"
#include "lwip/ip_addr.h"
#include "mbedtls/ssl.h"
#include "mbedtls/platform_time.h"
#include "mbedtls/memory_buffer_alloc.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// --- Sony OAuth constants (from RPOAuth.swift) -------------------------------
#define RP_OAUTH_HOST  "auth.api.sonyentertainmentnetwork.com"
#define RP_OAUTH_PORT  443
#define RP_OAUTH_PATH  "/2.0/oauth/token"
#define RP_REDIRECT    "https://remoteplay.dl.playstation.net/remoteplay/redirect"
// base64("<clientId>:<clientSecret>")
#define RP_BASIC_AUTH  "YmE0OTVhMjQtODE4Yy00NzJiLWIxMmQtZmYyMzFjMWI1NzQ1Om12YWlaa1JzQXNJMUlCa1k="
// space-joined, %20-encoded scope set (authorize + token must match)
#define RP_SCOPE_ENC \
    "psn:clientapp%20referenceDataService:countryConfig.read%20" \
    "pushNotification:webSocket.desktop.connect%20" \
    "sessionManager:remotePlaySession.system.update"

#define RP_TIMEOUT_MS  20000
#define RP_RESP_MAX    4096
#define RP_CODE_MAX    256
#define RP_TOKEN_MAX   256

// Dedicated mbedTLS arena. 64KB comfortably holds the TLS-1.2 handshake peak
// (16KB in + 2KB out + cert parse + ECDHE working set). Static → always present,
// never fragmented by the rest of the firmware. 8-byte aligned for the allocator.
#define RP_MBED_POOL_SIZE  (64 * 1024)
static uint8_t s_mbed_pool[RP_MBED_POOL_SIZE] __attribute__((aligned(8)));
static bool    s_pool_inited = false;

static rp_oauth_state_t s_state = RP_OAUTH_IDLE;
static char             s_err[80];
static char             s_online_id[24];

static char             s_code[RP_CODE_MAX];
static char             s_token[RP_TOKEN_MAX];
static ip_addr_t        s_ip;
static uint32_t         s_deadline_ms;
static bool             s_info_phase;      // false=token POST, true=info GET

static struct altcp_tls_config* s_tls_conf = NULL;
static struct altcp_pcb*        s_pcb = NULL;

static char     s_resp[RP_RESP_MAX];
static uint16_t s_resp_len;

static uint32_t now_ms(void) { return to_ms_since_boot(get_absolute_time()); }

// mbedTLS clock (MBEDTLS_PLATFORM_TIME_MACRO). No RTC; monotonic boot seconds is
// enough — we don't verify cert time windows and the server accepts any stamp.
long rp_mbedtls_time(long* t)
{
    long s = (long)(to_ms_since_boot(get_absolute_time()) / 1000);
    if (t) *t = s;
    return s;
}

// MBEDTLS_PLATFORM_MS_TIME_ALT hook — monotonic milliseconds since boot.
mbedtls_ms_time_t mbedtls_ms_time(void)
{
    return (mbedtls_ms_time_t)to_ms_since_boot(get_absolute_time());
}

// altcp callbacks (defined below)
static err_t oauth_recv(void*, struct altcp_pcb*, struct pbuf*, err_t);
static err_t oauth_connected(void*, struct altcp_pcb*, err_t);
static void  oauth_err(void*, err_t);

// --- tiny JSON string-field extractor: "key":"value" ------------------------
static bool json_str(const char* json, const char* key, char* out, int outlen)
{
    char pat[40];
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

static void fail(const char* msg)
{
    snprintf(s_err, sizeof(s_err), "%s", msg);
    s_state = RP_OAUTH_ERROR;
    printf("[rp_oauth] error: %s\n", msg);
}

// Error + mbedTLS pool high-water (so an out-of-arena failure is visible over
// CDC even with no UART): "<msg> (pool used=<cur>/<max> of 64k)".
static void fail_mem(const char* msg)
{
    size_t cur = 0, curb = 0, mx = 0, mxb = 0;
    mbedtls_memory_buffer_alloc_cur_get(&cur, &curb);
    mbedtls_memory_buffer_alloc_max_get(&mx, &mxb);
    (void)curb; (void)mxb;
    snprintf(s_err, sizeof(s_err), "%s (pool %u/%u of %uk)",
             msg, (unsigned)cur, (unsigned)mx, (unsigned)(RP_MBED_POOL_SIZE / 1024));
    s_state = RP_OAUTH_ERROR;
    printf("[rp_oauth] error: %s\n", s_err);
}

// Tear down the current TLS connection (config kept until the whole flow ends).
static void conn_cleanup(void)
{
    if (s_pcb) {
        altcp_arg(s_pcb, NULL);
        altcp_recv(s_pcb, NULL);
        altcp_err(s_pcb, NULL);
        altcp_poll(s_pcb, NULL, 0);
        altcp_close(s_pcb);
        s_pcb = NULL;
    }
}

static void flow_cleanup(void)
{
    conn_cleanup();
    if (s_tls_conf) {
        altcp_tls_free_config(s_tls_conf);
        s_tls_conf = NULL;
    }
}

// Parse the HTTP body out of s_resp (past the header terminator).
static const char* http_body(void)
{
    const char* b = strstr(s_resp, "\r\n\r\n");
    return b ? b + 4 : s_resp;
}

static bool http_status_ok(void)
{
    // First line "HTTP/1.1 200 ...".
    const char* sp = strchr(s_resp, ' ');
    return sp && sp[1] == '2';   // 2xx
}

// --- account id derivation --------------------------------------------------
static void finish_with_account(const char* body)
{
    char user_id[24];
    if (!json_str(body, "user_id", user_id, sizeof(user_id))) {
        fail("no user_id in response");
        return;
    }
    if (!json_str(body, "online_id", s_online_id, sizeof(s_online_id)))
        s_online_id[0] = '\0';

    // user_id is a decimal string -> uint64 -> little-endian 8 bytes.
    char* end = NULL;
    unsigned long long v = strtoull(user_id, &end, 10);
    if (end == user_id) { fail("bad user_id format"); return; }
    uint8_t id[RP_ACCOUNT_LEN];
    for (int i = 0; i < RP_ACCOUNT_LEN; i++) id[i] = (uint8_t)((v >> (8 * i)) & 0xFF);

    rp_config_set_account_id(id);
    rp_config_save();
    s_state = RP_OAUTH_DONE;
    printf("[rp_oauth] linked account online_id='%s'\n",
           s_online_id[0] ? s_online_id : "(unknown)");
}

// --- request building -------------------------------------------------------
static void send_request(void)
{
    char req[1024];
    int n;
    if (!s_info_phase) {
        char body[RP_CODE_MAX + 256];
        int bn = snprintf(body, sizeof(body),
            "grant_type=authorization_code&code=%s&scope=%s&redirect_uri=%s&",
            s_code, RP_SCOPE_ENC, RP_REDIRECT);
        n = snprintf(req, sizeof(req),
            "POST %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "Authorization: Basic %s\r\n"
            "Content-Type: application/x-www-form-urlencoded\r\n"
            "Content-Length: %d\r\n"
            "Connection: close\r\n"
            "\r\n%s",
            RP_OAUTH_PATH, RP_OAUTH_HOST, RP_BASIC_AUTH, bn, body);
    } else {
        n = snprintf(req, sizeof(req),
            "GET %s/%s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "Authorization: Basic %s\r\n"
            "Connection: close\r\n"
            "\r\n",
            RP_OAUTH_PATH, s_token, RP_OAUTH_HOST, RP_BASIC_AUTH);
    }
    if (n <= 0 || n >= (int)sizeof(req)) { fail("request too large"); return; }

    err_t e = altcp_write(s_pcb, req, (u16_t)n, TCP_WRITE_FLAG_COPY);
    if (e == ERR_OK) e = altcp_output(s_pcb);
    if (e != ERR_OK) { fail("tls write failed"); return; }
    s_resp_len = 0;
    s_resp[0] = '\0';
    s_state = s_info_phase ? RP_OAUTH_INFO : RP_OAUTH_TOKEN;
}

// A completed response (peer closed) -> advance the flow.
static void on_response_complete(void)
{
    s_resp[s_resp_len] = '\0';
    if (!http_status_ok()) {
        conn_cleanup();
        fail(s_info_phase ? "account info rejected" : "token exchange rejected");
        return;
    }
    const char* body = http_body();
    if (!s_info_phase) {
        if (!json_str(body, "access_token", s_token, sizeof(s_token))) {
            conn_cleanup();
            fail("no access_token");
            return;
        }
        conn_cleanup();
        // Kick off phase 2 (info GET) on a fresh connection.
        s_info_phase = true;
        s_state = RP_OAUTH_CONNECTING;
        s_pcb = altcp_tls_new(s_tls_conf, IPADDR_TYPE_V4);
        if (!s_pcb) { fail_mem("tls alloc failed (phase2)"); return; }
        mbedtls_ssl_set_hostname((mbedtls_ssl_context*)altcp_tls_context(s_pcb),
                                 RP_OAUTH_HOST);
        altcp_arg(s_pcb, NULL);
        altcp_recv(s_pcb, oauth_recv);
        altcp_err(s_pcb, oauth_err);
        err_t e = altcp_connect(s_pcb, &s_ip, RP_OAUTH_PORT, oauth_connected);
        if (e != ERR_OK) fail("connect failed (phase2)");
        return;
    }
    // Phase 2 complete.
    finish_with_account(body);
    conn_cleanup();
}

// --- altcp callbacks --------------------------------------------------------
static err_t oauth_recv(void* arg, struct altcp_pcb* conn, struct pbuf* p, err_t err)
{
    (void)arg; (void)err;
    if (!p) {                       // peer closed -> response complete
        on_response_complete();
        return ERR_OK;
    }
    for (struct pbuf* q = p; q; q = q->next) {
        uint16_t space = (s_resp_len < RP_RESP_MAX - 1)
                       ? (RP_RESP_MAX - 1 - s_resp_len) : 0;
        uint16_t take = q->len < space ? q->len : space;
        if (take) { memcpy(s_resp + s_resp_len, q->payload, take); s_resp_len += take; }
    }
    altcp_recved(conn, p->tot_len);
    pbuf_free(p);
    return ERR_OK;
}

static err_t oauth_connected(void* arg, struct altcp_pcb* conn, err_t err)
{
    (void)arg; (void)conn;
    if (err != ERR_OK) { fail("tls connect error"); return ERR_OK; }
    send_request();
    return ERR_OK;
}

static void oauth_err(void* arg, err_t err)
{
    (void)arg;
    s_pcb = NULL;   // pcb already freed by lwip
    if (s_state != RP_OAUTH_DONE && s_state != RP_OAUTH_ERROR) {
        char m[48]; snprintf(m, sizeof(m), "tls error %d", (int)err);
        fail(m);
    }
}

// --- DNS --------------------------------------------------------------------
static void start_connect(void)
{
    s_state = RP_OAUTH_CONNECTING;
    s_pcb = altcp_tls_new(s_tls_conf, IPADDR_TYPE_V4);
    if (!s_pcb) { fail_mem("tls alloc failed"); return; }
    mbedtls_ssl_set_hostname((mbedtls_ssl_context*)altcp_tls_context(s_pcb),
                             RP_OAUTH_HOST);
    altcp_arg(s_pcb, NULL);
    altcp_recv(s_pcb, oauth_recv);
    altcp_err(s_pcb, oauth_err);
    err_t e = altcp_connect(s_pcb, &s_ip, RP_OAUTH_PORT, oauth_connected);
    if (e != ERR_OK) fail("connect failed");
}

static void dns_cb(const char* name, const ip_addr_t* ipaddr, void* arg)
{
    (void)name; (void)arg;
    if (!ipaddr) { fail("dns lookup failed"); return; }
    s_ip = *ipaddr;
    start_connect();
}

// --- public API -------------------------------------------------------------
void rp_oauth_init(void)
{
    // Hand mbedTLS its dedicated arena before any TLS allocation happens.
    if (!s_pool_inited) {
        mbedtls_memory_buffer_alloc_init(s_mbed_pool, sizeof(s_mbed_pool));
        s_pool_inited = true;
    }
    s_state = RP_OAUTH_IDLE;
    s_err[0] = s_online_id[0] = '\0';
}

bool rp_oauth_start(const char* code)
{
    if (!code || !code[0]) return false;
    if (s_state == RP_OAUTH_RESOLVING || s_state == RP_OAUTH_CONNECTING ||
        s_state == RP_OAUTH_TOKEN || s_state == RP_OAUTH_INFO) return false; // busy

    flow_cleanup();
    snprintf(s_code, sizeof(s_code), "%s", code);
    s_token[0] = s_online_id[0] = s_err[0] = '\0';
    s_info_phase = false;
    s_resp_len = 0;
    s_deadline_ms = now_ms() + RP_TIMEOUT_MS;

    s_tls_conf = altcp_tls_create_config_client(NULL, 0);  // no CA -> VERIFY_NONE
    if (!s_tls_conf) { fail_mem("tls config alloc failed"); return false; }

    printf("[rp_oauth] starting exchange (code len=%u)\n", (unsigned)strlen(code));
    s_state = RP_OAUTH_RESOLVING;

    cyw43_arch_lwip_begin();
    err_t e = dns_gethostbyname(RP_OAUTH_HOST, &s_ip, dns_cb, NULL);
    cyw43_arch_lwip_end();
    if (e == ERR_OK) {           // cached -> resolve immediately
        start_connect();
    } else if (e != ERR_INPROGRESS) {
        fail("dns start failed");
        return false;
    }
    return true;
}

void rp_oauth_task(void)
{
    switch (s_state) {
        case RP_OAUTH_RESOLVING: case RP_OAUTH_CONNECTING:
        case RP_OAUTH_TOKEN:     case RP_OAUTH_INFO:
            if ((int32_t)(now_ms() - s_deadline_ms) >= 0) {
                conn_cleanup();
                fail("timed out");
            }
            break;
        case RP_OAUTH_DONE: case RP_OAUTH_ERROR:
            if (s_tls_conf) flow_cleanup();   // release TLS RAM once settled
            break;
        default: break;
    }
}

rp_oauth_state_t rp_oauth_get_state(void) { return s_state; }
const char* rp_oauth_error(void)     { return s_err; }
const char* rp_oauth_online_id(void) { return s_online_id; }

const char* rp_oauth_state_str(void)
{
    switch (s_state) {
        case RP_OAUTH_IDLE:       return "idle";
        case RP_OAUTH_RESOLVING:  return "resolving";
        case RP_OAUTH_CONNECTING: return "connecting";
        case RP_OAUTH_TOKEN:      return "token";
        case RP_OAUTH_INFO:       return "account";
        case RP_OAUTH_DONE:       return "done";
        case RP_OAUTH_ERROR:      return "error";
    }
    return "?";
}
