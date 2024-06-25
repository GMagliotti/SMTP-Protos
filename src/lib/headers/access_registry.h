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
void print_access_registry(char* buf, int buf_size);
void print_mails(char* buf, int buf_size, char* user);
void print_mails_by_time(char* buf, int buf_size, time_t start, time_t end);
void print_mails_by_day(char* buf, int buf_size, time_t day, const char* user);
bool authenticate(char* pwd);
bool is_user(char* user);
bool convert_and_validate_date(char* date_str, time_t* out_time);

#endif