#ifndef FDSET_H
#define FDSET_H

#include "io/event_handler.h"

namespace net {
	class fdset {
		public:
			enum fdtype {
				FD_NONE,
				FD_SOCKET,
				FD_LISTENER
			};

			// Constructor.
			fdset();

			// Destructor.
			~fdset();

			// Create.
			bool create();

			// Get size.
			size_t size() const;

			// Get count.
			size_t count() const;

			// Add descriptor.
			bool add(unsigned fd, fdtype type, io::event_handler* handler);

			// Remove descriptor.
			bool remove(unsigned fd);

			// Get index.
			int index(unsigned fd) const;

			// Get type.
			fdtype type(unsigned fd) const;

			// Get I/O event handler.
			io::event_handler* handler(unsigned fd) const;

			// Get descriptor.
			int fd(unsigned index) const;

		protected:
			struct fdentry {
				int index;
				fdtype type;
				io::event_handler* handler;
			};

			fdentry* _M_entries;
			size_t _M_size;
			size_t _M_used;

			unsigned* _M_index;
	};

	inline size_t fdset::size() const
	{
		return _M_size;
	}

	inline size_t fdset::count() const
	{
		return _M_used;
	}

	inline int fdset::index(unsigned fd) const
	{
		return _M_entries[fd].index;
	}

	inline fdset::fdtype fdset::type(unsigned fd) const
	{
		return (_M_entries[fd].index != -1) ? _M_entries[fd].type : FD_NONE;
	}

	inline io::event_handler* fdset::handler(unsigned fd) const
	{
		return (_M_entries[fd].index != -1) ? _M_entries[fd].handler : NULL;
	}

	inline int fdset::fd(unsigned index) const
	{
		return (index < _M_used) ? _M_index[index] : -1;
	}
}

#endif // FDSET_H
