#include <stdlib.h>
#include <string.h>
#include "net/socket_address.h"
#include "net/ipv4_address.h"
#include "net/ipv6_address.h"
#include "net/local_address.h"
#include "macros/macros.h"

bool net::socket_address::build(const char* address, unsigned short port)
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

		memset(addr->sin_zero, 0, sizeof(addr->sin_zero));
	}

	return true;
}

bool net::socket_address::build(const char* address)
{
	const char* colon = NULL;
	const char* p = address;
	char c;
	while ((c = *p) != 0) {
		if (c == ':') {
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
	while ((c = *p) != 0) {
		if (!IS_DIGIT(c)) {
			return false;
		}

		if ((port = (port * 10) + (c - '0')) > 65535) {
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
				const ipv4_address* addr1 = reinterpret_cast<const ipv4_address*>(this);
				const ipv4_address* addr2 = reinterpret_cast<const ipv4_address*>(&addr);
				return ((addr1->sin_port == addr2->sin_port) && (addr1->sin_addr.s_addr == addr2->sin_addr.s_addr));
			}
		case AF_INET6:
			{
				const ipv6_address* addr1 = reinterpret_cast<const ipv6_address*>(this);
				const ipv6_address* addr2 = reinterpret_cast<const ipv6_address*>(&addr);
				return ((addr1->sin6_port == addr2->sin6_port) && (memcmp(&addr1->sin6_addr, &addr2->sin6_addr, sizeof(struct in6_addr)) == 0));
			}
		case AF_UNIX:
			{
				const local_address* addr1 = reinterpret_cast<const local_address*>(this);
				const local_address* addr2 = reinterpret_cast<const local_address*>(&addr);
				return (strcmp(addr1->sun_path, addr2->sun_path) == 0);
			}
		default:
			return false;
	}
}
