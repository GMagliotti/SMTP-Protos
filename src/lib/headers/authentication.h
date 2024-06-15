#ifndef AUTHENTICATION_H
#define AUTHENTICATION_H

#include <stdbool.h>  // For bool type

// Define constants for authentication mechanisms
#define AUTH_MECH_PLAIN "PLAIN"
#define AUTH_MECH_LOGIN "LOGIN"
#define USER_DB_SIZE    3

// Function declarations
bool authenticate_plain(const char* username, const char* password);
bool authenticate_login(const char* username, const char* password);

#endif /* AUTHENTICATION_H */