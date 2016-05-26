#ifndef FILESENDER_H
#define FILESENDER_H

#include <sys/types.h>
#include "net/socket.h"

#if HAVE_SSL
	#include "net/ssl_socket.h"
	#include "string/buffer.h"
#endif // HAVE_SSL

#include "fs/file.h"

namespace net {
	class filesender {
		public:
			static off_t sendfile(socket& s, fs::file& f, off_t filesize, off_t& offset, off_t count, int timeout = -1);

#if HAVE_SSL
			// Reset.
			void reset();

			off_t sendfile(ssl_socket& s, fs::file& f, off_t filesize, off_t& offset, off_t count, bool& want_read, bool& want_write);
			off_t sendfile(ssl_socket& s, fs::file& f, off_t filesize, off_t& offset, off_t count, int timeout = -1);
#endif // HAVE_SSL

		private:
			static const size_t READ_BUFFER_SIZE = 8 * 1024;

#if HAVE_SSL
			string::buffer _M_output;
#endif // HAVE_SSL
	};

#if HAVE_SSL
	inline void filesender::reset()
	{
		_M_output.clear();
	}
#endif // HAVE_SSL
}

#endif // FILESENDER_H
