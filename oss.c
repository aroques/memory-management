#include <stdio.h>
#include <sys/wait.h>
#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <locale.h>
#include <sys/time.h>
#include <time.h>
#include <sys/queue.h>
#include <math.h>
#include <errno.h>

#include "global_constants.h"
#include "helpers.h"
#include "shared_memory.h"
#include "message_queue.h"
#include "queue.h"
#include "memory.h"

void wait_for_all_children();
void add_signal_handlers();
void handle_sigint(int sig);
void handle_sigalrm(int sig);
void cleanup_and_exit();
void fork_child(char** execv_arr, unsigned int pid);
struct clock get_time_to_fork_new_proc(struct clock sysclock);
unsigned int get_nanoseconds();
int get_available_pid();
struct message parse_msg(char* mtext);
unsigned int get_work();
void print_blocked_queue();
float percent(int numerator, int denominator);
void init_childpid_array();
void open_file_for_writing(char* filename);
struct MainMemory get_main_memory();
void print_exit_reason(int proc_cnt);
int get_request_time(struct message msg);
bool page_fault(int frame_number);

#define FIFTEEN_MILLION 15000000

// Globals used in signal handler
int max_running_procs;
int simulated_clock_id, page_tbl_id, mem_msg_box_id;
int* page_table;
struct clock* sysclock;
int cleaning_up = 0;
pid_t* childpids;
FILE* fp;

struct message {
    int pid;
    char txt[6];
    int page;
};


int main (int argc, char* argv[]) {
    const unsigned int TOTAL_RUNTIME = 2;       // Max seconds oss should run for

    set_timer(MAX_RUNTIME);                     // Set timer that triggers SIGALRM
    add_signal_handlers();          
    setlocale(LC_NUMERIC, "");                  // For comma separated integers in printf
    srand(time(NULL) ^ getpid());
    
    // Start keeping track of how long program is running
    struct timeval tv_start, tv_stop;           // Used to calculated real elapsed time
    gettimeofday(&tv_start, NULL);

    // Get command line arg
    max_running_procs = parse_cmd_line_args(argc, argv);
    unsigned int elapsed_seconds = 0;           // Holds total real seconds the program has run

    /*
     *  Declare variables used in main loop
     */
    int i, pid = 0, num_messages;                        // Used in various scopes throughout main loop
    char buffer[255];                                // Used to hold output that will be printed and written to log file
    int proc_cnt = 0;                                // Holds total number of active child processes
    struct clock time_to_fork = get_clock();         // Holds time to schedule new process
    struct msqid_ds msgq_ds;                         // Used to check number of messages in msg box
    struct MainMemory main_mem = get_main_memory();  // Simulated main memory
    struct MemoryStats stats = get_memory_stats();   // Used to report statistics
    int frame_number, request_time;
    struct clock time_unblocked[max_running_procs];         // Holds clock of time process i was blocked
    for (i = 0; i < max_running_procs; i++) {
        time_unblocked[i] = get_clock();
    }
    struct Queue blocked;
    init_queue(&blocked);

    // Setup execv array to pass initial data to children processes
    char* execv_arr[EXECV_SIZE];                
    execv_arr[0] = "./user";
    execv_arr[EXECV_SIZE - 1] = NULL;
    
    /*
     *  Setup shared memory
     */
    // Shared logical clock
    simulated_clock_id = get_shared_memory();
    sysclock = (struct clock*) attach_to_shared_memory(simulated_clock_id, 0);
    reset_clock(sysclock);
    
    // Shared Page Table
    page_tbl_id = get_shared_memory();
    page_table = (int*) attach_to_shared_memory(page_tbl_id, 0);
    init_page_table(page_table, max_running_procs);

    // Shared message box for user processes to request read/write memory 
    mem_msg_box_id = get_message_queue();
    struct msgbuf mem_msg_box;
    struct message msg;
    
    // Initialize childpid array
    init_childpid_array();

    // Open log file for writing
    open_file_for_writing("./oss.log");

    // Get a time to fork first process at
    time_to_fork = get_time_to_fork_new_proc(*sysclock);
    
    // Increment current time so it is time to fork a user process
    *sysclock = time_to_fork;

    /*
     *  Main loop
     */
    while ( elapsed_seconds < TOTAL_RUNTIME || proc_cnt < 100) {
        // Check if it is time to fork a new user process
        if (compare_clocks(*sysclock, time_to_fork) >= 0 && proc_cnt < max_running_procs) {
            // Fork a new process
            pid = get_available_pid();
            
            fork_child(execv_arr, pid);
            proc_cnt++;
            
            sprintf(buffer, "OSS: Generating P%d at time %ld:%'ld\n",
                pid, sysclock->seconds, sysclock->nanoseconds);
            print_and_write(buffer, fp);

            time_to_fork = get_time_to_fork_new_proc(*sysclock);
        }

        // Get number of messages
        msgctl(mem_msg_box_id, IPC_STAT, &msgq_ds);
        num_messages = msgq_ds.msg_qnum;

        // Check for any messages
        while (num_messages > 0) {
            printf("found a message\n");
            
            receive_msg(mem_msg_box_id, &mem_msg_box, 0);
            msg = parse_msg(mem_msg_box.mtext);
            
            pid = msg.pid;
            request_time = get_request_time(msg);

            if (strcmp(msg.txt, "TERM") != 0) {
                if (!page_number_is_valid(pid, msg.page)) {
                    // seg fault
                    // so temrinatate/kill process and free frames
                }

                frame_number = get_frame_from_main_memory(main_mem.memory, msg.page);
                
                if (page_fault(frame_number)) {
                    // Page fault
                    // so add process to blocked queue
                    time_unblocked[pid] = *sysclock;
                    increment_clock(&time_unblocked[pid], FIFTEEN_MILLION);
                    enqueue(&blocked, pid);
                }
                else {
                    // Valid
                    sprintf(buffer, "OSS: Granting P%d %s access on page %d at time %ld:%'ld\n",
                        pid, msg.txt, msg.page, sysclock->seconds, sysclock->nanoseconds);
                    print_and_write(buffer, fp);
                    increment_clock(sysclock, request_time);
                }
            }
            else {
                // Process terminated
                sprintf(buffer, "OSS: Acknowledging P%d terminated at time %ld:%'ld\n",
                    pid, sysclock->seconds, sysclock->nanoseconds);
                print_and_write(buffer, fp);
                // Free page numbers in main memory and frame numbers in page table
                free_frames(main_mem.memory, page_table, pid);
                
                // Free space in childpids array
                childpids[pid] = 0;
            }

            sprintf(buffer, "\n");
            print_and_write(buffer, fp);

            // Increment clock slightly whenever a resource is granted or released
            increment_clock(sysclock, get_work());
        }

        increment_clock(sysclock, get_nanoseconds());

        // Calculate total elapsed real-time seconds
        gettimeofday(&tv_stop, NULL);
        elapsed_seconds = tv_stop.tv_sec - tv_start.tv_sec;
    }

    print_exit_reason(proc_cnt);
    print_statistics(fp, stats);

    cleanup_and_exit();

    return 0;
}

void fork_child(char** execv_arr, unsigned int pid) {
    if ((childpids[pid] = fork()) == 0) {
        // Child so...
        char clock_id[10];
        char pg_tbl_id[10];
        char msgbox_id[10];
        char p_id[5];
        
        sprintf(clock_id, "%d", simulated_clock_id);
        sprintf(pg_tbl_id, "%d", page_tbl_id);
        sprintf(msgbox_id, "%d", mem_msg_box_id);
        sprintf(p_id, "%d", pid);
        
        execv_arr[SYSCLOCK_ID_IDX] = clock_id;
        execv_arr[PAGE_TBL_ID_IDX] = pg_tbl_id;
        execv_arr[MEM_MSGBX_ID_IDX] = msgbox_id;
        execv_arr[PID_IDX] = p_id;

        execvp(execv_arr[0], execv_arr);

        perror("Child failed to execvp the command!");
        exit(1);
    }

    if (childpids[pid] == -1) {
        perror("Child failed to fork!\n");
        exit(1);
    }
}

void wait_for_all_children() {
    pid_t pid;
    printf("OSS: Waiting for all children to exit\n");
    fprintf(fp, "OSS: Waiting for all children to exit\n");
    
    while ((pid = wait(NULL))) {
        if (pid < 0) {
            if (errno == ECHILD) {
                perror("wait");
                break;
            }
        }
    }
}

void terminate_children() {
    printf("OSS: Sending SIGTERM to all children\n");
    fprintf(fp, "OSS: Sending SIGTERM to all children\n");
    int i;
    for (i = 1; i <= max_running_procs; i++) {
        if (childpids[i] == 0) {
            continue;
        }
        if (kill(childpids[i], SIGTERM) < 0) {
            if (errno != ESRCH) {
                // Child process exists and kill failed
                perror("kill");
            }
        }
    }
    free(childpids);
}

void add_signal_handlers() {
    struct sigaction act;
    act.sa_handler = handle_sigint; // Signal handler
    sigemptyset(&act.sa_mask);      // No other signals should be blocked
    act.sa_flags = 0;               // 0 so do not modify behavior
    if (sigaction(SIGINT, &act, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    act.sa_handler = handle_sigalrm; // Signal handler
    sigemptyset(&act.sa_mask);       // No other signals should be blocked
    if (sigaction(SIGALRM, &act, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }
}

void handle_sigint(int sig) {
    printf("\nOSS: Caught SIGINT signal %d\n", sig);
    fprintf(fp, "\nOSS: Caught SIGINT signal %d\n", sig);
    if (cleaning_up == 0) {
        cleaning_up = 1;
        cleanup_and_exit();
    }
}

void handle_sigalrm(int sig) {
    printf("\nOSS: Caught SIGALRM signal %d\n", sig);
    fprintf(fp, "\nOSS: Caught SIGALRM signal %d\n", sig);
    if (cleaning_up == 0) {
        cleaning_up = 1;
        cleanup_and_exit();
    }

}

void cleanup_and_exit() {
    terminate_children();
    printf("OSS: Removing message queues and shared memory\n");
    fprintf(fp, "OSS: Removing message queues and shared memory\n");
    remove_message_queue(mem_msg_box_id);
    wait_for_all_children();
    cleanup_shared_memory(simulated_clock_id, sysclock);
    cleanup_shared_memory(page_tbl_id, page_table);
    fclose(fp);
    exit(0);
}

struct clock get_time_to_fork_new_proc(struct clock sysclock) {
    unsigned int ns_before_next_proc = rand() % MAX_NS_BEFORE_NEW_PROC; 
    increment_clock(&sysclock, ns_before_next_proc);
    return sysclock;
}

unsigned int get_nanoseconds() {
    return (rand() % 800000) + 10000; // 10,000 - 800,000 inclusive
}

unsigned int get_work() {
    return (rand() % 100000) + 10000; // 10,000 - 100,000 inclusive
}

int get_available_pid() {
    int pid, i;
    for (i = 0; i < max_running_procs; i++) {
        if (childpids[i] > 0) {
            continue;
        }
        pid = i;
        break;
    }
    return pid;
}

struct message parse_msg(char* mtext) {
    // Parse a message sent from a user process
    struct message msg;
    char ** msg_info = split_string(mtext, ",");
    
    msg.pid = atoi(msg_info[0]);
    strcpy(msg.txt, msg_info[1]);
    msg.page = atoi(msg_info[2]);

    free(msg_info);

    return msg;
}


float percent(int numerator, int denominator) {
    return  (numerator / (float) denominator) * 100;
}

void init_childpid_array() {
    int i;
    childpids = malloc(sizeof(pid_t) * max_running_procs);
    for (i = 0; i < max_running_procs; i++) {
        childpids[i] = 0;
    }
}

void open_file_for_writing(char* filename) {
    if ((fp = fopen(filename, "w")) == NULL) {
        perror("fopen");
        exit(1);
    }
}

void print_exit_reason(int proc_cnt) {
    // Print information before exiting
    char reason[100];
    char buffer[100];
    
    if (proc_cnt < 100) {
        sprintf(reason, "because %d seconds have been passed", TOTAL_RUNTIME);
    }
    else {
        sprintf(reason, "because %d processes have been generated", proc_cnt);
    }

    sprintf(buffer, "OSS: Exiting at time %'ld:%'ld %s\n", 
        sysclock->seconds, sysclock->nanoseconds, reason);
    print_and_write(buffer, fp);

    sprintf(buffer, "\n");
    print_and_write(buffer, fp);
}

int get_request_time(struct message msg) {
    if (strcmp(msg.txt, "READ") == 0) {
        return 10; // nanoseconds
    }
    else if (strcmp(msg.txt, "WRITE") == 0) {
        return 20; // nanseconds
    }
    return 0;
}

bool page_fault(int frame_number) {
    if (frame_number < 0) {
        return 1;
    }
    return 0;
}