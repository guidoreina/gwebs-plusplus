#ifndef HTTP_CONNECTION_H
#define HTTP_CONNECTION_H

#include <sys/types.h>
#include "fs/file.h"
#include "net/tcp_connection.h"
#include "net/internet/http/method.h"
#include "net/internet/http/headers.h"
#include "net/internet/http/vhost.h"
#include "util/ranges.h"
#include "macros/macros.h"

namespace net {
	namespace internet {
		namespace http {
			struct connection : public tcp_connection {
				public:
					static const unsigned MAX_REQUESTS_PER_CONNECTION = 10;

					static const size_t REQUEST_LINE_MAX_LEN = 32 * 1024;

					// HTTP states.
					static const unsigned char STATE_HANDSHAKING = 0;
					static const unsigned char STATE_READING_REQUEST_LINE = 1;
					static const unsigned char STATE_AFTER_REQUEST_LINE = 2;
					static const unsigned char STATE_READING_HEADERS = 3;
					static const unsigned char STATE_PROCESSING_REQUEST = 4;
					static const unsigned char STATE_PREPARING_ERROR_PAGE = 5;
					static const unsigned char STATE_SENDING_TWO_BUFFERS = 6;
					static const unsigned char STATE_SENDING_HEADERS = 7;
					static const unsigned char STATE_SENDING_BODY = 8;
					static const unsigned char STATE_SENDING_PART_HEADER = 9;
					static const unsigned char STATE_SENDING_MULTIPART_FOOTER = 10;
					static const unsigned char STATE_REQUEST_COMPLETED = 11;

					// HTTP versions.
					static const unsigned char HTTP_0_9 = 0;
					static const unsigned char HTTP_1_0 = 1;
					static const unsigned char HTTP_1_1 = 2;

					headers _M_headers;

					vhost* _M_vhost;

					util::ranges _M_ranges;
					unsigned _M_nrange;
					unsigned _M_boundary;

					unsigned short _M_nrequests;

					unsigned short _M_url;
					unsigned short _M_urllen;

					unsigned short _M_token;

					unsigned short _M_host;
					unsigned short _M_hostlen;

					unsigned short _M_port;

					string::buffer _M_path;
					unsigned short _M_pathlen;

					unsigned short _M_extension;

					unsigned short _M_query;
					unsigned short _M_querylen;

					unsigned short _M_fragment;
					unsigned short _M_fragmentlen;

					string::buffer _M_body;
					const string::buffer* _M_bodyp;

					method _M_method;

					const char* _M_mime_type;
					unsigned short _M_mime_type_len;

					fs::file _M_file;
					off_t _M_filesize;
					time_t _M_last_modified;

					off_t _M_bodysize;

					unsigned _M_substate:5;

#if HAVE_SSL
					unsigned _M_https:1;
#endif

					unsigned _M_ip_literal:1;
					unsigned _M_ipv6:1;
					unsigned _M_http_version:2;
					unsigned _M_keep_alive:1;

					// Constructor.
					connection();

					// Destructor.
					~connection();

					// Free.
					virtual void free();

					// Reset.
					virtual void reset();

					// On timer.
					bool on_timer(unsigned id);

					// Run.
					bool run();

					// Add common headers.
					bool add_common_headers(headers& h);

				private:
					// Reset.
					void _reset();

					// Parse request line.
					unsigned short parse_request_line();

					// Parse path.
					unsigned short parse_path();

					// Process request.
					unsigned short process_request();

					// Compute Content-Length.
					off_t compute_content_length() const;

					// Build part header.
					bool build_part_header();

					static int hex(const char* src, const char* end);
			};

			inline connection::connection() : _M_file(-1)
			{
				_M_nrange = 0;

				_M_nrequests = 0;
				_M_substate = 0;

				_M_http_version = HTTP_0_9;
				_M_keep_alive = 0;
			}

			inline connection::~connection()
			{
				if (_M_file.fd() != -1) {
					_M_file.close();
				}
			}

			inline void connection::free()
			{
				tcp_connection::free();

				_M_nrequests = 0;

				_reset();
			}

			inline void connection::reset()
			{
				tcp_connection::reset();
				_reset();
			}

			inline void connection::_reset()
			{
				_M_headers.reset();

				_M_ranges.reset();
				_M_nrange = 0;

				if (_M_path.size() > 32) {
					_M_path.free();
				} else {
					_M_path.reset();
				}

				if (_M_body.size() > 2 * 1024) {
					_M_body.free();
				} else {
					_M_body.reset();
				}

				if (_M_file.fd() != -1) {
					_M_file.close();
					_M_file.fd(-1);
				}

				_M_substate = 0;

				_M_http_version = HTTP_0_9;
				_M_keep_alive = 0;
			}

			inline bool connection::build_part_header()
			{
				const util::range* range = _M_ranges.get(_M_nrange);

				return _M_out.format("\r\n--%0*u\r\nContent-Type: %.*s\r\nContent-Range: bytes %lld-%lld/%lld\r\n\r\n", headers::BOUNDARY_LEN, _M_boundary, _M_mime_type_len, _M_mime_type, range->from, range->to, _M_filesize);
			}

			inline int connection::hex(const char* src, const char* end)
			{
				if (src + 1 >= end) {
					return -1;
				}

				unsigned char c = *src;

				unsigned char hex;

				if (IS_DIGIT(c)) {
					hex = c - '0';
				} else if ((c >= 'A') && (c <= 'F')) {
					hex = 10 + (c - 'A');
				} else if ((c >= 'a') && (c <= 'f')) {
					hex = 10 + (c - 'a');
				} else {
					return -1;
				}

				c = *(src + 1);

				if (IS_DIGIT(c)) {
					return (hex * 16) + (c - '0');
				} else if ((c >= 'A') && (c <= 'F')) {
					return (hex * 16) + (10 + (c - 'A'));
				} else if ((c >= 'a') && (c <= 'f')) {
					return (hex * 16) + (10 + (c - 'a'));
				} else {
					return -1;
				}
			}
		}
	}
}

#endif // HTTP_CONNECTION_H
