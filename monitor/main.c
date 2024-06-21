#include "monitor.h"
#include "selector.h"

#include <errno.h>
#include <limits.h>
#include <netinet/in.h>
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
	selector_status ss = SELECTOR_SUCCESS;
	fd_selector selector = NULL;

	struct sockaddr_in6 addr6;
	struct sockaddr_in addr4;
	memset(&addr6, 0, sizeof(addr6));
	memset(&addr4, 0, sizeof(addr4));

	addr6.sin6_family = AF_INET6;   // IPv6
	addr6.sin6_addr = in6addr_any;  // any local address, filter
	addr6.sin6_port = htons(port);  // set port

	addr4.sin_family = AF_INET;                 // IPv4
	addr4.sin_addr.s_addr = htonl(INADDR_ANY);  // any local address, filter
	addr4.sin_port = htons(port);               // set port

	// create sockets
	const int server6 = socket(AF_INET6, SOCK_DGRAM, 0);
	if (server6 < 0) {
		err_msg = "unable to create IPv6 socket";
		goto finally;
	}

	const int server4 = socket(AF_INET, SOCK_DGRAM, 0);
	if (server4 < 0) {
		err_msg = "unable to create IPv4 socket";
		goto finally;
	}

	fprintf(stdout, "Listening on UPD port %d\n", port);

	// man 7 ip. no importa reportar nada si falla.

	setsockopt(server6, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int));
	setsockopt(server4, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int));

	// nos aseguramos que el socket IPv6 no escuche en direcciones IPv4
	setsockopt(server6, IPPROTO_IPV6, IPV6_V6ONLY, &(int){ 1 }, sizeof(int));

	// UDP close operation is blocking, so we use SO_LINGER to make it non-blocking
	int option_len = sizeof(struct linger);
	struct linger linger;

	int rc = getsockopt(server4, SOL_SOCKET, SO_LINGER, (void*)&linger, &option_len);

	if (rc < 0) {
		perror("getsockopt");
		return 1;
	}

	linger.l_onoff = 1;
	linger.l_linger = 0;

	rc = setsockopt(server4, SOL_SOCKET, SO_LINGER, (void*)&linger, option_len);

	if (rc < 0) {
		perror("setsockopt");
		return 1;
	}

	// same for IPv6
	rc = getsockopt(server6, SOL_SOCKET, SO_LINGER, (void*)&linger, &option_len);

	if (rc < 0) {
		perror("getsockopt");
		return 1;
	}

	linger.l_onoff = 1;
	linger.l_linger = 0;

	// now set the option back
	rc = setsockopt(server6, SOL_SOCKET, SO_LINGER, (void*)&linger, option_len);
	if (rc < 0) {
		perror("setsockopt");
		return 1;
	}

	// bind sockets to IP and port
	if (bind(server6, (struct sockaddr*)&addr6, sizeof(addr6)) < 0) {
		err_msg = "unable to bind IPv6 socket";
		goto finally;
	}

	if (bind(server4, (struct sockaddr*)&addr4, sizeof(addr4)) < 0) {
		err_msg = "unable to bind IPv4 socket";
		goto finally;
	}
	/* We now use UDP, not TCP. So listening to incoming connections is not needed.
	    // listen for incoming connections
	    if (listen(server6, 20) < 0) {
	        err_msg = "unable to listen on IPv6 socket";
	        goto finally;
	    }

	    if (listen(server4, 20) < 0) {
	        err_msg = "unable to listen on IPv4 socket";
	        goto finally;
	    }
	*/
	// registrar sigterm es útil para terminar el programa normalmente.
	// esto ayuda mucho en herramientas como valgrind.
	signal(SIGTERM, sigterm_handler);
	signal(SIGINT, sigterm_handler);

	// set socket to Non blocking [SETTING SERVER SOCKET FLAGS]
	if (selector_fd_set_nio(server6) == -1) {
		err_msg = "getting server IPv6 socket flags";
		goto finally;
	}
	if (selector_fd_set_nio(server4) == -1) {
		err_msg = "getting server IPv4 socket flags";
		goto finally;
	}

	// TODO: check if we need timeout
	const struct selector_init conf = {
        .signal = SIGALRM,
        .select_timeout = {
            .tv_sec  = 10,
            .tv_nsec = 0,
        },
    };

	if (0 != selector_init(&conf)) {
		err_msg = "initializing selector";
		goto finally;
	}

	selector = selector_new(1024);

	if (selector == NULL) {
		err_msg = "unable to create selector";
		goto finally;
	}

	const struct fd_handler monitor = {
		.handle_read = handle_udp_packet,
		.handle_write = NULL,
		.handle_close = NULL,  // nada que liberar
	};

	// register servers fd to selector in readfds set
	ss = selector_register(selector, server6, &monitor, OP_READ, NULL);
	if (ss != SELECTOR_SUCCESS) {
		err_msg = "registering IPv6 fd";
		goto finally;
	}

	ss = selector_register(selector, server4, &monitor, OP_READ, NULL);
	if (ss != SELECTOR_SUCCESS) {
		err_msg = "registering IPv4 fd";
		goto finally;
	}

	// main loop to serve clients
	while (!done) {
		err_msg = NULL;
		ss = selector_select(selector);
		if (ss != SELECTOR_SUCCESS) {
			err_msg = "serving";
			goto finally;
		}
	}

	if (err_msg == NULL) {
		err_msg = "closing";
	}
	int ret = 0;
finally:

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

	if (server6 >= 0) {
		close(server6);
	}
	if (server4 >= 0) {
		close(server4);
	}
	return ret;
}
