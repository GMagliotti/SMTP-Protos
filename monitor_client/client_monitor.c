#include "client_monitor.h"

/* Request format: 14 bytes
 0      7 8     15 16    23 24    31 32    39 40    47 48    55 56    63
+--------+--------+--------+--------+--------+--------+--------+--------+
|                 |        |                 |                          |
|    signature    |  vers  |    request_id   |           auth           |
|                 |        |                 |                          |
+--------+--------+--------+--------+--------+--------+--------+--------+
|                                            |        |                 |
|                    auth                    |  cmd   |                 |
|                                            |        |                 |
+--------+--------+--------+--------+--------+--------+--------+--------+
*/

void
print_bytes_recieved(uint8_t* buffer, int command)
{
	uint32_t qty;
	uint64_t bytes;
	switch (command) {
		case HIST_C:
			// qty need to be filled with buffer[6] to buffer[9]
			qty = (buffer[6] << 24) | (buffer[7] << 16) | (buffer[8] << 8) | buffer[9];
			qty = ntohl(qty);
			printf("Historical connections: %d\n", qty);
			return;
		case CONC_C:
			qty = (buffer[6] << 24) | (buffer[7] << 16) | (buffer[8] << 8) | buffer[9];
			qty = ntohl(qty);
			printf("Simultaneous connections: %d\n", qty);
			return;
		case BYTES_T:
			bytes = ((uint64_t)buffer[6] << 56) | ((uint64_t)buffer[7] << 48) | ((uint64_t)buffer[8] << 40) |
			        ((uint64_t)buffer[9] << 32) | ((uint64_t)buffer[10] << 24) | ((uint64_t)buffer[11] << 16) |
			        ((uint64_t)buffer[12] << 8) | (uint64_t)buffer[13];
			printf("Transfered bytes: %lu\n", bytes);
			return;
		case TRANS_S:
			// it is a boolean, 0x00 is off, 0x01 is on
			if (buffer[6] == 0x00)
				printf("Transformations are off\n");
			else
				printf("Transformations are on\n");
			return;
		case TRANS_ON:
			printf("Transformations turned on\n");
			return;
		case TRANS_OFF:
			printf("Transformations turned off\n");
			return;
		default:
			return;
	}
}

int
sock_addrs_equal(const struct sockaddr* addr1, const struct sockaddr* addr2)
{
	if (addr1 == NULL || addr2 == NULL)
		return addr1 == addr2;
	else if (addr1->sa_family != addr2->sa_family)
		return 0;
	else if (addr1->sa_family == AF_INET) {
		struct sockaddr_in* ipv4_addr1 = (struct sockaddr_in*)addr1;
		struct sockaddr_in* ipv4_addr2 = (struct sockaddr_in*)addr2;
		return ipv4_addr1->sin_addr.s_addr == ipv4_addr2->sin_addr.s_addr &&
		       ipv4_addr1->sin_port == ipv4_addr2->sin_port;
	} else if (addr1->sa_family == AF_INET6) {
		struct sockaddr_in6* ipv6_addr1 = (struct sockaddr_in6*)addr1;
		struct sockaddr_in6* ipv6_addr2 = (struct sockaddr_in6*)addr2;
		return memcmp(&ipv6_addr1->sin6_addr, &ipv6_addr2->sin6_addr, sizeof(struct in6_addr)) == 0 &&
		       ipv6_addr1->sin6_port == ipv6_addr2->sin6_port;
	} else
		return 0;
}

int
udp_client_socket(const char* host, const char* service, struct addrinfo** serv_addr)
{
	// Pedimos solamente para UDP, pero puede ser IPv4 o IPv6
	struct addrinfo addr_criteria;
	memset(&addr_criteria, 0, sizeof(addr_criteria));
	addr_criteria.ai_family = AF_UNSPEC;      // Any address family
	addr_criteria.ai_socktype = SOCK_DGRAM;   // Only datagram sockets
	addr_criteria.ai_protocol = IPPROTO_UDP;  // Only UDP protocol

	// Tomamos la primera de la lista
	int rtn = getaddrinfo(host, service, &addr_criteria, serv_addr);
	if (rtn != 0) {
		perror("getaddrinfo() failed");
		return -1;
	}

	// Socket cliente UDP
	return socket(
	    (*serv_addr)->ai_family, (*serv_addr)->ai_socktype, (*serv_addr)->ai_protocol);  // Socket descriptor for client
}

/*
struct protocol_request {
    // protocol data
    uint16_t signature;
    uint8_t version;
    uint16_t request_id;  // generated by the client
    uint8_t token[8];     // 8 unsigned big-endian bytes
    uint8_t command;
}
*/

void
prepare_buffer(uint8_t* buffer, uint16_t request_id, uint8_t cmd)
{
	// we want to convert the values to Network Byte Order using netdb.h

	uint16_t signature = htons((uint16_t)SIGNATURE);
	request_id = htons(request_id);

	buffer[0] = (signature >> 8) & 0xff;
	buffer[1] = signature & 0xff;
	buffer[2] = VERSION;
	buffer[3] = (request_id >> 8) & 0xff;
	buffer[4] = request_id & 0xff;
	buffer[5] = (TOKEN >> 56) & 0xff;
	buffer[6] = (TOKEN >> 48) & 0xff;
	buffer[7] = (TOKEN >> 40) & 0xff;
	buffer[8] = (TOKEN >> 32) & 0xff;
	buffer[9] = (TOKEN >> 24) & 0xff;
	buffer[10] = (TOKEN >> 16) & 0xff;
	buffer[11] = (TOKEN >> 8) & 0xff;
	buffer[12] = TOKEN & 0xff;
	buffer[13] = cmd;
}
/*
enum commands
{
    HIST_C = 0x00,
    CONC_C,
    BYTES_T,
    TRANS_S,
    TRANS_ON,
    TRANS_OFF
};
static const char* commands_str[] = { "HIST", "CONC", "BYTES", "STATUS", "T_ON", "T_OFF" };
 */
int
command_exists(char* command, int* command_reference)
{
	for (int i = 0; i < 6; i++) {
		if (strcmp(command, commands_str[i]) == 0) {
			*command_reference = i;
			return 1;
		}
	}
	return 0;
}

int
args_quantity_ok(int command, int argc)
{
	if (command < 0 || argc <= 1)
		return 0;
	return 1;
}

uint16_t
request_id_generator(void)
{
	static uint16_t request_id = 0;
	return request_id++;
}

int
main(int argc, char* argv[])
{
	// we need to implement a client application for the monitor protocol
	/*
	    Client works like this:
	    1. Validate the command line arguments
	    2. Prepare the buffer
	    3. Send the buffer
	    4. Wait for the response
	    5. Validate the response
	    6. If the response never arrives, the client should timeout
	    7. If the response is invalid, the client should print an error message
	    8. If the response is valid, the client should print the response data
	*/

	if (argc <= 3 || strcmp("-h", argv[3]) == 0) {
		fprintf(stderr,
		        "Usage: %s [HOST] [PORT] [OPTION]\n"
		        "\n use '-' to specify default values for [HOST] and [PORT]\n"
		        "\n"
		        "   -h                                        Prints help and finishes.\n"
		        "   HIST                                      Request historical conections.\n"
		        "   CONC                                      Request simultaneous conections.\n"
		        "   BYTES                                     Request ammount of transfered bytes.\n"
		        "   STATUS     	                              Request information about transformations.\n"
		        "   T_ON                                      Request to turn on transformations.\n"
		        "   T_OFF                                     Request to turn off transformations.\n"
		        "\n",
		        argv[0]);
		return 0;
	}

	const char* host = argv[1];
	const char* port = argv[2];

	if (host[0] == '-')
		host = DEFAULT_HOST;
	if (port[0] == '-')
		port = DEFAULT_PORT;

	char* command = argv[3];
	int command_reference;

	if (!command_exists(command, &command_reference)) {
		printf("%s: is not a valid command\n", command);
		return -1;
	}

	if (!args_quantity_ok(command_reference, argc)) {
		printf("%s: few arguments\n", command);
		return -1;
	}

	struct addrinfo* serv_addr;

	uint8_t buffer[REQUEST_SIZE] = { 0 };
	prepare_buffer(buffer, request_id_generator(), command_reference);

	int sock = udp_client_socket(host, port, &serv_addr);

	printf("Sending command %s to %s:%s\n", commands_str[command_reference], host, port);
	ssize_t num_bytes = sendto(sock, buffer, REQUEST_SIZE, 0, serv_addr->ai_addr, serv_addr->ai_addrlen);

	if (num_bytes < 0) {
		perror("sendto() failed");
		return -1;
	}

	if (num_bytes < 0) {
		perror("sendto() failed");
	} else if (num_bytes != REQUEST_SIZE) {
		perror("sendto() error, sent unexpected number of bytes");
	}

	// Guardamos la direccion/puerto de respuesta para verificar que coincida con el servidor
	struct sockaddr_storage from_addr;  // Source address of server
	socklen_t from_addr_len = sizeof(from_addr);
	uint8_t rec_buffer[REQUEST_SIZE + 1];

	// Establecemos un timeout de 5 segundos para la respuesta
	struct timeval tv;  // Timeout for recvfrom(). It is in library sys/time.h
	tv.tv_sec = 5;
	tv.tv_usec = 0;
	if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
		perror("Error setting timeout");
	}

	num_bytes = recvfrom(sock, rec_buffer, REQUEST_SIZE, 0, (struct sockaddr*)&from_addr, &from_addr_len);
	if (num_bytes < 0) {
		perror("recvfrom() failed");
	} else {
		if (num_bytes != REQUEST_SIZE)
			perror("recvfrom() error, received unexpected number of bytes");

		// "Autenticamos" la respuesta
		if (!sock_addrs_equal(serv_addr->ai_addr, (struct sockaddr*)&from_addr))
			perror("recvfrom() error, received a packet from an unknown source");

		rec_buffer[num_bytes] = '\0';
		print_bytes_recieved(rec_buffer, command_reference);
	}

	freeaddrinfo(serv_addr);
	close(sock);

	return 0;
}