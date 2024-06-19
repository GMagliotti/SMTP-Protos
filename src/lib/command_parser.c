
#include "command_parser.h"

#include "parser.h"
#include "selector.h"
#include "smtp.h"

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

static void reading_command(struct selector_key* key, u_int8_t c);
static void reading_args(struct selector_key* key, u_int8_t c);
static void error_state_arrival(struct selector_key* key, uint8_t c);
static void final_state_arrival(struct selector_key* key, u_int8_t c);

static parser_definition command_parser;
static parser_transition* transitions_list[STATES_QTY];

static parser_state state_details[] = { { .id = S0, .is_final = false, .on_departure = reading_command },
	                                    { .id = S1, .is_final = false, .on_departure = reading_args },
	                                    { .id = S2, .is_final = false },
	                                    { .id = S3, .is_final = true, .on_arrival = final_state_arrival },
	                                    { .id = SERROR, .is_final = true, .on_arrival = error_state_arrival } };

static size_t transitions_per_state[] = {
	3,
	2,
	1,
	0,
	0
	// S0, S1, ...
};

static parser_transition S0_transitions[] = { { .from_state = S0, .to_state = S0 },
	                                          { .from_state = S0, .to_state = S1 },
	                                          { .from_state = S0, .to_state = S2 } };

static parser_transition S1_transitions[] = { { .from_state = S1, .to_state = S1 },
	                                          { .from_state = S1, .to_state = S2 } };

static parser_transition S2_transitions[] = { { .from_state = S2, .to_state = S3 } };

void
init_command_parsing(smtp_command* smtp_command)
{
	// El cliente de este parser de comandos se encarga de asignar la memoria para la estructura del comando
	if (smtp_command == NULL) {
		// TODO: loggear error Trying to initialize NULL smtp_command
		return;
	}

	smtp_command->parser = &command_parser;
	smtp_command->ended = false;
	smtp_command->error = false;
	smtp_command->current_state = S0;
	smtp_command->command_dim = 0;
	smtp_command->arg_dim = 0;

	memset(smtp_command->command, 0, MAX_COMMAND_LEN + 1);
	memset(smtp_command->arg, 0, MAX_LINE_LEN - MAX_COMMAND_LEN - 3 + 1);
}

void
parser_configuration()
{
	// Tenemos que asignar memoria para algunos campos del parser_definition
	command_parser.states = state_details;
	command_parser.states_count = STATES_QTY;
	command_parser.initial_state = &state_details[S0];
	command_parser.error_state = &state_details[SERROR];

	transitions_list[S0] = S0_transitions;
	transitions_list[S1] = S1_transitions;
	transitions_list[S2] = S2_transitions;
	transitions_list[S3] = NULL;
	transitions_list[SERROR] = NULL;

	command_parser.transitions = transitions_list;
	command_parser.transitions_per_state = transitions_per_state;

	// Finalmente configuramos los vectores de booleanos
	add_rejected_chars_to_transition(&S0_transitions[0], (uint8_t*)" \r\n\0");
	add_accepted_chars_to_transition(&S0_transitions[1], (uint8_t*)" \0");
	add_accepted_chars_to_transition(&S0_transitions[2], (uint8_t*)"\r\0");

	add_rejected_chars_to_transition(&S1_transitions[0], (uint8_t*)"\r\n\0");
	add_accepted_chars_to_transition(&S1_transitions[1], (uint8_t*)"\r");

	add_accepted_chars_to_transition(&S2_transitions[0], (uint8_t*)"\n");
}

int
parse_command(struct selector_key* key, smtp_command* smtp_command, struct buffer* buffer)
{
	// smtp_data * client_data = ATTACHMENT(key);
	int state = 0;
	// Tenemos que empezar a recorrer la maquina de estados
	// Cuando finalizamos? Cuando caemos en un estado de error o final o cuando no hay mas para leer
	// Para recorrer la maquina de estados usamos la referencia al parser dentro del smtp_command
	while (smtp_command->current_state != SERROR && buffer_can_read(buffer) && smtp_command->ended != true) {
		state = parser_feed(key, smtp_command->parser, smtp_command->current_state, buffer_read(buffer));
		smtp_command->current_state = state;
	}

	return state;
}

/** Estamos en S0 y entra un char, empezamos a procesar. SMTP NO ES case sensitive **/
static void
reading_command(struct selector_key* key, u_int8_t c)
{
	smtp_data* client_data = ATTACHMENT(key);
	smtp_command* smtp_command = &client_data->command_parser;

	if (smtp_command->command_dim >= MAX_COMMAND_LEN) {
		// No es comando valido
		// TODO: Aca hay que llamar al error handler o algo que indique que el comando es bochado
		// o capaz no se maneja aca, ni idea ajaj
		return;
	}
	if (c != '\n' && c != '\r')
	{
		smtp_command->command[smtp_command->command_dim] = c;
	}
	
	smtp_command->command_dim++;

	return;
}

/** Estamos en S1 y entra un char, empezamos a procesar.**/
static void
reading_args(struct selector_key* key, u_int8_t c)
{
	smtp_data* client_data = ATTACHMENT(key);
	smtp_command* smtp_command = &client_data->command_parser;

	if (smtp_command->arg_dim >= MAX_ARG_LEN) {
		// No es argumento valido
		// TODO: Aca hay que llamar al error handler o algo que indique que el comando es bochado
		// o capaz no se maneja aca, ni idea ajaj
		return;
	}
	if(c != '\n' && c != '\r'){
		smtp_command->arg[smtp_command->arg_dim] = c;
	}
	smtp_command->arg_dim++;

	return;
}

static void
final_state_arrival(struct selector_key* key, u_int8_t c)
{
	smtp_data* client_data = ATTACHMENT(key);
	smtp_command* smtp_command = &client_data->command_parser;
	smtp_command->ended = true;
}

static void
error_state_arrival(struct selector_key* key, uint8_t c)
{
	smtp_data* client_data = ATTACHMENT(key);
	smtp_command* smtp_command = &client_data->command_parser;
	smtp_command->ended = true;
	smtp_command->error = true;
}