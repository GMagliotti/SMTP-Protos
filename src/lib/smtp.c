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
#include "selector.h"

#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>

unsigned int request_read_handler(struct selector_key* key);
// unsigned int request_data_handler(struct selector_key* key);
unsigned int request_write_handler(struct selector_key* key);
unsigned int request_process(struct selector_key* key);
void request_read_init(unsigned int state, struct selector_key* key);
void request_read_close(unsigned int state, struct selector_key* key);
void smtp_done(selector_key* key);
const fd_handler* get_smtp_handler(void);

static const struct state_definition smtp_states[] = {
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
	    .on_read_ready = NULL,

	},
	{
	    .state = DONE,
	},
	{
	    .state = ERROR,
	    .on_write_ready = NULL,
	}

};

//
static void read_handler(struct selector_key* key);
static void write_handler(struct selector_key* key);
static void close_handler(struct selector_key* key);

// BASICAMENTE LLAMAN A LOS HANDLERS DE LA MAQUINA DE ESTADOS
static void
read_handler(struct selector_key* key)
{
	smtp_data* data = ATTACHMENT(key);
	const enum smtp_states st = stm_handler_read(&data->stm, key);
	if (ERROR == st || DONE == st) {
		smtp_done(key);
	}
}
static void
write_handler(struct selector_key* key)
{
	smtp_data* data = ATTACHMENT(key);
	const enum smtp_states st = stm_handler_write(&data->stm, key);
	if (ERROR == st || DONE == st) {
		smtp_done(key);
	}
}
static void
close_handler(struct selector_key* key)
{
	stm_handler_close(&ATTACHMENT(key)->stm, key);
	smtp_done(key);
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
	data->stm.max_state = ERROR;
	data->stm.states = smtp_states;

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
}

// REQUEST WRITE HANDLERS
enum smtp_states
request_write_handler(struct selector_key* key)
{
	smtp_data* data = ATTACHMENT(key);
	uint8_t* ptr;
	size_t count;
	ssize_t send_bytes;
	buffer* buff = &data->write_buffer;

	ptr = buffer_read_ptr(buff, &count);

	send_bytes = send(key->fd, ptr, count, MSG_NOSIGNAL);

	int ret;
	if (send_bytes >= 0) {
		buffer_read_adv(buff, send_bytes);  // avisa que hay send_bytes bytes menos por mandar (leer del buffer)
		if (!buffer_can_read(buff)) {
			// si no queda nada para mandar (leer del buffer write)
			if (SELECTOR_SUCCESS == selector_set_interest_key(key, OP_READ)) {
				ret = REQUEST_READ;
			} else {
				ret = ERROR;
			}
		}
	} else {
		ret = ERROR;
	}
	return ret;
}
void
request_read_init(unsigned int state, struct selector_key* key)
{
	printf("request_read_init\n %d", state);
	smtp_data* data = ATTACHMENT(key);
	data->request_parser.request = &data->request;
	request_parser_init(&data->request_parser);
}

// REQUEST READ HANDLERS
enum smtp_states
request_read_handler(struct selector_key* key)
{
	smtp_data* data = ATTACHMENT(key);
	size_t count;
	uint8_t* ptr = buffer_write_ptr(&data->read_buffer, &count);
	ssize_t recv_bytes = recv(key->fd, ptr, count, 0);

	int ret = REQUEST_READ;

	if (recv_bytes > 0) {
		buffer_write_adv(&data->read_buffer, recv_bytes);  // avisa que hay recv_bytes bytes menos por leer
		// procesamiento
		bool error = false;
		int st = request_consume(&data->read_buffer, &data->request_parser, &error);
		if (request_is_done(st, 0)) {
			// armado de la rta
			if (!error) {
				// no lei todos los bytes del buffer, pero quiero consumirlos como si hubiera terminado
				ret = request_process(key);
			} else {
				buffer_reset(&data->read_buffer);
			}
		}

	} else {
		ret = ERROR;
	}
	return ret;
}
// unsigned int request_data_handler(struct selector_key* key){
// 	/*
// 		Tenemos que leer hasta encontrar \r\n .
// 	*/

// }
enum smtp_states
request_process(struct selector_key* key)
{
	smtp_data* data = ATTACHMENT(key);

	int ret;
	size_t count;

	if (strcasecmp(data->request_parser.request->verb, "HELO") == 0 ||
	    strcasecmp(data->request_parser.request->verb, "EHLO") == 0) {
		// cambiar el estado a REQUEST_READ
		if (SELECTOR_SUCCESS == selector_set_interest_key(key, OP_WRITE)) {
			uint8_t* ptr = buffer_write_ptr(&data->write_buffer, &count);
			// acá entraría el tema del parser
			memcpy(ptr,
			       "250-foo.pdc\r\n250-PIPELINING\r\n250-SIZE "
			       "10240000\r\n250-VRFY\r\n250-ETRN\r\n250-STARTTLS\r\n250-ENHANCEDSTATUSCODES\r\n250-8BITMIME\r\n250-"
			       "DSN\r\n250-SMTPUTF8\r\n250 CHUNKING\r\n",
			       159);
			buffer_write_adv(&data->write_buffer, 159);
			ret = REQUEST_WRITE;
		}
	} else if (strcasecmp(data->request_parser.request->verb, "MAIL FROM") == 0) {
		if (SELECTOR_SUCCESS == selector_set_interest_key(key, OP_WRITE)) {
			memcpy(data->mail_from, data->request_parser.request->arg, N(data->request_parser.request->arg));
			uint8_t* ptr = buffer_write_ptr(&data->write_buffer, &count);
			memcpy(ptr, "250 2.1.0 Ok\n", 14);
			buffer_write_adv(&data->write_buffer, 14);
			ret = REQUEST_WRITE;
		}

	} else if (strcasecmp(data->request_parser.request->verb, "RCPT TO") == 0) {
		if (SELECTOR_SUCCESS == selector_set_interest_key(key, OP_WRITE)) {
			memcpy(data->rcpt_to, data->request_parser.request->arg, N(data->request_parser.request->arg));
			uint8_t* ptr = buffer_write_ptr(&data->write_buffer, &count);
			memcpy(ptr, "250 2.1.5 Ok\n", 14);
			buffer_write_adv(&data->write_buffer, 14);
			ret = REQUEST_WRITE;
		}

	} else if (strcasecmp(data->request_parser.request->verb, "DATA") == 0) {
		// cambiar el estado a REQUEST_DATA
		if (SELECTOR_SUCCESS == selector_set_interest_key(key, OP_WRITE)) {
			memcpy(data->rcpt_to, data->request_parser.request->arg, N(data->request_parser.request->arg));
			uint8_t* ptr = buffer_write_ptr(&data->write_buffer, &count);
			memcpy(ptr, "ACA EMPIEZA EL DATA\n", 21);
			buffer_write_adv(&data->write_buffer, 21);
			ret = REQUEST_WRITE;
		}
	} else {
		ret = ERROR;
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
