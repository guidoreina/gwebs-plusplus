#ifndef TCP_CONNECTION_H
#define TCP_CONNECTION_H

#include <sys/types.h>
#include "io/event_handler.h"
#include "timer/timer.h"
#include "net/socket.h"

#if HAVE_SSL
	#include "net/ssl_socket.h"
#endif // HAVE_SSL

#include "net/filesender.h"
#include "net/listener.h"
#include "string/buffer.h"
#include "fs/file.h"
#include "util/red_black_tree.h"
#include "util/ranges.h"

namespace net {
	class tcp_server;

	struct tcp_connection : public io::event_handler, public timer::event_handler {
		public:
			static const size_t READ_BUFFER_SIZE = 2 * 1024;
			static const unsigned MAX_IDLE_TIME = 30; // [seconds]

			string::buffer _M_in;
			string::buffer _M_out;

			off_t _M_inp;
			off_t _M_outp;

			listener* _M_listener;

			// Timer.
			util::red_black_tree<timer::timer>::iterator _M_timer;
			unsigned _M_timer_set:1;

			unsigned _M_state:5;

			unsigned _M_readable:1;
			unsigned _M_writable:1;

			static unsigned _M_max_idle_time;

			static tcp_server* _M_server;

			// Constructor.
			tcp_connection();

			// Destructor.
			virtual ~tcp_connection();

			// Free.
			virtual void free();

			// Reset.
			virtual void reset();

			// On readable.
			bool on_readable();

			// On writable.
			bool on_writable();

			// On timer.
			virtual bool on_timer(unsigned id) = 0;

			// Modify descriptor.
			bool modify(unsigned events);

#if HAVE_SSL
			// Perform TLS/SSL handshake.
			bool handshake(ssl_socket::ssl_mode mode, bool& completed);
#endif // HAVE_SSL

			// Read.
			bool read(string::buffer& buf, size_t& count);
			bool read(size_t& count);

			// Write.
			bool write(const string::buffer& buf);
			bool write();

			// Write from multiple buffers.
			bool writev(const string::buffer** bufs, unsigned count);

			// Send file.
			bool sendfile(fs::file& f, off_t filesize, const util::range* range);
			bool sendfile(fs::file& f, off_t filesize);

			// Run.
			virtual bool run() = 0;

			// Get socket descriptor.
			int fd() const;

			// Set socket descriptor.
			void fd(int descriptor);

		protected:
			socket _M_socket;

#if HAVE_SSL
			ssl_socket _M_ssl_socket;
			filesender _M_filesender;
#endif // HAVE_SSL

			// Read.
			bool unsecure_read(string::buffer& buf, size_t& count);

			// Write.
			bool unsecure_write(const string::buffer& buf);

			// Write from multiple buffers.
			bool unsecure_writev(const string::buffer** bufs, unsigned count);

			// Send file.
			bool unsecure_sendfile(fs::file& f, off_t filesize, const util::range* range);

#if HAVE_SSL
			// Read.
			bool secure_read(string::buffer& buf, size_t& count);

			// Write.
			bool secure_write(const string::buffer& buf);

			// Write from multiple buffers.
			bool secure_writev(const string::buffer** bufs, unsigned count);

			// Send file.
			bool secure_sendfile(fs::file& f, off_t filesize, const util::range* range);
#endif // HAVE_SSL

			// Add timer.
			bool add_timer();

			// Delete timer.
			void delete_timer();
	};

	inline tcp_connection::~tcp_connection()
	{
#if HAVE_SSL
		if (_M_ssl_socket.handshaked()) {
			_M_ssl_socket.free();
		}
#endif // HAVE_SSL
	}

	inline void tcp_connection::free()
	{
#if HAVE_SSL
		if (_M_ssl_socket.handshaked()) {
			_M_ssl_socket.free();
		}
#endif // HAVE_SSL

		tcp_connection::reset();

		_M_in.reset();
		_M_inp = 0;

		delete_timer();

		_M_readable = 0;
		_M_writable = 0;
	}

	inline bool net::tcp_connection::on_readable()
	{
		_M_readable = 1;

		if (!run()) {
			free();
			return false;
		}

		return true;
	}

	inline bool net::tcp_connection::on_writable()
	{
		_M_writable = 1;

		if (!run()) {
			free();
			return false;
		}

		return true;
	}

	inline bool tcp_connection::read(string::buffer& buf, size_t& count)
	{
#if !HAVE_SSL
		return unsecure_read(buf, count);
#else
		if (!_M_ssl_socket.handshaked()) {
			return unsecure_read(buf, count);
		} else {
			return secure_read(buf, count);
		}
#endif // HAVE_SSL
	}

	inline bool tcp_connection::read(size_t& count)
	{
#if !HAVE_SSL
		return unsecure_read(_M_in, count);
#else
		if (!_M_ssl_socket.handshaked()) {
			return unsecure_read(_M_in, count);
		} else {
			return secure_read(_M_in, count);
		}
#endif // HAVE_SSL
	}

	inline bool tcp_connection::write(const string::buffer& buf)
	{
#if !HAVE_SSL
		return unsecure_write(buf);
#else
		if (!_M_ssl_socket.handshaked()) {
			return unsecure_write(buf);
		} else {
			return secure_write(buf);
		}
#endif // HAVE_SSL
	}

	inline bool tcp_connection::write()
	{
#if !HAVE_SSL
		return unsecure_write(_M_out);
#else
		if (!_M_ssl_socket.handshaked()) {
			return unsecure_write(_M_out);
		} else {
			return secure_write(_M_out);
		}
#endif // HAVE_SSL
	}

	inline bool tcp_connection::writev(const string::buffer** bufs, unsigned count)
	{
#if !HAVE_SSL
		return unsecure_writev(bufs, count);
#else
		if (!_M_ssl_socket.handshaked()) {
			return unsecure_writev(bufs, count);
		} else {
			return secure_writev(bufs, count);
		}
#endif // HAVE_SSL
	}

	inline bool tcp_connection::sendfile(fs::file& f, off_t filesize, const util::range* range)
	{
#if !HAVE_SSL
		return unsecure_sendfile(f, filesize, range);
#else
		if (!_M_ssl_socket.handshaked()) {
			return unsecure_sendfile(f, filesize, range);
		} else {
			return secure_sendfile(f, filesize, range);
		}
#endif // HAVE_SSL
	}

	inline bool tcp_connection::sendfile(fs::file& f, off_t filesize)
	{
#if !HAVE_SSL
		return unsecure_sendfile(f, filesize, NULL);
#else
		if (!_M_ssl_socket.handshaked()) {
			return unsecure_sendfile(f, filesize, NULL);
		} else {
			return secure_sendfile(f, filesize, NULL);
		}
#endif // HAVE_SSL
	}

	inline int tcp_connection::fd() const
	{
		return _M_socket.fd();
	}

	inline void tcp_connection::fd(int descriptor)
	{
		_M_socket.fd(descriptor);

#if HAVE_SSL
		_M_ssl_socket.fd(descriptor);
#endif // HAVE_SSL
	}
}

#endif // TCP_CONNECTION_H
