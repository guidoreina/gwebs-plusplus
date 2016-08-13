#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <sys/types.h>
#include <stdlib.h>
#include <stdarg.h>
#include "string/buffer.h"
#include "macros/macros.h"

namespace util {
	static const size_t CONF_KEY_MAX_LEN = 32;
	static const size_t CONF_VALUE_MAX_LEN = 4 * 1024;

	class configuration {
		public:
			// Forward declaration.
			struct key;

		private:
			struct keys {
				struct key* keys;
				size_t size;
				size_t used;

				struct keys* parent;
			};

		public:
			enum key_type {KEY_HAS_VALUE, KEY_MIGHT_HAVE_CHILDREN};

			struct key {
				friend class configuration;

				public:
					key_type type() const;

					const char* name() const;
					unsigned short namelen() const;

					const char* value() const;
					unsigned short valuelen() const;

					size_t children_count() const;
					const struct key* child(unsigned i) const;

				private:
					key_type _M_type;

					struct _str {
						union {
							const char* data;
							size_t off;
						};

						unsigned short len;
					};

					struct _str _M_name;

					union {
						struct _str _M_value;
						struct keys* _M_children;
					};

					struct keys* _M_keys;

					// Valid key character?
					static bool valid_key_character(unsigned char c);

					// Valid value character?
					static bool valid_value_character(unsigned char c);
			};

			struct iterator {
				friend class configuration;

				public:
					const struct key* key;

				private:
					const struct keys* _M_keys;
					size_t _M_pos;
			};

			// Constructor.
			configuration(bool ordered = true);

			// Destructor.
			~configuration();

			// Free.
			void free();

			// Load.
			bool load(const char* filename);

			// Begin parsing.
			void begin_parsing();

			// Parse.
			bool parse(const char* buf, size_t len);

			// End parsing.
			bool end_parsing();

			// Get key.
			bool get_key(const char*& key, unsigned short& keylen, size_t i, ...) const;

			// Get value.
			bool get_value(const char*& value, unsigned short* valuelen, ...) const;

			// Get children count.
			bool get_children_count(size_t* count, ...) const;

			// Begin.
			bool begin(iterator& it) const;

			// End.
			bool end(iterator& it) const;

			// Next.
			bool next(iterator& it) const;

			// Previous.
			bool previous(iterator& it) const;

			// First child.
			bool first_child(iterator& it) const;

			// Last child.
			bool last_child(iterator& it) const;

			// Parent.
			bool parent(iterator& it) const;

			// Print.
			void print() const;

		private:
			static const off_t MAX_FILE_SIZE = 4 * 1024 * 1024;
			static const size_t KEY_ALLOC = 4;

			string::buffer _M_buf;

			struct keys _M_keys;

			struct state {
				// States.
				enum {
					STATE_INITIAL,
					STATE_PARSING_COMMENT,
					STATE_PARSING_KEY,
					STATE_SPACE_AFTER_KEY,
					STATE_AFTER_EQUAL,
					STATE_SPACE_AFTER_EQUAL,
					STATE_PARSING_VALUE,
					STATE_SPACE_AFTER_VALUE,
					STATE_PARSING_QUOTED_VALUE,
					STATE_PARSING_ESCAPE_CHARACTER,
					STATE_WAITING_FOR_KEY_OR_BRACE,
					STATE_AFTER_OPENING_BRACE,
					STATE_WAITING_FOR_KEY,
					STATE_AFTER_CLOSING_BRACE
				};

				struct keys* _M_keys;
				size_t _M_pos;

				char _M_key[CONF_KEY_MAX_LEN];
				unsigned short _M_keylen;

				char _M_value[CONF_VALUE_MAX_LEN];
				unsigned short _M_valuelen;

				unsigned _M_state;
				unsigned _M_prev_state;

				unsigned _M_line;

				void reset(struct keys* keys);
			};

			state _M_state;

			bool _M_ordered;

			// Delete keys.
			void delete_keys(struct keys* keys);

			// Add key.
			bool add_key(key_type type);

			// Create keys.
			bool create_keys();

			// Adjust pointers.
			void adjust_pointers(struct keys* keys);

			// Search key in key list.
			bool search(const struct keys* keys, const char* key, unsigned short len, size_t& pos) const;
			const struct key* search(const struct keys* keys, const char* key) const;

			// Find key.
			const struct key* find(va_list ap, unsigned& argc) const;
			const struct key* find(va_list ap) const;

			// Print.
			void print(const struct keys* keys, unsigned depth) const;

			// Print value.
			void print_value(const char* value, unsigned short valuelen) const;
	};

	inline configuration::key_type configuration::key::type() const
	{
		return _M_type;
	}

	inline const char* configuration::key::name() const
	{
		return _M_name.data;
	}

	inline unsigned short configuration::key::namelen() const
	{
		return _M_name.len;
	}

	inline const char* configuration::key::value() const
	{
		return (_M_type == KEY_HAS_VALUE) ? _M_value.data : NULL;
	}

	inline unsigned short configuration::key::valuelen() const
	{
		return (_M_type == KEY_HAS_VALUE) ? _M_value.len : 0;
	}

	inline size_t configuration::key::children_count() const
	{
		return ((_M_type == KEY_MIGHT_HAVE_CHILDREN) && (_M_children)) ? _M_children->used : 0;
	}

	inline const struct configuration::key* configuration::key::child(unsigned i) const
	{
		if ((_M_type == KEY_MIGHT_HAVE_CHILDREN) && (_M_children)) {
			return (i < _M_children->used) ? &_M_children->keys[i] : NULL;
		}

		return NULL;
	}

	inline bool configuration::key::valid_key_character(unsigned char c)
	{
		if ((IS_ALPHA(c)) || (IS_DIGIT(c))) {
			return true;
		} else {
			switch (c) {
				case '-':
				case '_':
				case '.':
				case ':':
					return true;
				default:
					return false;
			}
		}
	}

	inline bool configuration::key::valid_value_character(unsigned char c)
	{
		if ((IS_ALPHA(c)) || (IS_DIGIT(c))) {
			return true;
		} else {
			switch (c) {
				case '-':
				case '_':
				case '.':
				case '+':
				case '/':
				case ':':
				case '@':
					return true;
				default:
					return false;
			}
		}
	}

	inline configuration::configuration(bool ordered)
	{
		_M_keys.keys = NULL;
		_M_keys.size = 0;
		_M_keys.used = 0;

		_M_keys.parent = NULL;

		_M_ordered = ordered;
	}

	inline configuration::~configuration()
	{
		free();
	}

	inline void configuration::free()
	{
		_M_buf.free();
		delete_keys(&_M_keys);
	}

	inline void configuration::begin_parsing()
	{
		_M_state.reset(&_M_keys);
	}

	inline bool configuration::begin(iterator& it) const
	{
		if (_M_keys.used == 0) {
			return false;
		}

		it._M_keys = &_M_keys;
		it._M_pos = 0;

		it.key = &it._M_keys->keys[0];

		return true;
	}

	inline bool configuration::end(iterator& it) const
	{
		if (_M_keys.used == 0) {
			return false;
		}

		it._M_keys = &_M_keys;
		it._M_pos = it._M_keys->used - 1;

		it.key = &it._M_keys->keys[it._M_pos];

		return true;
	}

	inline bool configuration::next(iterator& it) const
	{
		if (it._M_pos == it._M_keys->used - 1) {
			return false;
		}

		it.key = &it._M_keys->keys[++it._M_pos];

		return true;
	}

	inline bool configuration::previous(iterator& it) const
	{
		if (it._M_pos == 0) {
			return false;
		}

		it.key = &it._M_keys->keys[--it._M_pos];

		return true;
	}

	inline bool configuration::first_child(iterator& it) const
	{
		if (it.key->children_count() == 0) {
			return false;
		}

		it._M_keys = it.key->_M_children;
		it._M_pos = 0;

		it.key = &it._M_keys->keys[0];

		return true;
	}

	inline bool configuration::last_child(iterator& it) const
	{
		if (it.key->children_count() == 0) {
			return false;
		}

		it._M_keys = it.key->_M_children;
		it._M_pos = it._M_keys->used - 1;

		it.key = &it._M_keys->keys[it._M_pos];

		return true;
	}

	inline bool configuration::parent(iterator& it) const
	{
		if (!it._M_keys->parent) {
			return false;
		}

		it._M_keys = it._M_keys->parent;
		it._M_pos = 0;

		it.key = &it._M_keys->keys[0];

		return true;
	}

	inline void configuration::print() const
	{
		print(&_M_keys, 0);
	}

	inline void configuration::state::reset(struct keys* keys)
	{
		_M_keys = keys;
		_M_pos = 0;

		_M_keylen = 0;
		_M_valuelen = 0;

		_M_state = STATE_INITIAL;

		_M_line = 1;
	}

	inline const struct configuration::key* configuration::find(va_list ap) const
	{
		unsigned argc;
		return find(ap, argc);
	}
}

#endif // CONFIGURATION_H
