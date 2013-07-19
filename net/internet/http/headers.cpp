#include <string.h>
#include "net/internet/http/headers.h"
#include "macros/macros.h"

const struct net::internet::http::standard_header net::internet::http::headers::_M_standard_headers[] = {
	{"Accept",               6, REQUEST_HEADER,  1, 0},
	{"Accept-Charset",      14, REQUEST_HEADER,  1, 0},
	{"Accept-Encoding",     15, REQUEST_HEADER,  1, 0},
	{"Accept-Language",     15, REQUEST_HEADER,  1, 0},
	{"Accept-Ranges",       13, RESPONSE_HEADER, 1, 0},
	{"Age",                  3, RESPONSE_HEADER, 0, 1},
	{"Allow",                5, ENTITY_HEADER,   1, 0},
	{"Authorization",       13, REQUEST_HEADER,  0, 0},
	{"Cache-Control",       13, GENERAL_HEADER,  1, 0},
	{"Connection",          10, GENERAL_HEADER,  1, 0},
	{"Content-Encoding",    16, ENTITY_HEADER,   1, 0},
	{"Content-Language",    16, ENTITY_HEADER,   1, 0},
	{"Content-Length",      14, ENTITY_HEADER,   0, 1},
	{"Content-Location",    16, ENTITY_HEADER,   0, 1},
	{"Content-MD5",         11, ENTITY_HEADER,   0, 1},
	{"Content-Range",       13, ENTITY_HEADER,   0, 0},
	{"Content-Type",        12, ENTITY_HEADER,   0, 0},
	{"Cookie",               6, REQUEST_HEADER,  1, 0},
	{"Date",                 4, GENERAL_HEADER,  0, 0},
	{"ETag",                 4, RESPONSE_HEADER, 0, 1},
	{"Expect",               6, REQUEST_HEADER,  1, 0},
	{"Expires",              7, ENTITY_HEADER,   0, 0},
	{"From",                 4, REQUEST_HEADER,  0, 1},
	{"Host",                 4, REQUEST_HEADER,  0, 1},
	{"If-Match",             8, REQUEST_HEADER,  1, 0},
	{"If-Modified-Since",   17, REQUEST_HEADER,  0, 0},
	{"If-None-Match",       13, REQUEST_HEADER,  1, 0},
	{"If-Range",             8, REQUEST_HEADER,  0, 0},
	{"If-Unmodified-Since", 19, REQUEST_HEADER,  0, 0},
	{"Keep-Alive",          10, GENERAL_HEADER,  1, 0},
	{"Last-Modified",       13, ENTITY_HEADER,   0, 0},
	{"Location",             8, RESPONSE_HEADER, 0, 1},
	{"Max-Forwards",        12, REQUEST_HEADER,  0, 1},
	{"Pragma",               6, GENERAL_HEADER,  1, 0},
	{"Proxy-Authenticate",  18, RESPONSE_HEADER, 1, 0},
	{"Proxy-Authorization", 19, REQUEST_HEADER,  0, 0},
	{"Proxy-Connection",    16, GENERAL_HEADER,  1, 0},
	{"Range",                5, REQUEST_HEADER,  0, 0},
	{"Referer",              7, REQUEST_HEADER,  0, 1},
	{"Retry-After",         11, RESPONSE_HEADER, 0, 0},
	{"Server",               6, RESPONSE_HEADER, 1, 0},
	{"Set-Cookie",          10, RESPONSE_HEADER, 1, 0},
	{"Status",               6, RESPONSE_HEADER, 0, 0},
	{"TE",                   2, REQUEST_HEADER,  1, 0},
	{"Trailer",              7, GENERAL_HEADER,  1, 0},
	{"Transfer-Encoding",   17, GENERAL_HEADER,  1, 0},
	{"Upgrade",              7, GENERAL_HEADER,  1, 0},
	{"User-Agent",          10, REQUEST_HEADER,  1, 0},
	{"Vary",                 4, RESPONSE_HEADER, 1, 0},
	{"Via",                  3, GENERAL_HEADER,  1, 0},
	{"Warning",              7, GENERAL_HEADER,  1, 0},
	{"WWW-Authenticate",    16, RESPONSE_HEADER, 1, 0}
};

bool net::internet::http::headers::get_ranges(off_t filesize, util::ranges& ranges) const
{
	const header_value* value;
	if ((value = get_header_value(header_name::RANGE)) == NULL) {
		return false;
	}

	if (value->len < 7) {
		return false;
	}

	const char* v = value->value;
	if (strncasecmp(v, "bytes", 5) != 0) {
		return false;
	}

	const char* end = v + value->len;
	v += 5;

	off_t from = 0;
	off_t to = 0;
	off_t count = 0;

	int state = 0; // Waiting for '='.

	while (v < end) {
		unsigned char c = (unsigned char) *v++;

		switch (state) {
			case 0: // Waiting for '='.
				if (c == '=') {
					state = 1; // After '='.
				} else if (!IS_WHITE_SPACE(c)) {
					return false;
				}

				break;
			case 1: // After '='.
				if (IS_DIGIT(c)) {
					from = c - '0';

					state = 2; // Parsing from.
				} else if (c == '-') {
					state = 8; // Start of suffix-length.
				} else if (!IS_WHITE_SPACE(c)) {
					return false;
				}

				break;
			case 2: // Parsing from.
				if (IS_DIGIT(c)) {
					off_t tmp;
					if ((tmp = (from * 10) + (c - '0')) < from) {
						return false;
					}

					from = tmp;
				} else if (c == '-') {
					state = 4; // After '-'.
				} else if (IS_WHITE_SPACE(c)) {
					state = 3; // Whitespace after from.
				} else {
					return false;
				}

				break;
			case 3: // Whitespace after from.
				if (c == '-') {
					state = 4; // After '-'.
				} else if (!IS_WHITE_SPACE(c)) {
					return false;
				}

				break;
			case 4: // After '-'.
				if (IS_DIGIT(c)) {
					to = c - '0';

					state = 5; // Parsing to.
				} else if (c == ',') {
					if (from < filesize) {
						if ((ranges.count() == MAX_RANGES) || (!ranges.add(from, filesize - 1))) {
							return false;
						}
					}

					state = 7; // After ','.
				} else if (!IS_WHITE_SPACE(c)) {
					return false;
				}

				break;
			case 5: // Parsing to.
				if (IS_DIGIT(c)) {
					off_t tmp;
					if ((tmp = (to * 10) + (c - '0')) < to) {
						return false;
					}

					to = tmp;
				} else if (c == ',') {
					if (from > to) {
						return false;
					}

					if (from < filesize) {
						if ((ranges.count() == MAX_RANGES) || (!ranges.add(from, MIN(to, filesize - 1)))) {
							return false;
						}
					}

					state = 7; // After ','.
				} else if (IS_WHITE_SPACE(c)) {
					if (from > to) {
						return false;
					}

					if (from < filesize) {
						if ((ranges.count() == MAX_RANGES) || (!ranges.add(from, MIN(to, filesize - 1)))) {
							return false;
						}
					}

					state = 6; // Whitespace after to.
				} else {
					return false;
				}

				break;
			case 6: // Whitespace after to.
				if (c == ',') {
					state = 7; // After ','.
				} else if (!IS_WHITE_SPACE(c)) {
					return false;
				}

				break;
			case 7: // After ','.
				if (IS_DIGIT(c)) {
					from = c - '0';

					state = 2; // Parsing from.
				} else if (c == '-') {
					state = 8; // Start of suffix-length.
				} else if (!IS_WHITE_SPACE(c)) {
					return false;
				}

				break;
			case 8: // Start of suffix-length.
				if (IS_DIGIT(c)) {
					count = c - '0';

					state = 9; // Parsing suffix-length.
				} else if (!IS_WHITE_SPACE(c)) {
					return false;
				}

				break;
			case 9: // Parsing suffix-length.
				if (IS_DIGIT(c)) {
					off_t tmp;
					if ((tmp = (count * 10) + (c - '0')) < count) {
						return false;
					}

					count = tmp;
				} else if (c == ',') {
					if ((count > 0) && (filesize > 0)) {
						if ((ranges.count() == MAX_RANGES) || (!ranges.add(MAX(filesize - count, 0), filesize - 1))) {
							return false;
						}
					}

					state = 7; // After ','.
				} else if (IS_WHITE_SPACE(c)) {
					if ((count > 0) && (filesize > 0)) {
						if ((ranges.count() == MAX_RANGES) || (!ranges.add(MAX(filesize - count, 0), filesize - 1))) {
							return false;
						}
					}

					state = 10; // Whitespace after suffix-length.
				} else {
					return false;
				}

				break;
			case 10: // Whitespace after suffix-length.
				if (c == ',') {
					state = 7; // After ','.
				} else if (!IS_WHITE_SPACE(c)) {
					return false;
				}

				break;
		}
	}

	switch (state) {
		case 4:
			if (from >= filesize) {
				return true;
			}

			return ((ranges.count() < MAX_RANGES) && (ranges.add(from, filesize - 1)));
		case 5:
			if (from > to) {
				return false;
			}

			if (from >= filesize) {
				return true;
			}

			return ((ranges.count() < MAX_RANGES) && (ranges.add(from, MIN(to, filesize - 1))));
		case 6:
		case 10:
			return true;
		case 9:
			if ((count > 0) && (filesize > 0)) {
				if ((ranges.count() == MAX_RANGES) || (!ranges.add(MAX(filesize - count, 0), filesize - 1))) {
					return false;
				}
			}

			return true;
		default:
			return false;
	}
}

bool net::internet::http::headers::add(const header_name& name, const header_value& value)
{
	if (!allocate()) {
		return false;
	}

	unsigned pos;
	if (name.value != header_name::UNKNOWN) {
		if (!_M_buf.allocate(value.len + 1)) {
			return false;
		}

		if (search(name.value, pos)) {
			pos++;
		}
	} else {
		if (!_M_buf.allocate(name.len + 1 + value.len + 1)) {
			return false;
		}

		if (search(name.name, name.len, pos)) {
			pos++;
		}
	}

	char* data = _M_buf.data();
	size_t count = _M_buf.count();

	int len;
	if ((len = clean(value, data + count)) < 0) {
		return false;
	}

	if (pos < _M_used) {
		memmove(&_M_headers[pos + 1], &_M_headers[pos], (_M_used - pos) * sizeof(struct header));
	}

	struct header* h = &_M_headers[pos];

	h->name.value = name.value;

	h->value.off = count;
	h->value.len = len;

	count += (len + 1);

	if (name.value != header_name::UNKNOWN) {
		h->name.name = _M_standard_headers[name.value].name;
		h->name.len = _M_standard_headers[name.value].len;
	} else {
		memcpy(data + count, name.name, name.len);
		h->name.off = count;
		h->name.len = name.len;

		count += name.len;

		data[count++] = 0;
	}

	_M_buf.count(count);

	_M_used++;

	return true;
}

bool net::internet::http::headers::remove(unsigned char header)
{
	unsigned pos;
	if (!search(header, pos)) {
		return false;
	}

	_M_used--;

	if (pos < _M_used) {
		memmove(&_M_headers[pos], &_M_headers[pos + 1], (_M_used - pos) * sizeof(struct header));
	}

	return true;
}

bool net::internet::http::headers::remove(const char* name, size_t len)
{
	unsigned pos;
	if (!search(name, len, pos)) {
		return false;
	}

	_M_used--;

	if (pos < _M_used) {
		memmove(&_M_headers[pos], &_M_headers[pos + 1], (_M_used - pos) * sizeof(struct header));
	}

	return true;
}

net::internet::http::headers::parse_result net::internet::http::headers::parse(const void* buf, size_t count)
{
	const char* b = (const char*) buf;
	const char* end = b + count;

	const char* ptr = b + _M_state.size;

	while (ptr < end) {
		unsigned char c = (unsigned char) *ptr++;

		switch (_M_state.state) {
			case 0: // Initial state.
				if (c == '\r') {
					_M_state.state = 6; // After second '\r'.
				} else if (c == '\n') {
					_M_state.size++;

					return PARSE_END_OF_HEADER;
				} else {
					if (!header_name::valid_character(c)) {
						return PARSE_INVALID_HEADER;
					}

					_M_state.name = _M_state.size;
					_M_state.namelen = 1;

					_M_state.state = 1; // Parsing header name.
				}

				break;
			case 1: // Parsing header name.
				if (c == ':') {
					_M_state.header = search_standard_header(b + _M_state.name, _M_state.namelen);

					_M_state.value = 0;

					_M_state.state = 2; // After colon.
				} else if (header_name::valid_character(c)) {
					// Field name too long?
					if (++_M_state.namelen > header_name::MAX_LEN) {
						return PARSE_INVALID_HEADER;
					}
				} else {
					return PARSE_INVALID_HEADER;
				}

				break;
			case 2: // After colon.
				if (c == '\r') {
					_M_state.state = 4; // After '\r'.
				} else if (c == '\n') {
					_M_state.state = 5; // After '\n'.
				} else if (header_value::valid_character(c)) {
					if ((c != ' ') && (c != '\t')) {
						_M_state.value = _M_state.size;
						_M_state.value_end = 0;

						_M_state.state = 3; // Parsing header value.
					}
				} else {
					return PARSE_INVALID_HEADER;
				}

				break;
			case 3: // Parsing header value.
				if (c == '\r') {
					if (_M_state.value_end == 0) {
						_M_state.value_end = _M_state.size;
					}

					_M_state.state = 4; // After '\r'.
				} else if (c == '\n') {
					if (_M_state.value_end == 0) {
						_M_state.value_end = _M_state.size;
					}

					_M_state.state = 5; // After '\n'.
				} else if (header_value::valid_character(c)) {
					if ((c == ' ') || (c == '\t')) {
						if (_M_state.value_end == 0) {
							_M_state.value_end = _M_state.size;
						}
					} else {
						if ((!_M_ignore_errors) && (_M_state.value_end != 0) && (_M_state.header != header_name::UNKNOWN) && (_M_standard_headers[_M_state.header].single_token)) {
							return PARSE_INVALID_HEADER;
						}

						_M_state.value_end = 0;
					}
				} else {
					return PARSE_INVALID_HEADER;
				}

				break;
			case 4: // After '\r'.
				if (c != '\n') {
					return PARSE_INVALID_HEADER;
				}

				_M_state.state = 5; // After '\n'.

				break;
			case 5: // After '\n'.
				if (c == '\r') {
					if (_M_state.value != 0) {
						if ((!_M_ignore_errors) && (_M_state.header != header_name::UNKNOWN) && (!_M_standard_headers[_M_state.header].comma_separated_list)) {
							// If the header has been inserted already...
							unsigned pos;
							if (search(_M_state.header, pos)) {
								return PARSE_INVALID_HEADER;
							}
						}

						header_name name(_M_state.header, b + _M_state.name, _M_state.namelen);
						header_value value(b + _M_state.value, _M_state.value_end - _M_state.value);

						if (!add(name, value)) {
							return PARSE_NO_MEMORY;
						}
					}

					_M_state.state = 6; // After second '\r'.
				} else if (c == '\n') {
					if (_M_state.value != 0) {
						if ((!_M_ignore_errors) && (_M_state.header != header_name::UNKNOWN) && (!_M_standard_headers[_M_state.header].comma_separated_list)) {
							// If the header has been inserted already...
							unsigned pos;
							if (search(_M_state.header, pos)) {
								return PARSE_INVALID_HEADER;
							}
						}

						header_name name(_M_state.header, b + _M_state.name, _M_state.namelen);
						header_value value(b + _M_state.value, _M_state.value_end - _M_state.value);

						if (!add(name, value)) {
							return PARSE_NO_MEMORY;
						}
					}

					_M_state.size++;

					return PARSE_END_OF_HEADER;
				} else if ((c == ' ') || (c == '\t')) {
					// Folded header value.
					_M_state.state = (_M_state.value == 0) ? 2 : 3;
				} else if (header_name::valid_character(c)) {
					if (_M_state.value != 0) {
						if ((!_M_ignore_errors) && (_M_state.header != header_name::UNKNOWN) && (!_M_standard_headers[_M_state.header].comma_separated_list)) {
							// If the header has been inserted already...
							unsigned pos;
							if (search(_M_state.header, pos)) {
								return PARSE_INVALID_HEADER;
							}
						}

						header_name name(_M_state.header, b + _M_state.name, _M_state.namelen);
						header_value value(b + _M_state.value, _M_state.value_end - _M_state.value);

						if (!add(name, value)) {
							return PARSE_NO_MEMORY;
						}
					}

					_M_state.name = _M_state.size;
					_M_state.namelen = 1;

					_M_state.state = 1; // Parsing header name.
				} else {
					return PARSE_INVALID_HEADER;
				}

				break;
			case 6: // After second '\r'.
				if (c != '\n') {
					return PARSE_INVALID_HEADER;
				}

				_M_state.size++;

				return PARSE_END_OF_HEADER;
		}

		if (++_M_state.size == HEADERS_MAX_LEN) {
			return PARSE_HEADERS_TOO_LARGE;
		}
	}

	return PARSE_NOT_END_OF_HEADER;
}

bool net::internet::http::headers::serialize(string::buffer& buf) const
{
	for (size_t i = 0; i < _M_used; i++) {
		const struct header* h = &_M_headers[i];

		if (h->name.value != header_name::UNKNOWN) {
			if (!buf.append(h->name.name, h->name.len)) {
				return false;
			}
		} else {
			if (!buf.append(_M_buf.data() + h->name.off, h->name.len)) {
				return false;
			}
		}

		if ((!buf.append(": ", 2)) || (!buf.append(_M_buf.data() + h->value.off, h->value.len)) || (!buf.append("\r\n", 2))) {
			return false;
		}
	}

	return buf.append("\r\n", 2);
}

unsigned char net::internet::http::headers::search_standard_header(const char* name, size_t len)
{
	int i = 0;
	int j = ARRAY_SIZE(_M_standard_headers) - 1;

	while (i <= j) {
		int pivot = (i + j) / 2;
		int ret = strncasecmp(name, _M_standard_headers[pivot].name, len);
		if (ret < 0) {
			j = pivot - 1;
		} else if (ret == 0) {
			if (len < _M_standard_headers[pivot].len) {
				j = pivot - 1;
			} else if (len == _M_standard_headers[pivot].len) {
				return pivot;
			} else {
				i = pivot + 1;
			}
		} else {
			i = pivot + 1;
		}
	}

	return header_name::UNKNOWN;
}

bool net::internet::http::headers::search(unsigned char header, unsigned& pos) const
{
	header_type type = _M_standard_headers[header].type;

	bool ret = false;

	for (pos = 0; pos < _M_used; pos++) {
		const struct header* h = &_M_headers[pos];

		if (header == h->name.value) {
			ret = true;
		} else if (ret) {
			pos--;
			return true;
		} else if ((h->name.value == header_name::UNKNOWN) || ((int) type < (int) _M_standard_headers[h->name.value].type)) {
			return false;
		}
	}

	if (ret) {
		pos--;
		return true;
	} else {
		return false;
	}
}

bool net::internet::http::headers::search(const char* name, size_t len, unsigned& pos) const
{
	bool ret = false;

	for (pos = 0; pos < _M_used; pos++) {
		const struct header* h = &_M_headers[pos];

		if (h->name.value != header_name::UNKNOWN) {
			continue;
		}

		if ((len == h->name.len) && (strncasecmp(name, _M_buf.data() + h->name.off, len) == 0)) {
			ret = true;
		} else if (ret) {
			pos--;
			return true;
		}
	}

	if (ret) {
		pos--;
		return true;
	} else {
		return false;
	}
}

bool net::internet::http::headers::allocate()
{
	if (_M_used == _M_size) {
		size_t size = (_M_size == 0) ? HEADER_ALLOC : (_M_size * 2);

		struct header* headers;
		if ((headers = (struct header*) realloc(_M_headers, size * sizeof(struct header))) == NULL) {
			return false;
		}

		_M_headers = headers;
		_M_size = size;
	}

	return true;
}

int net::internet::http::headers::clean(const header_value& value, char* dest) const
{
	const char* ptr = value.value;
	const char* end = ptr + value.len;
	char* d = dest;

	int state = 0;

	while (ptr < end) {
		unsigned char c = (unsigned char) *ptr++;

		if (state == 0) {
			if (c == '\r') {
				state = 2; // After '\r'.
			} else if (c == '\n') {
				state = 3; // After '\n'.
			} else if (header_value::valid_character(c)) {
				if ((c == ' ') || (c == '\t')) {
					state = 1; // After space.
				} else {
					*d++ = c;
				}
			} else {
				return -1;
			}
		} else if (state == 1) {
			// After space.
			if (c == '\r') {
				state = 2; // After '\r'.
			} else if (c == '\n') {
				state = 3; // After '\n'.
			} else if (header_value::valid_character(c)) {
				if ((c != ' ') && (c != '\t')) {
					// If not the first character.
					if (d > dest) {
						*d++ = ' ';
					}

					*d++ = c;

					state = 0;
				}
			} else {
				return -1;
			}
		} else if (state == 2) {
			// After '\r'.
			if (c != '\n') {
				return -1;
			}

			state = 3; // After '\n'.
		} else {
			// After '\n'.
			if ((c != ' ') && (c != '\t')) {
				return -1;
			}

			state = 1; // After space.
		}
	}

	if (d == dest) {
		return -1;
	}

	*d = 0;

	return d - dest;
}
