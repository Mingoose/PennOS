#pragma once
#include <unistd.h>

#include "pcb.h"

/**
 * called when a terminated/finished thread’s resources needs to be cleaned up.
 * Such clean-up may include freeing memory, setting the status of the child,
 * etc.
 */
void k_process_cleanup(Pcb *process);

/**
 * Create a new child thread and associated PCB. The new thread
 * should retain much of the properties of the parent. The function
 * should return a reference to the new PCB.
 */
Pcb *k_process_create(Pcb *parent);

/**
 * kill the process referenced by process with the signal signal.
 */
void k_process_kill(Pcb *process, int signal);

void k_process_cleanup_helper(Pcb *process);