
#include "parser.h"
#include "selector.h"

/** Parser de comandos o "linea" ingresada por el usuario
 *  Estados: S0, S1, S2, S3, SERROR. Siendo S2 y SERROR estados finales
 *  Transiciones:
 *  
 *  S0 -> ~{' ', \r, \n} -> S0
 *  S0 -> {' '} -> S1
 *  S0 -> {\r} -> S2
 * 
 *  S1 -> ~{\r, \n} -> S1
 *  S1 -> {\r} -> S2
 * 
 *  S2 -> {\n} -> S3
 * 
 * **/


static void reading_command(struct selector_key * key, u_int8_t c);
static void reading_args(struct selector_key * key, u_int8_t c);
static void error_state_arrival(struct selector_key * key, uint8_t c);
static void final_state_arrival(struct selector_key * key, u_int8_t c);

static parser_definition command_parser;

enum states {
    S0 = 0,
    S1,
    S2,
    S3,
    SERROR
};

static parser_state state_details[] = {
    {.id = S0, .is_final = false, .on_departure = reading_command},
    {.id = S1, .is_final = false, .on_departure = reading_args},
    {.id = S2, .is_final = false},
    {.id = S3, .is_final = true, .on_arrival = final_state_arrival},
    {.id = SERROR, .is_final = true, .on_arrival = error_state_arrival}
} ;

static size_t transitions_per_state[] = {
    3, 2, 1, 0, 0
    // S0, S1, ...
};

static parser_transition S0_transitions[] = {
    {.from_state = S0, .to_state = S0},
    {.from_state = S0, .to_state = S1},
    {.from_state = S0, .to_state = S2}
};

static parser_transition S1_transitions[] = {
    {.from_state = S1, .to_state = S1},
    {.from_state = S1, .to_state = S2}
};

static parser_transition S2_transitions[] = {
    {.from_state = S2, .to_state = S3}
};

void init_command_parsing(){

}

void parser_configuration(){

}

void finish_command_parsing(){

}