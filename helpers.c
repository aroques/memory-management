#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>

#include "helpers.h"

char** split_string(char* str, char* delimeter) {
    char** strings = malloc(10 * sizeof(char*));
    char* substr;

    substr = strtok(str, delimeter);

    int i = 0;
    while (substr != NULL)
    {
        strings[i] = substr;
        substr = strtok(NULL, delimeter);
        i++;
    }

    return strings;

}

char* get_timestamp() {
    char* timestamp = malloc(sizeof(char)*10);
    time_t rawtime;
    struct tm* timeinfo;

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    sprintf(timestamp, "%d:%d:%d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    
    return timestamp;
}

int parse_cmd_line_args(int argc, char* argv[]) {
    int option;
    int num_child_procs = 0;
    while ((option = getopt (argc, argv, "hn:")) != -1)
    switch (option) {
        case 'h':
            print_usage();
            break;
        case 'n':
            num_child_procs = atoi(optarg);
            break;
        default:
            print_usage();
    }

    if (num_child_procs == 0) {
        // Default to 8
        num_child_procs = 8;
    }

    if (num_child_procs > 18) {
        // Set max to 18
        num_child_procs = 18;
    }
    
    printf("num child procs set to %d\n", num_child_procs);
    return num_child_procs;
}

void print_usage() {
    fprintf(stderr, "Usage: oss [-n max number of concurrent child processes]\n");
    exit(0);
}

void set_timer(int duration) {
    struct itimerval value;
    value.it_interval.tv_sec = duration;
    value.it_interval.tv_usec = 0;
    value.it_value = value.it_interval;
    if (setitimer(ITIMER_REAL, &value, NULL) == -1) {
        perror("setitimer");
        exit(1);
    }
}

bool event_occured(unsigned int pct_chance) {
    unsigned int percent = (rand() % 100) + 1;
    if (percent <= pct_chance) {
        return 1;
    }
    else {
        return 0;
    }
}

unsigned int** create_array(int m, int n) {
    // Creates a m rows x n column matrix
    unsigned int* values = calloc(m * n, sizeof(unsigned int));
    unsigned int** rows = malloc(m * sizeof(unsigned int*));
    int i;
    for (i = 0; i < m; i++)
    {
        rows[i] = values + (i * n);
    }
    return rows;
}

void destroy_array(unsigned int** arr) {
    free(*arr);
    free(arr);
}

void print_and_write(char* str, FILE* fp) {
    fputs(str, stdout);
    fputs(str, fp);
}