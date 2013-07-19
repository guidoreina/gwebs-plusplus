#ifndef SELECT_SELECTOR_H
#define SELECT_SELECTOR_H

#include <sys/select.h>
#include "net/iselector.h"

namespace net {
	class selector : public iselector {
		public:
			// Modify descriptor.
			bool modify(unsigned fd, unsigned events);

		protected:
			// Constructor.
			selector();

			// Destructor.
			virtual ~selector();

			// Create.
			virtual bool create();

			// Add descriptor.
			bool add(unsigned fd, fdset::fdtype type, io::event_handler* handler, unsigned events);

			// Remove descriptor.
			bool remove(unsigned fd);

			// Process events.
			bool process_events();
			bool process_events(unsigned timeout);

		private:
			static const int UNDEFINED = -2;
			static const int EMPTY_SET = -1;

			fd_set _M_rfds;
			fd_set _M_wfds;

			int _M_highest_fd;

			// Process events.
			void process(const fd_set* rfds, const fd_set* wfds, unsigned nevents);

			// Compute highest file descriptor.
			int highest_fd();
	};

	inline selector::~selector()
	{
	}

	inline bool selector::create()
	{
		return _M_fdset.create();
	}

	inline int selector::highest_fd()
	{
		if (_M_highest_fd == UNDEFINED) {
			size_t count = _M_fdset.count();
			for (size_t i = 0; i < count; i++) {
				int fd;
				if ((fd = _M_fdset.fd(i)) > _M_highest_fd) {
					_M_highest_fd = fd;
				}
			}
		}

		return _M_highest_fd;
	}
}

#endif // SELECT_SELECTOR_H
