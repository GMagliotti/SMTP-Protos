#include "parser.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

int * parser_feed(struct selector_key * key, parser_definition * parser, unsigned current_state_id, uint8_t c){
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
			parser_state * from_state = transitions[i].from_state;
			if(from_state->on_departure != NULL) {
				from_state->on_departure(key, c);
			}
			//Vemos si hay un metodo de entrada para el nuevo estado
			parser_state * to_state = transitions[i].to_state;
			if(to_state->on_arrival != NULL) {
				to_state->on_arrival(key, c);
			}

			return to_state->id;
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
	
	

}

bool is_final(parser_state * state){
	return state->is_final;
}
