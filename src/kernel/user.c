#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <ucontext.h>

#include "../p_errno.h"
#include "kernel.h"
#include "kernel-system.h"
#include "log.h"
#include "queue.h"
#include "scheduler.h"

/**
 * User function that forks a new
 * thread that retains most of the attributes of the parent thread (see
 * k_process_create). Once the thread is spawned, it executes the function
 * referenced by func with its argument array argv. fd0 is the file descriptor
 * for the input file, and fd1 is the file descriptor for the output file. It
 * returns the pid of the child thread on success, or -1 on error.
 */

pid_t p_spawn(void(*func), char *argv[], int argc, int fd0, int fd1,
              bool foreground) {
    // this will only get called from the shell I think. So we can just assume
    // that the shell is the parent process
    blockAlarm();
    extern Pcb *activeContextPcb;
    Pcb *child = k_process_create(activeContextPcb);
    if (child == NULL) {
        setAlarmHandler();
        return -1;
    }
    child->f_stdin = fd0;
    child->f_stdout = fd1;
    child->process_name = malloc(sizeof(char) * (strlen(argv[0]) + 1));
    if (child->process_name == NULL) {
        setAlarmHandler();
        return -1;
    }
    strcpy(child->process_name,
           argv[0]);  // have to do this because literal strings are not saved

    // set the child's context to the function that we want to run
    ucontext_t *childContext = (ucontext_t *)malloc(sizeof(ucontext_t));
    if (childContext == NULL) {
        setAlarmHandler();
        return -1;
    }
    makeContextArguments(childContext, func, argv, argc);
    child->context = childContext;
    logToFile(argv[0], "CREATE", 0, child->process_id);
    addToScheduler(child);
    if (foreground) {
        foregroundContextPcb = child;
    }
    setAlarmHandler();
    return child->process_id;
}

/**
 * User function that sets the calling thread
 * as blocked (if nohang is false) until a child of the calling thread changes
 * state. It is similar to Linux waitpid(2). If nohang is true, p_waitpid does
 * not block but returns immediately. p_waitpid returns the pid of the child
 * which has changed state on success, or -1 on error. If nohang and no children
 * have been spawned, p_waitpid returns 0.
 */

// right now it seems that the issue is that in recur trying to set *wstatus to
// child->finished is segfaulting because status is not initialized to point
// to anything in recur. This is not an issue in our normal shell when we call
// wait because I initialize status to point to something. We could do that in
// recur but i feel like we shouldn't need to.

pid_t p_waitpid(pid_t pid, int *wstatus, bool nohang) {
    extern Pcb *activeContextPcb;
    if (nohang) {
        if (pid == -1) {  // check all children
            //
            // check if any children are in the zombie queue
            struct Pcb *child;
            if (size(activeContextPcb->zombie_queue) >
                0) {  // process terminated
                // if so, return the pid of the first one
                child = (struct Pcb *)dequeue(activeContextPcb->zombie_queue);
                pid_t newPid = child->process_id;
                if (wstatus != NULL) {
                    *wstatus = child->finished;
                }
                logToFile(child->process_name, "WAITED", child->priority,
                          newPid);
                k_process_cleanup(child);
                return newPid;
            } else {  // process stopped or started
                // check changed children
                if (activeContextPcb->changed_children->size > 0) {
                    child = (struct Pcb *)dequeue(
                        activeContextPcb->changed_children);
                    pid_t newPid = child->process_id;
                    if (wstatus != NULL) {
                        *wstatus = child->finished;
                    }
                    logToFile(child->process_name, "WAITED", child->priority,
                              newPid);
                    return newPid;
                } else if (activeContextPcb->child_queue->size > 0) {
                    // if no children to wait on
                    return 0;
                } else {
                    return -1;
                }
            }
        }
    } else {  // blocking wait
        if (pid > -1) {
            Pcb *process = findNodeInScheduler(pid);
            if (process == NULL) {
                process = searchBlockedAndStopped(pid);
                if (process == NULL) {
                    p_errno = p_NOTF;
                    return -1;
                }
            }
            enqueueBlocked(activeContextPcb);
            removeFromScheduler(activeContextPcb);
            activeContextPcb->waiting = pid;
            extern ucontext_t schedulerContext;
            swapcontext(activeContextPcb->context, &schedulerContext);
            if (wstatus != NULL) {
                *wstatus = process->finished;
            }
            if (process->finished == EXITED || process->finished == SIGNALED) {
                logToFile(process->process_name, "WAITED", process->priority,
                          process->process_id);
                k_process_cleanup(process);
            } else if (process->finished == STOPPED ||
                       process->finished == RUNNING) {
                removeQueueNode(activeContextPcb->changed_children,
                                process->process_id);
            }
            return pid;
            // block the parent until the child is done
            // put the parent in the blocked queue
            // swap back to parents probably
            // loop through all children, first one to not be in a priority
            // queue should be added returned return the pid
        } else {  // pid is -1 so we need to wait on all children
            while (1) {
                if (activeContextPcb->child_queue->size == 0) {
                    return -1;
                }
                if (activeContextPcb->zombie_queue->size > 0) {
                    activeContextPcb->waiting = -1;
                    enqueueBlocked(activeContextPcb);
                    removeFromScheduler(activeContextPcb);
                    extern ucontext_t schedulerContext;
                    swapcontext(activeContextPcb->context, &schedulerContext);
                    Pcb *child = dequeue(
                        activeContextPcb
                            ->zombie_queue);  // activeContextPcb->zombie_queue->head->Pcb;
                    if (wstatus != NULL) {
                        *wstatus = child->finished;
                    }
                    pid_t newPid = child->process_id;
                    logToFile(child->process_name, "WAITED", child->priority,
                              child->process_id);
                    k_process_cleanup(child);
                    return newPid;
                } else if (activeContextPcb->changed_children->size > 0) {
                    activeContextPcb->waiting = -1;
                    enqueueBlocked(activeContextPcb);
                    removeFromScheduler(activeContextPcb);
                    extern ucontext_t schedulerContext;
                    swapcontext(activeContextPcb->context, &schedulerContext);
                    Pcb *child = activeContextPcb->changed_children->head->Pcb;
                    if (wstatus != NULL) {
                        *wstatus = child->finished;
                    }
                    pid_t newPid = child->process_id;
                    logToFile(child->process_name, "WAITED", child->priority,
                              child->process_id);
                    // dont clean up because process is not done
                    return newPid;
                }
            }
            // } else if (activeContextPcb->child_queue->size > 0) {
            //     return 0;
            // } else if (activeContextPcb->child_queue->size ==
            //            0) {  // no children to wait on
            //     return -1;
            // }
        }
    }
    return 0;
}

/**
 * User function that sends the signal sig to the
 * thread referenced by pid.
 * @return 0 on success, -1 on error.
 */
pid_t p_kill(pid_t pid, int sig) {
    Pcb *process;
    // find pcb with pid
    process = findNodeInScheduler(pid);
    if (process == NULL) {
        process = searchBlockedAndStopped(pid);
        if (process == NULL) {
            return -1;
        }
    }
    // call k_process_kill
    reset_p_errno();
    k_process_kill(process, sig);
    if (p_errno != 0) {
        p_perror("p_kill");
    }
    return 0;
}

/**
 * Exits thread unconditionally
 */
void p_exit(void) {
    extern struct ucontext_t mainContext;
    swapcontext(activeContextPcb->context, &mainContext);
}

/**
 * User function that sets the priority of the thread pid to priority
 * @return 0 on success, -1 on error
 */
int p_nice(pid_t pid, int priority) {
    extern struct Queue *highest, *medium, *lowest;
    struct Pcb *pcb = findNodeInScheduler(pid);
    if (pcb == NULL) {
        return -1;
    }
    removeFromScheduler(pcb);
    int oldPriority = pcb->priority;
    logNice(pcb->process_name, oldPriority, priority, pcb->process_id);
    pcb->priority = priority;
    addToScheduler(pcb);
    return 0;
}

/**
 * User function that sets the calling process to blocked until ticks of
 * the system clock elapse, and then sets the thread to running. Importantly,
 * p_sleep should not return until the thread resumes running; however, it can
 * be interrupted by a S_SIGTERM signal. Like sleep(3) in Linux, the clock keeps
 * ticking even when p_sleep is interrupted.
 */
void p_sleep(unsigned int sleep_ticks) {
    blockAlarm();
    extern int ticks;
    extern Pcb *activeContextPcb;
    activeContextPcb->sleeping = ticks + sleep_ticks;
    bool sleepFound = removeFromScheduler(activeContextPcb);
    if (sleepFound == true) {  // sleep was not already in blocked queue
        enqueueBlocked(activeContextPcb);
    }
    extern ucontext_t schedulerContext;
    setAlarmHandler();
    swapcontext(activeContextPcb->context, &schedulerContext);
}
