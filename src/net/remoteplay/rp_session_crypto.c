// rp_session_crypto.c - Remote Play session ECDH (secp256k1), modern mbedTLS.
// Mirrors chiaki's chiaki_ecdh_* semantics exactly (uncompressed pubkey +
// HMAC-SHA256 sig; shared secret = X coord, 32 bytes) but uses explicit ecp
// group/mpi/point objects so it builds against pico-sdk's mbedTLS.
// SPDX-License-Identifier: Apache-2.0

#include "rp_session_crypto.h"

#include "mbedtls/ecdh.h"
#include "mbedtls/md.h"

#include <string.h>

static int rng(void* p, unsigned char* out, size_t len)
{
    return mbedtls_ctr_drbg_random(p, out, len);
}

bool rp_ecdh_init(rp_ecdh_t* e)
{
    memset(e, 0, sizeof(*e));
    mbedtls_ecp_group_init(&e->grp);
    mbedtls_mpi_init(&e->d);
    mbedtls_ecp_point_init(&e->Q);
    mbedtls_ctr_drbg_init(&e->drbg);
    mbedtls_entropy_init(&e->entropy);

    static const char pers[] = "rp_ecdh";
    if (mbedtls_ctr_drbg_seed(&e->drbg, mbedtls_entropy_func, &e->entropy,
                              (const unsigned char*)pers, sizeof(pers)) != 0)
        return false;
    if (mbedtls_ecp_group_load(&e->grp, MBEDTLS_ECP_DP_SECP256K1) != 0)
        return false;
    if (mbedtls_ecp_gen_keypair(&e->grp, &e->d, &e->Q, rng, &e->drbg) != 0)
        return false;
    e->ready = true;
    return true;
}

void rp_ecdh_fini(rp_ecdh_t* e)
{
    if (!e) return;
    mbedtls_ecp_group_free(&e->grp);
    mbedtls_mpi_free(&e->d);
    mbedtls_ecp_point_free(&e->Q);
    mbedtls_ctr_drbg_free(&e->drbg);
    mbedtls_entropy_free(&e->entropy);
    e->ready = false;
}

bool rp_ecdh_get_local_pub_key(rp_ecdh_t* e, uint8_t* key_out, size_t* key_out_size,
                               const uint8_t* handshake_key,
                               uint8_t* sig_out, size_t* sig_out_size)
{
    size_t olen = 0;
    if (mbedtls_ecp_point_write_binary(&e->grp, &e->Q, MBEDTLS_ECP_PF_UNCOMPRESSED,
                                       &olen, key_out, *key_out_size) != 0)
        return false;
    *key_out_size = olen;

    // sig = HMAC-SHA256(handshake_key, pubkey)
    const mbedtls_md_info_t* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (!md) return false;
    if (mbedtls_md_hmac(md, handshake_key, RP_HANDSHAKE_KEY_SIZE,
                        key_out, olen, sig_out) != 0)
        return false;
    if (sig_out_size) *sig_out_size = 32;
    return true;
}

bool rp_ecdh_derive_secret(rp_ecdh_t* e, uint8_t* secret_out,
                           const uint8_t* remote_key, size_t remote_key_size)
{
    mbedtls_ecp_point Qp;
    mbedtls_mpi z;
    mbedtls_ecp_point_init(&Qp);
    mbedtls_mpi_init(&z);
    bool ok = false;

    if (mbedtls_ecp_point_read_binary(&e->grp, &Qp, remote_key, remote_key_size) != 0)
        goto out;
    if (mbedtls_ecdh_compute_shared(&e->grp, &z, &Qp, &e->d, rng, &e->drbg) != 0)
        goto out;
    if (mbedtls_mpi_write_binary(&z, secret_out, RP_ECDH_SECRET_SIZE) != 0)
        goto out;
    ok = true;
out:
    mbedtls_ecp_point_free(&Qp);
    mbedtls_mpi_free(&z);
    return ok;
}
