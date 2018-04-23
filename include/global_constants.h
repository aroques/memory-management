#ifndef GLOBAL_CONSTANTS_H
#define GLOBAL_CONSTANTS_H

#define FIVE_HUNDRED_MILLION 500000000

const unsigned int EXECV_SIZE = 6;
const unsigned int SYSCLOCK_ID_IDX = 1;
const unsigned int PAGE_TBL_ID_IDX = 2;
const unsigned int MEM_MSGBX_ID_IDX = 3;
const unsigned int PID_IDX = 4;

const unsigned int TOTAL_RUNTIME = 2; // seconds
const unsigned int MAX_RUNTIME = 20;  // seconds
const unsigned int MAX_PROC_CNT = 18;

const unsigned int MAX_NS_BEFORE_NEW_PROC = FIVE_HUNDRED_MILLION; // seconds (i.e., 500ms)

#endif
