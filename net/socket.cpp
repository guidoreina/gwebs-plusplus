#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <limits.h>
#include <errno.h>
#include "net/socket.h"
#include "net/local_address.h"

bool net::socket::close()
{
	if (::close(_M_fd) < 0) {
		return false;
	}

	return true;
}

bool net::socket::shutdown(int how)
{
	if (::shutdown(_M_fd, how) < 0) {
		return false;
	}

	return true;
}

bool net::socket::get_socket_error(int& error)
{
	socklen_t errorlen = sizeof(int);
	if (getsockopt(_M_fd, SOL_SOCKET, SO_ERROR, &error, &errorlen) < 0) {
		return false;
	}

	return true;
}

bool net::socket::connect(type type, const socket_address& addr, int timeout)
{
	// Create socket.
	if (!create(addr.ss_family, type)) {
		return false;
	}

	// Connect.
	if ((::connect(_M_fd, reinterpret_cast<const struct sockaddr*>(&addr), addr.size()) < 0) && (errno != EINPROGRESS)) {
		close();
		return false;
	}

	if (timeout != 0) {
		if (!wait_writable(timeout)) {
			close();
			return false;
		}

		int error;
		if ((!get_socket_error(error)) || (error != 0)) {
			close();
			return false;
		}
	}

	return true;
}

bool net::socket::listen(const socket_address& addr)
{
	// Create socket.
	if (!create(addr.ss_family, STREAM)) {
		return false;
	}

	// Reuse address.
	int optval = 1;
	if (setsockopt(_M_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int)) < 0) {
		close();
		return false;
	}

	// Bind.
	if (bind(_M_fd, reinterpret_cast<const struct sockaddr*>(&addr), addr.size()) < 0) {
		if (addr.ss_family == AF_UNIX) {
			unlink(reinterpret_cast<const local_address*>(&addr)->sun_path);
		}

		close();
		return false;
	}

	// Listen.
	if (::listen(_M_fd, BACKLOG) < 0) {
		if (addr.ss_family == AF_UNIX) {
			unlink(reinterpret_cast<const local_address*>(&addr)->sun_path);
		}

		close();
		return false;
	}

	if (addr.ss_family == AF_UNIX) {
		unlink(reinterpret_cast<const local_address*>(&addr)->sun_path);
	}

	return true;
}

ssize_t net::socket::read(void* buf, size_t count, int timeout)
{
	ssize_t ret;

	if (timeout == 0) {
		while (((ret = recv(_M_fd, buf, count, 0)) < 0) && (errno == EINTR));
	} else {
		do {
			if (!wait_readable(timeout)) {
				return -1;
			}
		} while (((ret = recv(_M_fd, buf, count, 0)) < 0) && ((errno == EINTR) || (errno == EAGAIN)));
	}

	return ret;
}

ssize_t net::socket::readv(const struct iovec* iov, unsigned iovcnt, int timeout)
{
	ssize_t ret;

	if (timeout == 0) {
		while (((ret = ::readv(_M_fd, iov, iovcnt)) < 0) && (errno == EINTR));
	} else {
		do {
			if (!wait_readable(timeout)) {
				return -1;
			}
		} while (((ret = ::readv(_M_fd, iov, iovcnt)) < 0) && ((errno == EINTR) || (errno == EAGAIN)));
	}

	return ret;
}

ssize_t net::socket::write(const void* buf, size_t count, int timeout)
{
	if (timeout == 0) {
		ssize_t ret;
		while (((ret = send(_M_fd, buf, count, 0)) < 0) && (errno == EINTR));

		return ret;
	} else {
		const char* b = (const char*) buf;
		size_t written = 0;

		do {
			if (!wait_writable(timeout)) {
				return -1;
			}

			ssize_t ret;
			if ((ret = send(_M_fd, b, count - written, 0)) < 0) {
				if ((errno != EINTR) && (errno != EAGAIN)) {
					return -1;
				}
			} else if (ret > 0) {
				if ((written += ret) == count) {
					return count;
				}

				b += ret;
			}
		} while (true);
	}
}

ssize_t net::socket::writev(const struct iovec* iov, unsigned iovcnt, int timeout)
{
	if (timeout == 0) {
		ssize_t ret;
		while (((ret = ::writev(_M_fd, iov, iovcnt)) < 0) && (errno == EINTR));

		return ret;
	} else {
		if (iovcnt > IOV_MAX) {
			errno = EINVAL;
			return -1;
		}

		struct iovec vec[IOV_MAX];
		size_t total = 0;

		for (unsigned i = 0; i < iovcnt; i++) {
			vec[i].iov_base = iov[i].iov_base;
			vec[i].iov_len = iov[i].iov_len;

			total += vec[i].iov_len;
		}

		struct iovec* v = vec;
		size_t written = 0;

		do {
			if (!wait_writable(timeout)) {
				return -1;
			}

			ssize_t ret;
			if ((ret = ::writev(_M_fd, v, iovcnt)) < 0) {
				if ((errno != EINTR) && (errno != EAGAIN)) {
					return -1;
				}
			} else if (ret > 0) {
				if ((written += ret) == total) {
					return total;
				}

				while ((size_t) ret >= v->iov_len) {
					ret -= v->iov_len;

					v++;
					iovcnt--;
				}

				if (ret > 0) {
					v->iov_base = (char*) v->iov_base + ret;
					v->iov_len -= ret;
				}
			}
		} while (true);
	}
}

bool net::socket::get_recvbuf_size(int& size) const
{
	socklen_t optlen = sizeof(int);
	if (getsockopt(_M_fd, SOL_SOCKET, SO_RCVBUF, &size, &optlen) < 0) {
		return false;
	}

	return true;
}

bool net::socket::set_recvbuf_size(int size)
{
	if (setsockopt(_M_fd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(int)) < 0) {
		return false;
	}

	return true;
}

bool net::socket::get_sendbuf_size(int& size) const
{
	socklen_t optlen = sizeof(int);
	if (getsockopt(_M_fd, SOL_SOCKET, SO_SNDBUF, &size, &optlen) < 0) {
		return false;
	}

	return true;
}

bool net::socket::set_sendbuf_size(int size)
{
	if (setsockopt(_M_fd, SOL_SOCKET, SO_SNDBUF, &size, sizeof(int)) < 0) {
		return false;
	}

	return true;
}

bool net::socket::get_tcp_no_delay(bool& on) const
{
	int optval;
	socklen_t optlen = sizeof(int);
	if (getsockopt(_M_fd, IPPROTO_TCP, TCP_NODELAY, &optval, &optlen) < 0) {
		return false;
	}

	on = (optval != 0);

	return true;
}

bool net::socket::set_tcp_no_delay(bool on)
{
	int optval = (on == true);
	if (setsockopt(_M_fd, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(int)) < 0) {
		return false;
	}

	return true;
}

bool net::socket::cork()
{
#if HAVE_TCP_CORK
	int optval = 1;
	if (setsockopt(_M_fd, IPPROTO_TCP, TCP_CORK, &optval, sizeof(int)) < 0) {
		return false;
	}
#elif HAVE_TCP_NOPUSH
	int optval = 1;
	if (setsockopt(_M_fd, IPPROTO_TCP, TCP_NOPUSH, &optval, sizeof(int)) < 0) {
		return false;
	}
#endif

	return true;
}

bool net::socket::uncork()
{
#if HAVE_TCP_CORK
	int optval = 0;
	if (setsockopt(_M_fd, IPPROTO_TCP, TCP_CORK, &optval, sizeof(int)) < 0) {
		return false;
	}
#elif HAVE_TCP_NOPUSH
	int optval = 0;
	if (setsockopt(_M_fd, IPPROTO_TCP, TCP_NOPUSH, &optval, sizeof(int)) < 0) {
		return false;
	}
#endif

	return true;
}

bool net::socket::wait_readable(int timeout)
{
	fd_set rfds;
	FD_ZERO(&rfds);
	FD_SET(_M_fd, &rfds);

	if (timeout < 0) {
		return (select(_M_fd + 1, &rfds, NULL, NULL, NULL) > 0);
	} else {
		struct timeval tv;
		tv.tv_sec = timeout / 1000;
		tv.tv_usec = (timeout % 1000) * 1000;

		return (select(_M_fd + 1, &rfds, NULL, NULL, &tv) > 0);
	}
}

bool net::socket::wait_writable(int timeout)
{
	fd_set wfds;
	FD_ZERO(&wfds);
	FD_SET(_M_fd, &wfds);

	if (timeout < 0) {
		return (select(_M_fd + 1, NULL, &wfds, NULL, NULL) > 0);
	} else {
		struct timeval tv;
		tv.tv_sec = timeout / 1000;
		tv.tv_usec = (timeout % 1000) * 1000;

		return (select(_M_fd + 1, NULL, &wfds, NULL, &tv) > 0);
	}
}

bool net::socket::create(int domain, int type)
{
	if ((_M_fd = ::socket(domain, type, 0)) < 0) {
		return false;
	}

	if (!set_non_blocking()) {
		close();
		return false;
	}

	return true;
}

bool net::socket::set_non_blocking()
{
#if USE_FIONBIO
	int value = 1;
	if (ioctl(_M_fd, FIONBIO, &value) < 0) {
		return false;
	}
#else
	int flags = fcntl(_M_fd, F_GETFL);
	flags |= O_NONBLOCK;

	if (fcntl(_M_fd, F_SETFL, flags) < 0) {
		return false;
	}
#endif

	return true;
}

bool net::socket::accept(socket& s, socket_address* addr, socklen_t* addrlen, int timeout)
{
	if (timeout != 0) {
		if (!wait_readable(timeout)) {
			return false;
		}
	}

	int fd;

#if HAVE_ACCEPT4
	if ((fd = accept4(_M_fd, reinterpret_cast<struct sockaddr*>(addr), addrlen, SOCK_NONBLOCK)) < 0) {
		return false;
	}

	s._M_fd = fd;
#else
	if ((fd = ::accept(_M_fd, reinterpret_cast<struct sockaddr*>(addr), addrlen)) < 0) {
		return false;
	}

	s._M_fd = fd;

	if (!s.set_non_blocking()) {
		s.close();
		return false;
	}
#endif

	return true;
}
