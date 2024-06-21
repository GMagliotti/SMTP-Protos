/**
 * request.c -- parser del request de SOCKS5
 */
#include "request.h"

#include <arpa/inet.h>
#include <string.h>
#include <strings.h>

typedef enum request_state (*state_handler)(const uint8_t c, request_parser* p);

// Prototipos de funciones estáticas
static enum request_state handle_helo(const uint8_t c, request_parser* p);
static enum request_state handle_arg(const uint8_t c, request_parser* p);
static enum request_state handle_from(const uint8_t c, request_parser* p);
static enum request_state handle_to(const uint8_t c, request_parser* p);
static enum request_state handle_mail(const uint8_t c, request_parser* p);
static enum request_state handle_body(const uint8_t c, request_parser* p);
static enum request_state handle_data(const uint8_t c, request_parser* p);
static enum request_state handle_cr(const uint8_t c, request_parser* p);
enum request_state state_error(enum request_state next, struct request_parser* p);

// Array de punteros a funciones para manejar estados
static state_handler state_handlers[] = {
	[request_helo] = handle_helo, [request_arg] = handle_arg,      [request_from] = handle_from,
	[request_to] = handle_to,     [request_mail_to] = handle_mail, [request_mail_from] = handle_mail,
	[request_body] = handle_body, [request_data] = handle_data,    [request_cr] = handle_cr,
};

// Estructura para almacenar los mensajes de éxito y error

extern void
request_parser_init(request_parser* p)
{
	p->state = p->next_state;
	p->i = 0;
}

extern void
request_parser_data_init(request_parser* p)
{
	p->state = request_body;
	p->i = 0;
}

extern enum request_state
request_parser_feed(request_parser* p, const uint8_t c)
{
	return state_handlers[p->state](c, p);
}

extern enum request_state
request_consume(buffer* b, request_parser* p, bool* errored)
{
	enum request_state st = p->state;
	while (buffer_can_read(b)) {
		const uint8_t c = buffer_read(b);
		st = request_parser_feed(p, c);
		if (request_is_done(st, errored) || st == request_flush) {
			if (*errored) {
				p->i = 0;
			}
			break;
		}

		// if (request_is_data_body(st)) {
		// 	return st;
		// }
	}
	return st;
}

extern bool
request_is_done(const enum request_state st, bool* errored)
{
	if (st == request_error) {
		*errored = true;
	}
	return st >= request_done && st <= request_error;
}

extern bool
request_is_data(const enum request_state st)
{
	return st == request_data;
}

extern bool
request_is_data_body(const enum request_state st)
{
	return st == request_body;
}

// extern bool
// request_file_flush(enum request_state st, request_parser* p)
// {
// 	// es necesario flushear si estamos en data_body y si el buffer está lleno
// 	// o bien si estamos en doneF
// 	(void*) p;
// 	(void*) st;
// 	return false;
// }
enum request_state
jump_to_error(enum request_state next, struct request_parser* p)
{
	p->last_state = next;
	return request_error;
}

enum request_state
jump_to_internal(enum request_state next, struct request_parser* p)
{
	p->state = next;
	return next;
}
void
jump_from_internal(enum request_state next,
                   enum request_state current,
                   enum request_state last,
                   struct request_parser* p)
{
	p->last_state = last;
	p->next_state = next;
	p->state = current;
}

// enum request_state
// jump(enum request_state next, struct request_parser* p)
// {
// 	p->last_state =
// }
void
request_close(request_parser* p)
{
	memset(p->request, 0, sizeof(*(p->request)));
	p->state = request_helo;
	p->i = 0;
}

static enum request_state
handle_helo(const uint8_t c, request_parser* p)
{
	enum request_state next;
	switch (c) {
		case ' ':
			p->request->verb[p->i] = '\0';
			if (strcasecmp(p->request->verb, "HELO") == 0 || strcasecmp(p->request->verb, "EHLO") == 0) {
				p->i = 0;
				next = jump_to_internal(request_arg, p);
				// next = request_arg;
				// p->state = next;
			} else {
				next = jump_to_error(request_helo, p);
				// p->last_state = request_helo;
				// next = request_error;
			}
			break;
		case '\r':
			// p->last_state = request_helo;
			// next = request_error;
			break;
		default:
			if (p->i < sizeof(p->request->verb) - 1) {
				p->request->verb[p->i++] = (char)c;
				next = request_helo;
			} else {
				next = jump_to_error(request_helo, p);
				// p->last_state = request_helo;
				// next = request_error;
			}
			break;
	}
	return next;
}
static enum request_state
handle_arg(const uint8_t c, request_parser* p)
{
	enum request_state next;
	switch (c) {
		case '\r':
			p->request->arg[p->i] = '\0';
			jump_from_internal(request_from, request_helo, request_cr, p);
			// p->next_state = request_from;
			// p->last_state = request_helo;
			// p->state = request_cr;
			break;
		default:
			if (p->i < sizeof(p->request->arg) - 1) {
				p->request->arg[p->i++] = (char)c;
				next = request_arg;
			} else {
				next = request_error;
			}
			break;
	}
	return next;
}
static enum request_state
handle_from(const uint8_t c, request_parser* p)
{
	enum request_state next;

	switch (c) {
		case ':':
			p->request->verb[p->i] = '\0';
			if (strcasecmp(p->request->verb, "MAIL FROM") == 0) {
				p->i = 0;
				next = jump_to_internal(request_mail_from, p);
				// next = request_mail_from;
				// p->state = next;
			}
			break;
		case '\r':
			next = request_error;
			break;
		default:
			if (p->i < sizeof(p->request->verb) - 1) {
				p->request->verb[p->i++] = (char)c;
				next = request_from;
			} else {
				next = jump_to_error(request_from, p);
				// next = request_error;
			}
			break;
	}
	return next;
}
static enum request_state
handle_to(const uint8_t c, request_parser* p)
{
	enum request_state next;

	switch (c) {
		case ':':
			p->request->verb[p->i] = '\0';
			if (strcasecmp(p->request->verb, "RCPT TO") == 0) {
				p->i = 0;
				next = jump_to_internal(request_mail_to, p);
				// next = request_mail_to;
				// p->state = next;

			} else {
				next = jump_to_error(request_to, p);

				// next = request_error;
			}
			break;
		case '\r':
			next = jump_to_error(request_to, p);
			// next = request_error;
			break;
		default:
			if (p->i < sizeof(p->request->verb) - 1) {
				p->request->verb[p->i++] = (char)c;
				next = request_to;
			} else {
				next = jump_to_error(request_to, p);
				// next = request_error;
			}
			break;
	}
	return next;
}
static enum request_state
handle_mail(const uint8_t c, request_parser* p)
{
	enum request_state next;

	switch (c) {
		case '<':
			if (p->i != 0) {
				next = jump_to_error(p->state, p);
				// next = request_error;
			}
			next = p->state;
			break;
		case '>':
			if (p->i == 0) {
				next = jump_to_error(p->state, p);

				return next;
			}
			p->request->arg[p->i] = '>';
			next = p->state;
			break;
		case '\r':
			// el caracter anterior tiene que ser '>'
			if (p->request->arg[p->i] != '>') {
				return request_error;
			}
			p->request->arg[p->i] = '\0';
			jump_from_internal(p->state + 1, request_cr, p->state, p);
			// p->next_state = p->state + 1;
			// p->last_state = p->state;
			// p->state = request_cr;

			break;
		case '\n':
			next = jump_to_error(p->state, p);
			// next = request_error;
			break;
		default:
			if (p->i < sizeof(p->request->arg) - 1) {
				p->request->arg[p->i++] = (char)c;

				next = p->state;
			} else {
				next = jump_to_error(p->state, p);
				// next = request_error;
			}
			break;
	}
	return next;
}
static enum request_state
handle_body(const uint8_t c, request_parser* p)
{
	enum request_state next;
 	switch (c) {
		case '\r':
			if (p->i > 1 && p->request->data[p->i - 1] == '.' && p->request->data[p->i - 2] == '\n') {
				p->request->data[p->i - 3] = '\0';
				jump_from_internal(request_done, request_cr, request_body, p);
				// p->next_state = request_done;
				// p->last_state = request_body;
				// next = request_cr;
			} else {
				next = request_body;
			}
			break;
		default:
			break;
	}

	if (p->i < sizeof(p->request->data) - 1) {
		p->request->data[p->i++] = (char)c;
		next = request_body;
	} else {
		next = request_flush;
	}

	return next;
}
static enum request_state
handle_data(const uint8_t c, request_parser* p)
{
	enum request_state next;

	switch (c) {
		case '\r':
			if (strcasecmp(p->request->verb, "DATA") == 0) {
				// goto
				p->request->verb[p->i] = '\0';

				jump_from_internal(request_body, request_cr, request_data, p);

				// p->last_state = request_data;
				// p->next_state = request_body;
				// next = request_cr;
			} else {
				next = request_error;
			}
			break;

		default:
			if (p->i < sizeof(p->request->verb) - 1) {
				p->request->verb[p->i++] = (char)c;
				next = request_data;
			} else {
				next = request_error;
			}
			break;
	}
	return next;
}
static enum request_state
handle_cr(const uint8_t c, request_parser* p)
{
	enum request_state next;
	// use p without doing anything with it
	(void)p;
	switch (c) {
		case '\n':
			p->i = 0;
			next = p->next_state;
			break;
		default:
			next = request_error;
			break;
	}
	return next;
}
