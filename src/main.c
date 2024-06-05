/**
 * main.c - servidor proxy socks concurrente
 *
 * Interpreta los argumentos de línea de comandos, y monta un socket
 * pasivo.
 *
 * Todas las conexiones entrantes se manejarán en éste hilo.
 *
 * Se descargará en otro hilos las operaciones bloqueantes (resolución de
 * DNS utilizando getaddrinfo), pero toda esa complejidad está oculta en
 * el selector.
 */
#include "lib/headers/selector.h"

#include <errno.h>
#include <limits.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>  // socket
#include <sys/types.h>   // socket
#include <unistd.h>

static bool done = false;

static void
sigterm_handler(const int signal)
{
	printf("signal %d, cleaning up and exiting\n", signal);
	done = true;
}

int
main(const int argc, const char** argv)
{
	unsigned port = 2525;

	if (argc == 1) {
		// utilizamos el default
	} else if (argc == 2) {
		char* end = 0;
		const long sl = strtol(argv[1], &end, 10);

		if (end == argv[1] || '\0' != *end || ((LONG_MIN == sl || LONG_MAX == sl) && ERANGE == errno) || sl < 0 ||
		    sl > USHRT_MAX) {
			fprintf(stderr, "port should be an integer: %s\n", argv[1]);
			return 1;
		}
		port = sl;
	} else {
		fprintf(stderr, "Usage: %s <port>\n", argv[0]);
		return 1;
	}

	// no tenemos nada que leer de stdin
	close(0);

	const char* err_msg = NULL;
	TSelectorStatus ss = SELECTOR_SUCCESS;
	TSelector selector = NULL;

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);

	// crear socket

	const int server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (server < 0) {
		err_msg = "unable to create socket";
		goto finally;
	}

	fprintf(stdout, "Listening on TCP port %d\n", port);

	// man 7 ip. no importa reportar nada si falla.  [SETTING SERVER SOCKET OPTIONS]
	setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int));

finally:
	int ret = 0;
	if (ss != SELECTOR_SUCCESS) {
		fprintf(stderr,
		        "%s: %s\n",
		        (err_msg == NULL) ? "" : err_msg,
		        ss == SELECTOR_IO ? strerror(errno) : selector_error(ss));
		ret = 2;
	} else if (err_msg) {
		perror(err_msg);
		ret = 1;
	}
	if (selector != NULL) {
		selector_destroy(selector);
	}
	selector_close();

	if (server >= 0) {
		close(server);
	}
	return ret;
}
