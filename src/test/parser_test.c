#include "parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

// Definición de estados
enum states {
    S0,
    S1
};

enum event_type {
    FOO,
    BAR,
};

// Eventos de parser
struct parser_event {
    enum event_type type;
    int n;
    uint8_t data[1];
};

// Acciones para los eventos
static void foo(struct selector_key* key, uint8_t c) {
    struct parser_event* event = (struct parser_event*)key->data;
    event->type = FOO;
    event->n = 1;
    event->data[0] = c;
}

static void bar(struct selector_key* key, uint8_t c) {
    struct parser_event* event = (struct parser_event*)key->data;
    event->type = BAR;
    event->n = 1;
    event->data[0] = c;
}

// Definición de transiciones
static parser_transition ST_S0[3];
static parser_transition ST_S1[3];

void initialize_transitions() {
    uint8_t ST_S0_0_accepted_chars[] = {'F', '\0'};
    add_accepted_chars_to_transition(&ST_S0[0], ST_S0_0_accepted_chars);
    ST_S0[0].from_state = S0;
    ST_S0[0].to_state = S0;

    uint8_t ST_S0_1_accepted_chars[] = {'f', '\0'};
    add_accepted_chars_to_transition(&ST_S0[1], ST_S0_1_accepted_chars);
    ST_S0[1].from_state = S0;
    ST_S0[1].to_state = S0;

    uint8_t ST_S0_2_accepted_chars[] = {'@', '\0'};
    add_rejected_chars_to_transition(&ST_S0[2], ST_S0_2_accepted_chars);
    ST_S0[2].from_state = S0;
    ST_S0[2].to_state = S1;

    uint8_t ST_S1_0_accepted_chars[] = {'F', '\0'};
    add_accepted_chars_to_transition(&ST_S1[0], ST_S1_0_accepted_chars);
    ST_S1[0].from_state = S1;
    ST_S1[0].to_state = S0;

    uint8_t ST_S1_1_accepted_chars[] = {'f', '\0'};
    add_accepted_chars_to_transition(&ST_S1[1], ST_S1_1_accepted_chars);
    ST_S1[1].from_state = S1;
    ST_S1[1].to_state = S0;

    uint8_t ST_S1_2_accepted_chars[] = {'@', '\0'};
    add_rejected_chars_to_transition(&ST_S1[2], ST_S1_2_accepted_chars);
    ST_S1[2].from_state = S1;
    ST_S1[2].to_state = S1;
}

#define N(x) (sizeof(x) / sizeof((x)[0]))

static parser_transition* states[] = {
    ST_S0,
    ST_S1,
};

static size_t transitions_per_state[] = {
    N(ST_S0),
    N(ST_S1),
};

static parser_state states_array[] = {
    {.id = S0, .on_arrival = foo, .on_departure = bar, .is_final = false},
    {.id = S1, .on_arrival = bar, .on_departure = foo, .is_final = false},
};

static parser_state error_state = {.id = -1, .on_arrival = NULL, .on_departure = NULL, .is_final = true};

static parser_definition definition = {
    .states_count = N(states),
    .states = states_array,
    .initial_state = &states_array[S0],
    .error_state = &error_state,
    .transitions = states,
    .transitions_per_state = transitions_per_state,
};

// Funciones de prueba

void assert_eq(const unsigned type, const int c, const struct parser_event* e) {
    if (type != e->type || 1 != e->n || c != e->data[0]) {
        fprintf(stderr, "Assertion failed: expected (type=%u, n=1, data[0]=%d), but got (type=%u, n=%d, data[0]=%d)\n",
                type, c, e->type, e->n, e->data[0]);
        exit(EXIT_FAILURE);
    }
}

void test_basic() {
    initialize_transitions();

    struct parser_event event;
    struct selector_key key = {.data = &event};
    unsigned current_state = S0;

    current_state = parser_feed(&key, &definition, current_state, 'f');
    assert_eq(FOO, 'f', &event);

    current_state = parser_feed(&key, &definition, current_state, 'F');
    assert_eq(FOO, 'F', &event);

    current_state = parser_feed(&key, &definition, current_state, 'B');
    assert_eq(BAR, 'B', &event);

    current_state = parser_feed(&key, &definition, current_state, 'b');
    assert_eq(BAR, 'b', &event);
}

int main(void) {
    test_basic();
    printf("All tests passed.\n");
    return EXIT_SUCCESS;
}
