#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

#include <parser.h>
#include <selector.h>
#include <buffer.h>

#define STATES_QTY 5
/** Todos los verbos son de 4 caracteres, exceptuando STARTTLS
 *  Segun el RFC, para el comando MAIL FROM, FROM es argumento**/
#define MAX_COMMAND_LEN 8
/** Segun el RFC (4.5.3 Sizes and Timeouts), una linea entera de SMTP no debe exceder los 512 caracteres, si restamos \r y \n tenemos 510 caracteres, 
 *  si restamos el comando 502 caracteres, si restamos el espacio que separa al verbo del argumento 501.
 *  Tenemos 502 caracteres para argumentos. En principio, tenemos dos argumentos como maximo: TO:, FROM: y el contenido de estos 
 *  mas el de HELO, EXPN y VRFY. La maxima longitud para un dominio es 255, si sumamos los <> 257
 **/
#define MAX_ARG_LEN 257
/** Muy a chequear esto, lo iremos adaptando bajo demanda **/
#define MAX_ARG 2
#define MAX_LINE_LEN 512

/** Estructura que ira recolectando caracteres hasta armar un comando smtp capaz de ser procesado.
 *  Un puntero al command_parser debe ser necesario para ir procesando cada caracter**/
typedef struct smtp_command
{
    parser_definition * parser;
    size_t current_state;
    bool ended;
    bool error;
    //Guardamos espacio extra para el \0
    char command[MAX_COMMAND_LEN + 1];
    //Cada argumento esta separado por un espacio
    char arg[MAX_LINE_LEN - MAX_COMMAND_LEN - 3 + 1];
    size_t command_dim;
    size_t arg_dim;

} smtp_command;

/** Inicializamos el parseo de comandos **/
void init_command_parsing(smtp_command * smtp_command);

/** Configuracion del parser de comandos, armamado de la estructura parser_definition, setup de vectores de chars aceptados**/
void parser_configuration();

/** Encargada de ir parseando el comando, es la funcion que llamara a process_char. Necesita del buffer para poder consumir el char 
 * Retorna el estado que corresponde 
 * **/
int parse_command(struct selector_key * key, smtp_command * smtp_command, struct buffer * buffer);

void finish_command_parsing();

#endif