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

#define ASCII_CHARS 127

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
	bool accepted_chars[ASCII_CHARS];
} parser_transition;

/** Declaracion completa de una máquina de estados */
typedef struct parser_definition
{
	size_t states_count;
	parser_state * states;
	parser_state * initial_state;
	parser_state * error_state;
} parser_definition;


/**
 * El usuario alimenta el parser con un caracter, y el parser retorna el id del estado al que debera redirigirse. 
 * Los eventos son reusado entre llamadas por lo que si se desea
 */
int * parser_feed(struct selector_key * key, parser_definition * parser, unsigned current_state_id, uint8_t c);

/**
 * Retorna si el estado es final o no
 */
bool is_final(parser_state * state);

#endif
