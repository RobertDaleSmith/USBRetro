#ifndef RP_COMPAT_SYS_SOCKET_H
#define RP_COMPAT_SYS_SOCKET_H
#include <stdint.h>
#include <stddef.h>
typedef uint32_t socklen_t;
typedef uint16_t sa_family_t;
typedef uint16_t in_port_t;
struct in_addr { uint32_t s_addr; };
struct in6_addr { uint8_t s6_addr[16]; };
struct sockaddr { sa_family_t sa_family; char sa_data[14]; };
struct sockaddr_in { sa_family_t sin_family; in_port_t sin_port; struct in_addr sin_addr; char sin_zero[8]; };
struct sockaddr_in6 { sa_family_t sin6_family; in_port_t sin6_port; uint32_t sin6_flowinfo; struct in6_addr sin6_addr; uint32_t sin6_scope_id; };
struct sockaddr_storage { sa_family_t ss_family; char __pad[128 - sizeof(sa_family_t)]; };
#define AF_INET   2
#define AF_INET6  10
#define AF_UNSPEC 0
#define SOCK_DGRAM 2
#define SOCK_STREAM 1
#define IPPROTO_UDP 17
#define IPPROTO_TCP 6
#define INADDR_ANY 0
#define INET_ADDRSTRLEN  16
#define INET6_ADDRSTRLEN 46
#define SOL_SOCKET 1
#define SO_BROADCAST 6
#define SO_REUSEADDR 2
#endif
