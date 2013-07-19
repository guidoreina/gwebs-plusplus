#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <memory>
#include "net/tcp_server.h"
#include "net/listener.h"
#include "net/tcp_connection.h"

net::tcp_server::tcp_server(bool client_writes_first, bool have_timer)
{
	listener::_M_server = this;
	tcp_connection::_M_server = this;

	_M_listeners = NULL;
	_M_nlisteners = 0;

	_M_connections = NULL;

	_M_client_writes_first = client_writes_first;

	update_time();

	_M_have_timer = have_timer;

	_M_handle_alarm = false;

	_M_must_stop = true;
}

net::tcp_server::~tcp_server()
{
	if (_M_listeners) {
		for (unsigned i = 0; i < _M_nlisteners; i++) {
			delete _M_listeners[i];
		}

		free(_M_listeners);
	}

	if (_M_connections) {
		free(_M_connections);
	}
}

bool net::tcp_server::create()
{
	if (!selector::create()) {
		return false;
	}

	if ((_M_connections = (tcp_connection**) malloc(_M_fdset.size() * sizeof(tcp_connection*))) == NULL) {
		return false;
	}

	if (!create_connections()) {
		return false;
	}

	return true;
}

bool net::tcp_server::start()
{
	if (_M_nlisteners == 0) {
		return false;
	}

	_M_must_stop = false;

	do {
		if (_M_handle_alarm) {
			handle_alarm();
			_M_handle_alarm = false;
		}

		process_events(1000);

		handle_expired(_M_current_msec);
	} while (!_M_must_stop);

	return true;
}

bool net::tcp_server::on_new_connection(socket& client, const socket_address& addr, struct listener* listener)
{
	if (!allow_connection(addr, listener)) {
		return false;
	}

	if (!client.set_tcp_no_delay(true)) {
		return false;
	}

	tcp_connection* conn = _M_connections[client.fd()];

	// Add timer.
	if (!timer::timers::add(_M_current_msec + (tcp_connection::_M_max_idle_time * 1000), conn, 0, conn->_M_timer)) {
		return false;
	}

	if (!selector::add(client.fd(), fdset::FD_SOCKET, conn, _M_client_writes_first ? READ : WRITE)) {
		// Delete timer.
		del(conn->_M_timer);

		return false;
	}

	conn->fd(client.fd());

	conn->_M_listener = listener;

	conn->_M_timer_set = 1;

	return true;
}

void net::tcp_server::delete_connection(tcp_connection* conn)
{
	conn->free();
	remove(conn->fd());
}

bool net::tcp_server::listen(const socket_address& addr, void* data)
{
	for (unsigned i = 0; i < _M_nlisteners; i++) {
		if (addr == _M_listeners[i]->_M_addr) {
			// Already listening.
			return true;
		}
	}

	socket s;
	if (!s.listen(addr)) {
		return false;
	}

	if (!add_listener(s, addr, data)) {
		s.close();
		return false;
	}

	return true;
}

bool net::tcp_server::add_listener(const socket& s, const socket_address& addr, void* data)
{
	listener** listeners;
	if ((listeners = (listener**) realloc(_M_listeners, (_M_nlisteners + 1) * sizeof(listener*))) == NULL) {
		return false;
	}

	_M_listeners = listeners;

	listener* l;
	if ((l = new (std::nothrow) listener()) == NULL) {
		return false;
	}

	if (!selector::add(s.fd(), fdset::FD_LISTENER, l, selector::READ)) {
		delete l;
		return false;
	}

	l->fd(s.fd());

	l->_M_addr = addr;
	l->_M_data = data;

	_M_listeners[_M_nlisteners++] = l;

	return true;
}

void net::tcp_server::update_time()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);

	_M_current_time = tv.tv_sec;
	_M_current_msec = (unsigned long long) _M_current_time * 1000 + (tv.tv_usec / 1000);

	gmtime_r(&_M_current_time, &_M_gmtime);
	localtime_r(&_M_current_time, &_M_localtime);
}
