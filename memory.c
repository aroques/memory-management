#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "helpers.h"

struct MainMemory get_main_memory() {
    struct MainMemory main_mem;
    int i;
    for (i = 0; i < MAIN_MEMORY_SZE; i++) {
        main_mem.memory[i] = 0;
        main_mem.allocated[i] = 0;
        main_mem.dirty[i] = 0;
    }
    return main_mem;
}

void init_page_table(int* page_table, int max_running_procs) {
    int i, total_pages = get_total_pages(max_running_procs);
    for (i = 0; i < total_pages; i++) {
        page_table[i] = 0;
    }
}

struct MemoryStats get_memory_stats() {
    struct MemoryStats stats = {
        .num_memory_accesses = 0,
        .num_seconds = 0,
        .proc_cnt = 0,
        .num_page_faults = 0,
        .num_seg_faults = 0,
        .total_mem_access_time = 0
    };

    return stats;
}


void print_statistics(FILE* fp, struct MemoryStats stats) {
    char buffer[1000];

    // Calculate
    float mem_access_per_ms = stats.num_memory_accesses / (stats.num_seconds * 1000);
    float page_fault_per_access = stats.num_page_faults / (float) stats.num_memory_accesses;
    float avg_mem_access_speed = stats.total_mem_access_time / (float) stats.num_memory_accesses;
    float throughput = stats.proc_cnt / stats.num_seconds;

    // Print
    sprintf(buffer, "Statistics\n");
    sprintf(buffer + strlen(buffer), "  %-23s: %'d\n", "Memory Accesses", stats.num_memory_accesses);
    sprintf(buffer + strlen(buffer), "  %-23s: %.2f\n", "Memory Accesses/Millisecond", mem_access_per_ms);
    sprintf(buffer + strlen(buffer), "  %-23s: %.2f\n", "Page Faults/Memory Access", page_fault_per_access);
    sprintf(buffer + strlen(buffer), "  %-23s: %.2f ms\n", "Avg Memory Access Speed", avg_mem_access_speed); 
    sprintf(buffer + strlen(buffer), "  %-23s: %'d\n", "Segmentation Faults", stats.num_seg_faults);
    sprintf(buffer + strlen(buffer), "  %-23s: %.2f processes/sec\n", "Throughput", throughput);
    sprintf(buffer + strlen(buffer), "\n");
    
    print_and_write(buffer, fp);
}

int get_total_pages(int max_running_procs) {
    return max_running_procs * PROCESS_PAGES;
}