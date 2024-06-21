/**
 * request.c -- parser del request de SOCKS5
 */
#include "request.h"

#include <arpa/inet.h>
#include <string.h>
#include <strings.h>

typedef request_state (*state_handler)(const uint8_t c, request_parser* p);

// Prototipos de funciones estáticas
static request_state handle_helo(const uint8_t c, request_parser* p);
static request_state handle_arg(const uint8_t c, request_parser* p);
static request_state handle_from(const uint8_t c, request_parser* p);
static request_state handle_to(const uint8_t c, request_parser* p);
static request_state handle_mail(const uint8_t c, request_parser* p);
static request_state handle_body(const uint8_t c, request_parser* p);
static request_state handle_data(const uint8_t c, request_parser* p);
static request_state handle_cr(const uint8_t c, request_parser* p);

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

extern request_state
request_parser_feed(request_parser* p, const uint8_t c)
{
	return state_handlers[p->state](c, p);
}

/*
extern request_state
request_parser_feed(request_parser* p, const uint8_t c)
{
    request_state next;

    switch (p->state) {
        case request_verb:
            next = verb(c, p);
            break;
        case request_mail:
            next = mail(c, p);
            break;
        case request_data:  // no se debería llegar nunca acá, pues se hace un cambio de estado de la stm del smtp.c
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

*/

extern request_state
request_consume(buffer* b, request_parser* p, bool* errored)
{
	request_state st = p->state;
	while (buffer_can_read(b)) {
		const uint8_t c = buffer_read(b);
		st = request_parser_feed(p, c);
		if (request_is_done(st, errored) || st == request_flush) {
			if (*errored) {
				p->i = 0;
			}
			break;
		}

		if (request_is_data(st)) {
			return st;
		}
	}
	return st;
}

extern bool
request_is_done(const request_state st, bool* errored)
{
	if (st == request_error) {
		*errored = true;
	}
	return st >= request_done;
}

extern bool
request_is_data(const request_state st)
{
	return st == request_data;
}

extern bool
request_is_data_body(const request_state st)
{
	return st == request_body;
}

extern bool
request_file_flush(request_state st, request_parser* p)
{
	// es necesario flushear si estamos en data_body y si el buffer está lleno
	// o bien si estamos en doneF
}

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
				next = request_arg;
				p->state = next;
			} else {
				next = request_error;
			}
			break;
		case '\r':
			next = request_error;
			break;
		default:
			if (p->i < sizeof(p->request->verb) - 1) {
				p->request->verb[p->i++] = (char)c;
				next = request_helo;
			} else {
				next = request_error;
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
			p->next_state = request_from;
			p->last_state = request_helo;
			p->state = request_cr;
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
				next = request_mail_from;
				p->state = next;
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
				next = request_error;
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
				next = request_mail_to;
				p->state = next;

			} else {
				next = request_error;
			}
			break;
		case '\r':
			next = request_error;
			break;
		default:
			if (p->i < sizeof(p->request->verb) - 1) {
				p->request->verb[p->i++] = (char)c;
				next = request_to;
			} else {
				next = request_error;
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
				return request_error;
			}
			next = p->state;
			break;
		case '>':
			if (p->i == 0) {
				return request_error;
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

			p->next_state = p->state + 1;
			p->last_state = p->state;
			p->state = request_cr;

			break;
		case '\n':
			next = request_error;
			break;
		default:
			if (p->i < sizeof(p->request->arg) - 1) {
				p->request->arg[p->i++] = (char)c;

				next = p->state;
			} else {
				next = request_error;
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
			if (p->i > 1 && p->request->data[p->i-1] == '.' && p->request->data[p->i - 2] == '\n') {
				p->request->data[p->i - 3] = '\0';
				p->next_state = request_done;
				p->last_state = request_body;
				next = request_cr;
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

				p->last_state = request_data;
				p->next_state = request_body;
				next = request_cr;
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

/*
static enum request_state mail(const uint8_t c, request_parser* p);
static enum request_state verb(const uint8_t c, request_parser* p);

static enum request_state
helo(const uint8_t c, request_parser* p)
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
verb(const uint8_t c, request_parser* p)
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
            } else if (strcasecmp(p->request->verb, "QUIT") == 0) {
                next = request_done;
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
mail(const uint8_t c, request_parser* p)
{
    enum request_state next;
    // should be <mail>

    switch (c) {
        case '<':
            if (p->i == 0) {
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
data_body(const uint8_t c, request_parser* p)
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
            next = request_flush;
        }
    }
    return next;
}



*/
/*
extern enum request_state
request_data_consume(buffer* b, request_parser* p, bool* errored)
{
    enum request_state st = p->state;
    while (buffer_can_read(b)) {
        const uint8_t c = buffer_read(b);
        st = request_parser_feed(p, c);
        if (request_is_done(st, errored) || st == request_flush) {
            break;
        }
    }
    return st;
}
*/
