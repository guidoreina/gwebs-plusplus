#ifndef URL_H
#define URL_H

#include "string/buffer.h"
#include "macros/macros.h"

namespace net {
	namespace internet {
		bool is_unreserved(unsigned char c);
		bool is_reserved(unsigned char c);
		bool is_gen_delim(unsigned char c);
		bool is_sub_delim(unsigned char c);

		class url {
			public:
				// Encode.
				static bool encode(const char* url, size_t len, string::buffer& buf);
		};

		inline bool is_unreserved(unsigned char c)
		{
			if ((IS_ALPHA(c)) || (IS_DIGIT(c))) {
				return true;
			} else {
				switch (c) {
					case '-':
					case '.':
					case '_':
					case '~':
						return true;
					default:
						return false;
				}
			}
		}

		inline bool is_reserved(unsigned char c)
		{
			return ((is_gen_delim(c)) || (is_sub_delim(c)));
		}

		inline bool is_gen_delim(unsigned char c)
		{
			switch (c) {
				case ':':
				case '/':
				case '?':
				case '#':
				case '[':
				case ']':
				case '@':
					return true;
				default:
					return false;
			}
		}

		inline bool is_sub_delim(unsigned char c)
		{
			switch (c) {
				case '!':
				case '$':
				case '&':
				case '\'':
				case '(':
				case ')':
				case '*':
				case '+':
				case ',':
				case ';':
				case '=':
					return true;
				default:
					return false;
			}
		}
	}
}

#endif // URL_H
