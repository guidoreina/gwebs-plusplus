#include <stdlib.h>
#include <sys/socket.h>
#include <limits.h>
#include <errno.h>
#include "net/tcp_connection.h"
#include "net/tcp_connection.inl"
#include "net/tcp_server.h"

unsigned net::tcp_connection::_M_max_idle_time = MAX_IDLE_TIME;
net::tcp_server* net::tcp_connection::_M_server = NULL;

net::tcp_connection::tcp_connection() : _M_in(READ_BUFFER_SIZE)
{
	_M_inp = 0;
	_M_outp = 0;

	_M_timer_set = 0;

	_M_state = 0;

	_M_readable = 0;
	_M_writable = 0;
}

void net::tcp_connection::reset()
{
	_M_out.reset();
	_M_outp = 0;

	_M_state = 0;

#if HAVE_SSL
	_M_filesender.reset();
#endif
}

bool net::tcp_connection::unsecure_read(string::buffer& buf, size_t& count)
{
	// Allocate memory.
	if (!buf.allocate(READ_BUFFER_SIZE)) {
		return false;
	}

	// Receive.
	ssize_t ret;
	if ((ret = _M_socket.read(buf.data() + buf.count(), READ_BUFFER_SIZE, 0)) < 0) {
		if (errno == EAGAIN) {
			if (!add_timer()) {
				return false;
			}

			count = 0;
			_M_readable = 0;
		} else {
			return false;
		}
	} else if (ret == 0) {
		// The peer has performed an orderly shutdown.
		return false;
	} else {
		buf.increment_count(ret);
		count = ret;

		if ((size_t) ret < count) {
			if (!add_timer()) {
				return false;
			}

			_M_readable = 0;
		} else {
			delete_timer();
		}
	}

	return true;
}

bool net::tcp_connection::unsecure_write(const string::buffer& buf)
{
	// Send.
	size_t count = buf.count() - _M_outp;
	ssize_t ret;
	if ((ret = _M_socket.write(buf.data() + _M_outp, count, 0)) < 0) {
		if (errno == EAGAIN) {
			if (!add_timer()) {
				return false;
			}

			_M_writable = 0;
		} else {
			return false;
		}
	} else {
		_M_outp += ret;

		if ((size_t) ret < count) {
			if (!add_timer()) {
				return false;
			}

			_M_writable = 0;
		} else {
			delete_timer();
		}
	}

	return true;
}

bool net::tcp_connection::unsecure_writev(const string::buffer** bufs, unsigned count)
{
	// Too many buffers?
	if (count > IOV_MAX) {
		return false;
	}

	off_t outp = _M_outp;
	while ((count > 0) && (outp >= (*bufs)->count())) {
		outp -= (*bufs)->count();

		bufs++;
		count--;
	}

	struct iovec vec[IOV_MAX];
	size_t total = 0;

	for (unsigned i = 0; i < count; i++) {
		vec[i].iov_base = (char*) (*bufs)->data() + outp;
		vec[i].iov_len = (*bufs)->count() - outp;

		total += vec[i].iov_len;

		outp = 0;

		bufs++;
	}

	// Send.
	ssize_t ret;
	if ((ret = _M_socket.writev(vec, count, 0)) < 0) {
		if (errno == EAGAIN) {
			if (!add_timer()) {
				return false;
			}

			_M_writable = 0;
		} else {
			return false;
		}
	} else {
		_M_outp += ret;

		if ((size_t) ret < total) {
			if (!add_timer()) {
				return false;
			}

			_M_writable = 0;
		} else {
			delete_timer();
		}
	}

	return true;
}

bool net::tcp_connection::unsecure_sendfile(fs::file& f, off_t filesize, const util::range* range)
{
	// Get the number of bytes to send.
	off_t count;

	if (!range) {
		count = filesize - _M_outp;
	} else {
		count = range->to - _M_outp + 1;
	}

	// Send file.
	off_t ret;
	if ((ret = filesender::sendfile(_M_socket, f, filesize, _M_outp, count, 0)) < 0) {
		if (errno == EAGAIN) {
			if (!add_timer()) {
				return false;
			}

			_M_writable = 0;
		} else {
			return false;
		}
	} else {
		if (ret < count) {
			if (!add_timer()) {
				return false;
			}

			_M_writable = 0;
		} else {
			delete_timer();
		}
	}

	return true;
}

#if HAVE_SSL
	bool net::tcp_connection::handshake(ssl_socket::ssl_mode mode, bool& completed)
	{
		// Handshake.
		bool want_read;
		bool want_write;
		if (!_M_ssl_socket.handshake(mode, want_read, want_write)) {
			return false;
		}

		if (want_read) {
			if (!add_timer()) {
				return false;
			}

			_M_readable = 0;

			completed = false;

			return modify(iselector::READ);
		} else if (want_write) {
			if (!add_timer()) {
				return false;
			}

			_M_writable = 0;

			completed = false;

			return modify(iselector::WRITE);
		}

		completed = true;

		delete_timer();

		return true;
	}

	bool net::tcp_connection::secure_read(string::buffer& buf, size_t& count)
	{
		// Allocate memory.
		if (!buf.allocate(READ_BUFFER_SIZE)) {
			return false;
		}

		// Receive.
		bool want_read;
		bool want_write;
		ssize_t ret;
		if ((ret = _M_ssl_socket.read(buf.data() + buf.count(), READ_BUFFER_SIZE, want_read, want_write)) < 0) {
			if (want_read) {
				if (!add_timer()) {
					return false;
				}

				_M_readable = 0;

				count = 0;

				return modify(iselector::READ);
			} else if (want_write) {
				if (!add_timer()) {
					return false;
				}

				_M_writable = 0;

				count = 0;

				return modify(iselector::WRITE);
			} else {
				return false;
			}
		} else {
			buf.increment_count(ret);
			count = ret;

			if (want_read) {
				if (!add_timer()) {
					return false;
				}

				_M_readable = 0;
				return modify(iselector::READ);
			} else if (want_write) {
				if (!add_timer()) {
					return false;
				}

				_M_writable = 0;
				return modify(iselector::WRITE);
			}

			delete_timer();

			return true;
		}
	}

	bool net::tcp_connection::secure_write(const string::buffer& buf)
	{
		// Send.
		bool want_read;
		bool want_write;
		size_t count = buf.count() - _M_outp;
		ssize_t ret;
		if ((ret = _M_ssl_socket.write(buf.data() + _M_outp, count, want_read, want_write)) < 0) {
			if (want_read) {
				if (!add_timer()) {
					return false;
				}

				_M_readable = 0;
				return modify(iselector::READ);
			} else if (want_write) {
				if (!add_timer()) {
					return false;
				}

				_M_writable = 0;
				return modify(iselector::WRITE);
			} else {
				return false;
			}
		} else {
			_M_outp += ret;

			delete_timer();

			return true;
		}
	}

	bool net::tcp_connection::secure_writev(const string::buffer** bufs, unsigned count)
	{
		// Too many buffers?
		if (count > IOV_MAX) {
			return false;
		}

		off_t outp = _M_outp;
		while ((count > 0) && (outp >= (*bufs)->count())) {
			outp -= (*bufs)->count();

			bufs++;
			count--;
		}

		struct iovec vec[IOV_MAX];

		for (unsigned i = 0; i < count; i++) {
			vec[i].iov_base = (char*) (*bufs)->data() + outp;
			vec[i].iov_len = (*bufs)->count() - outp;

			outp = 0;

			bufs++;
		}

		// Send.
		bool want_read;
		bool want_write;
		ssize_t ret;
		if ((ret = _M_ssl_socket.writev(vec, count, want_read, want_write)) < 0) {
			if (want_read) {
				if (!add_timer()) {
					return false;
				}

				_M_readable = 0;
				return modify(iselector::READ);
			} else if (want_write) {
				if (!add_timer()) {
					return false;
				}

				_M_writable = 0;
				return modify(iselector::WRITE);
			} else {
				return false;
			}
		} else {
			_M_outp += ret;

			delete_timer();

			return true;
		}
	}

	bool net::tcp_connection::secure_sendfile(fs::file& f, off_t filesize, const util::range* range)
	{
		// Get the number of bytes to send.
		off_t count;

		if (!range) {
			count = filesize - _M_outp;
		} else {
			count = range->to - _M_outp + 1;
		}

		// Send file.
		bool want_read;
		bool want_write;
		off_t ret;
		if ((ret = _M_filesender.sendfile(_M_ssl_socket, f, filesize, _M_outp, count, want_read, want_write)) < 0) {
			if (want_read) {
				if (!add_timer()) {
					return false;
				}

				_M_readable = 0;
				return modify(iselector::READ);
			} else if (want_write) {
				if (!add_timer()) {
					return false;
				}

				_M_writable = 0;
				return modify(iselector::WRITE);
			} else {
				return false;
			}
		} else {
			if (want_read) {
				if (!add_timer()) {
					return false;
				}

				_M_readable = 0;
				return modify(iselector::READ);
			} else if (want_write) {
				if (!add_timer()) {
					return false;
				}

				_M_writable = 0;
				return modify(iselector::WRITE);
			}

			delete_timer();

			return true;
		}
	}
#endif // HAVE_SSL

bool net::tcp_connection::add_timer()
{
	delete_timer();

	if (!_M_server->timer::timers::add(_M_server->current_msec() + (_M_max_idle_time * 1000), this, 0, _M_timer)) {
		return false;
	}

	_M_timer_set = 1;

	return true;
}

void net::tcp_connection::delete_timer()
{
	if (_M_timer_set) {
		_M_server->del(_M_timer);
		_M_timer_set = 0;
	}
}
