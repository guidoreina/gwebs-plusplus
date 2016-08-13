#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <new>
#include "net/tcp_server.h"
#include "net/listener.h"
#include "net/internet/http/connection.h"
#include "net/internet/http/vhosts.h"
#include "net/internet/http/error.h"
#include "net/internet/mime/types.h"

namespace net {
	namespace internet {
		namespace http {
			class server : public tcp_server {
				public:
					// Constructor.
					server();

					// Destructor.
					~server();

					// Create.
					using tcp_server::create;
					bool create(const char* config_file, const char* mime_types_file);

					// On new connection.
					bool on_new_connection(socket& client, const socket_address& addr, struct listener* listener);

					// Get virtual hosts.
					vhosts& virtual_hosts();

					// Get MIME type.
					const char* mime_type(const char* extension, unsigned short extensionlen, unsigned short& mime_type_len) const;

					// Get boundary.
					unsigned boundary();

				protected:
					connection* _M_http_connections;

					vhosts _M_vhosts;

#if HAVE_SSL
					bool _M_ssl_initialized;
#endif // HAVE_SSL

					mime::types _M_mime_types;

					unsigned _M_boundary;

					// Load configuration.
					bool load_config(const char* config_file);

					// Create connections.
					bool create_connections();

					// Listen.
					bool listen(const socket_address& addr, bool https);

					// Get extension.
					static const char* extension(const char* filename, unsigned short len, unsigned short& extensionlen);
			};

			inline server::server() : tcp_server(true, false)
			{
#if HAVE_SSL
				_M_ssl_initialized = false;
#endif // HAVE_SSL

				_M_boundary = 0;
			}

			inline server::~server()
			{
				if (_M_http_connections) {
					delete [] _M_http_connections;
				}

#if HAVE_SSL
				if (_M_ssl_initialized) {
					ssl_socket::free_ssl_library();
				}
#endif // HAVE_SSL
			}

			inline bool server::on_new_connection(socket& client, const socket_address& addr, struct listener* listener)
			{
				if (!tcp_server::on_new_connection(client, addr, listener)) {
					return false;
				}

#if HAVE_SSL
				_M_http_connections[client.fd()]._M_https = (listener->_M_data != NULL);
#endif // HAVE_SSL

				return true;
			}

			inline vhosts& server::virtual_hosts()
			{
				return _M_vhosts;
			}

			inline const char* server::mime_type(const char* extension, unsigned short extensionlen, unsigned short& len) const
			{
				return _M_mime_types.mime_type(extension, extensionlen, len);
			}

			inline unsigned server::boundary()
			{
				return ++_M_boundary;
			}

			inline bool server::create_connections()
			{
				if ((_M_http_connections = new (std::nothrow) connection[_M_fdset.size()]) == NULL) {
					return false;
				}

				size_t size = _M_fdset.size();
				for (size_t i = 0; i < size; i++) {
					_M_connections[i] = &_M_http_connections[i];
				}

				return true;
			}
		}
	}
}

#endif // HTTP_SERVER_H
