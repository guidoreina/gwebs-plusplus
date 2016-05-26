#include <stdlib.h>
#include <time.h>
#include "net/internet/http/dirlisting.h"
#include "net/internet/url.h"
#include "html/html.h"
#include "constants/months_and_days.h"

bool net::internet::http::dirlisting::build(const char* dir, unsigned short dirlen, string::buffer& buf)
{
	if (_M_rootlen + dirlen >= PATH_MAX) {
		return false;
	}

	memcpy(_M_root + _M_rootlen, dir, dirlen);

	unsigned short len = _M_rootlen + dirlen;
	_M_root[len] = 0;

	_M_directory.reset();
	if (!_M_directory.read(_M_root, len, _M_criteria, _M_order)) {
		return false;
	}

#define FIRST "<?xml version=\"1.0\" encoding=\"UTF-8\"?><!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\"><html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"en\"><head><title>Index of "
#define SECOND "</title></head><body><h1>Index of "
#define THIRD "</h1><pre>Name"

	if (!buf.append(FIRST, sizeof(FIRST) - 1)) {
		return false;
	}

	size_t offset = buf.length();

	size_t count;
	if (!html::encode(dir, dirlen, dirlen, buf, count)) {
		return false;
	}

	if (!buf.append(SECOND, sizeof(SECOND) - 1)) {
		return false;
	}

	if (!buf.allocate(count)) {
		return false;
	}

	memcpy(buf.end(), buf.data() + offset, count);
	buf.increment_length(count);

	if (!buf.append(THIRD, sizeof(THIRD) - 1)) {
		return false;
	}

	unsigned nspaces = WIDTH_OF_NAME_COLUMN - 4;
	if (!buf.format("%*s    Last modified        Size</pre><hr/><pre>\n", nspaces, " ")) {
		return false;
	}

	// If not the root directory...
	if (dirlen > 1) {
		const char* slash = dir + dirlen - 1;

		do {
			if (*(slash - 1) == '/') {
				break;
			}

			slash--;
		} while (slash > dir);

		if (!buf.append("<a href=\"", 9)) {
			return false;
		}

		if (!url::encode(dir, slash - dir, buf)) {
			return false;
		}

		if (!buf.append("\">Parent directory</a>\n", 23)) {
			return false;
		}
	}

	// For each directory...
	const fs::directory::entry* entry;
	for (unsigned i = 0; ((entry = _M_directory.get_directory(i)) != NULL); i++) {
		if (!buf.append("<a href=\"", 9)) {
			return false;
		}

		if (!url::encode(entry->name, entry->namelen, buf)) {
			return false;
		}

		if (!buf.append("/\">", 3)) {
			return false;
		}

		if (entry->utf8len + 1 > WIDTH_OF_NAME_COLUMN) {
			if (!html::encode(entry->name, entry->namelen, WIDTH_OF_NAME_COLUMN - 4, buf)) {
				return false;
			}

			if (!buf.append(".../</a>", 8)) {
				return false;
			}
		} else {
			if (!html::encode(entry->name, entry->namelen, entry->utf8len, buf)) {
				return false;
			}

			if (entry->utf8len + 1 < WIDTH_OF_NAME_COLUMN) {
				if (!buf.format("/</a>%*s", WIDTH_OF_NAME_COLUMN - entry->utf8len - 1, " ")) {
					return false;
				}
			} else {
				if (!buf.append("/</a>", 5)) {
					return false;
				}
			}
		}

		struct tm stm;
		gmtime_r(&entry->mtime, &stm);

		if (!buf.format("    %02d-%s-%04d %02d:%02d     -\n", stm.tm_mday, constants::months[stm.tm_mon], 1900 + stm.tm_year, stm.tm_hour, stm.tm_min)) {
			return false;
		}
	}

	// For each file...
	for (unsigned i = 0; ((entry = _M_directory.get_file(i)) != NULL); i++) {
		if (!buf.append("<a href=\"", 9)) {
			return false;
		}

		if (!url::encode(entry->name, entry->namelen, buf)) {
			return false;
		}

		if (!buf.append("\">", 2)) {
			return false;
		}

		if (entry->utf8len > WIDTH_OF_NAME_COLUMN) {
			if (!html::encode(entry->name, entry->namelen, WIDTH_OF_NAME_COLUMN - 3, buf)) {
				return false;
			}

			if (!buf.append("...</a>", 7)) {
				return false;
			}
		} else {
			if (!html::encode(entry->name, entry->namelen, entry->utf8len, buf)) {
				return false;
			}

			if (entry->utf8len < WIDTH_OF_NAME_COLUMN) {
				if (!buf.format("</a>%*s", WIDTH_OF_NAME_COLUMN - entry->utf8len, " ")) {
					return false;
				}
			} else {
				if (!buf.append("</a>", 4)) {
					return false;
				}
			}
		}

		struct tm stm;
		gmtime_r(&entry->mtime, &stm);

		if (!buf.format("    %02d-%s-%04d %02d:%02d    ", stm.tm_mday, constants::months[stm.tm_mon], 1900 + stm.tm_year, stm.tm_hour, stm.tm_min)) {
			return false;
		}

		if (_M_show_exact_size) {
			if (!buf.format("%lld", entry->size)) {
				return false;
			}
		} else {
			if (entry->size <= 1024) {
				if (!buf.format("%lld\n", entry->size)) {
					return false;
				}
			} else if (entry->size <= 1024 * 1024) {
				if (!buf.format("%.01fK\n", (float) entry->size / (float) (1024.0))) {
					return false;
				}
			} else if (entry->size <= (off_t) 1024 * 1024 * 1024) {
				if (!buf.format("%.01fM\n", (float) entry->size / (float) (1024.0 * 1024.0))) {
					return false;
				}
			} else {
				if (!buf.format("%.01fG\n", (float) entry->size / (float) (1024.0 * 1024.0 * 1024.0))) {
					return false;
				}
			}
		}
	}

	if (!buf.append("</pre><hr/>", 11)) {
		return false;
	}

	if (!_M_footer.empty()) {
		if (!buf.append(_M_footer.data(), _M_footer.length())) {
			return false;
		}
	}

	return buf.append("</body></html>", 14);
}
