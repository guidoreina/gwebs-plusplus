#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "net/socket_address.h"
#include "net/ipv4_address.h"
#include "net/ipv6_address.h"
#include "net/local_address.h"
#include "macros/macros.h"

socklen_t net::socket_address::size() const
{
  switch (ss_family) {
    case AF_INET:
      return static_cast<socklen_t>(sizeof(ipv4_address));
    case AF_INET6:
      return static_cast<socklen_t>(sizeof(ipv6_address));
    case AF_UNIX:
      return static_cast<socklen_t>(sizeof(local_address));
    default:
      return 0;
  }
}

bool net::socket_address::build(const char* address, in_port_t port)
{
  unsigned char buf[sizeof(struct in6_addr)];

  // Try first with IPv4.
  if (inet_pton(AF_INET, address, buf) <= 0) {
    if (inet_pton(AF_INET6, address, buf) <= 0) {
      return false;
    }

    ipv6_address* addr = reinterpret_cast<ipv6_address*>(this);

    memset(addr, 0, sizeof(ipv6_address));

    addr->sin6_family = AF_INET6;
    memcpy(&addr->sin6_addr, buf, sizeof(struct in6_addr));
    addr->port(port);
  } else {
    ipv4_address* addr = reinterpret_cast<ipv4_address*>(this);

    addr->sin_family = AF_INET;
    memcpy(&addr->sin_addr, buf, sizeof(struct in_addr));
    addr->port(port);

#ifndef __minix
    memset(addr->sin_zero, 0, sizeof(addr->sin_zero));
#endif // !__minix
  }

  return true;
}

bool net::socket_address::build(const char* address)
{
  // Unix socket?
  if (*address == '/') {
    local_address* addr = reinterpret_cast<local_address*>(this);
    addr->sun_family = AF_UNIX;
    return addr->set_path(address);
  }

  // Search the last colon (for IPv6 there might be more than one).
  const char* colon = NULL;
  const char* p = address;
  while (*p) {
    if (*p == ':') {
      colon = p;
    }

    p++;
  }

  // If there is no port...
  if (!colon) {
    return false;
  }

  char addr[64];
  size_t len;
  if (((len = colon - address) == 0) || (len >= sizeof(addr))) {
    return false;
  }

  p = colon + 1;
  unsigned port = 0;
  while (*p) {
    if (!IS_DIGIT(*p)) {
      return false;
    }

    if ((port = (port * 10) + (*p - '0')) > 65535) {
      return false;
    }

    p++;
  }

  if (port == 0) {
    return false;
  }

  memcpy(addr, address, len);
  addr[len] = 0;

  return build(addr, port);
}

bool net::socket_address::operator==(const socket_address& addr) const
{
  if (ss_family != addr.ss_family) {
    return false;
  }

  switch (ss_family) {
    case AF_INET:
      {
        const ipv4_address* addr1 =
                            reinterpret_cast<const ipv4_address*>(this);
        const ipv4_address* addr2 =
                            reinterpret_cast<const ipv4_address*>(&addr);
        return ((addr1->sin_port == addr2->sin_port) &&
                (addr1->sin_addr.s_addr == addr2->sin_addr.s_addr));
      }
    case AF_INET6:
      {
        const ipv6_address* addr1 =
                            reinterpret_cast<const ipv6_address*>(this);
        const ipv6_address* addr2 =
                            reinterpret_cast<const ipv6_address*>(&addr);
        return
          ((addr1->sin6_port == addr2->sin6_port) &&

#if defined(__linux__)
           (((addr1->sin6_addr.s6_addr32[0] ^ addr2->sin6_addr.s6_addr32[0]) |
             (addr1->sin6_addr.s6_addr32[1] ^ addr2->sin6_addr.s6_addr32[1]) |
             (addr1->sin6_addr.s6_addr32[2] ^ addr2->sin6_addr.s6_addr32[2]) |
             (addr1->sin6_addr.s6_addr32[3] ^ addr2->sin6_addr.s6_addr32[3]))
#elif defined(__FreeBSD__) || \
      defined(__OpenBSD__) || \
      defined(__NetBSD__) || \
      defined(__DragonFly__) || \
      defined(__minix)
           (((addr1->sin6_addr.__u6_addr.__u6_addr32[0] ^
              addr2->sin6_addr.__u6_addr.__u6_addr32[0]) |
             (addr1->sin6_addr.__u6_addr.__u6_addr32[1] ^
              addr2->sin6_addr.__u6_addr.__u6_addr32[1]) |
             (addr1->sin6_addr.__u6_addr.__u6_addr32[2] ^
              addr2->sin6_addr.__u6_addr.__u6_addr32[2]) |
             (addr1->sin6_addr.__u6_addr.__u6_addr32[3] ^
              addr2->sin6_addr.__u6_addr.__u6_addr32[3]))
#elif defined(__sun__)
           (((addr1->sin6_addr._S6_un._S6_u32[0] ^
              addr2->sin6_addr._S6_un._S6_u32[0]) |
             (addr1->sin6_addr._S6_un._S6_u32[1] ^
              addr2->sin6_addr._S6_un._S6_u32[1]) |
             (addr1->sin6_addr._S6_un._S6_u32[2] ^
              addr2->sin6_addr._S6_un._S6_u32[2]) |
             (addr1->sin6_addr._S6_un._S6_u32[3] ^
              addr2->sin6_addr._S6_un._S6_u32[3]))
#endif
            == 0));
      }
    case AF_UNIX:
      {
        const local_address* addr1 =
                             reinterpret_cast<const local_address*>(this);
        const local_address* addr2 =
                             reinterpret_cast<const local_address*>(&addr);
        return (strcmp(addr1->sun_path, addr2->sun_path) == 0);
      }
    default:
      return false;
  }
}

const char* net::socket_address::to_string_with_port(char* s, size_t n) const
{
  switch (ss_family) {
    case AF_INET:
      {
        if (!inet_ntop(AF_INET,
                       &reinterpret_cast<const ipv4_address*>(this)->sin_addr,
                       s,
                       n)) {
          return NULL;
        }

        size_t len = strlen(s);
        size_t left = n - len;

        if (snprintf(s + len,
                     left,
                     ":%u",
                     reinterpret_cast<const ipv4_address*>(this)->port())
            >= static_cast<int>(left)) {
          errno = ENOSPC;
          return NULL;
        }

        return s;
      }
    case AF_INET6:
      {
        if (!inet_ntop(AF_INET6,
                       &reinterpret_cast<const ipv6_address*>(this)->sin6_addr,
                       s,
                       n)) {
          return NULL;
        }

        size_t len = strlen(s);
        size_t left = n - len;

        if (snprintf(s + len,
                     left,
                     ":%u",
                     reinterpret_cast<const ipv6_address*>(this)->port())
            >= static_cast<int>(left)) {
          errno = ENOSPC;
          return NULL;
        }

        return s;
      }
    case AF_UNIX:
      {
        const local_address* addr =
                             reinterpret_cast<const local_address*>(this);

        if (n < sizeof(addr->sun_path)) {
          errno = ENOSPC;
          return NULL;
        }

        return reinterpret_cast<char*>(
                 memcpy(s, addr->sun_path, sizeof(addr->sun_path))
               );
      }
    default:
      return NULL;
  }
}

const char* net::socket_address::to_string_without_port(char* s, size_t n) const
{
  switch (ss_family) {
    case AF_INET:
      return inet_ntop(AF_INET,
                       &reinterpret_cast<const ipv4_address*>(this)->sin_addr,
                       s,
                       n);
    case AF_INET6:
      return inet_ntop(AF_INET6,
                       &reinterpret_cast<const ipv6_address*>(this)->sin6_addr,
                       s,
                       n);
    case AF_UNIX:
      {
        const local_address* addr =
                             reinterpret_cast<const local_address*>(this);

        if (n < sizeof(addr->sun_path)) {
          errno = ENOSPC;
          return NULL;
        }

        return reinterpret_cast<char*>(
                 memcpy(s, addr->sun_path, sizeof(addr->sun_path))
               );
      }
    default:
      return NULL;
  }
}
