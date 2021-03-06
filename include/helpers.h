#ifndef HELPERS_H
#define HELPERS_H

#include <stdbool.h>

char** split_string(char* str, char* delimeter);
char* get_timestamp();
void print_usage();
int parse_cmd_line_args(int argc, char* argv[]);
void set_timer(int duration);
bool event_occured(unsigned int pct_chance);
unsigned int** create_array(int m, int n);
void destroy_array(unsigned int** arr);
void print_and_write(char* str, FILE* fp);
bool event_occured_out_of_one_thousand(unsigned int chance);

#endif
