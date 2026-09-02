#ifndef RP_COMPAT_NETDB_H
#define RP_COMPAT_NETDB_H
#include "sys/socket.h"
struct addrinfo { int ai_flags, ai_family, ai_socktype, ai_protocol; socklen_t ai_addrlen;
    struct sockaddr *ai_addr; char *ai_canonname; struct addrinfo *ai_next; };
int getaddrinfo(const char*, const char*, const struct addrinfo*, struct addrinfo**);
void freeaddrinfo(struct addrinfo*);
#endif
