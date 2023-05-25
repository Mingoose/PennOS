#include "job_handler.h"

#include <stdbool.h>
#include <stdio.h>      // perror
#include <stdlib.h>     // malloc, free
#include <string.h>     // strlen, strcpy
#include <sys/types.h>  // pid_t

#include "../macros.h"
#include "../p_errno.h"
int add_to_job_queue(job_queue** head, pid_t pid, char* command) {
    job_queue* new_job = malloc(sizeof(job_queue));
    if (new_job == NULL) {
        perror("malloc");
        return -1;
    }

    new_job->command = malloc(strlen(command) + 1);
    if (new_job->command == NULL) {
        perror("malloc");
        return -1;
    }
    strcpy(new_job->command, command);

    new_job->pid = pid;
    new_job->pid_status = RUNNING;
    new_job->status = RUNNING;
    new_job->next = NULL;
    // new_job->stopped_by_SIGTTIN = 0;
    // new_job->num_commands = num_commands;
    if (*head == NULL) {
        new_job->jobId = 1;
        *head = new_job;
    } else {
        job_queue* curr_job = *head;
        while (curr_job->next != NULL) {
            curr_job = curr_job->next;
        }
        new_job->jobId = curr_job->jobId + 1;
        curr_job->next = new_job;
    }
    return new_job->jobId;
}

void update_status(job_queue** head, pid_t pid, int status) {
    job_queue* curr_job = *head;
    while (curr_job != NULL && curr_job->pid != pid) {
        curr_job = curr_job->next;
    }
    if (curr_job != NULL) {
        if (curr_job->status != status){
            curr_job->status = status;
            char* statusString = "";
            if (status == STOPPED) {
                statusString = "stopped";
            } else if (status == RUNNING) {
                statusString = "running";
            } else {
                statusString = "done";
            }
            fprintf(stderr, "[%d]  %s\t%s\n", curr_job->jobId, statusString,
                    curr_job->command);
        }
    }
    return;
}

void return_job_queue(job_queue** head) {
    job_queue* curr_job = *head;
    while (curr_job != NULL) {
        if (curr_job->status == RUNNING) {
            fprintf(stderr, "[%d] %s (running)\n", curr_job->jobId,
                    curr_job->command);
        } else if (curr_job->status == STOPPED) {
            fprintf(stderr, "[%d] %s (stopped)\n", curr_job->jobId,
                    curr_job->command);
        }
        curr_job = curr_job->next;
    }
}

void delete_job(job_queue** head, pid_t pid) {
    job_queue* prev_job = NULL;
    job_queue* curr_job = *head;
    while (curr_job != NULL && curr_job->pid != pid) {
        prev_job = curr_job;
        curr_job = curr_job->next;
    }
    if (prev_job == NULL) {
        *head = curr_job->next;
    } else {
        prev_job->next = curr_job->next;
    }
    free(curr_job->command);
    free(curr_job);
}

void delete_queue(job_queue** head) {
    job_queue* curr_job = *head;
    job_queue* prev_job = NULL;
    while (curr_job != NULL) {
        prev_job = curr_job;
        curr_job = curr_job->next;
        free(prev_job->command);
        free(prev_job);
    }
    return;
}

int get_current_job(job_queue** head) {
    job_queue* curr_job = *head;
    job_queue* prev_job = NULL;
    job_queue* current_job_output = NULL;
    while (curr_job != NULL) {
        if (curr_job->status == STOPPED) {
            current_job_output = curr_job;
        }
        prev_job = curr_job;
        curr_job = curr_job->next;
    }
    if (current_job_output == NULL) {
        return prev_job->pid;
    }
    return current_job_output->pid;
}

int get_num_jobs(job_queue** head) {
    int total = 0;
    job_queue* curr_job = *head;
    while (curr_job != NULL) {
        curr_job = curr_job->next;
        total += 1;
    }
    return total;
}

char* get_name(job_queue** head, pid_t pid) {
    job_queue* curr_job = *head;
    while (curr_job != NULL) {
        if (curr_job->pid == pid) {
            return curr_job->command;
        }
        curr_job = curr_job->next;
    }
    return NULL;
}

int get_status(job_queue** head, pid_t pid) {
    job_queue* curr_job = *head;
    while (curr_job != NULL) {
        if (curr_job->pid == pid) {
            return curr_job->status;
        }
        curr_job = curr_job->next;
    }
    return 0;
}

void terminate_process(job_queue** head, pid_t pid) {
    job_queue* curr_job = *head;
    while (curr_job != NULL) {
        if (curr_job->pid == pid) {
            curr_job->pid_status =
                EXITED;  // idk what this is but we def need the next line
            curr_job->status = EXITED;
        }
        curr_job = curr_job->next;
    }
}

int clean_dead_jobs(job_queue** head) {
    job_queue* curr_job = *head;
    while (curr_job != NULL) {
        bool stillRunning = false;
        if (curr_job->pid_status == RUNNING ||
            curr_job->pid_status == STOPPED) {
            stillRunning = true;
        }
        if (!stillRunning) {
            fprintf(stderr, "[1]  done: %s\n", curr_job->command);
            delete_job(head, curr_job->pid);
            return 1;
        } else {
            curr_job = curr_job->next;
        }
    }
    return 0;
}

pid_t get_job_by_id(job_queue** head, int job_id) {
    job_queue* curr_job = *head;
    while (curr_job != NULL) {
        if (curr_job->jobId == job_id) {
            return curr_job->pid;
        }
        curr_job = curr_job->next;
    }

    return 0;
}

// void update_SIGTTIN(job_queue** head, pid_t pid) {
//     job_queue* curr_job = *head;
//     while (curr_job != NULL && curr_job->pid != pid) {
//         curr_job = curr_job->next;
//     }
//     if (curr_job != NULL) {
//         curr_job->stopped_by_SIGTTIN = 1;
//     }
//     return;
// }

// int get_SIGTTIN(job_queue** head, pid_t pid) {
//     job_queue* curr_job = *head;
//     while (curr_job != NULL) {
//         if (curr_job->pid == pid) {
//             return curr_job->stopped_by_SIGTTIN;
//         }
//         curr_job = curr_job->next;
//     }
//     return -1;
// }