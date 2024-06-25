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

#include "access_registry.h"
#include "buffer.h"
#include "logger.h"
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

char* welcome_message = "220 local ESMTP Postfix (Ubuntu)\n"
                        "\nComandos SMTP\n"
                        "EHLO <user>\n"
                        "MAIL FROM:<user@local>\n"
                        "RCPTO TO:<other_user@local>\n\n"
                        "Comandos ESMTP\n"
                        "XAUTH token\n"
                        "XFROM <local_user>\n"
                        "XGET ALL\n"
                        "XGET dd/mm/yyyy\n"
                        "XALL <local_user>\n"
                        "XQUIT\n\n";

typedef enum request_state (*state_handler)(const uint8_t c, struct request_parser* p);
const fd_handler* get_smtp_handler(void);

unsigned int request_read_handler(struct selector_key* key);
void request_read_init(unsigned int state, struct selector_key* key);
void request_read_close(unsigned int state, struct selector_key* key);
static socket_state request_actual_read(struct selector_key* key);

unsigned int request_write_handler(struct selector_key* key);

unsigned int request_process(struct selector_key* key);

unsigned int request_data_handler(struct selector_key* key);
void request_data_init(unsigned int state, struct selector_key* key);
void request_data_close(unsigned int state, struct selector_key* key);

unsigned int request_admin_handler(struct selector_key* key);
void request_admin_init(unsigned int state, struct selector_key* key);
void request_admin_close(unsigned int state, struct selector_key* key);
void on_done_init(const unsigned state, struct selector_key* key);

unsigned int write_file_handler(struct selector_key* key);
void init_status(char* program);
bool read_complete(enum request_state st);
static inline void clean_request(struct selector_key* key);

typedef enum request_state (*consume_handler)(buffer* b, struct request_parser* p, bool* errored);

static const struct state_definition states_handlers[] = { {
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
	                                                           .on_read_ready = request_read_handler,
	                                                           .on_arrival = request_data_init,
	                                                           .on_departure = request_data_close,
	                                                       },
	                                                       {
	                                                           .state = REQUEST_ADMIN,
	                                                           .on_read_ready = request_read_handler,
	                                                           .on_arrival = request_admin_init,
	                                                           .on_departure = request_admin_close,
	                                                       },
	                                                       {
	                                                           .state = REQUEST_DATA_WRITE,
	                                                           .on_write_ready = write_file_handler,

	                                                       },

	                                                       {
	                                                           .state = REQUEST_DONE,
	                                                       },
	                                                       {
	                                                           .state = REQUEST_ERROR,
	                                                       }

};

process_handler handlers_table[] = {
	[EHLO] = handle_helo, [FROM] = handle_from,   [TO] = handle_to,       [DATA] = handle_data, [BODY] = handle_body,
	[ERROR] = NULL,       [XAUTH] = handle_xauth, [XFROM] = handle_xfrom, [XGET] = handle_xget
};

consume_handler consumers_table[] = { [REQUEST_READ] = request_consume,
	                                  [REQUEST_ADMIN] = request_consume_admin,
	                                  [REQUEST_DATA] = request_consume_data };

struct status config = { 0 };

static void read_handler(struct selector_key* key);
static void write_handler(struct selector_key* key);
static void close_handler(struct selector_key* key);
static void write_file(struct selector_key* key);

// BASICAMENTE LLAMAN A LOS HANDLERS DE LA MAQUINA DE ESTADOS

void
init_status(char* program)
{
	config.program = program;
	config.transform = program != NULL ? true : false;
}

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

	if (REQUEST_DONE == st || REQUEST_ERROR == st) {
		smtp_done(key);
	} else if (REQUEST_READ == st || REQUEST_DATA == st) {
		buffer* rb = &ATTACHMENT(key)->read_buffer;
		if (buffer_can_read(rb)) {
			read_handler(key);  // Si hay para leer en el buffer, sigo leyendo sin bloquearme
		}
	}
}
static void
close_handler(struct selector_key* key)
{
	stm_handler_close(&ATTACHMENT(key)->stm, key);
	monitor_close_connection();
}

static void
write_file(struct selector_key* key)
{
	smtp_data* data = ATTACHMENT(key);
	stm_handler_write(&data->stm, key);
}

static fd_handler smtp_handler = {
	.handle_read = read_handler,
	.handle_write = write_handler,
	.handle_close = close_handler,
};
static fd_handler file_handler = {
	.handle_read = NULL,
	.handle_write = write_file,
	.handle_close = NULL,
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
	log(LOG_DEBUG, "Accepted new connection");
	// Crear un nuevo socket

	// this struct may hold ipv4 or ipv6
	struct sockaddr_storage client_addr;

	socklen_t client_addr_len = sizeof(client_addr);

	int new_socket = accept(key->fd, (struct sockaddr*)&client_addr, &client_addr_len);

	if (new_socket < 0) {
		log(LOG_ERROR, "Error opening connection accepting socket");
		perror("accept");
		return;
	}

	smtp_data* data = calloc(1, sizeof(*data));

	if (data == NULL) {
		log(LOG_ERROR, "Error allocating memory for smtp data struct");
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

	memcpy(&data->raw_buff_write, welcome_message, strlen(welcome_message));
	buffer_write_adv(&data->write_buffer, strlen(welcome_message));

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

	char msg[RESPONSE_SIZE];
	smtp_state st = data->state;

	// if (SELECTOR_SUCCESS != selector_set_interest_key(key, OP_WRITE)) {
	// 	return REQUEST_ERROR;
	// }
	size_t count;

	uint8_t* ptr = buffer_write_ptr(&data->write_buffer, &count);

	// LLAMAR A HANDLERS NO SECUENCIALES
	bool is_noop = handle_noop(key, msg);
	bool is_quit = handle_quit(key, msg);
	bool is_rset = handle_reset(key, msg);
	bool is_xquit = handle_xquit(key, msg);
	if (is_quit) {
		return REQUEST_DONE;
	}

	// LLAMAR AL SECUENCIAL
	if (!(is_noop || is_rset || is_xquit)) {
		process_handler fn = handlers_table[st];
		smtp_state next = fn(key, msg);
		data->state = next;
	}

	size_t len = strlen(msg);
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

	logf(LOG_DEBUG, "key->fd: %d, ptr=%p, count=%lu", key->fd, ptr, count);
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
			if (data->state >= XAUTH && data->state <= XQUIT) {
				if (SELECTOR_SUCCESS == selector_set_interest_key(key, OP_READ)) {
					return REQUEST_ADMIN;
				}
			}
			if (SELECTOR_SUCCESS == selector_set_interest_key(key, OP_READ)) {
				return REQUEST_READ;
			} else {
				ret = REQUEST_ERROR;
			}
		}
	} else {
		perror("Send effed");
		log(LOG_FATAL, "Send error in write handler");
		ret = REQUEST_ERROR;
	}
	return ret;
}

//  REQUEST READ HANDLERS
void
request_read_init(unsigned int state, struct selector_key* key)
{
	logf(LOG_DEBUG, "Request read initiated, currently in state: %d", state);
	smtp_data* data = ATTACHMENT(key);
	data->request_parser.request = &data->request;
	request_parser_init(&data->request_parser);
}

static socket_state
request_actual_read(struct selector_key* key)
{
	smtp_data* data = ATTACHMENT(key);
	socket_state ret = data->stm.current->state;

	bool error;

	consume_handler consumer = consumers_table[data->stm.current->state];

	enum request_state state = consumer(&data->read_buffer, &data->request_parser, &error);

	if (data->stm.current->state == REQUEST_DATA) {
		if (SELECTOR_SUCCESS == selector_set_interest_key(key, OP_NOOP)) {
			if (SELECTOR_SUCCESS == selector_set_interest(key->s, data->output_fd, OP_WRITE)) {
				strcpy((char*)data->data, (char*)data->request_parser.request->data);
				ret = REQUEST_DATA_WRITE;
			} else
				ret = REQUEST_ERROR;
		} else {
			ret = REQUEST_ERROR;
		}
	}

	else {
		if (request_is_done(state, 0)) {
			if (SELECTOR_SUCCESS == selector_set_interest_key(key, OP_WRITE)) {
				// Procesamiento
				ret = request_process(key);
			} else {
				ret = REQUEST_ERROR;
			}
		}
	}

	return ret;
}

socket_state
request_read_handler(struct selector_key* key)
{
	smtp_data* data = ATTACHMENT(key);

	if (buffer_can_read(&data->read_buffer)) {
		return request_actual_read(key);
	}

	size_t count;
	uint8_t* ptr = buffer_write_ptr(&data->read_buffer, &count);
	ssize_t recv_bytes = recv(key->fd, ptr, count, 0);

	if (recv_bytes <= 0) {
		return REQUEST_ERROR;
	}

	buffer_write_adv(&data->read_buffer, recv_bytes);  // avisa que hay recv_bytes bytes menos por leer

	return request_actual_read(key);
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

	socket_state ret = REQUEST_ADMIN;

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
	if (state == request_error) {
		buffer_reset(&data->read_buffer);
	}
	return ret;
}

void
request_admin_init(unsigned int state, struct selector_key* key)
{
	logf(LOG_DEBUG, "Request admin initiated, currently in state: %d", state);
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

	return ret;
}
void
request_data_init(unsigned int state, struct selector_key* key)
{
	logf(LOG_DEBUG, "Request data initiated, currently in state: %d", state);
	smtp_data* data = ATTACHMENT(key);
	data->request_parser.request = &data->request;
	data->request_parser.output_fd = &data->output_fd;
	request_parser_data_init(&data->request_parser);
	data->is_body = false;

	int file = create_temp_mail_file((char*)data->mail_from, data->filename_fd);

	if (config.transform) {
		int pipe_fd[2];
		if (pipe(pipe_fd) != 0) {
			perror("Error while creating pipe");
			exit(EXIT_FAILURE);
		}

		int pid = fork();
		if (pid < 0) {
			perror("Error while creating slave");
			exit(EXIT_FAILURE);
		}

		if (pid == 0) {  // Child process
			close(STDIN_FILENO);
			dup2(pipe_fd[0], STDIN_FILENO);  // Correctly duplicate to stdin
			close(STDOUT_FILENO);
			dup2(file, STDOUT_FILENO);  // Correctly duplicate to stdout

			close(pipe_fd[0]);
			close(pipe_fd[1]);
			close(file);

			execlp(config.program, config.program, (char*)NULL);
			perror("Error while executing transformation program");
			exit(EXIT_FAILURE);
		}

		// Parent process
		close(pipe_fd[0]);             // Close read end, not used by parent
		close(file);                   // Close file, as it's now handled by child
		data->output_fd = pipe_fd[1];  // Use write end of the pipe to write data
	} else {
		data->output_fd = file;
	}

	// Escribir la información del remitente
	dprintf(data->output_fd, "MAIL FROM: <%s>\r\n", data->mail_from);

	// Escribir la información de los destinatarios
	for (size_t i = 0; i < data->rcpt_qty; i++) {
		dprintf(data->output_fd, "RCPT TO: <%s>\r\n", data->rcpt_to[i]);
	}
	dprintf(file, "DATA\r\n");

	selector_register(key->s, data->output_fd, &file_handler, OP_NOOP, data);
}

void
request_data_close(unsigned int state, struct selector_key* key)
{
	if (state == REQUEST_DATA) {
		smtp_data* data = ATTACHMENT(key);

		// // qne patch, replace if possible
		// for (size_t i = 0; i < data->rcpt_qty; i++) {
		// 	copy_temp_to_new_single((char*)data->rcpt_to[i], data->output_fd, data->filename_fd);
		// }

		request_close(&data->request_parser);
	}
}

socket_state
write_file_handler(struct selector_key* key)
{
	socket_state ret = REQUEST_DATA_WRITE;
	smtp_data* data = ATTACHMENT(key);

	char* data_buffer = (char*)data->data;
	size_t count = strlen(data_buffer);
	ssize_t n = write(data->output_fd, data_buffer, count);

	if (n < 0) {
		return REQUEST_ERROR;
	}

	if (SELECTOR_SUCCESS != selector_set_interest_key(key, OP_NOOP)) {
		return REQUEST_ERROR;
	}

	if (data->request_parser.state == request_done) {
		for (size_t i = 0; i < data->rcpt_qty; i++) {
			copy_temp_to_new_single((char*)data->rcpt_to[i], data->output_fd, data->filename_fd);
			time_t now = time(NULL);
			register_mail((char*)data->mail_from, (char*)data->rcpt_to[i], data->filename_fd, now);
		}
		if (SELECTOR_SUCCESS != selector_set_interest(key->s, data->fd, OP_WRITE)) {
			return REQUEST_ERROR;
		}

		close(data->output_fd);

		// rename  rcpt file

		if (SELECTOR_SUCCESS != selector_unregister_fd(key->s, data->output_fd))
			return REQUEST_ERROR;

		data->output_fd = 0;
		clean_request(key);

		// Procesamiento
		return request_process(key);

		// escribo mi rta
	} else {
		if (SELECTOR_SUCCESS != selector_set_interest(key->s, data->fd, OP_READ))
			return REQUEST_ERROR;
		else
			ret = REQUEST_DATA;
	}
	return ret;
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

void
on_done_init(const unsigned state, struct selector_key* key)
{
	printf("on_done_init\n %d", state);
	smtp_data* data = ATTACHMENT(key);
	free(data);
	// anything else to free?
}

static inline void
clean_request(struct selector_key* key)
{
	smtp_data* data = ATTACHMENT(key);
	// close(data->output_fd);
	// free(data->request.data); // freeing the data buffer
	memset(&data->request, 0, sizeof((data->request)));
	memset(&data->mail_from, 0, sizeof((data->mail_from)));
	memset(&data->rcpt_to, 0, sizeof((data->rcpt_to)));
	memset(&data->rcpt_qty, 0, sizeof((data->rcpt_qty)));
}
