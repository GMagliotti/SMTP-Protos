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
#include "string_utils.h"

#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

unsigned int request_read_handler(struct selector_key* key);
unsigned int request_write_handler(struct selector_key* key);
static ssize_t send_msg(struct selector_key * key, const char * msg);
void request_read_init(unsigned int state, struct selector_key* key);
void request_read_close(unsigned int state, struct selector_key* key);
void smtp_done(selector_key* key);
static void handle_request(struct selector_key * key, char msg[MAX_COMMAND_LEN]);
const fd_handler* get_smtp_handler(void);

static const struct state_definition smtp_states[] = {
	// definir los estados de la maquina de estados del protocolo SMTP
	// no necesariamente tenemos que llenar todos los campos en cada estado

	{
		//Aca irian las funciones que utilizan el command parser
	    .state = REQUEST_READ,
	    .on_read_ready = request_read_handler,
	    .on_arrival = request_read_init,
		.on_departure = request_read_close,
	},

	{
		//Aca irian las funciones que utilizan el command parser
	    .state = REQUEST_WRITE,
	    .on_write_ready = request_write_handler,
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
	printf("220 Bienvenido al servidor SMTP\n\0");
	buffer_write_adv(&data->write_buffer, 32);

	selector_status status = selector_register(key->s, new_socket, get_smtp_handler(), OP_WRITE, data);

	if (status != SELECTOR_SUCCESS) {
		perror("selector_register");
		smtp_done(key);
		return;
	}
}

// REQUEST WRITE HANDLERS
unsigned int
request_write_handler(struct selector_key* key)
{
	smtp_data* data = ATTACHMENT(key);

	buffer * output_buffer = &data->write_buffer;
	buffer * input_buffer = &data->read_buffer;

	//Veamos que efectivamente haya algo para leer
	if(!buffer_can_read(input_buffer)){
		selector_set_interest_key(key, OP_READ);
		return REQUEST_READ;
	}

	smtp_command * smtp_command = &data->command_parser;
	parse_command(key, smtp_command, input_buffer);
	if(!smtp_command->ended){
		selector_set_interest_key(key, OP_READ);
		return REQUEST_READ;
	}

	//Tenemos una linea!
	char msg[MAX_LINE_LEN];
	//Procesamos!
	handle_request(key, msg);
	//Ahora enviamos al socket
	ssize_t sent_bytes = send_msg(key, msg);
	if(sent_bytes < 0){
		return -1;
	}
	buffer_read_adv(output_buffer, sent_bytes);
	
	//Veamos si quedan cosas por leer
	if(buffer_can_read(input_buffer)) {
		selector_set_interest_key(key, OP_WRITE);
	} else {
        selector_set_interest_key(key, OP_READ);
    }
}

static ssize_t send_msg(struct selector_key * key, const char * msg){

        if (key == NULL) {
                //TODO loggear error NULL key
                return -1;
        }

        if (msg == NULL) {
                //TODO loggear error NULL msg
                return -1;
        }

		smtp_data * data = ATTACHMENT(key);
		buffer * output_buffer = &data->write_buffer;
		size_t msg_len = strlen(msg);

		//Veamos de escribir en el buffer de salida
		if(!buffer_can_write(output_buffer)){
			buffer_compact(output_buffer);
			if(!buffer_can_write(output_buffer)){
				return -1;
			}
		}

		size_t write_limit = 0;
		//Pido un puntero para escribir, tambien me dice cuanto puedo escribir
		char * w_ptr =  (char *)buffer_write_ptr(output_buffer, &write_limit);
		strcpy(w_ptr, msg);
		//Hago avanzar el puntero de escritura para poder usar el de lectura
		buffer_write_adv(output_buffer, msg_len);

		size_t read_limit = 0;
		char * r_ptr =  (char *)buffer_read_ptr(output_buffer, &read_limit);
		//Escribimos en el socket
		ssize_t sent_bytes = send(key->fd, r_ptr, read_limit, 0);
		if(sent_bytes < 0){
			return -1;
		}

		return sent_bytes;
}

void
request_read_init(unsigned int state, struct selector_key* key)
{
	smtp_data * data = ATTACHMENT(key);
	data->request_parser.request = &data->request;
	//Inicializamos maquina de estados para comandos
	init_command_parsing(&data->command_parser);
	parser_configuration();
}

// REQUEST READ HANDLERS
unsigned int
request_read_handler(struct selector_key* key)
{
	smtp_data* data = ATTACHMENT(key);
	//Buffer donde se escriben los datos a leer por el servidor
	buffer * data_buffer = &data->read_buffer;
	//Podemos escribir en el buffer o todavia hay algo por leer?
	if(!buffer_can_write(data_buffer)){
		//Todavia hay por leer
		buffer_compact(&data->read_buffer);
		if(!buffer_can_write(data_buffer)){
			//Si aun asi todavia puedo escribir, hay algo mal
			if(buffer_can_read(&data->write_buffer)){
				selector_set_interest_key(key, OP_WRITE);
				return REQUEST_WRITE;
			}
			
			return ERROR;
		}
	}
	//read data from socket
	size_t count = 0;
	//pido un puntero para escribir, tambien me dice cuanto puedo escribir
	uint8_t* ptr = buffer_write_ptr(&data->read_buffer, &count);
	//Escribimos en el buffer de lectura lo leido del socket
	ssize_t recv_bytes = recv(key->fd, ptr, count, 0);

	//Si se leyo algo entramos
	if (recv_bytes > 0) {
		//Hacemos avanzar el puntero de escritura
		buffer_write_adv(&data->read_buffer, recv_bytes);  
		selector_set_interest_key(key, OP_WRITE);
		return REQUEST_WRITE;

	} else if (recv_bytes == 0) {
		return DONE;
	} else return ERROR;
}

void
request_read_close(unsigned int state, struct selector_key* key)
{
	if (state == REQUEST_READ) {
		smtp_data* data = ATTACHMENT(key);
		request_close(&data->request_parser);
	}
}

static void handle_request(struct selector_key * key, char msg[MAX_COMMAND_LEN])
{
	smtp_data * data = ATTACHMENT(key);
	char * cmd = data->command_parser.command;
	convertToUpper(cmd);

	if(strcmp(cmd, "EHLO") == 0){
		sprintf(msg, "EHLO\r\n");
	} else if(strcmp(cmd, "HELO") == 0) {
		sprintf(msg, "HELO\r\n");
	} else if(strcmp(cmd, "MAIL") == 0) {
		sprintf(msg, "MAIL\r\n");
	} else if(strcmp(cmd, "RCPT") == 0) {
		sprintf(msg, "RCPT\r\n");
	} else if(strcmp(cmd, "DATA") == 0) {
		sprintf(msg, "DATA\r\n");
	} else if(strcmp(cmd, "RSET") == 0) {
		sprintf(msg, "RSET\r\n");
	} else if(strcmp(cmd, "VRFY") == 0) {
		sprintf(msg, "VRFY\r\n");
	} else if(strcmp(cmd, "EXPN") == 0) {
		sprintf(msg, "EXPN\r\n");
	} else if(strcmp(cmd, "HELP") == 0) {
		sprintf(msg, "HELP\r\n");
	} else if(strcmp(cmd, "NOOP") == 0) {
		sprintf(msg, "NOOP\r\n");
	} else if(strcmp(cmd, "QUIT") == 0) {
		sprintf(msg, "QUIT\r\n");
	} else {
		sprintf(msg, "Invalid command\r\n");
	}
}