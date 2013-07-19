#ifndef ISELECTOR_H
#define ISELECTOR_H

#include "net/fdset.h"

namespace net {
	class iselector {
		public:
			static const unsigned READ;
			static const unsigned WRITE;

		protected:
			fdset _M_fdset;

			// Create.
			virtual bool create() = 0;

			// Add descriptor.
			virtual bool add(unsigned fd, fdset::fdtype type, io::event_handler* handler, unsigned events) = 0;

			// Remove descriptor.
			virtual bool remove(unsigned fd) = 0;

			// Modify descriptor.
			virtual bool modify(unsigned fd, unsigned events) = 0;

			// Process events.
			virtual bool process_events() = 0;
			virtual bool process_events(unsigned timeout) = 0;

			// Post-events-wait.
			virtual void post_events_wait() = 0;
	};
}

#endif // ISELECTOR_H
