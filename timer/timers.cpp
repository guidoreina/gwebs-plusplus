#include <stdlib.h>
#include "timer/timers.h"

void timer::timers::handle_expired(unsigned current_msec)
{
	util::red_black_tree<timer>::iterator it;

	while (_M_timers.begin(it)) {
		timer* t = it.data;

		if (current_msec < t->msec) {
			return;
		}

		t->handler->on_timer(t->id);

		_M_timers.erase(it);
	}
}
