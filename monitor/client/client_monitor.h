#ifndef CLIENT_MONITOR_H
#define CLIENT_MONITOR_H
// This is the implementation of a client for the monitor protocol
#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#define SIGNATURE    0xfffe
#define VERSION      0x00
#define TOKEN        0xffe91a2b3c4d5e6f
#define REQUEST_SIZE 14
#define CMD1         0x00
#define DEFAULT_HOST "127.0.0.1"
#define DEFAULT_PORT "2526"

enum commands
{
	HIST_C = 0x00,
	CONC_C,
	BYTES_T,
	TRANS_S,
	TRANS_ON,
	TRANS_OFF
};

static const char* commands_str[] = { "HIST", "CONC", "BYTES", "STATUS", "T_ON", "T_OFF" };

#endif