// rp_proto.c - minimal proto2 wire codec (see rp_proto.h)
// SPDX-License-Identifier: Apache-2.0
#include "rp_proto.h"
#include <string.h>

// TakionMessage field numbers (takion.proto)
#define F_TKMSG_TYPE      1
#define F_TKMSG_BIG       2
#define F_TKMSG_BANG      3
#define F_TKMSG_CTRLCONN  22
// BigPayload
#define F_BIG_CLIENT_VER  1
#define F_BIG_SESSION_KEY 2
#define F_BIG_LAUNCH_SPEC 3
#define F_BIG_ENC_KEY     4
#define F_BIG_ECDH_PUB    5
#define F_BIG_ECDH_SIG    6
// BangPayload
#define F_BANG_ENC_ACCEPT 3
#define F_BANG_VER_ACCEPT 4
#define F_BANG_ECDH_PUB   8
#define F_BANG_ECDH_SIG   9
// ControllerConnectionPayload
#define F_CC_CONNECTED    2
#define F_CC_TYPE         3
#define CC_TYPE_DUALSHOCK4 2
#define CC_TYPE_DUALSENSE  6

#define WT_VARINT 0
#define WT_LEN    2

// --- writer -------------------------------------------------------------------
void rp_pb_init(rp_pb_writer* w, uint8_t* buf, size_t cap)
{ w->buf = buf; w->cap = cap; w->len = 0; w->ok = true; }

static void put_byte(rp_pb_writer* w, uint8_t b)
{ if (w->len < w->cap) w->buf[w->len++] = b; else w->ok = false; }

static void put_varint(rp_pb_writer* w, uint64_t v)
{ do { uint8_t b = v & 0x7f; v >>= 7; if (v) b |= 0x80; put_byte(w, b); } while (v); }

static void put_tag(rp_pb_writer* w, uint32_t field, uint32_t wt)
{ put_varint(w, ((uint64_t)field << 3) | wt); }

void rp_pb_varint_field(rp_pb_writer* w, uint32_t field, uint64_t v)
{ put_tag(w, field, WT_VARINT); put_varint(w, v); }

void rp_pb_bytes_field(rp_pb_writer* w, uint32_t field, const uint8_t* data, size_t n)
{
    put_tag(w, field, WT_LEN);
    put_varint(w, n);
    for (size_t i = 0; i < n; i++) put_byte(w, data[i]);
}

// Length-delimited sub-message: we reserve a single length byte, write children,
// then patch. All our sub-messages are < 128 bytes except BIG (launch_spec can be
// large) — so use a 2-byte reserved length and shift if needed. Simplest robust
// approach: write children to the tail, then insert the varint length. We instead
// reserve up to 3 length bytes and memmove to close the gap.
size_t rp_pb_submsg_begin(rp_pb_writer* w, uint32_t field)
{
    put_tag(w, field, WT_LEN);
    // reserve 3 bytes for the length varint (covers up to 2^21-1 payload)
    size_t mark = w->len;
    put_byte(w, 0); put_byte(w, 0); put_byte(w, 0);
    return mark;
}

void rp_pb_submsg_end(rp_pb_writer* w, size_t mark)
{
    if (!w->ok) return;
    size_t payload = w->len - (mark + 3);
    // encode length as varint
    uint8_t lb[3]; int ln = 0; uint64_t v = payload;
    do { uint8_t b = v & 0x7f; v >>= 7; if (v) b |= 0x80; lb[ln++] = b; } while (v);
    // shift payload left to sit right after the ln-byte length
    if (ln < 3) {
        memmove(w->buf + mark + ln, w->buf + mark + 3, payload);
        w->len -= (3 - ln);
    }
    memcpy(w->buf + mark, lb, ln);
}

// --- specific encoders --------------------------------------------------------
size_t rp_proto_encode_big(uint8_t* out, size_t out_cap,
                           uint32_t client_version,
                           const char* session_key,
                           const char* launch_spec_b64,
                           const uint8_t* ecdh_pub, size_t ecdh_pub_len,
                           const uint8_t* ecdh_sig, size_t ecdh_sig_len)
{
    rp_pb_writer w; rp_pb_init(&w, out, out_cap);
    rp_pb_varint_field(&w, F_TKMSG_TYPE, RP_TKMSG_BIG);
    size_t m = rp_pb_submsg_begin(&w, F_TKMSG_BIG);
    rp_pb_varint_field(&w, F_BIG_CLIENT_VER, client_version);
    rp_pb_bytes_field(&w, F_BIG_SESSION_KEY, (const uint8_t*)session_key, strlen(session_key));
    rp_pb_bytes_field(&w, F_BIG_LAUNCH_SPEC, (const uint8_t*)launch_spec_b64, strlen(launch_spec_b64));
    static const uint8_t zero4[4] = {0,0,0,0};
    rp_pb_bytes_field(&w, F_BIG_ENC_KEY, zero4, 4);
    rp_pb_bytes_field(&w, F_BIG_ECDH_PUB, ecdh_pub, ecdh_pub_len);
    rp_pb_bytes_field(&w, F_BIG_ECDH_SIG, ecdh_sig, ecdh_sig_len);
    rp_pb_submsg_end(&w, m);
    return w.ok ? w.len : 0;
}

size_t rp_proto_encode_controller_connection(uint8_t* out, size_t out_cap, bool dualsense)
{
    rp_pb_writer w; rp_pb_init(&w, out, out_cap);
    rp_pb_varint_field(&w, F_TKMSG_TYPE, RP_TKMSG_CONTROLLERCONNECTION);
    size_t m = rp_pb_submsg_begin(&w, F_TKMSG_CTRLCONN);
    rp_pb_varint_field(&w, F_CC_CONNECTED, 1);
    rp_pb_varint_field(&w, F_CC_TYPE, dualsense ? CC_TYPE_DUALSENSE : CC_TYPE_DUALSHOCK4);
    rp_pb_submsg_end(&w, m);
    return w.ok ? w.len : 0;
}

size_t rp_proto_encode_type_only(uint8_t* out, size_t out_cap, uint32_t type)
{
    rp_pb_writer w; rp_pb_init(&w, out, out_cap);
    rp_pb_varint_field(&w, F_TKMSG_TYPE, type);
    return w.ok ? w.len : 0;
}

// --- reader -------------------------------------------------------------------
static bool get_varint(const uint8_t** p, const uint8_t* end, uint64_t* out)
{
    uint64_t v = 0; int shift = 0;
    while (*p < end) {
        uint8_t b = *(*p)++;
        v |= (uint64_t)(b & 0x7f) << shift;
        if (!(b & 0x80)) { *out = v; return true; }
        shift += 7;
        if (shift >= 64) return false;
    }
    return false;
}

// Walk fields, calling back with (field, wiretype, data-ptr, data-len).
// For varint, data points at the value already parsed (passed via val).
static bool parse_bang_payload(const uint8_t* buf, size_t len, rp_bang_t* b)
{
    const uint8_t* p = buf; const uint8_t* end = buf + len;
    while (p < end) {
        uint64_t tag; if (!get_varint(&p, end, &tag)) return false;
        uint32_t field = tag >> 3, wt = tag & 7;
        if (wt == WT_VARINT) {
            uint64_t v; if (!get_varint(&p, end, &v)) return false;
            if (field == F_BANG_ENC_ACCEPT) b->encrypted_key_accepted = (v != 0);
            else if (field == F_BANG_VER_ACCEPT) b->version_accepted = (v != 0);
        } else if (wt == WT_LEN) {
            uint64_t n; if (!get_varint(&p, end, &n)) return false;
            if ((uint64_t)(end - p) < n) return false;
            if (field == F_BANG_ECDH_PUB && n <= sizeof(b->ecdh_pub)) {
                memcpy(b->ecdh_pub, p, n); b->ecdh_pub_len = n;
            } else if (field == F_BANG_ECDH_SIG && n <= sizeof(b->ecdh_sig)) {
                memcpy(b->ecdh_sig, p, n); b->ecdh_sig_len = n;
            }
            p += n;
        } else if (wt == 5) { p += 4; }
        else if (wt == 1) { p += 8; }
        else return false;
    }
    return true;
}

int rp_proto_parse_message(const uint8_t* buf, size_t len, rp_bang_t* bang)
{
    const uint8_t* p = buf; const uint8_t* end = buf + len;
    int type = -1;
    if (bang) memset(bang, 0, sizeof(*bang));
    while (p < end) {
        uint64_t tag; if (!get_varint(&p, end, &tag)) return -1;
        uint32_t field = tag >> 3, wt = tag & 7;
        if (wt == WT_VARINT) {
            uint64_t v; if (!get_varint(&p, end, &v)) return -1;
            if (field == F_TKMSG_TYPE) type = (int)v;
        } else if (wt == WT_LEN) {
            uint64_t n; if (!get_varint(&p, end, &n)) return -1;
            if ((uint64_t)(end - p) < n) return -1;
            if (field == F_TKMSG_BANG && bang) { bang->found = true; parse_bang_payload(p, n, bang); }
            p += n;
        } else if (wt == 5) { p += 4; }
        else if (wt == 1) { p += 8; }
        else return -1;
    }
    return type;
}
