#include "kernel.h"
#include "kernel-system.h"

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>     //
#include <stdlib.h>    // malloc, free
#include <sys/time.h>  // setitimer
#include <ucontext.h>  // getcontext, makecontext, setcontext, swapcontext
#include <valgrind/valgrind.h>

#include "../fs/pennfat.h"
#include "../fs/system.h"
#include "../macros.h"
#include "../p_errno.h"
#include "log.h"
#include "parser.h"
#include "penn-shell.h"
#include "queue.h"
#include "scheduler.h"
#include "user.h"
#include "builtins.h"

int ticks = 0;
ucontext_t mainContext;
ucontext_t schedulerContext;
ucontext_t shellContext;
// static ucontext_t testContext, testContext2, testContext3;
ucontext_t *activeContext = &schedulerContext;
struct Pcb *foregroundContextPcb;
struct Pcb *idlePcb;
struct ucontext_t idleContext;
struct Pcb *activeContextPcb = NULL;
struct Queue *blocked, *stopped;
extern fileSystem *currFS;
extern bool isMounted;
extern filell *fdList;
char *logFile = "log/log";

void initKernelQueues() {
    blocked = malloc(sizeof(struct Queue));
    stopped = malloc(sizeof(struct Queue));
    init(blocked);
    init(stopped);
}

static void freeStacks(void) { free(schedulerContext.uc_stack.ss_sp); }

Pcb *findProcess(pid_t pid) {
    Pcb *process = NULL;
    // find pcb with pid
    process = findNodeInScheduler(pid);
    if (process == NULL) {
        process = searchBlockedAndStopped(pid);
    }
    return process;
}

void add_to_parent_zombie(Pcb *process) {
    Pcb *parent = findProcess(process->parent_id);
    logToFile(process->process_name, "ZOMBIE", process->priority,
              process->process_id);
    if (parent != NULL) {
        // if process was already in zombie queue remove it then add it again
        removeQueueNode(parent->zombie_queue, process->process_id);
        enqueue(parent->zombie_queue, process, process->process_id);
    } else {
        p_errno = p_PNOTF;
    }
}

void add_to_parent_changed_children(Pcb *process) {
    Pcb *parent = findProcess(process->parent_id);
    if (parent != NULL) {
        // if process was already in changed child queue remove it then add it
        // again
        removeQueueNode(parent->changed_children, process->process_id);
        enqueue(parent->changed_children, process, process->process_id);
    } else {
        p_errno = p_NOTF;
    }
}
// void check_zombie(){
//     // check all processes, if zombie finished is exited or signaled then add
//     to parent zombie queue and remove from scheduler Pcb *process = NULL;

// }

void check_blocked() {
    struct QueueNode *curr = blocked->head;
    while (curr != NULL) {              // iterate through the blocked queue
        if (curr->Pcb->waiting != 0) {  // if the process is waiting
            if (curr->Pcb->waiting > 0) {
                struct Pcb *zombie =
                    findQueueNode(curr->Pcb->zombie_queue, curr->Pcb->waiting);
                if (zombie != NULL) {
                    curr->Pcb->waiting = 0;
                    // remove from blocked queue
                    removeQueueNode(blocked, curr->pid);
                    logToFile(curr->Pcb->process_name, "UNBLOCKED",
                              curr->Pcb->priority, curr->Pcb->process_id);
                    // add to scheduler
                    addToScheduler(curr->Pcb);
                }
                struct Pcb *changed = findQueueNode(curr->Pcb->changed_children,
                                                    curr->Pcb->waiting);
                if (changed != NULL) {
                    curr->Pcb->waiting = 0;
                    // remove from blocked queue
                    removeQueueNode(blocked, curr->pid);
                    logToFile(curr->Pcb->process_name, "UNBLOCKED",
                              curr->Pcb->priority, curr->Pcb->process_id);
                    // add to scheduler
                    addToScheduler(curr->Pcb);
                }
            } else {  // waiting -1 meaning we will wait for any child to finish
                if (curr->Pcb->zombie_queue->size > 0) {
                    curr->Pcb->waiting = 0;
                    removeQueueNode(blocked, curr->pid);
                    fflush(stdout);
                    logToFile(curr->Pcb->process_name, "UNBLOCKED",
                              curr->Pcb->priority, curr->Pcb->process_id);
                    // add to scheduler
                    addToScheduler(curr->Pcb);
                    fflush(stdout);
                } else if (curr->Pcb->changed_children->size > 0) {
                    curr->Pcb->waiting = 0;
                    removeQueueNode(blocked, curr->pid);
                    logToFile(curr->Pcb->process_name, "UNBLOCKED",
                              curr->Pcb->priority, curr->Pcb->process_id);
                    // add to scheduler
                    addToScheduler(curr->Pcb);
                }
            }
        } else if (curr->Pcb->sleeping != 0) {
            if (curr->Pcb->sleeping <= ticks) {
                curr->Pcb->sleeping = 0;
                // remove from blocked queue
                removeQueueNode(blocked, curr->pid);
                logToFile(curr->Pcb->process_name, "UNBLOCKED",
                          curr->Pcb->priority, curr->Pcb->process_id);
                // add to scheduler
                addToScheduler(curr->Pcb);
            }
        }
        curr = curr->next;
    }
}

static void alarmHandler(int signum)  // SIGALRM
{
    check_blocked();
    //logClickTick();
    ticks++;
    if (activeContextPcb == idlePcb) {
        setcontext(&schedulerContext);
    } else {
        swapcontext(activeContextPcb->context, &schedulerContext);
    }
}

void blockAlarmHandler(int signum) { return; }

void blockAlarm(void) {
    struct sigaction act;
    act.sa_handler = blockAlarmHandler;
    act.sa_flags = SA_RESTART;
    sigfillset(&act.sa_mask);
    sigaction(SIGALRM, &act, NULL);
}

void setAlarmHandler(void) {
    struct sigaction act;
    act.sa_handler = alarmHandler;
    act.sa_flags = SA_RESTART;
    sigfillset(&act.sa_mask);
    sigaction(SIGALRM, &act, NULL);
}

static void setTimer(void) {
    struct itimerval it;
    it.it_interval = (struct timeval){.tv_usec = CENTISECOND * 10};
    it.it_value = it.it_interval;
    setitimer(ITIMER_REAL, &it, NULL);
}

#include <valgrind/valgrind.h>

// also taken from sched-demo.c
static void setStack(stack_t *stack) {
    void *sp = malloc(8 * SIGSTKSZ);
    VALGRIND_STACK_REGISTER(sp, sp + 8 * SIGSTKSZ);
    *stack = (stack_t){.ss_sp = sp, .ss_size = 8 * SIGSTKSZ};
}

void makeContextArguments(ucontext_t *ucp, void (*func)(), char *argv[],
                          int argc) {
    getcontext(ucp);
    sigemptyset(&ucp->uc_sigmask);
    setStack(&ucp->uc_stack);
    ucp->uc_link = &mainContext;  // NULL;
    makecontext(ucp, func, 2, argv, argc);
}

void makeContextNoArguments(ucontext_t *ucp, void (*func)()) {
    getcontext(ucp);
    sigemptyset(&ucp->uc_sigmask);
    setStack(&ucp->uc_stack);
    ucp->uc_link = &mainContext;
    makecontext(ucp, func, 0);
}

void enqueueBlocked(Pcb *pcb) {
    logToFile(pcb->process_name, "BLOCKED", pcb->priority, pcb->process_id);
    enqueue(blocked, pcb, pcb->process_id);
}

void enqueueStopped(Pcb *pcb) { enqueue(stopped, pcb, pcb->process_id); }

/*
 * Essentially the same code that gets called in main but I put it
 * here so that we dont have to do weird context switching in kill
 */
void processFinishedCleanup(Pcb *pcb) {
    removeFromScheduler(pcb);
    removeQueueNode(blocked, pcb->process_id);
    if (pcb == foregroundContextPcb) {
        foregroundContextPcb = NULL;
    }
    if (pcb->finished == RUNNING) {
        pcb->finished = EXITED;
    }
    if (pcb->finished == EXITED || pcb->finished == SIGNALED) {
        add_to_parent_zombie(pcb);
    } else if (pcb->finished == STOPPED) {
        add_to_parent_changed_children(pcb);
    }
}

/**
 * removes a process from the kernel, wherever it is
 * e.g. it might be in the scheduler queues or the blocked/stopped queues
 */
int removeProcess(Pcb *process) {
    Pcb *blockedRemove = removeQueueNode(blocked, process->process_id);
    Pcb *stoppedRemove = removeQueueNode(stopped, process->process_id);
    bool schedulerRemove = removeFromScheduler(process);
    if (blockedRemove != NULL || stoppedRemove != NULL || schedulerRemove) {
        return 0;
    } else {
        return -1;
    }
}

Pcb *searchBlockedAndStopped(pid_t pid) {
    struct Pcb *pcb = findQueueNode(blocked, pid);
    if (pcb == NULL) {
        pcb = findQueueNode(stopped, pid);
    }
    return pcb;
}

Pcb *removeFromBlocked(pid_t pid) {
    struct Pcb *pcb = removeQueueNode(blocked, pid);
    if (pcb == NULL) {
        p_errno = p_NOTF;
    }
    return pcb;
}

void idle(void) {
    while (1) {
        sigset_t sigset;
        sigemptyset(&sigset);
        sigsuspend(&sigset);
    }
}

void infinite(void) {
    while (1) {
    }
}

void sigIntHandler(int signum) {;
    if (signum == SIGINT) {
        if (foregroundContextPcb != NULL &&
            foregroundContextPcb->context != &shellContext) {
            k_process_kill(foregroundContextPcb, S_SIGTERM);
            foregroundContextPcb = NULL;
        }
    }
}

void sigTstpHandler(int signum) {
    if (signum == SIGTSTP) {
        if (foregroundContextPcb != NULL &&
            foregroundContextPcb->context != &shellContext) {
            reset_p_errno();
            k_process_kill(foregroundContextPcb, S_SIGSTOP);
            if (p_errno != 0) {
                p_perror("SIGTSTP handler");
                // exit(EXIT_FAILURE);
            }
        }
    }
}

int main(int argc, char *argv[]) {
    currFS = calloc(1, sizeof(fileSystem));
    isMounted = false;
    if (argc < 2) {
        fprintf(stderr, "pennos: please mount a file system!\n");
        // exit(EXIT_FAILURE);
    } else if (argc > 3) {
        fprintf(stderr, "pennos: too many arguments\n");
        exit(EXIT_FAILURE);
    } else if (argc == 2) {
        mount(argv[1]);
    }
    if (argc == 3) {
        logFile = argv[2];
    }

    signal(SIGINT, sigIntHandler);  // Ctrl-C
    // signal(SIGQUIT, SIG_IGN);  /* Ctrl-\ */
    signal(SIGTSTP, sigTstpHandler);  // Ctrl-Z
    initKernelQueues();
    initSchedulerQueues();
    fdList = createFD(F_STDIN, F_APPEND, 0);
    filell *f_stdout = createFD(F_STDOUT, F_APPEND, 0);
    filell *f_stderr = createFD(F_STDERR, F_APPEND, 0);
    fdList->next = f_stdout;
    f_stdout->prev = fdList;
    f_stdout->next = f_stderr;
    f_stderr->prev = f_stdout;

    FILE *fp = fopen(logFile, "w");
    fclose(fp);

    mainContext = *(ucontext_t *)malloc(sizeof(ucontext_t));
    schedulerContext = *(ucontext_t *)malloc(sizeof(ucontext_t));
    makeContextNoArguments(&schedulerContext, scheduler);
    shellContext = *(ucontext_t *)malloc(sizeof(ucontext_t));
    idleContext = *(ucontext_t *)malloc(sizeof(ucontext_t));
    makeContextNoArguments(&idleContext, idle);
    makeContextNoArguments(&shellContext, shell);

    idlePcb = (Pcb *)malloc(sizeof(Pcb));
    idlePcb->context = &idleContext;
    idlePcb->process_id = 0;
    idlePcb->parent_id = 0;
    idlePcb->process_name = "idle";
    idlePcb->priority = 0;

    Pcb *shellPcb = malloc(sizeof(Pcb));
    shellPcb->context = &shellContext;
    shellPcb->process_id = 1;
    shellPcb->process_name = "shell";
    shellPcb->priority = -1;
    shellPcb->f_stdin = 0;
    shellPcb->f_stdout = 1;
    shellPcb->parent_id = 0;
    shellPcb->child_queue = malloc(sizeof(struct Queue));
    init(shellPcb->child_queue);
    shellPcb->zombie_queue = malloc(sizeof(struct Queue));
    init(shellPcb->zombie_queue);
    shellPcb->changed_children = malloc(sizeof(struct Queue));
    init(shellPcb->changed_children);
    addToScheduler(shellPcb);

    setAlarmHandler();
    setTimer();
    while (1) {
        swapcontext(&mainContext, &schedulerContext);
        blockAlarm();
        // we will return here if a process finishes which means we need to
        // clean up. just dequeue from every queue idk)
        removeFromScheduler(activeContextPcb);
        removeQueueNode(blocked, activeContextPcb->process_id);
        if (activeContextPcb == foregroundContextPcb) {
            foregroundContextPcb = NULL;
        }
        if (activeContextPcb->finished == RUNNING) {
            activeContextPcb->finished = EXITED;
        }
        if (activeContextPcb->finished == EXITED ||
            activeContextPcb->finished == SIGNALED) {
            add_to_parent_zombie(activeContextPcb);
        } else if (activeContextPcb->finished == STOPPED) {
            add_to_parent_changed_children(activeContextPcb);
        }
        setAlarmHandler();
    }
    freeStacks();
}
