#ifndef _MAILDIR_H_
#define _MAILDIR_H_

#include "logger.h"
#include <stdbool.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>


/**
 * @brief Gets the maildir path for a given email.
 * @param email A valid email address
 * @returns The maildir path or NULL if an error occurred or if the folder does not exist
 */
char* get_maildir(char* email);

/**
 * @brief Generates the maildir standard folder structure.
 * If the maildir already exists, neither it or its contents will be modified.
 * @returns 0 if the maildir was created successfully, -1 otherwise.
 */
int create_maildir_directory(char* maildir);

/**
 * @brief Gets the maildir path for a given email. If the maildir does not exist, it will be created. 
 * This is functionally similar to get_maildir, but it will create the maildir if it does not exist.
 * @param email A valid email address
 * @returns The maildir path or NULL if an error occurred
 */
char* create_maildir(char* email);

/**
 * @brief Get the file descriptor of a temporary file inside the user's maildir.
 * This function expects the file to be under the path mail/<domain>/<user>/tmp/<timestamp>.
 * If the file does not exist, it will be created.
 * @returns The file descriptor of the temporary file or -1 if an error occurred
 */
int get_temp_file_fd();

/**
 * @brief Copy the contents of the temporary file to a new file in the user's maildir. 
 * This new file will be under the path mail/<domain>/<user>/new, named with a timestamp.
 * Both the temporary file and the new file will be closed upon successful completion.
 */
void copy_temp_to_new(char* email, int temp_file_fd);
#endif