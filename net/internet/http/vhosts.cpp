#include <string.h>
#include "net/internet/http/vhosts.h"

bool net::internet::http::vhosts::add(vhost* v, bool default_vhost)
{
	if ((default_vhost) && (_M_default_vhost)) {
		return false;
	}

	size_t pos;
	if (search(v->name(), v->namelen(), v->port(), pos)) {
		// Duplicated virtual host.
		return false;
	}

	if (_M_used == _M_size) {
		size_t size = (_M_size == 0) ? VHOST_ALLOC : _M_size * 2;

		vhost** vhosts;
		if ((vhosts = (vhost**) realloc(_M_vhosts, size * sizeof(vhost*))) == NULL) {
			return false;
		}

		_M_vhosts = vhosts;
		_M_size = size;
	}

	if (pos < _M_used) {
		memmove(&_M_vhosts[pos + 1], &_M_vhosts[pos], (_M_used - pos) * sizeof(vhost*));
	}

	_M_vhosts[pos] = v;

	_M_used++;

	if (default_vhost) {
		_M_default_vhost = v;
	}

	return true;
}

bool net::internet::http::vhosts::search(const char* name, unsigned short namelen, unsigned short port, size_t& pos) const
{
	int i = 0;
	int j = _M_used - 1;

	while (i <= j) {
		int pivot = (i + j) / 2;

		const vhost* v = _M_vhosts[pivot];

		int ret = strncasecmp(name, v->name(), namelen);
		if (ret < 0) {
			j = pivot - 1;
		} else if (ret == 0) {
			if (namelen < v->namelen()) {
				j = pivot - 1;
			} else if (namelen == v->namelen()) {
				if (port < v->port()) {
					j = pivot - 1;
				} else if (port == v->port()) {
					pos = (size_t) pivot;
					return true;
				} else {
					i = pivot + 1;
				}
			} else {
				i = pivot + 1;
			}
		} else {
			i = pivot + 1;
		}
	}

	pos = (size_t) i;

	return false;
}
