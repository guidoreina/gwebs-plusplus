#ifndef IO_EVENT_HANDLER_H
#define IO_EVENT_HANDLER_H

namespace io {
	class event_handler {
		public:
			// On readable.
			virtual bool on_readable() = 0;

			// On writable.
			virtual bool on_writable() = 0;
	};
}

#endif // IO_EVENT_HANDLER_H
