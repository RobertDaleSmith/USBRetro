// rp_regist.c - On-device PS5 registration (pairing) over raw lwip
// SPDX-License-Identifier: Apache-2.0
//
// Implements the chiaki registration handshake against a PS5 on the LAN:
//   1. UDP "search": send "SRC3\0" to <ip>:9295, wait for a "RES3..." reply
//      (wakes the console's registration listener). Short settle delay.
//   2. TCP: connect <ip>:9295, send an HTTP POST whose body is AES-CFB encrypted
//      with a key derived from the console PIN + our PSN account id + a random
//      ambassador (chiaki rpcrypt). The console replies with an encrypted body
//      carrying RP-Key + PS5-RegistKey.
//   3. Decrypt + parse the response, store RP-Key + Regist Key into rp_config.
//
// The crypto is chiaki's rpcrypt.c (compiled with mbedTLS). Everything else —
// the payload framing, the network state machine — is here, on raw lwip (NO_SYS
// poll mode), driven by rp_regist_task(). Only compiled when chiaki is vendored;
// otherwise rp_regist_stub.c stands in. See rp_regist.h.

#include "rp_regist.h"
#include "rp_config.h"
#include "wifi_station.h"

#include "pico/cyw43_arch.h"
#include "pico/rand.h"
#include "lwip/udp.h"
#include "lwip/tcp.h"
#include "lwip/pbuf.h"
#include "lwip/ip_addr.h"

#include <chiaki/rpcrypt.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define REGIST_PORT     9295
#define REGIST_TARGET   CHIAKI_TARGET_PS5_1
// PS5 client type (from chiaki regist.c). Identifies us as a Remote Play client.
#define CLIENT_TYPE     "dabfa2ec873de5839bee8d3f4c0239c4282c07c25c6077a2931afcf0adc0d34f"

#define INNER_OFF       0x1e0        // encrypted inner-header offset in the payload
#define SEARCH_TIMEOUT  4000
#define PROBE_INTERVAL  200
#define SETTLE_MS       200          // PS5 doesn't accept the TCP request immediately
#define RESP_TIMEOUT    5000
#define RESP_MAX        1500

static rp_regist_state_t s_state = RP_REGIST_IDLE;
static char              s_err[96];

static ChiakiRPCrypt     s_crypt;    // kept between request build and response decrypt
static uint8_t           s_payload[0x400];
static size_t            s_payload_size;
static char              s_req[0x500];
static size_t            s_req_size;

static ip_addr_t         s_ip;
static uint32_t          s_pending_pin;   // console PIN, used when building the request
static uint32_t          s_deadline;
static uint32_t          s_last_probe;
static uint32_t          s_settle_until;
static bool              s_res_ok;

static struct udp_pcb*   s_udp = NULL;
static struct tcp_pcb*   s_tcp = NULL;

static uint8_t           s_resp[RESP_MAX];
static uint16_t          s_resp_len;

static uint32_t now_ms(void) { return to_ms_since_boot(get_absolute_time()); }

static void fail(const char* msg)
{
    snprintf(s_err, sizeof(s_err), "%s", msg);
    s_state = RP_REGIST_ERROR;
    printf("[rp_regist] error: %s\n", msg);
}

// --- base64 (account id -> Np-AccountId), standard alphabet with padding ------
static void b64_encode(const uint8_t* in, int n, char* out)
{
    static const char* A = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int o = 0;
    for (int i = 0; i < n; i += 3) {
        uint32_t v = in[i] << 16;
        if (i + 1 < n) v |= in[i + 1] << 8;
        if (i + 2 < n) v |= in[i + 2];
        out[o++] = A[(v >> 18) & 0x3f];
        out[o++] = A[(v >> 12) & 0x3f];
        out[o++] = (i + 1 < n) ? A[(v >> 6) & 0x3f] : '=';
        out[o++] = (i + 2 < n) ? A[v & 0x3f] : '=';
    }
    out[o] = '\0';
}

// --- hex string -> bytes (returns bytes decoded, -1 on error) -----------------
static int hex_decode(const char* hex, int hexlen, uint8_t* out, int outmax)
{
    if (hexlen % 2) return -1;
    int n = hexlen / 2;
    if (n > outmax) return -1;
    for (int i = 0; i < n; i++) {
        char b[3] = { hex[i * 2], hex[i * 2 + 1], 0 };
        char* end = NULL;
        long v = strtol(b, &end, 16);
        if (end != b + 2) return -1;
        out[i] = (uint8_t)v;
    }
    return n;
}

// Extract a "Key: value" line's value (up to CR/LF) from a header blob.
static bool header_value(const char* body, int body_len, const char* key,
                         char* out, int outmax)
{
    int klen = (int)strlen(key);
    for (int i = 0; i + klen < body_len; i++) {
        if (memcmp(body + i, key, klen) == 0 && body[i + klen] == ':') {
            const char* p = body + i + klen + 1;
            const char* end = body + body_len;
            while (p < end && (*p == ' ' || *p == '\t')) p++;
            int o = 0;
            while (p < end && *p != '\r' && *p != '\n' && o < outmax - 1) out[o++] = *p++;
            out[o] = '\0';
            return true;
        }
    }
    return false;
}

// --- payload + request framing (chiaki regist request, PS5 path) --------------
static bool build_request(void)
{
    rp_config_t* cfg = rp_config_get();

    uint8_t* buf = s_payload;
    memset(buf, 'A', INNER_OFF);             // deterministic fill (chiaki does the same)
    size_t key_0_off = buf[0x18D] & 0x1F;
    size_t key_1_off = buf[0] >> 3;

    uint8_t ambassador[CHIAKI_RPCRYPT_KEY_SIZE];
    for (int i = 0; i < CHIAKI_RPCRYPT_KEY_SIZE; i += 4) {
        uint32_t r = get_rand_32();
        memcpy(ambassador + i, &r, 4);
    }

    if (chiaki_rpcrypt_init_regist(&s_crypt, REGIST_TARGET, ambassador, key_0_off,
                                   s_pending_pin) != CHIAKI_ERR_SUCCESS) {
        fail("crypt init failed"); return false;
    }
    uint8_t aeropause[0x10];
    if (chiaki_rpcrypt_aeropause(REGIST_TARGET, key_1_off, aeropause,
                                 s_crypt.ambassador) != CHIAKI_ERR_SUCCESS) {
        fail("aeropause failed"); return false;
    }
    memcpy(buf + 0xc7, aeropause + 8, 8);
    memcpy(buf + 0x191, aeropause, 8);

    char b64[24];
    b64_encode(cfg->account_id, RP_ACCOUNT_LEN, b64);
    int inner = snprintf((char*)buf + INNER_OFF, sizeof(s_payload) - INNER_OFF,
                         "Client-Type: %s\r\nNp-AccountId: %s\r\n", CLIENT_TYPE, b64);
    if (inner < 0 || inner >= (int)(sizeof(s_payload) - INNER_OFF)) {
        fail("inner header too big"); return false;
    }
    if (chiaki_rpcrypt_encrypt(&s_crypt, 0, buf + INNER_OFF, buf + INNER_OFF,
                               inner) != CHIAKI_ERR_SUCCESS) {
        fail("payload encrypt failed"); return false;
    }
    s_payload_size = INNER_OFF + inner;

    char ip[16]; wifi_station_get_ip(ip, sizeof(ip));
    if (!ip[0]) snprintf(ip, sizeof(ip), "10.0.2.15");
    int n = snprintf(s_req, sizeof(s_req),
        "POST /sie/ps5/rp/sess/rgst HTTP/1.1\r\n HTTP/1.1\r\n"
        "HOST: %s\r\n"
        "User-Agent: remoteplay Windows\r\n"
        "Connection: close\r\n"
        "Content-Length: %u\r\n"
        "RP-Version: 1.0\r\n"
        "\r\n",
        ip, (unsigned)s_payload_size);
    if (n < 0 || n + s_payload_size > sizeof(s_req)) { fail("request too big"); return false; }
    memcpy(s_req + n, s_payload, s_payload_size);
    s_req_size = n + s_payload_size;
    return true;
}

// --- response parse -----------------------------------------------------------
static void parse_response(void)
{
    s_resp[s_resp_len < RESP_MAX ? s_resp_len : RESP_MAX - 1] = '\0';
    // Status line
    const char* sp = strchr((char*)s_resp, ' ');
    if (!sp || sp[1] != '2') { fail("console rejected pairing (bad PIN?)"); return; }
    // Header/body split
    char* hdr_end = strstr((char*)s_resp, "\r\n\r\n");
    if (!hdr_end) { fail("no response header"); return; }
    int header_size = (int)(hdr_end - (char*)s_resp) + 4;

    char clbuf[12];
    if (!header_value((char*)s_resp, header_size, "Content-Length", clbuf, sizeof(clbuf))) {
        fail("no content-length"); return;
    }
    int content = atoi(clbuf);
    if (content <= 0 || header_size + content > s_resp_len) { fail("short response body"); return; }

    // Body is AES-CFB encrypted with the same crypt at counter 0.
    uint8_t* body = s_resp + header_size;
    if (chiaki_rpcrypt_decrypt(&s_crypt, 0, body, body, content) != CHIAKI_ERR_SUCCESS) {
        fail("response decrypt failed"); return;
    }

    char rp_hex[48], regist_hex[48];
    uint8_t rp_key[RP_KEY_LEN], regist_key[RP_REGIST_LEN];
    // RP-Key is exactly 16 bytes (32 hex).
    if (!header_value((char*)body, content, "RP-Key", rp_hex, sizeof(rp_hex)) ||
        hex_decode(rp_hex, (int)strlen(rp_hex), rp_key, RP_KEY_LEN) != RP_KEY_LEN) {
        fail("no RP-Key in response"); return;
    }
    // RegistKey is variable-length (up to 16 bytes), zero-padded — like chiaki's
    // parse_hex. Don't require a full 16 bytes (an 8-char key decodes to 4).
    memset(regist_key, 0, sizeof(regist_key));
    if (!header_value((char*)body, content, "PS5-RegistKey", regist_hex, sizeof(regist_hex))) {
        fail("no RegistKey in response"); return;
    }
    if (hex_decode(regist_hex, (int)strlen(regist_hex), regist_key, RP_REGIST_LEN) < 0) {
        char m[56]; snprintf(m, sizeof(m), "bad RegistKey (len %u)", (unsigned)strlen(regist_hex));
        fail(m); return;
    }

    rp_config_set_keys(rp_key, regist_key);
    rp_config_save();
    s_state = RP_REGIST_DONE;
    printf("[rp_regist] paired — RP-Key + Regist Key stored\n");
}

// --- lwip TCP -----------------------------------------------------------------
static void tcp_cleanup(void)
{
    if (s_tcp) {
        tcp_arg(s_tcp, NULL);
        tcp_recv(s_tcp, NULL);
        tcp_err(s_tcp, NULL);
        tcp_close(s_tcp);
        s_tcp = NULL;
    }
}

static err_t tcp_recv_cb(void* arg, struct tcp_pcb* pcb, struct pbuf* p, err_t err)
{
    (void)arg; (void)err;
    if (!p) {                          // console closed -> parse what we have
        if (s_state == RP_REGIST_RECV) parse_response();
        tcp_cleanup();
        return ERR_OK;
    }
    for (struct pbuf* q = p; q; q = q->next) {
        uint16_t space = (s_resp_len < RESP_MAX) ? (RESP_MAX - s_resp_len) : 0;
        uint16_t take = q->len < space ? q->len : space;
        if (take) { memcpy(s_resp + s_resp_len, q->payload, take); s_resp_len += take; }
    }
    tcp_recved(pcb, p->tot_len);
    pbuf_free(p);

    // Parse as soon as the full Content-Length body has arrived.
    char* he = strstr((char*)s_resp, "\r\n\r\n");
    if (he) {
        int header_size = (int)(he - (char*)s_resp) + 4;
        char clbuf[12];
        if (header_value((char*)s_resp, header_size, "Content-Length", clbuf, sizeof(clbuf))) {
            int content = atoi(clbuf);
            if (content > 0 && header_size + content <= s_resp_len) {
                parse_response();
                tcp_cleanup();
            }
        }
    }
    return ERR_OK;
}

static void tcp_err_cb(void* arg, err_t err)
{
    (void)arg;
    s_tcp = NULL;
    if (s_state == RP_REGIST_CONNECT || s_state == RP_REGIST_SEND || s_state == RP_REGIST_RECV) {
        char m[48]; snprintf(m, sizeof(m), "tcp error %d", (int)err);
        fail(m);
    }
}

static err_t tcp_connected_cb(void* arg, struct tcp_pcb* pcb, err_t err)
{
    (void)arg;
    if (err != ERR_OK) { fail("tcp connect failed"); return ERR_OK; }
    s_state = RP_REGIST_SEND;
    err_t e = tcp_write(pcb, s_req, (u16_t)s_req_size, TCP_WRITE_FLAG_COPY);
    if (e == ERR_OK) e = tcp_output(pcb);
    if (e != ERR_OK) { fail("tcp send failed"); tcp_cleanup(); return ERR_OK; }
    s_resp_len = 0;
    s_deadline = now_ms() + RESP_TIMEOUT;
    s_state = RP_REGIST_RECV;
    return ERR_OK;
}

static void start_tcp(void)
{
    s_tcp = tcp_new();
    if (!s_tcp) { fail("tcp alloc failed"); return; }
    tcp_arg(s_tcp, NULL);
    tcp_recv(s_tcp, tcp_recv_cb);
    tcp_err(s_tcp, tcp_err_cb);
    s_state = RP_REGIST_CONNECT;
    s_deadline = now_ms() + RESP_TIMEOUT;
    if (tcp_connect(s_tcp, &s_ip, REGIST_PORT, tcp_connected_cb) != ERR_OK)
        fail("tcp connect start failed");
}

// --- lwip UDP search ----------------------------------------------------------
static void udp_recv_cb(void* arg, struct udp_pcb* pcb, struct pbuf* p,
                        const ip_addr_t* addr, u16_t port)
{
    (void)arg; (void)pcb; (void)addr; (void)port;
    if (!p) return;
    char b[8];
    int n = p->tot_len < 7 ? p->tot_len : 7;
    pbuf_copy_partial(p, b, n, 0); b[n] = '\0';
    pbuf_free(p);
    if (n >= 4 && memcmp(b, "RES3", 4) == 0) s_res_ok = true;
}

static void udp_cleanup(void)
{
    if (s_udp) { udp_recv(s_udp, NULL, NULL); udp_remove(s_udp); s_udp = NULL; }
}

static void search_probe(void)
{
    if (!s_udp) {
        s_udp = udp_new();
        if (!s_udp) { fail("udp alloc failed"); return; }
        udp_bind(s_udp, IP_ANY_TYPE, 0);
        udp_recv(s_udp, udp_recv_cb, NULL);
    }
    struct pbuf* pb = pbuf_alloc(PBUF_TRANSPORT, 5, PBUF_RAM);  // "SRC3\0"
    if (!pb) return;
    memcpy(pb->payload, "SRC3", 5);
    cyw43_arch_lwip_begin();
    udp_sendto(s_udp, pb, &s_ip, REGIST_PORT);
    cyw43_arch_lwip_end();
    pbuf_free(pb);
}

// --- public API ---------------------------------------------------------------
void rp_regist_init(void)
{
    s_state = RP_REGIST_IDLE;
    s_err[0] = '\0';
}

bool rp_regist_start(const char* ps5_ip, uint32_t pin)
{
    if (s_state == RP_REGIST_SEARCH || s_state == RP_REGIST_CONNECT ||
        s_state == RP_REGIST_SEND || s_state == RP_REGIST_RECV) return false; // busy
    if (!wifi_station_is_connected()) { fail("wifi not connected"); return false; }

    rp_config_t* cfg = rp_config_get();
    bool have_acct = false;
    for (int i = 0; i < RP_ACCOUNT_LEN; i++) if (cfg->account_id[i]) { have_acct = true; break; }
    if (!have_acct) { fail("sign in to PSN first"); return false; }
    if (!ps5_ip || !ipaddr_aton(ps5_ip, &s_ip)) { fail("bad console IP"); return false; }

    s_pending_pin = pin;
    if (!build_request()) return false;   // sets error on failure

    udp_cleanup(); tcp_cleanup();
    s_res_ok = false;
    s_settle_until = 0;
    s_resp_len = 0;
    s_last_probe = 0;
    s_deadline = now_ms() + SEARCH_TIMEOUT;
    s_state = RP_REGIST_SEARCH;
    printf("[rp_regist] pairing with %s (pin set)\n", ps5_ip);
    return true;
}

void rp_regist_task(void)
{
    uint32_t now = now_ms();
    switch (s_state) {
        case RP_REGIST_SEARCH:
            if (s_res_ok) {
                if (!s_settle_until) { udp_cleanup(); s_settle_until = now + SETTLE_MS; }
                else if (now >= s_settle_until) start_tcp();
                break;
            }
            if ((int32_t)(now - s_deadline) >= 0) {
                udp_cleanup();
                fail("no reply from console (put it in Link Device mode)");
                break;
            }
            if (now - s_last_probe >= PROBE_INTERVAL) { s_last_probe = now; search_probe(); }
            break;
        case RP_REGIST_CONNECT: case RP_REGIST_SEND: case RP_REGIST_RECV:
            if ((int32_t)(now - s_deadline) >= 0) { tcp_cleanup(); fail("pairing timed out"); }
            break;
        default: break;
    }
}

rp_regist_state_t rp_regist_get_state(void) { return s_state; }
const char* rp_regist_error(void) { return s_err; }

const char* rp_regist_state_str(void)
{
    switch (s_state) {
        case RP_REGIST_IDLE:        return "idle";
        case RP_REGIST_SEARCH:      return "searching";
        case RP_REGIST_CONNECT:     return "connecting";
        case RP_REGIST_SEND:        return "sending";
        case RP_REGIST_RECV:        return "waiting";
        case RP_REGIST_DONE:        return "paired";
        case RP_REGIST_ERROR:       return "error";
        case RP_REGIST_UNAVAILABLE: return "unavailable";
    }
    return "?";
}
