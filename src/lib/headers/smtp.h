#ifndef SMTP_SERVER_H
#define SMTP_SERVER_H
#include "buffer.h"
#include "states.h"
#include "request.h"
#include "selector.h"
#include "stm.h"
#include <netdb.h>
#include <stdbool.h>
#include <string.h>
#include <sys/socket.h>

#define ATTACHMENT(key)      ((smtp_data*)(key)->data)
#define N(x)                 (sizeof(x) / sizeof((x)[0]))
#define BUFFER_SIZE          4096
#define LOCAL_USER_NAME_SIZE 64
#define DOMAIN_NAME_SIZE     255
#define COMMAND_LINE_SIZE    512
#define MAIL_SIZE            255
#define BODY_SIZE            1024

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

	int output_fd;  // file descriptor for the output file

	// parser
	smtp_state state;
	struct request_parser request_parser;
	struct request request;

	uint8_t mail_from[LOCAL_USER_NAME_SIZE + 1 + DOMAIN_NAME_SIZE];
	uint8_t rcpt_to[LOCAL_USER_NAME_SIZE + 1 + DOMAIN_NAME_SIZE];
	uint8_t data[BODY_SIZE];

	// raw buffer
	uint8_t raw_buff_write[BUFFER_SIZE];
	uint8_t raw_buff_read[BUFFER_SIZE];
} smtp_data;

 

typedef enum 
{

	REQUEST_READ = 0,
	REQUEST_WRITE,
	REQUEST_DATA,
	REQUEST_DONE,
	REQUEST_ERROR,
	// definir los estados de la maquina de estados del protocolo SMTP
} socket_state;
 


void smtp_passive_accept(selector_key* key);
void destroy_socket(selector_key* data);

#endif