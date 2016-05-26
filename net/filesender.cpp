#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include "net/filesender.h"
#include "macros/macros.h"

#if HAVE_SENDFILE
	#if defined(__linux__)
		#include <sys/sendfile.h>

		off_t net::filesender::sendfile(socket& s, fs::file& f, off_t filesize, off_t& offset, off_t count, int timeout)
		{
			if (timeout == 0) {
				ssize_t ret;
				while (((ret = ::sendfile(s.fd(), f.fd(), &offset, count)) < 0) && (errno == EINTR));

				return ret;
			} else {
				off_t written = 0;

				do {
					if (!s.wait_writable(timeout)) {
						return -1;
					}

					ssize_t ret;
					if ((ret = ::sendfile(s.fd(), f.fd(), &offset, count - written)) < 0) {
						if ((errno != EINTR) && (errno != EAGAIN)) {
							return -1;
						}
					} else if (ret > 0) {
						if ((written += ret) == count) {
							return count;
						}
					}
				} while (true);
			}
		}
	#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
		#include <sys/uio.h>

		off_t net::filesender::sendfile(socket& s, fs::file& f, off_t filesize, off_t& offset, off_t count, int timeout)
		{
			off_t written = 0;
			off_t sbytes = 0;

			if (timeout == 0) {
				while (::sendfile(f.fd(), s.fd(), offset, count - written, NULL, &sbytes, 0) < 0) {
					if (errno == EINTR) {
						offset += sbytes;
						written += sbytes;

						sbytes = 0;
						continue;
					} else if (errno == EAGAIN) {
						break;
					} else {
						return -1;
					}
				}

				offset += sbytes;
				written += sbytes;

				return written;
			} else {
				do {
					if (!s.wait_writable(timeout)) {
						return -1;
					}

					if (::sendfile(f.fd(), s.fd(), offset, count - written, NULL, &sbytes, 0) < 0) {
						if ((errno == EINTR) || (errno == EAGAIN)) {
							offset += sbytes;
							written += sbytes;

							sbytes = 0;
							continue;
						} else {
							return -1;
						}
					} else {
						offset += sbytes;

						if ((written += sbytes) == count) {
							return count;
						}
					}
				} while (true);
			}
		}
	#elif defined(__sun__)
		#include <sys/sendfile.h>

		off_t net::filesender::sendfile(socket& s, fs::file& f, off_t filesize, off_t& offset, off_t count, int timeout)
		{
			struct sendfilevec vec;
			vec.sfv_fd = f.fd();
			vec.sfv_flag = 0;
			vec.sfv_off = offset;
			vec.sfv_len = count;

			if (timeout == 0) {
				size_t xferred = 0;
				ssize_t ret;
				while (((ret = sendfilev(s.fd(), &vec, 1, &xferred)) < 0) && (errno == EINTR));

				offset += xferred;

				return ret;
			} else {
				off_t written = 0;

				do {
					if (!s.wait_writable(timeout)) {
						return -1;
					}

					size_t xferred;
					ssize_t ret;
					if ((ret = sendfilev(s.fd(), &vec, 1, &xferred)) < 0) {
						if ((errno != EINTR) && (errno != EAGAIN)) {
							return -1;
						}
					} else if (ret > 0) {
						offset += xferred;

						if ((written += xferred) == count) {
							return count;
						}

						vec.sfv_off = offset;
						vec.sfv_len = count - written;
					}
				} while (true);
			}
		}
	#endif
#elif HAVE_MMAP
	#include <sys/mman.h>

	off_t net::filesender::sendfile(socket& s, fs::file& f, off_t filesize, off_t& offset, off_t count, int timeout)
	{
		void* data;
		if ((data = mmap(NULL, filesize, PROT_READ, MAP_SHARED, f.fd(), 0)) == MAP_FAILED) {
			return -1;
		}

		ssize_t ret = s.write((const char*) data + offset, count, timeout);

		// Save 'errno'.
		int error = errno;

		munmap(data, filesize);

		if (ret > 0) {
			offset += ret;
		}

		// Restore 'errno'.
		errno = error;

		return ret;
	}
#else
	off_t net::filesender::sendfile(socket& s, fs::file& f, off_t filesize, off_t& offset, off_t count, int timeout)
	{
	#if !HAVE_PREAD
		if (f.seek(offset, SEEK_SET) != offset) {
			return -1;
		}
	#endif // !HAVE_PREAD

		char buf[READ_BUFFER_SIZE];

		off_t written = 0;

		if (timeout == 0) {
			do {
				off_t bytes = MIN(sizeof(buf), count - written);

		#if HAVE_PREAD
				if ((bytes = f.pread(buf, bytes, offset)) <= 0) {
		#else
				if ((bytes = f.read(buf, bytes)) <= 0) {
		#endif
					return -1;
				}

				ssize_t ret;
				if ((ret = s.write(buf, bytes, 0)) < 0) {
					if ((errno == EAGAIN) && (written > 0)) {
						return written;
					}

					return -1;
				} else if (ret == 0) {
					return written;
				} else {
					offset += ret;

					if ((written += ret) == count) {
						return count;
					}

					if (ret < bytes) {
						return written;
					}
				}
			} while (true);
		} else {
			do {
				off_t bytes = MIN(sizeof(buf), count - written);

		#if HAVE_PREAD
				if ((bytes = f.pread(buf, bytes, offset)) <= 0) {
		#else
				if ((bytes = f.read(buf, bytes)) <= 0) {
		#endif
					return -1;
				}

				ssize_t ret;
				if ((ret = s.write(buf, bytes, timeout)) < 0) {
					return -1;
				} else {
					offset += ret;

					if ((written += ret) == count) {
						return count;
					}
				}
			} while (true);
		}
	}
#endif

#if HAVE_SSL
	#if HAVE_MMAP
		#include <sys/mman.h>

		off_t net::filesender::sendfile(ssl_socket& s, fs::file& f, off_t filesize, off_t& offset, off_t count, bool& want_read, bool& want_write)
		{
			off_t written = 0;

			// If there is still data in the buffer...
			if (!_M_output.empty()) {
				ssize_t ret;
				if ((ret = s.write(_M_output.data(), _M_output.length(), want_read, want_write)) < 0) {
					return -1;
				}

				offset += ret;
				written = ret;

				_M_output.clear();

				if (written == count) {
					return count;
				}
			}

			void* data;
			if ((data = mmap(NULL, filesize, PROT_READ, MAP_SHARED, f.fd(), 0)) == MAP_FAILED) {
				want_read = false;
				want_write = false;

				return -1;
			}

			do {
				off_t bytes = MIN((off_t) READ_BUFFER_SIZE, count - written);
				if (!_M_output.append((const char*) data + offset, bytes)) {
					munmap(data, filesize);

					want_read = false;
					want_write = false;

					return -1;
				}

				ssize_t ret;
				if ((ret = s.write(_M_output.data(), bytes, want_read, want_write)) < 0) {
					munmap(data, filesize);

					if ((!want_read) && (!want_write)) {
						return -1;
					} else {
						return (written == 0) ? -1 : written;
					}
				} else {
					offset += ret;
					written += ret;

					_M_output.clear();
				}
			} while (written < count);

			munmap(data, filesize);

			return count;
		}

		off_t net::filesender::sendfile(ssl_socket& s, fs::file& f, off_t filesize, off_t& offset, off_t count, int timeout)
		{
			off_t written = 0;

			// If there is still data in the buffer...
			if (!_M_output.empty()) {
				ssize_t ret;
				if ((ret = s.write(_M_output.data(), _M_output.length(), timeout)) < 0) {
					return -1;
				}

				offset += ret;
				written = ret;

				_M_output.clear();

				if (written == count) {
					return count;
				}
			}

			void* data;
			if ((data = mmap(NULL, filesize, PROT_READ, MAP_SHARED, f.fd(), 0)) == MAP_FAILED) {
				return -1;
			}

			do {
				off_t bytes = MIN((off_t) READ_BUFFER_SIZE, count - written);
				if (!_M_output.append((const char*) data + offset, bytes)) {
					munmap(data, filesize);
					return -1;
				}

				ssize_t ret;
				if ((ret = s.write(_M_output.data(), bytes, timeout)) < 0) {
					munmap(data, filesize);
					return -1;
				} else {
					offset += ret;
					written += ret;

					_M_output.clear();
				}
			} while (written < count);

			munmap(data, filesize);

			return count;
		}
	#else
		off_t net::filesender::sendfile(ssl_socket& s, fs::file& f, off_t filesize, off_t& offset, off_t count, bool& want_read, bool& want_write)
		{
			off_t written = 0;

			// If there is still data in the buffer...
			if (!_M_output.empty()) {
				ssize_t ret;
				if ((ret = s.write(_M_output.data(), _M_output.length(), want_read, want_write)) < 0) {
					return -1;
				}

				offset += ret;
				written = ret;

				_M_output.clear();

				if (written == count) {
					return count;
				}
			}

		#if !HAVE_PREAD
			if (f.seek(offset, SEEK_SET) != offset) {
				want_read = false;
				want_write = false;

				return -1;
			}
		#endif // !HAVE_PREAD

			off_t bytes = MIN(READ_BUFFER_SIZE, count - written);
			if (!_M_output.allocate(bytes)) {
				want_read = false;
				want_write = false;

				return -1;
			}

			do {
		#if HAVE_PREAD
				if ((bytes = f.pread(_M_output.data(), bytes, offset)) <= 0) {
		#else
				if ((bytes = f.read(_M_output.data(), bytes)) <= 0) {
		#endif
					want_read = false;
					want_write = false;

					return -1;
				}

				ssize_t ret;
				if ((ret = s.write(_M_output.data(), bytes, want_read, want_write)) < 0) {
					if ((!want_read) && (!want_write)) {
						return -1;
					} else {
						_M_output.length(bytes);

						return (written == 0) ? -1 : written;
					}
				} else {
					offset += ret;

					if ((written += ret) == count) {
						return count;
					}

					bytes = MIN(READ_BUFFER_SIZE, count - written);
				}
			} while (true);
		}

		off_t net::filesender::sendfile(ssl_socket& s, fs::file& f, off_t filesize, off_t& offset, off_t count, int timeout)
		{
			off_t written = 0;

			// If there is still data in the buffer...
			if (!_M_output.empty()) {
				ssize_t ret;
				if ((ret = s.write(_M_output.data(), _M_output.length(), timeout)) < 0) {
					return -1;
				}

				offset += ret;
				written = ret;

				_M_output.clear();

				if (written == count) {
					return count;
				}
			}

		#if !HAVE_PREAD
			if (f.seek(offset, SEEK_SET) != offset) {
				return -1;
			}
		#endif // !HAVE_PREAD

			off_t bytes = MIN(READ_BUFFER_SIZE, count - written);
			if (!_M_output.allocate(bytes)) {
				return -1;
			}

			do {
		#if HAVE_PREAD
				if ((bytes = f.pread(_M_output.data(), bytes, offset)) <= 0) {
		#else
				if ((bytes = f.read(_M_output.data(), bytes)) <= 0) {
		#endif
					return -1;
				}

				ssize_t ret;
				if ((ret = s.write(_M_output.data(), bytes, timeout)) < 0) {
					return -1;
				} else {
					offset += ret;

					if ((written += ret) == count) {
						return count;
					}

					bytes = MIN(READ_BUFFER_SIZE, count - written);
				}
			} while (true);
		}
	#endif
#endif // HAVE_SSL
