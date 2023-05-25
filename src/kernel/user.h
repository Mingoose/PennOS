#pragma once
#include <sys/types.h>
#include <stdbool.h>

/**
 * User function that forks a new
 * thread that retains most of the attributes of the parent thread (see
 * k_process_create). Once the thread is spawned, it executes the function
 * referenced by func with its argument array argv. fd0 is the file descriptor
 * for the input file, and fd1 is the file descriptor for the output file. It
 * returns the pid of the child thread on success, or -1 on error.
 */
pid_t p_spawn(void(*func), char *argv[], int argc, int fd0, int fd1, bool foreground);

/**
 * User function that sets the calling thread
 * as blocked (if nohang is false) until a child of the calling thread changes
 * state. It is similar to Linux waitpid(2). If nohang is true, p_waitpid does
 * not block but returns immediately. p_waitpid returns the pid of the child
 * which has changed state on success, or -1 on error
 */
pid_t p_waitpid(pid_t pid, int *wstatus, bool nohang);

/**
 * User function that sends the signal sig to the
 * thread referenced by pid.
 * @return 0 on success, -1 on error.
 */
pid_t p_kill(pid_t pid, int sig);

/**
 * Exits thread unconditionally
 */
void p_exit(void);

/**
 * User function that sets the priority of the thread pid to priority
 * @return 0 on success, -1 on error
 */
int p_nice(pid_t pid, int priority);

/**
 * User function that sets the calling process to blocked until ticks of
 * the system clock elapse, and then sets the thread to running. Importantly,
 * p_sleep should not return until the thread resumes running; however, it can
 * be interrupted by a S_SIGTERM signal. Like sleep(3) in Linux, the clock keeps
 * ticking even when p_sleep is interrupted.
 */
void p_sleep(unsigned int ticks);
