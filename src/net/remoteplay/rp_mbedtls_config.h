// rp_mbedtls_config.h - mbedTLS configuration for the usb2wifi PSN OAuth client
//
// This is a TLS 1.2 *client* config, just big enough to open an HTTPS connection
// to Sony's auth endpoint (auth.api.sonyentertainmentnetwork.com) and POST/GET
// the OAuth token exchange. It is NOT the PS4 RSA-signing config
// (ps4_mbedtls_config.h) — usb2wifi links pico_lwip_mbedtls (altcp_tls) instead.
//
// Certificate verification is intentionally disabled (VERIFY_NONE, see rp_oauth.c):
// we don't bundle Sony's root CA chain (it rotates), and this is a one-time
// provisioning step on the user's own LAN. The tradeoff is documented there.
//
// Usage: -DMBEDTLS_CONFIG_FILE="rp_mbedtls_config.h" with this directory on the
// include path (wired in src/CMakeLists.txt for joypad_usb2wifi).
//
// SPDX-License-Identifier: Apache-2.0

#ifndef RP_MBEDTLS_CONFIG_H
#define RP_MBEDTLS_CONFIG_H

// Some mbedtls sources use INT_MAX without including limits.h.
#include <limits.h>

// --- Platform: bare-metal, no OS entropy / filesystem / sockets ---------------
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_PLATFORM_MEMORY          // use platform calloc/free (lwip/libc)
#define MBEDTLS_NO_PLATFORM_ENTROPY      // no /dev/urandom
#define MBEDTLS_ENTROPY_HARDWARE_ALT     // pico_mbedtls provides mbedtls_hardware_poll()
#define MBEDTLS_ALLOW_PRIVATE_ACCESS     // altcp_tls reaches into ssl_context

// Time: altcp_tls references mbedtls_ssl_session.start (present only with
// MBEDTLS_HAVE_TIME) and the handshake stamps ClientHello.random. We don't have
// an RTC and we don't verify cert validity windows (VERIFY_NONE), so we provide
// a monotonic boot-seconds clock rather than depend on newlib time().
#define MBEDTLS_HAVE_TIME
#define MBEDTLS_PLATFORM_TIME_TYPE_MACRO  long
#define MBEDTLS_PLATFORM_TIME_MACRO       rp_mbedtls_time
#define MBEDTLS_PLATFORM_MS_TIME_ALT      // we supply mbedtls_ms_time() (rp_oauth.c)
#ifndef __ASSEMBLER__
long rp_mbedtls_time(long* t);           // defined in rp_oauth.c
#endif

// --- RNG / entropy ------------------------------------------------------------
#define MBEDTLS_ENTROPY_C
#define MBEDTLS_CTR_DRBG_C

// Memory: MBEDTLS_PLATFORM_MEMORY makes altcp_tls route all mbedTLS allocations
// through lwip's mem_malloc (its tls_malloc shim). The TLS session buffers thus
// come from the lwip heap (MEM_SIZE in lwipopts.h), which we've sized for a full
// TLS 1.2 session. (A private mbedtls buffer pool would just be overridden by
// altcp, so we don't use one.)

// --- TLS 1.2 client -----------------------------------------------------------
#define MBEDTLS_SSL_TLS_C
#define MBEDTLS_SSL_CLI_C
#define MBEDTLS_SSL_PROTO_TLS1_2
#define MBEDTLS_SSL_SERVER_NAME_INDICATION   // SNI — Sony's vhost needs it
#define MBEDTLS_SSL_OUT_CONTENT_LEN    2048  // our requests are tiny
// IN_CONTENT_LEN left at the 16384 default so a full server cert chain fits.

// Key exchanges seen from Sony's CDN/edge (ECDHE-RSA is the common one).
#define MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED
#define MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED
#define MBEDTLS_KEY_EXCHANGE_RSA_ENABLED

// --- Public key / X.509 (parse server cert, even though we don't verify it) ---
#define MBEDTLS_PK_C
#define MBEDTLS_PK_PARSE_C
#define MBEDTLS_RSA_C
#define MBEDTLS_PKCS1_V15
#define MBEDTLS_PKCS1_V21
#define MBEDTLS_ECDH_C
#define MBEDTLS_ECDSA_C
#define MBEDTLS_ECP_C
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C
#define MBEDTLS_OID_C
#define MBEDTLS_X509_USE_C
#define MBEDTLS_X509_CRT_PARSE_C
#define MBEDTLS_BIGNUM_C

// Curves offered in the handshake.
#define MBEDTLS_ECP_DP_SECP256K1_ENABLED   // Remote Play session ECDH uses secp256k1
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED
#define MBEDTLS_ECP_DP_SECP384R1_ENABLED
#define MBEDTLS_ECP_DP_SECP521R1_ENABLED
#define MBEDTLS_ECP_DP_CURVE25519_ENABLED

// --- Ciphers / hashes ---------------------------------------------------------
#define MBEDTLS_AES_C
#define MBEDTLS_AES_FEWER_TABLES
#define MBEDTLS_GCM_C
#define MBEDTLS_CIPHER_MODE_CBC
#define MBEDTLS_CIPHER_MODE_CFB    // chiaki rpcrypt (console registration) uses AES-CFB128
#define MBEDTLS_CIPHER_C
#define MBEDTLS_MD_C
#define MBEDTLS_MD5_C            // TLS 1.2 PRF / legacy
#define MBEDTLS_SHA1_C          // cert signatures still use it
#define MBEDTLS_SHA224_C
#define MBEDTLS_SHA256_C
#define MBEDTLS_SHA256_SMALLER
#define MBEDTLS_SHA384_C
#define MBEDTLS_SHA512_C

// --- Diagnostics --------------------------------------------------------------
#define MBEDTLS_ERROR_C

#endif // RP_MBEDTLS_CONFIG_H
