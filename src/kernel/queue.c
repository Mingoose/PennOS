#include "queue.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void init(struct Queue *queue) {
    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;
}

void enqueue(struct Queue *queue, struct Pcb *pcb, pid_t pid) {
    ++queue->size;
    struct QueueNode *node = malloc(sizeof(struct QueueNode));
    if (node == NULL) {
        perror("enqueue/malloc");
        return;
    }
    node->Pcb = pcb;
    node->pid = pid;
    // print the pcb context
    if (queue->head == NULL) {
        // there are no elements
        queue->head = node;
        queue->tail = node;
        node->next = NULL;
        // node->prev = NULL;
    } else {
        // there are some elements
        queue->tail->next = node;
        // node->prev = list->tail;
        queue->tail = node;
        node->next = NULL;
    }
    // printf("done with enqueue\n");
}

struct Pcb *dequeue(struct Queue *queue) {
    if (queue->head == NULL) {
        // there are no elements
        return NULL;
    }
    --queue->size;
    if (queue->head == queue->tail) {
        // there is only one element
        struct Pcb *pcb = queue->head->Pcb;
        queue->head = NULL;
        queue->tail = NULL;
        free(queue->head);
        return pcb;
    } else {
        // there are some elements
        struct Pcb *pcb = queue->head->Pcb;
        struct QueueNode *temp = queue->head;
        queue->head = queue->head->next;
        free(temp);
        return pcb;
    }
}

struct Pcb *removeQueueNode(struct Queue *queue, pid_t pid) {
    struct QueueNode *node = queue->head;
    struct QueueNode *prev = NULL;
    while (node != NULL) {
        if (node->pid == pid) {
            if (prev == NULL) {
                // node is the head
                queue->head = node->next;
            } else {
                // node is not the head
                prev->next = node->next;
            }
            if (node->next == NULL) {
                // node is the tail
                queue->tail = prev;
            }
            struct Pcb *pcb = node->Pcb;
            // free(node); removed this free to stop segfault
            --queue->size;
            return pcb;
        }
        prev = node;
        node = node->next;
    }
    return NULL;
}

struct Pcb *findQueueNode(struct Queue *queue, pid_t pid) {
    struct QueueNode *node = queue->head;
    while (node != NULL) {
        if (node->pid == pid) {
            return node->Pcb;
        }
        node = node->next;
    }
    return NULL;
}

int size(struct Queue *queue) { return queue->size; }

void printQueueProcesses(struct Queue *queue, char status) {
    struct QueueNode *node = queue->head;
    while (node != NULL) {
        Pcb *nodePcb = node->Pcb;
        printf("%d\t %d\t %d\t %c\t %s\n", nodePcb->process_id,
               nodePcb->parent_id, nodePcb->priority, status,
               nodePcb->process_name);
        if (nodePcb->zombie_queue != NULL) {
            printQueueProcesses(nodePcb->zombie_queue, 'Z');
        }
        node = node->next;
    }
}