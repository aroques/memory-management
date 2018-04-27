#ifndef MEMORY_H
#define MEMORY_H

#include <stdbool.h>
#include <stdio.h>
#include "clock.h"

#define MAIN_MEMORY_SZE 256
#define PROCESS_PAGES 32
#define ONE_MILLION 1000000

struct MainMemory {
    int memory[MAIN_MEMORY_SZE];
    bool second_chance[MAIN_MEMORY_SZE];
    bool dirty[MAIN_MEMORY_SZE];
    int second_chance_ptr;
};

struct MemoryStats {
    int num_memory_accesses;
    int num_page_faults;
    int num_seg_faults;
    int proc_cnt;
    long double num_seconds;
    unsigned long total_mem_access_time;
};

struct MainMemory get_main_memory();
void init_page_table(int* page_table, int max_running_procs);
void print_statistics(FILE* fp, struct MemoryStats stats);
struct MemoryStats get_memory_stats();
int get_total_pages(int max_running_procs);
bool page_number_is_valid(int pid, int page_number);
int get_frame_from_main_memory(int* main_mem, int page_number);
int get_start_index(int pid);
int get_end_index(int start_index);
void free_frames(struct MainMemory* main_mem, int* page_table, int pid);
int get_free_frame_number(int* main_mem);
bool main_memory_is_full(int free_frame_number);
int second_chance_page_replacement(struct MainMemory* main_mem);
void add_frame_to_page_table(int frame_number, int* page_table, int pid);
void print_main_memory(FILE* fp, struct MainMemory main_mem);
void print_frames(int* page_table, int pid);

#endif
