#ifndef SOCKET_H
#define SOCKET_H

#include <sys/uio.h>
#include "net/socket_address.h"

namespace net {
	class socket {
		public:
			static const unsigned BACKLOG = 128;

			enum type {STREAM = SOCK_STREAM, DATAGRAM = SOCK_DGRAM, RAW = SOCK_RAW};

			// Constructor.
			socket();
			socket(int fd);

			// Close socket.
			bool close();

			// Shutdown socket.
			bool shutdown(int how);

			// Get socket error.
			bool get_socket_error(int& error);

			// Connect.
			bool connect(type type, const socket_address& addr, int timeout = -1);

			// Listen.
			bool listen(const socket_address& addr);

			// Accept.
			bool accept(socket& s, int timeout = -1);
			bool accept(socket& s, socket_address& addr, int timeout = -1);

			// Read.
			ssize_t read(void* buf, size_t count, int timeout = -1);

			// Read into multiple buffers.
			ssize_t readv(const struct iovec* iov, unsigned iovcnt, int timeout = -1);

			// Write.
			ssize_t write(const void* buf, size_t count, int timeout = -1);

			// Write from multiple buffers.
			ssize_t writev(const struct iovec* iov, unsigned iovcnt, int timeout = -1);

			// Get receive buffer size.
			bool get_recvbuf_size(int& size) const;

			// Set receive buffer size.
			bool set_recvbuf_size(int size);

			// Get send buffer size.
			bool get_sendbuf_size(int& size) const;

			// Set send buffer size.
			bool set_sendbuf_size(int size);

			// Get TCP no delay.
			bool get_tcp_no_delay(bool& on) const;

			// Set TCP no delay.
			bool set_tcp_no_delay(bool on);

			// Cork.
			bool cork();

			// Uncork.
			bool uncork();

			// Get socket descriptor.
			int fd() const;

			// Set socket descriptor.
			void fd(int descriptor);

			// Wait readable.
			bool wait_readable(int timeout);

			// Wait writable.
			bool wait_writable(int timeout);

		protected:
			int _M_fd;

			// Create socket.
			bool create(int domain, int type);

			// Make socket non-blocking.
			bool set_non_blocking();

			// Accept.
			bool accept(socket& s, socket_address* addr, socklen_t* addrlen, int timeout);
	};

	inline socket::socket()
	{
	}

	inline socket::socket(int fd)
	{
		_M_fd = fd;
	}

	inline bool socket::accept(socket& s, int timeout)
	{
		return accept(s, NULL, NULL, timeout);
	}

	inline bool socket::accept(socket& s, socket_address& addr, int timeout)
	{
		socklen_t addrlen = sizeof(socket_address);
		return accept(s, &addr, &addrlen, timeout);
	}

	inline int socket::fd() const
	{
		return _M_fd;
	}

	inline void socket::fd(int descriptor)
	{
		_M_fd = descriptor;
	}
}

#endif // SOCKET_H
