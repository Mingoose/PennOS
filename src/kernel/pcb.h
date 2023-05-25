#pragma once
#include <unistd.h>

#include "../macros.h"
#include "queue.h"

/*
A PCB is a structure that describes all the needed
information about a running process (or thread).
One of the entries will clearly be the ucontext
information, but additionally, you will need to
store the thread’s process id, parent process
id, children process ids, open file descriptors, priority level, etc.
*/
typedef struct Pcb {
    // need to figure out ucontext stuff
    int process_id;
    pid_t parent_id;
    int f_stdin;
    int f_stdout;
    char* process_name;
    // add more file descriptors as needed
    int priority;  // 3 levels: -1, 0, 1. -1 being the highest priority
    struct Queue* zombie_queue;
    struct Queue* changed_children;
    struct Queue* child_queue;
    struct ucontext_t* context;
    int waiting;   // pid of process that this process is waiting on
    int sleeping;  // timestamp when this process will stop sleeping
    int finished;
} Pcb;
