#ifndef Au9MTAsFSOTIW3GaVruXIl3gVBU_REQUEST_H
#define Au9MTAsFSOTIW3GaVruXIl3gVBU_REQUEST_H

#include "buffer.h"

#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
/*   The SOCKS request is formed as follows:
 *
 *      +----+-----+-------+------+----------+----------+
 *      |VER | CMD |  RSV  | ATYP | DST.ADDR | DST.PORT |
 *      +----+-----+-------+------+----------+----------+
 *      | 1  |  1  | X'00' |  1   | Variable |    2     |
 *      +----+-----+-------+------+----------+----------+
 *
 *   Where:
 *
 *        o  VER    protocol version: X'05'
 *        o  CMD
 *           o  CONNECT X'01'
 *           o  BIND X'02'
 *           o  UDP ASSOCIATE X'03'
 *        o  RSV    RESERVED
 *        o  ATYP   address type of following address
 *           o  IP V4 address: X'01'
 *           o  DOMAINNAME: X'03'
 *           o  IP V6 address: X'04'
 *        o  DST.ADDR       desired destination address
 *        o  DST.PORT desired destination port in network octet
 *           order
 */
/*
 * miembros de la sección 4: `Requests'
 *  - Cmd
 *  - AddressType
 *  - Address: IPAddress (4 y 6), DomainNameAdddres
 */

#define DATA_SIZE 1024

struct request
{
	char verb[16];
	char arg[256];
	char data[DATA_SIZE];
};

enum request_state
{
	request_flush = 0,
	request_helo,
	request_arg,
	request_cr,
	request_from,
	request_mail_from,
	request_to,
	request_mail_to,
	request_data,
	request_body,
	request_done,
	request_error,
};

struct state_message
{
	const char* success;
	const char* error;
};
// Arreglo de mensajes para cada estado
// Arreglo de mensajes para cada estado
static const struct state_message state_messages[] = {
	[request_helo] = { "250 HELO OK\r\n", "500 HELO Error\r\n" },
	[request_arg] = { "250 ARG OK\r\n", "500 ARG Error\r\n" },
	[request_from] = { "250 MAIL FROM OK\r\n", "500 MAIL FROM Error\r\n" },
	[request_to] = { "250 RCPT TO OK\r\n", "500 RCPT TO Error\r\n" },
	[request_mail_from] = { "250 MAIL FROM Address OK\r\n", "500 MAIL FROM Address Error\r\n" },
	[request_mail_to] = { "250 RCPT TO Address OK\r\n", "500 RCPT TO Address Error\r\n" },
	[request_data] = { "354 Start mail input; end with <CRLF>.<CRLF>\r\n", "500 DATA Error\r\n" },
	[request_body] = { "250 DATA Body OK\r\n", "500 DATA Body Error\r\n" },
	[request_cr] = { "250 CR OK\r\n", "500 CR Error\r\n" },
	[request_done] = { "221 Bye\r\n", "500 Error\r\n" }
};

typedef struct request_parser
{
	struct request* request;
	enum request_state state;
	enum request_state next_state;
	enum request_state last_state;

	/** cuantos bytes tenemos que leer*/
	unsigned int n;
	/** cuantos bytes ya leimos */
	unsigned int i;
} request_parser;

/** inicializa el parser */
void request_parser_init(struct request_parser* p);

/** inicializa el parser para la lectura de data */
void request_parser_data_init(struct request_parser* p);

/** entrega un byte al parser. retorna true si se llego al final  */
enum request_state request_parser_feed(struct request_parser* p, const uint8_t c);

/**
 * por cada elemento del buffer llama a `request_parser_feed' hasta que
 * * el parseo se encuentra completo o se requieren mas bytes.

 * @param errored parametro de salida. si es diferente de NULL se deja dicho
 *   si el parsing se debió a una condición de error
 */
enum request_state request_consume(buffer* b, struct request_parser* p, bool* errored);

/**
 * Permite distinguir a quien usa socks_hello_parser_feed si debe seguir
 * enviando caracters o no.
 *
 * En caso de haber terminado permite tambien saber si se debe a un error
 */
bool request_is_done(const enum request_state st, bool* errored);

bool request_is_data(const enum request_state st);
bool request_is_data_body(const enum request_state st);

/** Devuelve verdadero si es necesario flushear al archivo de salida */
bool request_file_flush(enum request_state st, struct request_parser* p);

void request_close(struct request_parser* p);

#endif