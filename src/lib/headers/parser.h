#ifndef PARSER_H_00180a6350a1fbe79f133adf0a96eb6685c242b6
#define PARSER_H_00180a6350a1fbe79f133adf0a96eb6685c242b6

/**
 * parser.c -- pequeño motor para parsers/lexers.
 *
 * El usuario describe estados y transiciones.
 * Las transiciones contienen una condición, un estado destino y acciones.
 *
 * El usuario provee al parser con bytes y éste retona eventos que pueden
 * servir para delimitar tokens o accionar directamente.
 */
#include <stddef.h>
#include <stdint.h>
#include <selector.h>

/** Restamos todos los ASCII que no sean printeables exceptuando por ' ', '\r', '\n' **/
/**
 ! (33) -> 0 (33-32-1)
 " (34) -> 1 (34-32-1)
 ...
 ? (127) -> 94 (127-32-1)
 ' ' -> 95
 '\n'-> 96
 '\r'->97
 **/
#define PRINTABLE_ASCII_CHARS 127 - 32 + 3
#define CHARS_ARRAY_SHIFT 32
#define SPACE_INDEX 95
#define LF_INDEX 96
#define CR_INDEX 97

/** Funcion de transicion de un estado a otro, recibe todas las funciones de callback de handler y char procesado **/
typedef void (* parser_state_function) (struct selector_key * key, u_int8_t c);

/** Describe un estado por su id, funciones de llegada y salida, y un booleano que indica si es final o no **/
typedef struct parser_state
{
	unsigned id;
	parser_state_function on_arrival;
	parser_state_function on_departure;
	bool is_final;
} parser_state;

/** Describe la transicion de un estado a otro. Compuesta por origen, destino y caracteres aceptados **/
/** Los chars aceptados por la transicion estaran marcados en 1 **/
typedef struct parser_transition {
	unsigned from_state;
	unsigned to_state;
	bool accepted_chars[PRINTABLE_ASCII_CHARS];
} parser_transition;

/** Declaracion completa de una máquina de estados */
typedef struct parser_definition
{
	size_t states_count;
	parser_state * states;
	parser_state * initial_state;
	parser_state * error_state;
	/** [{transiciones estado 0, transiciones 1, ..., transiciones N}] **/
	parser_transition ** transitions;
	/** [#transiciones estado 0, #transiciones 1, ..., #transiciones N] **/
	size_t * transitions_per_state;
} parser_definition;


/**
 * El usuario alimenta el parser con un caracter, y el parser retorna el id del estado al que debera redirigirse. 
 */
int * parser_feed(struct selector_key * key, parser_definition * parser, unsigned current_state_id, uint8_t c);

/*
 * Se agregan a la transicion aquellos chars aceptados
*/
void add_accepted_chars_to_transition(parser_transition * transition, char * array_chars);

/**
 * Retorna si el estado es final o no
 */
bool is_final(parser_state * state);

#endif
