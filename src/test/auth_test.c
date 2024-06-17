#define USER_DB_SIZE       3
#define AUTH_PLAIN_MAX_LEN 256

#include <assert.h>       // For assert
#include <openssl/evp.h>  // For base64 encoding/decoding
#include <stdbool.h>      // For bool type
#include <string.h>       // For strcmp

int
base64_decode(const char* input, int length, unsigned char* output)
{
	// EVP_DecodeBlock() decodes base64 data.
	// It returns the length of the decoded data, or -1 on error.
	// Note: EVP_DecodeBlock() adds up to 2 '\0' padding characters.
	int output_length = EVP_DecodeBlock(output, (const unsigned char*)input, length);

	// Adjust length if input includes newline characters or padding '=' at the end.
	if (output_length > 0) {
		if (input[length - 1] == '=')
			output_length--;
		if (input[length - 2] == '=')
			output_length--;
	}

	return output_length;
}

// simulated
const char* valid_users[USER_DB_SIZE][2] = { { "user1", "password1" },
	                                         { "user2", "password2" },
	                                         { "user3", "password3" } };

bool
authenticate_plain(const char* auth_data)
{
	// For PLAIN mechanism, the client sends the username and password in a single base64-encoded string
	// separated by a NULL byte. The server must decode the string and check if the provided credentials are valid.

	// Decode the base64-encoded string
	// The decoded string should be in the format "\0username\0password"
	unsigned char decoded_data[AUTH_PLAIN_MAX_LEN];
	int decoded_length = base64_decode(auth_data, strlen(auth_data), decoded_data);

	// Extract the username and password from the decoded string
	char* username = (char*)decoded_data + 1;
	char* password = strchr(username, '\0') + 1;

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

void
test_auth_plain_init()
{
	// Test case 1: Valid username and password
	const char* auth_data = "AHVzZXIxAHBhc3N3b3JkMQ==";  // base64("\0user1\0password1")
	assert(authenticate_plain(auth_data) == true);

	// Test case 2: Invalid username and password
	auth_data = "dXNlcjF1c2VyNA==";  // base64("\0user1\0password4")
	assert(authenticate_plain(auth_data) == false);
}

void
test_auth_login_init()
{
	// Test case 1: Valid username and password
	const char* username = "user1";
	const char* password = "password1";
	assert(authenticate_login(username, password) == true);

	// Test case 2: Invalid username and password
	username = "user1";
	password = "password4";
	assert(authenticate_login(username, password) == false);
}

int
main()
{
	test_auth_plain_init();
	test_auth_login_init();
	printf("All tests passed successfully.\n");
	return 0;
}