#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>
#include "fs/directory.h"
#include "string/utf8.h"

void fs::directory::free()
{
	_M_buf.free();

	if (_M_entries) {
		::free(_M_entries);
		_M_entries = NULL;
	}

	_M_size = 0;
	_M_used = 0;

	if (_M_files) {
		::free(_M_files);
		_M_files = NULL;
	}

	_M_nfiles = 0;

	if (_M_directories) {
		::free(_M_directories);
		_M_directories = NULL;
	}

	_M_ndirectories = 0;
}

bool fs::directory::read(const char* name, size_t namelen, sort_criteria criteria, sort_order order)
{
	DIR* dir;
	if ((dir = opendir(name)) == NULL) {
		return false;
	}

	_M_criteria = criteria;
	_M_order = order;

	// The caller has already checked that 'namelen' < 'PATH_MAX'.
	char path[PATH_MAX + 1];
	memcpy(path, name, namelen);

	int left = sizeof(path) - namelen;

	do {
		struct dirent e;
		struct dirent* res;
		if (readdir_r(dir, &e, &res) != 0) {
			closedir(dir);
			return false;
		}

		if (!res) {
			break;
		}

		if (e.d_name[0] == '.') {
			continue;
		}

		if (snprintf(path + namelen, left, "%s", e.d_name) >= left) {
			continue;
		}

		struct stat buf;
		if (stat(path, &buf) < 0) {
			continue;
		}

		if (S_ISREG(buf.st_mode)) {
			size_t len;
			size_t utf8len;
			if (!string::utf8::lengths(e.d_name, len, utf8len)) {
				continue;
			}

			if (!add(FILE_ENTRY, e.d_name, len, utf8len, buf.st_size, buf.st_mtime)) {
				closedir(dir);
				return false;
			}
		} else if (S_ISDIR(buf.st_mode)) {
			size_t len;
			size_t utf8len;
			if (!string::utf8::lengths(e.d_name, len, utf8len)) {
				continue;
			}

			if (!add(DIRECTORY_ENTRY, e.d_name, len, utf8len, buf.st_size, buf.st_mtime)) {
				closedir(dir);
				return false;
			}
		}
	} while (true);

	closedir(dir);

	// Set pointers.
	for (size_t i = 0; i < _M_used; i++) {
		_M_entries[i].name = _M_buf.data() + _M_entries[i].offset;
	}

	return true;
}

bool fs::directory::add(entry_type type, const char* name, unsigned short namelen, unsigned short utf8len, off_t size, time_t mtime)
{
	size_t pos;
	if (type == FILE_ENTRY) {
		if (search_file(name, utf8len, size, mtime, pos)) {
			// Already inserted (should not happen).
			return true;
		}
	} else {
		if (search_directory(name, utf8len, size, mtime, pos)) {
			// Already inserted (should not happen).
			return true;
		}
	}

	size_t offset = _M_buf.count();

	if (!_M_buf.append(name, namelen + 1)) {
		return false;
	}

	if (!allocate()) {
		return false;
	}

	struct entry* entry = &_M_entries[_M_used];

	entry->type = type;
	entry->namelen = namelen;
	entry->utf8len = utf8len;
	entry->size = size;
	entry->mtime = mtime;
	entry->offset = offset;

	if (type == FILE_ENTRY) {
		if (pos < _M_nfiles) {
			memmove(&_M_files[pos + 1], &_M_files[pos], (_M_nfiles - pos) * sizeof(size_t));
		}

		_M_files[pos] = _M_used;
		_M_nfiles++;
	} else {
		if (pos < _M_ndirectories) {
			memmove(&_M_directories[pos + 1], &_M_directories[pos], (_M_ndirectories - pos) * sizeof(size_t));
		}

		_M_directories[pos] = _M_used;
		_M_ndirectories++;
	}

	_M_used++;

	return true;
}

bool fs::directory::search(size_t* entries, size_t count, const char* name, unsigned short utf8len, off_t size, time_t mtime, size_t& pos) const
{
	int i = 0;
	int j = count - 1;

	while (i <= j) {
		int pivot = (i + j) / 2;

		off_t ret;
		switch (_M_criteria) {
			case SORT_BY_NAME:
				ret = strcasecmp(name, _M_buf.data() + _M_entries[entries[pivot]].offset);
				break;
			case SORT_BY_NAMELEN:
				ret = utf8len - _M_entries[entries[pivot]].utf8len;
				break;
			case SORT_BY_SIZE:
				ret = size - _M_entries[entries[pivot]].size;
				break;
			case SORT_BY_TIME:
				ret = mtime - _M_entries[entries[pivot]].mtime;
				break;
		}

		if ((ret *= _M_order) < 0) {
			j = pivot - 1;
		} else if (ret == 0) {
			pos = (size_t) pivot;
			return true;
		} else {
			i = pivot + 1;
		}
	}

	pos = (size_t) i;

	return false;
}

bool fs::directory::allocate()
{
	if (_M_used < _M_size) {
		return true;
	}

	size_t size = (_M_size == 0) ? ENTRY_ALLOC : _M_size * 2;

	struct entry* entries;
	if ((entries = (struct entry*) malloc(size * sizeof(struct entry))) == NULL) {
		return false;
	}

	size_t* files;
	if ((files = (size_t*) malloc(size * sizeof(size_t))) == NULL) {
		::free(entries);
		return false;
	}

	size_t* directories;
	if ((directories = (size_t*) realloc(_M_directories, size * sizeof(size_t))) == NULL) {
		::free(files);
		::free(entries);

		return false;
	}

	if (_M_entries) {
		memcpy(entries, _M_entries, _M_used * sizeof(struct entry));
		::free(_M_entries);

		if (_M_nfiles > 0) {
			memcpy(files, _M_files, _M_nfiles * sizeof(size_t));
		}

		::free(_M_files);
	}

	_M_entries = entries;
	_M_files = files;
	_M_directories = directories;

	_M_size = size;

	return true;
}
