gwebs++ is an asynchronous event-driven web server with HTTPS support which runs under Linux, FreeBSD and Opensolaris.

It is written in C++, but it doesn't make use of the STL or of any additional library.

It supports the following event notification mechanisms:

- epoll (edge-triggered) under Linux
- kqueue under FreeBSD
- port under Solaris
- poll
- select

It has the following main features:
- HTTP/1.1
- Multiport (it can listen in more than one port)
- HTTPS
- MIME types support
- Pipelining
- Virtual hosts
- Keep-Alive
- Directory listing (with optional footer file)
- Handling of the If-Modified-Since header
- HTTP ranges

To do:
- Logs
- Reverse proxy
- FastCGI
