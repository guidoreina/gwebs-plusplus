#ifndef RANGES_H
#define RANGES_H

#include <sys/types.h>

namespace util {
	struct range {
		off_t from;
		off_t to;
	};

	class ranges {
		public:
			// Constructor.
			ranges();

			// Destructor.
			~ranges();

			// Free object.
			void free();

			// Reset object.
			void reset();

			// Get count.
			size_t count() const;

			// Get.
			bool get(unsigned idx, off_t& from, off_t& to) const;
			const range* get(unsigned idx) const;

			// Add range.
			bool add(off_t from, off_t to);

		private:
			static const size_t INITIAL_SIZE = 2;

			range* _M_ranges;
			size_t _M_size;
			size_t _M_used;
	};

	inline ranges::ranges()
	{
		_M_ranges = NULL;
		_M_size = 0;
		_M_used = 0;
	}

	inline ranges::~ranges()
	{
		free();
	}

	inline void ranges::reset()
	{
		_M_used = 0;
	}

	inline size_t ranges::count() const
	{
		return _M_used;
	}

	inline bool ranges::get(unsigned idx, off_t& from, off_t& to) const
	{
		if (idx >= _M_used) {
			return false;
		}

		from = _M_ranges[idx].from;
		to = _M_ranges[idx].to;

		return true;
	}

	inline const range* ranges::get(unsigned idx) const
	{
		if (idx >= _M_used) {
			return NULL;
		}

		return &_M_ranges[idx];
	}
}

#endif // RANGES_H
