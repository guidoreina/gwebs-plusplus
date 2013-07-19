#ifndef TIMERS_H
#define TIMERS_H

#include "timer/timer.h"
#include "util/red_black_tree.h"

namespace timer {
	class timers {
		public:
			// Constructor.
			timers();

			// Destructor.
			~timers();

			// Add timer.
			bool add(unsigned msec, event_handler* handler, unsigned id, util::red_black_tree<timer>::iterator& it);
			bool add(const timer& t, util::red_black_tree<timer>::iterator& it);

			// Delete timer.
			void del(util::red_black_tree<timer>::iterator& it);

		protected:
			// Handle expired.
			void handle_expired(unsigned current_msec);

		private:
			util::red_black_tree<timer> _M_timers;
	};

	inline timers::timers()
	{
	}

	inline timers::~timers()
	{
	}

	inline bool timers::add(unsigned msec, event_handler* handler, unsigned id, util::red_black_tree<timer>::iterator& it)
	{
		timer t;
		t.msec = msec;
		t.handler = handler;
		t.id = id;

		return _M_timers.insert(t, &it);
	}

	inline bool timers::add(const timer& t, util::red_black_tree<timer>::iterator& it)
	{
		return _M_timers.insert(t, &it);
	}

	inline void timers::del(util::red_black_tree<timer>::iterator& it)
	{
		_M_timers.erase(it);
	}
}

#endif // TIMERS_H
