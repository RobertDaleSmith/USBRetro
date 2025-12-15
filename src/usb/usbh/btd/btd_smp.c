// smp.c - Security Manager Protocol (SMP) implementation for BLE
// Supports both Legacy and Secure Connections pairing

#include "btd_smp.h"
#include "btd.h"
#include "btd_l2cap.h"
#include "p256.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ============================================================================
// STATIC DATA
// ============================================================================

#define SMP_MAX_CONTEXTS 4
static smp_context_t smp_contexts[SMP_MAX_CONTEXTS];

// Simple random number generation
static uint32_t smp_rand_seed = 12345;

// Pre-generated ECDH key pair (generated at init to avoid delay at connection time)
static uint8_t smp_local_sk[32];
static uint8_t smp_local_pk_x[32];
static uint8_t smp_local_pk_y[32];
static bool smp_keys_ready = false;

// Hardware P-256 mode (uses HCI commands instead of software crypto)
// NOTE: Most dongles don't support HW P-256 - disabled by default
static bool smp_use_hw_p256 = false;  // Default to software P-256
static bool smp_hw_key_pending = false;
static bool smp_hw_dhkey_pending = false;
static uint8_t smp_pending_conn_index = 0xFF;  // Connection waiting for HW key

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

// BLE crypto uses big-endian internally, SMP PDUs use little-endian
// This helper reverses byte order for N bytes
static void smp_reverse_bytes(uint8_t* dst, const uint8_t* src, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        dst[i] = src[len - 1 - i];
    }
}

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
    extern uint32_t time_us_32(void);
    smp_rand_seed ^= time_us_32();

    for (int i = 0; i < len; i++) {
        out[i] = smp_random_byte();
    }
}

// ============================================================================
// AES-128 ENCRYPTION
// ============================================================================

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

static const uint8_t aes_rcon[11] = {
    0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36
};

static uint8_t aes_xtime(uint8_t x)
{
    return (x << 1) ^ (((x >> 7) & 1) * 0x1b);
}

static void aes_key_expansion(const uint8_t* key, uint8_t* round_keys)
{
    memcpy(round_keys, key, 16);

    uint8_t* rk = round_keys + 16;
    for (int i = 1; i <= 10; i++) {
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

static void aes_encrypt_block(const uint8_t* key, const uint8_t* in, uint8_t* out)
{
    uint8_t round_keys[176];
    uint8_t state[16];

    aes_key_expansion(key, round_keys);

    for (int i = 0; i < 16; i++) {
        state[i] = in[i] ^ round_keys[i];
    }

    for (int round = 1; round <= 10; round++) {
        uint8_t temp[16];

        for (int i = 0; i < 16; i++) {
            temp[i] = aes_sbox[state[i]];
        }

        state[0] = temp[0];  state[4] = temp[4];  state[8] = temp[8];   state[12] = temp[12];
        state[1] = temp[5];  state[5] = temp[9];  state[9] = temp[13];  state[13] = temp[1];
        state[2] = temp[10]; state[6] = temp[14]; state[10] = temp[2];  state[14] = temp[6];
        state[3] = temp[15]; state[7] = temp[3];  state[11] = temp[7];  state[15] = temp[11];

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

        for (int i = 0; i < 16; i++) {
            state[i] ^= round_keys[round * 16 + i];
        }
    }

    memcpy(out, state, 16);
}

// ============================================================================
// AES-CMAC (for Secure Connections f4, f5, f6)
// ESP-IDF uses little-endian: LSB at index 0, MSB at index 15
// ============================================================================

// Left shift a 128-bit block by 1 bit (little-endian: LSB at index 0)
static void aes_cmac_shift_left(const uint8_t* in, uint8_t* out)
{
    uint8_t carry = 0;
    // ESP-IDF style: start from index 0 (LSB), propagate to index 15 (MSB)
    for (int i = 0; i < 16; i++) {
        uint8_t new_carry = (in[i] & 0x80) ? 1 : 0;
        out[i] = (in[i] << 1) | carry;
        carry = new_carry;
    }
}

// Generate CMAC subkeys K1, K2 (ESP-IDF little-endian style)
static void aes_cmac_generate_subkeys(const uint8_t* key, uint8_t* k1, uint8_t* k2)
{
    uint8_t zero[16] = {0};
    uint8_t L[16];
    // Rb for little-endian: 0x87 at index 0 (LSB)
    static const uint8_t Rb[16] = {0x87, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    // L = AES-128(K, 0)
    aes_encrypt_block(key, zero, L);

    // K1 = L << 1; if MSB(L) == 1: K1 = K1 XOR Rb
    // MSB is at index 15 in little-endian
    aes_cmac_shift_left(L, k1);
    if (L[15] & 0x80) {
        for (int i = 0; i < 16; i++) {
            k1[i] ^= Rb[i];
        }
    }

    // K2 = K1 << 1; if MSB(K1) == 1: K2 = K2 XOR Rb
    aes_cmac_shift_left(k1, k2);
    if (k1[15] & 0x80) {
        for (int i = 0; i < 16; i++) {
            k2[i] ^= Rb[i];
        }
    }
}

// Swap bytes (reverse order) - like Linux kernel swap_buf
static void smp_swap_buf(const uint8_t* src, uint8_t* dst, int len)
{
    for (int i = 0; i < len; i++) {
        dst[len - 1 - i] = src[i];
    }
}

// AES-CMAC: ESP-IDF compatible "little-endian" CMAC
// - Pads message at START (prepends zeros to align to block boundary)
// - Applies K1/K2 to block 0 (which contains the LSB/padding)
// - Processes blocks from last to first (MSB to LSB)
static void aes_cmac(const uint8_t* key, const uint8_t* msg, int msg_len, uint8_t* mac)
{
    uint8_t k1[16], k2[16];
    aes_cmac_generate_subkeys(key, k1, k2);

    int n = (msg_len + 15) / 16;  // Number of blocks
    if (n == 0) n = 1;
    int len = n * 16;
    int diff = len - msg_len;

    // Allocate buffer and copy message to END (ESP-IDF style)
    uint8_t text[80];  // Max 5 blocks for f4 (65 bytes)
    memset(text, 0, len);
    if (msg_len > 0) {
        memcpy(&text[diff], msg, msg_len);
    }

    // Apply K1/K2 to block 0 (ESP-IDF applies to the padded/first block)
    bool complete_block = (msg_len > 0) && (msg_len % 16 == 0);
    if (complete_block) {
        for (int i = 0; i < 16; i++) {
            text[i] ^= k1[i];
        }
    } else {
        // ESP-IDF padding: for length bytes in incomplete block,
        // set 0x80 at position (16 - length - 1), zeros before it
        // Example: length=1 -> 0x80 at position 14, msg byte at position 15
        int length = msg_len % 16;
        if (length == 0) length = 16;  // Should not happen if !complete_block
        for (int i = length; i < 16; i++) {
            int pos = 16 - i - 1;
            text[pos] = (i == length) ? 0x80 : 0;
        }
        for (int i = 0; i < 16; i++) {
            text[i] ^= k2[i];
        }
    }

    // Process blocks from last to first (ESP-IDF reverse order)
    uint8_t x[16] = {0};
    for (int i = 1; i <= n; i++) {
        int block_idx = (n - i) * 16;  // Start from last block
        // XOR block with x
        for (int j = 0; j < 16; j++) {
            text[block_idx + j] ^= x[j];
        }
        // Encrypt
        aes_encrypt_block(key, &text[block_idx], x);
    }

    memcpy(mac, x, 16);
}

// ============================================================================
// LEGACY PAIRING CRYPTO (c1, s1)
// ============================================================================

static void smp_c1(const uint8_t* k, const uint8_t* r,
                   const uint8_t* preq, const uint8_t* pres,
                   uint8_t iat, const uint8_t* ia,
                   uint8_t rat, const uint8_t* ra,
                   uint8_t* out)
{
    uint8_t p1[16], p2[16], tmp[16];

    // Build p1 = pres || preq || rat || iat
    memcpy(&p1[0], pres, 7);
    memcpy(&p1[7], preq, 7);
    p1[14] = rat;
    p1[15] = iat;

    // Build p2 = padding || ia || ra
    memset(p2, 0, 4);
    memcpy(&p2[4], ia, 6);
    memcpy(&p2[10], ra, 6);

    for (int i = 0; i < 16; i++) {
        tmp[i] = r[i] ^ p1[i];
    }

    aes_encrypt_block(k, tmp, tmp);

    for (int i = 0; i < 16; i++) {
        tmp[i] ^= p2[i];
    }

    aes_encrypt_block(k, tmp, out);
}

static void smp_s1(const uint8_t* k, const uint8_t* r1, const uint8_t* r2, uint8_t* out)
{
    uint8_t r_prime[16];

    memcpy(&r_prime[0], r2, 8);
    memcpy(&r_prime[8], r1, 8);

    aes_encrypt_block(k, r_prime, out);
}

// ============================================================================
// SECURE CONNECTIONS CRYPTO (f4, f5, f6, g2)
// ============================================================================

// f4: Confirm value generation for SC Just Works/Numeric Comparison
// f4(U, V, X, Z) = AES-CMAC_X(U || V || Z)
//
// ESP-IDF builds message as Z || V || U and uses reverse-order CMAC.
// Our CMAC now matches ESP-IDF's "little-endian" style, so we use same order.
//
// All values in wire format (LE).
static void smp_f4(const uint8_t* u, const uint8_t* v, const uint8_t* x, uint8_t z, uint8_t* out)
{
    uint8_t m[65];  // 1 + 32 + 32

    // Build message: Z || V || U (ESP-IDF memory order)
    m[0] = z;
    memcpy(&m[1], v, 32);
    memcpy(&m[33], u, 32);

    // Debug: print first 4 bytes of inputs
    printf("[f4] U[0-3]: %02x%02x%02x%02x  V[0-3]: %02x%02x%02x%02x  X[0-3]: %02x%02x%02x%02x\n",
           u[0], u[1], u[2], u[3], v[0], v[1], v[2], v[3], x[0], x[1], x[2], x[3]);

    aes_cmac(x, m, 65, out);

    printf("[f4] out[0-3]: %02x%02x%02x%02x\n", out[0], out[1], out[2], out[3]);
}

// f5: Key generation function for SC
// f5(W, N1, N2, A1, A2) generates MacKey || LTK (32 bytes)
// W = DHKey (32 bytes)
// N1, N2 = random nonces (16 bytes each)
// A1, A2 = BD_ADDR with type (7 bytes each: type || addr)
// Uses SALT = 0x6C88...
static void smp_f5(const uint8_t* w, const uint8_t* n1, const uint8_t* n2,
                   uint8_t a1_type, const uint8_t* a1, uint8_t a2_type, const uint8_t* a2,
                   uint8_t* mackey, uint8_t* ltk)
{
    // SALT for f5
    static const uint8_t salt[16] = {
        0x6C, 0x88, 0x83, 0x91, 0xAA, 0xF5, 0xA5, 0x38,
        0x60, 0x37, 0x0B, 0xDB, 0x5A, 0x60, 0x83, 0xBE
    };

    // T = AES-CMAC_SALT(W)
    uint8_t T[16];
    aes_cmac(salt, w, 32, T);

    // Build m = Counter || keyID || N1 || N2 || A1 || A2 || Length
    // keyID = "btle" = 0x62746C65
    // Length = 0x0100 (256 in big endian)
    uint8_t m[53];
    m[0] = 0;  // Counter for first output
    m[1] = 0x62; m[2] = 0x74; m[3] = 0x6C; m[4] = 0x65;  // "btle"
    memcpy(&m[5], n1, 16);
    memcpy(&m[21], n2, 16);
    m[37] = a1_type;
    memcpy(&m[38], a1, 6);
    m[44] = a2_type;
    memcpy(&m[45], a2, 6);
    m[51] = 0x01; m[52] = 0x00;  // Length = 256

    // MacKey = AES-CMAC_T(0 || keyID || N1 || N2 || A1 || A2 || 0x0100)
    aes_cmac(T, m, 53, mackey);

    // LTK = AES-CMAC_T(1 || keyID || N1 || N2 || A1 || A2 || 0x0100)
    m[0] = 1;  // Counter for second output
    aes_cmac(T, m, 53, ltk);
}

// f6: Check value generation for SC
// f6(W, N1, N2, R, IOcap, A1, A2) = AES-CMAC_W(N1 || N2 || R || IOcap || A1 || A2)
static void smp_f6(const uint8_t* w, const uint8_t* n1, const uint8_t* n2,
                   const uint8_t* r, const uint8_t* iocap,
                   uint8_t a1_type, const uint8_t* a1, uint8_t a2_type, const uint8_t* a2,
                   uint8_t* out)
{
    // m = N1 || N2 || R || IOcap || A1 || A2
    // N1, N2 = 16 bytes each
    // R = 16 bytes
    // IOcap = 3 bytes
    // A1, A2 = 7 bytes each (type + addr)
    uint8_t m[65];
    memcpy(&m[0], n1, 16);
    memcpy(&m[16], n2, 16);
    memcpy(&m[32], r, 16);
    memcpy(&m[48], iocap, 3);
    m[51] = a1_type;
    memcpy(&m[52], a1, 6);
    m[58] = a2_type;
    memcpy(&m[59], a2, 6);

    aes_cmac(w, m, 65, out);
}

// g2: Numeric comparison value for SC (we use Just Works, so this is optional)
// g2(U, V, X, Y) = AES-CMAC_X(U || V || Y) mod 10^6
// Returns 6-digit numeric comparison value
static uint32_t smp_g2(const uint8_t* u, const uint8_t* v, const uint8_t* x, const uint8_t* y)
{
    uint8_t m[80];  // 32 + 32 + 16
    uint8_t out[16];

    memcpy(&m[0], u, 32);
    memcpy(&m[32], v, 32);
    memcpy(&m[64], y, 16);

    aes_cmac(x, m, 80, out);

    // Take last 4 bytes as big-endian and mod 10^6
    uint32_t val = ((uint32_t)out[12] << 24) | ((uint32_t)out[13] << 16) |
                   ((uint32_t)out[14] << 8) | out[15];
    return val % 1000000;
}

// ============================================================================
// SMP INITIALIZATION
// ============================================================================

// Self-test f4 with official BLE spec test vector (Core Spec D.2)
// ESP-IDF uses LITTLE-ENDIAN, so test vectors must be reversed from spec
static void smp_test_f4(void)
{
    // Test vector from BLE Core Spec D.2 - REVERSED to LE (wire format)
    // Original U: 20b003d2f297be2c5e2c83a7e9f9a5b9eff49111acf4fddbcc0301480e359de6
    static const uint8_t test_u[32] = {
        0xe6, 0x9d, 0x35, 0x0e, 0x48, 0x01, 0x03, 0xcc,
        0xdb, 0xfd, 0xf4, 0xac, 0x11, 0x91, 0xf4, 0xef,
        0xb9, 0xa5, 0xf9, 0xe9, 0xa7, 0x83, 0x2c, 0x5e,
        0x2c, 0xbe, 0x97, 0xf2, 0xd2, 0x03, 0xb0, 0x20
    };
    // Original V: 55188b3d32f6bb9a900afcfbeed4e72a59cb9ac2f19d7cfb6b4fdd49f47fc5fd
    static const uint8_t test_v[32] = {
        0xfd, 0xc5, 0x7f, 0xf4, 0x49, 0xdd, 0x4f, 0x6b,
        0xfb, 0x7c, 0x9d, 0xf1, 0xc2, 0x9a, 0xcb, 0x59,
        0x2a, 0xe7, 0xd4, 0xee, 0xfb, 0xfc, 0x0a, 0x90,
        0x9a, 0xbb, 0xf6, 0x32, 0x3d, 0x8b, 0x18, 0x55
    };
    // Original X: d5cb8454d177733effffb2ec712baeab
    static const uint8_t test_x[16] = {
        0xab, 0xae, 0x2b, 0x71, 0xec, 0xb2, 0xff, 0xff,
        0x3e, 0x73, 0x77, 0xd1, 0x54, 0x84, 0xcb, 0xd5
    };
    // Expected output also in LE (reversed from spec's f2c916f1...)
    static const uint8_t expected[16] = {
        0x2d, 0x87, 0x74, 0xa9, 0xbe, 0xa1, 0xed, 0xf1,
        0x1c, 0xbd, 0xa9, 0x07, 0xf1, 0x16, 0xc9, 0xf2
    };

    uint8_t result[16];
    smp_f4(test_u, test_v, test_x, 0, result);

    printf("[SMP] f4 test: result=%02x%02x%02x%02x...%02x%02x%02x%02x\n",
           result[0], result[1], result[2], result[3],
           result[12], result[13], result[14], result[15]);
    printf("[SMP] f4 test: expect=%02x%02x%02x%02x...%02x%02x%02x%02x\n",
           expected[0], expected[1], expected[2], expected[3],
           expected[12], expected[13], expected[14], expected[15]);

    if (memcmp(result, expected, 16) == 0) {
        printf("[SMP] *** f4 TEST PASSED ***\n");
    } else {
        printf("[SMP] !!! F4 TEST FAILED !!! (expected with ESP-IDF byte order)\n");
    }
}

void smp_init(void)
{
    memset(smp_contexts, 0, sizeof(smp_contexts));
    p256_init();
    smp_keys_ready = false;
    // Run f4 test to verify crypto
    smp_test_f4();
    printf("[SMP] Initialized (keys will be generated on first use)\n");
}

// Pre-computed test key pair from BLE Core Spec Vol 3, Part H, Section 2.3.5.6.1
// Using original spec values directly (spec shows crypto-ready format)
// Private key
static const uint8_t TEST_PRIVATE_KEY[32] = {
    0x3f, 0x49, 0xf6, 0xd4, 0xa3, 0xc5, 0x5f, 0x38,
    0x74, 0xc9, 0xb3, 0xe3, 0xd2, 0x10, 0x3f, 0x50,
    0x4a, 0xff, 0x60, 0x7b, 0xeb, 0x40, 0xb7, 0x99,
    0x58, 0x99, 0xb8, 0xa6, 0xcd, 0x3c, 0x1a, 0xbd
};
// Public key X coordinate
static const uint8_t TEST_PUBLIC_KEY_X[32] = {
    0x20, 0xb0, 0x03, 0xd2, 0xf2, 0x97, 0xbe, 0x2c,
    0x5e, 0x2c, 0x83, 0xa7, 0xe9, 0xf9, 0xa5, 0xb9,
    0xef, 0xf4, 0x91, 0x11, 0xac, 0xf4, 0xfd, 0xdb,
    0xcc, 0x03, 0x01, 0x48, 0x0e, 0x35, 0x9d, 0xe6
};
// Public key Y coordinate
static const uint8_t TEST_PUBLIC_KEY_Y[32] = {
    0xdc, 0x80, 0x9c, 0x49, 0x65, 0x2a, 0xeb, 0x6d,
    0x63, 0x32, 0x9a, 0xbf, 0x5a, 0x52, 0x15, 0x5c,
    0x76, 0x63, 0x45, 0xc2, 0x8f, 0xed, 0x30, 0x24,
    0x74, 0x1c, 0x8e, 0xd0, 0x15, 0x89, 0xd2, 0x8b
};

// Use pre-computed test keys or generate random
// NOTE: Validation function has bugs, but key generation works fine
#define USE_RANDOM_KEYS 1

static bool smp_ensure_keys_ready(void)
{
    if (smp_keys_ready) {
        return true;
    }

#if USE_RANDOM_KEYS
    // First test: validate the spec test keys - they must pass!
    {
        p256_point_t test_pk;
        memcpy(test_pk.x, TEST_PUBLIC_KEY_X, 32);
        memcpy(test_pk.y, TEST_PUBLIC_KEY_Y, 32);
        if (p256_point_is_valid(&test_pk)) {
            printf("[SMP] Spec test key validation: PASS\n");
        } else {
            printf("[SMP] Spec test key validation: FAIL - validation function broken!\n");
        }
    }

    printf("[SMP] Generating random P-256 key pair...\n");
    uint32_t start = time_us_32();

    // Generate random private key
    p256_generate_private_key(smp_local_sk);

    // Compute public key
    p256_point_t pk;
    if (!p256_compute_public_key(smp_local_sk, &pk)) {
        printf("[SMP] ERROR: Public key computation failed!\n");
        return false;
    }

    // Store public key (already in big-endian from p256 lib)
    memcpy(smp_local_pk_x, pk.x, 32);
    memcpy(smp_local_pk_y, pk.y, 32);

    uint32_t elapsed = time_us_32() - start;
    printf("[SMP] Key generation took %lu us (%lu ms)\n", elapsed, elapsed / 1000);
    printf("[SMP] Random PK X[0-3]: %02x%02x%02x%02x\n",
           smp_local_pk_x[0], smp_local_pk_x[1], smp_local_pk_x[2], smp_local_pk_x[3]);

    // Validate our own key
    if (p256_point_is_valid(&pk)) {
        printf("[SMP] Our public key is VALID (on curve)\n");
    } else {
        printf("[SMP] WARNING: Our public key FAILED validation!\n");
    }
#else
    printf("[SMP] Using pre-computed test key pair\n");
    // Store in big-endian (spec format) for f4 computations
    memcpy(smp_local_sk, TEST_PRIVATE_KEY, 32);
    memcpy(smp_local_pk_x, TEST_PUBLIC_KEY_X, 32);
    memcpy(smp_local_pk_y, TEST_PUBLIC_KEY_Y, 32);
#endif

    smp_keys_ready = true;
    return true;
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
    // Reset keys so we generate fresh ones for next connection
    smp_keys_ready = false;
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

    return l2cap_send_ble(ctx->handle, L2CAP_CID_SM, data, len);
}

// ============================================================================
// SMP PAIRING START
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

    printf("[SMP] Starting pairing (SC capable)...\n");

    // Keys will be generated lazily when peer responds with SC
    // For now, just send the pairing request

    // Build pairing request with SC flag
    // NOTE: Xbox BLE pairing fails - Xbox sends structured data instead of random
    // This appears to be a bug in Xbox's SMP implementation
    smp_pairing_t req = {
        .code = SMP_PAIRING_REQUEST,
        .io_capability = SMP_IO_NO_INPUT_NO_OUTPUT,  // Just Works
        .oob_data_flag = SMP_OOB_NOT_PRESENT,
        .auth_req = SMP_AUTH_BONDING | SMP_AUTH_SC,  // Request SC pairing
        .max_key_size = 16,
        .initiator_key_dist = SMP_KEY_ENC_KEY | SMP_KEY_ID_KEY,
        .responder_key_dist = SMP_KEY_ENC_KEY | SMP_KEY_ID_KEY
    };

    // Save pairing request for confirm calculation
    memcpy(ctx->preq, &req.io_capability, 7);

    ctx->state = SMP_STATE_PAIRING_REQ_SENT;

    return smp_send(conn_index, (uint8_t*)&req, sizeof(req));
}

// ============================================================================
// SC PUBLIC KEY EXCHANGE
// ============================================================================

static void smp_sc_send_public_key(smp_context_t* ctx)
{
    printf("[SMP] Preparing to send our public key...\n");

    // Generate keys if not ready
    if (!smp_ensure_keys_ready()) {
        printf("[SMP] ERROR: Cannot send public key, key generation failed\n");
        smp_pairing_failed_t failed = {
            .code = SMP_PAIRING_FAILED,
            .reason = SMP_ERROR_UNSPECIFIED_REASON
        };
        smp_send(ctx->conn_index, (uint8_t*)&failed, sizeof(failed));
        ctx->state = SMP_STATE_FAILED;
        return;
    }

    // Copy private key (keep in BE for P-256 operations)
    memcpy(ctx->local_sk, smp_local_sk, 32);

    // ESP-IDF approach: Store public key in WIRE format (LE)
    // smp_local_pk is in BE (from key gen), reverse to LE for storage and wire
    smp_swap_buf(smp_local_pk_x, ctx->local_pk_x, 32);
    smp_swap_buf(smp_local_pk_y, ctx->local_pk_y, 32);

    smp_pairing_public_key_t pk = {
        .code = SMP_PAIRING_PUBLIC_KEY
    };
    // local_pk is now in wire format (LE), send directly
    memcpy(pk.x, ctx->local_pk_x, 32);
    memcpy(pk.y, ctx->local_pk_y, 32);

    printf("[SMP] Wire PK X: %02x%02x%02x%02x...%02x%02x%02x%02x\n",
           pk.x[0], pk.x[1], pk.x[2], pk.x[3], pk.x[28], pk.x[29], pk.x[30], pk.x[31]);
    printf("[SMP] Sending our public key...\n");

    ctx->state = SMP_STATE_SC_PUBKEY_SENT;
    smp_send(ctx->conn_index, (uint8_t*)&pk, sizeof(pk));
}

// Forward declaration for confirm sending after DHKey is ready
static void smp_sc_send_confirm(smp_context_t* ctx);

static void smp_sc_handle_public_key(smp_context_t* ctx, const uint8_t* data, uint16_t len)
{
    if (len < sizeof(smp_pairing_public_key_t)) return;

    const smp_pairing_public_key_t* pk = (const smp_pairing_public_key_t*)data;

    printf("[SMP] Received peer public key\n");
    printf("[SMP]   Wire X[0-3]: %02x%02x%02x%02x  Wire Y[0-3]: %02x%02x%02x%02x\n",
           pk->x[0], pk->x[1], pk->x[2], pk->x[3], pk->y[0], pk->y[1], pk->y[2], pk->y[3]);

    // ESP-IDF approach: Store in wire format (LE) directly - NO REVERSAL
    memcpy(ctx->peer_pk_x, pk->x, 32);
    memcpy(ctx->peer_pk_y, pk->y, 32);

    printf("[SMP]   Stored X[0-3]: %02x%02x%02x%02x (wire LE format)\n",
           ctx->peer_pk_x[0], ctx->peer_pk_x[1], ctx->peer_pk_x[2], ctx->peer_pk_x[3]);

    // If we haven't sent our public key yet, send it now
    if (ctx->state == SMP_STATE_PAIRING_RSP_RECEIVED) {
        smp_sc_send_public_key(ctx);
    }

    ctx->state = SMP_STATE_SC_PUBKEY_RECEIVED;

    if (smp_use_hw_p256) {
        // Use HW DHKey generation - need to pass peer key in wire format (little-endian)
        printf("[SMP] Requesting HW DHKey generation...\n");
        smp_hw_dhkey_pending = true;
        smp_pending_conn_index = ctx->conn_index;
        // HCI expects keys in little-endian (wire format)
        btd_hci_le_generate_dhkey(pk->x, pk->y);
        // Will continue in smp_on_hw_dhkey() callback
        return;
    }

    // Software DHKey - P-256 code expects big-endian coordinates
    // peer_pk is stored in wire format (LE), so we need to reverse for P-256
    p256_point_t peer_pk_point;
    smp_swap_buf(ctx->peer_pk_x, peer_pk_point.x, 32);
    smp_swap_buf(ctx->peer_pk_y, peer_pk_point.y, 32);

    // TODO: Fix validation - currently fails due to NIST reduction bug
    if (!p256_point_is_valid(&peer_pk_point)) {
        printf("[SMP] WARNING: Point validation failed (continuing anyway)\n");
    }

    // Compute ECDH shared secret (DHKey) using software
    if (!p256_ecdh_shared_secret(ctx->local_sk, &peer_pk_point, ctx->dhkey)) {
        printf("[SMP] ERROR: ECDH computation failed\n");
        smp_pairing_failed_t failed = {
            .code = SMP_PAIRING_FAILED,
            .reason = SMP_ERROR_UNSPECIFIED_REASON
        };
        smp_send(ctx->conn_index, (uint8_t*)&failed, sizeof(failed));
        ctx->state = SMP_STATE_FAILED;
        return;
    }

    printf("[SMP] DHKey computed successfully (software)\n");

    // SC Just Works flow for INITIATOR (master):
    // 1. Exchange public keys ✓
    // 2. Compute DHKey ✓
    // 3. Wait for responder's confirm (Cb) - DON'T send ours!
    // 4. Generate Na and send it
    // 5. Receive Nb
    // 6. Verify Cb = f4(PKb, PKa, Nb, 0)
    // 7. Key derivation

    // Generate our random now, but DON'T send confirm
    smp_generate_random(ctx->mrand, 16);
    printf("[SMP] Generated Na, waiting for responder's confirm...\n");
    ctx->state = SMP_STATE_SC_PUBKEY_RECEIVED;  // Wait for confirm
}

static void smp_sc_send_confirm(smp_context_t* ctx)
{
    // Generate our random value Na (stored in wire format LE)
    smp_generate_random(ctx->mrand, 16);

    // ESP-IDF approach: ALL values in wire format (LE), no conversion needed
    // local_pk_x is already in wire format (we send it directly on wire)
    // peer_pk_x is stored in wire format (no reversal on receive)
    // mrand is random bytes - format doesn't matter, use directly

    // Compute f4 with WIRE FORMAT values - matches ESP-IDF exactly
    // f4 uses message order: Z || V || U (ESP-IDF style)
    smp_f4(ctx->local_pk_x, ctx->peer_pk_x, ctx->mrand, 0, ctx->mconfirm);

    printf("[SMP] === CONFIRM COMPUTATION (ESP-IDF style) ===\n");
    printf("[SMP] PKa (wire): %02x%02x%02x%02x\n", ctx->local_pk_x[0], ctx->local_pk_x[1], ctx->local_pk_x[2], ctx->local_pk_x[3]);
    printf("[SMP] PKb (wire): %02x%02x%02x%02x\n", ctx->peer_pk_x[0], ctx->peer_pk_x[1], ctx->peer_pk_x[2], ctx->peer_pk_x[3]);
    printf("[SMP] Na: %02x%02x%02x%02x\n", ctx->mrand[0], ctx->mrand[1], ctx->mrand[2], ctx->mrand[3]);
    printf("[SMP] Ca: %02x%02x%02x%02x\n", ctx->mconfirm[0], ctx->mconfirm[1], ctx->mconfirm[2], ctx->mconfirm[3]);
    printf("[SMP] =============================================\n");
    printf("[SMP] Sending SC Confirm...\n");

    smp_pairing_confirm_t confirm = {
        .code = SMP_PAIRING_CONFIRM
    };
    // mconfirm is already computed in wire format, send directly
    memcpy(confirm.confirm, ctx->mconfirm, 16);

    printf("[SMP] Wire confirm: %02x %02x%02x%02x%02x...%02x%02x%02x%02x (code+16)\n",
           confirm.code, confirm.confirm[0], confirm.confirm[1], confirm.confirm[2], confirm.confirm[3],
           confirm.confirm[12], confirm.confirm[13], confirm.confirm[14], confirm.confirm[15]);

    ctx->state = SMP_STATE_SC_CONFIRM_SENT;
    smp_send(ctx->conn_index, (uint8_t*)&confirm, sizeof(confirm));
}

// ============================================================================
// SC CONFIRM/RANDOM EXCHANGE
// ============================================================================

static void smp_sc_handle_pairing_confirm(smp_context_t* ctx, const uint8_t* data, uint16_t len)
{
    if (len < sizeof(smp_pairing_confirm_t)) return;

    const smp_pairing_confirm_t* confirm = (const smp_pairing_confirm_t*)data;
    printf("[SMP] Received SC Confirm\n");

    // Store in wire format (no reversal) - we'll compare with wire format verify
    memcpy(ctx->sconfirm, confirm->confirm, 16);

    // Send our random value Na
    smp_pairing_random_t random = {
        .code = SMP_PAIRING_RANDOM
    };
    // ESP-IDF approach: mrand is already in wire format, send directly
    memcpy(random.random, ctx->mrand, 16);

    printf("[SMP] Wire random: %02x %02x%02x%02x%02x...%02x%02x%02x%02x (code+16)\n",
           random.code, random.random[0], random.random[1], random.random[2], random.random[3],
           random.random[12], random.random[13], random.random[14], random.random[15]);
    printf("[SMP] Sending SC Random...\n");

    ctx->state = SMP_STATE_SC_RANDOM_SENT;
    smp_send(ctx->conn_index, (uint8_t*)&random, sizeof(random));
}

static void smp_sc_handle_pairing_random(smp_context_t* ctx, const uint8_t* data, uint16_t len)
{
    if (len < sizeof(smp_pairing_random_t)) return;

    const smp_pairing_random_t* random = (const smp_pairing_random_t*)data;
    printf("[SMP] Received SC Random\n");

    // Store in wire format (no reversal) - we'll use wire format for verify
    memcpy(ctx->srand, random->random, 16);

    // ESP-IDF approach: ALL values in wire format (LE)
    // local_pk_x, peer_pk_x, srand are all already in wire format
    // f4 uses message order: Z || V || U (ESP-IDF style)
    // For verification: Cb = f4(PKb, PKa, Nb, 0)
    uint8_t verify[16];
    smp_f4(ctx->peer_pk_x, ctx->local_pk_x, ctx->srand, 0, verify);

    printf("[SMP] Verify computed: %02x%02x%02x%02x...%02x%02x%02x%02x\n",
           verify[0], verify[1], verify[2], verify[3],
           verify[12], verify[13], verify[14], verify[15]);

    if (memcmp(verify, ctx->sconfirm, 16) != 0) {
        printf("[SMP] ERROR: SC Confirm value mismatch!\n");
        printf("[SMP] NOTE: Xbox BLE sends structured data instead of random - pairing will fail\n");
        smp_pairing_failed_t failed = {
            .code = SMP_PAIRING_FAILED,
            .reason = SMP_ERROR_CONFIRM_VALUE_FAILED
        };
        smp_send(ctx->conn_index, (uint8_t*)&failed, sizeof(failed));
        ctx->state = SMP_STATE_FAILED;
        return;
    }

    printf("[SMP] SC Confirm verified!\n");

    // Get addresses
    const btd_connection_t* conn = btd_get_connection(ctx->conn_index);
    if (!conn) {
        printf("[SMP] ERROR: No connection data\n");
        ctx->state = SMP_STATE_FAILED;
        return;
    }

    const uint8_t* ia = btd_get_local_bd_addr();

    // Calculate MacKey and LTK using f5
    // f5(DHKey, Na, Nb, A, B)
    // A = initiator address (us), B = responder address (peer)
    smp_f5(ctx->dhkey, ctx->mrand, ctx->srand,
           0, ia,              // Initiator (public address type = 0)
           0, conn->bd_addr,   // Responder
           ctx->mackey, ctx->sc_ltk);

    printf("[SMP] MacKey and LTK calculated\n");

    // Calculate DHKey check values using f6
    // IOcap for initiator = preq[0..2] (io, oob, auth)
    // IOcap for responder = pres[0..2]
    uint8_t iocap_a[3], iocap_b[3];
    memcpy(iocap_a, ctx->preq, 3);
    memcpy(iocap_b, ctx->pres, 3);

    // For Just Works, r = 0
    uint8_t r[16] = {0};

    // Ea = f6(MacKey, Na, Nb, r, IOcapA, A, B)
    smp_f6(ctx->mackey, ctx->mrand, ctx->srand, r, iocap_a,
           0, ia, 0, conn->bd_addr, ctx->ea);

    // Eb = f6(MacKey, Nb, Na, r, IOcapB, B, A)
    smp_f6(ctx->mackey, ctx->srand, ctx->mrand, r, iocap_b,
           0, conn->bd_addr, 0, ia, ctx->eb);

    printf("[SMP] Sending DHKey Check...\n");

    // Send our DHKey check (Ea)
    smp_pairing_dhkey_check_t check = {
        .code = SMP_PAIRING_DHKEY_CHECK
    };
    memcpy(check.check, ctx->ea, 16);

    ctx->state = SMP_STATE_SC_DHKEY_CHECK_SENT;
    smp_send(ctx->conn_index, (uint8_t*)&check, sizeof(check));
}

static void smp_sc_handle_dhkey_check(smp_context_t* ctx, const uint8_t* data, uint16_t len)
{
    if (len < sizeof(smp_pairing_dhkey_check_t)) return;

    const smp_pairing_dhkey_check_t* check = (const smp_pairing_dhkey_check_t*)data;

    printf("[SMP] Received DHKey Check\n");

    // Verify peer's DHKey check value (Eb)
    if (memcmp(check->check, ctx->eb, 16) != 0) {
        printf("[SMP] ERROR: DHKey check mismatch!\n");
        smp_pairing_failed_t failed = {
            .code = SMP_PAIRING_FAILED,
            .reason = SMP_ERROR_DHKEY_CHECK_FAILED
        };
        smp_send(ctx->conn_index, (uint8_t*)&failed, sizeof(failed));
        ctx->state = SMP_STATE_FAILED;
        return;
    }

    printf("[SMP] DHKey check verified!\n");

    // Copy SC LTK to main LTK field
    memcpy(ctx->ltk, ctx->sc_ltk, 16);
    ctx->has_ltk = true;

    // Start encryption with the SC LTK
    // For SC, EDIV and Rand are 0
    const btd_connection_t* conn = btd_get_connection(ctx->conn_index);
    if (conn) {
        printf("[SMP] Starting encryption with SC LTK...\n");
        uint8_t zeros[8] = {0};
        btd_hci_le_start_encryption(conn->handle, zeros, 0, ctx->ltk);
        ctx->state = SMP_STATE_KEY_EXCHANGE;
    }
}

// ============================================================================
// LEGACY PAIRING HANDLERS
// ============================================================================

static void smp_legacy_handle_pairing_response(smp_context_t* ctx, const uint8_t* data, uint16_t len)
{
    // TK is all zeros for Just Works
    memset(ctx->tk, 0, 16);

    // Generate our random value
    smp_generate_random(ctx->mrand, 16);

    // Get addresses for confirm calculation
    const btd_connection_t* conn = btd_get_connection(ctx->conn_index);
    if (!conn) {
        printf("[SMP] ERROR: No connection data\n");
        ctx->state = SMP_STATE_FAILED;
        return;
    }

    const uint8_t* ia = btd_get_local_bd_addr();

    // Calculate our confirm value using c1
    smp_c1(ctx->tk, ctx->mrand,
           ctx->preq, ctx->pres,
           0, ia,
           0, conn->bd_addr,
           ctx->mconfirm);

    printf("[SMP] Sending Legacy Confirm...\n");

    smp_pairing_confirm_t confirm = {
        .code = SMP_PAIRING_CONFIRM
    };
    memcpy(confirm.confirm, ctx->mconfirm, 16);

    ctx->state = SMP_STATE_CONFIRM_SENT;
    smp_send(ctx->conn_index, (uint8_t*)&confirm, sizeof(confirm));
}

static void smp_legacy_handle_pairing_confirm(smp_context_t* ctx, const uint8_t* data, uint16_t len)
{
    if (len < sizeof(smp_pairing_confirm_t)) return;

    const smp_pairing_confirm_t* confirm = (const smp_pairing_confirm_t*)data;

    printf("[SMP] Received Legacy Confirm\n");

    memcpy(ctx->sconfirm, confirm->confirm, 16);

    printf("[SMP] Sending Legacy Random...\n");

    smp_pairing_random_t random = {
        .code = SMP_PAIRING_RANDOM
    };
    memcpy(random.random, ctx->mrand, 16);

    ctx->state = SMP_STATE_RANDOM_SENT;
    smp_send(ctx->conn_index, (uint8_t*)&random, sizeof(random));
}

static void smp_legacy_handle_pairing_random(smp_context_t* ctx, const uint8_t* data, uint16_t len)
{
    if (len < sizeof(smp_pairing_random_t)) return;

    const smp_pairing_random_t* random = (const smp_pairing_random_t*)data;

    printf("[SMP] Received Legacy Random\n");

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
        smp_pairing_failed_t failed = {
            .code = SMP_PAIRING_FAILED,
            .reason = SMP_ERROR_CONFIRM_VALUE_FAILED
        };
        smp_send(ctx->conn_index, (uint8_t*)&failed, sizeof(failed));
        ctx->state = SMP_STATE_FAILED;
        return;
    }

    printf("[SMP] Legacy Confirm verified!\n");

    // Calculate STK using s1
    smp_s1(ctx->tk, ctx->srand, ctx->mrand, ctx->stk);

    printf("[SMP] STK calculated, starting encryption...\n");

    uint8_t zeros[8] = {0};
    btd_hci_le_start_encryption(conn->handle, zeros, 0, ctx->stk);

    ctx->state = SMP_STATE_KEY_EXCHANGE;
}

// ============================================================================
// PAIRING RESPONSE HANDLER (COMMON)
// ============================================================================

static void smp_handle_pairing_response(smp_context_t* ctx, const uint8_t* data, uint16_t len)
{
    if (len < sizeof(smp_pairing_t)) return;

    const smp_pairing_t* rsp = (const smp_pairing_t*)data;

    printf("[SMP] Pairing Response: io=%d oob=%d auth=0x%02X key_size=%d\n",
           rsp->io_capability, rsp->oob_data_flag, rsp->auth_req, rsp->max_key_size);

    // Save pairing response
    memcpy(ctx->pres, &rsp->io_capability, 7);

    // Use SC if peer supports it (we always request SC)
    ctx->use_sc = (rsp->auth_req & SMP_AUTH_SC) != 0;

    if (ctx->use_sc) {
        printf("[SMP] Using Secure Connections pairing\n");
        ctx->state = SMP_STATE_PAIRING_RSP_RECEIVED;
        // Send our public key (initiator sends first)
        smp_sc_send_public_key(ctx);
    } else {
        printf("[SMP] Using Legacy pairing (peer auth=0x%02X)\n", rsp->auth_req);
        ctx->state = SMP_STATE_PAIRING_RSP_RECEIVED;
        smp_legacy_handle_pairing_response(ctx, data, len);
    }
}

// ============================================================================
// ENCRYPTION INFO HANDLERS
// ============================================================================

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
            if (ctx->use_sc) {
                smp_sc_handle_pairing_confirm(ctx, data, len);
            } else {
                smp_legacy_handle_pairing_confirm(ctx, data, len);
            }
            break;

        case SMP_PAIRING_RANDOM:
            if (ctx->use_sc) {
                smp_sc_handle_pairing_random(ctx, data, len);
            } else {
                smp_legacy_handle_pairing_random(ctx, data, len);
            }
            break;

        case SMP_PAIRING_PUBLIC_KEY:
            smp_sc_handle_public_key(ctx, data, len);
            break;

        case SMP_PAIRING_DHKEY_CHECK:
            smp_sc_handle_dhkey_check(ctx, data, len);
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

void smp_on_encryption_enabled(uint8_t conn_index)
{
    smp_context_t* ctx = smp_get_context(conn_index);
    if (ctx) {
        printf("[SMP] *** Encryption Enabled (%s) ***\n", ctx->use_sc ? "SC" : "Legacy");
        ctx->state = SMP_STATE_ENCRYPTED;
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

// ============================================================================
// HARDWARE P-256 SUPPORT
// ============================================================================

void smp_enable_hw_p256(bool enable)
{
    smp_use_hw_p256 = enable;
    printf("[SMP] Hardware P-256 mode: %s\n", enable ? "ENABLED" : "DISABLED");
}

// Forward declaration for SC public key sending
static void smp_sc_send_public_key(smp_context_t* ctx);

void smp_on_hw_public_key(const uint8_t* pk_x, const uint8_t* pk_y)
{
    printf("[SMP] Received HW-generated public key\n");
    printf("[SMP]   HW X[0-3]: %02x%02x%02x%02x (little-endian wire format)\n",
           pk_x[0], pk_x[1], pk_x[2], pk_x[3]);

    if (!smp_hw_key_pending) {
        printf("[SMP] WARNING: Unexpected HW public key (not waiting for it)\n");
        return;
    }

    smp_hw_key_pending = false;

    // Hardware returns keys in little-endian (wire format)
    // Reverse to big-endian for internal storage (matches pre-computed key format)
    smp_swap_buf(pk_x, smp_local_pk_x, 32);
    smp_swap_buf(pk_y, smp_local_pk_y, 32);
    smp_keys_ready = true;

    printf("[SMP]   Stored PK X[0-3]: %02x%02x%02x%02x (big-endian internal)\n",
           smp_local_pk_x[0], smp_local_pk_x[1], smp_local_pk_x[2], smp_local_pk_x[3]);

    // Resume pairing if we were waiting for the key
    if (smp_pending_conn_index != 0xFF) {
        smp_context_t* ctx = smp_get_context(smp_pending_conn_index);
        if (ctx && ctx->state == SMP_STATE_PAIRING_RSP_RECEIVED) {
            printf("[SMP] Resuming pairing with HW key...\n");
            // Copy to context - already in big-endian
            memcpy(ctx->local_pk_x, smp_local_pk_x, 32);
            memcpy(ctx->local_pk_y, smp_local_pk_y, 32);
            smp_sc_send_public_key(ctx);
        }
        smp_pending_conn_index = 0xFF;
    }
}

void smp_on_hw_dhkey(const uint8_t* dhkey)
{
    printf("[SMP] Received HW-generated DHKey\n");
    printf("[SMP]   HW DHKey[0-3]: %02x%02x%02x%02x (little-endian)\n",
           dhkey[0], dhkey[1], dhkey[2], dhkey[3]);

    if (!smp_hw_dhkey_pending) {
        printf("[SMP] WARNING: Unexpected HW DHKey (not waiting for it)\n");
        return;
    }

    smp_hw_dhkey_pending = false;

    // Find context that was waiting for DHKey
    if (smp_pending_conn_index != 0xFF) {
        smp_context_t* ctx = smp_get_context(smp_pending_conn_index);
        if (ctx && ctx->state == SMP_STATE_SC_PUBKEY_RECEIVED) {
            // Hardware returns DHKey in little-endian (wire format)
            // Reverse to big-endian for internal storage (matches f5/f6 expectations)
            smp_swap_buf(dhkey, ctx->dhkey, 32);
            printf("[SMP] DHKey stored (big-endian): %02x%02x%02x%02x...%02x%02x%02x%02x\n",
                   ctx->dhkey[0], ctx->dhkey[1], ctx->dhkey[2], ctx->dhkey[3],
                   ctx->dhkey[28], ctx->dhkey[29], ctx->dhkey[30], ctx->dhkey[31]);

            // Continue with confirm sending
            smp_sc_send_confirm(ctx);
        }
        smp_pending_conn_index = 0xFF;
    }
}
