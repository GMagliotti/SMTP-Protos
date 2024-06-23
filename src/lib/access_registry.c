#include "access_registry.h"

#include <time.h>
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

typedef struct access_registry_t
{
	uint32_t mails_count;

	mail_entry_t* first_by_name;
	mail_entry_t* last_by_name;

	mail_entry_t* first_by_time;
	mail_entry_t* last_by_time;

} access_registry_t;

access_registry_t* access_registry;

void
init_access_registry()
{
	access_registry = (access_registry_t*)malloc(sizeof(access_registry_t));
	access_registry->mails_count = 0;
	access_registry->first_by_name = NULL;
	access_registry->last_by_name = NULL;
	access_registry->first_by_time = NULL;
	access_registry->last_by_time = NULL;
}

void
register_mail(char* from, char* to, char* path, time_t time)
{
	mail_entry_t* new_mail = (mail_entry_t*)malloc(sizeof(mail_entry_t));
	new_mail->from = from;
	new_mail->to = to;
	new_mail->path = path;
	new_mail->time = time;
	new_mail->next_by_name = NULL;
	new_mail->next_by_time = NULL;

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
