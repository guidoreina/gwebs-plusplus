#ifndef HEADERS_H
#define HEADERS_H

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "net/internet/http/date.h"
#include "util/number.h"
#include "util/ranges.h"
#include "string/buffer.h"
#include "constants/months_and_days.h"

namespace net {
	namespace internet {
		namespace http {
			enum header_type {
				GENERAL_HEADER,
				REQUEST_HEADER,
				RESPONSE_HEADER,
				ENTITY_HEADER
			};

			struct standard_header {
				const char* name;
				unsigned short len;

				header_type type;

				unsigned char comma_separated_list;
				unsigned char single_token;
			};

			bool is_char(unsigned char c);
			bool is_control(unsigned char c);
			bool is_separator(unsigned char c);

			struct header_name {
				friend class headers;

				public:
					static const unsigned char ACCEPT               = 0;
					static const unsigned char ACCEPT_CHARSET       = 1;
					static const unsigned char ACCEPT_ENCODING      = 2;
					static const unsigned char ACCEPT_LANGUAGE      = 3;
					static const unsigned char ACCEPT_RANGES        = 4;
					static const unsigned char AGE                  = 5;
					static const unsigned char ALLOW                = 6;
					static const unsigned char AUTHORIZATION        = 7;
					static const unsigned char CACHE_CONTROL        = 8;
					static const unsigned char CONNECTION           = 9;
					static const unsigned char CONTENT_ENCODING     = 10;
					static const unsigned char CONTENT_LANGUAGE     = 11;
					static const unsigned char CONTENT_LENGTH       = 12;
					static const unsigned char CONTENT_LOCATION     = 13;
					static const unsigned char CONTENT_MD5          = 14;
					static const unsigned char CONTENT_RANGE        = 15;
					static const unsigned char CONTENT_TYPE         = 16;
					static const unsigned char COOKIE               = 17;
					static const unsigned char DATE                 = 18;
					static const unsigned char ETAG                 = 19;
					static const unsigned char EXPECT               = 20;
					static const unsigned char EXPIRES              = 21;
					static const unsigned char FROM                 = 22;
					static const unsigned char HOST                 = 23;
					static const unsigned char IF_MATCH             = 24;
					static const unsigned char IF_MODIFIED_SINCE    = 25;
					static const unsigned char IF_NONE_MATCH        = 26;
					static const unsigned char IF_RANGE             = 27;
					static const unsigned char IF_UNMODIFIED_SINCE  = 28;
					static const unsigned char KEEP_ALIVE           = 29;
					static const unsigned char LAST_MODIFIED        = 30;
					static const unsigned char LOCATION             = 31;
					static const unsigned char MAX_FORWARDS         = 32;
					static const unsigned char PRAGMA               = 33;
					static const unsigned char PROXY_AUTHENTICATE   = 34;
					static const unsigned char PROXY_AUTHORIZATION  = 35;
					static const unsigned char PROXY_CONNECTION     = 36;
					static const unsigned char RANGE                = 37;
					static const unsigned char REFERER              = 38;
					static const unsigned char RETRY_AFTER          = 39;
					static const unsigned char SERVER               = 40;
					static const unsigned char SET_COOKIE           = 41;
					static const unsigned char STATUS               = 42;
					static const unsigned char TE                   = 43;
					static const unsigned char TRAILER              = 44;
					static const unsigned char TRANSFER_ENCODING    = 45;
					static const unsigned char UPGRADE              = 46;
					static const unsigned char USER_AGENT           = 47;
					static const unsigned char VARY                 = 48;
					static const unsigned char VIA                  = 49;
					static const unsigned char WARNING              = 50;
					static const unsigned char WWW_AUTHENTICATE     = 51;
					static const unsigned char UNKNOWN              = 0xff;

					static const size_t MAX_LEN = 256;

					// Constructor.
					header_name();
					header_name(unsigned char v);
					header_name(const char* n, size_t l);
					header_name(unsigned char v, const char* n, size_t l);

					// Valid header name character?
					static bool valid_character(unsigned char c);

					unsigned char value;

					mutable const char* name;
					unsigned short len;

				private:
					size_t off;
			};

			struct header_value {
				friend class headers;

				public:
					// Constructor.
					header_value();
					header_value(const char* v, size_t l);

					// Valid header value character?
					static bool valid_character(unsigned char c);

					mutable const char* value;
					unsigned short len;

				private:
					size_t off;
			};

			struct header {
				header_name name;
				header_value value;
			};

			class headers {
				public:
					static const size_t HEADERS_MAX_LEN = 64 * 1024;
					static const size_t BOUNDARY_LEN = 11;
					static const unsigned MAX_RANGES = 16;

					// Constructor.
					headers(bool ignore_errors = true);

					// Destructor.
					~headers();

					// Free.
					void free();

					// Reset.
					void reset();

					// Get header.
					const struct header* get_header(unsigned idx) const;

					// Get header value.
					const header_value* get_header_value(unsigned char header) const;
					bool get_header_value(unsigned char header, time_t& t, struct tm& timestamp) const;
					bool get_header_value(unsigned char header, time_t& t) const;
					bool get_header_value(unsigned char header, unsigned& n) const;
					bool get_header_value(unsigned char header, off_t& n) const;
					const header_value* get_header_value(const char* name, size_t len) const;

					// Get ranges.
					bool get_ranges(off_t filesize, util::ranges& ranges) const;

					// Add header.
					bool add(const struct header& h);
					bool add(const header_name& name, const header_value& value);
					bool add(const header_name& name, const struct tm& tm);
					bool add(const header_name& name, time_t t);
					bool add(const header_name& name, off_t n);
					bool add(const header_name& name, size_t n);

					// Remove header.
					bool remove(unsigned char header);
					bool remove(const char* name, size_t len);

					// Parse.
					enum parse_result {
						PARSE_NO_MEMORY,
						PARSE_INVALID_HEADER,
						PARSE_HEADERS_TOO_LARGE,
						PARSE_NOT_END_OF_HEADER,
						PARSE_END_OF_HEADER
					};

					parse_result parse(const void* buf, size_t count);

					// Serialize.
					bool serialize(string::buffer& buf) const;

					// Get header size.
					size_t size() const;

				protected:
					static const size_t HEADER_ALLOC = 8;

					static const struct standard_header _M_standard_headers[];

					struct header* _M_headers;
					size_t _M_size;
					size_t _M_used;

					string::buffer _M_buf;

					struct state {
						unsigned char header;

						size_t name;
						size_t namelen;

						size_t value;
						size_t value_end;

						size_t size;

						int state;

						void reset();
					};

					state _M_state;

					bool _M_ignore_errors;

					// Search standard header.
					static unsigned char search_standard_header(const char* name, size_t len);

					// Search header.
					bool search(unsigned char header, unsigned& pos) const;
					bool search(const char* name, size_t len, unsigned& pos) const;

					// Allocate.
					bool allocate();

					// Clean.
					int clean(const header_value& value, char* dest) const;
			};

			inline bool is_char(unsigned char c)
			{
				return ((c >= 0) && (c <= 127));
			}

			inline bool is_control(unsigned char c)
			{
				return (((c >= 0) && (c <= 31)) || (c == 127));
			}

			inline bool is_separator(unsigned char c)
			{
				switch (c) {
					case '(':
					case ')':
					case '<':
					case '>':
					case '@':
					case ',':
					case ';':
					case ':':
					case '\\':
					case '"':
					case '/':
					case '[':
					case ']':
					case '?':
					case '=':
					case '{':
					case '}':
					case ' ':
					case '\t':
						return true;
					default:
						return false;
				}
			}

			inline header_name::header_name()
			{
			}

			inline header_name::header_name(unsigned char v)
			{
				value = v;
			}

			inline header_name::header_name(const char* n, size_t l)
			{
				value = UNKNOWN;

				name = n;
				len = l;
			}

			inline header_name::header_name(unsigned char v, const char* n, size_t l)
			{
				value = v;

				name = n;
				len = l;
			}

			inline bool header_name::valid_character(unsigned char c)
			{
				return ((is_char(c)) && (!is_control(c)) && (!is_separator(c)));
			}

			inline header_value::header_value()
			{
			}

			inline header_value::header_value(const char* v, size_t l)
			{
				value = v;
				len = l;
			}

			inline bool header_value::valid_character(unsigned char c)
			{
				return ((c == '\t') || (!is_control(c)));
			}

			inline headers::headers(bool ignore_errors) : _M_buf(512)
			{
				_M_headers = NULL;
				_M_size = 0;
				_M_used = 0;

				_M_state.reset();

				_M_ignore_errors = ignore_errors;
			}

			inline headers::~headers()
			{
				free();
			}

			inline void headers::free()
			{
				if (_M_headers) {
					::free(_M_headers);
					_M_headers = NULL;
				}

				_M_size = 0;
				_M_used = 0;

				_M_buf.free();

				_M_state.reset();
			}

			inline void headers::reset()
			{
				_M_used = 0;
				_M_buf.reset();

				_M_state.reset();
			}

			inline const struct header* headers::get_header(unsigned idx) const
			{
				if (idx >= _M_used) {
					return NULL;
				}

				const struct header* h = &_M_headers[idx];

				if (h->name.value == header_name::UNKNOWN) {
					h->name.name = _M_buf.data() + h->name.off;
				}

				h->value.value = _M_buf.data() + h->value.off;

				return h;
			}

			inline const header_value* headers::get_header_value(unsigned char header) const
			{
				unsigned pos;
				if (!search(header, pos)) {
					return NULL;
				}

				const struct header* h = &_M_headers[pos];
				h->value.value = _M_buf.data() + h->value.off;

				return &h->value;
			}

			inline bool headers::get_header_value(unsigned char header, time_t& t, struct tm& timestamp) const
			{
				const header_value* value;
				if ((value = get_header_value(header)) == NULL) {
					return false;
				}

				return ((t = date::parse(value->value, value->len, timestamp)) != (time_t) -1);
			}

			inline bool headers::get_header_value(unsigned char header, time_t& t) const
			{
				const header_value* value;
				if ((value = get_header_value(header)) == NULL) {
					return false;
				}

				struct tm timestamp;
				return ((t = date::parse(value->value, value->len, timestamp)) != (time_t) -1);
			}

			inline bool headers::get_header_value(unsigned char header, unsigned& n) const
			{
				const header_value* value;
				if ((value = get_header_value(header)) == NULL) {
					return false;
				}

				return (util::number::parse(value->value, value->len, n) == util::number::PARSE_SUCCEEDED);
			}

			inline bool headers::get_header_value(unsigned char header, off_t& n) const
			{
				const header_value* value;
				if ((value = get_header_value(header)) == NULL) {
					return false;
				}

				return (util::number::parse(value->value, value->len, n) == util::number::PARSE_SUCCEEDED);
			}

			inline const header_value* headers::get_header_value(const char* name, size_t len) const
			{
				unsigned pos;
				if (!search(name, len, pos)) {
					return NULL;
				}

				const struct header* h = &_M_headers[pos];
				h->value.value = _M_buf.data() + h->value.off;

				return &h->value;
			}

			inline bool headers::add(const struct header& h)
			{
				return add(h.name, h.value);
			}

			inline bool headers::add(const header_name& name, const struct tm& tm)
			{
				char buf[32];

				header_value value;
				value.value = buf;
				value.len = snprintf(buf, sizeof(buf), "%s, %02u %s %u %02u:%02u:%02u GMT", constants::days[tm.tm_wday], tm.tm_mday, constants::months[tm.tm_mon], 1900 + tm.tm_year, tm.tm_hour, tm.tm_min, tm.tm_sec);

				return add(name, value);
			}

			inline bool headers::add(const header_name& name, time_t t)
			{
				struct tm tm;
				gmtime_r(&t, &tm);

				return add(name, tm);
			}

			inline bool headers::add(const header_name& name, off_t n)
			{
				char buf[32];

				header_value value;
				value.value = buf;
				value.len = snprintf(buf, sizeof(buf), "%lld", n);

				return add(name, value);
			}

			inline bool headers::add(const header_name& name, size_t n)
			{
				char buf[32];

				header_value value;
				value.value = buf;
				value.len = snprintf(buf, sizeof(buf), "%lu", n);

				return add(name, value);
			}

			inline size_t headers::size() const
			{
				return _M_state.size;
			}

			inline void headers::state::reset()
			{
				namelen = 0;
				value_end = 0;
				size = 0;
				state = 0;
			}
		}
	}
}

#endif // HEADERS_H
