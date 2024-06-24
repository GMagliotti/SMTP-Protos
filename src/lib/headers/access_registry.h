#ifndef ACCESS_REGISTRY_H
#define ACCESS_REGISTRY_H

#include <stdbool.h>
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
void print_mails_by_day(int fd, time_t day);
bool authenticate(char* pwd);
bool is_user(char* user);
bool convert_and_validate_date(char* date_str, time_t* out_time);

#endif