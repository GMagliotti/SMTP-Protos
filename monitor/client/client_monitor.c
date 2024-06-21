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
printBytesRecieved(uint8_t* buffer, ssize_t numBytes)
{
	printf("Received: ");
	for (int i = 0; i < numBytes; i++) {
		printf("buff[%d]=%02x ", i, buffer[i]);
	}
	printf("\n");
	// now print them like in an array and bing hex values: { 0x00, 0x00, 0x00, }
	printf("Received: { ");
	for (int i = 0; i < numBytes; i++) {
		printf("0x%02x, ", buffer[i]);
	}
	printf("}\n");
}

int
sockAddrsEqual(const struct sockaddr* addr1, const struct sockaddr* addr2)
{
	if (addr1 == NULL || addr2 == NULL)
		return addr1 == addr2;
	else if (addr1->sa_family != addr2->sa_family)
		return 0;
	else if (addr1->sa_family == AF_INET) {
		struct sockaddr_in* ipv4Addr1 = (struct sockaddr_in*)addr1;
		struct sockaddr_in* ipv4Addr2 = (struct sockaddr_in*)addr2;
		return ipv4Addr1->sin_addr.s_addr == ipv4Addr2->sin_addr.s_addr && ipv4Addr1->sin_port == ipv4Addr2->sin_port;
	} else if (addr1->sa_family == AF_INET6) {
		struct sockaddr_in6* ipv6Addr1 = (struct sockaddr_in6*)addr1;
		struct sockaddr_in6* ipv6Addr2 = (struct sockaddr_in6*)addr2;
		return memcmp(&ipv6Addr1->sin6_addr, &ipv6Addr2->sin6_addr, sizeof(struct in6_addr)) == 0 &&
		       ipv6Addr1->sin6_port == ipv6Addr2->sin6_port;
	} else
		return 0;
}

int
udpClientSocket(const char* host, const char* service, struct addrinfo** servAddr)
{
	// Pedimos solamente para UDP, pero puede ser IPv4 o IPv6
	struct addrinfo addrCriteria;
	memset(&addrCriteria, 0, sizeof(addrCriteria));
	addrCriteria.ai_family = AF_UNSPEC;      // Any address family
	addrCriteria.ai_socktype = SOCK_DGRAM;   // Only datagram sockets
	addrCriteria.ai_protocol = IPPROTO_UDP;  // Only UDP protocol

	// Tomamos la primera de la lista
	int rtnVal = getaddrinfo(host, service, &addrCriteria, servAddr);
	if (rtnVal != 0) {
		perror("getaddrinfo() failed");
		return -1;
	}

	// Socket cliente UDP
	return socket(
	    (*servAddr)->ai_family, (*servAddr)->ai_socktype, (*servAddr)->ai_protocol);  // Socket descriptor for client
}

uint16_t
request_id_generator(void)
{
	static uint16_t request_id = 0;
	return request_id++;
}

int
main(void)
{
	char* port = "2525";
	struct addrinfo* servAddr;

	char* server = "127.0.0.1";

	printf("Case 1: command 1\n");
	uint8_t buffer[REQUEST_SIZE] = { 0 };
	buffer[0] = (SIGNATURE >> 8) & 0xff;
	buffer[1] = SIGNATURE & 0xff;
	buffer[2] = VERSION;
	uint16_t request_id = request_id_generator();
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
	buffer[13] = CMD1;

	int sock = udpClientSocket(server, port, &servAddr);

	ssize_t numBytes = sendto(sock, buffer, REQUEST_SIZE, 0, servAddr->ai_addr, servAddr->ai_addrlen);

	if (numBytes < 0) {
		perror("sendto() failed");
		return -1;
	}

	if (numBytes < 0) {
		perror("sendto() failed");
	} else if (numBytes != REQUEST_SIZE) {
		perror("sendto() error, sent unexpected number of bytes");
	}

	// Guardamos la direccion/puerto de respuesta para verificar que coincida con el servidor
	struct sockaddr_storage fromAddr;  // Source address of server
	socklen_t fromAddrLen = sizeof(fromAddr);
	uint8_t rec_buffer[REQUEST_SIZE + 1];

	// Establecemos un timeout de 5 segundos para la respuesta
	struct timeval tv;  // Timeout for recvfrom(). It is in library sys/time.h
	tv.tv_sec = 60 * 10;
	tv.tv_usec = 0;
	if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
		perror("Error setting timeout");
	}

	numBytes = recvfrom(sock, rec_buffer, REQUEST_SIZE, 0, (struct sockaddr*)&fromAddr, &fromAddrLen);
	if (numBytes < 0) {
		perror("recvfrom() failed");
	} else {
		if (numBytes != REQUEST_SIZE)
			perror("recvfrom() error, received unexpected number of bytes");

		// "Autenticamos" la respuesta
		if (!sockAddrsEqual(servAddr->ai_addr, (struct sockaddr*)&fromAddr))
			perror("recvfrom() error, received a packet from an unknown source");

		rec_buffer[numBytes] = '\0';
		printBytesRecieved(rec_buffer, numBytes);
	}

	freeaddrinfo(servAddr);
	close(sock);

	return 0;
}
