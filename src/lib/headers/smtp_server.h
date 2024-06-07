#ifndef SMTP_SERVER_H
#define SMTP_SERVER_H
#include "buffer.h"
#include "selector.h"
#include "stm.h"

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

	REQUEST_READ = 0,
	REQUEST_WRITE,
	// EHLO = 0,
	// FROM,
	// RCPT,
	// DATA,
	DONE,
	ERROR,
	// definir los estados de la maquina de estados del protocolo SMTP
};

void smtp_passive_accept(selector_key* key);

#endif