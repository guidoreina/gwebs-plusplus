#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include "net/internet/http/server.h"
#include "net/internet/http/version.h"

#define CONFIG_FILE "gwebs++.conf"
#define MIME_TYPES_FILE "mime.types"

static void print_usage(const char* program);
static void signal_handler(int nsignal);

net::internet::http::server server;

int main(int argc, char** argv)
{
	const char* config_file = NULL;
	const char* mime_types_file = NULL;

	int i = 1;
	while (i < argc) {
		if (strcasecmp(argv[i], "--conf") == 0) {
			if ((i == argc - 1) || (config_file)) {
				print_usage(argv[0]);
				return -1;
			}

			config_file = argv[i + 1];

			i += 2;
		} else if (strcasecmp(argv[i], "--mime") == 0) {
			if ((i == argc - 1) || (mime_types_file)) {
				print_usage(argv[0]);
				return -1;
			}

			mime_types_file = argv[i + 1];

			i += 2;
		} else {
			print_usage(argv[0]);
			return -1;
		}
	}

	struct sigaction act;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	act.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &act, NULL);

	act.sa_handler = signal_handler;
	sigaction(SIGTERM, &act, NULL);
	sigaction(SIGINT, &act, NULL);

	if (!server.create(config_file ? config_file : CONFIG_FILE, mime_types_file ? mime_types_file : MIME_TYPES_FILE)) {
		fprintf(stderr, "Couldn't create HTTP server.\n");
		return -1;
	}

	server.start();

	return 0;
}

void print_usage(const char* program)
{
	fprintf(stderr, "%s [--conf <config_file>] [--mime <mime_types_file>]\n", program);
	fprintf(stderr, "Version: %s\n", WEBSERVER_NAME);
}

void signal_handler(int nsignal)
{
	fprintf(stderr, "Signal received...\n");

	server.stop();
}
