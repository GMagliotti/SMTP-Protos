/*
Acá vamos a definir el read handler que acepta las conexiones entrantes, agrega el nuevo fd al selector.
Vamos a definir una maquina de estados para manejar el socket activo  (el que se acaba de aceptar). -> READ / WRITE
Vamos a definir otra maquina de estados para manejar el Protocolo SMTP -> Los estados del protocolo SMTP

Una Maquina de Estados para cada entrada del Selector:
    * read
    * write
    * close   # Estos handlers van a llamar a los handlers del del state de la segunda stm
    * block

Una Maquina de estado para cada Cliente:
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



    struct socks5
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

#include "lib/headers/buffer.h"
#include "lib/headers/selector.h"
#include "lib/headers/stm.h"

#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

void destroy_socket(selector_key* data);

static const struct state_definition smtp_states[] = {
	// definir los estados de la maquina de estados del protocolo SMTP
	// no necesariamente tenemos que llenar todos los campos en cada estado
	{
	    .state = EHLO,
	    .on_arrival = NULL,
	    .on_departure = NULL,
	    .on_read_ready = NULL,
	    .on_write_ready = NULL,

	},

	{
	    .state = FROM,
	    .on_arrival = NULL,
	    .on_departure = NULL,
	    .on_read_ready = NULL,
	    .on_write_ready = NULL,
	},

	{
	    .state = RCPT,
	    .on_arrival = NULL,
	    .on_departure = NULL,
	    .on_read_ready = NULL,
	    .on_write_ready = NULL,
	},

	{
	    .state = DATA,
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
	},

	{
	    .state = DONE,
	    .on_arrival = NULL,
	    .on_departure = NULL,
	    .on_read_ready = NULL,
	    .on_write_ready = NULL,
	},
};

static void read_handler(struct selector_key* key);
static void write_handler(struct selector_key* key);
static void block_handler(struct selector_key* key);
static void close_handler(struct selector_key* key);

static fd_handler smtp_handler = {
	.handle_read = read_handler,
	.handle_write = write_handler,
	.handle_block = block_handler,
	.handle_close = close_handler,
};

const fd_handler*
get_smtp_handler(void)
{
	return &smtp_handler;
};

static void read_handler(struct selector_key* key) {};
static void write_handler(struct selector_key* key) {};
static void block_handler(struct selector_key* key) {};
static void close_handler(struct selector_key* key) {};

void
destroy_socket(selector_key* key)
{
	smtp_data* data = ATTACHMENT(key);

	selector_unregister_fd(key->s, data->fd);

	buffer_destroy(&data->read_buffer);
	buffer_destroy(&data->write_buffer);

	free(data);
};

void
new_active_socket(selector_key* key)
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
	data->stm.initial = EHLO;
	data->stm.max_state = DONE;
	data->stm.states = smtp_states;

	buffer_init(&data->read_buffer, BUFFER_SIZE, N(data->read_buffer.data));
	buffer_init(&data->write_buffer, BUFFER_SIZE, N(data->write_buffer.data));

	stm_init(&data->stm);

	// read, write, block, close handlers para el nuevo socket TODO
	selector_status status = selector_register(key->s, new_socket, NULL, OP_READ, data);

	if (status != SELECTOR_SUCCESS) {
		perror("selector_register");
		close(new_socket);
		free(data);
		return;
	}
}
