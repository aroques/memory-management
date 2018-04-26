#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <sys/time.h>
#include <locale.h>
#include <signal.h>

#include "global_constants.h"
#include "helpers.h"
#include "message_queue.h"
#include "shared_memory.h"
#include "clock.h"
#include "memory.h"

bool will_terminate();
bool read_memory();
void add_signal_handlers();
void handle_sigterm(int sig);
int get_random_valid_page_number(int page_tbl_start_idx);
int get_invalid_page_number(int page_tbl_start_idx);
int get_page_number(int page_tbl_start_idx);
void send_termination_notification(int mem_msg_box_id, int pid);
int get_num_mem_references();

void get_reference_type(char* reference_type);

const unsigned int CHANCE_TERMINATE = 50;
const unsigned int CHANCE_SEG_FAULT = 4;
const unsigned int CHANCE_READ = 70;

int main (int argc, char *argv[]) {
    add_signal_handlers();
    srand(time(NULL) ^ getpid());
    setlocale(LC_NUMERIC, "");      // For comma separated integers in printf

    // Get shared memory IDs
    int sysclock_id = atoi(argv[SYSCLOCK_ID_IDX]);
    int page_tbl_id = atoi(argv[PAGE_TBL_ID_IDX]);
    int mem_msg_box_id = atoi(argv[MEM_MSGBX_ID_IDX]);
    int out_msg_box_id = atoi(argv[OUT_MSGBX_ID_IDX]);
    int pid = atoi(argv[PID_IDX]);

    // Attach to shared memory
    struct clock* sysclock = attach_to_shared_memory(sysclock_id, 1);
    int* page_table = attach_to_shared_memory(page_tbl_id, 0);

    // Declare variables
    int page_tbl_start_idx = pid * PROCESS_PAGES;
    int references_before_terminating = get_num_mem_references();
    int num_requests = 0;
    int page_number;
    struct msgbuf mem_msg_box, out_msg_box;
    char reference_type[6];

    while (1) {
        // Determine whether memory will be read from or written to
        get_reference_type(reference_type);
        
        page_number = get_page_number(page_tbl_start_idx);

        // Create message for OSS
        sprintf(mem_msg_box.mtext, "%d,%s,%d", pid, reference_type, page_number);

        // Send message to OSS
        send_msg(mem_msg_box_id, &mem_msg_box, pid);
        
        // Blocking Receive - wait until OSS grants request
        receive_msg(out_msg_box_id, &out_msg_box, pid);
        
        if (num_requests == references_before_terminating) {
            // Check to see if process will terminate
            if (will_terminate()) {
                send_termination_notification(mem_msg_box_id, pid);
                break;
            }
            references_before_terminating += get_num_mem_references();
        }

        num_requests++;
    }

    return 0;  
}

bool will_seg_fault() {
    return event_occured(CHANCE_SEG_FAULT);
}

bool will_terminate() {
    return event_occured(CHANCE_TERMINATE);
}

bool read_memory() {
    return event_occured(CHANCE_READ);
}

void add_signal_handlers() {
    struct sigaction act;
    act.sa_handler = handle_sigterm; // Signal handler
    sigemptyset(&act.sa_mask);      // No other signals should be blocked
    act.sa_flags = 0;               // 0 so do not modify behavior
    if (sigaction(SIGTERM, &act, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }
}

void handle_sigterm(int sig) {
    //printf("USER %d: Caught SIGTERM %d\n", getpid(), sig);
    _exit(0);
}

int get_random_valid_page_number(int page_tbl_start_idx) {
    int random_offset = rand() % PROCESS_PAGES;
    return page_tbl_start_idx + random_offset;
}

int get_invalid_page_number(int page_tbl_start_idx) {
    int random_offset = rand() % PROCESS_PAGES;
    return page_tbl_start_idx - random_offset;
}

int get_page_number(int page_tbl_start_idx) {
    if (will_seg_fault()) {
        return get_invalid_page_number(page_tbl_start_idx);
    }
    else {
        return get_random_valid_page_number(page_tbl_start_idx);
    }
}

void get_reference_type(char* reference_type) {
    if (read_memory()) {
        sprintf(reference_type, "READ");
    } 
    else { // write memory
        sprintf(reference_type, "WRITE");
    } 
}

void send_termination_notification(int mem_msg_box_id, int pid) {
    struct msgbuf mem_msg_box;
    sprintf(mem_msg_box.mtext, "%d,TERM,0", pid);
    send_msg(mem_msg_box_id, &mem_msg_box, pid); 
}

int get_num_mem_references() {
    int base = 900;
    int random_offset = rand() % 200;
    return base + random_offset;
}