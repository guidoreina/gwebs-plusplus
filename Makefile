CC=g++
CXXFLAGS=-O3 -Wall -pedantic -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -Wno-format -Wno-long-long -I.

ifeq ($(shell uname), Linux)
	CXXFLAGS+=-std=c++11

	CXXFLAGS+=-DHAVE_TCP_CORK -DHAVE_ACCEPT4 -DUSE_FIONBIO -DHAVE_EPOLL -DHAVE_POLL
	CXXFLAGS+=-DHAVE_SENDFILE -DHAVE_MMAP -DHAVE_PREAD -DHAVE_PWRITE -DHAVE_MEMRCHR
	CXXFLAGS+=-DHAVE_TIMEGM -DHAVE_MEMRCHR
	CXXFLAGS+=-DHAVE_SSL
else
	ifeq ($(shell uname), FreeBSD)
		CXXFLAGS+=-std=c++11

		CXXFLAGS+=-DHAVE_TCP_NOPUSH -DHAVE_KQUEUE -DHAVE_POLL
		CXXFLAGS+=-DHAVE_SENDFILE -DHAVE_MMAP -DHAVE_PREAD -DHAVE_PWRITE -DHAVE_MEMRCHR
		CXXFLAGS+=-DHAVE_TIMEGM -DHAVE_MEMRCHR
		CXXFLAGS+=-DHAVE_SSL
	else
		ifeq ($(shell uname), SunOS)
			CXXFLAGS+=-std=c++0x

			CXXFLAGS+=-DHAVE_PORT -DHAVE_POLL
			CXXFLAGS+=-DHAVE_SENDFILE -DHAVE_MMAP -DHAVE_PREAD -DHAVE_PWRITE
		endif
	endif
endif

LDFLAGS=
LIBS=-lssl -lcrypto

MAKEDEPEND=${CC} -MM
PROGRAM=gwebs++

OBJS =	constants/months_and_days.o \
	string/buffer.o string/memcasemem.o string/memrchr.o string/utf8.o \
	fs/file.o fs/directory.o html/html.o \
	timer/timers.o \
	util/ranges.o util/number.o util/configuration.o \
	net/socket_address.o net/ipv4_address.o net/ipv6_address.o net/socket.o \
	net/filesender.o net/listener.o net/fdset.o net/tcp_connection.o \
	net/tcp_server.o net/internet/url.o net/internet/mime/types.o \
	net/internet/http/headers.o net/internet/http/date.o \
	net/internet/http/method.o net/internet/scheme.o \
	net/internet/http/server.o net/internet/http/connection.o \
	net/internet/http/error.o net/internet/http/dirlisting.o \
	net/internet/http/vhost.o net/internet/http/vhosts.o \
	main.o

ifneq (,$(findstring HAVE_EPOLL, $(CXXFLAGS)))
	OBJS+=net/epoll_selector.o
else
	ifneq (,$(findstring HAVE_KQUEUE, $(CXXFLAGS)))
		OBJS+=net/kqueue_selector.o
	else
		ifneq (,$(findstring HAVE_PORT, $(CXXFLAGS)))
			OBJS+=net/port_selector.o
		else
			ifneq (,$(findstring HAVE_POLL, $(CXXFLAGS)))
				OBJS+=net/poll_selector.o
			else
				OBJS+=net/select_selector.o
			endif
		endif
	endif
endif

ifneq (,$(findstring HAVE_SSL, $(CXXFLAGS)))
	OBJS+=net/ssl_socket.o
endif

DEPS:= ${OBJS:%.o=%.d}

all: $(PROGRAM)

${PROGRAM}: ${OBJS}
	${CC} ${CXXFLAGS} ${LDFLAGS} ${OBJS} ${LIBS} -o $@

clean:
	rm -f ${PROGRAM} ${OBJS} ${OBJS} ${DEPS}

${OBJS} ${DEPS} ${PROGRAM} : Makefile

.PHONY : all clean

%.d : %.cpp
	${MAKEDEPEND} ${CXXFLAGS} $< -MT ${@:%.d=%.o} > $@

%.o : %.cpp
	${CC} ${CXXFLAGS} -c -o $@ $<

-include ${DEPS}
