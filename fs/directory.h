#ifndef DIRECTORY_H
#define DIRECTORY_H

#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include "string/buffer.h"

namespace fs {
	class directory {
		public:
			enum entry_type {
				FILE_ENTRY,
				DIRECTORY_ENTRY
			};

			enum sort_criteria {
				SORT_BY_NAME,
				SORT_BY_NAMELEN,
				SORT_BY_SIZE,
				SORT_BY_TIME
			};

			enum sort_order {
				ASCENDING = 1,
				DESCENDING = -1
			};

			struct entry {
				friend class directory;

				public:
					entry_type type;

					const char* name;
					unsigned short namelen;
					unsigned short utf8len;

					off_t size;
					time_t mtime;

				private:
					size_t offset;
			};

			// Constructor.
			directory();

			// Destructor.
			~directory();

			// Free.
			void free();

			// Reset.
			void reset();

			// Read.
			bool read(const char* name, size_t namelen, sort_criteria criteria = SORT_BY_NAME, sort_order order = ASCENDING);

			// Get number of files.
			size_t get_file_count() const;

			// Get number of directories.
			size_t get_directory_count() const;

			// Get file entry.
			const struct entry* get_file(unsigned i) const;

			// Get directory entry.
			const struct entry* get_directory(unsigned i) const;

		private:
			static const size_t ENTRY_ALLOC = 32;

			string::buffer _M_buf;

			struct entry* _M_entries;
			size_t _M_size;
			size_t _M_used;

			size_t* _M_files;
			size_t _M_nfiles;

			size_t* _M_directories;
			size_t _M_ndirectories;

			sort_criteria _M_criteria;
			sort_order _M_order;

			// Add.
			bool add(entry_type type, const char* name, unsigned short namelen, unsigned short utf8len, off_t size, time_t mtime);

			// Search file.
			bool search_file(const char* name, unsigned short utf8len, off_t size, time_t mtime, size_t& pos) const;

			// Search directory.
			bool search_directory(const char* name, unsigned short utf8len, off_t size, time_t mtime, size_t& pos) const;

			// Search.
			bool search(size_t* entries, size_t count, const char* name, unsigned short utf8len, off_t size, time_t mtime, size_t& pos) const;

			// Allocate.
			bool allocate();
	};

	inline directory::directory() : _M_buf(512)
	{
		_M_entries = NULL;
		_M_size = 0;
		_M_used = 0;

		_M_files = NULL;
		_M_nfiles = 0;

		_M_directories = NULL;
		_M_ndirectories = 0;
	}

	inline directory::~directory()
	{
		free();
	}

	inline void directory::reset()
	{
		_M_buf.reset();
		_M_used = 0;
		_M_nfiles = 0;
		_M_ndirectories = 0;
	}

	inline size_t directory::get_file_count() const
	{
		return _M_nfiles;
	}

	inline size_t directory::get_directory_count() const
	{
		return _M_ndirectories;
	}

	inline const struct directory::entry* directory::get_file(unsigned i) const
	{
		if (i >= _M_nfiles) {
			return NULL;
		}

		return &_M_entries[_M_files[i]];
	}

	inline const struct directory::entry* directory::get_directory(unsigned i) const
	{
		if (i >= _M_ndirectories) {
			return NULL;
		}

		return &_M_entries[_M_directories[i]];
	}

	inline bool directory::search_file(const char* name, unsigned short utf8len, off_t size, time_t mtime, size_t& pos) const
	{
		return search(_M_files, _M_nfiles, name, utf8len, size, mtime, pos);
	}

	inline bool directory::search_directory(const char* name, unsigned short utf8len, off_t size, time_t mtime, size_t& pos) const
	{
		return search(_M_directories, _M_ndirectories, name, utf8len, size, mtime, pos);
	}
}

#endif // DIRECTORY_H
