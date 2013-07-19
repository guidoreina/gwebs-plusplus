#ifndef SCHEME_H
#define SCHEME_H

// Subset of the schemes.
// http://www.iana.org/assignments/uri-schemes.html

#include "macros/macros.h"

namespace net {
	namespace internet {
		struct scheme {
			public:
				static const size_t MAX_LEN = 32;

				static const unsigned char FILE    = 0;
				static const unsigned char FTP     = 1;
				static const unsigned char HTTP    = 2;
				static const unsigned char HTTPS   = 3;
				static const unsigned char ICAP    = 4;
				static const unsigned char IMAP    = 5;
				static const unsigned char LDAP    = 6;
				static const unsigned char MAILTO  = 7;
				static const unsigned char NEWS    = 8;
				static const unsigned char NFS     = 9;
				static const unsigned char POP     = 10;
				static const unsigned char TELNET  = 11;
				static const unsigned char UNKNOWN = 12;

				unsigned char value;

				// Constructor.
				scheme();
				scheme(unsigned char v);

				const char* name() const;

				bool operator==(unsigned char v) const;
				bool operator!=(unsigned char v) const;

				// Search.
				static scheme search(const char* name, size_t len);

				// Valid scheme character?
				static bool valid_character(unsigned char c);

			private:
				struct _scheme {
					const char* name;
					size_t len;
				};

				static const struct _scheme _M_schemes[];
		};

		inline scheme::scheme()
		{
		}

		inline scheme::scheme(unsigned char v)
		{
			value = v;
		}

		inline const char* scheme::name() const
		{
			if (value >= UNKNOWN) {
				return NULL;
			}

			return _M_schemes[value].name;
		}

		inline bool scheme::operator==(unsigned char v) const
		{
			return (v == value);
		}

		inline bool scheme::operator!=(unsigned char v) const
		{
			return (v != value);
		}

		inline bool scheme::valid_character(unsigned char c)
		{
			if ((IS_ALPHA(c)) || (IS_DIGIT(c))) {
				return true;
			} else {
				switch (c) {
					case '+':
					case '-':
					case '.':
						return true;
					default:
						return false;
				}
			}
		}
	}
}

#endif // SCHEME_H
