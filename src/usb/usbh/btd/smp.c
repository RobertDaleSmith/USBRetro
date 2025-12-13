// smp.c - Security Manager Protocol (SMP) implementation for BLE
// Implements "Just Works" pairing for BLE HID devices

#include "smp.h"
#include "btd.h"
#include "l2cap.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ============================================================================
// STATIC DATA
// ============================================================================

#define SMP_MAX_CONTEXTS 4
static smp_context_t smp_contexts[SMP_MAX_CONTEXTS];

// Simple random number generation (not cryptographically secure but works for our purpose)
static uint32_t smp_rand_seed = 12345;

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

static smp_context_t* smp_get_context(uint8_t conn_index)
{
    for (int i = 0; i < SMP_MAX_CONTEXTS; i++) {
        if (smp_contexts[i].conn_index == conn_index && smp_contexts[i].handle != 0) {
            return &smp_contexts[i];
        }
    }
    return NULL;
}

static smp_context_t* smp_alloc_context(uint8_t conn_index, uint16_t handle)
{
    for (int i = 0; i < SMP_MAX_CONTEXTS; i++) {
        if (smp_contexts[i].handle == 0) {
            memset(&smp_contexts[i], 0, sizeof(smp_context_t));
            smp_contexts[i].conn_index = conn_index;
            smp_contexts[i].handle = handle;
            smp_contexts[i].state = SMP_STATE_IDLE;
            return &smp_contexts[i];
        }
    }
    return NULL;
}

static void smp_free_context(smp_context_t* ctx)
{
    if (ctx) {
        memset(ctx, 0, sizeof(smp_context_t));
    }
}

// Simple pseudo-random generator
static uint8_t smp_random_byte(void)
{
    smp_rand_seed = smp_rand_seed * 1103515245 + 12345;
    return (uint8_t)(smp_rand_seed >> 16);
}

static void smp_generate_random(uint8_t* out, int len)
{
    // Seed with some entropy from time
    extern uint32_t time_us_32(void);
    smp_rand_seed ^= time_us_32();

    for (int i = 0; i < len; i++) {
        out[i] = smp_random_byte();
    }
}

// ============================================================================
// AES-128 ENCRYPTION (Simple software implementation)
// ============================================================================

// AES S-box
static const uint8_t aes_sbox[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
};

// Round constants
static const uint8_t aes_rcon[11] = {
    0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36
};

// Multiply by 2 in GF(2^8)
static uint8_t aes_xtime(uint8_t x)
{
    return (x << 1) ^ (((x >> 7) & 1) * 0x1b);
}

// Key expansion
static void aes_key_expansion(const uint8_t* key, uint8_t* round_keys)
{
    // First round key is the key itself
    memcpy(round_keys, key, 16);

    uint8_t* rk = round_keys + 16;
    for (int i = 1; i <= 10; i++) {
        // RotWord + SubWord + Rcon
        rk[0] = round_keys[(i-1)*16 + 0] ^ aes_sbox[round_keys[(i-1)*16 + 13]] ^ aes_rcon[i];
        rk[1] = round_keys[(i-1)*16 + 1] ^ aes_sbox[round_keys[(i-1)*16 + 14]];
        rk[2] = round_keys[(i-1)*16 + 2] ^ aes_sbox[round_keys[(i-1)*16 + 15]];
        rk[3] = round_keys[(i-1)*16 + 3] ^ aes_sbox[round_keys[(i-1)*16 + 12]];

        for (int j = 4; j < 16; j++) {
            rk[j] = round_keys[(i-1)*16 + j] ^ rk[j - 4];
        }
        rk += 16;
    }
}

// AES-128 encrypt single block
static void aes_encrypt_block(const uint8_t* key, const uint8_t* in, uint8_t* out)
{
    uint8_t round_keys[176];
    uint8_t state[16];

    aes_key_expansion(key, round_keys);

    // Initial round key addition
    for (int i = 0; i < 16; i++) {
        state[i] = in[i] ^ round_keys[i];
    }

    // Main rounds
    for (int round = 1; round <= 10; round++) {
        uint8_t temp[16];

        // SubBytes
        for (int i = 0; i < 16; i++) {
            temp[i] = aes_sbox[state[i]];
        }

        // ShiftRows
        state[0] = temp[0];  state[4] = temp[4];  state[8] = temp[8];   state[12] = temp[12];
        state[1] = temp[5];  state[5] = temp[9];  state[9] = temp[13];  state[13] = temp[1];
        state[2] = temp[10]; state[6] = temp[14]; state[10] = temp[2];  state[14] = temp[6];
        state[3] = temp[15]; state[7] = temp[3];  state[11] = temp[7];  state[15] = temp[11];

        // MixColumns (not in last round)
        if (round < 10) {
            for (int c = 0; c < 4; c++) {
                int i = c * 4;
                uint8_t a0 = state[i], a1 = state[i+1], a2 = state[i+2], a3 = state[i+3];
                temp[i]   = aes_xtime(a0) ^ aes_xtime(a1) ^ a1 ^ a2 ^ a3;
                temp[i+1] = a0 ^ aes_xtime(a1) ^ aes_xtime(a2) ^ a2 ^ a3;
                temp[i+2] = a0 ^ a1 ^ aes_xtime(a2) ^ aes_xtime(a3) ^ a3;
                temp[i+3] = aes_xtime(a0) ^ a0 ^ a1 ^ a2 ^ aes_xtime(a3);
            }
            memcpy(state, temp, 16);
        }

        // AddRoundKey
        for (int i = 0; i < 16; i++) {
            state[i] ^= round_keys[round * 16 + i];
        }
    }

    memcpy(out, state, 16);
}

// ============================================================================
// SMP CRYPTO FUNCTIONS (c1 and s1)
// ============================================================================

// c1 function for confirm value calculation
// c1(k, r, pres, preq, iat, rat, ia, ra) = e(k, e(k, r XOR p1) XOR p2)
// p1 = pres || preq || rat || iat
// p2 = padding || ia || ra
static void smp_c1(const uint8_t* k, const uint8_t* r,
                   const uint8_t* preq, const uint8_t* pres,
                   uint8_t iat, const uint8_t* ia,
                   uint8_t rat, const uint8_t* ra,
                   uint8_t* out)
{
    uint8_t p1[16], p2[16], tmp[16];

    // Build p1 = pres || preq || rat || iat
    // Note: pres and preq are 7 bytes each
    p1[0] = iat;
    p1[1] = rat;
    memcpy(&p1[2], preq, 7);
    memcpy(&p1[9], pres, 7);

    // Build p2 = padding || ia || ra
    // padding = 4 bytes of zeros
    memset(p2, 0, 4);
    memcpy(&p2[4], ia, 6);
    memcpy(&p2[10], ra, 6);

    // tmp = r XOR p1
    for (int i = 0; i < 16; i++) {
        tmp[i] = r[i] ^ p1[i];
    }

    // tmp = e(k, tmp)
    aes_encrypt_block(k, tmp, tmp);

    // tmp = tmp XOR p2
    for (int i = 0; i < 16; i++) {
        tmp[i] ^= p2[i];
    }

    // out = e(k, tmp)
    aes_encrypt_block(k, tmp, out);
}

// s1 function for STK generation
// s1(k, r1, r2) = e(k, r2[0..7] || r1[0..7])
static void smp_s1(const uint8_t* k, const uint8_t* r1, const uint8_t* r2, uint8_t* out)
{
    uint8_t r_prime[16];

    // r' = r2[0..7] || r1[0..7] (least significant octets)
    memcpy(&r_prime[0], r2, 8);
    memcpy(&r_prime[8], r1, 8);

    // out = e(k, r')
    aes_encrypt_block(k, r_prime, out);
}

// ============================================================================
// SMP INITIALIZATION
// ============================================================================

void smp_init(void)
{
    memset(smp_contexts, 0, sizeof(smp_contexts));
    printf("[SMP] Initialized\n");
}

// ============================================================================
// CONNECTION MANAGEMENT
// ============================================================================

void smp_on_connect(uint8_t conn_index, uint16_t handle)
{
    printf("[SMP] BLE connection %d (handle=0x%04X)\n", conn_index, handle);
    smp_alloc_context(conn_index, handle);
}

void smp_on_disconnect(uint8_t conn_index)
{
    printf("[SMP] BLE disconnection %d\n", conn_index);
    smp_context_t* ctx = smp_get_context(conn_index);
    if (ctx) {
        smp_free_context(ctx);
    }
}

// ============================================================================
// SMP SEND
// ============================================================================

bool smp_send(uint8_t conn_index, const uint8_t* data, uint16_t len)
{
    smp_context_t* ctx = smp_get_context(conn_index);
    if (!ctx) {
        printf("[SMP] ERROR: No context for conn %d\n", conn_index);
        return false;
    }

    // Send via L2CAP on fixed CID 0x0006 (SM)
    return l2cap_send_ble(ctx->handle, L2CAP_CID_SM, data, len);
}

// ============================================================================
// SMP PAIRING
// ============================================================================

bool smp_start_pairing(uint8_t conn_index)
{
    smp_context_t* ctx = smp_get_context(conn_index);
    if (!ctx) {
        printf("[SMP] ERROR: No context for conn %d\n", conn_index);
        return false;
    }

    if (ctx->state != SMP_STATE_IDLE) {
        printf("[SMP] Already pairing\n");
        return false;
    }

    printf("[SMP] Starting Just Works pairing...\n");

    // Build pairing request
    smp_pairing_t req = {
        .code = SMP_PAIRING_REQUEST,
        .io_capability = SMP_IO_NO_INPUT_NO_OUTPUT,  // Just Works
        .oob_data_flag = SMP_OOB_NOT_PRESENT,
        .auth_req = SMP_AUTH_BONDING,  // Request bonding
        .max_key_size = 16,
        .initiator_key_dist = SMP_KEY_ENC_KEY,  // We'll send LTK
        .responder_key_dist = SMP_KEY_ENC_KEY   // We want their LTK
    };

    // Save pairing request for confirm calculation
    memcpy(ctx->preq, &req.io_capability, 7);

    // TK is all zeros for Just Works
    memset(ctx->tk, 0, 16);

    ctx->state = SMP_STATE_PAIRING_REQ_SENT;

    return smp_send(conn_index, (uint8_t*)&req, sizeof(req));
}

// ============================================================================
// SMP RESPONSE HANDLERS
// ============================================================================

static void smp_handle_pairing_response(smp_context_t* ctx, const uint8_t* data, uint16_t len)
{
    if (len < sizeof(smp_pairing_t)) return;

    const smp_pairing_t* rsp = (const smp_pairing_t*)data;

    printf("[SMP] Pairing Response: io=%d oob=%d auth=0x%02X key_size=%d\n",
           rsp->io_capability, rsp->oob_data_flag, rsp->auth_req, rsp->max_key_size);

    // Save pairing response for confirm calculation
    memcpy(ctx->pres, &rsp->io_capability, 7);

    // Generate our random value
    smp_generate_random(ctx->mrand, 16);

    // Get addresses for confirm calculation
    const btd_connection_t* conn = btd_get_connection(ctx->conn_index);
    if (!conn) {
        printf("[SMP] ERROR: No connection data\n");
        ctx->state = SMP_STATE_FAILED;
        return;
    }

    // Get our BD_ADDR (initiator address)
    const uint8_t* ia = btd_get_local_bd_addr();

    // Calculate our confirm value
    // c1(TK, Mrand, preq, pres, iat, ia, rat, ra)
    // iat = 0 (public), rat = 0 (public) - Xbox uses public addresses
    smp_c1(ctx->tk, ctx->mrand,
           ctx->preq, ctx->pres,
           0, ia,              // Initiator: public addr
           0, conn->bd_addr,   // Responder: public addr
           ctx->mconfirm);

    printf("[SMP] Sending Confirm...\n");

    // Send our confirm value
    smp_pairing_confirm_t confirm = {
        .code = SMP_PAIRING_CONFIRM
    };
    memcpy(confirm.confirm, ctx->mconfirm, 16);

    ctx->state = SMP_STATE_CONFIRM_SENT;
    smp_send(ctx->conn_index, (uint8_t*)&confirm, sizeof(confirm));
}

static void smp_handle_pairing_confirm(smp_context_t* ctx, const uint8_t* data, uint16_t len)
{
    if (len < sizeof(smp_pairing_confirm_t)) return;

    const smp_pairing_confirm_t* confirm = (const smp_pairing_confirm_t*)data;

    printf("[SMP] Received Confirm\n");

    // Save their confirm value
    memcpy(ctx->sconfirm, confirm->confirm, 16);

    // Send our random value
    printf("[SMP] Sending Random...\n");

    smp_pairing_random_t random = {
        .code = SMP_PAIRING_RANDOM
    };
    memcpy(random.random, ctx->mrand, 16);

    ctx->state = SMP_STATE_RANDOM_SENT;
    smp_send(ctx->conn_index, (uint8_t*)&random, sizeof(random));
}

static void smp_handle_pairing_random(smp_context_t* ctx, const uint8_t* data, uint16_t len)
{
    if (len < sizeof(smp_pairing_random_t)) return;

    const smp_pairing_random_t* random = (const smp_pairing_random_t*)data;

    printf("[SMP] Received Random\n");

    // Save their random value
    memcpy(ctx->srand, random->random, 16);

    // Get addresses for verify
    const btd_connection_t* conn = btd_get_connection(ctx->conn_index);
    if (!conn) {
        printf("[SMP] ERROR: No connection data\n");
        ctx->state = SMP_STATE_FAILED;
        return;
    }

    const uint8_t* ia = btd_get_local_bd_addr();

    // Verify their confirm value
    uint8_t verify[16];
    smp_c1(ctx->tk, ctx->srand,
           ctx->preq, ctx->pres,
           0, ia,
           0, conn->bd_addr,
           verify);

    if (memcmp(verify, ctx->sconfirm, 16) != 0) {
        printf("[SMP] ERROR: Confirm value mismatch!\n");

        // Send pairing failed
        smp_pairing_failed_t failed = {
            .code = SMP_PAIRING_FAILED,
            .reason = SMP_ERROR_CONFIRM_VALUE_FAILED
        };
        smp_send(ctx->conn_index, (uint8_t*)&failed, sizeof(failed));
        ctx->state = SMP_STATE_FAILED;
        return;
    }

    printf("[SMP] Confirm verified!\n");

    // Calculate STK
    smp_s1(ctx->tk, ctx->srand, ctx->mrand, ctx->stk);

    printf("[SMP] STK calculated, starting encryption...\n");

    // Start encryption with STK
    // For LE Legacy pairing, EDIV and Rand are 0
    uint8_t zeros[8] = {0};
    btd_hci_le_start_encryption(conn->handle, zeros, 0, ctx->stk);

    ctx->state = SMP_STATE_KEY_EXCHANGE;
}

static void smp_handle_pairing_failed(smp_context_t* ctx, const uint8_t* data, uint16_t len)
{
    if (len < sizeof(smp_pairing_failed_t)) return;

    const smp_pairing_failed_t* failed = (const smp_pairing_failed_t*)data;

    printf("[SMP] Pairing Failed: reason=0x%02X\n", failed->reason);
    ctx->state = SMP_STATE_FAILED;
}

static void smp_handle_encryption_info(smp_context_t* ctx, const uint8_t* data, uint16_t len)
{
    if (len < sizeof(smp_encryption_info_t)) return;

    const smp_encryption_info_t* info = (const smp_encryption_info_t*)data;

    printf("[SMP] Received LTK\n");
    memcpy(ctx->ltk, info->ltk, 16);
}

static void smp_handle_master_ident(smp_context_t* ctx, const uint8_t* data, uint16_t len)
{
    if (len < sizeof(smp_master_ident_t)) return;

    const smp_master_ident_t* ident = (const smp_master_ident_t*)data;

    printf("[SMP] Received EDIV=0x%04X\n", ident->ediv);
    ctx->ediv = ident->ediv;
    memcpy(ctx->rand, ident->rand, 8);
    ctx->has_ltk = true;

    // TODO: Store LTK for future reconnection
}

// ============================================================================
// SMP DATA PROCESSING
// ============================================================================

void smp_process_data(uint8_t conn_index, const uint8_t* data, uint16_t len)
{
    if (len < 1) return;

    smp_context_t* ctx = smp_get_context(conn_index);
    if (!ctx) {
        printf("[SMP] ERROR: No context for conn %d\n", conn_index);
        return;
    }

    uint8_t opcode = data[0];

    switch (opcode) {
        case SMP_PAIRING_RESPONSE:
            smp_handle_pairing_response(ctx, data, len);
            break;

        case SMP_PAIRING_CONFIRM:
            smp_handle_pairing_confirm(ctx, data, len);
            break;

        case SMP_PAIRING_RANDOM:
            smp_handle_pairing_random(ctx, data, len);
            break;

        case SMP_PAIRING_FAILED:
            smp_handle_pairing_failed(ctx, data, len);
            break;

        case SMP_ENCRYPTION_INFO:
            smp_handle_encryption_info(ctx, data, len);
            break;

        case SMP_MASTER_IDENT:
            smp_handle_master_ident(ctx, data, len);
            break;

        default:
            printf("[SMP] Unknown opcode: 0x%02X\n", opcode);
            break;
    }
}

// ============================================================================
// ENCRYPTION STATE
// ============================================================================

bool smp_is_encrypted(uint8_t conn_index)
{
    smp_context_t* ctx = smp_get_context(conn_index);
    return ctx && ctx->state == SMP_STATE_ENCRYPTED;
}

// Called from btd.c when encryption change event is received
void smp_on_encryption_enabled(uint8_t conn_index)
{
    smp_context_t* ctx = smp_get_context(conn_index);
    if (ctx) {
        printf("[SMP] *** Encryption Enabled! ***\n");
        ctx->state = SMP_STATE_ENCRYPTED;

        // Notify ATT layer that encryption is ready
        smp_on_encrypted(conn_index);
    }
}

// ============================================================================
// WEAK CALLBACK
// ============================================================================

__attribute__((weak)) void smp_on_encrypted(uint8_t conn_index)
{
    printf("[SMP] Encryption ready on conn %d (weak handler)\n", conn_index);
}
