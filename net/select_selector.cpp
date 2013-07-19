#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include "net/select_selector.h"

const unsigned net::iselector::READ = 1;
const unsigned net::iselector::WRITE = 2;

net::selector::selector()
{
	FD_ZERO(&_M_rfds);
	FD_ZERO(&_M_wfds);

	_M_highest_fd = EMPTY_SET;
}

bool net::selector::add(unsigned fd, fdset::fdtype type, io::event_handler* handler, unsigned events)
{
	if (!_M_fdset.add(fd, type, handler)) {
		// The file descriptor has been already inserted.
		return true;
	}

	if (events & READ) {
		FD_SET(fd, &_M_rfds);
	}

	if (events & WRITE) {
		FD_SET(fd, &_M_wfds);
	}

	if ((_M_highest_fd == EMPTY_SET) || ((_M_highest_fd != UNDEFINED) && ((int) fd > _M_highest_fd))) {
		_M_highest_fd = fd;
	}

	return true;
}

bool net::selector::remove(unsigned fd)
{
	if (!_M_fdset.remove(fd)) {
		// The file descriptor has not been inserted.
		return true;
	}

	FD_CLR(fd, &_M_rfds);
	FD_CLR(fd, &_M_wfds);

	// If the file descriptor was the highest-numbered file descriptor...
	if ((int) fd == _M_highest_fd) {
		_M_highest_fd = UNDEFINED;
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

	FD_CLR(fd, &_M_rfds);
	FD_CLR(fd, &_M_wfds);

	if (events & READ) {
		FD_SET(fd, &_M_rfds);
	}

	if (events & WRITE) {
		FD_SET(fd, &_M_wfds);
	}

	return true;
}

bool net::selector::process_events()
{
	fd_set rfds = _M_rfds;
	fd_set wfds = _M_wfds;

	int ret;
	if ((ret = select(highest_fd() + 1, &rfds, &wfds, NULL, NULL)) <= 0) {
		post_events_wait();
		return false;
	}

	post_events_wait();

	process(&rfds, &wfds, ret);

	return true;
}

bool net::selector::process_events(unsigned timeout)
{
	struct timeval tv;
	tv.tv_sec = timeout / 1000;
	tv.tv_usec = (timeout % 1000) * 1000;

	fd_set rfds = _M_rfds;
	fd_set wfds = _M_wfds;

	int ret;
	if ((ret = select(highest_fd() + 1, &rfds, &wfds, NULL, &tv)) <= 0) {
		post_events_wait();
		return false;
	}

	post_events_wait();

	process(&rfds, &wfds, ret);

	return true;
}

void net::selector::process(const fd_set* rfds, const fd_set* wfds, unsigned nevents)
{
	size_t i = 0;
	while ((i < _M_fdset.count()) && (nevents > 0)) {
		unsigned fd = _M_fdset.fd(i);
		unsigned events = 0;

		if (FD_ISSET(fd, rfds)) {
			events |= READ;
			nevents--;
		}

		if (FD_ISSET(fd, wfds)) {
			events |= WRITE;
			nevents--;
		}

		if (events) {
			io::event_handler* handler;
			if ((handler = _M_fdset.handler(fd)) != NULL) {
				if (events & READ) {
					if (!handler->on_readable()) {
						// Remove from set.
						remove(fd);
						continue;
					}
				}

				if (events & WRITE) {
					if (!handler->on_writable()) {
						// Remove from set.
						remove(fd);
						continue;
					}
				}
			}
		}

		i++;
	}
}
