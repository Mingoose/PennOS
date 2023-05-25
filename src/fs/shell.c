#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include "pennfat.h"
#include "fatll.h"
#include "inputParse.h"

extern fileSystem* currFS;
extern bool isMounted;
extern bool catLoop;
extern int fs_fd;

void signalHandler(int signum) {
    //handles ctrl+z
    if (signum == SIGTSTP) {
        if (catLoop) {
            catLoop = false;
        }
    //handles ctrl+c
    } 
    else if (signum == SIGINT) {
        if (catLoop) {
            catLoop = false;
        }
        printf("\n");
    }
}

int main(int argc, char *argv[]) {
    currFS = calloc(1, sizeof(fileSystem));
    isMounted = false;
    while(1) {
        if (signal(SIGTSTP, signalHandler) == SIG_ERR) { // handles when the user presses ctrl-z
            perror("signal failed");
        }
        if (signal(SIGINT, signalHandler) == SIG_ERR) { // handles when the user presses ctrl-c
            perror("signal failed");
        }

        write(STDERR_FILENO, PROMPT, sizeof(PROMPT));

        char cmdInput[MAXLINELENGTH]; 

        for (int i = 0; i < MAXLINELENGTH; i++) { //initialize values of cmd to avoid invalid read/write
            cmdInput[i] = '\0';
        }


        int numBytes;

        numBytes = read(STDIN_FILENO, cmdInput, MAXLINELENGTH);
        
        for (int i = 0; i < MAXLINELENGTH; i++) { //initialize values of cmd to avoid invalid read/write
            if(cmdInput[i] == '\n') {
                cmdInput[i] = '\0';
            }
       }

        if (numBytes == 0) { // user pressed ctrl-d without typing anything
            write(STDERR_FILENO, "\n", sizeof("\n"));
            exit(EXIT_FAILURE);
        }

        if (numBytes < 0){
            write(STDOUT_FILENO, "read error", sizeof("read error"));
        }

        int numArgs = getNumTokens(cmdInput);
        if (numArgs == 0) {
            continue;
        }
        char** args = calloc(1, (numArgs + 1) * (sizeof(char*)));

        getTokens(numArgs, args, cmdInput);

        if (strcmp(args[0], "mkfs") == 0) {
            if (numArgs < 4) {
                printf("not enough args!\n");
            } else {
                mkfs(args[1], atoi(args[2]), atoi(args[3]));
            }
        } else if (strcmp(args[0], "mount") == 0) {
            if (numArgs < 2) {
                printf("not enough args!\n");
            } else {
                mount(args[1]);
            }
        } else if (strcmp(args[0], "unmount") == 0) {
            if (!isMounted) {
                printf("nothing is mounted!\n");
            } else {
                unmount();
            }
        } else if (strcmp(args[0], "touch") == 0) {
            if (numArgs < 2) {
                printf("not enough args!\n");
            } else if (!isMounted) {
                printf("nothing is mounted!\n");
            } else {
                for (int i = 1; i < numArgs; i++) {
                    touch(args[i]);
                }
            }
        } else if (strcmp(args[0], "mv") == 0) {
            if (numArgs < 3) {
                printf("not enough args!\n");
            } else if (!isMounted) {
                printf("nothing is mounted!\n");
            } else {
                int found = checkInDirectory(args[1]);
                if (found == -1) {
                    printf("file not found!\n");
                } else {
                    touch(args[2]);
                    clearBlocks(args[2]);
                    copyFromFile(args[1], args[2]);
                    rm(args[1]);
                    changeName(args[1], args[2]);
                }
            }
        } else if (strcmp(args[0], "rm") == 0) {
            if (numArgs < 2) {
                printf("not enough args!\n");
            } else if (!isMounted) {
                printf("nothing is mounted!\n");
            } else {
                for (int i = 1; i < numArgs; i++) {
                    rm(args[1]);
                }
            }
        } else if (strcmp(args[0], "cat") == 0) {
            if (!isMounted) {
                printf("nothing is mounted!\n");
            } else if (numArgs < 2) {
                printf("not enough args!\n");
            } else {
                if (strcmp(args[1], "-w") == 0) {
                    touch(args[2]);
                    clearBlocks(args[2]);
                    char* line = NULL;
                    size_t len = 0;
                    catLoop = true;
                    while (catLoop) {
                        struct sigaction sa = {.sa_handler = signalHandler};
                        sigaction(SIGINT, &sa, 0);
                        getline(&line, &len, stdin);
                        
                        if (catLoop) {
                            addToFile(args[2], line);
                            free(line);
                            line = NULL;
                        }
                    }
                } else if (strcmp(args[1], "-a") == 0) {
                    touch(args[2]);
                    char* line = NULL;
                    size_t len = 0;
                    catLoop = true;
                    while (catLoop) {
                        struct sigaction sa = {.sa_handler = signalHandler};
                        sigaction(SIGINT, &sa, 0);
                        getline(&line, &len, stdin);
                        if (catLoop) {
                            addToFile(args[2], line);
                            free(line);
                            line = NULL;
                        }
                    }
                } else {
                    if (strcmp(args[numArgs - 2], "-w") == 0) {
                        touch(args[numArgs - 1]);
                        clearBlocks(args[numArgs - 1]);
                        for (int i = 1; i < numArgs - 2; i++) {
                            copyFromFile(args[i], args[numArgs - 1]);
                        }
                    } else if (strcmp(args[numArgs - 2], "-a") == 0) {
                        touch(args[numArgs - 1]);
                        for (int i = 1; i < numArgs - 2; i++) {
                            copyFromFile(args[i], args[numArgs - 1]);
                        }
                    } else {
                        for (int i = 1; i < numArgs; i++) {
                            printFile(args[i]);
                        }
                    }
                }
            }
        } else if (strcmp(args[0], "ls") == 0) {
            if (!isMounted) {
                printf("nothing is mounted!\n");
            } else {
                ls();
            }
        } else if (strcmp(args[0], "cp") == 0) {
            if (numArgs < 3) {
                printf("not enough args!\n");
            } else if (!isMounted) {
                printf("nothing is mounted!\n");
            } else {
                if (strcmp(args[1], "-h") == 0) {
                    touch(args[3]);
                    clearBlocks(args[3]);
                    copyFromOSFile(args[3], args[2]);
                } else if (strcmp(args[2], "-h") == 0) {
                    writeFileToOS(args[1], args[3]);
                } else {
                    if (findInDirectory(args[1]) == -1) {
                        printf("file not found!\n");
                    } else {
                        touch(args[2]);
                        clearBlocks(args[2]);
                        copyFromFile(args[1], args[2]);
                    }
                }
            }
        } else if (strcmp(args[0], "chmod") == 0) {
            if (numArgs < 3) {
                printf("not enough args!\n");
            } else if (!isMounted) {
                printf("nothing is mounted!\n");
            } else {
                for (int i = 2; i < numArgs; i++) {
                    modifyPerms(args[i], args[1]);
                }
            }
        }
        free(args);
    }
}