#ifndef UTIL_NUMBER_H
#define UTIL_NUMBER_H

#include <sys/types.h>
#include <stdint.h>
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

			static parse_result parse(const void* buf, size_t len, int32_t& n, int32_t min = INT_MIN, int32_t max = INT_MAX);
			static parse_result parse(const void* buf, size_t len, uint32_t& n, uint32_t min = 0, uint32_t max = UINT_MAX);
			static parse_result parse(const void* buf, size_t len, int64_t& n, int64_t min = LLONG_MIN, int64_t max = LLONG_MAX);
			static parse_result parse(const void* buf, size_t len, uint64_t& n, uint64_t min = 0, uint64_t max = ULLONG_MAX);
	};
}

#endif // UTIL_NUMBER_H
