#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <new>
#include "net/socket.h"
#include "net/ssl_socket.h"
#include "net/listener.h"
#include "net/internet/http/headers.h"
#include "net/internet/http/date.h"
#include "net/internet/http/connection.h"
#include "net/internet/http/server.h"
#include "net/internet/http/vhosts.h"
#include "net/internet/mime/types.h"
#include "util/configuration.h"
#include "fs/directory.h"
#include "util/number.h"
#include "util/ranges.h"
#include "net/ipv4_address.h"
#include "net/ipv6_address.h"

#define LISTENING_PORT 8000

#define TIMEOUT (30 * 1000)

static void ipv4_request();
static void ipv6_request();

static void ipv4_ssl_request();
static void ipv4_ssl_server();

static void ipv4_server();
static void ipv6_server();

static void test_headers();
static void test_date();
static void test_number();
//static void test_path_parser();
//static void test_path_request_line();
static void test_http_server();
static void test_mime_types();
static void test_directory();
static void test_vhosts();
static void test_configuration();

static bool fill_ipv4_address(const char* address, unsigned short port, net::ipv4_address& addr);
static bool fill_ipv6_address(const char* address, unsigned short port, net::ipv6_address& addr);

static void make_request(net::socket& s);
static void make_ssl_request(net::ssl_socket& s);
static void serve_requests(net::socket& s);
static void serve_ssl_requests(net::socket& s);

int main()
{
	//ipv4_request();
	//ipv6_request();

	//ipv4_ssl_request();
	//ipv4_ssl_server();

	//ipv4_server();
	//ipv6_server();

	//test_headers();
	//test_date();
	//test_number();

	//test_path_parser();
	//test_path_request_line();
	//test_http_server();
	//test_mime_types();
	//test_directory();
	//test_vhosts();
	test_configuration();

	return 0;
}

void ipv4_request()
{
	net::ipv4_address addr;
	if (!fill_ipv4_address("www.google.de", 80, addr)) {
		return;
	}

	// Connect.
	net::socket s;
	if (!s.connect(net::socket::STREAM, addr, TIMEOUT)) {
		fprintf(stderr, "Couldn't connect.\n");
		return;
	}

	make_request(s);

	s.close();
}

void ipv6_request()
{
	net::ipv6_address addr;
	if (!fill_ipv6_address(net::ipv6_address::ANY_ADDRESS, LISTENING_PORT, addr)) {
		return;
	}

	// Connect.
	net::socket s;
	if (!s.connect(net::socket::STREAM, addr, TIMEOUT)) {
		fprintf(stderr, "Couldn't connect.\n");
		return;
	}

	make_request(s);

	s.close();
}

void ipv4_ssl_request()
{
	if (!net::ssl_socket::init_ssl_library()) {
		fprintf(stderr, "Error initializing SSL library.\n");
		return;
	}

	net::ipv4_address addr;
	if (!fill_ipv4_address("www.google.de", 443, addr)) {
		net::ssl_socket::free_ssl_library();
		return;
	}

	// Connect.
	net::socket s;
	if (!s.connect(net::socket::STREAM, addr, TIMEOUT)) {
		fprintf(stderr, "Couldn't connect.\n");

		net::ssl_socket::free_ssl_library();
		return;
	}

	net::ssl_socket ssl(s);
	make_ssl_request(ssl);

	s.close();

	net::ssl_socket::free_ssl_library();
}

void ipv4_ssl_server()
{
	if (!net::ssl_socket::init_ssl_library()) {
		fprintf(stderr, "Error initializing SSL library.\n");
		return;
	}

	if (!net::ssl_socket::load_certificate("server.crt", "privkey.pem")) {
		fprintf(stderr, "Error loading certificate.\n");

		net::ssl_socket::free_ssl_library();
		return;
	}

	net::ipv4_address addr;
	if (!fill_ipv4_address(net::ipv4_address::ANY_ADDRESS, LISTENING_PORT, addr)) {
		net::ssl_socket::free_ssl_library();
		return;
	}

	// Create listener.
	net::socket s;
	if (!s.listen(addr)) {
		fprintf(stderr, "Couldn't create listener.\n");

		net::ssl_socket::free_ssl_library();
		return;
	}

	serve_ssl_requests(s);

	s.close();

	net::ssl_socket::free_ssl_library();
}

void ipv4_server()
{
	net::ipv4_address addr;
	if (!fill_ipv4_address(net::ipv4_address::ANY_ADDRESS, LISTENING_PORT, addr)) {
		return;
	}

	// Create listener.
	net::socket s;
	if (!s.listen(addr)) {
		fprintf(stderr, "Couldn't create listener.\n");
		return;
	}

	serve_requests(s);

	s.close();
}

void ipv6_server()
{
	net::ipv6_address addr;
	if (!fill_ipv6_address(net::ipv6_address::ANY_ADDRESS, LISTENING_PORT, addr)) {
		return;
	}

	// Create listener.
	net::socket s;
	if (!s.listen(addr)) {
		fprintf(stderr, "Couldn't create listener.\n");
		return;
	}

	serve_requests(s);

	s.close();
}

void test_headers()
{
	net::internet::http::headers headers(true);
	net::internet::http::header_name name(net::internet::http::header_name::SERVER);

	const char* v = "   			  gweb++/0.0.1		\r\n 				\r\n\t server";
	net::internet::http::header_value value(v, strlen(v));

	if (!headers.add(name, value)) {
		fprintf(stderr, "Couldn't insert field.\n");
	}

	name.value = net::internet::http::header_name::CONNECTION;

	v = "   			  close		\r\n 				\r\n";
	value.value = v;
	value.len = strlen(v);

	if (!headers.add(name, value)) {
		fprintf(stderr, "Couldn't insert field.\n");
	}

	string::buffer buf;
	if (!headers.serialize(buf)) {
		fprintf(stderr, "Couldn't serialize headers.\n");
	} else {
		printf("%.*s", buf.count(), buf.data());
	}

	printf("==================================================\n");

	headers.reset();

	int fd;
	if ((fd = open("headers.txt", O_RDONLY)) < 0) {
		fprintf(stderr, "Couldn't open file.\n");
		return;
	}

	net::internet::http::headers::parse_result res = net::internet::http::headers::PARSE_NOT_END_OF_HEADER;

	buf.reset();

	do {
		if (!buf.allocate(32)) {
			fprintf(stderr, "Couldn't allocate memory.\n");
			return;
		}

		ssize_t ret;
		if ((ret = read(fd, buf.data() + buf.count(), 32)) <= 0) {
			break;
		}

		buf.increment_count(ret);

		if ((res = headers.parse(buf.data(), buf.count())) != net::internet::http::headers::PARSE_NOT_END_OF_HEADER) {
			fprintf(stderr, "parse() returned %u.\n", res);
			break;
		}
	} while (true);

	close(fd);

	if (res == net::internet::http::headers::PARSE_END_OF_HEADER) {
		name.value = net::internet::http::header_name::UNKNOWN;

		name.name = "Test";
		name.len = 4;

		value.value = "value";
		value.len = 5;

		if (!headers.add(name, value)) {
			fprintf(stderr, "Couldn't insert field.\n");
		} else {
			buf.reset();
			if (!headers.serialize(buf)) {
				fprintf(stderr, "Couldn't serialize headers.\n");
			} else {
				printf("%.*s", buf.count(), buf.data());
			}

			headers.remove(net::internet::http::header_name::CONNECTION);

			headers.remove("Test", 4);

			printf("=========================================================\n");
			buf.reset();
			if (!headers.serialize(buf)) {
				fprintf(stderr, "Couldn't serialize headers.\n");
			} else {
				printf("%.*s", buf.count(), buf.data());
			}

			util::ranges ranges;
			if (headers.get_ranges(1000, ranges)) {
				printf("Number of ranges: %u.\n", ranges.count());

				off_t from, to;
				for (size_t i = 0; ranges.get(i, from, to); i++) {
					printf("[%lld, %lld]\n", from, to);
				}
			} else {
				printf("get_ranges failed.\n");
			}
		}
	}
}

void test_date()
{
	time_t t;
	struct tm timestamp;

#define DATE "Sunday,06-Nov-94 08:49:37 GMT"
	if ((t = net::internet::http::date::parse(DATE, sizeof(DATE) - 1, timestamp)) != (time_t) -1) {
		printf("%04u/%02u/%02u %02u:%02u:%02u\n", 1900 + timestamp.tm_year, 1 + timestamp.tm_mon, timestamp.tm_mday, timestamp.tm_hour, timestamp.tm_min, timestamp.tm_sec);
	}
}

void test_number()
{
	off_t number = -99999999;
	printf("Length of %lld = %u.\n", number, util::number::length(number));

	util::number::parse_result res;
	unsigned n;
	res = util::number::parse("1234", 4, n);
	if (res == util::number::PARSE_SUCCEEDED) {
		printf("n = %u.\n", n);
	} else {
		printf("res = %u.\n", (unsigned) res);
	}

	off_t off;
	res = util::number::parse("-1234", 5, off);
	if (res == util::number::PARSE_SUCCEEDED) {
		printf("off = %lld.\n", off);
	} else {
		printf("res = %u.\n", (unsigned) res);
	}
}

/*void test_path_parser()
{
	net::internet::http::connection conn;

	do {
		char line[1024];
		if (fgets(line, sizeof(line), stdin)) {
			size_t len;
			if ((len = strlen(line)) == 0) {
				continue;
			}

			if (line[len - 1] == '\n') {
				line[--len] = 0;
			}

			if (len == 0) {
				continue;
			} else if ((len == 4) && (strcasecmp(line, "quit") == 0)) {
				break;
			}

			conn._M_in.reset();
			if (!conn._M_in.append(line, len)) {
				fprintf(stderr, "Couldn't allocate memory.\n");
				break;
			}

			conn._M_path.reset();

			conn._M_pathlen = len;
			conn._M_token = 0;

			unsigned short ret;
			if ((ret = conn.parse_path()) != 0) {
				printf("parse_path() returned %u.\n", ret);
			} else {
				if (conn._M_path.count() == 0) {
					printf("Final path: [/]\n");
				} else {
					printf("Final path: [%.*s]\n", conn._M_path.count(), conn._M_path.data());
				}
			}
		}
	} while (true);
}

void test_path_request_line()
{
	net::internet::http::connection conn;

	do {
		char line[1024];
		if (fgets(line, sizeof(line), stdin)) {
			size_t len;
			if ((len = strlen(line)) == 0) {
				continue;
			}

			if (len == 0) {
				continue;
			} else if ((len == 5) && (strcasecmp(line, "quit\n") == 0)) {
				break;
			}

			conn.free();
			conn._M_substate = 0;
			if (!conn._M_in.append(line, len)) {
				fprintf(stderr, "Couldn't allocate memory.\n");
				break;
			}

			conn._M_inp = 0;

			unsigned short ret;
			if ((ret = conn.parse_request_line()) != 0) {
				printf("parse_request_line() returned %u.\n", ret);
			} else {
				if ((ret = conn.parse_path()) != 0) {
					printf("parse_path() returned %u.\n", ret);
				} else {
					switch (conn._M_http_version) {
						case net::internet::http::connection::HTTP_0_9:
							printf("HTTP/0.9\n");
							break;
						case net::internet::http::connection::HTTP_1_0:
							printf("HTTP/1.0\n");
							break;
						case net::internet::http::connection::HTTP_1_1:
							printf("HTTP/1.1\n");
							break;
					}

					printf("Method: %s.\n", conn._M_method.name());

					if (conn._M_hostlen > 0) {
						printf("Host: [%.*s:%u].\n", conn._M_hostlen, conn._M_in.data() + conn._M_host, conn._M_port);
					}

					if (conn._M_path.count() == 0) {
						printf("Final path: [/]\n");
					} else {
						printf("Final path: [%.*s]\n", conn._M_path.count(), conn._M_path.data());
					}

					if (conn._M_extension != 0) {
						printf("Extension: [%.*s]\n", conn._M_pathlen - conn._M_extension, conn._M_path.data() + conn._M_extension);
					}

					if (conn._M_querylen > 0) {
						printf("Query: [%.*s].\n", conn._M_querylen, conn._M_in.data() + conn._M_query);
					}

					if (conn._M_fragmentlen > 0) {
						printf("Fragment: [%.*s].\n", conn._M_fragmentlen, conn._M_in.data() + conn._M_fragment);
					}
				}
			}
		}
	} while (true);
}
*/

void test_http_server()
{
	net::internet::http::server server;
}

void test_mime_types()
{
	net::internet::mime::types types;
	if (!types.load()) {
		fprintf(stderr, "Couldn't load MIME types.\n");
		return;
	}

	do {
		char line[1024];
		if (!fgets(line, sizeof(line), stdin)) {
			continue;
		}

		size_t len = strlen(line);
		if ((len == 5) && (strncasecmp(line, "quit\n", 5) == 0)) {
			break;
		}

		if ((len > 0) && (line[len - 1] == '\n')) {
			len--;
		}

		unsigned short l;
		const char* mime_type = types.mime_type(line, len, l);
		printf("MIME type: %.*s\n", l, mime_type);
	} while (true);
}

void test_directory()
{
#define DIRECTORY "/home/guido/"

	fs::directory dir;
	if (!dir.read(DIRECTORY, sizeof(DIRECTORY) - 1, fs::directory::SORT_BY_NAME, fs::directory::ASCENDING)) {
		fprintf(stderr, "Couldn't read directory %s.\n", DIRECTORY);
		return;
	}

	printf("# of files: %u.\n", dir.get_file_count());
	printf("# of directories: %u.\n", dir.get_directory_count());

	printf("Files:\n");
	const fs::directory::entry* entry;
	for (size_t i = 0; ((entry = dir.get_file(i)) != NULL); i++) {
		printf("\t%s, size: %lld, mtime = %ld.\n", entry->name, entry->size, entry->mtime);
	}

	printf("Directories:\n");
	for (size_t i = 0; ((entry = dir.get_directory(i)) != NULL); i++) {
		printf("\t%s, size: %lld, mtime = %ld.\n", entry->name, entry->size, entry->mtime);
	}
}

void test_vhosts()
{
	net::internet::http::vhost* v1;
	if ((v1 = new (std::nothrow) net::internet::http::vhost()) == NULL) {
		printf("Couldn't create virtual host.\n");
		return;
	}

	v1->name("www.example.com", 15);
	v1->root("/home/guido1", 12);

	net::internet::http::vhosts vhosts;
	if (!vhosts.add(v1, false)) {
		printf("Couldn't add virtual host.\n");
		delete v1;

		return;
	}

	net::internet::http::vhost* v2;
	if ((v2 = new (std::nothrow) net::internet::http::vhost()) == NULL) {
		printf("Cannot create virtual host.\n");
		return;
	}

	v2->name("www.exampla.com", 15);
	v2->root("/home/guido2", 12);

	if (!vhosts.add(v2, true)) {
		printf("Couldn't add virtual host.\n");
		delete v2;

		return;
	}

	net::internet::http::vhost* alias;
	if ((alias = new (std::nothrow) net::internet::http::vhost(v1)) == NULL) {
		printf("Cannot create virtual host.\n");
		return;
	}

	alias->name("www.alias.com", 13);

	if (!vhosts.add(alias, false)) {
		printf("Couldn't add virtual host.\n");
		delete alias;

		return;
	}

	net::internet::http::vhost* v;
	if ((v = vhosts.find("www.alias.com", 13)) == NULL) {
		printf("[Not expected] Couldn't find host www.alias.com.\n");
	} else {
		printf("Root of www.alias.com is %s.\n", v->root());
	}

	if ((v = vhosts.find("www.abc.com", 11)) == NULL) {
		printf("[Expected] Couldn't find host www.abc.com.\n");
	} else {
		printf("[Not expected] Host www.abc.com found.\n");
	}
}

static void print_key(const struct util::configuration::key* k, unsigned depth)
{
	for (unsigned i = 0; i < depth; i++) {
		printf("\t");
	}

	if (k->type() == util::configuration::KEY_HAS_VALUE) {
		printf("%s = %s\n", k->name(), k->value());
	} else {
		printf("%s\n", k->name());
	}
}

static void print_keys_forward(const util::configuration& conf, util::configuration::iterator& it, unsigned depth)
{
	do {
		print_key(it.key, depth);

		if (it.key->children_count() > 0) {
			util::configuration::iterator children = it;
			if (conf.first_child(children)) {
				print_keys_forward(conf, children, depth + 1);
			}
		}
	} while (conf.next(it));
}

static void print_keys_backward(const util::configuration& conf, util::configuration::iterator& it, unsigned depth)
{
	do {
		print_key(it.key, depth);

		if (it.key->children_count() > 0) {
			util::configuration::iterator children = it;
			if (conf.last_child(children)) {
				print_keys_backward(conf, children, depth + 1);
			}
		}
	} while (conf.previous(it));
}

void test_configuration()
{
	util::configuration conf;
	if (!conf.load("config.cfg")) {
		printf("Couldn't load configuration file.\n");
		return;
	}

	conf.print();

	size_t count;
	if (conf.get_children_count(count, NULL)) {
		printf("# children in the root: %u\n", count);
	}

	if (conf.get_children_count(count, "3", "3.1", "3.1.1", NULL)) {
		printf("# children of 3 -> 3.1 -> 3.1.1: %u.\n", count);
	}

	if (conf.get_children_count(count, "3", "3.1", "3.1.1", "3.1.1.1", NULL)) {
		printf("# children of 3 -> 3.1 -> 3.1.1 -> 3.1.1.1: %u.\n", count);
	}

	if (!conf.get_children_count(count, "3", "3.1", "3.1.1", "3.1.1.2", NULL)) {
		printf("3 -> 3.1 -> 3.1.1 -> 3.1.1.2 has a value and no children.\n");
	}

	const char* key;
	unsigned short keylen;
	if (conf.get_key(key, keylen, 3, NULL)) {
		printf("Fourth key is %s.\n", key);
	}

	if (conf.get_key(key, keylen, 1, "1", NULL)) {
		printf("Second key of 1 is %s.\n", key);
	}

	if (!conf.get_key(key, keylen, 0, "3", "3.1", "3.1.1", "3.1.1.1", NULL)) {
		printf("Key 3 -> 3.1 -> 3.1.1 -> 3.1.1.1 has no children.\n");
	}

	if (conf.get_key(key, keylen, 1, "3", "3.1", "3.1.1", NULL)) {
		printf("Second key of 3 -> 3.1 -> 3.1.1 is %s.\n", key);
	}

	const char* value;
	unsigned short valuelen;
	if (conf.get_value(value, valuelen, "0", NULL)) {
		printf("Value of 0 is %s.\n", value);
	}

	if (conf.get_value(value, valuelen, "3", "3.1", "3.1.1", "3.1.1.2", NULL)) {
		printf("Value of 3 -> 3.1 -> 3.1.1 -> 3.1.1.2 is %s.\n", value);
	}

	if (!conf.get_value(value, valuelen, "7", NULL)) {
		printf("Key 7 not found or has no value.\n");
	}

	if (conf.get_value(value, valuelen, "1", "1.2", NULL)) {
		printf("Value of 1 -> 1.2 is %s.\n", value);
	}

	printf("Forward iterator:\n");

	util::configuration::iterator it;
	if (conf.begin(it)) {
		print_keys_forward(conf, it, 0);
	}

	printf("Backward iterator:\n");
	if (conf.end(it)) {
		print_keys_backward(conf, it, 0);
	}
}

bool fill_ipv4_address(const char* address, unsigned short port, net::ipv4_address& addr)
{
	struct hostent* res;
	if ((res = gethostbyname(address)) == NULL) {
		perror("gethostbyname");
		return false;
	}

	memcpy(&addr.sin_addr, res->h_addr_list[0], res->h_length);

	addr.port(port);

	return true;
}

bool fill_ipv6_address(const char* address, unsigned short port, net::ipv6_address& addr)
{
	// Resolve host.
	if (inet_pton(AF_INET6, address, &addr.sin6_addr) <= 0) {
		perror("inet_pton");
		return false;
	}

	addr.port(port);

	return true;
}

void make_request(net::socket& s)
{
	// Send request.
#define REQUEST "GET / HTTP/1.1\r\nHost: www.google.de\r\nConnection: close\r\n\r\n"

	const char* b = REQUEST;
	size_t len = sizeof(REQUEST) - 1;

	size_t written = 0;

	do {
		ssize_t ret;
		if ((ret = s.write(b + written, len - written, TIMEOUT)) < 0) {
			fprintf(stderr, "Error writing.\n");
			return;
		}

		written += ret;
	} while (written < len);

	// Receive response.
	char buf[4 * 1024];
	do {
		ssize_t ret;
		if ((ret = s.read(buf, sizeof(buf), TIMEOUT)) <= 0) {
			break;
		}

		printf("%.*s", ret, buf);
	} while (true);

	printf("\n");
}

void make_ssl_request(net::ssl_socket& s)
{
	// Perform TLS/SSL handshake.
	if (!s.handshake(net::ssl_socket::CLIENT_MODE, TIMEOUT)) {
		fprintf(stderr, "Error performing handshake.\n");
		return;
	}

	// Send request.
#define SSL_REQUEST "GET / HTTP/1.1\r\nHost: www.google.de\r\nConnection: close\r\n\r\n"

	const char* b = SSL_REQUEST;
	size_t len = sizeof(SSL_REQUEST) - 1;

	size_t written = 0;

	do {
		ssize_t ret;
		if ((ret = s.write(b + written, len - written, TIMEOUT)) < 0) {
			fprintf(stderr, "Error writing.\n");
			return;
		}

		written += ret;
	} while (written < len);

	// Receive response.
	char buf[4 * 1024];
	ssize_t ret;
	do {
		if ((ret = s.read(buf, sizeof(buf), TIMEOUT)) <= 0) {
			break;
		}

		printf("%.*s", ret, buf);
	} while (true);

	printf("\n");

	if (!s.shutdown(true, TIMEOUT)) {
		fprintf(stderr, "Error performing shutdown.\n");
	}
}

void serve_requests(net::socket& s)
{
	do {
		// Wait for a connection.
		net::socket client;
		if (!s.accept(client)) {
			continue;
		}

		// Read request.
		char buf[4 * 1024];
		ssize_t ret;
		if ((ret = client.read(buf, sizeof(buf), TIMEOUT)) > 0) {
			printf("%.*s\n", ret, buf);

			// Send response.
#define RESPONSE "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 16\r\n\r\n0123456789ABCDEF"
			const char* b = RESPONSE;
			size_t len = sizeof(RESPONSE) - 1;

			size_t written = 0;

			do {
				ssize_t w;
				if ((w = client.write(b + written, len - written, TIMEOUT)) <= 0) {
					break;
				}

				written += w;
			} while (written < len);
		}

		client.close();
	} while (true);
}

void serve_ssl_requests(net::socket& s)
{
	do {
		// Wait for a connection.
		net::socket client;
		if (!s.accept(client)) {
			continue;
		}

		net::ssl_socket ssl(client);
		if (!ssl.handshake(net::ssl_socket::SERVER_MODE, TIMEOUT)) {
			fprintf(stderr, "Error performing TLS/SSL handshake.\n");

			ssl.close();
			continue;
		}

		// Read request.
		char buf[4 * 1024];
		ssize_t ret;
		if ((ret = ssl.read(buf, sizeof(buf), TIMEOUT)) > 0) {
			printf("%.*s\n", ret, buf);

			// Send response.
#define SSL_RESPONSE "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 16\r\n\r\n0123456789ABCDEF"
			const char* b = SSL_RESPONSE;
			size_t len = sizeof(SSL_RESPONSE) - 1;

			size_t written = 0;

			do {
				ssize_t w;
				if ((w = ssl.write(b + written, len - written, TIMEOUT)) <= 0) {
					break;
				}

				written += w;
			} while (written < len);
		}

		//ssl.shutdown(true, TIMEOUT);
		ssl.close();
	} while (true);
}
