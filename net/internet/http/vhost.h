#ifndef VHOST_H
#define VHOST_H

#include <stdlib.h>
#include <new>
#include "net/internet/http/dirlisting.h"
#include "string/buffer.h"

namespace net {
	namespace internet {
		namespace http {
			class vhost {
				public:
					// Constructor.
					vhost(vhost* parent = NULL);

					// Destructor.
					~vhost();

					// Get name.
					const char* name() const;

					// Get length of the name.
					unsigned short namelen() const;

					// Set name.
					bool name(const char* s, unsigned short n);

					// Get port.
					unsigned short port() const;

					// Set port.
					void port(unsigned short p);

					// Get root.
					const char* root() const;

					// Get length of the root.
					unsigned short rootlen() const;

					// Set root.
					bool root(const char* s, unsigned short n);

					// Get index file.
					const char* index(unsigned i, unsigned short& len, const char*& mime_type, unsigned short& mime_type_len) const;

					// Add index file.
					bool add_index(const char* s, unsigned short n, const char* mime_type, unsigned short mime_type_len);

					// Get directory listing.
					dirlisting* get_directory_listing();

					// Set directory listing.
					bool set_directory_listing();

				private:
					static const size_t INDEX_ALLOC = 4;

					string::buffer _M_buf;

					size_t _M_name;
					unsigned short _M_namelen;

					unsigned short _M_port;

					size_t _M_root;
					unsigned short _M_rootlen;

					struct index {
						size_t index;
						unsigned short indexlen;

						const char* mime_type;
						unsigned short mime_type_len;
					};

					struct indices {
						struct index* indices;
						size_t size;
						size_t used;
					};

					struct indices _M_indices;

					dirlisting* _M_dirlisting;

					vhost* _M_parent;
			};

			inline vhost::vhost(vhost* parent)
			{
				_M_namelen = 0;
				_M_port = 0;
				_M_rootlen = 0;

				_M_indices.indices = NULL;
				_M_indices.size = 0;
				_M_indices.used = 0;

				_M_dirlisting = NULL;

				_M_parent = parent ? parent : this;
			}

			inline vhost::~vhost()
			{
				if (_M_parent == this) {
					if (_M_indices.indices) {
						free(_M_indices.indices);
					}

					if (_M_dirlisting) {
						delete _M_dirlisting;
					}
				}
			}

			inline const char* vhost::name() const
			{
				return _M_buf.data() + _M_name;
			}

			inline unsigned short vhost::namelen() const
			{
				return _M_namelen;
			}

			inline bool vhost::name(const char* s, unsigned short n)
			{
				size_t off = _M_buf.length();

				if (!_M_buf.append_nul_terminated_string(s, n)) {
					return false;
				}

				_M_name = off;
				_M_namelen = n;

				return true;
			}

			inline unsigned short vhost::port() const
			{
				return _M_port;
			}

			inline void vhost::port(unsigned short p)
			{
				_M_port = p;
			}

			inline const char* vhost::root() const
			{
				return _M_parent->_M_buf.data() + _M_parent->_M_root;
			}

			inline unsigned short vhost::rootlen() const
			{
				return _M_parent->_M_rootlen;
			}

			inline bool vhost::root(const char* s, unsigned short n)
			{
				if (_M_parent != this) {
					return false;
				}

				size_t off = _M_buf.length();

				if (!_M_buf.append_nul_terminated_string(s, n)) {
					return false;
				}

				_M_root = off;
				_M_rootlen = n;

				return true;
			}

			inline const char* vhost::index(unsigned i, unsigned short& len, const char*& mime_type, unsigned short& mime_type_len) const
			{
				if (i >= _M_parent->_M_indices.used) {
					return NULL;
				}

				const struct index* idx = &_M_parent->_M_indices.indices[i];

				len = idx->indexlen;

				mime_type = idx->mime_type;
				mime_type_len = idx->mime_type_len;

				return _M_parent->_M_buf.data() + idx->index;
			}

			inline dirlisting* vhost::get_directory_listing()
			{
				return _M_parent->_M_dirlisting;
			}

			inline bool vhost::set_directory_listing()
			{
				if ((_M_parent != this) || (!_M_root)) {
					return false;
				}

				if ((_M_dirlisting = new (std::nothrow) dirlisting()) == NULL) {
					return false;
				}

				return _M_dirlisting->root_directory(_M_buf.data() + _M_root, _M_rootlen);
			}
		}
	}
}

#endif // VHOST_H
