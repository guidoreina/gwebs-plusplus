#include <stdlib.h>
#include "net/internet/url.h"

bool net::internet::url::encode(const char* url, size_t len, string::buffer& buf)
{
	if (!buf.allocate(len * 3)) {
		return false;
	}

	const char* end = url + len;

	char* d = buf.data() + buf.count();
	char* dest = d;

	while (url < end) {
		unsigned char c = (unsigned char) *url++;

		if ((is_unreserved(c)) || (c == '/')) {
			*dest++ = c;
		} else {
			*dest++ = '%';

			unsigned char n;
			if ((n = c / 16) < 10) {
				*dest++ = '0' + n;
			} else {
				*dest++ = 'a' + (n - 10);
			}

			if ((n = c % 16) < 10) {
				*dest++ = '0' + n;
			} else {
				*dest++ = 'a' + (n - 10);
			}
		}
	}

	buf.increment_count(dest - d);

	return true;
}
