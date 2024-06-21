#include "monitor.h"

#include "selector.h"

#include <arpa/inet.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>  // this library is for the Network Order functions

// the monitor protocol is a simple protocol that allows the client to send a message to the server
#define SIGNATURE 0xfffe
#define VERSION   0x00
#define TOKEN     0xffe91a2b3c4d5e6f

enum commands
{
	CMD_HIST_C = 0x00,
	CMD_CONC_C,
	CMD_BYTES_T,
	CMD_TRANS_S,
	CMD_TRANS_ON,
	CMD_TRANS_OFF
};

enum status
{
	S_SUCCESS = 0x00,
	S_INV_VERS,
	S_AUTH_FAIL,
	S_INV_CMD,
	S_INV_REQ_LEN,
	S_UNEXPECTED,
	S_SIGNATURE_ERR
};

void monitor_done(selector_key* key);

unsigned int m_req_read_handler(struct selector_key* key);
unsigned int m_req_write_handler(struct selector_key* key);
static void read_handler(struct selector_key* key);
static void write_handler(struct selector_key* key);
static void close_handler(struct selector_key* key);

static const struct state_definition monitor_states[] = {
	{
	    .state = M_REQ_READ,
	    .on_arrival = NULL,
	    .on_departure = NULL,
	    .on_read_ready = m_req_read_handler,
	},
	{
	    .state = M_REQ_WRITE,
	    .on_arrival = NULL,
	    .on_departure = NULL,
	    .on_write_ready = m_req_write_handler,
	},
	{
	    .state = ERROR,
	    .on_arrival = NULL,
	},
	{
	    .state = DONE,
	},
};

unsigned int
m_req_read_handler(struct selector_key* key)
{
	while (1)
		;
	monitor_data* data = ATTACHMENT(key);
	ssize_t n = recv(key->fd, data->raw_buff_read, BUFFER_SIZE, 0);
	if (n > 0) {
		buffer_write(&data->read_buffer, n);
		return M_REQ_WRITE;
	} else {
		return ERROR;
	}
}

unsigned int
m_req_write_handler(struct selector_key* key)
{
	monitor_data* data = ATTACHMENT(key);
	data = data;
	while (1)
		;
}

static void
read_handler(struct selector_key* key)
{
	monitor_data* data = ATTACHMENT(key);
	const enum monitor_states st = stm_handler_read(&data->stm, key);
	if (ERROR == st || DONE == st) {
		monitor_done(key);
	}
}
static void
write_handler(struct selector_key* key)
{
	monitor_data* data = ATTACHMENT(key);
	const enum monitor_states st = stm_handler_write(&data->stm, key);
	if (ERROR == st || DONE == st) {
		monitor_done(key);
	}
}
static void
close_handler(struct selector_key* key)
{
	stm_handler_close(&ATTACHMENT(key)->stm, key);
	monitor_done(key);
}

static fd_handler monitor_handler = {
	.handle_read = read_handler,
	.handle_write = write_handler,
	.handle_close = close_handler,
};

void
monitor_done(selector_key* key)
{
	monitor_data* data = ATTACHMENT(key);
	free(data);
}

int
is_valid_command(uint8_t command)
{
	switch (command) {
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x03:
		case 0x04:
		case 0x05:
			return 1;
		default:
			return -1;
	}
}

ssize_t
send_response(int sockfd, const void* buf, size_t len, int flags, const struct sockaddr* dest_addr, socklen_t addrlen)
{
	ssize_t sent_bytes = sendto(sockfd, buf, len, flags, dest_addr, addrlen);
	if (sent_bytes < 0) {
		perror("sendto");
		// Handle error
	}
	return sent_bytes;
}

void
process_valid_command(uint8_t command, uint8_t* response, uint8_t* status)
{
	*status = S_SUCCESS;
	uint16_t val;
	switch (command) {
		case 0x00:
			val = 10503;
			val = htons(val);
			response[6] = (val >> 8) & 0xff;
			response[7] = val & 0xff;
			break;
		case 0x01:
		case 0x02:
			response[6] = 0x29;  // hardcoded
			response[7] = 0x07;
			response[8] = 0x00;
			response[9] = 0x00;
			response[10] = 0x00;
			response[11] = 0x00;
			response[12] = 0x00;
			response[13] = 0x00;
			break;
		case 0x03:
			response[6] = 0x03;
			break;
		case 0x04:
			response[6] = 0x04;
			break;
		case 0x05:
			response[6] = 0x05;
			break;
		default:
			break;
	}
}

int
parse_monitor_message(const uint8_t* buffer,
                      size_t n,
                      uint8_t* data_command,
                      uint16_t* data_request_id,
                      uint8_t* response,
                      uint8_t* status)
{
	// we check if the message is a monitor message.
	// a monitor message is a message that has the following format:
	// 2 bytes for the signature
	// 1 byte for the version
	// 2 bytes for the request id
	// 8 bytes for the token
	// 1 byte for the command

	if (n < 14) {
		return -1;
	}

	uint16_t signature = buffer[0] << 8 | buffer[1];
	signature = ntohs(signature);

	uint8_t version = buffer[2];

	uint16_t request_id = buffer[3] << 8 | buffer[4];
	uint16_t request_id_from_net = ntohs(request_id);

	uint8_t token_arr[8];
	for (int i = 0; i < 8; i++) {
		token_arr[i] = buffer[5 + i];
	}

	uint64_t token = 0;
	for (int i = 0; i < 8; i++) {
		token = token << 8 | token_arr[i];
	}

	uint8_t command = buffer[13];

	if (signature != SIGNATURE) {
		return -1;
	}
	if (version != VERSION) {
		*status = S_INV_VERS;
		return -1;
	}
	if (token != TOKEN) {
		*status = S_AUTH_FAIL;
		return -1;
	}
	if (is_valid_command(command) < 0) {
		*status = S_INV_CMD;
		return -1;
	}
	if (n != 14) {
		*status = S_INV_REQ_LEN;
		return -1;
	}
	*status = S_SUCCESS;

	// we have a valid message

	// we write into the response buffer the response message

	// 2 bytes for the signature
	signature = htons((uint16_t)SIGNATURE);
	response[0] = (signature >> 8) & 0xff;
	response[1] = signature & 0xff;

	// 1 byte for the version
	response[2] = VERSION;

	// 2 bytes for the request id
	response[3] = buffer[3];
	response[4] = buffer[4];

	// 1 byte for the status
	response[5] = S_SUCCESS;

	*data_command = command;
	*data_request_id = request_id_from_net;

	return 1;
}

void
handle_udp_packet(struct selector_key* key)
{
	monitor_data* data = calloc(1, sizeof(*data));
	if (data == NULL) {
		return;
	}
	monitor_handler = monitor_handler;

	data->client_addr_len = sizeof(data->client_addr);
	data->stm.initial = M_REQ_WRITE;
	data->stm.max_state = ERROR;
	data->stm.states = monitor_states;

	buffer_init(&data->write_buffer, BUFFER_SIZE, data->raw_buff_write);
	buffer_init(&data->read_buffer, BUFFER_SIZE, data->raw_buff_read);

	int bytes_read = recvfrom(
	    key->fd, data->raw_buff_read, BUFFER_SIZE, 0, (struct sockaddr*)&data->client_addr, &data->client_addr_len);

	if (bytes_read < 0) {
		perror("recvfrom");
		monitor_done(key);
		return;
	}

	uint8_t status = S_SIGNATURE_ERR;

	if (parse_monitor_message(
	        data->raw_buff_read, bytes_read, &data->command, &data->request_id, data->raw_buff_write, &status) > 0) {
		// valid command, we send the response

		// should we do this in a separate thread?
		process_valid_command(data->command, data->raw_buff_write, &status);

		send_response(
		    key->fd, data->raw_buff_write, BUFFER_SIZE, 0, (struct sockaddr*)&data->client_addr, data->client_addr_len);

	} else {
		if (status < S_SIGNATURE_ERR) {
			// we send the error response
			data->raw_buff_write[5] = status;
		}
	}

	// we free the data
	monitor_done(key);
}
