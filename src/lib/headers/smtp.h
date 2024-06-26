#ifndef SMTP_SERVER_H
#define SMTP_SERVER_H
#include "buffer.h"
#include "maildir.h"
#include "request.h"
#include "selector.h"
#include "states.h"
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
#define MAX_RCPT             101
#define RESPONSE_SIZE        1024
#define MAX_PATH             300
#define MAX_FILE_NAME        20
#define LOCAL_DOMAIN         "local"

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
	char filename_fd[MAIL_FILE_NAME_LENGTH];
	char temp_full_path[MAIL_DIR_SIZE + 1 + LOCAL_USER_NAME_SIZE + 1 + MAILBOX_INNER_DIR_SIZE + 1 + MAIL_FILE_NAME_LENGTH + 1];

	// parser
	smtp_state state;
	struct request_parser request_parser;
	struct request request;

	uint8_t mail_from[LOCAL_USER_NAME_SIZE + 1 + DOMAIN_NAME_SIZE];
	uint8_t rcpt_to[LOCAL_USER_NAME_SIZE + 1 + DOMAIN_NAME_SIZE][MAX_RCPT];
	size_t rcpt_qty;
	uint8_t data[BODY_SIZE];

	uint8_t user[LOCAL_USER_NAME_SIZE + 1 + DOMAIN_NAME_SIZE];  // for admin requests

	char file_full_name[MAX_PATH];
	char file_name[MAX_FILE_NAME];

	bool is_body;

	// raw buffer
	uint8_t raw_buff_write[BUFFER_SIZE];
	uint8_t raw_buff_read[BUFFER_SIZE];
} smtp_data;

struct status
{
	char* program;
	bool transform;
};

typedef enum
{

	REQUEST_READ = 0,
	REQUEST_WRITE,
	REQUEST_DATA,
	REQUEST_ADMIN,
	REQUEST_DATA_WRITE,
	REQUEST_DONE,
	REQUEST_ERROR,
	// definir los estados de la maquina de estados del protocolo SMTP
} socket_state;

void smtp_done(selector_key* key);

void smtp_passive_accept(selector_key* key);
void init_status(char* program);
void set_status(bool value);

#endif