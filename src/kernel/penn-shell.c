#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "job_handler.h"
#include "kernel.h"
#include "parser.h"
#include "pipe.h"
#include "user.h"

#define MAXLINELENGTH 4096

void handleInputs(job_queue** head) {
    struct parsed_command* cmd;
    if (isatty(STDIN_FILENO)) {
        write(STDERR_FILENO, PROMPT, sizeof(PROMPT));
    }
    char cmdInput[MAXLINELENGTH];

    for (int i = 0; i < MAXLINELENGTH;
         i++) {  // initialize values of cmd to avoid invalid read/write
        cmdInput[i] = '\0';
    }

    int numBytes;

    if (isatty(STDIN_FILENO)) {
        numBytes = read(STDIN_FILENO, cmdInput, MAXLINELENGTH);
    } else {
        size_t bufsize = MAXLINELENGTH;
        char* cmdInputPointer = &cmdInput[0];
        numBytes = getline(&cmdInputPointer, &bufsize, stdin);
        if (numBytes < 0) {
            exit(EXIT_FAILURE);
        }
    }

    if (numBytes < 0) {
        write(STDOUT_FILENO, "read error", sizeof("read error"));
    }

    if (numBytes == 0) {  // user pressed ctrl-d without typing anything
        write(STDOUT_FILENO, "\n", sizeof("\n"));
        delete_queue(head);
        exit(EXIT_FAILURE);
    }

    if (numBytes == 1) {  // if user only presses enter we want to reprompt and
                          // not run the "command"
        if (cmdInput[0] == '\n') {
            poll_children(head);
            return;
        }
    }

    for (int i = 0; i < MAXLINELENGTH;
         i++) {  // eliminate trailing \n so that it does not get passed as part
                 // of a command
        if (cmdInput[i] == '\n') {
            cmdInput[i] = '\0';
        }
    }

    int err = parse_command(cmdInput, &cmd);
    if (err < 0) {
        perror("parse_command");
        return;
    }
    if (err > 0) {
        fprintf(stdout, "syntax error: %d\n", err);
        return;
    }

    for (int i = 0; i < MAXLINELENGTH; i++) {  // eliminate trailing &
        if (cmdInput[i] == '&') {
            cmdInput[i] = '\0';
        }
    }
    runPipes(cmdInput, cmd, head);
}

void shell() {
    job_queue* currHead = NULL;
    while (1) {
        handleInputs(&currHead);
    }
}