#include "access_registry.h"

#include <time.h>
// we will keep track of mails in two ways: by name of the sender and by time. Using tww linked lists.
typedef struct mail_entry_t
{
	uint8_t from[LOCAL_USER_NAME_SIZE];
	uint8_t to[LOCAL_USER_NAME_SIZE];
	time_t time;
	ssize_t data_size;

	struct mail_entry_t* next_by_name;

	struct mail_entry_t* next_by_time;
} mail_entry_t;

typedef struct access_registry_t
{
	uint32_t mails_count;

	mail_entry_t* first_by_name;

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
	access_registry->first_by_time = NULL;
	access_registry->last_by_time = NULL;
}

void
register_mail(uint8_t* from, uint8_t* to, time_t time, ssize_t data_size)
{
	mail_entry_t* new_mail = (mail_entry_t*)malloc(sizeof(mail_entry_t));
	strcpy(new_mail->from, from);
	strcpy(new_mail->to, to);
	new_mail->time = time;
	new_mail->data_size = data_size;
	new_mail->next_by_name = NULL;
	new_mail->next_by_time = NULL;

	if (access_registry->first_by_name == NULL) {
		access_registry->first_by_name = new_mail;
	} else {
		mail_entry_t* current = access_registry->first_by_name;
		mail_entry_t* previous = NULL;
		while (current != NULL && strcmp(current->from, from) < 0) {
			previous = current;
			current = current->next_by_name;
		}
		if (previous == NULL) {
			new_mail->next_by_name = access_registry->first_by_name;
			access_registry->first_by_name = new_mail;
		} else {
			previous->next_by_name = new_mail;
			new_mail->next_by_name = current;
		}
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
		dprintf(fd, "Path: %s/%s/%s/%s/%ld\n", MAILBOX_DIR, DOMAIN_NAME, current->from, MAILDIR_TMP, current->time);
		dprintf(fd, "Time: %ld\n", current->time);
		dprintf(fd, "\n");
		current = current->next_by_name;
	}
}

void
print_mails_by_user(int fd, char* user)
{
	// we take advantage of the fact that the list is sorted by name
	mail_entry_t* current = access_registry->first_by_name;
	while (current != NULL && strcmp(current->from, user) < 0) {
		current = current->next_by_name;
	}
	if (current == NULL) {
		dprintf(fd, "No mails found for user %s\n", user);
		return;
	}
	int fd_mail = 0;
	ssize_t bytes_sent;
	char path[MAIL_DIR_SIZE + 1 + DOMAIN_NAME_SIZE + 1 + LOCAL_USER_NAME_SIZE + 1 + MAILBOX_INNER_DIR_SIZE + 1 +
	          MS_TEXT_SIZE] = { 0 };
	while (current != NULL && strcmp(current->from, user) == 0) {
		dprintf(fd, "From: %s\n", current->from);
		dprintf(fd, "To: %s\n", current->to);
		dprintf(fd, "Time: %ld\n", current->time);
		sprintf(path, "%s/%s/%s/%s/%ld", MAILBOX_DIR, DOMAIN_NAME, current->from, MAILDIR_TMP, current->time);
		fd_mail = open(path, O_RDONLY);
		bytes_sent = sendfile(fd, fd_mail, 0, current->data_size);
		if (bytes_sent < 0) {
			dprintf(fd, "Error sending file %s\n", path);
			return;
		}
		close(fd_mail);
		dprintf(fd, "\n");
		current = current->next_by_name;
	}
}

void
print_mails_by_time(int fd, time_t start, time_t end)  // [start, end]
{
	if (start > end) {
		dprintf(fd, "Invalid time range\n");
		return;
	}
	mail_entry_t* current = access_registry->first_by_time;
	while (current != NULL && current->time < start) {
		current = current->next_by_time;
	}
	if (current == NULL || current->time > end) {
		dprintf(fd, "No mails found in the given time range\n");
		return;
	}
	while (current != NULL && current->time <= end) {
		dprintf(fd, "From: %s\n", current->from);
		dprintf(fd, "To: %s\n", current->to);
		dprintf(fd, "Path: %s/%s/%s/%s/%ld\n", MAILBOX_DIR, DOMAIN_NAME, current->from, MAILDIR_TMP, current->time);
		dprintf(fd, "Time: %ld\n", current->time);
		dprintf(fd, "\n");
		current = current->next_by_time;
	}
}

void
print_last_mail(int fd)
{
	if (access_registry->last_by_time != NULL) {
		dprintf(fd, "From: %s\n", access_registry->last_by_time->from);
		dprintf(fd, "To: %s\n", access_registry->last_by_time->to);
		dprintf(fd,
		        "Path: %s/%s/%s/%s/%ld\n",
		        MAILBOX_DIR,
		        DOMAIN_NAME,
		        access_registry->last_by_time->from,
		        MAILDIR_TMP,
		        access_registry->last_by_time->time);
		dprintf(fd, "Time: %ld\n", access_registry->last_by_time->time);
		dprintf(fd, "\n");
	}
}
