#ifndef SMTP_SERVER_H
#define SMTP_SERVER_H
#include "lib/headers/buffer.h"
#include "lib/headers/selector.h"
#include "lib/headers/stm.h"

#include <netdb.h>
#include <stdbool.h>
#include <string.h>
#include <sys/socket.h>

#define ATTACHMENT(key) ((smtp_data*)(key)->data)
#define N(x)            (sizeof(x) / sizeof((x)[0]))
#define BUFFER_SIZE     4096
typedef struct smtp_data
{
	struct state_machine stm;

	// parser ?)
	// client address Ipv4 / Ipv6
	struct sockaddr_storage client_addr;

	int fd;  // socket file descriptor

	// buffers
	struct buffer read_buffer;
	struct buffer write_buffer;

} smtp_data;

enum smtp_states
{
	EHLO = 0,
	FROM,
	RCPT,
	DATA,
	ERROR,
	DONE,
	// definir los estados de la maquina de estados del protocolo SMTP
};

void new_active_socket(selector_key* key);

#endif