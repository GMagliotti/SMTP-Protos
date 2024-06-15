/**
 * request.c -- parser del request de SOCKS5
 */
#include "request.h"

#include <arpa/inet.h>
#include <string.h>  // memset
// static void remaining_is_done(struct request_parser* p);
// static void remaining_set(struct request_parser* p, const int n);
// static void
// remaining_set(struct request_parser* p, const int n)
// {
// 	p->i = 0;
// 	p->n = n;
// }

// static void
// remaining_is_done(struct request_parser* p)
// {
// 	return p->i >= p->n;
// }

//////////////////////////////////////////////////////////////////////////////

static enum request_state
verb(const uint8_t c, struct request_parser* p)
{
	// TODO
	enum request_state next;
	switch (c) {
		case '\r':
			next = request_cr;
			break;
		case ' ':
			next = request_sep_arg1;
			break;
		default:
			next = request_verb;  // next = request_error; ?
			break;
	}
	if (next == request_verb) {
		if (p->i < sizeof(p->request->verb)) {  // checking for buffer overflow
			p->request->verb[p->i++] = c;       // should it be sizeof - 1 instead?
		}
	} else {
		p->request->verb[p->i] = '\0';
	}
	return next;
}

extern void
request_parser_init(struct request_parser* p)
{
	p->state = request_verb;
	memset(p->request, 0, sizeof(*(p->request)));
}

extern enum request_state
request_parser_feed(struct request_parser* p, const uint8_t c)
{
	enum request_state next;

	switch (p->state) {
		case request_verb:
			next = verb(c, p);
			break;
		case request_sep_arg1:
			// next = cmd(c, p);
			break;
		case request_arg1:
			// next = rsv(c, p);
			break;
		case request_cr:
			switch (c) {
				case '\n':
					next = request_done;
					break;

				default:
					next = request_verb;
					break;
			}
			break;

		case request_done:
		case request_error:
			next = p->state;
			break;
		default:
			next = request_error;
			break;
	}

	return p->state = next;
}

extern bool
request_is_done(const enum request_state st, bool* errored)
{
	if (st >= request_error && errored != 0) {
		*errored = true;
	}
	return st >= request_done;
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

extern void
request_close(struct request_parser* p)
{
	(void)p;
	// nada que hacer
}
