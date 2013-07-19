#include <stdlib.h>
#include "html/html.h"

bool html::encode(const char* string, size_t len, size_t max_utf8len, string::buffer& buf, size_t& count)
{
	if (!buf.allocate(len * 6)) {
		return false;
	}

	const char* end = string + len;

	char* d = buf.data() + buf.count();
	char* dest = d;

	size_t utf8len = 0;
	unsigned left = 0;

	int state = 0;

	while ((string < end) && (utf8len < max_utf8len)) {
		unsigned char c = (unsigned char) *string++;

		if (state == 0) {
			switch (c) {
				case '<':
					*dest++ = '&';
					*dest++ = 'l';
					*dest++ = 't';
					*dest++ = ';';

					utf8len++;
					break;
				case '>':
					*dest++ = '&';
					*dest++ = 'g';
					*dest++ = 't';
					*dest++ = ';';

					utf8len++;
					break;
				case '&':
					*dest++ = '&';
					*dest++ = 'a';
					*dest++ = 'm';
					*dest++ = 'p';
					*dest++ = ';';

					utf8len++;
					break;
				case '"':
					*dest++ = '&';
					*dest++ = 'q';
					*dest++ = 'u';
					*dest++ = 'o';
					*dest++ = 't';
					*dest++ = ';';

					utf8len++;
					break;
				case '\'':
					*dest++ = '&';
					*dest++ = '#';
					*dest++ = '3';
					*dest++ = '9';
					*dest++ = ';';

					utf8len++;
					break;
				default:
					if ((c & 0x80) == 0) {
						utf8len++;
					} else if ((c & 0xe0) == 0xc0) {
						left = 1;
						state = 1;
					} else if ((c & 0xf0) == 0xe0) {
						left = 2;
						state = 1;
					} else if ((c & 0xf8) == 0xf0) {
						left = 3;
						state = 1;
					} else {
						return false;
					}

					*dest++ = c;
			}
		} else {
			if ((c & 0xc0) != 0x80) {
				return false;
			}

			if (--left == 0) {
				utf8len++;
				state = 0;
			}

			*dest++ = c;
		}
	}

	count = dest - d;

	buf.increment_count(count);

	return true;
}
