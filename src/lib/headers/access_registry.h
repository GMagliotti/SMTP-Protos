#ifndef ACCESS_REGISTRY_H
#define ACCESS_REGISTRY_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void init_access_registry();
void register_mail(char* from, char* to, char* path, time_t time);
void free_access_registry();
void print_access_registry(int fd);
void print_mails(int fd, char* user);
void print_mails_by_time(int fd, time_t start, time_t end);

#endif