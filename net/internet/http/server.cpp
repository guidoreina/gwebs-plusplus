#include <stdlib.h>
#include <sys/stat.h>
#include "net/internet/http/server.h"
#include "net/internet/default_ports.h"
#include "util/configuration.h"
#include "string/memrchr.h"

bool net::internet::http::server::create(const char* config_file, const char* mime_types_file)
{
	if (!tcp_server::create()) {
		return false;
	}

	if (!_M_mime_types.load(mime_types_file)) {
		return false;
	}

	if (!load_config(config_file)) {
		return false;
	}

	if (!error::init()) {
		return false;
	}

	return true;
}

bool net::internet::http::server::load_config(const char* config_file)
{
	// Load configuration file.
	util::configuration conf;
	if (!conf.load(config_file)) {
		return false;
	}

	enum tribool {
		TRIBOOL_TRUE,
		TRIBOOL_FALSE,
		TRIBOOL_UNDEFINED
	};

	tribool global_directory_listing;

	const char* value;
	unsigned short valuelen;
	if (!conf.get_value(value, valuelen, "http", "directory_listing", NULL)) {
		global_directory_listing = TRIBOOL_UNDEFINED;
	} else {
		if (valuelen == 3) {
			if (strncasecmp(value, "yes", 3) == 0) {
				global_directory_listing = TRIBOOL_TRUE;
			} else {
				fprintf(stderr, "Invalid value \"%s\" for key \"http\" -> \"directory_listing\".\n", value);
				return false;
			}
		} else if (valuelen == 2) {
			if (strncasecmp(value, "no", 2) == 0) {
				global_directory_listing = TRIBOOL_FALSE;
			} else {
				fprintf(stderr, "Invalid value \"%s\" for key \"http\" -> \"directory_listing\".\n", value);
				return false;
			}
		} else {
			fprintf(stderr, "Invalid value \"%s\" for key \"http\" -> \"directory_listing\".\n", value);
			return false;
		}
	}

	// Load hosts.
	const char* host;
	unsigned short hostlen;
	for (size_t i = 0; conf.get_key(host, hostlen, i, "http", "hosts", NULL); i++) {
		unsigned port;
		const char* colon;
		if ((colon = (const char*) string::memrchr(host, ':', hostlen)) != NULL) {
			unsigned short len;
			if ((len = colon - host) == 0) {
				fprintf(stderr, "Invalid host name \"%s\".\n", host);
				return false;
			}

			if (util::number::parse(colon + 1, hostlen - len - 1, port, 1, 65535) != util::number::PARSE_SUCCEEDED) {
				fprintf(stderr, "Invalid host name \"%s\".\n", host);
				return false;
			}

			hostlen = len;
		} else {
			port = HTTP_DEFAULT_PORT;
		}

		const char* root;
		unsigned short rootlen;
		if (!conf.get_value(root, rootlen, "http", "hosts", host, "root", NULL)) {
			fprintf(stderr, "Host \"%s\" doesn't have root.\n", host);
			return false;
		}

		// If the root doesn't exist or is not a directory...
		struct stat buf;
		if ((stat(root, &buf) < 0) || (!S_ISDIR(buf.st_mode))) {
			fprintf(stderr, "\"%s\" doesn't exist or is not a directory.\n", root);
			return false;
		}

		bool default_vhost;
		if (!conf.get_value(value, valuelen, "http", "hosts", host, "default", NULL)) {
			default_vhost = false;
		} else {
			if (valuelen == 3) {
				if (strncasecmp(value, "yes", 3) == 0) {
					default_vhost = true;
				} else {
					fprintf(stderr, "Invalid value \"%s\" for key \"http\" -> \"hosts\" -> \"%s\" -> \"default\".\n", value, host);
					return false;
				}
			} else if (valuelen == 2) {
				if (strncasecmp(value, "no", 2) == 0) {
					default_vhost = false;
				} else {
					fprintf(stderr, "Invalid value \"%s\" for key \"http\" -> \"hosts\" -> \"%s\" -> \"default\".\n", value, host);
					return false;
				}
			} else {
				fprintf(stderr, "Invalid value \"%s\" for key \"http\" -> \"hosts\" -> \"%s\" -> \"default\".\n", value, host);
				return false;
			}
		}

		bool directory_listing;
		if (global_directory_listing == TRIBOOL_FALSE) {
			directory_listing = false;
		} else {
			if (!conf.get_value(value, valuelen, "http", "hosts", host, "directory_listing", NULL)) {
				directory_listing = (global_directory_listing == TRIBOOL_TRUE) ? true : false;
			} else {
				if (valuelen == 3) {
					if (strncasecmp(value, "yes", 3) == 0) {
						directory_listing = true;
					} else {
						fprintf(stderr, "Invalid value \"%s\" for key \"http\" -> \"hosts\" -> \"%s\" -> \"directory_listing\".\n", value, host);
						return false;
					}
				} else if (valuelen == 2) {
					if (strncasecmp(value, "no", 2) == 0) {
						directory_listing = false;
					} else {
						fprintf(stderr, "Invalid value \"%s\" for key \"http\" -> \"hosts\" -> \"%s\" -> \"directory_listing\".\n", value, host);
						return false;
					}
				} else {
					fprintf(stderr, "Invalid value \"%s\" for key \"http\" -> \"hosts\" -> \"%s\" -> \"directory_listing\".\n", value, host);
					return false;
				}
			}
		}

		vhost* v;
		if ((v = new (std::nothrow) vhost()) == NULL) {
			return false;
		}

		if (!v->name(host, hostlen)) {
			delete v;
			return false;
		}

		if (!v->root(root, rootlen)) {
			delete v;
			return false;
		}

		if (directory_listing) {
			if (!v->set_directory_listing()) {
				delete v;
				return false;
			}
		}

		// Index files.
		const char* index;
		unsigned short indexlen;
		for (size_t j = 0; conf.get_key(index, indexlen, j, "http", "hosts", host, "index", NULL); j++) {
			const char* type;
			unsigned short mime_type_len;

			unsigned short extlen;
			const char* ext;
			if ((ext = extension(index, indexlen, extlen)) == NULL) {
				type = mime::types::DEFAULT_MIME_TYPE;
				mime_type_len = mime::types::DEFAULT_MIME_TYPE_LEN;
			} else {
				type = mime_type(ext, extlen, mime_type_len);
			}

			if (!v->add_index(index, indexlen, type, mime_type_len)) {
				delete v;
				return false;
			}
		}

		v->port(port);

		if (!_M_vhosts.add(v, default_vhost)) {
			delete v;
			return false;
		}

		// Load aliases.
		const char* alias;
		unsigned short aliaslen;
		for (size_t j = 0; conf.get_key(alias, aliaslen, j, "http", "hosts", host, "alias", NULL); j++) {
			if ((colon = (const char*) string::memrchr(alias, ':', aliaslen)) != NULL) {
				unsigned short len;
				if ((len = colon - alias) == 0) {
					fprintf(stderr, "Invalid alias \"%s\".\n", alias);
					return false;
				}

				if (util::number::parse(colon + 1, aliaslen - len - 1, port, 1, 65535) != util::number::PARSE_SUCCEEDED) {
					fprintf(stderr, "Invalid alias \"%s\".\n", alias);
					return false;
				}

				aliaslen = len;
			} else {
				port = HTTP_DEFAULT_PORT;
			}

			vhost* a;
			if ((a = new (std::nothrow) vhost(v)) == NULL) {
				return false;
			}

			if (!a->name(alias, aliaslen)) {
				delete a;
				return false;
			}

			a->port(port);

			if (!_M_vhosts.add(a, false)) {
				delete a;
				return false;
			}
		}

		// Start listeners.
		const char* listener;
		unsigned short listenerlen;
		for (size_t j = 0; conf.get_key(listener, listenerlen, j, "http", "hosts", host, "listen", NULL); j++) {
			socket_address addr;
			if (!addr.build(listener)) {
				fprintf(stderr, "Invalid address \"%s\".\n", listener);
				return false;
			}

			if (!conf.get_value(value, valuelen, "http", "hosts", host, "listen", listener, NULL)) {
				fprintf(stderr, "Value for listener \"%s\" is missing.\n", listener);
				return false;
			}

			bool https;
			if (valuelen == 4) {
				if (strncasecmp(value, "http", 4) != 0) {
					fprintf(stderr, "Invalid value \"%s\" for listener \"%s\".\n", value, listener);
					return false;
				}

				https = false;
			} else if (valuelen == 5) {
				if (strncasecmp(value, "https", 5) != 0) {
					fprintf(stderr, "Invalid value \"%s\" for listener \"%s\".\n", value, listener);
					return false;
				}

				https = true;
			} else {
				fprintf(stderr, "Invalid value \"%s\" for listener \"%s\".\n", value, listener);
				return false;
			}

			if (!listen(addr, https)) {
				return false;
			}
		}
	}

	return true;
}

bool net::internet::http::server::listen(const socket_address& addr, bool https)
{
#if HAVE_SSL
	if ((https) && (!_M_ssl_initialized)) {
		if (!ssl_socket::init_ssl_library()) {
			return false;
		}

		_M_ssl_initialized = true;

		if (!ssl_socket::load_certificate("server.crt", "privkey.pem")) {
			return false;
		}
	}
#endif // HAVE_SSL

	return tcp_server::listen(addr, https ? (void*) 1 : NULL);
}

const char* net::internet::http::server::extension(const char* filename, unsigned short len, unsigned short& extensionlen)
{
	const char* end = filename + len - 1;
	const char* ext = end;

	while (--ext > filename) {
		if (*ext == '.') {
			extensionlen = end - ext;
			return ext + 1;
		}
	}

	return NULL;
}
