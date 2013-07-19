#ifndef MIME_TYPES_H
#define MIME_TYPES_H

#include <stdlib.h>
#include "string/buffer.h"

namespace net {
	namespace internet {
		namespace mime {
			class types {
				public:
					static const char* DEFAULT_FILE;

					static const char* DEFAULT_MIME_TYPE;
					static const unsigned short DEFAULT_MIME_TYPE_LEN = 24;

					// Constructor.
					types();

					// Destructor.
					~types();

					// Free.
					void free();

					// Load.
					bool load(const char* filename = DEFAULT_FILE);

					// Get MIME type.
					const char* mime_type(const char* extension, unsigned short extensionlen, unsigned short& len) const;

				private:
					static const size_t EXTENSION_ALLOC = 256;

					string::buffer _M_buf;

					struct extension {
						size_t ext;
						size_t type;

						unsigned short extlen;
						unsigned short typelen;
					};

					struct extension* _M_extensions;
					size_t _M_size;
					size_t _M_used;

					// Add.
					bool add(size_t type, unsigned short typelen, const char* extension, unsigned short extensionlen);

					// Search.
					bool search(const char* extension, unsigned short extensionlen, size_t& pos) const;
			};

			inline types::types() : _M_buf(16 * 1024)
			{
				_M_extensions = NULL;
				_M_size = 0;
				_M_used = 0;
			}

			inline types::~types()
			{
				free();
			}

			inline void types::free()
			{
				_M_buf.free();

				if (_M_extensions) {
					::free(_M_extensions);
					_M_extensions = NULL;
				}

				_M_size = 0;
				_M_used = 0;
			}

			inline const char* types::mime_type(const char* extension, unsigned short extensionlen, unsigned short& len) const
			{
				size_t pos;
				if (!search(extension, extensionlen, pos)) {
					len = DEFAULT_MIME_TYPE_LEN;
					return DEFAULT_MIME_TYPE;
				}

				len = _M_extensions[pos].typelen;
				return (_M_buf.data() + _M_extensions[pos].type);
			}
		}
	}
}

#endif // MIME_TYPES_H
