#include <stdlib.h>
#include <unistd.h>
#include "net/kqueue_selector.h"

const unsigned net::iselector::READ = 1;
const unsigned net::iselector::WRITE = 2;

net::selector::selector()
{
	_M_fd = -1;

	_M_events = NULL;
}

net::selector::~selector()
{
	if (_M_fd != -1) {
		close(_M_fd);
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

	if ((_M_fd = kqueue()) < 0) {
		return false;
	}

	if ((_M_events = (struct kevent*) malloc(_M_fdset.size() * sizeof(struct kevent))) == NULL) {
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

	struct kevent ev[2];
	unsigned nevents;

	if (type == fdset::FD_SOCKET) {
		EV_SET(&ev[0], fd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, NULL);
		EV_SET(&ev[1], fd, EVFILT_WRITE, EV_ADD | EV_CLEAR, 0, 0, NULL);

		nevents = 2;
	} else {
		EV_SET(&ev[0], fd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, NULL);
		nevents = 1;
	}

	struct timespec timeout = {0, 0};

	if (kevent(_M_fd, ev, nevents, NULL, 0, &timeout) < 0) {
		_M_fdset.remove(fd);
		return false;
	}

	return true;
}

bool net::selector::remove(unsigned fd)
{
	if (!_M_fdset.remove(fd)) {
		// The file descriptor has not been inserted.
		return true;
	}

	// Events which are attached to file descriptors are automatically deleted
	// on the last close of the descriptor.
	close(fd);

	return true;
}

bool net::selector::process_events()
{
	int ret;
	if ((ret = kevent(_M_fd, NULL, 0, _M_events, _M_fdset.size(), NULL)) <= 0) {
		post_events_wait();
		return false;
	}

	post_events_wait();

	process(ret);

	return true;
}

bool net::selector::process_events(unsigned timeout)
{
	struct timespec ts;
	ts.tv_sec = timeout / 1000;
	ts.tv_nsec = (timeout % 1000) * 1000000;

	int ret;
	if ((ret = kevent(_M_fd, NULL, 0, _M_events, _M_fdset.size(), &ts)) <= 0) {
		post_events_wait();
		return false;
	}

	post_events_wait();

	process(ret);

	return true;
}

void net::selector::process(unsigned nevents)
{
	for (unsigned i = 0; i < nevents; i++) {
		if (_M_events[i].flags & EV_ERROR) {
			continue;
		}

		unsigned fd = _M_events[i].ident;

		io::event_handler* handler;
		if ((handler = _M_fdset.handler(fd)) == NULL) {
			continue;
		}

		if (_M_events[i].filter == EVFILT_READ) {
			if (!handler->on_readable()) {
				// Remove from set.
				remove(fd);
			}
		} else if (_M_events[i].filter == EVFILT_WRITE) {
			if (!handler->on_writable()) {
				// Remove from set.
				remove(fd);
			}
		}
	}
}
