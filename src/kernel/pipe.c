#include "pipe.h"

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../fs/system.h"
#include "../p_errno.h"
#include "builtins.h"
#include "job_handler.h"
#include "kernel.h"
#include "scheduler.h"
#include "user.h"

#define MAX_ARGS 100
#define NUM_CMDS_WITH_CHILDREN 15

int pid = 0;

void poll_children(job_queue** head) {
    while (true) {
        int pollStatus;
        int waitStatus = p_waitpid(-1, &pollStatus, true);
        if (waitStatus == 0 || waitStatus == -1) {
            break;
        } else if (W_WIFSTOPPED(pollStatus)) {
            update_status(head, waitStatus, STOPPED);
        } else if (W_WIFCONTINUED(pollStatus)) {
            update_status(head, waitStatus, RUNNING);
        } else {
            terminate_process(head, waitStatus);
        }
    }
    while (true) {
        if (clean_dead_jobs(head) == 0) {
            break;
        }
    }
}

/**
 * Runs a command with the given parsed command.
 * This function was separated from runPipes for the `nice priority` builtin
 * so that the priority can be set before the p_spawn. Thus this function only
 * contains the commands that spawn a child process.
 */
pid_t execCommand(struct parsed_command* cmd, int* status, int argc,
                  job_queue** head, char* cmdName) {
    pid_t child = -1;
    /***** builtins that spawn children */
    char* cmdsWithChildren[NUM_CMDS_WITH_CHILDREN] = {
        "echo", "sleep", "busy",  "ps", "kill", "zombify", "orphanify", "mount",
        "cat",  "ls",    "touch", "mv", "cp",   "rm",      "chmod"};
    for (int i = 0; i < NUM_CMDS_WITH_CHILDREN; i++) {
        if (strcmp(cmd->commands[0][0], cmdsWithChildren[i]) == 0) {
            int inputFD = F_STDIN;
            int outputFD = F_STDOUT;
            if (cmd->stdin_file != NULL) {
                inputFD = f_open(cmd->stdin_file, F_READ);
            }
            if (cmd->stdout_file != NULL) {
                if (cmd->is_file_append) {
                    outputFD = f_open(cmd->stdout_file, F_APPEND);
                } else {
                    outputFD = f_open(cmd->stdout_file, F_WRITE);
                }
            }
            child = p_spawn(getBuiltIn(cmd->commands[0][0]), cmd->commands[0],
                            argc, inputFD, outputFD, !(cmd->is_background));

            p_waitpid(child, status, cmd->is_background);
            // if foreground only add to jobs queue if it is stoppped
            // if background add to jobs queue always
            if (!cmd->is_background) {  // foreground
                if (W_WIFSTOPPED(*status)) {
                    add_to_job_queue(head, child, cmdName);
                    update_status(head, child, STOPPED);
                }
            } else {  // background job
                int jobId = add_to_job_queue(head, child, cmdName);
                fprintf(stderr, "[%d] %d\n", jobId, child);
            }
            return child;
        }
    }

    /******* scripts ********/
    // not sure what the intedned behavior is here in regards to
    // background
    // if else then we know it is the name of a script
    // open script file
    int scriptFD = f_open(cmd->commands[0][0], F_READ);
    if (scriptFD == -1) {
        p_errno = p_INVALID;
        // fprintf(stderr, "Error: script %s not found",
        // cmd->commands[0][0]);
        return -1;
    }
    char buff[4096];
    for (int i = 0; i < 4096; i++){
        buff[i] = '\0';
    }
    f_lseek(activeContextPcb->f_stdin, 0, F_SEEK_SET);
    int numRead = f_read(scriptFD, 4096, buff);
    if (numRead == -1){
        printf("couldnt read!!!\n"); //put perror stuff here
    }
    f_close(scriptFD);
    // execute each line of the script using execCommand
    struct parsed_command* newCmd;

    //while ((read = getline(&line, &len, script)) != -1) {
    int currentChar = 0;
    int numCmds = 1;
    while(1) {
        char line[4096];
        for (int i = 0; i < 4096; i++){
            line[i] = '\0';
        }
        int lineCurrChar = 0;
        for (; currentChar < 4096; currentChar++){
            line[lineCurrChar] = buff[currentChar];
            if(buff[currentChar] == '\n' || buff[currentChar] == '\0'){
                currentChar++;
                break;
            }
            lineCurrChar++;
        }
        if (strlen(line) == 1){ // just a \n
            continue;
        } else if (strlen(line) == 0){
            break;
        }
        // remove newline
        line[strlen(line) - 1] = '\0';
        // parse line
        int parse = parse_command(line, &newCmd);
        if (parse < 0) {
            perror("Error parsing command");
        }
        int newArgc = 0;
        for (int i = 0; i < MAX_ARGS;
             i++) {  // surely we don't have more than 100 arguments
            if (newCmd->commands[0][i] != NULL) {
                newArgc++;
            } else {
                break;
            }
        }
        if (cmd->is_background) {
            newCmd->is_background = true;
        }
        // execute line
        if (cmd -> is_file_append == true || (cmd -> stdout_file != NULL && numCmds > 1)){
            newCmd->is_file_append = true;
        }
        if (cmd -> stdin_file != NULL){
            newCmd->stdin_file = cmd -> stdin_file;
        }
        if (cmd -> stdout_file != NULL){
            newCmd->stdout_file = cmd -> stdout_file;
        }
        execCommand(newCmd, status, newArgc, head, cmdName);
        numCmds++;
    }
    // free parsed command
    free(newCmd);

    // set status here?
    return child;
}

void runPipes(char cmdInput[MAXLINELENGTH], struct parsed_command* cmd,
              job_queue** head) {
    poll_children(head);
    reset_p_errno();
    // built-ins first
    int status;
    int argc = 0;
    for (int i = 0; i < MAX_ARGS;
         i++) {  // surely we don't have more than 100 arguments
        if (cmd->commands[0][i] != NULL) {
            argc++;
        } else {
            break;
        }
    }
    /******* subroutines ******/
    // job control
    if (strcmp(cmd->commands[0][0], "fg") == 0) {  // fg still not working!!!!
        if (get_num_jobs(head) == 0) {
            fprintf(stderr, "fg: no current job\n");
            return;
        }
        int curr_job = 0;
        if ((cmd->commands)[0][1] == NULL) {  // no pid given
            curr_job = get_current_job(head);
        } else {
            curr_job = get_job_by_id(head, atoi((cmd->commands)[0][1]));
        }
        update_status(head, curr_job, RUNNING);
        p_kill(curr_job, S_SIGCONT);
        Pcb* curr = findNodeInScheduler(curr_job);
        foregroundContextPcb = curr;
        if (curr == NULL) {  // we know that curr is sleep meaning that it can
                             // be found in blocked
            curr = searchBlockedAndStopped(curr_job);
            foregroundContextPcb = curr;
        }
        p_waitpid(curr_job, &status, false);
        // if it's stopped again
        if (W_WIFSTOPPED(status)) {
            update_status(head, curr_job, STOPPED);
        } else if (W_WIFCONTINUED(
                       status)) {  // if it is continued again when it is
                                   // brought to the foreground then we don't
                                   // actually want it to stop!!
            p_waitpid(curr_job, &status, false);
            if (W_WIFSTOPPED(status)) {
                update_status(head, curr_job, STOPPED);
            } else {
                terminate_process(head, curr_job);
            }
        } else {
            terminate_process(head, curr_job);
        }

    } else if (strcmp(cmd->commands[0][0], "bg") == 0) {
        if (get_num_jobs(head) == 0) {
            fprintf(stderr, "bg: no current job\n");
            return;
        }
        int curr_job = 0;
        if ((cmd->commands)[0][1] == NULL) {  // no pid given
            curr_job = get_current_job(head);
        } else {
            curr_job = get_job_by_id(head, atoi((cmd->commands)[0][1]));
        }
        p_kill(curr_job, S_SIGCONT);
        update_status(head, curr_job, RUNNING);
        char* name = get_name(head, curr_job);
        fprintf(stderr, "running: %s\n", name);
    } else if (strcmp(cmd->commands[0][0], "jobs") == 0) {
        return_job_queue(head);
    } else if (strcmp(cmd->commands[0][0], "nice") ==
               0) {  // might want to change later to handle foreground
        int priority = atoi(cmd->commands[0][1]);
        if (priority != 0 && priority != 1 && priority != -1) {
            p_errno = p_ARGS;
        }
        cmd->commands[0] = &cmd->commands[0][2];  // ignores "nice [priority]"
        pid_t childPid = execCommand(cmd, &status, argc - 2, head, cmdInput);
        p_nice(childPid, priority);
    } else if (strcmp(cmd->commands[0][0], "nice_pid") == 0) {
        p_nice(atoi(cmd->commands[0][2]), atoi(cmd->commands[0][1]));
    } else if (strcmp(cmd->commands[0][0], "man") == 0) {
        manBuiltin();
    } else if (strcmp(cmd->commands[0][0], "logout") == 0) {
        exit(EXIT_SUCCESS);
    } else if (strcmp(cmd->commands[0][0], "nohang") == 0) {
        nohang();
    } else if (strcmp(cmd->commands[0][0], "hang") == 0) {
        hang();
    } else if (strcmp(cmd->commands[0][0], "recur") == 0) {
        recur();
    } else {
        execCommand(cmd, &status, argc, head, cmdInput);
    }
    if (p_errno != 0) {
        p_perror(cmd->commands[0][0]);
    }
    free(cmd);
}
