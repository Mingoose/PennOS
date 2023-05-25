#pragma once

#define S_SIGSTOP 0
#define S_SIGCONT 1
#define S_SIGTERM 2

// used for W_WIFEXITED, W_WIFSIGNALED, W_WIFSTOPPED, and job control?
#define EXITED -1
#define STOPPED 0
#define RUNNING 1
#define SIGNALED 2

#define W_WIFEXITED(status) (status == EXITED)
#define W_WIFSTOPPED(status) (status == STOPPED)
#define W_WIFSIGNALED(status) (status == SIGNALED)
#define W_WIFCONTINUED(status) (status == RUNNING)

#define CENTISECOND 10000
#define CLOCK_CYCLE_LENGTH (10 * CENTISECOND)

#define MAN_STRING "cat:\n\tcat FILE ... [ -w OUTPUT_FILE ] \n \t\tConcatenates the files and prints them to stdout by default, or overwrites OUTPUT_FILE. If OUTPUT_FILE does not exist, it will be created. (Same for OUTPUT_FILE in the commands below.)\n \tcat FILE ... [ -a OUTPUT_FILE ]\n\t\tConcatenates the files and prints them to stdout by default, or appends to OUTPUT_FILE.\n\tcat -w OUTPUT_FILE \n\t\tReads from the terminal and overwrites OUTPUT_FILE.\n\tcat -a OUTPUT_FILE\n\t\tReads from the terminal and appends to OUTPUT_FILE.\nsleep n\n\tsleep for n seconds\nbusy\n\tbusy wait indefinitely\necho s\n\tprints s to stdout or file if redirected\nls\n\tlist all files in the working directory\ntouch file\n\tcreate an empty file if it does not exist, or update its timestamp otherwise\nmv src dest\n\trename src to dest\ncp src dest\n\tcopy src to dest\nrm file\n\tremove files\nchmod\n\tchanges permissions of file\nps\n\tlist all processes\nkill SIGNAL_NAME pid\n\tend the specified signal to the specified processes, where -SIGNAL_NAME is either term (the default), stop, or cont\n\tterm will terminate the program corresponding to pid, stop will stop the program, and cont will continue the program.\nzombify\n\tcreates a zombie child process\norphanify\n\tcreates an orphan process\nnice priority command [arg]\n\tset the priority of the command to priority and execute the command.\nnice_pid priority pid\n\tadjust the nice level of process pid to priority priority.\nman\n\tlist all available commands.\nbg [job_id]\n\tcontinue the last stopped job, or the job specified by job_id 6.\nfg [job_id]\n\tbring the last stopped or backgrounded job to the foreground, or the job specified by job_id.\njobs\n\tlist all jobs.\nlogout\n\texit the shell and shutdown PennOS.\n"

