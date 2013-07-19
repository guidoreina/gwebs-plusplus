#ifndef PORT_SELECTOR_H
#define PORT_SELECTOR_H

#include <port.h>
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

			struct port_event_t* _M_port_events;
			int* _M_events;

			// Process events.
			void process(unsigned nevents);
	};
}

#endif // PORT_SELECTOR_H
