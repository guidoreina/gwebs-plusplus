#include <sys/socket.h>
#include <openssl/engine.h>
#include <openssl/objects.h>
#include <openssl/conf.h>
#include <openssl/err.h>
#include <limits.h>
#include <errno.h>
#include "net/ssl_socket.h"

SSL_CTX* net::ssl_socket::_M_ctx = NULL;

bool net::ssl_socket::init_ssl_library()
{
	SSL_load_error_strings();
	SSL_library_init();

	// Create SSL context.
	if ((_M_ctx = SSL_CTX_new(SSLv23_method())) == NULL) {
		ERR_clear_error();

		free_ssl_library();
		return false;
	}

	return true;
}

void net::ssl_socket::free_ssl_library()
{
	if (_M_ctx) {
		SSL_CTX_free(_M_ctx);
		_M_ctx = NULL;
	}

	CONF_modules_unload(1);
	OBJ_cleanup();
	EVP_cleanup();
	ENGINE_cleanup();
	CRYPTO_cleanup_all_ex_data();

#if OPENSSL_VERSION_NUMBER >= 0x1000103fL
	ERR_remove_thread_state(NULL);
#else
	ERR_remove_state(0);
#endif

	ERR_free_strings();
}

bool net::ssl_socket::load_certificate(const char* certificate, const char* key)
{
	if (SSL_CTX_use_certificate_chain_file(_M_ctx, certificate) != 1) {
		ERR_clear_error();
		return false;
	}

	if (SSL_CTX_use_PrivateKey_file(_M_ctx, key, SSL_FILETYPE_PEM) != 1) {
		ERR_clear_error();
		return false;
	}

	return true;
}

bool net::ssl_socket::handshake(ssl_mode mode, bool& want_read, bool& want_write)
{
	if (!_M_ssl) {
		if (!init_ssl_struct(mode)) {
			return false;
		}
	}

	return ssl_handshake(want_read, want_write);
}

bool net::ssl_socket::handshake(ssl_mode mode, int timeout)
{
	if (!init_ssl_struct(mode)) {
		return false;
	}

	do {
		bool want_read, want_write;
		if (!ssl_handshake(want_read, want_write)) {
			return false;
		} else {
			if (want_read) {
				if (!wait_readable(timeout)) {
					return false;
				}
			} else if (want_write) {
				if (!wait_writable(timeout)) {
					return false;
				}
			} else {
				return true;
			}
		}
	} while (true);
}

bool net::ssl_socket::shutdown(bool bidirectional, bool& want_read, bool& want_write)
{
	if (!bidirectional) {
		SSL_set_shutdown(_M_ssl, SSL_get_shutdown(_M_ssl) | SSL_RECEIVED_SHUTDOWN);
	}

	do {
		// Clear the error queue.
		ERR_clear_error();

		int ret;
		if ((ret = SSL_shutdown(_M_ssl)) == 1) {
			want_read = false;
			want_write = false;

			SSL_free(_M_ssl);
			_M_ssl = NULL;

			return true;
		}

		int err;
		switch ((err = SSL_get_error(_M_ssl, ret))) {
			case SSL_ERROR_WANT_READ:
			case SSL_ERROR_WANT_WRITE:
				want_read = (err == SSL_ERROR_WANT_READ);
				want_write = !want_read;

				return true;
			case SSL_ERROR_SYSCALL:
				if ((ret < 0) && (errno == EINTR)) {
					continue;
				}

				// Fall through.
			case SSL_ERROR_ZERO_RETURN:
				// The TLS/SSL connection has been closed.
			default:
				want_read = false;
				want_write = false;

				SSL_free(_M_ssl);
				_M_ssl = NULL;

				return false;
		}
	} while (true);
}

bool net::ssl_socket::shutdown(bool bidirectional, int timeout)
{
	do {
		bool want_read, want_write;
		if (!shutdown(bidirectional, want_read, want_write)) {
			return false;
		} else {
			if (want_read) {
				if (!wait_readable(timeout)) {
					return false;
				}
			} else if (want_write) {
				if (!wait_writable(timeout)) {
					return false;
				}
			} else {
				return true;
			}
		}
	} while (true);
}

ssize_t net::ssl_socket::read(void* buf, size_t count, bool& want_read, bool& want_write)
{
	char* b = (char*) buf;
	size_t total = 0;

	do {
		// Clear the error queue.
		ERR_clear_error();

		int ret;
		if ((ret = SSL_read(_M_ssl, b, count - total)) > 0) {
			if ((total += ret) == count) {
				want_read = false;
				want_write = false;

				return count;
			}

			b += ret;

			continue;
		}

		int err;
		switch ((err = SSL_get_error(_M_ssl, ret))) {
			case SSL_ERROR_WANT_READ:
			case SSL_ERROR_WANT_WRITE:
				want_read = (err == SSL_ERROR_WANT_READ);
				want_write = !want_read;

				if (total == 0) {
					return -1;
				} else {
					return total;
				}

				break;
			case SSL_ERROR_SYSCALL:
				if (ret < 0) {
					if (errno == EINTR) {
						continue;
					}
				} else {
					if (total > 0) {
						want_read = false;
						want_write = false;

						return total;
					}
				}

				// Fall through.
			case SSL_ERROR_ZERO_RETURN:
				// The TLS/SSL connection has been closed.
			default:
				want_read = false;
				want_write = false;

				return -1;
		}
	} while (true);
}

ssize_t net::ssl_socket::read(void* buf, size_t count, int timeout)
{
	do {
		bool want_read, want_write;
		ssize_t ret;
		if ((ret = read(buf, count, want_read, want_write)) > 0) {
			return ret;
		} else {
			if (want_read) {
				if (!wait_readable(timeout)) {
					return -1;
				}
			} else if (want_write) {
				if (!wait_writable(timeout)) {
					return -1;
				}
			} else {
				return -1;
			}
		}
	} while (true);
}

ssize_t net::ssl_socket::write(const void* buf, size_t count, bool& want_read, bool& want_write)
{
	do {
		// Clear the error queue.
		ERR_clear_error();

		int ret;
		if ((ret = SSL_write(_M_ssl, buf, count)) > 0) {
			want_read = false;
			want_write = false;

			return ret;
		}

		int err;
		switch ((err = SSL_get_error(_M_ssl, ret))) {
			case SSL_ERROR_WANT_READ:
			case SSL_ERROR_WANT_WRITE:
				want_read = (err == SSL_ERROR_WANT_READ);
				want_write = !want_read;

				return -1;
			case SSL_ERROR_SYSCALL:
				if ((ret < 0) && (errno == EINTR)) {
					continue;
				}

				// Fall through.
			case SSL_ERROR_ZERO_RETURN:
				// The TLS/SSL connection has been closed.
			default:
				want_read = false;
				want_write = false;

				return -1;
		}
	} while (true);
}

ssize_t net::ssl_socket::write(const void* buf, size_t count, int timeout)
{
	do {
		bool want_read, want_write;
		ssize_t ret;
		if ((ret = write(buf, count, want_read, want_write)) > 0) {
			return ret;
		} else {
			if (want_read) {
				if (!wait_readable(timeout)) {
					return -1;
				}
			} else if (want_write) {
				if (!wait_writable(timeout)) {
					return -1;
				}
			} else {
				return -1;
			}
		}
	} while (true);
}

ssize_t net::ssl_socket::writev(const struct iovec* iov, unsigned iovcnt, bool& want_read, bool& want_write)
{
	if (_M_gather_output.empty()) {
		// Too many buffers?
		if (iovcnt > IOV_MAX) {
			want_read = false;
			want_write = false;

			return -1;
		}

		for (unsigned i = 0; i < iovcnt; i++) {
			if (!_M_gather_output.append((const char*) iov->iov_base, iov->iov_len)) {
				want_read = false;
				want_write = false;

				return -1;
			}

			iov++;
		}
	}

	ssize_t ret;
	if ((ret = write(_M_gather_output.data(), _M_gather_output.length(), want_read, want_write)) < 0) {
		return -1;
	} else {
		_M_gather_output.clear();

		return ret;
	}
}

ssize_t net::ssl_socket::writev(const struct iovec* iov, unsigned iovcnt, int timeout)
{
	if (_M_gather_output.empty()) {
		// Too many buffers?
		if (iovcnt > IOV_MAX) {
			return -1;
		}

		for (unsigned i = 0; i < iovcnt; i++) {
			if (!_M_gather_output.append((const char*) iov->iov_base, iov->iov_len)) {
				return -1;
			}

			iov++;
		}
	}

	ssize_t ret;
	if ((ret = write(_M_gather_output.data(), _M_gather_output.length(), timeout)) < 0) {
		return -1;
	} else {
		_M_gather_output.clear();

		return ret;
	}
}

bool net::ssl_socket::ssl_handshake(bool& want_read, bool& want_write)
{
	do {
		// Clear the error queue.
		ERR_clear_error();

		int ret;
		if ((ret = SSL_do_handshake(_M_ssl)) == 1) {
			want_read = false;
			want_write = false;

			return true;
		}

		int err;
		switch ((err = SSL_get_error(_M_ssl, ret))) {
			case SSL_ERROR_WANT_READ:
			case SSL_ERROR_WANT_WRITE:
				want_read = (err == SSL_ERROR_WANT_READ);
				want_write = !want_read;

				return true;
			case SSL_ERROR_SYSCALL:
				if ((ret < 0) && (errno == EINTR)) {
					continue;
				}

				// Fall through.
			case SSL_ERROR_ZERO_RETURN:
				// The TLS/SSL connection has been closed.
			default:
				return false;
		}
	} while (true);
}

bool net::ssl_socket::init_ssl_struct(ssl_mode mode)
{
	if ((_M_ssl = SSL_new(_M_ctx)) == NULL) {
		ERR_clear_error();
		return false;
	}

	if (!SSL_set_fd(_M_ssl, _M_fd)) {
		ERR_clear_error();
		return false;
	}

	if (mode == CLIENT_MODE) {
		SSL_set_connect_state(_M_ssl);
	} else {
		SSL_set_accept_state(_M_ssl);
	}

	return true;
}
