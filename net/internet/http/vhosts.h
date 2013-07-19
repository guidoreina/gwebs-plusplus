#ifndef VHOSTS_H
#define VHOSTS_H

#include <stdlib.h>
#include "net/internet/http/vhost.h"

namespace net {
	namespace internet {
		namespace http {
			class vhosts {
				public:
					// Constructor.
					vhosts();

					// Destructor.
					~vhosts();

					// Add virtual host.
					bool add(vhost* v, bool default_vhost);

					// Find virtual host.
					vhost* find(const char* name, unsigned short namelen, unsigned short port);

					// Get virtual host.
					vhost* get(size_t i);

					// Get default virtual host.
					vhost* default_vhost();

					// Get count.
					size_t count() const;

				private:
					static const size_t VHOST_ALLOC = 4;

					vhost** _M_vhosts;
					size_t _M_size;
					size_t _M_used;

					vhost* _M_default_vhost;

					// Search.
					bool search(const char* name, unsigned short namelen, unsigned short port, size_t& pos) const;
			};

			inline vhosts::vhosts()
			{
				_M_vhosts = NULL;
				_M_size = 0;
				_M_used = 0;

				_M_default_vhost = NULL;
			}

			inline vhosts::~vhosts()
			{
				if (_M_vhosts) {
					for (size_t i = 0; i < _M_used; i++) {
						delete _M_vhosts[i];
					}

					free(_M_vhosts);
				}
			}

			inline vhost* vhosts::find(const char* name, unsigned short namelen, unsigned short port)
			{
				size_t pos;
				if (!search(name, namelen, port, pos)) {
					return NULL;
				}

				return _M_vhosts[pos];
			}

			inline vhost* vhosts::get(size_t i)
			{
				if (i >= _M_used) {
					return NULL;
				}

				return _M_vhosts[i];
			}

			inline vhost* vhosts::default_vhost()
			{
				return _M_default_vhost;
			}

			inline size_t vhosts::count() const
			{
				return _M_used;
			}
		}
	}
}

#endif // VHOSTS_H
