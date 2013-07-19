#ifndef SSL_SOCKET_H
#define SSL_SOCKET_H

#include <stdlib.h>
#include <openssl/ssl.h>
#include "net/socket.h"
#include "string/buffer.h"

namespace net {
	class ssl_socket : public socket {
		public:
			// Initialize SSL library.
			static bool init_ssl_library();

			// Free SSL library.
			static void free_ssl_library();

			// Load certificate.
			static bool load_certificate(const char* certificate, const char* key);

			// Constructor.
			ssl_socket();
			ssl_socket(int fd);
			ssl_socket(const socket& s);

			// Close socket.
			bool close();

			// Free.
			void free();

			// Perform TLS/SSL handshake.
			enum ssl_mode {CLIENT_MODE, SERVER_MODE};
			bool handshake(ssl_mode mode, bool& want_read, bool& want_write);
			bool handshake(ssl_mode mode, int timeout = -1);

			// Handshake performed?
			bool handshaked() const;

			// Shutdown TLS/SSL connection.
			bool shutdown(bool bidirectional, bool& want_read, bool& want_write);
			bool shutdown(bool bidirectional, int timeout = -1);

			// Read.
			ssize_t read(void* buf, size_t count, bool& want_read, bool& want_write);
			ssize_t read(void* buf, size_t count, int timeout = -1);

			// Write.
			ssize_t write(const void* buf, size_t count, bool& want_read, bool& want_write);
			ssize_t write(const void* buf, size_t count, int timeout = -1);

			// Write from multiple buffers.
			ssize_t writev(const struct iovec* iov, unsigned iovcnt, bool& want_read, bool& want_write);
			ssize_t writev(const struct iovec* iov, unsigned iovcnt, int timeout = -1);

		protected:
			static SSL_CTX* _M_ctx;

			SSL* _M_ssl;

			// Perform TLS/SSL handshake.
			bool ssl_handshake(bool& want_read, bool& want_write);

			// Initialize SSL structure.
			bool init_ssl_struct(ssl_mode mode);

		private:
			static const size_t GATHER_OUTPUT_MAX_SIZE = 2 * 1024;

			string::buffer _M_gather_output;
	};

	inline ssl_socket::ssl_socket()
	{
		_M_ssl = NULL;
	}

	inline ssl_socket::ssl_socket(int fd)
	{
		_M_fd = fd;
		_M_ssl = NULL;
	}

	inline ssl_socket::ssl_socket(const socket& s)
	{
		_M_fd = s.fd();
		_M_ssl = NULL;
	}

	inline bool ssl_socket::close()
	{
		free();
		return socket::close();
	}

	inline void ssl_socket::free()
	{
		if (_M_ssl) {
			SSL_free(_M_ssl);
			_M_ssl = NULL;
		}

		if (_M_gather_output.size() > GATHER_OUTPUT_MAX_SIZE) {
			_M_gather_output.free();
		} else {
			_M_gather_output.reset();
		}
	}

	inline bool ssl_socket::handshaked() const
	{
		return (_M_ssl != NULL);
	}
}

#endif // SSL_SOCKET_H
