#include "headers/access_registry.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#define SUPER_SECRET_PASSWORD "password"


static bool is_user_present(const char* user);
void add_user(const char* user);
static bool is_leap_year(int year);
static bool is_valid_time(char* time);

// we will keep track of mails in two ways: by name of the sender and by time. Using tww linked lists.
typedef struct mail_entry_t
{
	char* from;
	char* to;
	char* path;
	time_t time;

	struct mail_entry_t* next_by_name;

	struct mail_entry_t* next_by_time;
} mail_entry_t;

typedef struct user_entry_t
{
	char* user;
	struct user_entry_t* next;
} user_entry_t;

typedef struct access_registry_t
{
	uint32_t mails_count;

	user_entry_t* users;

	mail_entry_t* first_by_name;
	mail_entry_t* last_by_name;

	mail_entry_t* first_by_time;
	mail_entry_t* last_by_time;

} access_registry_t;

access_registry_t* access_registry;

void
init_access_registry()
{
	access_registry = (access_registry_t*)calloc(1, sizeof(access_registry_t));
	access_registry->mails_count = 0;


	// llamar al mock
	
}

void
register_mail(char* from, char* to, char* path, time_t time)
{
	mail_entry_t* new_mail = (mail_entry_t*)calloc(1, sizeof(mail_entry_t));
	new_mail->from = from;
	new_mail->to = to;
	new_mail->path = path;
	new_mail->time = time;

	add_user(from);
	add_user(to);

	if (access_registry->first_by_name == NULL) {
		access_registry->first_by_name = new_mail;
		access_registry->last_by_name = new_mail;
	} else {
		access_registry->last_by_name->next_by_name = new_mail;
		access_registry->last_by_name = new_mail;
	}

	if (access_registry->first_by_time == NULL) {
		access_registry->first_by_time = new_mail;
		access_registry->last_by_time = new_mail;
	} else {
		mail_entry_t* current = access_registry->first_by_time;
		mail_entry_t* previous = NULL;
		while (current != NULL && current->time < time) {
			previous = current;
			current = current->next_by_time;
		}
		if (previous == NULL) {
			new_mail->next_by_time = access_registry->first_by_time;
			access_registry->first_by_time = new_mail;
		} else {
			previous->next_by_time = new_mail;
			new_mail->next_by_time = current;
		}
	}

	access_registry->mails_count++;
}

void
free_access_registry()
{
	mail_entry_t* current = access_registry->first_by_name;
	mail_entry_t* next = NULL;
	while (current != NULL) {
		next = current->next_by_name;
		free(current);
		current = next;
	}

	user_entry_t* current_user = access_registry->users;
	user_entry_t* next_user = NULL;
	while (current_user != NULL) {
		next_user = current_user->next;
		free(current_user->user);
		free(current_user);
		current_user = next_user;
	}
	free(access_registry);
}

void
print_access_registry(int fd)
{
	mail_entry_t* current = access_registry->first_by_name;
	while (current != NULL) {
		dprintf(fd, "From: %s\n", current->from);
		dprintf(fd, "To: %s\n", current->to);
		dprintf(fd, "Path: %s\n", current->path);
		dprintf(fd, "Time: %ld\n", current->time);
		dprintf(fd, "\n");
		current = current->next_by_name;
	}
}

void
print_mails(int fd, char* user)
{
	mail_entry_t* current = access_registry->first_by_name;
	while (current != NULL) {
		if (strcmp(current->to, user) == 0) {
			dprintf(fd, "From: %s\n", current->from);
			dprintf(fd, "To: %s\n", current->to);
			dprintf(fd, "Path: %s\n", current->path);
			dprintf(fd, "Time: %ld\n", current->time);
			dprintf(fd, "\n");
		}
		current = current->next_by_name;
	}
}

void
print_mails_by_time(int fd, time_t start, time_t end)
{
	mail_entry_t* current = access_registry->first_by_time;
	while (current != NULL && current->time < start) {
		current = current->next_by_time;
	}
	while (current != NULL && current->time <= end) {
		dprintf(fd, "From: %s\n", current->from);
		dprintf(fd, "To: %s\n", current->to);
		dprintf(fd, "Path: %s\n", current->path);
		dprintf(fd, "Time: %ld\n", current->time);
		dprintf(fd, "\n");
		current = current->next_by_time;
	}
}
void
print_mails_by_day(int fd, time_t day)
{
	struct tm* day_info = localtime(&day);
	int day_year = day_info->tm_year;
	int day_month = day_info->tm_mon;
	int day_mday = day_info->tm_mday;

	mail_entry_t* current = access_registry->first_by_time;
	while (current != NULL) {
		struct tm* mail_time_info = localtime(&(current->time));
		if (mail_time_info->tm_year == day_year && mail_time_info->tm_mon == day_month &&
		    mail_time_info->tm_mday == day_mday) {
			dprintf(fd, "From: %s\n", current->from);
			dprintf(fd, "To: %s\n", current->to);
			dprintf(fd, "Path: %s\n", current->path);
			dprintf(fd, "Time: %ld\n", current->time);
			dprintf(fd, "\n");
		}
		current = current->next_by_time;
	}
}
static bool
is_user_present(const char* user)
{
	user_entry_t* current = access_registry->users;
	while (current != NULL) {
		if (strcmp(current->user, user) == 0) {
			return true;
		}
		current = current->next;
	}
	return false;
}

void
add_user(const char* user)
{
	if (!is_user_present(user)) {
		user_entry_t* new_user = (user_entry_t*)calloc(1, sizeof(user_entry_t));
		new_user->user = strdup(user);  // Allocate memory and copy the user string
		new_user->next = access_registry->users;
		access_registry->users = new_user;
	}
}
bool
authenticate(char* pwd)
{
	return strcmp(SUPER_SECRET_PASSWORD, pwd) == 0;
}

bool
is_user(char* user)
{
	return is_user_present(user);
}

bool
convert_and_validate_date(char* date_str, time_t* out_time)
{
	if (!is_valid_time(date_str)) {
		return false;
	}

	struct tm tm_date = { 0 };
	int year, month, day;

	// Parsea la fecha
	if (sscanf(date_str, "%d/%d/%d", &day, &month, &year) == 3) {
		tm_date.tm_year = year - 1900;  // tm_year es años desde 1900
		tm_date.tm_mon = month - 1;     // tm_mon es 0-based
		tm_date.tm_mday = day;
		tm_date.tm_hour = 0;
		tm_date.tm_min = 0;
		tm_date.tm_sec = 0;
		tm_date.tm_isdst = -1;  // Determina automáticamente DST

		// Convierte a time_t
		*out_time = mktime(&tm_date);

		// Verifica si la conversión fue exitosa
		if (*out_time != (time_t)-1) {
			return true;
		}
	}
	return false;
}

static bool
is_leap_year(int year)
{
	return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

static bool
is_valid_time(char* time)
{
	// Validate the date format dd/mm/yyyy
	if (strlen(time) != 10) {
		return false;
	}
	if (time[2] != '/' || time[5] != '/') {
		return false;
	}

	// Validate the values
	int day = (time[0] - '0') * 10 + (time[1] - '0');
	int month = (time[3] - '0') * 10 + (time[4] - '0');
	int year = (time[6] - '0') * 1000 + (time[7] - '0') * 100 + (time[8] - '0') * 10 + (time[9] - '0');

	if (year < 1900 || year > 2100) {  // Assuming valid years range
		return false;
	}
	if (month < 1 || month > 12) {
		return false;
	}
	if (day < 1 || day > 31) {
		return false;
	}

	// Validate days in month, including leap year for February
	if (month == 2) {
		if (is_leap_year(year)) {
			if (day > 29)
				return false;
		} else {
			if (day > 28)
				return false;
		}
	} else if (month == 4 || month == 6 || month == 9 || month == 11) {
		if (day > 30)
			return false;
	}

	return true;
}



// int
// main()
// {
// 	// Initialize the registry
// 	init_access_registry();

// 	// Register some sample emails
// 	time_t now = time(NULL);
// 	register_mail("alice@example.com", "bob@example.com", "/path/to/email1", now - 60);    // 1 minute ago
// 	register_mail("carol@example.com", "dave@example.com", "/path/to/email2", now - 120);  // 2 minutes ago
// 	register_mail("eve@example.com", "bob@example.com", "/path/to/email3", now - 180);     // 3 minutes ago
// 	register_mail("bob@example.com", "carol@example.com", "/path/to/email4", now - 30);    // 30 seconds ago

// 	// Print all emails in the order of their registration (by name)
// 	printf("All emails by name:\n");
// 	print_access_registry(STDOUT_FILENO);

// 	// Print all emails received by bob@example.com
// 	printf("Emails to bob@example.com:\n");
// 	print_mails(STDOUT_FILENO, "bob@example.com");

// 	// Print all emails within the last 2 minutes
// 	printf("Emails in the last 2 minutes:\n");
// 	print_mails_by_time(STDOUT_FILENO, now - 120, now);

// 	// Print emails by specific day
// 	time_t specific_day;
// 	if (convert_and_validate_date("01/01/2023", &specific_day)) {
// 		printf("Emails on 01/01/2023:\n");
// 		print_mails_by_day(STDOUT_FILENO, specific_day);
// 	} else {
// 		printf("Invalid date format for specific day.\n");
// 	}

// 	// Authenticate user
// 	if (authenticate("password")) {
// 		printf("User authenticated.\n");
// 	} else {
// 		printf("Authentication failed.\n");
// 	}

// 	// Check if a user exists
// 	if (is_user("alice@example.com")) {
// 		printf("User alice@example.com exists.\n");
// 	} else {
// 		printf("User alice@example.com does not exist.\n");
// 	}

// 	printf("THIS SHOULD FAIL\n");
// 	// 1. Registrar un correo con datos inválidos
// 	register_mail("", "", "/invalid/path", now);  // Datos inválidos
// 	printf("Intento de registro con datos inválidos completado.\n");

// 	// 2. Imprimir correos para un usuario que no existe
// 	printf("Emails to nonexisting@example.com:\n");
// 	print_mails(STDOUT_FILENO, "nonexisting@example.com \n");

// 	// 3. Solicitar correos en un rango de tiempo no válido
// 	printf("Emails in an invalid time range:\n");
// 	print_mails_by_time(STDOUT_FILENO, now, now - 120);  // Rango de tiempo inválido

// 	// 4. Usar una fecha inválida para la función convert_and_validate_date
// 	if (convert_and_validate_date("invalid_date", &specific_day)) {
// 		printf("Emails on invalid_date:\n");
// 		print_mails_by_day(STDOUT_FILENO, specific_day);
// 	} else {
// 		printf("Invalid date format for specific day.\n");
// 	}

// 	// 5. Autenticar con una contraseña incorrecta
// 	if (authenticate("wrong_password")) {
// 		printf("User authenticated with wrong password.\n");
// 	} else {
// 		printf("Authentication failed with wrong password.\n");
// 	}

// 	// 6. Verificar la existencia de un usuario que no ha sido registrado
// 	if (is_user("nonregistered@example.com")) {
// 		printf("User nonregistered@example.com exists.\n");
// 	} else {
// 		printf("User nonregistered@example.com does not exist.\n");
// 	}

// 	// Free the registry
// 	free_access_registry();

// 	return 0;
// }