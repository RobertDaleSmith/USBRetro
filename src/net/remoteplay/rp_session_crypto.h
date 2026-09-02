// rp_session_crypto.h - Remote Play session ECDH (secp256k1) for the poll-mode
// engine. Reimplements chiaki's chiaki_ecdh_* against modern mbedTLS (chiaki's
// own mbedtls path uses an ECDH-context struct API pico's mbedTLS dropped).
// gkcrypt + rpcrypt are reused from chiaki directly; only ECDH is reimplemented.
// SPDX-License-Identifier: Apache-2.0

#ifndef RP_SESSION_CRYPTO_H
#define RP_SESSION_CRYPTO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "mbedtls/ecp.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"

#define RP_ECDH_SECRET_SIZE     32   // secp256k1 field size (== CHIAKI_ECDH_SECRET_SIZE)
#define RP_ECDH_PUBKEY_MAX      65   // uncompressed 0x04 || X[32] || Y[32]
#define RP_HANDSHAKE_KEY_SIZE   16

typedef struct {
    mbedtls_ecp_group        grp;
    mbedtls_mpi              d;      // private scalar
    mbedtls_ecp_point        Q;      // local public point
    mbedtls_ctr_drbg_context drbg;
    mbedtls_entropy_context  entropy;
    bool                     ready;
} rp_ecdh_t;

// Generate a secp256k1 ephemeral keypair. Returns true on success.
bool rp_ecdh_init(rp_ecdh_t* e);
void rp_ecdh_fini(rp_ecdh_t* e);

// Export the local public key (uncompressed) into key_out (>= RP_ECDH_PUBKEY_MAX)
// and its HMAC-SHA256(handshake_key, pubkey) signature into sig_out (32 bytes).
bool rp_ecdh_get_local_pub_key(rp_ecdh_t* e, uint8_t* key_out, size_t* key_out_size,
                               const uint8_t* handshake_key,
                               uint8_t* sig_out, size_t* sig_out_size);

// Derive the shared secret (RP_ECDH_SECRET_SIZE bytes) from the remote pubkey.
bool rp_ecdh_derive_secret(rp_ecdh_t* e, uint8_t* secret_out,
                           const uint8_t* remote_key, size_t remote_key_size);

#endif // RP_SESSION_CRYPTO_H
