#ifndef MEMORY_H
#define MEMORY_H

#include <stdbool.h>
#include <stdio.h>
#include "clock.h"

#define MAIN_MEMORY_SZE 256
#define PROCESS_PAGES 32

struct MainMemory {
    int memory[MAIN_MEMORY_SZE];
    bool allocated[MAIN_MEMORY_SZE];
    bool dirty[MAIN_MEMORY_SZE];
};

struct MemoryStats {
    int num_memory_accesses;
    int num_page_faults;
    int num_seg_faults;
    int proc_cnt;
    long double num_seconds;
    unsigned int total_mem_access_time;
};

struct MainMemory get_main_memory();
void init_page_table(int* page_table, int max_running_procs);
void print_statistics(FILE* fp, struct MemoryStats stats);
struct MemoryStats get_memory_stats();
int get_total_pages(int max_running_procs);
bool page_number_is_valid(int pid, int page_number);
int get_frame_from_main_memory(struct MainMemory main_mem, int page_number);

#endif
