#include <stdlib.h>
#include <sys/socket.h>
#include "net/listener.h"
#include "net/tcp_server.h"

net::tcp_server* net::listener::_M_server = NULL;

bool net::listener::on_readable()
{
	socket s;
	socket_address addr;
	if (accept(s, addr, 0)) {
		if (!_M_server->on_new_connection(s, addr, this)) {
			s.close();
		}
	}

	return true;
}
