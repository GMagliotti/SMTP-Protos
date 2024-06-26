#ifndef REQUEST_H
#define REQUEST_H
#include "buffer.h"
#include <stdlib.h>
#include <fcntl.h>

#define DATA_SIZE 4097
enum request_state
{
	request_verb,
	request_arg,
	request_cr,
	request_data,
	request_body,
	request_done,
	request_error,

};

struct request
{
	char verb[16];
	char arg[256];
	char data[DATA_SIZE];
	//char* data;
	//unsigned int data_size;
};

typedef struct request_parser
{
	struct request* request;
	enum request_state state;

	int* output_fd;
	/** cuantos bytes tenemos que leer*/
	unsigned int n;
	/** cuantos bytes ya leimos */
	unsigned int i;
} request_parser;

void request_parser_init(struct request_parser* p);
/** entrega un byte al parser. retorna true si se llego al final  */
enum request_state request_parser_feed(struct request_parser* p, const uint8_t c);
/**
 * por cada elemento del buffer llama a `request_parser_feed' hasta que
 * el parseo se encuentra completo o se requieren mas bytes.
 *
 * @param errored parametro de salida. si es diferente de NULL se deja dicho
 *   si el parsing se debió a una condición de error
 */
enum request_state request_consume(buffer* b, struct request_parser* p, bool* errored);




void request_parser_data_init(struct request_parser* p);
/** entrega un byte al parser. retorna true si se llego al final  */
enum request_state request_parser_data_feed(struct request_parser* p, const uint8_t c);
/**
 * por cada elemento del buffer llama a `request_parser_data_feed' hasta que
 * el parseo se encuentra completo o se requieren mas bytes.
 *
 * @param errored parametro de salida. si es diferente de NULL se deja dicho
 *   si el parsing se debió a una condición de error
 */
enum request_state request_consume_data(buffer* b, struct request_parser* p, bool* errored);



void request_parser_admin_init(struct request_parser* p);
/** entrega un byte al parser. retorna true si se llego al final  */
enum request_state request_parser_admin_feed(struct request_parser* p, const uint8_t c);
/**
 * por cada elemento del buffer llama a `request_parser_data_feed' hasta que
 * el parseo se encuentra completo o se requieren mas bytes.
 *
 * @param errored parametro de salida. si es diferente de NULL se deja dicho
 *   si el parsing se debió a una condición de error
 */
enum request_state request_consume_admin(buffer* b, struct request_parser* p, bool* errored);




/**
 * enviando caracters o no.
 *
 * En caso de haber terminado permite tambien saber si se debe a un error
 */
bool request_is_done(const enum request_state st, bool* errored);
void request_close(struct request_parser* p);

#endif