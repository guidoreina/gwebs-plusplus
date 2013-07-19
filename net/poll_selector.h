#ifndef POLL_SELECTOR_H
#define POLL_SELECTOR_H

#include <poll.h>
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
			struct pollfd* _M_events;

			// Process events.
			void process(unsigned nevents);
	};
}

#endif // POLL_SELECTOR_H
