#include "authentication.h"

#include <string.h>  // For strcmp

// Simulated user database (replace with actual storage)

const char* valid_users[USER_DB_SIZE][2] = { { "user1", "password1" },
	                                         { "user2", "password2" },
	                                         { "user3", "password3" } };

bool
authenticate_plain(const char* username, const char* password)
{
	// Check if the provided username and password match any in the database
	for (int i = 0; i < USER_DB_SIZE; ++i) {
		if (strcmp(username, valid_users[i][0]) == 0 && strcmp(password, valid_users[i][1]) == 0) {
			return true;
		}
	}
	return false;
}

bool
authenticate_login(const char* username, const char* password)
{
	// For LOGIN mechanism, it's typically used as a two-step process
	// where the username is sent first, and then the password.
	// We assume the server has received the username and now needs to check the password.

	// Check if the provided username and password match any in the database
	for (int i = 0; i < USER_DB_SIZE; ++i) {
		if (strcmp(username, valid_users[i][0]) == 0 && strcmp(password, valid_users[i][1]) == 0) {
			return true;
		}
	}
	return false;
}
