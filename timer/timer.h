#ifndef TIMER_H
#define TIMER_H

#include "timer/event_handler.h"

namespace timer {
	struct timer {
		unsigned msec;
		event_handler* handler;
		unsigned id;

		bool operator<(const timer& t) const;
		bool operator==(const timer& t) const;

		int operator-(const timer& t) const;
	};

	inline bool timer::operator<(const timer& t) const
	{
		return ((int) (msec - t.msec) < 0);
	}

	inline bool timer::operator==(const timer& t) const
	{
		return msec == t.msec;
	}

	inline int timer::operator-(const timer& t) const
	{
		return (int) (msec - t.msec);
	}
}

#endif // TIMER_H
