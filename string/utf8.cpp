#include <stdlib.h>
#include "string/utf8.h"

bool string::utf8::lengths(const char* s, size_t& len, size_t& utf8len)
{
	const char* ptr = s;
	unsigned left = 0;

	int state = 0;

	utf8len = 0;

	unsigned char c;
	while ((c = (unsigned char) *ptr) != 0) {
		if (state == 0) {
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
		} else {
			if ((c & 0xc0) != 0x80) {
				return false;
			}

			if (--left == 0) {
				utf8len++;
				state = 0;
			}
		}

		ptr++;
	}

	len = ptr - s;

	return true;
}

bool string::utf8::length(const char* s, size_t len, size_t& utf8len)
{
	const char* end = s + len;
	unsigned left = 0;

	int state = 0;

	utf8len = 0;

	while (s < end) {
		unsigned char c = (unsigned char) *s++;
		if (state == 0) {
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
		} else {
			if ((c & 0xc0) != 0x80) {
				return false;
			}

			if (--left == 0) {
				utf8len++;
				state = 0;
			}
		}
	}

	return true;
}
