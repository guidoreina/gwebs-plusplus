#include <stdlib.h>
#include "net/internet/http/error.h"
#include "net/internet/http/server.h"
#include "net/internet/http/version.h"
#include "net/internet/default_ports.h"
#include "macros/macros.h"

net::internet::http::headers net::internet::http::error::_M_headers;

struct net::internet::http::error::err net::internet::http::error::_M_errors[] = {
	{MOVED_PERMANENTLY, "Moved Permanently"},
	{NOT_MODIFIED, "Not Modified"},
	{BAD_REQUEST, "Bad Request"},
	{FORBIDDEN, "Forbidden"},
	{NOT_FOUND, "Not Found"},
	{LENGTH_REQUIRED, "Length Required"},
	{REQUEST_ENTITY_TOO_LARGE, "Request Entity Too Large"},
	{REQUEST_URI_TOO_LONG, "Request-URI Too Long"},
	{REQUESTED_RANGE_NOT_SATISFIABLE, "Requested Range Not Satisfiable"},
	{INTERNAL_SERVER_ERROR, "Internal Server Error"},
	{NOT_IMPLEMENTED, "Not Implemented"},
	{BAD_GATEWAY, "Bad Gateway"},
	{SERVICE_UNAVAILABLE, "Service Unavailable"},
	{GATEWAY_TIMEOUT, "Gateway Timeout"}
};

bool net::internet::http::error::init()
{
	for (unsigned i = 0; i < ARRAY_SIZE(_M_errors); i++) {
		err* e = &_M_errors[i];
		if (e->status_code != NOT_MODIFIED) {
			if (!e->body.format(
				"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
				"<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\""
				" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">"
				"<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"en\">"
					"<head>"
						"<title>%s</title>"
					"</head>"
					"<body>"
						"<h1>"
							"HTTP/1.1 %d %s"
						"</h1>"
					"</body>"
				"</html>",
				e->reason_phrase,
				e->status_code,
				e->reason_phrase)) {
					return false;
			}
		}
	}

	return true;
}

bool net::internet::http::error::build_page(connection& conn, unsigned short status_code)
{
	const err* e;
	if ((e = search(status_code)) == NULL) {
		return false;
	}

	if (!conn.add_common_headers(_M_headers)) {
		return false;
	}

	char buffer[2 * PATH_MAX];
	header_value value;
	value.value = buffer;

	switch (status_code) {
		case MOVED_PERMANENTLY:
			if (conn._M_hostlen > 0) {
				value.len = snprintf(buffer, sizeof(buffer), "%.*s/", conn._M_urllen - conn._M_querylen - conn._M_fragmentlen, conn._M_in.data() + conn._M_url);
			} else {
				bool https = (conn._M_listener->_M_data != NULL);
				in_port_t port = conn._M_listener->port();

				if (!https) {
					if (port == HTTP_DEFAULT_PORT) {
						value.len = snprintf(buffer, sizeof(buffer), "http://%.*s%.*s/", conn._M_vhost->namelen(), conn._M_vhost->name(), conn._M_path.count(), conn._M_path.data());
					} else {
						value.len = snprintf(buffer, sizeof(buffer), "http://%.*s:%u%.*s/", conn._M_vhost->namelen(), conn._M_vhost->name(), port, conn._M_path.count(), conn._M_path.data());
					}
				} else {
					if (port == HTTPS_DEFAULT_PORT) {
						value.len = snprintf(buffer, sizeof(buffer), "https://%.*s%.*s/", conn._M_vhost->namelen(), conn._M_vhost->name(), conn._M_path.count(), conn._M_path.data());
					} else {
						value.len = snprintf(buffer, sizeof(buffer), "https://%.*s:%u%.*s/", conn._M_vhost->namelen(), conn._M_vhost->name(), port, conn._M_path.count(), conn._M_path.data());
					}
				}
			}

			if (!_M_headers.add(header_name::LOCATION, value)) {
				return false;
			}

			if (!_M_headers.add(header_name::CONTENT_TYPE, header_value("text/html; charset=UTF-8", 24))) {
				return false;
			}

			if (!_M_headers.add(header_name::CONTENT_LENGTH, (uint64_t) e->body.count())) {
				return false;
			}

			break;
		case NOT_MODIFIED:
			value.len = snprintf(buffer, sizeof(buffer), "\"%x-%x\"", conn._M_last_modified, conn._M_filesize);

			if (!_M_headers.add(header_name::ETAG, value)) {
				return error::INTERNAL_SERVER_ERROR;
			}

			// The 304 response MUST NOT contain a message-body.
			if (!_M_headers.add_time(header_name::LAST_MODIFIED, conn._M_last_modified)) {
				return false;
			}

			break;
		case REQUESTED_RANGE_NOT_SATISFIABLE:
			value.len = snprintf(buffer, sizeof(buffer), "bytes */%lld", conn._M_filesize);

			if (!_M_headers.add(header_name::CONTENT_RANGE, value)) {
				return error::INTERNAL_SERVER_ERROR;
			}

			// Fall through.
		default:
			if (!_M_headers.add(header_name::CONTENT_TYPE, header_value("text/html; charset=UTF-8", 24))) {
				return false;
			}

			if (!_M_headers.add(header_name::CONTENT_LENGTH, (uint64_t) e->body.count())) {
				return false;
			}
	}

	if (!conn._M_out.format("HTTP/1.1 %u %s\r\n", e->status_code, e->reason_phrase)) {
		return false;
	}

	if (!_M_headers.serialize(conn._M_out)) {
		return false;
	}

	conn._M_bodyp = &e->body;

	return true;
}

const struct net::internet::http::error::err* net::internet::http::error::search(unsigned short status_code)
{
	int i = 0;
	int j = ARRAY_SIZE(_M_errors) - 1;

	while (i <= j) {
		int pivot = (i + j) / 2;
		if (status_code < _M_errors[pivot].status_code) {
			j = pivot - 1;
		} else if (status_code == _M_errors[pivot].status_code) {
			return &_M_errors[pivot];
		} else {
			i = pivot + 1;
		}
	}

	return NULL;
}
