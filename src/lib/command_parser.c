
#include "parser.h"
#include "selector.h"
#include "command_parser.h"

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

static parser_definition * command_parser;
static parser_transition ** transitions_list;

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

void init_command_parsing(smtp_command * smtp_command){
    //El cliente de este parser de comandos se encarga de asignar la memoria para la estructura del comando
    if (smtp_command == NULL)
    {
        //TODO: loggear error Trying to initialize NULL smtp_command
        return;
    }

    smtp_command->parser = &command_parser;
    //TODO: setear memoria de los vectores de comandos
}

void parser_configuration(){
    //Tenemos que asignar memoria para algunos campos del parser_definition
    command_parser->states = &state_details;
    command_parser->states_count = STATES_QTY;
    command_parser->initial_state = &state_details[S0];
    command_parser->error_state = &state_details[SERROR];

    transitions_list = malloc(STATES_QTY * sizeof(parser_transition *));
    transitions_list[S0] = S0_transitions;
    transitions_list[S1] = S1_transitions;
    transitions_list[S2] = S2_transitions;
    transitions_list[S3] = NULL;
    transitions_list[SERROR] = NULL;

    command_parser->transitions = transitions_list;
    command_parser->transitions_per_state = transitions_per_state;

    //Finalmente configuramos los vectores de booleanos
    add_rejected_chars_to_transition(&S0_transitions[0], (uint8_t *)" \r\n\0");
    add_accepted_chars_to_transition(&S0_transitions[1], (uint8_t *)" \0");
    add_accepted_chars_to_transition(&S0_transitions[2], (uint8_t *)"\r\0");

    add_rejected_chars_to_transition(&S1_transitions[0], (uint8_t *)"\r\n\0");
    add_accepted_chars_to_transition(&S1_transitions[1], (uint8_t *)"\r");

    add_accepted_chars_to_transition(&S2_transitions[0], (uint8_t *)"\n");
}

void finish_command_parsing(){
    free(transitions_list);
}