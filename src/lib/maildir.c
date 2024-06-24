#include "maildir.h"
#define MAIL_DIR_SIZE 4
#define DOMAIN_NAME_SIZE 255
#define LOCAL_USER_NAME_SIZE 64
#define MAILBOX_INNER_DIR_SIZE 3 // tmp, new, cur
#define MS_TEXT_SIZE 13

char* get_or_create_maildir(char* email) {
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

char* get_maildir(char* email) {
	return get_or_create_maildir(email);
}

char* create_maildir(char* email) {
	return get_or_create_maildir(email);
}

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

int get_temp_file_fd(char* email) {
	logf(LOG_DEBUG, "Creating temp file for %s", email);
	char* maildir = get_maildir(email);
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

	char filename[MAIL_DIR_SIZE + 1 + DOMAIN_NAME_SIZE + 1 + LOCAL_USER_NAME_SIZE + 1 + MAILBOX_INNER_DIR_SIZE + 1 +
	              MS_TEXT_SIZE] = { 0 };
	time_t ms = time(NULL);
	// filename like mail/<domain>/<user>/tmp/<timestamp>
	snprintf(filename, sizeof(filename), "%s/tmp/%ld", maildir, ms);

	int fd = open(filename, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG | S_IRWXO);
	if (fd < 0) {
		logf(LOG_ERROR, "Error creating temp file for %s", email);
		perror("open");
		return -1;
	}
	logf(LOG_DEBUG, "Created temp file %s opened with fd %d", filename, fd);

	free(maildir);
	return fd;
}

void copy_temp_to_new(char* email, int temp_file_fd) {
		// we copy the mail from mail/<domain>/<user>/tmp/<timestamp> to mail/<domain>/<rcpt_to>/new/<timestamp>
		// we need to create the new dir if it doesn't exist
		logf(LOG_DEBUG, "Copying temp file (fd=%d) to new for email %s", temp_file_fd, email);
		char* maildir = get_maildir(email);
		if (maildir == NULL) {
			logf(LOG_ERROR, "Error getting maildir for %s", email);
			perror("get_and_create_maildir");
			return;
		}

		// now we create new dir within maildir
		char full_dir[MAIL_DIR_SIZE + 1 + DOMAIN_NAME_SIZE + 1 + LOCAL_USER_NAME_SIZE + 1 + MAILBOX_INNER_DIR_SIZE] = {
			0
		};
		snprintf(full_dir, sizeof(full_dir), "%s/new", maildir);
		if (create_maildir_directory(full_dir) == -1) {
			logf(LOG_ERROR, "Error creating new directory for %s", email);
			perror("create_directory_if_not_exists");
			return;
		}
		

		free(maildir);

		char filename[MAIL_DIR_SIZE + 1 + DOMAIN_NAME_SIZE + 1 + LOCAL_USER_NAME_SIZE + 1 + MAILBOX_INNER_DIR_SIZE + 1 +
		              MS_TEXT_SIZE] = { 0 };

		time_t ms = time(NULL);
		snprintf(filename, sizeof(filename), "%s/%ld", full_dir, ms);

		int new_fd = open(filename, O_CREAT | O_WRONLY, 0777);
		if (new_fd < 0) {
			logf(LOG_ERROR, "Error creating new mail file for %s", email);
			perror("open");
			return;
		}

		// we need to copy the file from the tmp dir to the new dir
		// we need to read the file from the tmp dir

		char* domain = strchr(email, '@') + 1;
		char local_user[LOCAL_USER_NAME_SIZE] = { 0 };

		char tmp_filename[MAIL_DIR_SIZE + 1 + DOMAIN_NAME_SIZE + 1 + LOCAL_USER_NAME_SIZE + 1 + MAILBOX_INNER_DIR_SIZE +
		                  1 + MS_TEXT_SIZE] = { 0 };
		snprintf(tmp_filename, sizeof(tmp_filename), "mail/%s/%s/tmp/%ld", domain, local_user, ms);

		// we need to read from the tmp file and write to the new file
		char buffer[1024] = { 0 };

		ssize_t bytes_read = 0;
		lseek(temp_file_fd, 0, SEEK_SET);  // we need to go to the beginning of the file because we have already written to it

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
		if (close(temp_file_fd) != 0) {
			logf(LOG_ERROR, "Error closing temp mail file (fd=%d)", temp_file_fd);
			perror("close");
			return;
		}
}