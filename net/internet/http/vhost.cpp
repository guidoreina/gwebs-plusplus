#include "net/internet/http/vhost.h"

bool net::internet::http::vhost::add_index(const char* s, unsigned short n, const char* mime_type, unsigned short mime_type_len)
{
	if (_M_parent != this) {
		return false;
	}

	if (_M_indices.used == _M_indices.size) {
		size_t size = (_M_indices.size == 0) ? INDEX_ALLOC : _M_indices.size * 2;

		struct index* indices;
		if ((indices = (struct index*) realloc(_M_indices.indices, size * sizeof(struct index))) == NULL) {
			return false;
		}

		_M_indices.indices = indices;
		_M_indices.size = size;
	}

	size_t off = _M_buf.count();

	if (!_M_buf.append_nul_terminated_string(s, n)) {
		return false;
	}

	struct index* idx = &_M_indices.indices[_M_indices.used];

	idx->index = off;
	idx->indexlen = n;

	idx->mime_type = mime_type;
	idx->mime_type_len = mime_type_len;

	_M_indices.used++;

	return true;
}
