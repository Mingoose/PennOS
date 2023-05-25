#pragma once
#include <unistd.h>

#include "pcb.h"

extern int ticks;
extern char *logFile;
extern struct ucontext_t mainContext;
extern struct Pcb *activeContextPcb;
extern struct Pcb *idlePcb;
extern struct ucontext_t schedulerContext;
extern struct Pcb *foregroundContextPcb;
extern struct Queue *blocked, *stopped; 

void makeContextNoArguments(struct ucontext_t *ucp, void (*func)());

void makeContextArguments(struct ucontext_t *ucp, void (*func)(), char *argv[],
                          int argc);

void blockAlarm(void);

void setAlarmHandler(void);

void inc(int thread);  // for testing stuff

void enqueueBlocked(struct Pcb *pcb);

void enqueueStopped(struct Pcb *pcb);

Pcb *searchBlockedAndStopped(pid_t pid);

void printBlocked();

Pcb *removeFromBlocked(pid_t pid);

Pcb* findProcess(pid_t pid);

void add_to_parent_changed_children(Pcb *process);

void add_to_parent_zombie(Pcb *process);

void processFinishedCleanup(Pcb *process);

