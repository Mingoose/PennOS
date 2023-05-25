#include "kernel-system.h"


#include "kernel.h"
#include "../macros.h"
#include "../p_errno.h"
#include "log.h"
#include "parser.h"
#include "penn-shell.h"
#include "queue.h"
#include "scheduler.h"
#include "user.h"
#include "builtins.h"

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>     //
#include <stdlib.h>    // malloc, free
#include <sys/time.h>  // setitimer
#include <ucontext.h>  // getcontext, makecontext, setcontext, swapcontext
#include <valgrind/valgrind.h>

static int mostRecentPID = 1;
extern int ticks;
extern char *logFile;
extern struct ucontext_t mainContext;
extern struct Pcb *activeContextPcb;
extern struct Pcb *idlePcb;
extern struct ucontext_t schedulerContext;
extern struct Pcb *foregroundContextPcb;
extern struct Queue *blocked, *stopped;

Pcb *k_process_create(Pcb *parent) {
    Pcb *childPcb = (Pcb *)malloc(sizeof(Pcb));
    if (childPcb == NULL) {
        perror("k_process_create");
        return NULL;
    }
    childPcb->process_id = ++mostRecentPID;
    childPcb->parent_id = parent->process_id;
    childPcb->context = parent->context;
    childPcb->process_name = parent->process_name;
    childPcb->priority = 0;
    childPcb->f_stdin = parent->f_stdin;
    childPcb->f_stdout = parent->f_stdout;
    childPcb->zombie_queue = malloc(sizeof(struct Queue));
    if (childPcb->zombie_queue == NULL) {
        perror("k_process_create");
        return NULL;
    }
    init(childPcb->zombie_queue);
    childPcb->child_queue = malloc(sizeof(struct Queue));
    if (childPcb->child_queue == NULL) {
        perror("k_process_create");
        return NULL;
    }
    init(childPcb->child_queue);
    childPcb->changed_children = malloc(sizeof(struct Queue));
    if (childPcb->changed_children == NULL) {
        perror("k_process_create");
        return NULL;
    }
    init(childPcb->changed_children);
    childPcb->waiting = 0;
    childPcb->sleeping = 0;
    childPcb->finished = RUNNING;

    enqueue(parent->child_queue, childPcb, childPcb->process_id);

    return childPcb;
}

void k_process_kill(Pcb *process, int signal) {
    if (signal == S_SIGCONT) {
        if (process->finished == RUNNING) {
            return;
        }
        logToFile(process->process_name, "CONTINUED", process->priority,
                  process->process_id);
        // remove from stopped queue, add back to queue of its priority
        struct Pcb *pcb = removeQueueNode(stopped, process->process_id);
        pcb->finished = RUNNING;
        if (pcb == NULL) {
            p_errno = p_NOTF;
            return;
        }
        // if the process is sleep add to blocked queue
        if (pcb->sleeping != 0) {
            enqueueBlocked(pcb);
        } else {
            addToScheduler(pcb);
        }
        add_to_parent_changed_children(pcb);
    } else if (signal == S_SIGSTOP) {
        // removes process from scheduler and adds to stopped queue
        if (process->finished == STOPPED) {
            return;
        }
        logToFile(process->process_name, "STOPPED", process->priority,
                  process->process_id);
        fprintf(stderr, "stopped: %s\n", process->process_name);

        // need to check if the result of this is true (i.e. actually removed)
        Pcb *pcb = removeQueueNode(blocked, process->process_id);

        if (pcb == NULL) {
            bool found = removeFromScheduler(process);
            if (!found) {
                p_errno = p_NOTF;
                return;
            }
        }

        enqueue(stopped, process, process->process_id);
        process->finished = STOPPED;
        processFinishedCleanup(process);
        if (activeContextPcb == process) {
            swapcontext(process->context, &schedulerContext);
        }
    } else if (signal == S_SIGTERM) {
        logToFile(process->process_name, "SIGNALED", process->priority,
                  process->process_id);
        fileDescriptorCleanup();
        if (activeContextPcb->process_id == 0) {
            // everything is idle then we need to check blocked
            // loop through blocked queue looking for sleep
            struct QueueNode *curr = blocked->head;
            while (curr != NULL) {
                if (curr->Pcb->sleeping != 0) {
                    curr->Pcb->finished = SIGNALED;
                    processFinishedCleanup(process);
                }
                curr = curr->next;
            }
        } else {
            process->finished = SIGNALED;
            processFinishedCleanup(process);
        }
        if (activeContextPcb == process) {
            swapcontext(process->context, &schedulerContext);
        }
    }
}

void k_process_cleanup(Pcb *process) {
    k_process_cleanup_helper(process);
    free(process->context->uc_stack.ss_sp);
    free(process->context);
    free(process->process_name);
    free(process);
}

void k_process_cleanup_helper(Pcb *process) {
    logToFile(process->process_name, "EXITED", process->priority,
              process->process_id);
    struct Pcb *curr = NULL;
    // changed this to dequeue for mem reasons, not sure if it breaks anything
    fflush(stdout);

    Pcb *parent = findProcess(process->parent_id);
    if (parent == NULL) {
        // p_errno = p_PNOTF; // not an error but just an orphan child
        return;
    }

    //!< should we log anything for removing from zombie and child here
    removeQueueNode(parent->child_queue, process->process_id);
    removeQueueNode(parent->zombie_queue, process->process_id);

    while ((curr = dequeue(process->child_queue)) != NULL) {
        fflush(stdout);
        // log
        removeFromScheduler(curr);
        logToFile(curr->process_name, "ORPHAN", curr->priority,
                  curr->process_id);
        k_process_cleanup(curr);
    }
}