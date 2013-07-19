#ifndef HTML_H
#define HTML_H

#include "string/buffer.h"

namespace html {
	// Encode.
	bool encode(const char* string, size_t len, size_t max_utf8len, string::buffer& buf);
	bool encode(const char* string, size_t len, size_t max_utf8len, string::buffer& buf, size_t& count);

	inline bool encode(const char* string, size_t len, size_t max_utf8len, string::buffer& buf)
	{
		size_t count;
		return encode(string, len, max_utf8len, buf, count);
	}
}

#endif // HTML_H
