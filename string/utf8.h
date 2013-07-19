#ifndef UTF8_H
#define UTF8_H

namespace string {
	class utf8 {
		public:
			// Length.
			static bool lengths(const char* s, size_t& len, size_t& utf8len);
			static bool length(const char* s, size_t len, size_t& utf8len);
	};
}

#endif // UTF8_H
