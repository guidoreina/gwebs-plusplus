#ifndef ERROR_H
#define ERROR_H

#include "net/internet/http/headers.h"
#include "string/buffer.h"

namespace net {
	namespace internet {
		namespace http {
			struct connection;

			class error {
				public:
					static const unsigned short OK = 200;
					static const unsigned short MOVED_PERMANENTLY = 301;
					static const unsigned short NOT_MODIFIED = 304;
					static const unsigned short BAD_REQUEST = 400;
					static const unsigned short FORBIDDEN = 403;
					static const unsigned short NOT_FOUND = 404;
					static const unsigned short LENGTH_REQUIRED = 411;
					static const unsigned short REQUEST_ENTITY_TOO_LARGE = 413;
					static const unsigned short REQUEST_URI_TOO_LONG = 414;
					static const unsigned short REQUESTED_RANGE_NOT_SATISFIABLE = 416;
					static const unsigned short INTERNAL_SERVER_ERROR = 500;
					static const unsigned short NOT_IMPLEMENTED = 501;
					static const unsigned short BAD_GATEWAY = 502;
					static const unsigned short SERVICE_UNAVAILABLE = 503;
					static const unsigned short GATEWAY_TIMEOUT = 504;

					// Initialize.
					static bool init();

					// Build page.
					static bool build_page(connection& conn, unsigned short status_code);

				private:
					struct err {
						unsigned short status_code;
						const char* reason_phrase;
						string::buffer body;
					};

					static struct err _M_errors[];

					static headers _M_headers;

					static const struct err* search(unsigned short status_code);
			};
		}
	}
}

#endif // ERROR_H
