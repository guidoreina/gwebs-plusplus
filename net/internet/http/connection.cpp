#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "net/internet/http/connection.h"
#include "net/internet/http/server.h"
#include "net/internet/http/error.h"
#include "net/internet/http/version.h"
#include "net/tcp_connection.inl"
#include "net/internet/url.h"
#include "net/internet/scheme.h"
#include "net/internet/default_ports.h"
#include "net/internet/limits.h"
#include "string/memcasemem.h"
#include "string/memrchr.h"
#include "util/number.h"
#include "macros/macros.h"

bool net::internet::http::connection::on_timer(unsigned id)
{
	_M_timer_set = 0;

	_M_server->delete_connection(this);

	return true;
}

bool net::internet::http::connection::run()
{
	const string::buffer* bufs[2];
	unsigned short ret = 0;

	do {
		switch (_M_state) {
			case STATE_HANDSHAKING:
#if HAVE_SSL
				if (_M_https) {
					bool completed;
					if (!handshake(ssl_socket::SERVER_MODE, completed)) {
						return false;
					}

					if (!completed) {
						return true;
					}
				}
#endif // HAVE_SSL

				_M_state = STATE_READING_REQUEST_LINE;

				// Fall through.
			case STATE_READING_REQUEST_LINE:
				// If all the received data has been already processed...
				if (_M_inp == _M_in.count()) {
					if (!_M_readable) {
						return true;
					}

					size_t count;
					if (!read(count)) {
						return false;
					}

					// If nothing has been received...
					if (count == 0) {
						return true;
					}
				}

				if ((ret = parse_request_line()) != 0) {
					_M_state = STATE_PREPARING_ERROR_PAGE;
				} else {
					if (_M_substate == 26) {
						_M_state = STATE_AFTER_REQUEST_LINE;
					}
				}

				break;
			case STATE_AFTER_REQUEST_LINE:
				// If all the received data has been already processed...
				if (_M_inp == _M_in.count()) {
					if (!_M_readable) {
						_M_state = STATE_READING_HEADERS;
						return true;
					}

					size_t count;
					if (!read(count)) {
						return false;
					}

					// If nothing has been received...
					if (count == 0) {
						_M_state = STATE_READING_HEADERS;
						return true;
					}
				}

				switch (_M_headers.parse(_M_in.data() + _M_inp, _M_in.count() - _M_inp)) {
					case headers::PARSE_NO_MEMORY:
						ret = error::INTERNAL_SERVER_ERROR;
						_M_state = STATE_PREPARING_ERROR_PAGE;
						break;
					case headers::PARSE_INVALID_HEADER:
						ret = error::BAD_REQUEST;
						_M_state = STATE_PREPARING_ERROR_PAGE;
						break;
					case headers::PARSE_HEADERS_TOO_LARGE:
						ret = error::REQUEST_ENTITY_TOO_LARGE;
						_M_state = STATE_PREPARING_ERROR_PAGE;
						break;
					case headers::PARSE_END_OF_HEADER:
						_M_inp += _M_headers.size();

						_M_state = STATE_PROCESSING_REQUEST;
						break;
					default:
						_M_state = STATE_READING_HEADERS;
				}

				break;
			case STATE_READING_HEADERS:
				if (!_M_readable) {
					return true;
				}

				size_t count;
				if (!read(count)) {
					return false;
				}

				// If nothing has been received...
				if (count == 0) {
					return true;
				}

				switch (_M_headers.parse(_M_in.data() + _M_inp, _M_in.count() - _M_inp)) {
					case headers::PARSE_NO_MEMORY:
						ret = error::INTERNAL_SERVER_ERROR;
						_M_state = STATE_PREPARING_ERROR_PAGE;
						break;
					case headers::PARSE_INVALID_HEADER:
						ret = error::BAD_REQUEST;
						_M_state = STATE_PREPARING_ERROR_PAGE;
						break;
					case headers::PARSE_HEADERS_TOO_LARGE:
						ret = error::REQUEST_ENTITY_TOO_LARGE;
						_M_state = STATE_PREPARING_ERROR_PAGE;
						break;
					case headers::PARSE_END_OF_HEADER:
						_M_inp += _M_headers.size();

						_M_state = STATE_PROCESSING_REQUEST;
						break;
					default:
						;
				}

				break;
			case STATE_PROCESSING_REQUEST:
				if ((ret = process_request()) != 0) {
					_M_state = STATE_PREPARING_ERROR_PAGE;
				} else {
					if (!modify(tcp_server::WRITE)) {
						return false;
					}
				}

				break;
			case STATE_PREPARING_ERROR_PAGE:
				if (!error::build_page(*this, ret)) {
					return false;
				}

				if (!modify(tcp_server::WRITE)) {
					return false;
				}

				_M_state = (_M_method == method::HEAD) ? STATE_SENDING_HEADERS : STATE_SENDING_TWO_BUFFERS;

				break;
			case STATE_SENDING_TWO_BUFFERS:
				if (!_M_writable) {
					return true;
				}

				bufs[0] = &_M_out;
				bufs[1] = _M_bodyp;

				if (!writev(bufs, 2)) {
					return false;
				}

				// If everything has been sent...
				if (_M_outp == _M_out.count() + _M_bodyp->count()) {
					_M_state = STATE_REQUEST_COMPLETED;
				}

				break;
			case STATE_SENDING_HEADERS:
				if (!_M_writable) {
					return true;
				}

				if (!write()) {
					return false;
				}

				// If everything has been sent...
				if (_M_outp == _M_out.count()) {
					if (_M_method == method::HEAD) {
						_M_state = STATE_REQUEST_COMPLETED;
					} else if (_M_filesize == 0) {
						_M_state = STATE_REQUEST_COMPLETED;
					} else {
						if (_M_ranges.count() == 0) {
							_M_outp = 0;
						} else {
							_M_outp = _M_ranges.get(0)->from;
						}

						_M_state = STATE_SENDING_BODY;
					}
				}

				break;
			case STATE_SENDING_BODY:
				if (!_M_writable) {
					return true;
				}

				if (_M_ranges.count() == 0) {
					if (!sendfile(_M_file, _M_filesize)) {
						return false;
					}

					// If everything has been sent...
					if (_M_outp == _M_filesize) {
						_M_socket.uncork();

						_M_state = STATE_REQUEST_COMPLETED;
					}
				} else {
					const util::range* range = _M_ranges.get(_M_nrange);
					if (!sendfile(_M_file, _M_filesize, range)) {
						return false;
					}

					// If everything has been sent...
					if (_M_outp == range->to + 1) {
						if (_M_ranges.count() == 1) {
							_M_socket.uncork();

							_M_state = STATE_REQUEST_COMPLETED;
						} else {
							_M_out.reset();

							// Last part?
							if (++_M_nrange == _M_ranges.count()) {
								if (!_M_out.format("\r\n--%0*u--\r\n", headers::BOUNDARY_LEN, _M_boundary)) {
									return false;
								}

								_M_state = STATE_SENDING_MULTIPART_FOOTER;
							} else {
								if (!build_part_header()) {
									return false;
								}

								_M_state = STATE_SENDING_PART_HEADER;
							}

							_M_outp = 0;
						}
					}
				}

				break;
			case STATE_SENDING_PART_HEADER:
				if (!_M_writable) {
					return true;
				}

				if (!write()) {
					return false;
				}

				// If everything has been sent...
				if (_M_outp == _M_out.count()) {
					_M_outp = _M_ranges.get(_M_nrange)->from;
					_M_state = STATE_SENDING_BODY;
				}

				break;
			case STATE_SENDING_MULTIPART_FOOTER:
				if (!_M_writable) {
					return true;
				}

				if (!write()) {
					return false;
				}

				// If everything has been sent...
				if (_M_outp == _M_out.count()) {
					_M_socket.uncork();

					_M_state = STATE_REQUEST_COMPLETED;
				}

				break;
			case STATE_REQUEST_COMPLETED:
				// Close connection?
				if (!_M_keep_alive) {
					return false;
				} else {
					if (!modify(tcp_server::READ)) {
						return false;
					}

					reset();

					// If there is more data in the input buffer.
					size_t left;
					if ((left = _M_in.count() - _M_inp) > 0) {
						char* data = _M_in.data();
						memmove(data, data + _M_inp, left);
						_M_in.count(left);
					} else {
						_M_in.reset();
					}

					_M_inp = 0;

					_M_state = STATE_READING_REQUEST_LINE;
				}

				break;
		}
	} while (true);
}

bool net::internet::http::connection::add_common_headers(headers& h)
{
	// Keep-Alive?
	if (++_M_nrequests == MAX_REQUESTS_PER_CONNECTION) {
		_M_keep_alive = 0;
	} else {
		const header_value* v;
		if ((v = _M_headers.get_header_value(header_name::CONNECTION)) != NULL) {
			if (string::memcasemem(v->value, v->len, "Keep-Alive", 10)) {
				_M_keep_alive = 1;
			} else if (string::memcasemem(v->value, v->len, "close", 5)) {
				_M_keep_alive = 0;
			} else {
				_M_keep_alive = (_M_http_version == HTTP_1_1);
			}
		} else {
			_M_keep_alive = (_M_http_version == HTTP_1_1);
		}
	}

	h.reset();

	if (!h.add(header_name::DATE, _M_server->utc_time())) {
		return false;
	}

	if (_M_keep_alive) {
		if (!h.add(header_name::CONNECTION, header_value("Keep-Alive", 10))) {
			return false;
		}
	} else {
		if (!h.add(header_name::CONNECTION, header_value("close", 5))) {
			return false;
		}
	}

	if (!h.add(header_name::SERVER, header_value(WEBSERVER_NAME, sizeof(WEBSERVER_NAME) - 1))) {
		return false;
	}

	return true;
}

unsigned short net::internet::http::connection::parse_request_line()
{
	const char* data = _M_in.data();
	const char* end = data + _M_in.count();

	for (const char* d = data + _M_inp; d < end; d++) {
		unsigned char c = (unsigned char) *d;

		switch (_M_substate) {
			case 0: // Initial state.
				if ((c >= 'A') && (c <= 'Z')) {
					_M_token = _M_inp;

					_M_substate = 2; // Method.
				} else if (c == '\r') {
					_M_substate = 1; // '\r' before method.
				} else if (c != '\n') {
					_M_method = method::UNKNOWN;
					return error::BAD_REQUEST;
				}

				break;
			case 1: // '\r' before method.
				if (c != '\n') {
					_M_method = method::UNKNOWN;
					return error::BAD_REQUEST;
				}

				_M_substate = 0; // Initial state.
				break;
			case 2: // Method.
				if (IS_WHITE_SPACE(c)) {
					if ((_M_method = method::search(data + _M_token, _M_inp - _M_token)) == method::UNKNOWN) {
						return error::BAD_REQUEST;
					}

					_M_substate = 3; // Whitespace after method.
				} else if ((c >= 'A') && (c <= 'Z')) {
					// Method too long?
					if (_M_inp - _M_token > method::METHOD_MAX_LEN) {
						_M_method = method::UNKNOWN;
						return error::BAD_REQUEST;
					}
				} else {
					_M_method = method::UNKNOWN;
					return error::BAD_REQUEST;
				}

				break;
			case 3: // Whitespace after method.
				if (c == '/') {
					_M_url = _M_inp;

					_M_token = _M_inp;

					_M_hostlen = 0;

					_M_substate = 13; // Path.
				} else if (IS_ALPHA(c)) {
					_M_url = _M_inp;

					_M_substate = 4; // Scheme.
				} else if (!IS_WHITE_SPACE(c)) {
					return error::BAD_REQUEST;
				}

				break;
			case 4: // Scheme.
				if (c == ':') {
					scheme scheme;
					if ((scheme = scheme::search(data + _M_url, _M_inp - _M_url)) == scheme::HTTP) {
						_M_port = HTTP_DEFAULT_PORT;

#if HAVE_SSL
						_M_https = 0;
#endif
					} else if (scheme == scheme::HTTPS) {
#if !HAVE_SSL
						return error::NOT_FOUND;
#else
						_M_port = HTTPS_DEFAULT_PORT;

						_M_https = 1;
#endif // !HAVE_SSL
					} else if (scheme != scheme::UNKNOWN) {
						return error::NOT_FOUND;
					} else {
						return error::BAD_REQUEST;
					}

					_M_substate = 5; // ':' after scheme.
				} else if (scheme::valid_character(c)) {
					// Scheme too long?
					if (_M_inp - _M_url > scheme::kMaxLen) {
						return error::BAD_REQUEST;
					}
				} else {
					return error::BAD_REQUEST;
				}

				break;
			case 5: // ':' after scheme.
				if (c != '/') {
					return error::BAD_REQUEST;
				}

				_M_substate = 6; // ':/' after scheme.
				break;
			case 6: // ':/' after scheme.
				if (c != '/') {
					return error::BAD_REQUEST;
				}

				_M_substate = 7; // '://' after scheme.
				break;
			case 7: // '://' after scheme.
				if ((IS_ALPHA(c)) || (IS_DIGIT(c)) || (c == '.') || (c == '-')) {
					_M_host = _M_inp;
					_M_hostlen = 1;

					_M_ip_literal = 0;

					_M_substate = 8; // Host.
				} else if (c == '[') {
					_M_substate = 9; // Start of IP-literal.
				} else {
					return error::BAD_REQUEST;
				}

				break;
			case 8: // Host.
				if ((IS_ALPHA(c)) || (IS_DIGIT(c)) || (c == '.') || (c == '-')) {
					if (++_M_hostlen > HOST_MAX_LEN) {
						return error::BAD_REQUEST;
					}
				} else {
					switch (c) {
						case '/':
							_M_token = _M_inp;

							_M_substate = 13; // Path.
							break;
						case ':':
							_M_port = 0;

							_M_substate = 12; // Port.
							break;
						case ' ':
						case '\t':
							_M_urllen = _M_inp - _M_url;

							_M_pathlen = 0;

							_M_querylen = 0;
							_M_fragmentlen = 0;

							_M_substate = 16; // Whitespace after URL.
							break;
						case '\r':
							_M_urllen = _M_inp - _M_url;

							_M_pathlen = 0;

							_M_querylen = 0;
							_M_fragmentlen = 0;

							_M_http_version = HTTP_0_9;

							_M_substate = 25; // '\r' at the end of request line.
							break;
						case '\n':
							_M_urllen = _M_inp - _M_url;

							_M_pathlen = 0;

							_M_querylen = 0;
							_M_fragmentlen = 0;

							_M_http_version = HTTP_0_9;

							_M_substate = 26; // After request line.

							_M_inp++;
							return 0;
						default:
							return error::BAD_REQUEST;
					}
				}

				break;
			case 9: // Start of IP-literal.
				if ((!IS_XDIGIT(c)) && (c != ':') && (c != '.')) {
					return error::BAD_REQUEST;
				}

				_M_host = _M_inp;
				_M_hostlen = 1;

				_M_ip_literal = 1;

				_M_substate = 10; // IP-literal.
				break;
			case 10: // IP-literal.
				if (c == ']') {
					_M_substate = 11; // ']' after <IP-literal>
				} else if ((IS_XDIGIT(c)) || (c == ':') || (c == '.')) {
					if (++_M_hostlen > IPv6_ADDRESS_MAX_LEN) {
						return error::BAD_REQUEST;
					}
				} else {
					return error::BAD_REQUEST;
				}

				break;
			case 11: // ']' after <IP-literal>
				switch (c) {
					case '/':
						_M_token = _M_inp;

						_M_substate = 13; // Path.
						break;
					case ':':
						_M_port = 0;

						_M_substate = 12; // Port.
						break;
					case ' ':
					case '\t':
						_M_urllen = _M_inp - _M_url;

						_M_pathlen = 0;

						_M_querylen = 0;
						_M_fragmentlen = 0;

						_M_substate = 16; // Whitespace after URL.
						break;
					case '\r':
						_M_urllen = _M_inp - _M_url;

						_M_pathlen = 0;

						_M_querylen = 0;
						_M_fragmentlen = 0;

						_M_http_version = HTTP_0_9;

						_M_substate = 25; // '\r' at the end of request line.
						break;
					case '\n':
						_M_urllen = _M_inp - _M_url;

						_M_pathlen = 0;

						_M_querylen = 0;
						_M_fragmentlen = 0;

						_M_http_version = HTTP_0_9;

						_M_substate = 26; // After request line.

						_M_inp++;
						return 0;
					default:
						return error::BAD_REQUEST;
				}

				break;
			case 12: // Port.
				if (IS_DIGIT(c)) {
					unsigned port;
					if ((port = (_M_port * 10) + (c - '0')) > 65535) {
						return error::BAD_REQUEST;
					}

					_M_port = port;
				} else {
					switch (c) {
						case '/':
							if (_M_port == 0) {
								return error::BAD_REQUEST;
							}

							_M_token = _M_inp;

							_M_substate = 13; // Path.
							break;
						case ' ':
						case '\t':
							if (_M_port == 0) {
								return error::BAD_REQUEST;
							}

							_M_urllen = _M_inp - _M_url;

							_M_pathlen = 0;

							_M_querylen = 0;
							_M_fragmentlen = 0;

							_M_substate = 16; // Whitespace after URL.
							break;
						case '\r':
							if (_M_port == 0) {
								return error::BAD_REQUEST;
							}

							_M_urllen = _M_inp - _M_url;

							_M_pathlen = 0;

							_M_querylen = 0;
							_M_fragmentlen = 0;

							_M_http_version = HTTP_0_9;

							_M_substate = 25; // '\r' at the end of request line.
							break;
						case '\n':
							if (_M_port == 0) {
								return error::BAD_REQUEST;
							}

							_M_urllen = _M_inp - _M_url;

							_M_pathlen = 0;

							_M_querylen = 0;
							_M_fragmentlen = 0;

							_M_http_version = HTTP_0_9;

							_M_substate = 26; // After request line.

							_M_inp++;
							return 0;
						default:
							return error::BAD_REQUEST;
					}
				}

				break;
			case 13: // Path.
				switch (c) {
					case '?':
						_M_pathlen = _M_inp - _M_token;

						_M_query = _M_inp;

						_M_substate = 14; // Query.
						break;
					case '#':
						_M_pathlen = _M_inp - _M_token;

						_M_querylen = 0;

						_M_fragment = _M_inp;

						_M_substate = 15; // Fragment.
						break;
					case ' ':
					case '\t':
						_M_urllen = _M_inp - _M_url;

						_M_pathlen = _M_inp - _M_token;

						_M_querylen = 0;
						_M_fragmentlen = 0;

						_M_substate = 16; // Whitespace after URL.
						break;
					case '\r':
						_M_urllen = _M_inp - _M_url;

						_M_pathlen = _M_inp - _M_token;

						_M_querylen = 0;
						_M_fragmentlen = 0;

						_M_http_version = HTTP_0_9;

						_M_substate = 25; // '\r' at the end of request line.
						break;
					case '\n':
						_M_urllen = _M_inp - _M_url;

						_M_pathlen = _M_inp - _M_token;

						_M_querylen = 0;
						_M_fragmentlen = 0;

						_M_http_version = HTTP_0_9;

						_M_substate = 26; // After request line.

						_M_inp++;
						return 0;
					default:
						if (c < ' ') {
							return error::BAD_REQUEST;
						}
				}

				break;
			case 14: // Query.
				if ((!is_unreserved(c)) && (!is_sub_delim(c))) {
					switch (c) {
						case '%':
						case ':':
						case '@':
						case '/':
						case '?':
							break;
						case '#':
							_M_querylen = _M_inp - _M_query;

							_M_fragment = _M_inp;

							_M_substate = 15; // Fragment.
							break;
						case ' ':
						case '\t':
							_M_urllen = _M_inp - _M_url;

							_M_querylen = _M_inp - _M_query;

							_M_fragmentlen = 0;

							_M_substate = 16; // Whitespace after URL.
							break;
						case '\r':
							_M_urllen = _M_inp - _M_url;

							_M_querylen = _M_inp - _M_query;

							_M_fragmentlen = 0;

							_M_http_version = HTTP_0_9;

							_M_substate = 25; // '\r' at the end of request line.
							break;
						case '\n':
							_M_urllen = _M_inp - _M_url;

							_M_querylen = _M_inp - _M_query;

							_M_fragmentlen = 0;

							_M_http_version = HTTP_0_9;

							_M_substate = 26; // After request line.

							_M_inp++;
							return 0;
						default:
							return error::BAD_REQUEST;
					}
				}

				break;
			case 15: // Fragment.
				if ((!is_unreserved(c)) && (!is_sub_delim(c))) {
					switch (c) {
						case '%':
						case ':':
						case '@':
						case '/':
						case '?':
							break;
						case ' ':
						case '\t':
							_M_urllen = _M_inp - _M_url;

							_M_fragmentlen = _M_inp - _M_fragment;

							_M_substate = 16; // Whitespace after URL.
							break;
						case '\r':
							_M_urllen = _M_inp - _M_url;

							_M_fragmentlen = _M_inp - _M_fragment;

							_M_http_version = HTTP_0_9;

							_M_substate = 25; // '\r' at the end of request line.
							break;
						case '\n':
							_M_urllen = _M_inp - _M_url;

							_M_fragmentlen = _M_inp - _M_fragment;

							_M_http_version = HTTP_0_9;

							_M_substate = 26; // After request line.

							_M_inp++;
							return 0;
						default:
							return error::BAD_REQUEST;
					}
				}

				break;
			case 16: // Whitespace after URL.
				switch (c) {
					case 'H':
					case 'h':
						_M_substate = 17; // After <URL> H
						break;
					case ' ':
					case '\t':
						break;
					case '\r':
						_M_http_version = HTTP_0_9;
						break;
					case '\n':
						_M_http_version = HTTP_0_9;

						_M_substate = 26; // After request line.

						_M_inp++;
						return 0;
					default:
						return error::BAD_REQUEST;
				}

				break;
			case 17: // After <URL> H
				if ((c != 'T') && (c != 't')) {
					return error::BAD_REQUEST;
				}

				_M_substate = 18; // After <URL> HT
				break;
			case 18: // After <URL> HT
				if ((c != 'T') && (c != 't')) {
					return error::BAD_REQUEST;
				}

				_M_substate = 19; // After <URL> HTT
				break;
			case 19: // After <URL> HTT
				if ((c != 'P') && (c != 'p')) {
					return error::BAD_REQUEST;
				}

				_M_substate = 20; // After <URL> HTTP
				break;
			case 20: // After <URL> HTTP
				if (c != '/') {
					return error::BAD_REQUEST;
				}

				_M_substate = 21; // After <URL> HTTP/
				break;
			case 21: // After <URL> HTTP/
				if (c != '1') {
					return error::BAD_REQUEST;
				}

				_M_substate = 22; // After <URL> HTTP/1
				break;
			case 22: // After <URL> HTTP/1
				if (c != '.') {
					return error::BAD_REQUEST;
				}

				_M_substate = 23; // After <URL> HTTP/1.
				break;
			case 23: // After <URL> HTTP/1.
				if (c == '1') {
					_M_http_version = HTTP_1_1;
				} else if (c == '0') {
					_M_http_version = HTTP_1_0;
				} else {
					return error::BAD_REQUEST;
				}

				_M_substate = 24; // After <URL> HTTP/1.X
				break;
			case 24: // After <URL> HTTP/1.X
				switch (c) {
					case '\r':
						_M_substate = 25; // '\r' at the end of request line.
						break;
					case '\n':
						_M_substate = 26; // After request line.

						_M_inp++;
						return 0;
					case ' ':
					case '\t':
						break;
					default:
						return error::BAD_REQUEST;
				}

				break;
			case 25: // '\r' at the end of request line.
				if (c != '\n') {
					return error::BAD_REQUEST;
				}

				_M_substate = 26; // After request line.

				_M_inp++;
				return 0;
		}

		if (++_M_inp > REQUEST_LINE_MAX_LEN) {
			return error::REQUEST_ENTITY_TOO_LARGE;
		}
	}

	return 0;
}

unsigned short net::internet::http::connection::parse_path()
{
	if (_M_pathlen <= 1) {
		_M_extension = 0;
		return 0;
	}

	if (!_M_path.allocate(_M_pathlen)) {
		return error::INTERNAL_SERVER_ERROR;
	}

	const char* src = _M_in.data() + _M_token;
	const char* end = src + _M_pathlen;

	char* dest = _M_path.data();

	// Skip initial '/'
	src++;

	int state = 0; // Initial state.

	while (src < end) {
		unsigned char c = (unsigned char) *src;

		int h;

		switch (state) {
			case 0: // Initial state.
				switch (c) {
					case '.':
						state = 1; // '.'
						break;
					case '/':
						// Ignore duplicated '/'
						break;
					case '%':
						if ((h = hex(src + 1, end)) < 0) {
							return error::BAD_REQUEST;
						}

						switch (h) {
							case '.':
								state = 1; // '.'
								break;
							case '/':
								// Ignore duplicated '/'
								break;
							default:
								*dest++ = '/';
								*dest++ = h;

								state = 3; // Segment.
						}

						src += 2;

						break;
					default:
						*dest++ = '/';
						*dest++ = c;

						state = 3; // Segment.
				}

				break;
			case 1: // '.'
				switch (c) {
					case '/':
						state = 0; // Initial state.
						break;
					case '.':
						state = 2; // ".."
						break;
					case '%':
						if ((h = hex(src + 1, end)) < 0) {
							return error::BAD_REQUEST;
						}

						switch (h) {
							case '/':
								state = 0; // Initial state.
								break;
							case '.':
								state = 2; // ".."
								break;
							default:
								*dest++ = '/';
								*dest++ = '.';
								*dest++ = h;

								state = 3; // Segment.
						}

						src += 2;

						break;
					default:
						*dest++ = '/';
						*dest++ = '.';
						*dest++ = c;

						state = 3; // Segment.
				}

				break;
			case 2: // ".."
				switch (c) {
					case '/':
						state = 0; // Initial state.
						break;
					case '%':
						if ((h = hex(src + 1, end)) < 0) {
							return error::BAD_REQUEST;
						}

						if (h == '/') {
							state = 0; // Initial state.
						} else {
							*dest++ = '/';
							*dest++ = '.';
							*dest++ = '.';
							*dest++ = h;

							state = 3; // Segment.
						}

						src += 2;

						break;
					default:
						*dest++ = '/';
						*dest++ = '.';
						*dest++ = '.';
						*dest++ = c;

						state = 3; // Segment.
				}

				break;
			case 3: // Segment.
				switch (c) {
					case '/':
						_M_extension = 0;

						state = 4; // '/'
						break;
					case '%':
						if ((h = hex(src + 1, end)) < 0) {
							return error::BAD_REQUEST;
						}

						switch (h) {
							case '/':
								_M_extension = 0;

								state = 4; // '/'
								break;
							case '.':
								_M_extension = dest - _M_path.data() + 1;

								// Fall through.
							default:
								*dest++ = h;
						}

						src += 2;

						break;
					case '.':
						_M_extension = dest - _M_path.data() + 1;

						// Fall through.
					default:
						*dest++ = c;
				}

				break;
			case 4: // '/'
				switch (c) {
					case '.':
						state = 5; // "/."
						break;
					case '/':
						// Ignore duplicated '/'
						break;
					case '%':
						if ((h = hex(src + 1, end)) < 0) {
							return error::BAD_REQUEST;
						}

						switch (h) {
							case '.':
								state = 5; // "/."
								break;
							case '/':
								// Ignore duplicated '/'
								break;
							default:
								*dest++ = '/';
								*dest++ = h;

								state = 3; // Segment.
						}

						src += 2;

						break;
					default:
						*dest++ = '/';
						*dest++ = c;

						state = 3; // Segment.
				}

				break;
			case 5: // "/."
				switch (c) {
					case '/':
						state = 4; // '/'
						break;
					case '.':
						state = 6; // "/.."
						break;
					case '%':
						if ((h = hex(src + 1, end)) < 0) {
							return error::BAD_REQUEST;
						}

						switch (h) {
							case '/':
								state = 4; // '/'
								break;
							case '.':
								state = 6; // "/.."
								break;
							default:
								*dest++ = '/';
								*dest++ = '.';
								*dest++ = h;

								state = 3; // Segment.
						}

						src += 2;

						break;
					default:
						*dest++ = '/';
						*dest++ = '.';
						*dest++ = c;

						state = 3; // Segment.
				}

				break;
			case 6: // "/.."
				switch (c) {
					case '/':
						if (dest == _M_path.data()) {
							return error::FORBIDDEN;
						}

						while (--dest > _M_path.data()) {
							if (*dest == '/') {
								break;
							}
						}

						state = 4; // '/'
						break;
					case '%':
						if ((h = hex(src + 1, end)) < 0) {
							return error::BAD_REQUEST;
						}

						if (h == '/') {
							if (dest == _M_path.data()) {
								return error::FORBIDDEN;
							}

							while (--dest > _M_path.data()) {
								if (*dest == '/') {
									break;
								}
							}

							state = 4; // '/'
						} else {
							*dest++ = '/';
							*dest++ = '.';
							*dest++ = '.';
							*dest++ = h;

							state = 3; // Segment.
						}

						src += 2;

						break;
					default:
						*dest++ = '/';
						*dest++ = '.';
						*dest++ = '.';
						*dest++ = c;

						state = 3; // Segment.
				}

				break;
		}

		src++;
	}

	switch (state) {
		case 0: // Initial state.
		case 1: // '.'
		case 2: // ".."
			// Empty path.
			break;
		case 3: // Segment.
			_M_path.count(dest - _M_path.data());
			break;
		case 4: // '/'
			*dest++ = '/';
			_M_path.count(dest - _M_path.data());
			break;
		case 5: // "/."
			*dest++ = '/';
			_M_path.count(dest - _M_path.data());
			break;
		case 6: // "/.."
			if (dest == _M_path.data()) {
				return error::FORBIDDEN;
			}

			while (--dest > _M_path.data()) {
				if (*dest == '/') {
					dest++;
					break;
				}
			}

			_M_path.count(dest - _M_path.data());

			break;
	}

	return 0;
}

unsigned short net::internet::http::connection::process_request()
{
	// Parse path.
	unsigned short ret;
	if ((ret = parse_path()) != 0) {
		return ret;
	}

	const header_value* v;

	// If an absolute URI has been received...
	if (_M_hostlen > 0) {
		if (_M_http_version == HTTP_1_1) {
			// Ignore Host header (if present).
			if (!_M_headers.get_header_value(header_name::HOST)) {
				return error::BAD_REQUEST;
			}
		}

		_M_vhost = static_cast<server*>(_M_server)->virtual_hosts().find(_M_in.data() + _M_host, _M_hostlen, _M_port);
	} else {
		// Get Host header.
		if ((v = _M_headers.get_header_value(header_name::HOST)) == NULL) {
			// Host header not present.
			if (_M_http_version == HTTP_1_1) {
				return error::BAD_REQUEST;
			}

			_M_vhost = static_cast<server*>(_M_server)->virtual_hosts().default_vhost();
		} else {
			unsigned port;

			size_t len;
			const char* colon;
			if ((colon = (const char*) string::memrchr(v->value, ':', v->len)) != NULL) {
				len = colon - v->value;

				if (util::number::parse(colon + 1, v->len - len - 1, port, 1, 65535) != util::number::PARSE_SUCCEEDED) {
					return error::BAD_REQUEST;
				}
			} else {
				len = v->len;

				if (!_M_listener->_M_data) {
					port = HTTP_DEFAULT_PORT;
				} else {
					port = HTTPS_DEFAULT_PORT;
				}
			}

			_M_vhost = static_cast<server*>(_M_server)->virtual_hosts().find(v->value, len, port);
		}
	}

	if (!_M_vhost) {
		return error::NOT_FOUND;
	}

	// Only the GET and HEAD methods are supported.
	if ((_M_method != method::GET) && (_M_method != method::HEAD)) {
		return error::NOT_IMPLEMENTED;
	}

	// Check that the path is not too long.
	size_t rootlen = _M_vhost->rootlen();
	if (rootlen + ((_M_path.count() == 0) ? 1 : _M_path.count()) > PATH_MAX) {
		return error::REQUEST_URI_TOO_LONG;
	}

	// Compose path.
	char path[PATH_MAX + 1];
	memcpy(path, _M_vhost->root(), rootlen);
	size_t pathlen = rootlen;

	if (_M_path.count() == 0) {
		path[pathlen++] = '/';
	} else {
		memcpy(path + pathlen, _M_path.data(), _M_path.count());
		pathlen += _M_path.count();
	}

	path[pathlen] = 0;

	// Get file status.
	struct stat buf;
	if (stat(path, &buf) < 0) {
		return error::NOT_FOUND;
	}

	// Directory?
	const char* index = NULL;
	if (S_ISDIR(buf.st_mode)) {
		// If the directory name doesn't end with '/'...
		if (path[pathlen - 1] != '/') {
			return error::MOVED_PERMANENTLY;
		}

		// Search index file.
		unsigned short indexlen;
		for (unsigned i = 0; ((index = _M_vhost->index(i, indexlen, _M_mime_type, _M_mime_type_len)) != NULL); i++) {
			if (pathlen + indexlen < sizeof(path)) {
				memcpy(path + pathlen, index, indexlen + 1);
				if ((stat(path, &buf) == 0) && (S_ISREG(buf.st_mode))) {
					pathlen += indexlen;
					break;
				}
			}

			index = NULL;
		}

		// If no index file has been found...
		if (!index) {
			// If the directory listing is not enabled...
			dirlisting* dirlisting;
			if ((dirlisting = _M_vhost->get_directory_listing()) == NULL) {
				return error::NOT_FOUND;
			}

			// Build directory listing.
			if (!dirlisting->build(path + rootlen, pathlen - rootlen, _M_body)) {
				return error::INTERNAL_SERVER_ERROR;
			}

			// Add common headers.
			if (!add_common_headers(_M_headers)) {
				return error::INTERNAL_SERVER_ERROR;
			}

			// Add Content-Type header.
			if (!_M_headers.add(header_name::CONTENT_TYPE, header_value("text/html; charset=UTF-8", 24))) {
				return error::INTERNAL_SERVER_ERROR;
			}

			// Add Content-Length header.
			if (!_M_headers.add(header_name::CONTENT_LENGTH, _M_body.count())) {
				return error::INTERNAL_SERVER_ERROR;
			}

			// Add Status-Line.
			if (!_M_out.append("HTTP/1.1 200 OK\r\n", 17)) {
				return error::INTERNAL_SERVER_ERROR;
			}

			// Serialize headers.
			if (!_M_headers.serialize(_M_out)) {
				return error::INTERNAL_SERVER_ERROR;
			}

			_M_bodyp = &_M_body;

			_M_state = (_M_method == method::HEAD) ? STATE_SENDING_HEADERS : STATE_SENDING_TWO_BUFFERS;

			return 0;
		}
	} else if (!S_ISREG(buf.st_mode)) {
		return error::NOT_FOUND;
	}

	_M_filesize = buf.st_size;

	// File.
	if ((_M_method == method::GET) && (_M_filesize > 0)) {
		if (!_M_file.open(path, O_RDONLY)) {
			return error::INTERNAL_SERVER_ERROR;
		}
	}

	// If the If-Modified-Since header is present...
	time_t t;
	if (_M_headers.get_header_value(header_name::IF_MODIFIED_SINCE, t)) {
		_M_last_modified = buf.st_mtime;

		if (t == _M_last_modified) {
			return error::NOT_MODIFIED;
		}
	}

	unsigned status_code;

	if (!_M_headers.get_ranges(_M_filesize, _M_ranges)) {
		_M_ranges.reset();

		if (!_M_out.append("HTTP/1.1 200 OK\r\n", 17)) {
			return error::INTERNAL_SERVER_ERROR;
		}

		status_code = 200;
	} else if (_M_ranges.count() == 0) {
		return error::REQUESTED_RANGE_NOT_SATISFIABLE;
	} else {
		if (!_M_out.append("HTTP/1.1 206 Partial Content\r\n", 30)) {
			return error::INTERNAL_SERVER_ERROR;
		}

		status_code = 206;
	}

	if (!add_common_headers(_M_headers)) {
		return error::INTERNAL_SERVER_ERROR;
	}

	char buffer[128];
	header_value value;
	value.value = buffer;
	value.len = snprintf(buffer, sizeof(buffer), "\"%x-%x\"", buf.st_mtime, _M_filesize);

	if (!_M_headers.add(header_name::ETAG, value)) {
		return error::INTERNAL_SERVER_ERROR;
	}

	if (!_M_headers.add(header_name::ACCEPT_RANGES, header_value("bytes", 5))) {
		return error::INTERNAL_SERVER_ERROR;
	}

	// Add MIME type.
	if (!index) {
		unsigned short extlen;
		if ((_M_extension == 0) || ((extlen = _M_path.count() - _M_extension) == 0)) {
			_M_mime_type = mime::types::DEFAULT_MIME_TYPE;
			_M_mime_type_len = mime::types::DEFAULT_MIME_TYPE_LEN;
		} else {
			_M_mime_type = static_cast<server*>(_M_server)->mime_type(_M_path.data() + _M_extension, extlen, _M_mime_type_len);
		}
	}

	if (!_M_headers.add(header_name::CONTENT_TYPE, header_value(_M_mime_type, _M_mime_type_len))) {
		return error::INTERNAL_SERVER_ERROR;
	}

	_M_bodysize = compute_content_length();
	if (!_M_headers.add(header_name::CONTENT_LENGTH, _M_bodysize)) {
		return error::INTERNAL_SERVER_ERROR;
	}

	if (!_M_headers.add(header_name::LAST_MODIFIED, buf.st_mtime)) {
		return error::INTERNAL_SERVER_ERROR;
	}

	switch (_M_ranges.count()) {
		case 0:
			break;
		case 1:
			{
				const util::range* range = _M_ranges.get(0);

				value.len = snprintf(buffer, sizeof(buffer), "bytes %lld-%lld/%lld", range->from, range->to, _M_filesize);

				if (!_M_headers.add(header_name::CONTENT_RANGE, value)) {
					return error::INTERNAL_SERVER_ERROR;
				}
			}

			break;
		default:
			_M_boundary = static_cast<server*>(_M_server)->boundary();

			value.len = snprintf(buffer, sizeof(buffer), "multipart/byteranges; boundary=%0*u", headers::BOUNDARY_LEN, _M_boundary);

			if (!_M_headers.add(header_name::CONTENT_TYPE, value)) {
				return error::INTERNAL_SERVER_ERROR;
			}
	}

	if (!_M_headers.serialize(_M_out)) {
		return error::INTERNAL_SERVER_ERROR;
	}

	if ((_M_ranges.count() > 1) && (_M_method == method::GET)) {
		if (!build_part_header()) {
			return error::INTERNAL_SERVER_ERROR;
		}
	}

	if ((_M_method == method::GET) && (_M_filesize > 0)) {
		_M_socket.cork();
	}

	_M_state = STATE_SENDING_HEADERS;

	return 0;
}

off_t net::internet::http::connection::compute_content_length() const
{
	switch (_M_ranges.count()) {
		case 0:
			return _M_filesize;
		case 1:
			{
				const util::range* range = _M_ranges.get(0);
				return (range->to - range->from + 1);
			}

		default:
			{
				off_t content_length = 0;
				size_t len = util::number::length(_M_filesize);

				size_t nranges = _M_ranges.count();
				for (size_t i = 0; i < nranges; i++) {
					off_t from = 0, to = 0;
					_M_ranges.get(i, from, to);

					content_length += (2 + 2 + headers::BOUNDARY_LEN + 2 + 14 + _M_mime_type_len + 2 + 21 + util::number::length(from) + 1 + util::number::length(to) + 1 + len + 2 + 2);

					content_length += (to - from + 1);
				}

				return (content_length + 2 + 2 + headers::BOUNDARY_LEN + 2 + 2);
			}
	}
}
