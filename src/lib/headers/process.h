#ifndef PROCESS_H
#define PROCESS_H
#include "selector.h"
#include "states.h"

#include <arpa/inet.h>
#include <string.h>
#include <strings.h>

#define HELO_VERB   "HELO"
#define EHLO_VERB   "EHLO"
#define MAIL_VERB   "MAIL"
#define RCPT_VERB   "RCPT"
#define DATA_VERB   "DATA"
#define RSET_VERB   "RSET"
#define NOOP_VERB   "NOOP"
#define FROM_PREFIX "FROM:"
#define TO_PREFIX   "TO:"

typedef smtp_state (*process_handler)(struct selector_key* key, char* msg);

// Estructura para mensajes de estado y manejadores

smtp_state handle_helo(struct selector_key* key, char* msg);
smtp_state handle_from(struct selector_key* key, char* msg);
smtp_state handle_to(struct selector_key* key, char* msg);
smtp_state handle_body(struct selector_key* key, char* msg);
smtp_state handle_data(struct selector_key* key, char* msg);
bool handle_reset(struct selector_key* key, char* msg);
bool handle_noop(struct selector_key* key, char* msg);

// enum process_state {
// 	request_helo,
// 	request_from,
// 	request_to,
// 	request_data,
// 	request_body,
// 	request_done,
// 	request_error,
// };

#endif