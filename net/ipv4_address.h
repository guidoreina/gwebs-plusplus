#ifndef IPV4_ADDRESS_H
#define IPV4_ADDRESS_H

#include <netinet/in.h>
#include <arpa/inet.h>
#include "net/socket_address.h"

namespace net {
	class ipv4_address : public sockaddr_in {
		public:
			static const char* ANY_ADDRESS;

			// Constructor.
			ipv4_address();

			// Cast operators.
			operator const socket_address*() const;
			operator socket_address*();

			operator const socket_address&() const;
			operator socket_address&();

			// Get port.
			in_port_t port() const;

			// Set port.
			void port(in_port_t p);
	};

	inline ipv4_address::ipv4_address()
	{
		sin_family = AF_INET;

		for (size_t i = 0; i < sizeof(sin_zero); i++) {
			sin_zero[i] = 0;
		}
	}

	inline ipv4_address::operator const socket_address*() const
	{
		return reinterpret_cast<const socket_address*>(this);
	}

	inline ipv4_address::operator socket_address*()
	{
		return reinterpret_cast<socket_address*>(this);
	}

	inline ipv4_address::operator const socket_address&() const
	{
		return reinterpret_cast<const socket_address&>(*this);
	}

	inline ipv4_address::operator socket_address&()
	{
		return reinterpret_cast<socket_address&>(*this);
	}

	inline in_port_t ipv4_address::port() const
	{
		return ntohs(sin_port);
	}

	inline void ipv4_address::port(in_port_t p)
	{
		sin_port = htons(p);
	}
}

#endif // IPV4_ADDRESS_H
