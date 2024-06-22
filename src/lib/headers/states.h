#ifndef SMTP_STATE_H
#define SMTP_STATE_H

typedef enum 
{
	EHLO,
	FROM,
	TO,
	DATA,
	BODY,
	DONE,
	ERROR,
	// definir los estados de la maquina de estados del protocolo SMTP
} smtp_state;




#endif // SMTP_STATE_H