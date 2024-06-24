/*
Acá vamos a definir el read handler que acepta las conexiones entrantes, agrega el nuevo fd al selector.
Vamos a definir todos los handlers que le vamos a pasar al selector para que maneje los eventos de los sockets.

Esos handlers van a llamar a los handlers de la maquina de estados del protocolo SMTP.

Una Maquina de Estados para cada entrada del Selector:
    * read
    * write
    * close   # Estos handlers van a llamar a los handlers del del state de la segunda stm
    * block

Una Maquina de estado para cada Cliente SM:
    * Estados relacionados con el protocolo
    * EHLO
    * RCPT
    * FROM
    * DATA
    * etc


Cada estado va a tener un handlers que hay que definir
        .state            =
        .on_arrival       =
        .on_departure     =
        .on_read_ready    =
        .on_write_ready    =



    struct smtp
{
    …
        // maquinas de estados
        struct state_machine stm;

    // estados para el client_fd
    union
    {
        struct hello_st hello;
        struct request_st request;
        struct copy copy;
    } client;
    // estados para el origin_fd
    union
    {
        struct connecting conn;
        struct copy copy;
    } origin;


*/

#include "smtp.h"

#include "buffer.h"
#include "process.h"
#include "request.h"
#include "selector.h"
#include "states.h"

#include <fcntl.h>
#include <monitor.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define MS_TEXT_SIZE           13
#define MAILBOX_INNER_DIR_SIZE 3  // cur, new, tmp (3)
#define MAIL_DIR_SIZE          4

typedef enum request_state (*state_handler)(const uint8_t c, struct request_parser* p);
const fd_handler* get_smtp_handler(void);

unsigned int request_read_handler(struct selector_key* key);
void request_read_init(unsigned int state, struct selector_key* key);
void request_read_close(unsigned int state, struct selector_key* key);
unsigned int request_write_handler(struct selector_key* key);

unsigned int request_process(struct selector_key* key);

unsigned int request_data_handler(struct selector_key* key);
void request_data_init(unsigned int state, struct selector_key* key);
void request_data_close(unsigned int state, struct selector_key* key);

unsigned int request_admin_handler(struct selector_key* key);
void request_admin_init(unsigned int state, struct selector_key* key);
void request_admin_close(unsigned int state, struct selector_key* key);
void on_done_init(const unsigned state, struct selector_key* key);

void smtp_done(selector_key* key);
bool read_complete(enum request_state st);

int create_directory_if_not_exists(char* maildir);
static char* get_and_create_maildir(char* mail_from);
static const struct state_definition states_handlers[] = {
	// definir los estados de la maquina de estados del protocolo SMTP
	// no necesariamente tenemos que llenar todos los campos en cada estado

	{
	    .state = REQUEST_READ,
	    .on_read_ready = request_read_handler,
	    .on_arrival = request_read_init,
	    .on_departure = request_read_close,
	},

	{
	    .state = REQUEST_WRITE,
	    .on_write_ready = request_write_handler,
	},
	{
	    .state = REQUEST_DATA,
	    .on_read_ready = request_data_handler,  // handler para guardar en un buffer hasta encontrar <CRLF>.<CRLF>
	    .on_arrival = request_data_init,        // setteamos el parser. Abrimos el FD al archivo de salida.
	    .on_departure = request_data_close,     // cerramos el parser

	},
	{
	    .state = REQUEST_ADMIN,
	    .on_read_ready = request_admin_handler,  // handler para guardar en un buffer hasta encontrar <CRLF>.<CRLF>
	    .on_arrival = request_admin_init,        // setteamos el parser. Abrimos el FD al archivo de salida.
	    .on_departure = request_admin_close,     // cerramos el parser

	},
	{
	    .state = REQUEST_DONE,
	    .on_write_ready = NULL,

	},
	{
	    .state = REQUEST_ERROR,
	    .on_write_ready = NULL,
	}

};

process_handler handlers_table[] = {
	[EHLO] = handle_helo, [FROM] = handle_from,  [TO] = handle_to,      [DATA] = handle_data, [BODY] = handle_body,
	[ERROR] = NULL,       [XAUTH] = handle_body, [XFROM] = handle_body, [XGET] = handle_body
};

static void read_handler(struct selector_key* key);
static void write_handler(struct selector_key* key);
static void close_handler(struct selector_key* key);

// BASICAMENTE LLAMAN A LOS HANDLERS DE LA MAQUINA DE ESTADOS
static void
read_handler(struct selector_key* key)
{
	smtp_data* data = ATTACHMENT(key);
	const socket_state st = stm_handler_read(&data->stm, key);
	if (REQUEST_ERROR == st || REQUEST_DONE == st) {
		smtp_done(key);
	}
}
static void
write_handler(struct selector_key* key)
{
	smtp_data* data = ATTACHMENT(key);
	const socket_state st = stm_handler_write(&data->stm, key);
	if (REQUEST_ERROR == st || REQUEST_DONE == st) {
		smtp_done(key);
	}
}
static void
close_handler(struct selector_key* key)
{
	stm_handler_close(&ATTACHMENT(key)->stm, key);
	monitor_close_connection();
}

static fd_handler smtp_handler = {
	.handle_read = read_handler,
	.handle_write = write_handler,
	.handle_close = close_handler,
};

const fd_handler*
get_smtp_handler(void)
{
	return &smtp_handler;
}

void
smtp_done(selector_key* key)
{
	selector_status status = selector_unregister_fd(key->s, key->fd);
	if (status != SELECTOR_SUCCESS) {
		perror("selector_unregister_fd");
	}
	smtp_data* data = ATTACHMENT(key);
	free(data);
}
void
smtp_passive_accept(selector_key* key)
{
	// Crear un nuevo socket

	// this struct may hold ipv4 or ipv6
	struct sockaddr_storage client_addr;

	socklen_t client_addr_len = sizeof(client_addr);

	int new_socket = accept(key->fd, (struct sockaddr*)&client_addr, &client_addr_len);

	if (new_socket < 0) {
		perror("accept");
		return;
	}

	smtp_data* data = calloc(1, sizeof(*data));

	if (data == NULL) {
		perror("calloc");
		return;
	}

	data->fd = new_socket;
	data->client_addr = client_addr;
	data->stm.initial = REQUEST_WRITE;
	data->stm.max_state = REQUEST_ERROR;
	data->stm.states = states_handlers;
	data->rcpt_qty = 0;
	buffer_init(&data->read_buffer, N(data->raw_buff_read), data->raw_buff_read);
	buffer_init(&data->write_buffer, N(data->raw_buff_write), data->raw_buff_write);

	stm_init(&data->stm);

	memcpy(&data->raw_buff_write, "220 Bienvenido al servidor SMTP\n\0", 32);
	buffer_write_adv(&data->write_buffer, 32);

	selector_status status = selector_register(key->s, new_socket, get_smtp_handler(), OP_WRITE, data);

	if (status != SELECTOR_SUCCESS) {
		perror("selector_register");
		smtp_done(key);
		return;
	}

	monitor_add_connection();

	return;
}

// REQUEST WRITE HANDLERS

socket_state
request_process(struct selector_key* key)
{
	smtp_data* data = ATTACHMENT(key);
	// WRAPPER de los state process habdlers

	char msg[60];
	smtp_state st = data->state;

	if (SELECTOR_SUCCESS != selector_set_interest_key(key, OP_WRITE)) {
		return REQUEST_ERROR;
	}
	size_t count;

	uint8_t* ptr = buffer_write_ptr(&data->write_buffer, &count);

	// LLAMAR A HANDLERS NO SECUENCIALES
	bool is_noop = handle_noop(key, msg);
	bool is_rset = handle_reset(key, msg);
	bool is_xquit = handle_xquit(key, msg);



	// LLAMAR AL SECUENCIAL
	if (!(is_noop || is_rset || is_xquit)) {
		process_handler fn = handlers_table[st];
		smtp_state next = fn(key, msg);
		data->state = next;
	}

	size_t len = strlen(msg);
	msg[len++] = '\0';
	memcpy(ptr, msg, len);
	buffer_write_adv(&data->write_buffer, len);

	return REQUEST_WRITE;
}

// REQUEST WRITE HANDLERS
socket_state
request_write_handler(struct selector_key* key)
{
	smtp_data* data = ATTACHMENT(key);
	uint8_t* ptr;
	size_t count;
	ssize_t send_bytes;
	buffer* buff = &data->write_buffer;

	ptr = buffer_read_ptr(buff, &count);

	send_bytes = send(key->fd, ptr, count, MSG_NOSIGNAL);
	monitor_add_sent_bytes(send_bytes);

	int ret;
	if (send_bytes >= 0) {
		buffer_read_adv(buff, send_bytes);  // avisa que hay send_bytes bytes menos por mandar (leer del buffer)
		if (!buffer_can_read(buff)) {
			// si no queda nada para mandar (leer del buffer write)
			if (data->state == BODY) {
				if (SELECTOR_SUCCESS == selector_set_interest_key(key, OP_READ)) {
					return REQUEST_DATA;
				}
			}
			if(data->state >= XAUTH && data->state <= XQUIT){
				if (SELECTOR_SUCCESS == selector_set_interest_key(key, OP_READ)) {
					return REQUEST_ADMIN;
				}
			}
			if (SELECTOR_SUCCESS == selector_set_interest_key(key, OP_READ)) {
				return REQUEST_READ;
			} else {
				ret = ERROR;
			}
		}
	} else {
		ret = ERROR;
	}
	return ret;
}

//  REQUEST READ HANDLERS
void
request_read_init(unsigned int state, struct selector_key* key)
{
	printf("request_read_init\n %d", state);
	smtp_data* data = ATTACHMENT(key);
	data->request_parser.request = &data->request;
	request_parser_init(&data->request_parser);
}

socket_state
request_read_handler(struct selector_key* key)
{
	smtp_data* data = ATTACHMENT(key);
	size_t count;
	uint8_t* ptr = buffer_write_ptr(&data->read_buffer, &count);
	ssize_t recv_bytes = recv(key->fd, ptr, count, 0);

	socket_state ret = REQUEST_READ;

	if (recv_bytes <= 0) {
		return REQUEST_ERROR;
	}

	buffer_write_adv(&data->read_buffer, recv_bytes);  // avisa que hay recv_bytes bytes menos por leer
	// procesamiento
	bool error = false;

	enum request_state next_state = request_consume(&data->read_buffer, &data->request_parser, &error);

	if (request_is_done(next_state, &error)) {
		ret = request_process(key);
	}

	return ret;
}

void
request_read_close(unsigned int state, struct selector_key* key)
{
	if (state == REQUEST_READ) {
		smtp_data* data = ATTACHMENT(key);
		request_close(&data->request_parser);
	}
}

//  REQUEST ADMIN HANDLERS

unsigned int
request_admin_handler(struct selector_key* key)
{
	smtp_data* data = ATTACHMENT(key);
	size_t count;
	uint8_t* ptr = buffer_write_ptr(&data->read_buffer, &count);
	ssize_t recv_bytes = recv(key->fd, ptr, count, 0);

	socket_state ret = REQUEST_DATA;

	if (recv_bytes <= 0) {
		return REQUEST_ERROR;
	}

	buffer_write_adv(&data->read_buffer, recv_bytes);  // avisa que hay recv_bytes bytes menos por leer
	// procesamiento
	bool error = false;

	enum request_state state = request_consume_admin(&data->read_buffer, &data->request_parser, &error);

	if (request_is_done(state, 0)) {
		ret = request_process(key);
	}

	return ret;
}

void
request_admin_init(unsigned int state, struct selector_key* key)
{
	printf("request_data_init\n %d", state);

	smtp_data* data = ATTACHMENT(key);
	data->request_parser.request = &data->request;
	request_parser_admin_init(&data->request_parser);
}

//  REQUEST DATA HANDLERS

void
request_admin_close(unsigned int state, struct selector_key* key)
{
	if (state == REQUEST_ADMIN) {
		smtp_data* data = ATTACHMENT(key);
		request_close(&data->request_parser);
	}
}

unsigned int
request_data_handler(struct selector_key* key)
{
	smtp_data* data = ATTACHMENT(key);
	size_t count;
	uint8_t* ptr = buffer_write_ptr(&data->read_buffer, &count);
	ssize_t recv_bytes = recv(key->fd, ptr, count, 0);

	socket_state ret = REQUEST_DATA;

	if (recv_bytes <= 0) {
		return REQUEST_ERROR;
	}

	buffer_write_adv(&data->read_buffer, recv_bytes);  // avisa que hay recv_bytes bytes menos por leer
	// procesamiento
	bool error = false;

	enum request_state state = request_consume_data(&data->read_buffer, &data->request_parser, &error);

	if (request_is_done(state, 0)) {
		ret = request_process(key);
	}
	/*
	if (request_file_flush(st,
	                       &data->request_parser)) {  // request_file_flush returns true if the buffer is full
	                                                  // or the request is done

	    int fd = data->output_fd;
	    char* ptr = data->request_parser.request->data;

	    int index = data->request_parser.i;

	    int written = write(fd, ptr, index);
	    if (written < 0) {
	        perror("write");
	        return ERROR;
	    }
	}
	*/
	return ret;
}
void
request_data_init(unsigned int state, struct selector_key* key)
{
	printf("request_data_init\n %d", state);

	smtp_data* data = ATTACHMENT(key);
	data->request_parser.request = &data->request;
	request_parser_data_init(&data->request_parser);

	// We need to create a file in the maildir associated with the client
	// For doing so, we need to get the maildir associated with the client
	// My server doesn't work as a relay server, so we just need to create a file in the maildir associated with the
	// client

	// TODO: esto a una funcion
	char* maildir = get_and_create_maildir((char*)data->mail_from);
	if (maildir == NULL) {
		perror("get_and_create_maildir");
		return;
	}

	// now we create tmp dir within maildir
	char full_dir[MAIL_DIR_SIZE + 1 + DOMAIN_NAME_SIZE + 1 + LOCAL_USER_NAME_SIZE + 1 + MAILBOX_INNER_DIR_SIZE] = { 0 };
	snprintf(full_dir, sizeof(full_dir), "%s/tmp", maildir);
	if (create_directory_if_not_exists(full_dir) == -1) {
		perror("create_directory_if_not_exists");
		return;
	}

	char filename[MAIL_DIR_SIZE + 1 + DOMAIN_NAME_SIZE + 1 + LOCAL_USER_NAME_SIZE + 1 + MAILBOX_INNER_DIR_SIZE + 1 +
	              MS_TEXT_SIZE] = { 0 };
	time_t ms = time(NULL);
	// filename like mail/<domain>/<user>/tmp/<timestamp>
	snprintf(filename, sizeof(filename), "%s/tmp/%ld", maildir, ms);

	int fd = open(filename, O_CREAT | O_RDWR, 0777);
	if (fd < 0) {
		perror("open");
		return;
	}

	free(maildir);

	data->output_fd = fd;

	// We will write periodically to this file. Every time the buffer in the parser is full, we will write to the
	// file Also, we will write if we find a \r\n.\r\n in the buffer
}

void
request_data_close(unsigned int state, struct selector_key* key)
{
	if (state == REQUEST_DATA) {
		smtp_data* data = ATTACHMENT(key);
		// we copy the mail from mail/<domain>/<user>/tmp/<timestamp> to mail/<domain>/<rcpt_to>/new/<timestamp>
		// we need to create the new dir if it doesn't exist

		char* maildir = get_and_create_maildir((char*)data->rcpt_to);
		if (maildir == NULL) {
			perror("get_and_create_maildir");
			return;
		}

		// now we create new dir within maildir
		char full_dir[MAIL_DIR_SIZE + 1 + DOMAIN_NAME_SIZE + 1 + LOCAL_USER_NAME_SIZE + 1 + MAILBOX_INNER_DIR_SIZE] = {
			0
		};
		snprintf(full_dir, sizeof(full_dir), "%s/new", maildir);
		if (create_directory_if_not_exists(full_dir) == -1) {
			perror("create_directory_if_not_exists");
			return;
		}

		free(maildir);

		char filename[MAIL_DIR_SIZE + 1 + DOMAIN_NAME_SIZE + 1 + LOCAL_USER_NAME_SIZE + 1 + MAILBOX_INNER_DIR_SIZE + 1 +
		              MS_TEXT_SIZE] = { 0 };

		time_t ms = time(NULL);
		snprintf(filename, sizeof(filename), "%s/%ld", full_dir, ms);

		int new_fd = open(filename, O_CREAT | O_WRONLY, 0777);
		if (new_fd < 0) {
			perror("open");
			return;
		}

		// we need to copy the file from the tmp dir to the new dir
		// we need to read the file from the tmp dir

		char* domain = strchr((char*)data->rcpt_to, '@') + 1;
		char local_user[LOCAL_USER_NAME_SIZE] = { 0 };

		char tmp_filename[MAIL_DIR_SIZE + 1 + DOMAIN_NAME_SIZE + 1 + LOCAL_USER_NAME_SIZE + 1 + MAILBOX_INNER_DIR_SIZE +
		                  1 + MS_TEXT_SIZE] = { 0 };
		snprintf(tmp_filename, sizeof(tmp_filename), "mail/%s/%s/tmp/%ld", domain, local_user, ms);

		// the fd from tmp file is in data->output_fd

		// we need to read from the tmp file and write to the new file

		int tmp_fd = data->output_fd;

		char buffer[1024] = { 0 };

		ssize_t bytes_read = 0;
		lseek(tmp_fd, 0, SEEK_SET);  // we need to go to the beginning of the file because we have already written to it

		// This while loop is provisional. We need to read the whole file and apply the transformation if there is any
		// The reason for doing it as a while loop is because we haven't implemented transformations yet

		while ((bytes_read = read(tmp_fd, buffer, sizeof(buffer))) > 0) {
			ssize_t bytes_written = write(new_fd, buffer, bytes_read);
			if (bytes_written < 0) {
				perror("write");
				return;
			}
		}

		if (bytes_read < 0) {
			perror("read");
			return;
		}

		// we close the new file

		close(new_fd);

		// we close the tmp file

		request_close(&data->request_parser);
		close(data->output_fd);  // close the file
	}
}

char*
strndup(const char* s, size_t n)
{
	char* p = memchr(s, '\0', n);
	if (p != NULL)
		n = p - s;
	p = malloc(n + 1);
	if (p != NULL) {
		memcpy(p, s, n);
		p[n] = '\0';
	}
	return p;
}

int
create_directory_if_not_exists(char* maildir)
{
	// maildir tiene la forma mail/<domain>/<user>
	// i should check if the maildir exists, if not, create it
	struct stat st = { 0 };
	if (stat(maildir, &st) == -1) {
		if (mkdir(maildir, 0777) == -1) {  // if I want anyone to read, write or execute then i should use 0777
			perror("mkdir");
			return -1;
		}
	}
	return 0;
}

static char*
get_and_create_maildir(char* mail_from)
{
	int maildir_size = MAIL_DIR_SIZE + 1 + DOMAIN_NAME_SIZE + 1 + LOCAL_USER_NAME_SIZE;
	char* maildir = malloc(maildir_size);  // Consider dynamic sizing based on mail_from length
	if (maildir == NULL) {
		perror("malloc");
		return NULL;
	}
	char* domain = strchr(mail_from, '@');
	if (domain == NULL) {
		free(maildir);
		perror("strchr");
		return NULL;
	}
	domain++;
	char local_user[LOCAL_USER_NAME_SIZE] = { 0 };
	strncpy(local_user, mail_from, domain - mail_from - 1);
	snprintf(maildir, maildir_size, "mail/%s/%s", domain, local_user);

	// Create mail if it doesn't exist
	if (create_directory_if_not_exists("mail") == -1) {
		free(maildir);
		return NULL;
	}

	// Create mail/<domain> if it doesn't exist
	char domain_dir[100] = { 0 };
	snprintf(domain_dir, sizeof(domain_dir), "mail/%s", domain);
	if (create_directory_if_not_exists(domain_dir) == -1) {
		free(maildir);
		return NULL;
	}

	// Create mail/<domain>/<user> if it doesn't exist
	if (create_directory_if_not_exists(maildir) == -1) {
		free(maildir);
		return NULL;
	}

	return maildir;
}

void
on_done_init(const unsigned state, struct selector_key* key)
{
	printf("on_done_init\n %d", state);
	smtp_data* data = ATTACHMENT(key);
	free(data);
	// anything else to free?
}
