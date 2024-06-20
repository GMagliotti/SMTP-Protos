#include "request.h"

#include <arpa/inet.h>
#include <string.h>
#include <strings.h>

typedef request_state (*state_handler)(const uint8_t c, request_parser* p);

// Estructura para mensajes de estado y manejadores
typedef struct {
    state_handler handler;
    const char *success_msg;
    const char *error_msg;
} state_info;

static request_state handle_helo(const uint8_t c, request_parser* p);
static request_state handle_arg(const uint8_t c, request_parser* p);
static request_state handle_from(const uint8_t c, request_parser* p);
static request_state handle_to(const uint8_t c, request_parser* p);
static request_state handle_mail(const uint8_t c, request_parser* p);
static request_state handle_body(const uint8_t c, request_parser* p);
static request_state handle_data(const uint8_t c, request_parser* p);
static request_state handle_cr(const uint8_t c, request_parser* p);

static const state_info state_table[] = {
    [request_helo] = {handle_helo, "250 HELO OK\r\n", "500 HELO Error\r\n"},
    [request_arg] = {handle_arg, "250 ARG OK\r\n", "500 ARG Error\r\n"},
    [request_from] = {handle_from, "250 MAIL FROM OK\r\n", "500 MAIL FROM Error\r\n"},
    [request_to] = {handle_to, "250 RCPT TO OK\r\n", "500 RCPT TO Error\r\n"},
    [request_mail_from] = {handle_mail, "250 MAIL FROM Address OK\r\n", "500 MAIL FROM Address Error\r\n"},
    [request_mail_to] = {handle_mail, "250 RCPT TO Address OK\r\n", "500 RCPT TO Address Error\r\n"},
    [request_data] = {handle_data, "354 Start mail input; end with <CRLF>.<CRLF>\r\n", "500 DATA Error\r\n"},
    [request_body] = {handle_body, "250 DATA Body OK\r\n", "500 DATA Body Error\r\n"},
    [request_cr] = {handle_cr, "250 CR OK\r\n", "500 CR Error\r\n"},
    [request_done] = {NULL, "221 Bye\r\n", "500 Error\r\n"}
};
