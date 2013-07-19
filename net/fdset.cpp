#include <stdlib.h>
#include <unistd.h>
#include <sys/resource.h>
#include "net/fdset.h"

net::fdset::fdset()
{
	_M_entries = NULL;
	_M_size = 0;
	_M_used = 0;

	_M_index = NULL;
}

net::fdset::~fdset()
{
	if (_M_index) {
		for (size_t i = 0; i < _M_used; i++) {
			close(_M_index[i]);
		}

		free(_M_index);
	}

	if (_M_entries) {
		free(_M_entries);
	}
}

bool net::fdset::create()
{
	// Get the maximum number of file descriptors.
	struct rlimit rlim;
	if (getrlimit(RLIMIT_NOFILE, &rlim) < 0) {
		return false;
	}

	if ((_M_entries = (struct fdentry*) malloc(rlim.rlim_cur * sizeof(struct fdentry))) == NULL) {
		return false;
	}

	if ((_M_index = (unsigned*) malloc(rlim.rlim_cur * sizeof(unsigned))) == NULL) {
		return false;
	}

	_M_size = rlim.rlim_cur;

	for (size_t i = 0; i < _M_size; i++) {
		_M_entries[i].index = -1;
	}

	return true;
}

bool net::fdset::add(unsigned fd, fdtype type, io::event_handler* handler)
{
	if (_M_entries[fd].index != -1) {
		// Already inserted.
		return false;
	}

	_M_entries[fd].index = _M_used;
	_M_entries[fd].type = type;
	_M_entries[fd].handler = handler;

	_M_index[_M_used++] = fd;

	return true;
}

bool net::fdset::remove(unsigned fd)
{
	int index;
	if ((index = _M_entries[fd].index) == -1) {
		// Not inserted.
		return false;
	}

	_M_entries[fd].index = -1;

	_M_used--;

	if ((size_t) index < _M_used) {
		_M_index[index] = _M_index[_M_used];
		_M_entries[_M_index[index]].index = index;
	}

	return true;
}
