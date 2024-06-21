#ifndef MONITOR_H
#define MONITOR_H

#include "selector.h"

#include <netdb.h>
#include <stdbool.h>
#include <string.h>
#include <sys/socket.h>

#define ATTACHMENT(key) ((monitor_data*)(key)->data)
#define N(x)            (sizeof(x) / sizeof((x)[0]))  // number of elements in an array
#define BUFFER_SIZE     22                            // total bytes of UDP packet: 8 (header) + 14 (data) = 22
/*
                     0      7 8     15 16    23 24    31
                    +--------+--------+--------+--------+
                    |    Puerto de    |    Puerto de    |
                    |      Origen     |     Destino     |
                    +--------+--------+--------+--------+
                    |                 |                 |
                    |    Longitud     | Suma de Control |
                    +--------+--------+--------+--------+
                    |                                   |
                    |               Data                |
                    +-----------------------------------+
*/

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

/* Response format: 14 bytes
 0      7 8     15 16    23 24    31 32    39 40    47 48    55 56    63
+--------+--------+--------+--------+--------+--------+--------+--------+
|                 |        |                 |        |                 |
|    signature    |  vers  |    request_id   | status |  response data  |
|                 |        |                 |        |                 |
+--------+--------+--------+--------+--------+--------+--------+--------+
|                                                     |                 |
|                    response data                    |                 |
|                                                     |                 |
+--------+--------+--------+--------+--------+--------+--------+--------+
*/

// the monitor protocol is a simple protocol that allows the client to send a message to the server

enum monitor_states
{
	M_REQ_READ,
	M_REQ_WRITE,
	ERROR,
	DONE,
};

typedef struct monitor_data
{
	int client_fd;  // socket file descriptor
	struct sockaddr_storage client_addr;
	socklen_t client_addr_len;

	// raw buffer
	uint8_t raw_buff_write[BUFFER_SIZE];
	uint8_t raw_buff_read[BUFFER_SIZE];

	// protocol data
	uint16_t signature;
	uint8_t version;
	uint16_t request_id;  // generated by the client
	uint8_t token[8];     // 8 unsigned big-endian bytes
	uint8_t command;

	// response
	uint8_t status;    // 1 byte
	uint8_t qty[8];    // 8 unsigned big-endian bytes
	uint8_t bool_res;  // 1 byte
} monitor_data;

void handle_udp_packet(struct selector_key* key);

#endif