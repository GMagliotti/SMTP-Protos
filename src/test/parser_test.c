#include "parser.h"
#include <stdio.h>
#include <stdlib.h>

// definición de maquina

enum states
{
	S0,
	S1
};

enum event_type
{
	FOO,
	BAR,
};

static void
foo(struct parser_event* ret, const uint8_t c)
{
	ret->type = FOO;
	ret->n = 1;
	ret->data[0] = c;
}

static void
bar(struct parser_event* ret, const uint8_t c)
{
	ret->type = BAR;
	ret->n = 1;
	ret->data[0] = c;
}

static const struct parser_state_transition ST_S0[] = {
	{
	    .when = 'F',
	    .dest = S0,
	    .act1 = foo,
	},
	{
	    .when = 'f',
	    .dest = S0,
	    .act1 = foo,
	},
	{
	    .when = ANY,
	    .dest = S1,
	    .act1 = bar,
	},
};
static const struct parser_state_transition ST_S1[] = {
	{
	    .when = 'F',
	    .dest = S0,
	    .act1 = foo,
	},
	{
	    .when = 'f',
	    .dest = S0,
	    .act1 = foo,
	},
	{
	    .when = ANY,
	    .dest = S1,
	    .act1 = bar,
	},
};

static const struct parser_state_transition* states[] = {
	ST_S0,
	ST_S1,
};

#define N(x) (sizeof(x) / sizeof((x)[0]))

static const size_t states_n[] = {
	N(ST_S0),
	N(ST_S1),
};

static struct parser_definition definition = {
	.states_count = N(states),
	.states = states,
	.states_n = states_n,
	.start_state = S0,
};

//// TEST

void assert_eq(const unsigned type, const int c, const struct parser_event* e) {
    if (type != e->type || 1 != e->n || c != e->data[0]) {
        fprintf(stderr, "Assertion failed: expected (type=%u, n=1, data[0]=%d), but got (type=%u, n=%d, data[0]=%d)\n",
                type, c, e->type, e->n, e->data[0]);
        exit(EXIT_FAILURE);
    }
}

void test_basic()
{
	struct parser* parser = parser_init(parser_no_classes(), &definition);
	assert_eq(FOO, 'f', parser_feed(parser, 'f'));
	assert_eq(FOO, 'F', parser_feed(parser, 'F'));
	assert_eq(BAR, 'B', parser_feed(parser, 'B'));
	assert_eq(BAR, 'b', parser_feed(parser, 'b'));

	parser_destroy(parser);
}


int
main(void)
{
	test_basic();
    printf("All tests passed.\n");
	return EXIT_SUCCESS;
}
