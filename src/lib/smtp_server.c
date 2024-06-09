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

#include "smtp_server.h"

#include "buffer.h"
#include "selector.h"

#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

void destroy_socket(selector_key* data);
unsigned int write_handle_write(struct selector_key* key);
unsigned int read_handle_read(struct selector_key* key);
const fd_handler* get_smtp_handler(void);
static const struct state_definition smtp_states[] = {
	// definir los estados de la maquina de estados del protocolo SMTP
	// no necesariamente tenemos que llenar todos los campos en cada estado

	{

	    .state = REQUEST_READ,
	    .on_arrival = NULL,
	    .on_departure = NULL,
	    .on_read_ready = read_handle_read,
	    .on_write_ready = NULL,
	},

	{
	    .state = REQUEST_WRITE,
	    .on_arrival = NULL,
	    .on_departure = NULL,
	    .on_read_ready = NULL,
	    .on_write_ready = write_handle_write,
	},
	{
	    .state = DONE,
	    .on_arrival = NULL,
	    .on_departure = NULL,
	    .on_read_ready = NULL,
	    .on_write_ready = NULL,
	},
	{
	    .state = ERROR,
	    .on_arrival = NULL,
	    .on_departure = NULL,
	    .on_read_ready = NULL,
	    .on_write_ready = NULL,
	}

};

unsigned int
write_handle_write(struct selector_key* key)
{
	// smtp_data* data = ATTACHMENT(key);
	int send_bytes = send(key->fd, "Hola Bienvenido ! \0", 19, MSG_NOSIGNAL);
	if (send_bytes < 0) {
		perror("send");
		destroy_socket(key);
		return -1;
	}

	selector_set_interest_key(key, OP_READ);
	return REQUEST_READ;
}
unsigned int
read_handle_read(struct selector_key* key)
{
	// smtp_data* data = ATTACHMENT(key);
	char buffer[100];
	// read data from socket
	int read_bytes = recv(key->fd, buffer, 99, 0);

	if (read_bytes < 0) {
		perror("recv");
		destroy_socket(key);
		return -1;
	}
	printf("Read %d bytes\n", read_bytes);
	// print read_bytes from buffer
	buffer[read_bytes] = '\0';
	puts(buffer);
	selector_set_interest_key(key, OP_WRITE);
	return REQUEST_WRITE;
}

static void read_handler(struct selector_key* key);
static void write_handler(struct selector_key* key);
static void close_handler(struct selector_key* key);

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

static void
read_handler(struct selector_key* key)
{
	smtp_data* data = ATTACHMENT(key);
	stm_handler_read(&data->stm, key);
}
static void
write_handler(struct selector_key* key)
{
	smtp_data* data = ATTACHMENT(key);
	stm_handler_write(&data->stm, key);
}
static void
close_handler(struct selector_key* key)
{
	stm_handler_close(&ATTACHMENT(key)->stm, key);
	destroy_socket(key);
}

void
destroy_socket(selector_key* key)
{
	smtp_data* data = ATTACHMENT(key);

	selector_unregister_fd(key->s, data->fd);
	close(data->fd);
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

	// buffer_init(&data->read_buffer, BUFFER_SIZE, data->read_buffer.data);
	// buffer_init(&data->write_buffer, BUFFER_SIZE, data->write_buffer.data);

	stm_init(&data->stm);
	selector_status status = selector_register(key->s, new_socket, get_smtp_handler(), OP_WRITE, data);

	if (status != SELECTOR_SUCCESS) {
		perror("selector_register");
		destroy_socket(key);
		return;
	}
}
