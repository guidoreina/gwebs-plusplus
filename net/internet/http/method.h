#ifndef METHOD_H
#define METHOD_H

namespace net {
	namespace internet {
		namespace http {
			struct method {
				public:
					static const size_t METHOD_MAX_LEN   = 32;

					static const unsigned char CONNECT   = 0;
					static const unsigned char COPY      = 1;
					static const unsigned char DELETE    = 2;
					static const unsigned char GET       = 3;
					static const unsigned char HEAD      = 4;
					static const unsigned char LOCK      = 5;
					static const unsigned char MKCOL     = 6;
					static const unsigned char MOVE      = 7;
					static const unsigned char OPTIONS   = 8;
					static const unsigned char POST      = 9;
					static const unsigned char PROPFIND  = 10;
					static const unsigned char PROPPATCH = 11;
					static const unsigned char PUT       = 12;
					static const unsigned char TRACE     = 13;
					static const unsigned char UNLOCK    = 14;
					static const unsigned char UNKNOWN   = 15;

					unsigned char value;

					// Constructor.
					method();
					method(unsigned char v);

					const char* name() const;

					bool operator==(unsigned char v) const;
					bool operator!=(unsigned char v) const;

					// Search.
					static method search(const char* name, size_t len);

				private:
					struct _method {
						const char* name;
						size_t len;
					};

					static const struct _method _M_methods[];
			};

			inline method::method()
			{
			}

			inline method::method(unsigned char v)
			{
				value = v;
			}

			inline const char* method::name() const
			{
				if (value >= UNKNOWN) {
					return NULL;
				}

				return _M_methods[value].name;
			}

			inline bool method::operator==(unsigned char v) const
			{
				return (v == value);
			}

			inline bool method::operator!=(unsigned char v) const
			{
				return (v != value);
			}
		}
	}
}

#endif // METHOD_H
