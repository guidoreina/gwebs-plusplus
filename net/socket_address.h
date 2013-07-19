#ifndef SOCKET_ADDRESS_H
#define SOCKET_ADDRESS_H

#include <sys/socket.h>

namespace net {
	class socket_address : public sockaddr_storage {
		public:
			// Build.
			bool build(const char* address, unsigned short port);
			bool build(const char* address);

			// Comparison operator.
			bool operator==(const socket_address& addr) const;
	};
}

#endif // SOCKET_ADDRESS_H
