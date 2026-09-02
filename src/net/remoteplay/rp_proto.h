// rp_proto.h - minimal proto2 wire codec for the Remote Play Takion messages we
// need (BIG encode, BANG decode, ControllerConnection encode, StreamInfoAck).
// Hand-rolled so we don't drag in nanopb. Field numbers from takion.proto.
// SPDX-License-Identifier: Apache-2.0

#ifndef RP_PROTO_H
#define RP_PROTO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// --- generic proto2 writer (append into a caller buffer) ----------------------
typedef struct { uint8_t* buf; size_t cap; size_t len; bool ok; } rp_pb_writer;

void rp_pb_init(rp_pb_writer* w, uint8_t* buf, size_t cap);
void rp_pb_varint_field(rp_pb_writer* w, uint32_t field, uint64_t v);
void rp_pb_bytes_field(rp_pb_writer* w, uint32_t field, const uint8_t* data, size_t n);
// begin/end a length-delimited sub-message: write children between the calls.
size_t rp_pb_submsg_begin(rp_pb_writer* w, uint32_t field);
void   rp_pb_submsg_end(rp_pb_writer* w, size_t mark);

// --- specific messages --------------------------------------------------------
// BIG (TakionMessage type=BIG). Returns encoded length, or 0 on overflow.
size_t rp_proto_encode_big(uint8_t* out, size_t out_cap,
                           uint32_t client_version,
                           const char* session_key,
                           const char* launch_spec_b64,
                           const uint8_t* ecdh_pub, size_t ecdh_pub_len,
                           const uint8_t* ecdh_sig, size_t ecdh_sig_len);

// CONTROLLERCONNECTION (connected=true, type dualsense/ds4). Returns length.
size_t rp_proto_encode_controller_connection(uint8_t* out, size_t out_cap, bool dualsense);

// A bare TakionMessage with only `type` set (e.g. STREAMINFOACK=14). Returns length.
size_t rp_proto_encode_type_only(uint8_t* out, size_t out_cap, uint32_t type);

// BANG decode result.
typedef struct {
    bool    found;
    bool    version_accepted;
    bool    encrypted_key_accepted;
    uint8_t ecdh_pub[160]; size_t ecdh_pub_len;
    uint8_t ecdh_sig[64];  size_t ecdh_sig_len;
} rp_bang_t;

// Parse a TakionMessage; returns its `type` (-1 on parse error). If it's a BANG,
// fills *bang. Works on the decrypted DATA payload (the reassembled protobuf).
int rp_proto_parse_message(const uint8_t* buf, size_t len, rp_bang_t* bang);

// TakionMessage type enum values we care about
#define RP_TKMSG_BIG          0
#define RP_TKMSG_BANG         1
#define RP_TKMSG_DISCONNECT   8
#define RP_TKMSG_STREAMINFO   13
#define RP_TKMSG_STREAMINFOACK 14
#define RP_TKMSG_CONTROLLERCONNECTION 21
#define RP_TKMSG_HEARTBEAT    3

#endif // RP_PROTO_H
