#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <time.h>

#if HAVE_EPOLL
	#include "net/epoll_selector.h"
#elif HAVE_KQUEUE
	#include "net/kqueue_selector.h"
#elif HAVE_PORT
	#include "net/port_selector.h"
#elif HAVE_POLL
	#include "net/poll_selector.h"
#else
	#include "net/select_selector.h"
#endif

#include "net/socket.h"
#include "timer/timers.h"

namespace net {
	struct listener;
	struct tcp_connection;

	class tcp_server : public selector, public timer::timers {
		public:
			// Create.
			virtual bool create();

			// Start.
			virtual bool start();

			// Stop.
			void stop();

			// On alarm.
			void on_alarm();

			// On new connection.
			virtual bool on_new_connection(socket& client, const socket_address& addr, struct listener* listener);

			// Delete connection.
			void delete_connection(tcp_connection* conn);

			// Get UTC time.
			const struct tm& utc_time() const;

			// Get local time.
			const struct tm& local_time() const;

			// Get current milliseconds.
			unsigned current_msec() const;

		protected:
			// Listener sockets.
			listener** _M_listeners;
			unsigned _M_nlisteners;

			// TCP connections.
			tcp_connection** _M_connections;

			bool _M_client_writes_first;

			time_t _M_current_time;
			unsigned _M_current_msec;
			struct tm _M_gmtime;
			struct tm _M_localtime;

			bool _M_have_timer;

			bool _M_handle_alarm;

			bool _M_must_stop;

			// Constructor.
			tcp_server(bool client_writes_first, bool have_timer);

			// Destructor.
			virtual ~tcp_server();

			// Create connections.
			virtual bool create_connections() = 0;

			// Listen.
			bool listen(const socket_address& addr, void* data);

			// Allow connection?
			virtual bool allow_connection(const socket_address& addr, struct listener* listener);

			// Handle alarm.
			virtual void handle_alarm();

			// Post-events-wait.
			void post_events_wait();

		private:
			// Add listener.
			bool add_listener(const socket& s, const socket_address& addr, void* data);

			// Update time.
			void update_time();
	};

	inline void tcp_server::stop()
	{
		_M_must_stop = true;
	}

	inline void tcp_server::on_alarm()
	{
		_M_handle_alarm = true;
	}

	inline const struct tm& tcp_server::utc_time() const
	{
		return _M_gmtime;
	}

	inline const struct tm& tcp_server::local_time() const
	{
		return _M_localtime;
	}

	inline unsigned tcp_server::current_msec() const
	{
		return _M_current_msec;
	}

	inline bool tcp_server::allow_connection(const socket_address& addr, struct listener* listener)
	{
		return true;
	}

	inline void tcp_server::handle_alarm()
	{
		update_time();
	}

	inline void tcp_server::post_events_wait()
	{
		if (!_M_have_timer) {
			update_time();
		}
	}
}

#endif // TCP_SERVER_H
