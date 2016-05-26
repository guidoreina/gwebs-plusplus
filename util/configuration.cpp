#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include "util/configuration.h"
#include "fs/file.h"

bool util::configuration::load(const char* filename)
{
	// Check that the file exists.
	struct stat status;
	if (stat(filename, &status) < 0) {
		fprintf(stderr, "[util::configuration::load] File %s not found.\n", filename);
		return false;
	}

	// If not a regular file...
	if (!S_ISREG(status.st_mode)) {
		fprintf(stderr, "[util::configuration::load] File %s is not a regular file.\n", filename);
		return false;
	}

	// If the file is too big...
	if (status.st_size > MAX_FILE_SIZE) {
		fprintf(stderr, "[util::configuration::load] File %s is too big (%lld bytes).\n", filename, status.st_size);
		return false;
	}

	// If the file is empty...
	if (status.st_size == 0) {
		return true;
	}

	// Open file for reading.
	fs::file file;
	if (!file.open(filename, O_RDONLY)) {
		fprintf(stderr, "[util::configuration::load] Couldn't open file %s.\n", filename);
		return false;
	}

	// Allocate memory for the keys and values.
	if (!_M_buf.allocate(status.st_size)) {
		fprintf(stderr, "[util::configuration::load] Couldn't allocate memory.\n");

		file.close();
		return false;
	}

	// Start parsing.
	begin_parsing();

	off_t left = status.st_size;

	do {
		char buf[8 * 1024];
		ssize_t ret;
		if ((ret = file.read(buf, MIN(left, (off_t) sizeof(buf)))) <= 0) {
			fprintf(stderr, "[util::configuration::load] Error reading file %s.\n", filename);

			file.close();
			return false;
		}

		if (!parse(buf, ret)) {
			fprintf(stderr, "[util::configuration::load] Error parsing file %s:%u.\n", filename, _M_state._M_line);

			file.close();
			return false;
		}

		left -= ret;
	} while (left > 0);

	file.close();

	return end_parsing();
}

bool util::configuration::parse(const char* buf, size_t len)
{
	const char* end = buf + len;

	while (buf < end) {
		unsigned char c = (unsigned char) *buf++;

		switch (_M_state._M_state) {
			case state::STATE_INITIAL:
				if (key::valid_key_character(c)) {
					_M_state._M_key[0] = c;
					_M_state._M_keylen = 1;

					_M_state._M_state = state::STATE_PARSING_KEY;
				} else {
					switch (c) {
						case '#':
							_M_state._M_prev_state = state::STATE_INITIAL;
							_M_state._M_state = state::STATE_PARSING_COMMENT;
							break;
						case '\n':
							_M_state._M_line++;

							// Fall through.
						case ' ':
						case '\t':
						case '\r':
							break;
						case '}':
							if (!_M_state._M_keys->parent) {
								fprintf(stderr, "[util::configuration::parse] Found '}' at the root level.\n");
								return false;
							}

							_M_state._M_keys = _M_state._M_keys->parent;

							_M_state._M_state = state::STATE_AFTER_CLOSING_BRACE;
							break;
						default:
							fprintf(stderr, "[util::configuration::parse] Invalid character (%c, 0x%02x) in the initial state.\n", c, c);
							return false;
					}
				}

				break;
			case state::STATE_PARSING_COMMENT:
				if (c == '\n') {
					_M_state._M_line++;
					_M_state._M_state = _M_state._M_prev_state;
				}

				break;
			case state::STATE_PARSING_KEY:
				if (key::valid_key_character(c)) {
					// If the key is too long...
					if (_M_state._M_keylen == sizeof(_M_state._M_key)) {
						fprintf(stderr, "[util::configuration::parse] Key is too long.\n");
						return false;
					}

					_M_state._M_key[_M_state._M_keylen++] = c;
				} else {
					switch (c) {
						case '=':
							_M_state._M_state = state::STATE_AFTER_EQUAL;
							break;
						case ' ':
						case '\t':
							_M_state._M_state = state::STATE_SPACE_AFTER_KEY;
							break;
						case '\n':
							_M_state._M_line++;

							// Fall through.
						case '\r':
							if (!add_key(KEY_MIGHT_HAVE_CHILDREN)) {
								return false;
							}

							_M_state._M_state = state::STATE_WAITING_FOR_KEY_OR_BRACE;
							break;
						default:
							fprintf(stderr, "[util::configuration::parse] Invalid character (%c, 0x%02x) while parsing key.\n", c, c);
							return false;
					}
				}

				break;
			case state::STATE_SPACE_AFTER_KEY:
				switch (c) {
					case '=':
						_M_state._M_state = state::STATE_AFTER_EQUAL;
						break;
					case ' ':
					case '\t':
						break;
					case '{':
						if (!add_key(KEY_MIGHT_HAVE_CHILDREN)) {
							return false;
						}

						if (!create_keys()) {
							return false;
						}

						_M_state._M_state = state::STATE_AFTER_OPENING_BRACE;
						break;
					case '}':
						if (!_M_state._M_keys->parent) {
							fprintf(stderr, "[util::configuration::parse] Found '}' at the root level.\n");
							return false;
						}

						if (!add_key(KEY_MIGHT_HAVE_CHILDREN)) {
							return false;
						}

						_M_state._M_keys = _M_state._M_keys->parent;

						_M_state._M_state = state::STATE_AFTER_CLOSING_BRACE;
						break;
					case '#':
						if (!add_key(KEY_MIGHT_HAVE_CHILDREN)) {
							return false;
						}

						_M_state._M_prev_state = state::STATE_WAITING_FOR_KEY_OR_BRACE;
						_M_state._M_state = state::STATE_PARSING_COMMENT;
						break;
					case '\n':
						_M_state._M_line++;

						// Fall through.
					case '\r':
						if (!add_key(KEY_MIGHT_HAVE_CHILDREN)) {
							return false;
						}

						_M_state._M_state = state::STATE_WAITING_FOR_KEY_OR_BRACE;
						break;
					default:
						fprintf(stderr, "[util::configuration::parse] Invalid character (%c, 0x%02x) while parsing key.\n", c, c);
						return false;
				}

				break;
			case state::STATE_AFTER_EQUAL:
				switch (c) {
					case ' ':
					case '\t':
						_M_state._M_state = state::STATE_SPACE_AFTER_EQUAL;
						break;
					case '"':
						_M_state._M_valuelen = 0;
						_M_state._M_state = state::STATE_PARSING_QUOTED_VALUE;
						break;
					case '\n':
						_M_state._M_line++;

						// Fall through.
					case '\r':
						// Ignore key.
						_M_state._M_state = state::STATE_INITIAL;
						break;
					default:
						if (key::valid_value_character(c)) {
							_M_state._M_value[0] = c;
							_M_state._M_valuelen = 1;

							_M_state._M_state = state::STATE_PARSING_VALUE;
						} else {
							fprintf(stderr, "[util::configuration::parse] Invalid character (%c, 0x%02x) while waiting for value.\n", c, c);
							return false;
						}
				}

				break;
			case state::STATE_SPACE_AFTER_EQUAL:
				switch (c) {
					case ' ':
					case '\t':
						break;
					case '#':
						// Ignore key.
						_M_state._M_prev_state = state::STATE_INITIAL;
						_M_state._M_state = state::STATE_PARSING_COMMENT;
						break;
					case '"':
						_M_state._M_valuelen = 0;
						_M_state._M_state = state::STATE_PARSING_QUOTED_VALUE;
						break;
					case '\n':
						_M_state._M_line++;

						// Fall through.
					case '\r':
						// Ignore key.
						_M_state._M_state = state::STATE_INITIAL;
						break;
					default:
						if (key::valid_value_character(c)) {
							_M_state._M_value[0] = c;
							_M_state._M_valuelen = 1;

							_M_state._M_state = state::STATE_PARSING_VALUE;
						} else {
							fprintf(stderr, "[util::configuration::parse] Invalid character (%c, 0x%02x) while waiting for value.\n", c, c);
							return false;
						}
				}

				break;
			case state::STATE_PARSING_VALUE:
				if (key::valid_value_character(c)) {
					// If the value is too long...
					if (_M_state._M_valuelen == sizeof(_M_state._M_value)) {
						fprintf(stderr, "[util::configuration::parse] Value is too long.\n");
						return false;
					}

					_M_state._M_value[_M_state._M_valuelen++] = c;
				} else {
					switch (c) {
						case ' ':
						case '\t':
							if (!add_key(KEY_HAS_VALUE)) {
								return false;
							}

							_M_state._M_state = state::STATE_SPACE_AFTER_VALUE;
							break;
						case '\n':
							_M_state._M_line++;

							// Fall through.
						case '\r':
							if (!add_key(KEY_HAS_VALUE)) {
								return false;
							}

							_M_state._M_state = state::STATE_INITIAL;
							break;
						default:
							fprintf(stderr, "[util::configuration::parse] Invalid character (%c, 0x%02x) while parsing value.\n", c, c);
							return false;
					}
				}

				break;
			case state::STATE_SPACE_AFTER_VALUE:
				switch (c) {
					case ' ':
					case '\t':
						break;
					case '#':
						_M_state._M_prev_state = state::STATE_INITIAL;
						_M_state._M_state = state::STATE_PARSING_COMMENT;
						break;
					case '\n':
						_M_state._M_line++;

						// Fall through.
					case '\r':
						_M_state._M_state = state::STATE_INITIAL;
						break;
					default:
						fprintf(stderr, "[util::configuration::parse] Invalid character (%c, 0x%02x) after value.\n", c, c);
						return false;
				}

				break;
			case state::STATE_PARSING_QUOTED_VALUE:
				switch (c) {
					case '"':
						if (_M_state._M_valuelen > 0) {
							if (!add_key(KEY_HAS_VALUE)) {
								return false;
							}
						}

						_M_state._M_state = state::STATE_SPACE_AFTER_VALUE;
						break;
					case '\\':
						// If the value is too long...
						if (_M_state._M_valuelen == sizeof(_M_state._M_value)) {
							fprintf(stderr, "[util::configuration::parse] Value is too long.\n");
							return false;
						}

						_M_state._M_state = state::STATE_PARSING_ESCAPE_CHARACTER;
						break;
					case '\n':
						_M_state._M_line++;

						// Fall through.
					default:
						// If the value is too long...
						if (_M_state._M_valuelen == sizeof(_M_state._M_value)) {
							fprintf(stderr, "[util::configuration::parse] Value is too long.\n");
							return false;
						}

						_M_state._M_value[_M_state._M_valuelen++] = c;
				}

				break;
			case state::STATE_PARSING_ESCAPE_CHARACTER:
				switch (c) {
					case '"':
						_M_state._M_value[_M_state._M_valuelen++] = '"';
						break;
					case '\'':
						_M_state._M_value[_M_state._M_valuelen++] = '\'';
						break;
					case '\\':
						_M_state._M_value[_M_state._M_valuelen++] = '\\';
						break;
					case 't':
						_M_state._M_value[_M_state._M_valuelen++] = '\t';
						break;
					case 'r':
						_M_state._M_value[_M_state._M_valuelen++] = '\r';
						break;
					case 'n':
						_M_state._M_value[_M_state._M_valuelen++] = '\n';
						break;
					default:
						fprintf(stderr, "[util::configuration::parse] Invalid escape character '\\%c'.\n", c);
						return false;
				}

				_M_state._M_state = state::STATE_PARSING_QUOTED_VALUE;
				break;
			case state::STATE_WAITING_FOR_KEY_OR_BRACE:
				if (key::valid_key_character(c)) {
					_M_state._M_key[0] = c;
					_M_state._M_keylen = 1;

					_M_state._M_state = state::STATE_PARSING_KEY;
				} else {
					switch (c) {
						case '#':
							_M_state._M_prev_state = state::STATE_WAITING_FOR_KEY_OR_BRACE;
							_M_state._M_state = state::STATE_PARSING_COMMENT;
							break;
						case '\n':
							_M_state._M_line++;

							// Fall through.
						case ' ':
						case '\t':
						case '\r':
							break;
						case '{':
							if (!create_keys()) {
								return false;
							}

							_M_state._M_state = state::STATE_AFTER_OPENING_BRACE;
							break;
						case '}':
							if (!_M_state._M_keys->parent) {
								fprintf(stderr, "[util::configuration::parse] Found '}' at the root level.\n");
								return false;
							}

							_M_state._M_keys = _M_state._M_keys->parent;

							_M_state._M_state = state::STATE_AFTER_CLOSING_BRACE;
							break;
						default:
							fprintf(stderr, "[util::configuration::parse] Invalid character (%c, 0x%02x) while waiting for key or brace.\n", c, c);
							return false;
					}
				}

				break;
			case state::STATE_AFTER_OPENING_BRACE:
				switch (c) {
					case '\n':
						_M_state._M_line++;

						// Fall through.
					case ' ':
					case '\t':
					case '\r':
						_M_state._M_state = state::STATE_WAITING_FOR_KEY;
						break;
					default:
						fprintf(stderr, "[util::configuration::parse] Invalid character (%c, 0x%02x) after opening brace.\n", c, c);
						return false;
				}

				break;
			case state::STATE_WAITING_FOR_KEY:
				if (key::valid_key_character(c)) {
					_M_state._M_key[0] = c;
					_M_state._M_keylen = 1;

					_M_state._M_state = state::STATE_PARSING_KEY;
				} else {
					switch (c) {
						case '#':
							_M_state._M_prev_state = state::STATE_WAITING_FOR_KEY;
							_M_state._M_state = state::STATE_PARSING_COMMENT;
							break;
						case '\n':
							_M_state._M_line++;

							// Fall through.
						case ' ':
						case '\t':
						case '\r':
							break;
						default:
							fprintf(stderr, "[util::configuration::parse] Invalid character (%c, 0x%02x) while waiting for key.\n", c, c);
							return false;
					}
				}

				break;
			case state::STATE_AFTER_CLOSING_BRACE:
				switch (c) {
					case '\n':
						_M_state._M_line++;

						// Fall through.
					case ' ':
					case '\t':
					case '\r':
						_M_state._M_state = state::STATE_INITIAL;
						break;
					default:
						fprintf(stderr, "[util::configuration::parse] Invalid character (%c, 0x%02x) after closing brace.\n", c, c);
						return false;
				}

				break;
		}
	}

	return true;
}

bool util::configuration::end_parsing()
{
	switch (_M_state._M_state) {
		case state::STATE_INITIAL:
		case state::STATE_AFTER_EQUAL:
		case state::STATE_SPACE_AFTER_EQUAL:
		case state::STATE_WAITING_FOR_KEY_OR_BRACE:
		case state::STATE_AFTER_CLOSING_BRACE:
			if (_M_state._M_keys != &_M_keys) {
				return false;
			}

			adjust_pointers(&_M_keys);
			return true;
		case state::STATE_PARSING_COMMENT:
			if ((_M_state._M_prev_state == state::STATE_WAITING_FOR_KEY) || (_M_state._M_keys != &_M_keys)) {
				return false;
			}

			adjust_pointers(&_M_keys);
			return true;
		case state::STATE_PARSING_KEY:
		case state::STATE_SPACE_AFTER_KEY:
			if ((_M_state._M_keys != &_M_keys) || (!add_key(KEY_MIGHT_HAVE_CHILDREN))) {
				return false;
			}

			adjust_pointers(&_M_keys);
			return true;
		case state::STATE_PARSING_VALUE:
		case state::STATE_SPACE_AFTER_VALUE:
			if ((_M_state._M_keys != &_M_keys) || (!add_key(KEY_HAS_VALUE))) {
				return false;
			}

			adjust_pointers(&_M_keys);
			return true;
	}

	return false;
}

bool util::configuration::get_key(const char*& key, unsigned short& keylen, size_t i, ...) const
{
	va_list ap;
	va_start(ap, i);

	unsigned argc;
	const struct key* k = find(ap, argc);

	va_end(ap);

	if (!k) {
		if ((argc > 0) || (i >= _M_keys.used)) {
			return false;
		}

		k = &_M_keys.keys[i];

		key = k->_M_name.data;
		keylen = k->_M_name.len;

		return true;
	}

	if ((k->_M_type == KEY_HAS_VALUE) || (!k->_M_children) || (i >= k->_M_children->used)) {
		return false;
	}

	k = &k->_M_children->keys[i];

	key = k->_M_name.data;
	keylen = k->_M_name.len;

	return true;
}

bool util::configuration::get_value(const char*& value, unsigned short& valuelen, ...) const
{
	va_list ap;
	va_start(ap, valuelen);

	const struct key* k = find(ap);

	va_end(ap);

	if ((!k) || (k->_M_type != KEY_HAS_VALUE)) {
		return false;
	}

	value = k->_M_value.data;
	valuelen = k->_M_value.len;

	return true;
}

bool util::configuration::get_children_count(size_t& count, ...) const
{
	va_list ap;
	va_start(ap, count);

	unsigned argc;
	const struct key* k = find(ap, argc);

	va_end(ap);

	if (!k) {
		if (argc > 0) {
			return false;
		}

		count = _M_keys.used;
		return true;
	}

	if (k->_M_type == KEY_HAS_VALUE) {
		return false;
	}

	count = (k->_M_children) ? k->_M_children->used : 0;

	return true;
}

void util::configuration::delete_keys(struct keys* keys)
{
	if (keys->keys) {
		for (size_t i = keys->used; i > 0; i--) {
			struct key* k = &keys->keys[i - 1];

			if ((k->_M_type == KEY_MIGHT_HAVE_CHILDREN) && (k->_M_children)) {
				delete_keys(k->_M_children);
			}
		}

		::free(keys->keys);
		keys->keys = NULL;
	}

	keys->size = 0;
	keys->used = 0;

	keys->parent = NULL;

	if (keys != &_M_keys) {
		::free(keys);
	}
}

bool util::configuration::add_key(key_type type)
{
	size_t pos;
	if (search(_M_state._M_keys, _M_state._M_key, _M_state._M_keylen, pos)) {
		// Duplicated key.
		fprintf(stderr, "[util::configuration::parse] Duplicated key %.*s.\n", _M_state._M_keylen, _M_state._M_key);
		return false;
	}

	size_t len = _M_state._M_keylen + 1;

	if (type == KEY_HAS_VALUE) {
		len += (_M_state._M_valuelen + 1);
	}

	if (!_M_buf.allocate(len)) {
		return false;
	}

	struct keys* keys = _M_state._M_keys;

	if (keys->used == keys->size) {
		size_t size = (keys->size == 0) ? KEY_ALLOC : keys->size * 2;

		struct key* k;
		if ((k = (struct key*) realloc(keys->keys, size * sizeof(struct key))) == NULL) {
			return false;
		}

		keys->keys = k;
		keys->size = size;
	}

	if (pos < keys->used) {
		memmove(&keys->keys[pos + 1], &keys->keys[pos], (keys->used - pos) * sizeof(struct key));
	}

	struct key* k = &keys->keys[pos];

	k->_M_type = type;

	k->_M_name.off = _M_buf.length();
	k->_M_name.len = _M_state._M_keylen;

	char* data = _M_buf.end();

	memcpy(data, _M_state._M_key, _M_state._M_keylen);
	data += _M_state._M_keylen;
	*data++ = 0;

	_M_buf.increment_length(_M_state._M_keylen + 1);

	if (type == KEY_HAS_VALUE) {
		k->_M_value.off = _M_buf.length();
		k->_M_value.len = _M_state._M_valuelen;

		char* data = _M_buf.end();

		memcpy(data, _M_state._M_value, _M_state._M_valuelen);
		data += _M_state._M_valuelen;
		*data = 0;

		_M_buf.increment_length(_M_state._M_valuelen + 1);
	} else {
		k->_M_children = NULL;
	}

	k->_M_keys = keys;

	keys->used++;

	_M_state._M_pos = pos;

	return true;
}

bool util::configuration::create_keys()
{
	struct keys* keys;
	if ((keys = (struct keys*) malloc(sizeof(struct keys))) == NULL) {
		return false;
	}

	keys->keys = NULL;
	keys->size = 0;
	keys->used = 0;

	keys->parent = _M_state._M_keys;

	_M_state._M_keys->keys[_M_state._M_pos]._M_children = keys;

	_M_state._M_keys = keys;
	_M_state._M_pos = 0;

	return true;
}

void util::configuration::adjust_pointers(struct keys* keys)
{
	for (size_t i = keys->used; i > 0; i--) {
		struct key* k = &keys->keys[i - 1];

		k->_M_name.data = _M_buf.data() + k->_M_name.off;

		if (k->_M_type == KEY_HAS_VALUE) {
			k->_M_value.data = _M_buf.data() + k->_M_value.off;
		} else {
			if (k->_M_children) {
				adjust_pointers(k->_M_children);
			}
		}
	}
}

bool util::configuration::search(const struct keys* keys, const char* key, unsigned short len, size_t& pos) const
{
	if (_M_ordered) {
		int i = 0;
		int j = keys->used - 1;

		while (i <= j) {
			int pivot = (i + j) / 2;

			const struct key* k = &keys->keys[pivot];
			unsigned short l = k->_M_name.len;

			int ret = strncasecmp(key, _M_buf.data() + k->_M_name.off, MIN(len, l));
			if (ret < 0) {
				j = pivot - 1;
			} else if (ret == 0) {
				if (len < l) {
					j = pivot - 1;
				} else if (len == l) {
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
	} else {
		size_t i;
		for (i = 0; i < keys->used; i++) {
			const struct key* k = &keys->keys[i];
			unsigned short l = k->_M_name.len;
			if ((len == l) && (strncasecmp(key, _M_buf.data() + k->_M_name.off, len) == 0)) {
				pos = i;
				return true;
			}
		}

		pos = i;
	}

	return false;
}

const struct util::configuration::key* util::configuration::search(const struct keys* keys, const char* key) const
{
	if (_M_ordered) {
		int i = 0;
		int j = keys->used - 1;

		while (i <= j) {
			int pivot = (i + j) / 2;

			const struct key* k = &keys->keys[pivot];

			int ret = strcasecmp(key, k->_M_name.data);
			if (ret < 0) {
				j = pivot - 1;
			} else if (ret == 0) {
				return k;
			} else {
				i = pivot + 1;
			}
		}
	} else {
		size_t i;
		for (i = 0; i < keys->used; i++) {
			const struct key* k = &keys->keys[i];
			if (strcasecmp(key, k->_M_name.data) == 0) {
				return k;
			}
		}
	}

	return NULL;
}

const struct util::configuration::key* util::configuration::find(va_list ap, unsigned& argc) const
{
	const struct keys* keys = &_M_keys;
	const struct key* k = NULL;

	argc = 0;

	do {
		const char* key;
		if (((key = va_arg(ap, const char*)) == NULL) || (!*key)) {
			return k;
		}

		argc++;

		if ((k = search(keys, key)) == NULL) {
			return NULL;
		}

		if ((k->_M_type == KEY_HAS_VALUE) || (!k->_M_children)) {
			if (((key = va_arg(ap, const char*)) != NULL) && (*key)) {
				return NULL;
			}

			return k;
		}

		keys = k->_M_children;
	} while (true);

	return NULL;
}

void util::configuration::print(const struct keys* keys, unsigned depth) const
{
	for (size_t i = 0; i < keys->used; i++) {
		// Indent.
		for (unsigned j = 0; j < depth; j++) {
			printf("\t");
		}

		struct key* k = &keys->keys[i];

		if (k->_M_type == KEY_HAS_VALUE) {
			printf("%s = ", k->_M_name.data);
			print_value(k->_M_value.data, k->_M_value.len);
		} else {
			printf("%s\n", k->_M_name.data);

			if ((k->_M_children) && (k->_M_children->used > 0)) {
				for (unsigned j = 0; j < depth; j++) {
					printf("\t");
				}

				printf("{\n");

				print(k->_M_children, depth + 1);

				for (unsigned j = 0; j < depth; j++) {
					printf("\t");
				}

				printf("}\n");
			}
		}
	}
}

void util::configuration::print_value(const char* value, unsigned short valuelen) const
{
	for (unsigned short i = 0; i < valuelen; i++) {
		if (!key::valid_value_character(value[i])) {
			printf("\"");

			for (unsigned j = 0; j < valuelen; j++) {
				char c;
				switch ((c = value[j])) {
					case '"':
						printf("\\\"");
						break;
					case '\'':
						printf("\\'");
						break;
					case '\\':
						printf("\\\\");
						break;
					case 't':
						printf("\\t");
						break;
					case 'r':
						printf("\\r");
						break;
					case 'n':
						printf("\\n");
						break;
					default:
						printf("%c", c);
				}
			}

			printf("\"\n");
			return;
		}
	}

	printf("%s\n", value);
}
