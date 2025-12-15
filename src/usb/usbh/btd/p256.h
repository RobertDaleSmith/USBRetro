// p256.h - NIST P-256 Elliptic Curve for BLE Secure Connections
// Implements ECDH key exchange required for SMP SC pairing

#ifndef P256_H
#define P256_H

#include <stdint.h>
#include <stdbool.h>

// P-256 uses 256-bit (32-byte) integers
#define P256_BYTES 32

// P-256 point (uncompressed: 04 || x || y = 65 bytes)
typedef struct {
    uint8_t x[P256_BYTES];
    uint8_t y[P256_BYTES];
} p256_point_t;

// Initialize P-256 (call once at startup)
void p256_init(void);

// Generate a random private key (32 bytes)
void p256_generate_private_key(uint8_t* private_key);

// Compute public key from private key
// public_key = private_key * G (generator point)
bool p256_compute_public_key(const uint8_t* private_key, p256_point_t* public_key);

// Compute ECDH shared secret
// shared_secret = private_key * peer_public_key
// Returns only the X coordinate (32 bytes) as per BLE spec
bool p256_ecdh_shared_secret(const uint8_t* private_key,
                              const p256_point_t* peer_public_key,
                              uint8_t* shared_secret);

// Validate that a point is on the curve
bool p256_point_is_valid(const p256_point_t* point);

#endif // P256_H
