#ifndef DIRLISTING_H
#define DIRLISTING_H

#include <string.h>
#include <limits.h>
#include "fs/directory.h"
#include "fs/file.h"
#include "string/buffer.h"

namespace net {
	namespace internet {
		namespace http {
			class dirlisting {
				public:
					// Constructor.
					dirlisting();

					// Destructor.
					~dirlisting();

					// Free.
					void free();

					// Set root directory.
					bool root_directory(const char* root, unsigned short len);

					// Set sort criteria.
					void sort_criteria(fs::directory::sort_criteria criteria);

					// Set sort order.
					void sort_order(fs::directory::sort_order order);

					// Show files with exact size?
					void show_exact_size(bool value);

					// Load footer.
					bool load_footer(const char* filename);

					// Build.
					bool build(const char* dir, unsigned short dirlen, string::buffer& buf);

				private:
					static const unsigned short WIDTH_OF_NAME_COLUMN = 32;

					static const size_t FOOTER_MAX_SIZE = 32 * 1024;

					fs::directory _M_directory;

					char _M_root[PATH_MAX + 1];
					unsigned short _M_rootlen;

					fs::directory::sort_criteria _M_criteria;
					fs::directory::sort_order _M_order;

					bool _M_show_exact_size;

					string::buffer _M_footer;
			};

			inline dirlisting::dirlisting()
			{
				_M_rootlen = 0;

				_M_criteria = fs::directory::SORT_BY_NAME;
				_M_order = fs::directory::ASCENDING;

				_M_show_exact_size = false;
			}

			inline dirlisting::~dirlisting()
			{
			}

			inline void dirlisting::free()
			{
				_M_directory.free();
				_M_footer.free();
			}

			inline bool dirlisting::root_directory(const char* root, unsigned short len)
			{
				if (len >= sizeof(_M_root)) {
					return false;
				}

				if (root[len - 1] == '/') {
					len--;
				}

				memcpy(_M_root, root, len);
				_M_rootlen = len;

				return true;
			}

			inline void dirlisting::sort_criteria(fs::directory::sort_criteria criteria)
			{
				_M_criteria = criteria;
			}

			inline void dirlisting::sort_order(fs::directory::sort_order order)
			{
				_M_order = order;
			}

			inline void dirlisting::show_exact_size(bool value)
			{
				_M_show_exact_size = value;
			}

			inline bool dirlisting::load_footer(const char* filename)
			{
				return fs::file::read_all(filename, _M_footer, FOOTER_MAX_SIZE);
			}
		}
	}
}

#endif // DIRLISTING_H
