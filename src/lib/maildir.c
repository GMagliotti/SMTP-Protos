#include "maildir.h"

#include "smtp.h"

#include <sys/stat.h>
#define MAIL_DIR_SIZE          7
#define DOMAIN_NAME_SIZE       255
#define LOCAL_USER_NAME_SIZE   64
#define MAILBOX_INNER_DIR_SIZE 3  // tmp, new, cur
#define MS_TEXT_SIZE           13
#define MAIL_FILE_NAME_LENGTH  24
#define RAND_STR_LENGTH        10

static char*
rand_str(char* dest, size_t length)
{
	char charset[] = "0123456789"
	                 "abcdefghijklmnopqrstuvwxyz"
	                 "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

	while (length-- > 0) {
		size_t index = (double)rand() / RAND_MAX * (sizeof charset - 1);
		*dest++ = charset[index];
	}
	*dest = '\0';
	return dest;
}

int
create_nonexistent_dir(char* path)
{
	struct stat sb;
	if (stat(path, &sb) == -1) {
		if (mkdir(path, S_IRWXU | S_IRWXG | S_IRWXO) == -1) {
			logf(LOG_ERROR, "Error creating directory %s", path);
			perror("mkdir");
			return -1;
		}
		return 0;
	}
	return 0;
}

char*
create_maildir(char* user)
{
	char maildir_len = 2 + MAIL_DIR_SIZE + 1 + LOCAL_USER_NAME_SIZE + 1 + MAILBOX_INNER_DIR_SIZE;
	char* maildir = malloc(maildir_len);

	if (maildir == NULL) {
		log(LOG_ERROR, "Could not allocate memory for maildir path");
		return NULL;
	}

	snprintf(maildir, maildir_len, "./Maildir");
	if (create_nonexistent_dir(maildir) == -1) {
		goto finalize;
	}

	snprintf(maildir, maildir_len, "./Maildir/%s", user);
	if (create_nonexistent_dir(maildir) == -1) {
		goto finalize;
	}

	snprintf(maildir, maildir_len, "./Maildir/%s/cur", user);
	if (create_nonexistent_dir(maildir) == -1) {
		goto finalize;
	}

	snprintf(maildir, maildir_len, "./Maildir/%s/new", user);
	if (create_nonexistent_dir(maildir) == -1) {
		goto finalize;
	}

	snprintf(maildir, maildir_len, "./Maildir/%s/tmp", user);
	if (create_nonexistent_dir(maildir) == -1) {
		goto finalize;
	}
	snprintf(maildir, maildir_len, "./Maildir/%s", user);
	return maildir;

finalize:
	free(maildir);
	return NULL;
}

int
create_temp_mail_file(char* email, char* copy_addr)
{
	char* email_dup = strdup(email);
	char* name = strtok(email_dup, "@");
	char* maildir_path = create_maildir(name);
	if (maildir_path == NULL) {
		logf(LOG_ERROR, "Error creating maildir for %s", email);
	}
	strcat(maildir_path, "/tmp");
	strncat(maildir_path, copy_addr, MAIL_FILE_NAME_LENGTH);

	if (copy_addr[0] == '\0') {
		char random[RAND_STR_LENGTH + 1];
		rand_str(random, RAND_STR_LENGTH);
		snprintf(copy_addr, MAIL_FILE_NAME_LENGTH, "%lu_%s", time(NULL), random);
	}
	strcat(maildir_path, "/");
	strncat(maildir_path, copy_addr, MAIL_FILE_NAME_LENGTH);
	int fd = open(maildir_path, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG | S_IRWXO);
	free(email_dup);
	free(maildir_path);
	return fd;
}

void
copy_temp_to_new_single(char* email, char* temp_file_name)
{





	// we copy the mail from mail/<domain>/<user>/tmp/<timestamp> to mail/<domain>/<rcpt_to>/new/<timestamp>
	// we need to create the new dir if it doesn't exist
	logf(LOG_DEBUG, "Copying temp file (fd=%d) to new for email %s", temp_file_fd, email);
	char* email_dup = strdup(email);
	char* name = strtok(email_dup, "@");
	char* maildir_path = create_maildir(name);

	if (maildir_path == NULL) {
		logf(LOG_ERROR, "Error getting maildir for %s", email);
		perror("get_and_create_maildir");
		free(email_dup);
		return;
	}
	strcat(maildir_path, "/new");
	strcat(maildir_path, "/");
	strncat(maildir_path, temp_file_name, MAIL_FILE_NAME_LENGTH);

	int new_fd = open(maildir_path, O_CREAT | O_WRONLY, S_IRWXU | S_IRWXG | S_IRWXO);
	if (new_fd < 0) {
		logf(LOG_ERROR, "Error creating new mail file for %s", email);
		free(email_dup);
		free(maildir_path);
		perror("open");
		return;
	}

	free(email_dup);
	free(maildir_path);

	char buffer[1024] = { 0 };

	ssize_t bytes_read = 0;
	lseek(
	    temp_file_fd, 0, SEEK_SET);  // we need to go to the beginning of the file because we have already written to it

	// This while loop is provisional. We need to read the whole file and apply the transformation if there is any
	// The reason for doing it as a while loop is because we haven't implemented transformations yet

	while ((bytes_read = read(temp_file_fd, buffer, sizeof(buffer))) > 0) {
		ssize_t bytes_written = write(new_fd, buffer, bytes_read);
		if (bytes_written < 0) {
			logf(LOG_ERROR, "Error writing to new mail file for %s (fd=%d)", email, new_fd);
			perror("write");
			return;
		}
	}

	if (bytes_read < 0) {
		logf(LOG_ERROR, "Error reading from temp mail file (fd=%d)", temp_file_fd);
		perror("read");
		return;
	}

	if (close(new_fd) != 0) {
		logf(LOG_ERROR, "Error closing new mail file (fd=%d)", new_fd);
		perror("close");
		return;
	}
}

char*
get_or_create_maildir(char* email)
{
	int maildir_size = MAIL_DIR_SIZE + 1 + DOMAIN_NAME_SIZE + 1 + LOCAL_USER_NAME_SIZE;
	char* maildir = malloc(maildir_size);  // Consider dynamic sizing based on mail_from length
	if (maildir == NULL) {
		perror("malloc");
		return NULL;
	}
	char* domain = strchr(email, '@');
	if (domain == NULL) {
		free(maildir);
		perror("strchr");
		return NULL;
	}
	domain++;
	char local_user[LOCAL_USER_NAME_SIZE] = { 0 };
	strncpy(local_user, email, domain - email - 1);
	snprintf(maildir, maildir_size, "mail/%s/%s", domain, local_user);

	// Create mail if it doesn't exist
	if (create_maildir_directory("mail") == -1) {
		free(maildir);
		return NULL;
	}

	// Create mail/<domain> if it doesn't exist
	char domain_dir[100] = { 0 };
	snprintf(domain_dir, sizeof(domain_dir), "mail/%s", domain);
	if (create_maildir_directory(domain_dir) == -1) {
		free(maildir);
		return NULL;
	}

	// Create mail/<domain>/<user> if it doesn't exist
	if (create_maildir_directory(maildir) == -1) {
		free(maildir);
		return NULL;
	}

	return maildir;
}

// char* get_maildir(char* email) {
// 	return get_or_create_maildir(email);
// }

// char* create_maildir(char* email) {
// 	return get_or_create_maildir(email);
// }

int
create_maildir_directory(char* maildir_path)
{
	// maildir tiene la forma mail/<domain>/<user>
	// i should check if the maildir exists, if not, create it
	struct stat st = { 0 };
	if (stat(maildir_path, &st) == -1) {
		if (mkdir(maildir_path, 0777) == -1) {  // if I want anyone to read, write or execute then i should use 0777
			logf(LOG_ERROR, "Error creating directory %s", maildir_path);
			return -1;
		}
	}
	return 0;
}

int
get_temp_file_fd(char* email, char* copy_addr)
{
	logf(LOG_DEBUG, "Creating temp file for %s", email);
	char* maildir = NULL;

	if (maildir == NULL) {
		logf(LOG_ERROR, "Error getting maildir for %s", email);
		perror("get_and_create_maildir");
		return -1;
	}

	// now we create tmp dir within maildir
	char full_dir[MAIL_DIR_SIZE + 1 + DOMAIN_NAME_SIZE + 1 + LOCAL_USER_NAME_SIZE + 1 + MAILBOX_INNER_DIR_SIZE] = { 0 };
	snprintf(full_dir, sizeof(full_dir), "%s/tmp", maildir);
	if (create_maildir_directory(full_dir) == -1) {
		logf(LOG_ERROR, "Error creating tmp directory for %s", email);
		perror("create_directory_if_not_exists");
		return -1;
	}

	char file_path[MAIL_DIR_SIZE + 1 + DOMAIN_NAME_SIZE + 1 + LOCAL_USER_NAME_SIZE + 1 + MAILBOX_INNER_DIR_SIZE + 1 +
	               MS_TEXT_SIZE] = { 0 };

	time_t ms = time(NULL);

	char* unique_str = malloc(11);
	rand_str(unique_str, 10);

	if (copy_addr != NULL) {
		snprintf(copy_addr, MS_TEXT_SIZE + 1, "%ld_%s", ms, unique_str);
	}
	// file_path like mail/<domain>/<user>/tmp/<timestamp>
	snprintf(file_path, sizeof(file_path), "%s/tmp/%ld_%s", maildir, ms, unique_str);

	free(unique_str);
	int fd = open(file_path, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG | S_IRWXO);
	if (fd < 0) {
		logf(LOG_ERROR, "Error creating temp file for %s", email);
		perror("open");
		return -1;
	}
	logf(LOG_DEBUG, "Created temp file %s opened with fd %d", file_path, fd);

	free(maildir);
	return fd;
}

// void copy_temp_to_new(char*** recipients, size_t amount, int temp_file_fd) {
// 	for (size_t i = 0; i < amount; i++) {
// 		copy_temp_to_new_single((*recipients)[i], temp_file_fd);
// 	}
// }
