#pragma once
#include <stdbool.h>
#include <sys/types.h>
#include <unistd.h>

#include "pcb.h"

// changes these structs to be static to get things to compile
static struct Queue {
    struct QueueNode *head;
    struct QueueNode *tail;
    int size;
} Queue;

static struct QueueNode {
    pid_t pid;
    struct QueueNode *next;
    // struct linked_list_node *prev;
    struct Pcb *Pcb;
    // bool done;
} QueueNode;

void init(struct Queue *queue);

void enqueue(struct Queue *queue, struct Pcb *pcb, pid_t pid);

struct Pcb *removeQueueNode(struct Queue *queue, pid_t pid);

struct Pcb *dequeue(struct Queue *queue);

struct Pcb *findQueueNode(struct Queue *queue, pid_t pid);

int size(struct Queue *queue);

/**
 * Prints the processes in the queue, along with status in the form
 * PID PPID PRIORITY STATUS CMD
 * for usage in the `ps` built-in.
 */
void printQueueProcesses(struct Queue *queue, char status);