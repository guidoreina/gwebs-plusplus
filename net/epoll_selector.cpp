#include <stdlib.h>
#include <unistd.h>
#include "net/epoll_selector.h"

const unsigned net::iselector::READ = EPOLLIN;
const unsigned net::iselector::WRITE = EPOLLOUT;

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

	if ((_M_fd = epoll_create(_M_fdset.size())) < 0) {
		return false;
	}

	if ((_M_events = (struct epoll_event*) malloc(_M_fdset.size() * sizeof(struct epoll_event))) == NULL) {
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

	struct epoll_event ev;

	if (type == fdset::FD_SOCKET) {
		ev.events = EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLET;
	} else {
		ev.events = EPOLLIN;
	}

	ev.data.u64 = 0;
	ev.data.fd = fd;

	if (epoll_ctl(_M_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
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

	struct epoll_event ev;
	ev.events = EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLET;
	ev.data.u64 = 0;

	if (epoll_ctl(_M_fd, EPOLL_CTL_DEL, fd, &ev) < 0) {
		close(fd);
		return false;
	}

	close(fd);

	return true;
}

bool net::selector::process_events()
{
	int ret;
	if ((ret = epoll_wait(_M_fd, _M_events, _M_fdset.size(), -1)) <= 0) {
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
	if ((ret = epoll_wait(_M_fd, _M_events, _M_fdset.size(), timeout)) <= 0) {
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
		unsigned fd = _M_events[i].data.fd;

		io::event_handler* handler;
		if ((handler = _M_fdset.handler(fd)) == NULL) {
			continue;
		}

		uint32_t events = _M_events[i].events;
		if (events & EPOLLIN) {
			if (!handler->on_readable()) {
				// Remove from set.
				remove(fd);
				continue;
			}
		}

		if (events & EPOLLOUT) {
			if (!handler->on_writable()) {
				// Remove from set.
				remove(fd);
			}
		}
	}
}
