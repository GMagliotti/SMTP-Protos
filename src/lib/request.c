/**
 * request.c -- parser del request de SMTP
 */
#include "request.h"

#include <arpa/inet.h>
#include <string.h>
#include <strings.h>

#define DATA "DATA"

enum request_state verb(const uint8_t c, struct request_parser* p);
enum request_state arg(const uint8_t c, struct request_parser* p);
enum request_state body(const uint8_t c, struct request_parser* p);

extern void
request_parser_init(struct request_parser* p)
{
	p->i = 0;
	p->state = request_verb;
	memset(p->request, 0, sizeof(*(p->request)));
}

extern void
request_parser_data_init(struct request_parser* p)
{
	p->i = 0;
	p->state = request_body;
	memset(p->request, 0, sizeof(*(p->request)));
}

extern enum request_state
request_consume(buffer* b, struct request_parser* p, bool* errored)
{
	enum request_state st = p->state;

	while (buffer_can_read(b)) {
		const uint8_t c = buffer_read(b);
		st = request_parser_feed(p, c);
		if (request_is_done(st, errored)) {
			break;
		}
	}
	return st;
}

extern enum request_state
request_consume_data(buffer* b, struct request_parser* p, ssize_t* data_size, bool* errored)
{
	enum request_state st = p->state;

	while (buffer_can_read(b)) {
		const uint8_t c = buffer_read(b);
		*data_size += 1;
		st = request_parser_data_feed(p, c);
		if (request_is_done(st, errored)) {
			break;
		}
	}
	return st;
}

extern enum request_state
request_parser_data_feed(struct request_parser* p, const uint8_t c)
{
	enum request_state next;

	switch (p->state) {
		case request_body:
			next = body(c, p);
			break;

		case request_cr:

			if (c == '\n') {
				next = request_done;
			} else {
				next = request_error;
			}
			break;
		case request_error:
			next = p->state;
			break;
		default:
			next = request_error;
			break;
	}
	p->state = next;
	return p->state;
}
enum request_state
body(const uint8_t c, struct request_parser* p)
{
	// TODO
	enum request_state next;
	switch (c) {
		case '\r':
			if (p->i > 1 && p->request->data[p->i - 1] == '.' && p->request->data[p->i - 2] == '\n') {
				p->request->data[p->i - 3] = '\0';
				return request_cr;
			}
			break;
		default:
			break;
	}

	if (p->i < sizeof(p->request->data) - 1) {
		p->request->data[p->i++] = (char)c;
		next = request_body;
	}
	return next;
}

extern enum request_state
request_parser_feed(struct request_parser* p, const uint8_t c)
{
	enum request_state next;

	switch (p->state) {
		case request_verb:
			next = verb(c, p);  // rename
			break;
		case request_arg:
			next = arg(c, p);
			break;

		case request_cr:

			if (c == '\n') {
				next = request_done;
			} else {
				next = request_error;
			}
			break;
		case request_error:
			next = p->state;
			break;
		default:
			next = request_error;
			break;
	}
	p->state = next;
	return p->state;
}

enum request_state
verb(const uint8_t c, struct request_parser* p)
{
	// TODO
	enum request_state next;
	switch (c) {
		case ' ':
			p->request->arg[p->i] = '\0';
			p->i = 0;
			next = request_arg;

			break;
		case '\r':

			if (strncasecmp(p->request->verb, DATA, strlen(DATA)) == 0) {
				p->request->arg[p->i] = '\0';
				p->i = 0;

				next = request_cr;
			} else {
				next = request_error;
			}
			break;
		default:
			if (p->i < sizeof(p->request->verb) - 1) {
				p->request->verb[p->i++] = (char)c;
				next = request_verb;
			} else {
				next = request_error;
			}
			break;
	}
	return next;
}

enum request_state
arg(const uint8_t c, struct request_parser* p)
{
	// TODO
	enum request_state next;

	switch (c) {
		case '\r':
			p->request->arg[p->i] = '\0';
			return request_cr;
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

extern bool
request_is_done(const enum request_state st, bool* errored)
{
	if (st >= request_error && errored != 0) {
		*errored = true;
	}
	return st >= request_done;
}

extern void
request_close(struct request_parser* p)
{
	(void)p;
	// nada que hacer
}