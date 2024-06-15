#include "parser.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

int parser_feed(struct selector_key * key, parser_definition * parser, unsigned current_state_id, uint8_t c){
	if (parser == NULL){
		//TODO: loggear error NULL parser
		return -1;
	}

	size_t char_index;

	//Obtenemos la posicion del char en el array de booleanos (caracteres permitidos por la transicion)
	if(c < 32 || c > 127) {
		if (c == '\r') {
			char_index = CR_INDEX;
		} else if (c == '\n') {
			char_index = LF_INDEX;
		} else {
			//TODO: loggear error invalid character
			return -1;
		}
	} else {
		if (c == ' ') {
			char_index = SPACE_INDEX;
		} else {
			char_index = c - CHARS_ARRAY_SHIFT - 1;
		}
	}

	//Buscamos si hay alguna transicion que acepte el caracter, para eso primero obtenemos todas las transiciones del estado actual
	parser_transition * transitions = parser->transitions[current_state_id];	
	size_t transitions_amount = parser->transitions_per_state[current_state_id];
	for (size_t i = 0; i < transitions_amount; i++)
	{
		//Char aceptado
		if(transitions[i].accepted_chars[char_index]){
			//Vemos si hay un metodo de salida de estado
			parser_state * from_state = &parser->states[transitions[i].from_state];
			if(from_state->on_departure != NULL) {
				from_state->on_departure(key, c);
			}
			//Vemos si hay un metodo de entrada para el nuevo estado
			parser_state * to_state = &parser->states[transitions[i].to_state];
			if(to_state->on_arrival != NULL) {
				to_state->on_arrival(key, c);
			}

			return to_state->id;
		}

	}
	
	//No se encontro ninguna transicion para el char, por lo tanto no fue aceptado
	//Por lo tanto, una opcion ahora es ir al estado de error
	if(parser->states[current_state_id].on_departure != NULL){
		parser->states[current_state_id].on_departure(key, c);
	}

	if(parser->error_state->on_arrival != NULL){
		parser->error_state->on_arrival(key, c);
	}

	return parser->error_state->id;
}

void add_accepted_chars_to_transition(parser_transition * transition, uint8_t * array_chars){
	
	//Primero seteamos todas las posiciones en falso
	//TODO: no esta buena esta implementacion porque no puedo ir agregando chars de a varias llamadas.
	
	for (size_t i = 0; i < PRINTABLE_ASCII_CHARS; i++)
	{
		transition->accepted_chars[i] = false;
	}
	
	uint8_t c;
	for (size_t i = 0; array_chars[i] != '\0'; i++)
	{
		c = array_chars[i];
		if(c < 32 || c > 127) {
			if (c == '\r') {
				transition->accepted_chars[CR_INDEX] = true;
			} else if (c == '\n') {
				transition->accepted_chars[LF_INDEX] = true;
			} else {
				continue;
			}
		} else {
			if (c == ' ') {
				transition->accepted_chars[SPACE_INDEX] = true;
			} else {
				transition->accepted_chars[c - CHARS_ARRAY_SHIFT - 1] = true;
			}
		}
	}
}

void add_rejected_chars_to_transition(parser_transition * transition, u_int8_t * array_chars){
	
	//Primero seteamos todas las posiciones en verdadero
	//TODO: no esta buena esta implementacion porque no puedo ir agregando chars de a varias llamadas.
	
	for (size_t i = 0; i < PRINTABLE_ASCII_CHARS; i++)
	{
		transition->accepted_chars[i] = true;
	}

	uint8_t c;
	for (size_t i = 0; array_chars[i] != '\0'; i++)
	{
		c = array_chars[i];
		if(c < 32 || c > 127) {
			if (c == '\r') {
				transition->accepted_chars[CR_INDEX] = false;
			} else if (c == '\n') {
				transition->accepted_chars[LF_INDEX] = false;
			} else {
				continue;
			}
		} else {
			if (c == ' ') {
				transition->accepted_chars[SPACE_INDEX] = false;
			} else {
				transition->accepted_chars[c - CHARS_ARRAY_SHIFT - 1] = false;
			}
		}
	}
}

bool is_final(parser_state * state){
	return state->is_final;
}
