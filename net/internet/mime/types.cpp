#include <string.h>
#include <stdio.h>
#include "net/internet/mime/types.h"
#include "macros/macros.h"

const char* net::internet::mime::types::DEFAULT_FILE = "/etc/mime.types";
const char* net::internet::mime::types::DEFAULT_MIME_TYPE = "application/octet-stream";

bool net::internet::mime::types::load(const char* filename)
{
	FILE* file;
	if ((file = fopen(filename, "r")) == NULL) {
		return false;
	}

	char line[1024];
	while (fgets(line, sizeof(line), file)) {
		const char* type = NULL;
		unsigned short typelen = 0;

		const char* extension = NULL;
		unsigned short extensionlen = 0;

		size_t off = 0;

		int state = 0; // Initial state.

		const char* ptr = line;
		unsigned char c;
		while (((c = (unsigned char) *ptr) != 0) && (state != 6)) {
			switch (state) {
				case 0: // Initial state.
					if (c == '#') {
						state = 6; // Comment.
					} else if (c > ' ') {
						type = ptr;
						typelen = 1;

						state = 1; // Parsing MIME type.
					}

					break;
				case 1: // Parsing MIME type.
					if (c > ' ') {
						typelen++;
					} else {
						state = 2; // Whitespace after MIME type.
					}

					break;
				case 2: // Whitespace after MIME type.
					if (c > ' ') {
						extension = ptr;
						extensionlen = 1;

						state = 3; // Parsing first extension.
					}

					break;
				case 3: // Parsing first extension.
					if (c > ' ') {
						extensionlen++;
					} else {
						off = _M_buf.length();

						if (!_M_buf.append(type, typelen)) {
							fclose(file);
							return false;
						}

						if (!add(off, typelen, extension, extensionlen)) {
							fclose(file);
							return false;
						}

						state = 4; // After extension.
					}

					break;
				case 4: // After extension.
					if (c > ' ') {
						extension = ptr;
						extensionlen = 1;

						state = 5; // Parsing extension.
					}

					break;
				case 5: // Parsing extension.
					if (c > ' ') {
						extensionlen++;
					} else {
						if (!add(off, typelen, extension, extensionlen)) {
							fclose(file);
							return false;
						}

						state = 4; // After extension.
					}

					break;
			}

			ptr++;
		}

		switch (state) {
			case 3:
				off = _M_buf.length();

				if (!_M_buf.append(type, typelen)) {
					fclose(file);
					return false;
				}

				if (!add(off, typelen, extension, extensionlen)) {
					fclose(file);
					return false;
				}

				break;
			case 5:
				if (!add(off, typelen, extension, extensionlen)) {
					fclose(file);
					return false;
				}

				break;
		}
	}

	fclose(file);

	return true;
}

bool net::internet::mime::types::add(size_t type, unsigned short typelen, const char* extension, unsigned short extensionlen)
{
	size_t pos;
	if (search(extension, extensionlen, pos)) {
		// Duplicated extension.
		return true;
	}

	size_t off = _M_buf.length();

	if (!_M_buf.append(extension, extensionlen)) {
		return false;
	}

	if (_M_used == _M_size) {
		size_t size = (_M_size == 0) ? EXTENSION_ALLOC : _M_size * 2;

		struct extension* extensions;
		if ((extensions = (struct extension*) realloc(_M_extensions, size * sizeof(struct extension))) == NULL) {
			return false;
		}

		_M_extensions = extensions;
		_M_size = size;
	}

	if (pos < _M_used) {
		memmove(&_M_extensions[pos + 1], &_M_extensions[pos], (_M_used - pos) * sizeof(struct extension));
	}

	struct extension* ext = &_M_extensions[pos];

	ext->ext = off;
	ext->extlen = extensionlen;

	ext->type = type;
	ext->typelen = typelen;

	_M_used++;

	return true;
}

bool net::internet::mime::types::search(const char* extension, unsigned short extensionlen, size_t& pos) const
{
	int i = 0;
	int j = _M_used - 1;

	while (i <= j) {
		int pivot = (i + j) / 2;
		int ret = strncasecmp(extension, _M_buf.data() + _M_extensions[pivot].ext, MIN(extensionlen, _M_extensions[pivot].extlen));
		if (ret < 0) {
			j = pivot - 1;
		} else if (ret == 0) {
			if (extensionlen < _M_extensions[pivot].extlen) {
				j = pivot - 1;
			} else if (extensionlen == _M_extensions[pivot].extlen) {
				pos = (size_t) pivot;
				return true;
			} else {
				i = pivot + 1;
			}
		} else {
			i = pivot + 1;
		}
	}

	pos = (size_t) i;

	return false;
}
