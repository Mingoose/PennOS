#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <ucontext.h>
#include <unistd.h>

#include "kernel.h"
#include "log.h"
#include "queue.h"

#define STACKSIZE 4096  // ok this was an issue for some reason
#define SCHEDULE_CYCLE_LENGTH 19

const int SCHEDULE_ORDER[] = {-1, -1, 0,  1,  0, -1, -1, 1,  0, -1,
                              0,  1,  -1, -1, 0, 1,  -1, -1, 0};

struct Queue *highest, *medium, *lowest;

// for testing
void printQueues(void) {
    printf("highest size: %d medium size: %d lowest size: %d\n", highest->size,
           medium->size, lowest->size);
}

/**
 * This function implements the main logic for the scheduler.
 */
void scheduler(void) {
    static int idx = -1;
    extern struct Pcb *idlePcb;
    extern struct Pcb *activeContextPcb;
    while (1) {
        blockAlarm();
        idx++;
        if (idx == SCHEDULE_CYCLE_LENGTH) {
            idx = 0;
        }
        int currQueue = SCHEDULE_ORDER[idx];
        // need to incorporate the idle function later maybe
        if (highest->size == 0 && medium->size == 0 && lowest->size == 0) {
            // logToFile(idlePcb->process_name, "SCHEDULE", idlePcb->priority,
            //           idlePcb->process_id);
            activeContextPcb = idlePcb;
            setAlarmHandler();
            setcontext(activeContextPcb->context);
        }
        struct Queue *curr;
        if (currQueue == -1) {
            curr = highest;
        } else if (currQueue == 0) {
            curr = medium;
        } else {
            curr = lowest;
        }

        if (curr->size == 0) {
            continue;
        }
        Pcb *p = dequeue(curr);
        activeContextPcb = p;
        enqueue(curr, p, p->process_id);
        logToFile(p->process_name, "SCHEDULE", p->priority, p->process_id);
        setAlarmHandler();
        setcontext(activeContextPcb->context);
        perror("setcontext");
    }
    exit(EXIT_FAILURE);
}

void initSchedulerQueues() {
    highest = malloc(sizeof(struct Queue));
    medium = malloc(sizeof(struct Queue));
    lowest = malloc(sizeof(struct Queue));
    init(highest);
    init(medium);
    init(lowest);
}

/**
 * This function adds a process to its corresponding queue in the scheduler.
 */
void addToScheduler(struct Pcb *pcb) {
    //printf("adding to scheduler\n");
    if (pcb->priority == -1) {
        //printf("added something to scheduler high\n");
        enqueue(highest, pcb, pcb->process_id);
    } else if (pcb->priority == 0) {
        //printf("added something to scheduler medium\n");
        enqueue(medium, pcb, pcb->process_id);
        // printf("medium size: %d\n", medium->size);
    } else {
        //printf("added something to scheduler low");
        enqueue(lowest, pcb, pcb->process_id);
    }
}

/**
 * This function removes a process from consideration by the scheduler.
 * might need to change this so that it includes blocked and stopped
 */
bool removeFromScheduler(struct Pcb *pcb) {
    fflush(stdout);
    Pcb *result = NULL;
    if (pcb->priority == -1) {
        result = removeQueueNode(highest, pcb->process_id);
    } else if (pcb->priority == 0) {
        result = removeQueueNode(medium, pcb->process_id);
    } else {
        result = removeQueueNode(lowest, pcb->process_id);
    }
    return result != NULL;
}

/**
 * This function finds the pid of a given pid in the scheduler and returns the
 * Pcb struct associated with it.
 * @return pointer to the pcb, or NULL if not found
 */
struct Pcb *findNodeInScheduler(pid_t pid) {
    Pcb *process = NULL;
    process = findQueueNode(highest, pid);
    if (process != NULL) {
        return process;
    }
    process = findQueueNode(medium, pid);
    if (process != NULL) {
        return process;
    }
    process = findQueueNode(lowest, pid);
    if (process != NULL) {
        return process;
    }
    return NULL;
}
