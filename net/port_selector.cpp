#include <stdlib.h>
#include <unistd.h>
#include <poll.h>
#include "net/port_selector.h"

const unsigned net::iselector::READ = POLLIN;
const unsigned net::iselector::WRITE = POLLOUT;

net::selector::selector()
{
	_M_fd = -1;

	_M_port_events = NULL;
	_M_events = NULL;
}

net::selector::~selector()
{
	if (_M_fd != -1) {
		close(_M_fd);
	}

	if (_M_port_events) {
		free(_M_port_events);
	}

	if (_M_events) {
		free(_M_events);
	}
}

bool net::selector::create()
{
	if (!_M_fdset.create()) {
		return false;
	}

	if ((_M_fd = port_create()) < 0) {
		return false;
	}

	if ((_M_port_events = (struct port_event_t*) malloc(_M_fdset.size() * sizeof(struct port_event_t))) == NULL) {
		return false;
	}

	if ((_M_events = (int*) malloc(_M_fdset.size() * sizeof(int))) == NULL) {
		return false;
	}

	return true;
}

bool net::selector::add(unsigned fd, fdset::fdtype type, io::event_handler* handler, unsigned events)
{
	if (!_M_fdset.add(fd, type, handler)) {
		// The file descriptor has been already inserted.
		return true;
	}

	if (port_associate(_M_fd, PORT_SOURCE_FD, (uintptr_t) fd, events, NULL) < 0) {
		_M_fdset.remove(fd);
		return false;
	}

	_M_events[fd] = events;

	return true;
}

bool net::selector::remove(unsigned fd)
{
	if (!_M_fdset.remove(fd)) {
		// The file descriptor has not been inserted.
		return true;
	}

	if (port_dissociate(_M_fd, PORT_SOURCE_FD, (uintptr_t) fd) < 0) {
		close(fd);
		return false;
	}

	close(fd);

	return true;
}

bool net::selector::modify(unsigned fd, unsigned events)
{
	if (_M_fdset.index(fd) < 0) {
		// The file descriptor has not been inserted.
		return false;
	}

	_M_events[fd] = events;

	return true;
}

bool net::selector::process_events()
{
	uint_t nget = 1;
	if (port_getn(_M_fd, _M_port_events, _M_fdset.size(), &nget, NULL) < 0) {
		post_events_wait();
		return false;
	}

	post_events_wait();

	process(nget);

	return true;
}

bool net::selector::process_events(unsigned timeout)
{
	struct timespec ts;
	ts.tv_sec = timeout / 1000;
	ts.tv_nsec = (timeout % 1000) * 1000000;

	uint_t nget = 1;
	if ((port_getn(_M_fd, _M_port_events, _M_fdset.size(), &nget, &ts) < 0) || (nget == 0)) {
		post_events_wait();
		return false;
	}

	post_events_wait();

	process(nget);

	return true;
}

void net::selector::process(unsigned nevents)
{
	for (unsigned i = 0; i < nevents; i++) {
		unsigned fd = (unsigned) _M_events[i].portev_object;

		io::event_handler* handler;
		if ((handler = _M_fdset.handler(fd)) == NULL) {
			continue;
		}

		if (_M_events[i].portev_events & POLLIN) {
			if (!handler->on_readable()) {
				// Remove from set (the file descriptor is already dissociated).
				_M_fdset.remove(fd);
				close(fd);

				continue;
			}
		}

		if (_M_events[i].portev_events & POLLOUT) {
			if (!handler->on_writable()) {
				// Remove from set (the file descriptor is already dissociated).
				_M_fdset.remove(fd);
				close(fd);

				continue;
			}
		}

		// Reassociate the file descriptor.
		if (port_associate(_M_fd, PORT_SOURCE_FD, (uintptr_t) fd, _M_events[fd], NULL) < 0) {
			_M_fdset.remove(fd);
			close(fd);
		}
	}
}
