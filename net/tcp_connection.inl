#ifndef TCP_CONNECTION_INL
#define TCP_CONNECTION_INL

#include "net/tcp_connection.h"
#include "net/tcp_server.h"

inline bool net::tcp_connection::modify(unsigned events)
{
	return _M_server->modify(_M_socket.fd(), events);
}

#endif // TCP_CONNECTION_INL
