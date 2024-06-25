#include "request.h"
#include <arpa/inet.h>
#include <string.h>
#include <strings.h>
#include "logger.h"



enum request_state body(const uint8_t c, struct request_parser* p);

extern void
request_parser_data_init(struct request_parser* p)
{
	p->i = 0;
	p->state = request_body;
	memset(p->request, 0, sizeof(*(p->request)));
}

extern enum request_state
request_consume_data(buffer* b, struct request_parser* p, bool* errored)
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

/**
 * @brief Write up to len bytes to the temp file the contents of a buffer.
 * This functions assumes that the file descriptor is non-blocking.
 * @param fd File descriptor to write to
 * @param buf Buffer to write
 * @param len Length of the buffer
 */
// static int 
// write_partial(int fd, const char* buf, size_t len)
// {	
// 	logf(LOG_DEBUG, "Writing %ld bytes to fd %d", len, fd);
// 	int old_fl = fcntl(fd, F_GETFL);
// 	fcntl(fd, F_SETFL, O_APPEND);
// 	int bytes_written = write(fd, buf, len);
// 	if (bytes_written == -1) {
// 		logf(LOG_ERROR, "Error writing to fd %d", fd);
// 		perror("write");
// 	}
// 	fcntl(fd, F_SETFL, old_fl & ~O_APPEND);
// 	return bytes_written;
// }

enum request_state
body(const uint8_t c, struct request_parser* p)
{
	// TODO
	enum request_state next;
	switch (c) {
		case '\r':
			if (p->i > 1 && p->request->data[p->i - 1] == '.' && p->request->data[p->i - 2] == '\n') {
				p->request->data[p->i - 3] = '\0';
				//write_partial(*p->output_fd, p->request->data, p->i - 1);
				return request_cr;
			}
			break;
		default:
			break;
	}

	if (p->i < sizeof(p->request->data) - 1) {
		p->request->data[p->i++] = (char)c;
		next = request_body;
	} else {
		//write_partial(*p->output_fd, p->request->data, sizeof(p->request->data));
		p->i = 0;
		p->request->data[p->i++] = (char)c;
		next = request_body;
	}
	return next;
}

// extern void
// request_parser_data_init(struct request_parser* p)
// {
// 	p->i = 0;
// 	p->state = request_body;
// 	memset(p->request, 0, sizeof(*(p->request)));
// }

// extern enum request_state
// request_consume_data(buffer* b, struct request_parser* p, bool* errored)
// {
// 	enum request_state st = p->state;

// 	while (buffer_can_read(b)) {
// 		const uint8_t c = buffer_read(b);
// 		st = request_parser_data_feed(p, c);
// 		if (request_is_done(st, errored)) {
// 			break;
// 		}
// 	}
// 	return st;
// }

// extern enum request_state
// request_parser_data_feed(struct request_parser* p, const uint8_t c)
// {
// 	enum request_state next;

// 	switch (p->state) {
// 		case request_body:
// 			next = body(c, p);
// 			break;

// 		case request_cr:

// 			if (c == '\n') {
// 				next = request_done;
// 			} else {
// 				next = request_error;
// 			}
// 			break;
// 		case request_error:
// 			next = p->state;
// 			break;
// 		default:
// 			next = request_error;
// 			break;
// 	}
// 	p->state = next;
// 	return p->state;
// }
// enum request_state
// body(const uint8_t c, struct request_parser* p)
// {
// 	// TODO
// 	enum request_state next;
// 	switch (c) {
// 		case '\r':
// 			if (p->i > 1 && p->request->data[p->i - 1] == '.' && p->request->data[p->i - 2] == '\n') {
// 				p->request->data[p->i - 3] = '\0';
// 				return request_cr;
// 			}
// 			break;
// 		default:
// 			break;
// 	}

// 	if (p->i < sizeof(p->request->data) - 1) {
// 		p->request->data[p->i++] = (char)c;
// 		next = request_body;
// 	}
// 	return next;
// }