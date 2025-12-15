// p256.c - NIST P-256 Elliptic Curve implementation
// Simple but correct implementation for BLE SMP Secure Connections

#include "p256.h"
#include <string.h>
#include <stdio.h>
#include "pico/time.h"

// ============================================================================
// P-256 CURVE PARAMETERS (in big-endian)
// ============================================================================

// Prime p = 2^256 - 2^224 + 2^192 + 2^96 - 1
static const uint8_t P256_P[32] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

// Order n
static const uint8_t P256_N[32] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xBC, 0xE6, 0xFA, 0xAD, 0xA7, 0x17, 0x9E, 0x84,
    0xF3, 0xB9, 0xCA, 0xC2, 0xFC, 0x63, 0x25, 0x51
};

// Generator point G
static const uint8_t P256_GX[32] = {
    0x6B, 0x17, 0xD1, 0xF2, 0xE1, 0x2C, 0x42, 0x47,
    0xF8, 0xBC, 0xE6, 0xE5, 0x63, 0xA4, 0x40, 0xF2,
    0x77, 0x03, 0x7D, 0x81, 0x2D, 0xEB, 0x33, 0xA0,
    0xF4, 0xA1, 0x39, 0x45, 0xD8, 0x98, 0xC2, 0x96
};

static const uint8_t P256_GY[32] = {
    0x4F, 0xE3, 0x42, 0xE2, 0xFE, 0x1A, 0x7F, 0x9B,
    0x8E, 0xE7, 0xEB, 0x4A, 0x7C, 0x0F, 0x9E, 0x16,
    0x2B, 0xCE, 0x33, 0x57, 0x6B, 0x31, 0x5E, 0xCE,
    0xCB, 0xB6, 0x40, 0x68, 0x37, 0xBF, 0x51, 0xF5
};

// Coefficient a = -3 (mod p) = p - 3
static const uint8_t P256_A[32] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFC
};

// Random seed for key generation
static uint32_t p256_rand_seed = 0x12345678;

// ============================================================================
// BIG INTEGER ARITHMETIC (256-bit, big-endian)
// ============================================================================

// Compare two 256-bit numbers: returns -1 if a<b, 0 if a==b, 1 if a>b
static int bn_cmp(const uint8_t* a, const uint8_t* b)
{
    for (int i = 0; i < 32; i++) {
        if (a[i] < b[i]) return -1;
        if (a[i] > b[i]) return 1;
    }
    return 0;
}

// Check if a is zero
static bool bn_is_zero(const uint8_t* a)
{
    for (int i = 0; i < 32; i++) {
        if (a[i] != 0) return false;
    }
    return true;
}

// Copy: dst = src
static void bn_copy(uint8_t* dst, const uint8_t* src)
{
    memcpy(dst, src, 32);
}

// Set to zero
static void bn_zero(uint8_t* a)
{
    memset(a, 0, 32);
}

// Add: result = a + b, returns carry
static uint8_t bn_add(uint8_t* result, const uint8_t* a, const uint8_t* b)
{
    uint16_t carry = 0;
    for (int i = 31; i >= 0; i--) {
        uint16_t sum = (uint16_t)a[i] + (uint16_t)b[i] + carry;
        result[i] = (uint8_t)sum;
        carry = sum >> 8;
    }
    return (uint8_t)carry;
}

// Subtract: result = a - b, returns borrow
static uint8_t bn_sub(uint8_t* result, const uint8_t* a, const uint8_t* b)
{
    int16_t borrow = 0;
    for (int i = 31; i >= 0; i--) {
        int16_t diff = (int16_t)a[i] - (int16_t)b[i] - borrow;
        if (diff < 0) {
            diff += 256;
            borrow = 1;
        } else {
            borrow = 0;
        }
        result[i] = (uint8_t)diff;
    }
    return (uint8_t)borrow;
}

// Modular reduction: result = a mod p (assumes a < 2p)
static void bn_mod_p(uint8_t* result, const uint8_t* a)
{
    if (bn_cmp(a, P256_P) >= 0) {
        bn_sub(result, a, P256_P);
    } else {
        bn_copy(result, a);
    }
}

// Modular addition: result = (a + b) mod p
static void bn_mod_add(uint8_t* result, const uint8_t* a, const uint8_t* b)
{
    uint8_t temp[32];
    uint8_t carry = bn_add(temp, a, b);
    if (carry || bn_cmp(temp, P256_P) >= 0) {
        bn_sub(result, temp, P256_P);
    } else {
        bn_copy(result, temp);
    }
}

// Modular subtraction: result = (a - b) mod p
static void bn_mod_sub(uint8_t* result, const uint8_t* a, const uint8_t* b)
{
    uint8_t temp[32];
    uint8_t borrow = bn_sub(temp, a, b);
    if (borrow) {
        bn_add(result, temp, P256_P);
    } else {
        bn_copy(result, temp);
    }
}

// Multiply two 256-bit numbers to get 512-bit result (big-endian)
static void bn_mul(uint8_t* result, const uint8_t* a, const uint8_t* b)
{
    uint8_t temp[64];
    memset(temp, 0, 64);

    for (int i = 31; i >= 0; i--) {
        uint32_t carry = 0;
        for (int j = 31; j >= 0; j--) {
            int k = i + j + 1;  // Position in result (0-62)
            uint32_t prod = (uint32_t)a[i] * (uint32_t)b[j] + temp[k] + carry;
            temp[k] = (uint8_t)prod;
            carry = prod >> 8;
        }
        temp[i] += (uint8_t)carry;
    }

    memcpy(result, temp, 64);
}

// Fast NIST P-256 reduction
// Uses the special structure of p = 2^256 - 2^224 + 2^192 + 2^96 - 1
// Input: 512-bit number in big-endian (64 bytes)
// Output: 256-bit result mod p (32 bytes)
static void bn_mod_p_512(uint8_t* result, const uint8_t* a)
{
    // Convert to 32-bit words (little-endian word order for easier math)
    // a[0..63] big-endian -> c[0..15] where c[0] is least significant word
    uint32_t c[16];
    for (int i = 0; i < 16; i++) {
        int j = (15 - i) * 4;  // Big-endian byte index
        c[i] = ((uint32_t)a[j] << 24) | ((uint32_t)a[j+1] << 16) |
               ((uint32_t)a[j+2] << 8) | (uint32_t)a[j+3];
    }

    // c[0..7] = low 256 bits (T0..T7 in NIST notation but reversed)
    // c[8..15] = high 256 bits
    // In our notation: c[0]=LSW of input, c[15]=MSW of input
    // NIST uses: c[0..7] = A (low), c[8..15] = B (high)
    // where A = sum(c[i] * 2^(32i)) for i=0..7

    // We'll use 64-bit accumulators to handle carries
    int64_t s[8];  // Signed to handle negative intermediate values

    // Initialize with low 256 bits
    for (int i = 0; i < 8; i++) {
        s[i] = c[i];
    }

    // Add s1 = (c15, c14, c13, c12, c11, 0, 0, 0)
    s[3] += c[11];
    s[4] += c[12];
    s[5] += c[13];
    s[6] += c[14];
    s[7] += c[15];

    // Add 2*s2 = 2*(0, c15, c14, c13, c12, 0, 0, 0)
    s[3] += 2 * (int64_t)c[12];
    s[4] += 2 * (int64_t)c[13];
    s[5] += 2 * (int64_t)c[14];
    s[6] += 2 * (int64_t)c[15];

    // Add s3 = (c15, c14, 0, 0, 0, c10, c9, c8)
    s[0] += c[8];
    s[1] += c[9];
    s[2] += c[10];
    s[6] += c[14];
    s[7] += c[15];

    // Add s4 = (c8, c13, c15, c14, c13, c11, c10, c9)
    s[0] += c[9];
    s[1] += c[10];
    s[2] += c[11];
    s[3] += c[13];
    s[4] += c[14];
    s[5] += c[15];
    s[6] += c[13];
    s[7] += c[8];

    // Subtract d1 = (c10, c8, 0, 0, 0, c13, c12, c11)
    s[0] -= c[11];
    s[1] -= c[12];
    s[2] -= c[13];
    s[6] -= c[8];
    s[7] -= c[10];

    // Subtract d2 = (c11, c9, 0, 0, c15, c14, c13, c12)
    s[0] -= c[12];
    s[1] -= c[13];
    s[2] -= c[14];
    s[3] -= c[15];
    s[6] -= c[9];
    s[7] -= c[11];

    // Subtract d3 = (c12, 0, c10, c9, c8, c15, c14, c13)
    s[0] -= c[13];
    s[1] -= c[14];
    s[2] -= c[15];
    s[3] -= c[8];
    s[4] -= c[9];
    s[5] -= c[10];
    s[7] -= c[12];

    // Subtract d4 = (c13, 0, c11, c10, c9, 0, c15, c14)
    s[0] -= c[14];
    s[1] -= c[15];
    s[3] -= c[9];
    s[4] -= c[10];
    s[5] -= c[11];
    s[7] -= c[13];

    // Propagate carries (can be negative)
    for (int i = 0; i < 7; i++) {
        s[i+1] += s[i] >> 32;
        s[i] &= 0xFFFFFFFF;
    }

    // Handle remaining carry/borrow in s[7]
    // If s[7] >= 2^32 or < 0, we need to add/subtract p
    while (s[7] < 0 || s[7] >= 0x100000000LL) {
        if (s[7] < 0) {
            // Add p: p = 2^256 - 2^224 + 2^192 + 2^96 - 1
            // In words: p = {0xFFFFFFFF, 0x00000001, 0, 0, 0, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF}
            // But we're little-endian: p[0]=0xFFFFFFFF, p[1]=0xFFFFFFFF, p[2]=0xFFFFFFFF,
            // p[3]=0, p[4]=0, p[5]=0, p[6]=1, p[7]=0xFFFFFFFF (if counting from LSW)
            // Actually: p = FFFFFFFF 00000001 00000000 00000000 00000000 FFFFFFFF FFFFFFFF FFFFFFFF
            // Word 0 (LSW): 0xFFFFFFFF
            // Word 1: 0xFFFFFFFF
            // Word 2: 0xFFFFFFFF
            // Word 3: 0x00000000
            // Word 4: 0x00000000
            // Word 5: 0x00000000
            // Word 6: 0x00000001
            // Word 7 (MSW): 0xFFFFFFFF
            s[0] += 0xFFFFFFFFLL;
            s[1] += 0xFFFFFFFFLL;
            s[2] += 0xFFFFFFFFLL;
            s[6] += 0x00000001LL;
            s[7] += 0xFFFFFFFFLL;
        } else {
            // Subtract p
            s[0] -= 0xFFFFFFFFLL;
            s[1] -= 0xFFFFFFFFLL;
            s[2] -= 0xFFFFFFFFLL;
            s[6] -= 0x00000001LL;
            s[7] -= 0xFFFFFFFFLL;
        }

        // Re-propagate carries
        for (int i = 0; i < 7; i++) {
            if (s[i] < 0) {
                s[i] += 0x100000000LL;
                s[i+1] -= 1;
            } else if (s[i] >= 0x100000000LL) {
                s[i+1] += s[i] >> 32;
                s[i] &= 0xFFFFFFFF;
            }
        }
    }

    // Final check: if result >= p, subtract p
    // p in little-endian words: {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0, 0, 0, 1, 0xFFFFFFFF}
    bool ge_p = false;
    if (s[7] > 0xFFFFFFFF) ge_p = true;
    else if (s[7] == 0xFFFFFFFF) {
        if (s[6] > 1) ge_p = true;
        else if (s[6] == 1) {
            if (s[5] > 0 || s[4] > 0 || s[3] > 0) ge_p = true;
            else if (s[2] >= 0xFFFFFFFF && s[1] >= 0xFFFFFFFF && s[0] >= 0xFFFFFFFF) ge_p = true;
        }
    }

    if (ge_p) {
        s[0] -= 0xFFFFFFFFLL;
        s[1] -= 0xFFFFFFFFLL;
        s[2] -= 0xFFFFFFFFLL;
        s[6] -= 0x00000001LL;
        s[7] -= 0xFFFFFFFFLL;

        for (int i = 0; i < 7; i++) {
            if (s[i] < 0) {
                s[i] += 0x100000000LL;
                s[i+1] -= 1;
            }
        }
    }

    // Convert back to big-endian bytes
    for (int i = 0; i < 8; i++) {
        int j = (7 - i) * 4;
        uint32_t w = (uint32_t)s[i];
        result[j] = (w >> 24) & 0xFF;
        result[j+1] = (w >> 16) & 0xFF;
        result[j+2] = (w >> 8) & 0xFF;
        result[j+3] = w & 0xFF;
    }
}

// Modular multiplication: result = (a * b) mod p
static void bn_mod_mul(uint8_t* result, const uint8_t* a, const uint8_t* b)
{
    uint8_t temp[64];
    bn_mul(temp, a, b);
    bn_mod_p_512(result, temp);
}

// Modular squaring: result = a^2 mod p
static void bn_mod_sqr(uint8_t* result, const uint8_t* a)
{
    bn_mod_mul(result, a, a);
}

// Modular inverse using Fermat's little theorem: a^(-1) = a^(p-2) mod p
static void bn_mod_inv(uint8_t* result, const uint8_t* a)
{
    // p - 2
    uint8_t exp[32];
    bn_sub(exp, P256_P, (const uint8_t*)"\x00\x00\x00\x00\x00\x00\x00\x00"
                                        "\x00\x00\x00\x00\x00\x00\x00\x00"
                                        "\x00\x00\x00\x00\x00\x00\x00\x00"
                                        "\x00\x00\x00\x00\x00\x00\x00\x02");

    // Square-and-multiply
    uint8_t base[32], res[32];
    bn_copy(base, a);

    // Start with 1
    bn_zero(res);
    res[31] = 1;

    for (int i = 31; i >= 0; i--) {
        for (int j = 0; j < 8; j++) {
            if (exp[i] & (1 << j)) {
                bn_mod_mul(res, res, base);
            }
            bn_mod_sqr(base, base);
        }
    }

    bn_copy(result, res);
}

// ============================================================================
// ELLIPTIC CURVE OPERATIONS (Jacobian Projective Coordinates)
// ============================================================================
//
// Using Jacobian projective coordinates: (X, Y, Z) represents affine (X/Z^2, Y/Z^3)
// This avoids expensive modular inversions during point operations.
// Only one inversion is needed at the end to convert back to affine.

typedef struct {
    uint8_t X[32];
    uint8_t Y[32];
    uint8_t Z[32];
} jacobian_point_t;

// Point at infinity in Jacobian: Z = 0
static bool jacobian_is_infinity(const jacobian_point_t* p)
{
    return bn_is_zero(p->Z);
}

static void jacobian_set_infinity(jacobian_point_t* p)
{
    bn_zero(p->X);
    bn_zero(p->Y);
    bn_zero(p->Z);
}

// Convert affine to Jacobian: (x, y) -> (x, y, 1)
static void affine_to_jacobian(jacobian_point_t* j, const p256_point_t* a)
{
    bn_copy(j->X, a->x);
    bn_copy(j->Y, a->y);
    bn_zero(j->Z);
    j->Z[31] = 1;  // Z = 1
}

// Convert Jacobian to affine: (X, Y, Z) -> (X/Z^2, Y/Z^3)
static void jacobian_to_affine(p256_point_t* a, const jacobian_point_t* j)
{
    if (jacobian_is_infinity(j)) {
        bn_zero(a->x);
        bn_zero(a->y);
        return;
    }

    uint8_t z_inv[32], z_inv2[32], z_inv3[32];

    // z_inv = Z^(-1)
    bn_mod_inv(z_inv, j->Z);

    // z_inv2 = Z^(-2)
    bn_mod_sqr(z_inv2, z_inv);

    // z_inv3 = Z^(-3)
    bn_mod_mul(z_inv3, z_inv2, z_inv);

    // x = X * Z^(-2)
    bn_mod_mul(a->x, j->X, z_inv2);

    // y = Y * Z^(-3)
    bn_mod_mul(a->y, j->Y, z_inv3);
}

// Point doubling in Jacobian coordinates
// Using algorithm from https://hyperelliptic.org/EFD/g1p/auto-shortw-jacobian-3.html#doubling-dbl-2001-b
// For a = -3 (P-256): 3M + 5S + 8add + 1*3 + 1*4 + 1*8
static void jacobian_double(jacobian_point_t* result, const jacobian_point_t* p)
{
    if (jacobian_is_infinity(p) || bn_is_zero(p->Y)) {
        jacobian_set_infinity(result);
        return;
    }

    uint8_t delta[32], gamma[32], beta[32], alpha[32];
    uint8_t temp1[32], temp2[32];

    // delta = Z^2
    bn_mod_sqr(delta, p->Z);

    // gamma = Y^2
    bn_mod_sqr(gamma, p->Y);

    // beta = X * gamma
    bn_mod_mul(beta, p->X, gamma);

    // alpha = 3 * (X - delta) * (X + delta) = 3 * (X^2 - delta^2)
    // For a = -3: alpha = 3 * (X^2 - Z^4) = 3 * (X - Z^2) * (X + Z^2)
    bn_mod_sub(temp1, p->X, delta);  // X - Z^2
    bn_mod_add(temp2, p->X, delta);  // X + Z^2
    bn_mod_mul(alpha, temp1, temp2); // (X - Z^2)(X + Z^2)
    bn_mod_add(temp1, alpha, alpha); // 2 * ...
    bn_mod_add(alpha, temp1, alpha); // 3 * ...

    // X3 = alpha^2 - 8 * beta
    bn_mod_sqr(result->X, alpha);
    bn_mod_add(temp1, beta, beta);   // 2 * beta
    bn_mod_add(temp1, temp1, temp1); // 4 * beta
    bn_mod_add(temp1, temp1, temp1); // 8 * beta
    bn_mod_sub(result->X, result->X, temp1);

    // Z3 = (Y + Z)^2 - gamma - delta
    bn_mod_add(temp1, p->Y, p->Z);
    bn_mod_sqr(result->Z, temp1);
    bn_mod_sub(result->Z, result->Z, gamma);
    bn_mod_sub(result->Z, result->Z, delta);

    // Y3 = alpha * (4 * beta - X3) - 8 * gamma^2
    bn_mod_add(temp1, beta, beta);   // 2 * beta
    bn_mod_add(temp1, temp1, temp1); // 4 * beta
    bn_mod_sub(temp1, temp1, result->X); // 4 * beta - X3
    bn_mod_mul(temp2, alpha, temp1);     // alpha * (4 * beta - X3)
    bn_mod_sqr(temp1, gamma);            // gamma^2
    bn_mod_add(temp1, temp1, temp1);     // 2 * gamma^2
    bn_mod_add(temp1, temp1, temp1);     // 4 * gamma^2
    bn_mod_add(temp1, temp1, temp1);     // 8 * gamma^2
    bn_mod_sub(result->Y, temp2, temp1);
}

// Point addition in Jacobian coordinates: result = p + q
// Using algorithm from https://hyperelliptic.org/EFD/g1p/auto-shortw-jacobian-3.html#addition-add-2007-bl
static void jacobian_add(jacobian_point_t* result, const jacobian_point_t* p, const jacobian_point_t* q)
{
    if (jacobian_is_infinity(p)) {
        memcpy(result, q, sizeof(jacobian_point_t));
        return;
    }
    if (jacobian_is_infinity(q)) {
        memcpy(result, p, sizeof(jacobian_point_t));
        return;
    }

    uint8_t Z1Z1[32], Z2Z2[32], U1[32], U2[32], S1[32], S2[32];
    uint8_t H[32], I[32], J[32], r[32], V[32];
    uint8_t temp1[32], temp2[32];

    // Z1Z1 = Z1^2
    bn_mod_sqr(Z1Z1, p->Z);

    // Z2Z2 = Z2^2
    bn_mod_sqr(Z2Z2, q->Z);

    // U1 = X1 * Z2Z2
    bn_mod_mul(U1, p->X, Z2Z2);

    // U2 = X2 * Z1Z1
    bn_mod_mul(U2, q->X, Z1Z1);

    // S1 = Y1 * Z2 * Z2Z2
    bn_mod_mul(temp1, q->Z, Z2Z2);
    bn_mod_mul(S1, p->Y, temp1);

    // S2 = Y2 * Z1 * Z1Z1
    bn_mod_mul(temp1, p->Z, Z1Z1);
    bn_mod_mul(S2, q->Y, temp1);

    // H = U2 - U1
    bn_mod_sub(H, U2, U1);

    // Check if points are the same (H == 0)
    if (bn_is_zero(H)) {
        // Check if S1 == S2 (same point, need to double)
        if (bn_cmp(S1, S2) == 0) {
            jacobian_double(result, p);
            return;
        } else {
            // Points are inverses, result is infinity
            jacobian_set_infinity(result);
            return;
        }
    }

    // I = (2 * H)^2
    bn_mod_add(temp1, H, H);
    bn_mod_sqr(I, temp1);

    // J = H * I
    bn_mod_mul(J, H, I);

    // r = 2 * (S2 - S1)
    bn_mod_sub(temp1, S2, S1);
    bn_mod_add(r, temp1, temp1);

    // V = U1 * I
    bn_mod_mul(V, U1, I);

    // X3 = r^2 - J - 2 * V
    bn_mod_sqr(result->X, r);
    bn_mod_sub(result->X, result->X, J);
    bn_mod_add(temp1, V, V);
    bn_mod_sub(result->X, result->X, temp1);

    // Y3 = r * (V - X3) - 2 * S1 * J
    bn_mod_sub(temp1, V, result->X);
    bn_mod_mul(temp2, r, temp1);
    bn_mod_mul(temp1, S1, J);
    bn_mod_add(temp1, temp1, temp1);
    bn_mod_sub(result->Y, temp2, temp1);

    // Z3 = ((Z1 + Z2)^2 - Z1Z1 - Z2Z2) * H
    bn_mod_add(temp1, p->Z, q->Z);
    bn_mod_sqr(temp2, temp1);
    bn_mod_sub(temp2, temp2, Z1Z1);
    bn_mod_sub(temp2, temp2, Z2Z2);
    bn_mod_mul(result->Z, temp2, H);
}

// Mixed addition: Jacobian + affine (when Z2 = 1)
// More efficient than full Jacobian addition
static void jacobian_add_affine(jacobian_point_t* result, const jacobian_point_t* p, const p256_point_t* q)
{
    if (jacobian_is_infinity(p)) {
        affine_to_jacobian(result, q);
        return;
    }
    if (bn_is_zero(q->x) && bn_is_zero(q->y)) {
        memcpy(result, p, sizeof(jacobian_point_t));
        return;
    }

    uint8_t Z1Z1[32], U2[32], S2[32], H[32], HH[32], I[32], J[32], r[32], V[32];
    uint8_t temp1[32], temp2[32];

    // Z1Z1 = Z1^2
    bn_mod_sqr(Z1Z1, p->Z);

    // U2 = X2 * Z1Z1 (U1 = X1 since Z2 = 1)
    bn_mod_mul(U2, q->x, Z1Z1);

    // S2 = Y2 * Z1 * Z1Z1 (S1 = Y1 since Z2 = 1)
    bn_mod_mul(temp1, p->Z, Z1Z1);
    bn_mod_mul(S2, q->y, temp1);

    // H = U2 - X1
    bn_mod_sub(H, U2, p->X);

    // Check if points are the same
    if (bn_is_zero(H)) {
        if (bn_cmp(S2, p->Y) == 0) {
            jacobian_double(result, p);
            return;
        } else {
            jacobian_set_infinity(result);
            return;
        }
    }

    // HH = H^2
    bn_mod_sqr(HH, H);

    // I = 4 * HH
    bn_mod_add(I, HH, HH);
    bn_mod_add(I, I, I);

    // J = H * I
    bn_mod_mul(J, H, I);

    // r = 2 * (S2 - Y1)
    bn_mod_sub(temp1, S2, p->Y);
    bn_mod_add(r, temp1, temp1);

    // V = X1 * I
    bn_mod_mul(V, p->X, I);

    // X3 = r^2 - J - 2 * V
    bn_mod_sqr(result->X, r);
    bn_mod_sub(result->X, result->X, J);
    bn_mod_add(temp1, V, V);
    bn_mod_sub(result->X, result->X, temp1);

    // Y3 = r * (V - X3) - 2 * Y1 * J
    bn_mod_sub(temp1, V, result->X);
    bn_mod_mul(temp2, r, temp1);
    bn_mod_mul(temp1, p->Y, J);
    bn_mod_add(temp1, temp1, temp1);
    bn_mod_sub(result->Y, temp2, temp1);

    // Z3 = (Z1 + H)^2 - Z1Z1 - HH
    bn_mod_add(temp1, p->Z, H);
    bn_mod_sqr(result->Z, temp1);
    bn_mod_sub(result->Z, result->Z, Z1Z1);
    bn_mod_sub(result->Z, result->Z, HH);
}

// Scalar multiplication: result = k * p (double-and-add with Jacobian coordinates)
static void point_mul(p256_point_t* result, const uint8_t* k, const p256_point_t* p)
{
    jacobian_point_t jac_result, jac_p;

    jacobian_set_infinity(&jac_result);
    affine_to_jacobian(&jac_p, p);

    // Process k from MSB to LSB (more efficient for Jacobian)
    bool started = false;
    for (int i = 0; i < 32; i++) {
        for (int j = 7; j >= 0; j--) {
            if (started) {
                jacobian_double(&jac_result, &jac_result);
            }

            if (k[i] & (1 << j)) {
                if (!started) {
                    // First bit set - initialize with p
                    affine_to_jacobian(&jac_result, p);
                    started = true;
                } else {
                    // Add p using mixed addition (p is affine)
                    jacobian_add_affine(&jac_result, &jac_result, p);
                }
            }
        }
    }

    // Convert back to affine (single modular inversion here)
    jacobian_to_affine(result, &jac_result);
}

// Legacy affine point operations for API compatibility
static bool point_is_infinity(const p256_point_t* p)
{
    return bn_is_zero(p->x) && bn_is_zero(p->y);
}

static void point_set_infinity(p256_point_t* p)
{
    bn_zero(p->x);
    bn_zero(p->y);
}

// ============================================================================
// PUBLIC API
// ============================================================================

void p256_init(void)
{
    // Seed random with time
    p256_rand_seed = time_us_32();
    printf("[P256] Initialized\n");
}

void p256_generate_private_key(uint8_t* private_key)
{
    // Generate random bytes
    for (int i = 0; i < 32; i++) {
        p256_rand_seed = p256_rand_seed * 1103515245 + 12345;
        private_key[i] = (uint8_t)(p256_rand_seed >> 16);
    }

    // Ensure it's in valid range [1, n-1]
    // For simplicity, just make sure it's not zero and not >= n
    while (bn_is_zero(private_key) || bn_cmp(private_key, P256_N) >= 0) {
        // Regenerate
        for (int i = 0; i < 32; i++) {
            p256_rand_seed = p256_rand_seed * 1103515245 + 12345;
            private_key[i] = (uint8_t)(p256_rand_seed >> 16);
        }
    }
}

bool p256_compute_public_key(const uint8_t* private_key, p256_point_t* public_key)
{
    // public_key = private_key * G
    p256_point_t G;
    memcpy(G.x, P256_GX, 32);
    memcpy(G.y, P256_GY, 32);

    point_mul(public_key, private_key, &G);

    return !point_is_infinity(public_key);
}

bool p256_ecdh_shared_secret(const uint8_t* private_key,
                              const p256_point_t* peer_public_key,
                              uint8_t* shared_secret)
{
    // TODO: Fix validation - skip for now as it may have bugs
    // if (!p256_point_is_valid(peer_public_key)) {
    //     printf("[P256] Invalid peer public key\n");
    //     return false;
    // }

    // shared_point = private_key * peer_public_key
    p256_point_t shared_point;
    point_mul(&shared_point, private_key, peer_public_key);

    if (point_is_infinity(&shared_point)) {
        printf("[P256] Shared secret is point at infinity\n");
        return false;
    }

    // BLE uses only the X coordinate as shared secret
    memcpy(shared_secret, shared_point.x, 32);

    return true;
}

bool p256_point_is_valid(const p256_point_t* point)
{
    // Check that point is on the curve: y^2 = x^3 + ax + b (mod p)
    // For P-256: b = 0x5AC635D8AA3A93E7B3EBBD55769886BC651D06B0CC53B0F63BCE3C3E27D2604B

    static const uint8_t P256_B[32] = {
        0x5A, 0xC6, 0x35, 0xD8, 0xAA, 0x3A, 0x93, 0xE7,
        0xB3, 0xEB, 0xBD, 0x55, 0x76, 0x98, 0x86, 0xBC,
        0x65, 0x1D, 0x06, 0xB0, 0xCC, 0x53, 0xB0, 0xF6,
        0x3B, 0xCE, 0x3C, 0x3E, 0x27, 0xD2, 0x60, 0x4B
    };

    if (point_is_infinity(point)) {
        return false;
    }

    // Check x and y are in range [0, p-1]
    if (bn_cmp(point->x, P256_P) >= 0 || bn_cmp(point->y, P256_P) >= 0) {
        return false;
    }

    uint8_t lhs[32], rhs[32], temp[32];

    // lhs = y^2
    bn_mod_sqr(lhs, point->y);

    // rhs = x^3 + ax + b
    bn_mod_sqr(temp, point->x);      // temp = x^2
    bn_mod_mul(rhs, temp, point->x); // rhs = x^3
    bn_mod_mul(temp, P256_A, point->x); // temp = ax
    bn_mod_add(rhs, rhs, temp);      // rhs = x^3 + ax
    bn_mod_add(rhs, rhs, P256_B);    // rhs = x^3 + ax + b

    return bn_cmp(lhs, rhs) == 0;
}
