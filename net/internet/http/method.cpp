#include <stdlib.h>
#include <string.h>
#include "net/internet/http/method.h"
#include "macros/macros.h"

const struct net::internet::http::method::_method net::internet::http::method::_M_methods[] = {
	{"CONNECT", 7},
	{"COPY", 4},
	{"DELETE", 6},
	{"GET", 3},
	{"HEAD", 4},
	{"LOCK", 4},
	{"MKCOL", 5},
	{"MOVE", 4},
	{"OPTIONS", 7},
	{"POST", 4},
	{"PROPFIND", 8},
	{"PROPPATCH", 9},
	{"PUT", 3},
	{"TRACE", 5},
	{"UNLOCK", 6}
};

net::internet::http::method net::internet::http::method::search(const char* name, size_t len)
{
	int i = 0;
	int j = ARRAY_SIZE(_M_methods) - 1;

	while (i <= j) {
		int pivot = (i + j) / 2;
		int ret = strncmp(name, _M_methods[pivot].name, len);
		if (ret < 0) {
			j = pivot - 1;
		} else if (ret == 0) {
			if (len < _M_methods[pivot].len) {
				j = pivot - 1;
			} else if (len == _M_methods[pivot].len) {
				return method(pivot);
			} else {
				i = pivot + 1;
			}
		} else {
			i = pivot + 1;
		}
	}

	return method(UNKNOWN);
}
