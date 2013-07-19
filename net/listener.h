#ifndef LISTENER_H
#define LISTENER_H

#include <netinet/in.h>
#include "net/socket.h"
#include "net/ipv4_address.h"
#include "net/ipv6_address.h"
#include "io/event_handler.h"

namespace net {
	class tcp_server;

	struct listener : protected socket, public io::event_handler {
		socket_address _M_addr;
		void* _M_data;

		static tcp_server* _M_server;

		// Create listener.
		bool create(const socket_address& addr);

		// On readable.
		bool on_readable();

		// On writable.
		bool on_writable();

		// Set socket descriptor.
		void fd(int descriptor);

		// Get port.
		in_port_t port() const;
	};

	inline bool listener::create(const socket_address& addr)
	{
		if (!listen(addr)) {
			return false;
		}

		_M_addr = addr;

		return true;
	}

	inline bool listener::on_writable()
	{
		return true;
	}

	inline void listener::fd(int descriptor)
	{
		socket::fd(descriptor);
	}

	inline in_port_t listener::port() const
	{
		switch (_M_addr.ss_family) {
			case AF_INET:
				return reinterpret_cast<const ipv4_address*>(&_M_addr)->port();
			case AF_INET6:
				return reinterpret_cast<const ipv6_address*>(&_M_addr)->port();
			default:
				return 0;
		}
	}
}

#endif // LISTENER_H
