#ifndef LOCAL_ADDRESS_H
#define LOCAL_ADDRESS_H

#include <string.h>
#include <sys/un.h>
#include "net/socket_address.h"

namespace net {
	class local_address : public sockaddr_un {
		public:
			// Constructor.
			local_address();

			// Cast operators.
			operator const socket_address*() const;
			operator socket_address*();

			operator const socket_address&() const;
			operator socket_address&();

			// Set path.
			bool set_path(const char* path);
	};

	inline local_address::local_address()
	{
		sun_family = AF_UNIX;
	}

	inline local_address::operator const socket_address*() const
	{
		return reinterpret_cast<const socket_address*>(this);
	}

	inline local_address::operator socket_address*()
	{
		return reinterpret_cast<socket_address*>(this);
	}

	inline local_address::operator const socket_address&() const
	{
		return reinterpret_cast<const socket_address&>(*this);
	}

	inline local_address::operator socket_address&()
	{
		return reinterpret_cast<socket_address&>(*this);
	}

	inline bool local_address::set_path(const char* path)
	{
		size_t len;
		if ((len = strlen(path)) >= sizeof(sun_path)) {
			return false;
		}

		memcpy(sun_path, path, len);
		sun_path[len] = 0;

		return true;
	}
}

#endif // LOCAL_ADDRESS_H
