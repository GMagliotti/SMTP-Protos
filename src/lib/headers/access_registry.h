#ifndef ACCESS_REGISTRY_H
#define ACCESS_REGISTRY_H

#include "smtp.h"

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void init_access_registry();
void register_mail(uint8_t* from, uint8_t* to, time_t time, ssize_t data_size);
void free_access_registry();
void print_access_registry(int fd);
void print_mails_by_user(int fd, char* user);
void print_mails_by_time(int fd, time_t start, time_t end);
void print_last_mail(int fd);

#endif