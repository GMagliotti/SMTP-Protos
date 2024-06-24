#include "request.h"
#include <arpa/inet.h>
#include <string.h>
#include <strings.h>
enum request_state command(const uint8_t c, struct request_parser* p);
enum request_state command_arg(const uint8_t c, struct request_parser* p);


extern void
request_parser_admin_init(struct request_parser* p)
{
	p->i = 0;
	p->state = request_verb;
	memset(p->request, 0, sizeof(*(p->request)));
}

extern enum request_state
request_consume_admin(buffer* b, struct request_parser* p, bool* errored)
{
	enum request_state st = p->state;

	while (buffer_can_read(b)) {
		const uint8_t c = buffer_read(b);
		st = request_parser_data_feed(p, c);
		if (request_is_done(st, errored)) {
			break;
		}
	}
	return st;
}

extern enum request_state
request_parser_admin_feed(struct request_parser* p, const uint8_t c)
{
	enum request_state next;

	switch (p->state) {
		case request_verb:
			next = command(c, p);
			break;
        case request_arg:
            next = command_arg(c,p);
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
command(const uint8_t c, struct request_parser* p)
{
	// TODO
	enum request_state next;
	switch (c) {
		case ' ':
            p->request->verb[p->i] = '\0';
			return request_cr;
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


enum request_state
command_arg(const uint8_t c, struct request_parser* p)
{
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