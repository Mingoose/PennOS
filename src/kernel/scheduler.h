#pragma once

#include <stdbool.h>

#include "pcb.h"
#include "queue.h"

extern struct Queue *highest, *medium, *lowest;

void addToScheduler(struct Pcb *pcb);

bool removeFromScheduler(struct Pcb *pcb);

void scheduler(void);

void initSchedulerQueues(void);

void printQueues(void);

struct Pcb* findNodeInScheduler(pid_t pid);