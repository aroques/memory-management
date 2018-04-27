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
void print_blocked_queue();
float percent(int numerator, int denominator);
void init_childpid_array();
void open_file_for_writing(char* filename);
struct MainMemory get_main_memory();
void print_exit_reason(int proc_cnt);
int get_request_time(char* request_type);
bool page_fault(int frame_number);
void kill_process(int pid);
bool is_unblocked(int pid, struct clock time_unblocked);
struct BlockedInfo get_blocked_info();
int add_page_to_main_memory(struct MainMemory* main_mem, int page_number);

#define FIFTEEN_MILLION 15000000

// Globals used in signal handler
int max_running_procs;
int simulated_clock_id, page_tbl_id, mem_msg_box_id, out_msg_box_id;
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

struct BlockedInfo {
    int page_number;
    struct clock time_unblocked;
    char type_of_request[6];
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
    int elapsed_seconds = 0;           // Holds total real seconds the program has run

    /*
     *  Declare variables used in main loop
     */
    int i, pid = 0, num_messages;                     // Used in various scopes throughout main loop
    char buffer[255];                                // Used to hold output that will be printed and written to log file
    int proc_cnt = 0, total_procs = 0;                   
    struct clock time_to_fork = get_clock();         // Holds time to schedule new process
    struct msqid_ds msgq_ds;                         // Used to check number of messages in msg box
    struct MainMemory main_mem = get_main_memory();  // Simulated main memory
    struct MemoryStats stats = get_memory_stats();   // Used to report statistics
    int frame_number, request_time, page_number;
    struct BlockedInfo blocked_info[max_running_procs];
    for (i = 0; i < max_running_procs; i++) {
        blocked_info[i].time_unblocked = get_clock();
        blocked_info[i].page_number = 0;
    }
    struct BlockedInfo blkd_info;
    struct clock unblocked_time;
    struct clock difference;
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

    out_msg_box_id = get_message_queue();
    struct msgbuf out_msg_box;
    
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
    while ( elapsed_seconds < TOTAL_RUNTIME && proc_cnt < 100) {
        // Check if it is time to fork a new user process
        if (compare_clocks(*sysclock, time_to_fork) >= 0 && proc_cnt < max_running_procs) {
            // Fork a new process
            pid = get_available_pid();
            
            fork_child(execv_arr, pid);
            proc_cnt++;
            total_procs++;
            
            sprintf(buffer, "OSS: Generating P%d at time %ld:%'ld\n",
                pid, sysclock->seconds, sysclock->nanoseconds);
            print_and_write(buffer, fp);

            time_to_fork = get_time_to_fork_new_proc(*sysclock);
        }

        if (!empty(&blocked)) {
            // Check blocked queue
            pid = peek(&blocked);

            // Get blocked info struct from array
            blkd_info = blocked_info[pid];
            unblocked_time = blkd_info.time_unblocked;

            if (is_unblocked(pid, unblocked_time)) {
                // Process is unblocked
                page_number = blkd_info.page_number;

                sprintf(buffer, "\nOSS: 15ms have passed. Unblocking P%d and granting %s access on page %d at time %ld:%'ld.\n",
                    pid, blkd_info.type_of_request, page_number, sysclock->seconds, sysclock->nanoseconds);
                print_and_write(buffer, fp);

                // Add page to main memory
                frame_number = add_page_to_main_memory(&main_mem, page_number);

                // Add frame number to page table
                add_frame_to_page_table(frame_number, page_table, pid);

                // Set second chance bit
                main_mem.second_chance[frame_number] = 1;    

                // Mark frame as dirty if write
                if (strcmp(blkd_info.type_of_request, "WRITE") == 0) {
                    main_mem.dirty[frame_number] = 1;
                }

                // Remove from blocked queue
                dequeue(&blocked);

                // Send message
                send_msg(out_msg_box_id, &out_msg_box, pid);

                // Update stats
                stats.num_memory_accesses++;
    
            }

            // Check if all processes are blocked
            if (count(&blocked) == max_running_procs) {
                
                difference = subtract_clocks(unblocked_time, *sysclock);
                
                // Increment sysclock to unblock 1 process 
                *sysclock = unblocked_time;
                
                sprintf(buffer, "\nOSS: All processes blocked because of page faults. Incrementing clock %ld:%'ld to unblock 1 process at time %ld:%'ld.\n\n",
                    difference.seconds, difference.nanoseconds, sysclock->seconds, sysclock->nanoseconds);
                print_and_write(buffer, fp);
            }
        }
        
        // Get number of messages
        msgctl(mem_msg_box_id, IPC_STAT, &msgq_ds);
        num_messages = msgq_ds.msg_qnum;

        while (num_messages-- > 0) {
            receive_msg(mem_msg_box_id, &mem_msg_box, 0);
            msg = parse_msg(mem_msg_box.mtext);
            
            pid = msg.pid;
            request_time = get_request_time(msg.txt);

            if (strcmp(msg.txt, "TERM") != 0) {
                // Process is requesting memory to be read from or written to
                if (!page_number_is_valid(pid, msg.page)) {
                    // Seg Fault
                    sprintf(buffer, "\nOSS: P%d seg faulted and will be terminated at time %ld:%'ld.\n\n",
                        pid, sysclock->seconds, sysclock->nanoseconds);
                    print_and_write(buffer, fp);
                    
                    // Kill process
                    kill_process(pid);

                    // Free page numbers in main memory and frame numbers in page table
                    free_frames(&main_mem, page_table, pid);

                    // Free space in childpids array
                    childpids[pid] = 0;
                    proc_cnt--;

                    // Update stats
                    stats.num_seg_faults++;

                    // Print memory map
                    print_main_memory(fp, main_mem);
                }
                else {
                    // Page number is valid
                    frame_number = get_frame_from_main_memory(main_mem.memory, msg.page);
                    
                    if (page_fault(frame_number)) {
                        // Page fault
                        sprintf(buffer, "OSS: P%d requested %s access on page %d and page faulted at time %ld:%'ld.\n     Adding process to blocked queue.\n",
                            pid, msg.txt, msg.page, sysclock->seconds, sysclock->nanoseconds);
                        print_and_write(buffer, fp);
                        
                        // Store blocked information in struct
                        blkd_info = get_blocked_info();
                        blkd_info.page_number = msg.page;
                        strcpy(blkd_info.type_of_request, msg.txt);
                        blkd_info.time_unblocked = *sysclock;
                                        
                        // Store blocked information in blocked info array
                        blocked_info[pid] = blkd_info;
                        
                        // Increment clock 15ms
                        increment_clock(&blkd_info.time_unblocked, FIFTEEN_MILLION);   

                        // Add process to blocked queue
                        enqueue(&blocked, pid);
                        
                        // Update stats
                        stats.num_page_faults++;
                        stats.total_mem_access_time += FIFTEEN_MILLION;
                    }
                    else {
                        // Page is in main memory frame already
                        // sprintf(buffer, "OSS: Granting P%d %s access on page %d at time %ld:%'ld\n",
                        //     pid, msg.txt, msg.page, sysclock->seconds, sysclock->nanoseconds);
                        // print_and_write(buffer, fp);
                        
                        // Increment clock and update stats
                        increment_clock(sysclock, request_time);
                        stats.total_mem_access_time += request_time;

                        // Set second chance bit
                        main_mem.second_chance[frame_number] = 1;    

                        // Mark frame as dirty if write
                        if (strcmp(msg.txt, "WRITE") == 0) {
                            main_mem.dirty[frame_number] = 1;
                        }

                        // Send message
                        send_msg(out_msg_box_id, &out_msg_box, pid);

                        // Update stats
                        stats.num_memory_accesses++;
                    }
                }
            }
            else {
                // Process terminated
                sprintf(buffer, "\nOSS: Acknowledging P%d terminated at time %ld:%'ld\n\n",
                    pid, sysclock->seconds, sysclock->nanoseconds);
                print_and_write(buffer, fp);
                
                // Free page numbers in main memory and frame numbers in page table
                free_frames(&main_mem, page_table, pid);
                
                // Free space in childpids array
                childpids[pid] = 0;
                proc_cnt--;

                // Print memory map
                print_main_memory(fp, main_mem);
                
            }
        }

        // Increment clock a bit to simulate time passing
        increment_clock(sysclock, get_nanoseconds());

        // Calculate total elapsed real-time seconds
        gettimeofday(&tv_stop, NULL);
        elapsed_seconds = tv_stop.tv_sec - tv_start.tv_sec;
    }

    print_exit_reason(proc_cnt);

    // Load stats with pertinent information
    stats.num_seconds = clock_to_seconds(*sysclock);
    stats.proc_cnt = total_procs;

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
        char out_msgbox_id[10];
        char p_id[5];
        
        sprintf(clock_id, "%d", simulated_clock_id);
        sprintf(pg_tbl_id, "%d", page_tbl_id);
        sprintf(msgbox_id, "%d", mem_msg_box_id);
        sprintf(out_msgbox_id, "%d", out_msg_box_id);
        sprintf(p_id, "%d", pid);
        
        execv_arr[SYSCLOCK_ID_IDX] = clock_id;
        execv_arr[PAGE_TBL_ID_IDX] = pg_tbl_id;
        execv_arr[MEM_MSGBX_ID_IDX] = msgbox_id;
        execv_arr[OUT_MSGBX_ID_IDX] = out_msgbox_id;
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
    for (i = 0; i < max_running_procs; i++) {
        if (childpids[i] == 0) {
            continue;
        }
        kill_process(i);
    }
    free(childpids);
}

void kill_process(int pid) {
    if (kill(childpids[pid], SIGTERM) < 0) {
        if (errno != ESRCH) {
            // Child process exists and kill failed
            perror("kill");
        }
    }
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
    return (rand() % 50000) + 10000; // 500 - 5,000 inclusive
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

int get_request_time(char* request_type) {
    if (strcmp(request_type, "READ") == 0) {
        return 10; // nanoseconds
    }
    else if (strcmp(request_type, "WRITE") == 0) {
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

bool is_unblocked(int pid, struct clock time_unblocked) {
    if (compare_clocks(*sysclock, time_unblocked) >=  0) {
        return 1;
    }
    return 0;
}

struct BlockedInfo get_blocked_info() {
    struct BlockedInfo binfo = {
        .page_number = 0,
        .time_unblocked = get_clock(),
        .type_of_request = ""
    };
    return binfo;
}

int add_page_to_main_memory(struct MainMemory* main_mem, int page_number) {
    char buffer[256];
    int free_frame_number = get_free_frame_number(main_mem->memory);

    if (main_memory_is_full(free_frame_number)) {
        // Page swap
        free_frame_number = second_chance_page_replacement(main_mem);

        sprintf(buffer, "     Main memory is full so swapping page %d in frame %d with page %d\n\n",
            main_mem->memory[free_frame_number], free_frame_number, page_number);
        print_and_write(buffer, fp);

        if (main_mem->dirty[free_frame_number]) {
            // Swapping out a dirty page
            sprintf(buffer, "     Swapping out a dirty page, so incrementing the clock 15ms to simulate saving contents of the page to disk at time %ld:%'ld.\n\n",
                sysclock->seconds, sysclock->nanoseconds);
            print_and_write(buffer, fp);

            increment_clock(sysclock, FIFTEEN_MILLION);
        }
    }
    else {
        // Just put page in main memory
        sprintf(buffer, "     Main memory is not full so putting page %d in frame %d\n\n",
            page_number, free_frame_number);
        print_and_write(buffer, fp);
    }
    
    main_mem->memory[free_frame_number] = page_number;

    return free_frame_number;
}