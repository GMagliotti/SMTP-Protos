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

#include <fcntl.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

unsigned int request_read_handler(struct selector_key* key);
// unsigned int request_data_handler(struct selector_key* key);
unsigned int request_write_handler(struct selector_key* key);
unsigned int request_process(struct selector_key* key);
void request_read_init(unsigned int state, struct selector_key* key);
void request_read_close(unsigned int state, struct selector_key* key);
void smtp_done(selector_key* key);
const fd_handler* get_smtp_handler(void);
void request_data_init(unsigned int state, struct selector_key* key);
void request_data_close(unsigned int state, struct selector_key* key);
unsigned int request_data_handler(struct selector_key* key);

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
	    .on_read_ready = request_data_handler,  // handler para guardar en un buffer hasta encontrar <CRLF>.<CRLF>
	    .on_arrival = request_data_init,        // setteamos el parser. Abrimos el FD al archivo de salida.
	    .on_departure = request_data_close,     // cerramos el parser

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
				if (SELECTOR_SUCCESS == selector_set_interest_key(key, OP_WRITE)) {
					uint8_t* ptr = buffer_write_ptr(&data->write_buffer, &count);
					memcpy(ptr, "502 5.5.2 Error: command not recognized\n", 41);
					buffer_write_adv(&data->write_buffer, 41);
					ret = REQUEST_WRITE;
				}
			}
		} else if (request_is_data(st)) {
			ret = REQUEST_DATA;
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
			memcpy(ptr, "250 2.0.0 Ok: queued as FIXME\n", 30);
			buffer_write_adv(&data->write_buffer, 30);
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
get_maildir_and_create(char* mail_from)
{
	char* maildir = malloc(100);  // Consider dynamic sizing based on mail_from length
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
	char* user = strndup(mail_from, domain - mail_from - 1);
	if (user == NULL) {
		free(maildir);
		perror("strndup");
		return NULL;
	}
	snprintf(maildir, 100, "mail/%s/%s", domain, user);

	// Create mail if it doesn't exist
	if (create_directory_if_not_exists("mail") == -1) {
		free(maildir);
		free(user);
		return NULL;
	}

	// Create mail/<domain> if it doesn't exist
	char domain_dir[100] = { 0 };
	snprintf(domain_dir, sizeof(domain_dir), "mail/%s", domain);
	if (create_directory_if_not_exists(domain_dir) == -1) {
		free(maildir);
		free(user);
		return NULL;
	}

	// Create mail/<domain>/<user> if it doesn't exist
	if (create_directory_if_not_exists(maildir) == -1) {
		free(maildir);
		free(user);
		return NULL;
	}

	free(user);
	return maildir;
}

void
request_data_init(unsigned int state, struct selector_key* key)
{
	printf("request_data_init\n %d", state);
	// we need to print message 354 End data with <CR><LF>.<CR><LF>\n
	size_t count;
	uint8_t* ptr = buffer_write_ptr(&ATTACHMENT(key)->write_buffer, &count);
	memcpy(ptr, "354 End data with <CR><LF>.<CR><LF>\n", 36);
	buffer_write_adv(&ATTACHMENT(key)->write_buffer, 36);

	// we need to re-open the request_parser to start reading the data
	smtp_data* data = ATTACHMENT(key);
	data->request_parser.request = &data->request;
	request_parser_data_init(&data->request_parser);

	// We need to create a file in the maildir associated with the client
	// For doing so, we need to get the maildir associated with the client
	// My server doesn't work as a relay server, so we just need to create a file in the maildir associated with the
	// client

	char* maildir = get_maildir_and_create((char*)data->mail_from);
	if (maildir == NULL) {
		perror("get_maildir_and_create");
		return;
	}

	char filename[100] = { 0 }; // FIXME: filename should be somthing like time in ms
	snprintf(filename, sizeof(filename), "%s/out", maildir);

	int fd = open(filename, O_CREAT | O_WRONLY, 0644);
	if (fd < 0) {
		perror("open");
		return;
	}

	free(maildir);

	data->output_fd = fd;

	// We will write periodically to this file. Every time the buffer in the parser is full, we will write to the file
	// Also, we will write if we find a \r\n.\r\n in the buffer
}

void
request_data_close(unsigned int state, struct selector_key* key)
{
	if (state == REQUEST_DATA) {
		smtp_data* data = ATTACHMENT(key);
		request_close(&data->request_parser);
		close(data->output_fd);  // close the file
	}
}

unsigned int
request_data_handler(struct selector_key* key)
{
	smtp_data* data = ATTACHMENT(key);
	size_t count;
	uint8_t* ptr = buffer_write_ptr(&data->read_buffer, &count);
	ssize_t recv_bytes = recv(key->fd, ptr, count, 0);

	int ret = REQUEST_DATA;

	if (recv_bytes > 0) {
		buffer_write_adv(&data->read_buffer, recv_bytes);  // avisa que hay recv_bytes bytes menos por leer
		// procesamiento
		bool error = false;
		int st = request_consume(&data->read_buffer,
		                         &data->request_parser,
		                         &error);  // no necesitamos usar otro parser distinto. Ya se encuentra implementada la
		                                   // lógica de data en este
		if (request_is_done(st, 0)) {
			// armado de la rta
			if (!error) {
				// no lei todos los bytes del buffer, pero quiero consumirlos como si hubiera terminado
				ret = request_process(key);
			} else {
				buffer_reset(&data->read_buffer);
				if (SELECTOR_SUCCESS == selector_set_interest_key(key, OP_WRITE)) {
					uint8_t* ptr = buffer_write_ptr(&data->write_buffer, &count);
					memcpy(ptr, "502 5.5.2 Error: command not recognized\n", 41);
					buffer_write_adv(&data->write_buffer, 41);
					ret = REQUEST_WRITE;
				}
			}
		}
		if (request_file_flush(st,
		                       &data->request_parser)) {  // request_file_flush returns true if the buffer is full or
			                                              // the request is done

			int fd = data->output_fd;
			char* ptr = data->request_parser.request->data;

			int index = data->request_parser.i;

			int written = write(fd, ptr, index);
			if (written < 0) {
				perror("write");
				return ERROR;
			}
		}

	} else {
		ret = ERROR;
	}
	return ret;
}
