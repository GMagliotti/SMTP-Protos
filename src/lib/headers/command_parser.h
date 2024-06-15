#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

#include <parser.h>
#include <selector.h>

#define STATES_QTY 5

/** Estructura que ira recolectando caracteres hasta armar un comando smtp capaz de ser procesado.
 *  Un puntero al command_parser debe ser necesario para ir procesando cada caracter**/
typedef struct smtp_command
{
    parser_definition * parser;
    //char command[MAX_COMMAND_LEN];
    //char arg[MAX_ARG][MAX_ARG_LEN];
} smtp_command;

/** Inicializamos el parseo de comandos **/
void init_command_parsing(smtp_command * smtp_command);

/** Configuracion del parser de comandos, armamado de la estructura parser_definition, setup de vectores de chars aceptados**/
void parser_configuration();

/** Encargada de ir parseando el comando, es la funcion que llamara a process_char. Retorna el estado que corresponde **/
int parse_command(struct selector_key * key);

void finish_command_parsing();

#endif