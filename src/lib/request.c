/**
 * request.c -- parser del request de SOCKS5
 */
#include "request.h"

#include <arpa/inet.h>
#include <string.h>
#include <strings.h>

static enum request_state mail(const uint8_t c, struct request_parser* p);
static enum request_state verb(const uint8_t c, struct request_parser* p);

static enum request_state
helo(const uint8_t c, struct request_parser* p)
{
	enum request_state next;
	switch (c) {
		case '\r':
			next = request_cr;
			break;
		default:
			if (p->i < sizeof(p->request->arg) - 1) {
				p->request->arg[p->i++] = (char)c;
				next = request_helo;
			} else {
				next = request_error;
			}
			break;
	}
	return next;
}

static enum request_state
verb(const uint8_t c, struct request_parser* p)
{
	enum request_state next;
	switch (c) {
		case ':':
			if (strcasecmp(p->request->verb, "MAIL FROM") == 0 || strcasecmp(p->request->verb, "RCPT TO") == 0) {
				p->i = 0;
				next = request_mail;
			} else {
				next = request_error;
			}
			break;

		case '\r':
			if (strcasecmp(p->request->verb, "DATA") == 0) {
				p->i = 0;
				// we enter in data mode
				// we need to write the email to an output file.

				next = request_data;
			} else {
				next = request_error;
			}
			break;

		case ' ':
			p->request->verb[p->i] = '\0';
			if (strcasecmp(p->request->verb, "EHLO") == 0 || strcasecmp(p->request->verb, "HELO") == 0) {
				p->i = 0;
				next = request_helo;
				break;
			}
			// fall through
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
static enum request_state
mail(const uint8_t c, struct request_parser* p)
{
	enum request_state next;
	// should be <mail>

	switch (c) {
		case '<':
			if (p->i == 1) {  // p->i == 1 because we read char ' '. FIXME
				next = request_mail;
			} else {
				next = request_error;
			}
			break;
		case '>':
			if (p->i > 0) {
				next = request_mail;
				p->request->arg[p->i] = '\0';
			} else {
				next = request_error;
			}
			break;
		case '\r':
			next = request_cr;
			break;
		case '\n':
			next = request_error;
			break;
		default:
			if (p->i < sizeof(p->request->arg) - 1) {
				p->request->arg[p->i++] = (char)c;

				next = request_mail;
			} else {
				next = request_error;
			}
			break;
	}
	return next;
}
static enum request_state
data_body(const uint8_t c, struct request_parser* p)
{
	enum request_state next;

	if (c == '\r') {
		if (p->i > 1 && p->request->data[p->i - 1] == '.' && p->request->data[p->i - 2] == '\n') {
			p->request->data[p->i - 2] = '\0';
			next = request_done;
		} else {
			next = request_data_body;
		}
	} else {
		if (p->i < sizeof(p->request->data) - 1) {
			p->request->data[p->i++] = (char)c;
			next = request_data_body;
		} else {
			next = request_error;
		}
	}
	return next;
}
extern void
request_parser_init(struct request_parser* p)
{
	p->state = request_verb;
	p->i = 0;
}

extern enum request_state
request_parser_feed(struct request_parser* p, const uint8_t c)
{
	enum request_state next;

	switch (p->state) {
		case request_verb:
			next = verb(c, p);
			break;
		case request_mail:
			next = mail(c, p);
			break;
		case request_data:
			if (c == '\n') {
				p->i = 0;
				next = request_data_body;
			} else {
				next = request_error;
			}
			break;
		case request_data_body:
			next = data_body(c, p);

			break;
		case request_helo:
			next = helo(c, p);
			break;
		case request_cr:
			switch (c) {
				case '\n':
					next = request_done;
					break;
				default:
					next = request_error;
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

void
request_close(struct request_parser* p)
{
	memset(p->request, 0, sizeof(*(p->request)));
	p->state = request_verb;
	p->i = 0;
}