#ifndef NUMBER_H
#define NUMBER_H

#include <sys/types.h>
#include <limits.h>

namespace util {
	class number {
		public:
			static size_t length(off_t number);

			// Parse.
			enum parse_result {
				PARSE_ERROR,
				PARSE_UNDERFLOW,
				PARSE_OVERFLOW,
				PARSE_SUCCEEDED
			};

			static parse_result parse(const void* buf, size_t len, unsigned& n, unsigned min = 0, unsigned max = UINT_MAX);
			static parse_result parse(const void* buf, size_t len, off_t& n, off_t min = LLONG_MIN, off_t max = LLONG_MAX);
	};
}

#endif // NUMBER_H
