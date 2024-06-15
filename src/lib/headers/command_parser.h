#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

#include <parser.h>

/** Estructura que ira recolectando caracteres hasta armar un comando smtp capaz de ser procesado.
 *  Un puntero al command_parser debe ser necesario para ir procesando cada caracter**/
typedef struct smtp_command
{
    parser_definition * parser;
    //char command[MAX_COMMAND_LEN];
    //char arg[MAX_ARG][MAX_ARG_LEN];
} smtp_command;

/** Inicializamos el parseo de comandos **/
void init_command_parsing();

/** Configuracion del parser de comandos, armamado de la estructura parser_definition, setup de vectores de chars aceptados**/
void parser_configuration();


void finish_command_parsing();

#endif