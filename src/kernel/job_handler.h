#ifndef JOB_HANDLER_H
#define JOB_HANDLER_H

#include <sys/types.h>

typedef struct job_queue_struct {
    int jobId;
    // int pgid;
    int pid;
    char* command;
    int status;
    // might be redundant, but will keep for now
    int pid_status;
    // int stopped_by_SIGTTIN; // afaik we don't need this for pennos
    struct job_queue_struct* next;
} job_queue;

// add_to_job_queue adds the qId to the end of the queue and returns the jobId
// if it succeeds, and -1 otherwise
int add_to_job_queue(job_queue** head, pid_t pid, char* command);

// update_status takes in a pid and updates the status within it
void update_status(job_queue** head, pid_t pid, int status);

// returns a character array with all of the jobs and their statuses
void return_job_queue(job_queue** head);

/**
 * removes job with pid `pid` from the queue
 */
void delete_job(job_queue** head, pid_t pid);

/**
 * destructs queue
 */
void delete_queue(job_queue** head);

int get_current_job(job_queue** head);

int get_num_jobs(job_queue** head);

// int* get_pids(job_queue** head, int pgid); // not needed since only 1 pid

char* get_name(job_queue** head, pid_t pid);

int get_status(job_queue** head, pid_t pid);

void terminate_process(job_queue** head, pid_t pid);

/**
 * @return 1 if the job is stopped, 0 otherwise
 */
int clean_dead_jobs(job_queue** head);

/**
 * @return 0 if not found
 */
int get_job_by_id(job_queue** head, int job_id);

// void update_SIGTTIN(job_queue** head, int pgid);

// int get_SIGTTIN(job_queue** head, int pgid);
#endif