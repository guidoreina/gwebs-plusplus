#ifndef EPOLL_SELECTOR_H
#define EPOLL_SELECTOR_H

#include <sys/epoll.h>
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
			int _M_fd;

			struct epoll_event* _M_events;

			// Process events.
			void process(unsigned nevents);
	};

	inline bool selector::modify(unsigned fd, unsigned events)
	{
		return true;
	}
}

#endif // EPOLL_SELECTOR_H
