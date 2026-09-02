// Minimal shim so chiaki files that only need byte-order helpers build bare-metal.
#ifndef RP_COMPAT_ARPA_INET_H
#define RP_COMPAT_ARPA_INET_H
#include <stdint.h>
#include "../sys/socket.h"
static inline uint16_t htons(uint16_t x){ return __builtin_bswap16(x); }
static inline uint16_t ntohs(uint16_t x){ return __builtin_bswap16(x); }
static inline uint32_t htonl(uint32_t x){ return __builtin_bswap32(x); }
static inline uint32_t ntohl(uint32_t x){ return __builtin_bswap32(x); }
uint32_t inet_addr(const char *cp);
char *inet_ntoa(struct in_addr in);
int inet_pton(int af, const char *src, void *dst);
const char *inet_ntop(int af, const void *src, char *dst, socklen_t size);
#endif
