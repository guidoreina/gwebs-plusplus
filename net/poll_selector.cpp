#include <stdlib.h>
#include <unistd.h>
#include "net/poll_selector.h"

const unsigned net::iselector::READ = POLLIN;
const unsigned net::iselector::WRITE = POLLOUT;

net::selector::selector()
{
	_M_events = NULL;
}

net::selector::~selector()
{
	if (_M_events) {
		free(_M_events);
	}
}

bool net::selector::create()
{
	if (!_M_fdset.create()) {
		return false;
	}

	if ((_M_events = (struct pollfd*) malloc(_M_fdset.size() * sizeof(struct pollfd))) == NULL) {
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

	size_t index = _M_fdset.count() - 1;

	_M_events[index].fd = fd;
	_M_events[index].events = events;
	_M_events[index].revents = 0;

	return true;
}

bool net::selector::remove(unsigned fd)
{
	int index;
	if ((index = _M_fdset.index(fd)) < 0) {
		// The file descriptor has not been inserted.
		return true;
	}

	_M_fdset.remove(fd);

	if ((size_t) index < _M_fdset.count()) {
		_M_events[index] = _M_events[_M_fdset.count()];
	}

	close(fd);

	return true;
}

bool net::selector::modify(unsigned fd, unsigned events)
{
	int index;
	if ((index = _M_fdset.index(fd)) < 0) {
		// The file descriptor has not been inserted.
		return false;
	}

	_M_events[index].events = events;
	_M_events[index].revents = 0;

	return true;
}

bool net::selector::process_events()
{
	int ret;
	if ((ret = poll(_M_events, _M_fdset.count(), -1)) <= 0) {
		post_events_wait();
		return false;
	}

	post_events_wait();

	process(ret);

	return true;
}

bool net::selector::process_events(unsigned timeout)
{
	int ret;
	if ((ret = poll(_M_events, _M_fdset.count(), timeout)) <= 0) {
		post_events_wait();
		return false;
	}

	post_events_wait();

	process(ret);

	return true;
}

void net::selector::process(unsigned nevents)
{
	size_t i = 0;
	while ((i < _M_fdset.count()) && (nevents > 0)) {
		if (_M_events[i].revents != 0) {
			nevents--;

			unsigned fd = _M_events[i].fd;

			io::event_handler* handler;
			if ((handler = _M_fdset.handler(fd)) == NULL) {
				i++;
				continue;
			}

			if (_M_events[i].revents & POLLIN) {
				if (!handler->on_readable()) {
					// Remove from set.
					remove(fd);
					continue;
				}
			}

			if (_M_events[i].revents & POLLOUT) {
				if (!handler->on_writable()) {
					// Remove from set.
					remove(fd);
					continue;
				}
			}
		}

		i++;
	}
}
