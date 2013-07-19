#include <stdlib.h>
#include "util/number.h"
#include "macros/macros.h"

size_t util::number::length(off_t number)
{
	size_t len = 1;

	if (number < 0) {
		number *= -1;
		len++;
	}

	while (number > 9) {
		number /= 10;
		len++;
	}

	return len;
}

util::number::parse_result util::number::parse(const void* buf, size_t len, unsigned& n, unsigned min, unsigned max)
{
	if (len == 0) {
		return PARSE_ERROR;
	}

	const unsigned char* ptr = (const unsigned char*) buf;
	const unsigned char* end = ptr + len;

	n = 0;

	while (ptr < end) {
		unsigned char c = *ptr++;
		if (!IS_DIGIT(c)) {
			return PARSE_ERROR;
		}

		unsigned tmp = (n * 10) + (c - '0');

		// Overflow?
		if (tmp < n) {
			return PARSE_ERROR;
		}

		n = tmp;
	}

	if (n < min) {
		return PARSE_UNDERFLOW;
	} else if (n > max) {
		return PARSE_OVERFLOW;
	}

	return PARSE_SUCCEEDED;
}

util::number::parse_result util::number::parse(const void* buf, size_t len, off_t& n, off_t min, off_t max)
{
	if (len == 0) {
		return PARSE_ERROR;
	}

	const unsigned char* ptr = (const unsigned char*) buf;
	const unsigned char* end = ptr + len;

	off_t sign;

	if (*ptr == '-') {
		if (len == 1) {
			return PARSE_ERROR;
		}

		sign = -1;

		ptr++;
	} else if (*ptr == '+') {
		if (len == 1) {
			return PARSE_ERROR;
		}

		sign = 1;

		ptr++;
	} else {
		sign = 1;
	}

	n = 0;

	while (ptr < end) {
		unsigned char c = *ptr++;
		if (!IS_DIGIT(c)) {
			return PARSE_ERROR;
		}

		off_t tmp = (n * 10) + (c - '0');

		// Overflow?
		if (tmp < n) {
			return PARSE_ERROR;
		}

		n = tmp;
	}

	n *= sign;

	if (n < min) {
		return PARSE_UNDERFLOW;
	} else if (n > max) {
		return PARSE_OVERFLOW;
	}

	return PARSE_SUCCEEDED;
}
