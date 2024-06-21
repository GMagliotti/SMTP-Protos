#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

// Include your parser header
#include "request.h"

// Define a simple buffer structure
bool buffer_can_read(const buffer *b);
uint8_t buffer_read(buffer *b);
void buffer_write(buffer *b, const uint8_t *data, size_t len);
void buffer_free(buffer *b);



// Buffer functions
buffer* buffer_init(size_t capacity) {
    buffer *b = malloc(sizeof(buffer));
    b->data = malloc(capacity);
    b->read_pos = 0;
    b->write_pos = 0;
    b->capacity = capacity;
    return b;
}

void buffer_write(buffer *b, const uint8_t *data, size_t len) {
    if (b->write_pos + len > b->capacity) {
        fprintf(stderr, "Buffer overflow\n");
        exit(EXIT_FAILURE);
    }
    memcpy(b->data + b->write_pos, data, len);
    b->write_pos += len;
}

bool buffer_can_read(const buffer *b) {
    return b->read_pos < b->write_pos;
}

uint8_t buffer_read(buffer *b) {
    if (!buffer_can_read(b)) {
        fprintf(stderr, "Buffer underflow\n");
        exit(EXIT_FAILURE);
    }
    return b->data[b->read_pos++];
}

void buffer_free(buffer *b) {
    free(b->data);
    free(b);
}

// Test function
void test_parser(const char *input) {
    printf("Testing input: %s\n", input);

    struct request req;
    struct request_parser parser;
    parser.request = &req; // Ensure request pointer is set before initializing the parser
    request_parser_init(&parser);

    buffer *b = buffer_init(strlen(input) + 1);
    buffer_write(b, (const uint8_t *)input, strlen(input));

    bool errored = false;
    enum request_state state = request_consume(b, &parser, &errored);

    if (errored) {
        printf("Error occurred while parsing\n");
    } else if (state == request_done) {
        printf("Parsed successfully:\n");
        printf("Verb: %s\n", req.verb);
        printf("MAIL FROM: %s\n", req.mail_from);
        printf("RCPT TO: %s\n", req.rcpt_to);
        printf("DATA: %s\n", req.data);
    } else {
        printf("Parsing not completed\n");
    }

    buffer_free(b);
}

int main() {
    // Test cases
    const char *test_cases[] = {
        "MAIL FROM: <mperezdegracia@itba.edu.ar>\r\n",
        "RCPT TO: <someone@example.com>\r\n",
        "DATA\r\nThis is the email body.\r\n.\r\n",
        "mail from: <lowercase@example.com>\r\n",
        "rCPT To: <MixedCase@example.com>\r\n",
        "DaTa\r\nBody with mixed case command.\r\n.\r\n",
    };

    for (unsigned int i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
        test_parser(test_cases[i]);
    }

    return 0;
}