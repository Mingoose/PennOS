#include "builtins.h"

#include <stdio.h>   // printf
#include <stdlib.h>  // atoi
#include <string.h>  // strcmp
#include <unistd.h>  // pid_t

#include "../fs/pennfat.h"
#include "../fs/system.h"
#include "../macros.h"
#include "../p_errno.h"
#include "kernel.h"
#include "queue.h"
#include "scheduler.h"
#include "stress.h"
#include "user.h"

void fileDescriptorCleanup() {
    if (strcmp(activeContextPcb->process_name, "idle") == 0){
        return;
    }
    if (activeContextPcb->f_stdin != F_STDIN) {
        f_close(activeContextPcb->f_stdin);
    }
    if (activeContextPcb->f_stdout != F_STDOUT) {
        f_close(activeContextPcb->f_stdout);
    }
}

void print(char *str) {
    extern bool isMounted;
    if (isMounted) {
        f_write(activeContextPcb->f_stdout, str, strlen(str));
    } else {
        printf("%s", str);
    }
}

int checkValidRedirects(void){
    if (activeContextPcb -> f_stdin == -1){
        printf("input file does not exist\n");
        return -1;
    }
    if (activeContextPcb -> f_stdout == -1){
        printf("output file does not exist\n");
        return -1;
    }
    return 0;
}

void echo(char *argv[], int argc) {
    if (checkValidRedirects() == -1){
        return;
    }
    blockAlarm();
    for (int i = 1; i < argc; i++) {
        print(argv[i]);
        if (i != argc - 1) {
            print(" ");
        }
    }
    print("\n");
    setAlarmHandler();
    fileDescriptorCleanup();
}

void sleepBuiltin(char *argv[], int argc) {
    if (checkValidRedirects() == -1){
        return;
    }
    if (argc != 2) {
        p_errno = p_ARGS;
        return;
    }
    int numClockTicksPerSecond = 1000000 / CLOCK_CYCLE_LENGTH;
    p_sleep(numClockTicksPerSecond * atoi(argv[1]));
}

void mountBuiltin(char *argv[], int argc) {
    if (checkValidRedirects() == -1){
        return;
    }
    blockAlarm();
    mount(argv[1]);
    setAlarmHandler();
}

void catBuiltin(char *argv[], int argc) {
    if (checkValidRedirects() == -1){
        return;
    }
    extern struct Pcb *foregroundContextPcb;
    extern struct Pcb *activeContextPcb;
    if (argc == 1) {
        while (1) {  // this while 1 is to revive cat when it is brought to the
                     // foreground
            if (activeContextPcb == foregroundContextPcb) {
                // we know cat has terminal control
                f_lseek(activeContextPcb->f_stdin, 0, F_SEEK_SET);
                while (1) {
                    char buff[4096];
                    for (int i = 0; i < 4096; i++) {
                        buff[i] = '\0';
                    }
                    int numRead = f_read(activeContextPcb->f_stdin, 4096, buff);
                    if (numRead > 0) {
                        if (buff[numRead - 1] == '\n') {
                            buff[numRead - 1] = '\0';
                        }
                        print(buff);
                    }
                    if (activeContextPcb->f_stdin == F_STDIN){
                        print("\n");
                    }
                    if (numRead == 0){
                        print("\n");
                        fileDescriptorCleanup();
                        return;
                    }
                }
            } else {
                p_kill(activeContextPcb->process_id, S_SIGSTOP);
            }
        }
    } else {
        for (int i = 1; i < argc; i++) {
            int fd = f_open(argv[i], F_READ);
            if (fd == -1) {
                return;
            }
            int bytes = -1;
            f_lseek(fd, 0, F_SEEK_SET);
            while (bytes != 0) {
                char buf[4096];
                for (int i = 0; i < 4096; i++) {
                    buf[i] = '\0';
                }
                bytes = f_read(fd, 4096, buf);
                if (bytes != 0) {
                    print(buf);
                }
            }
            f_close(fd);
        }
    }
    fileDescriptorCleanup();
}

void lsBuiltin(char *argv[], int argc) {
    if (checkValidRedirects() == -1){
        return;
    }
    blockAlarm();
    char* lsPrintStatement;
    if (argc == 2){
        lsPrintStatement = f_ls(argv[1]);
    } else if (argc == 1){
        lsPrintStatement = f_ls(NULL);
    } else {
        p_errno = p_ARGS;
        return;
    }
    print(lsPrintStatement);
    free(lsPrintStatement);
    setAlarmHandler();
    fileDescriptorCleanup();
}

void touchBuiltin(char *argv[], int argc) {
    if (checkValidRedirects() == -1){
        return;
    }
    for (int i = 1; i < argc; i++) {
        int fd = f_open(argv[i], F_WRITE);
        f_close(fd);
    }
}

void mvBuiltin(char *argv[], int argc) {
    if (checkValidRedirects() == -1){
        return;
    }
    // src = argv[1], dest = argv[2]
    int fd_src = f_open(argv[1], F_READ);
    int fd_dest = f_open(argv[2], F_WRITE);
    int bytes = -1;
    while (bytes != 0) {
        char buf[10];
        for (int i = 0; i < 10; i++) {
            buf[i] = '\0';
        }
        bytes = f_read(fd_src, 10, buf);
        if (bytes != 0) {
            f_write(fd_dest, buf, 10);
        }
    }
    // need to rename
    f_rename(argv[2], argv[1]);
    f_unlink(argv[1]);
    f_close(fd_src);
    f_close(fd_dest);
}

void cpBuiltin(char *argv[], int argc) {
    if (checkValidRedirects() == -1){
        return;
    }
    int fd_src = f_open(argv[1], F_READ);
    int fd_dest = f_open(argv[2], F_WRITE);
    int bytes = -1;
    while (bytes != 0) {
        char buf[10];
        for (int i = 0; i < 10; i++) {
            buf[i] = '\0';
        }
        bytes = f_read(fd_src, 10, buf);
        if (bytes != 0) {
            f_write(fd_dest, buf, 10);
        }
    }
    f_close(fd_src);
    f_close(fd_dest);
}

void rmBuiltin(char *argv[], int argc) {
    if (checkValidRedirects() == -1){
        return;
    }
    for (int i = 1; i < argc; i++) {
        f_unlink(argv[i]);
    }
}

void chmodBuiltin(char *argv[], int argc) {
    if (checkValidRedirects() == -1){
        return;
    }
    for (int i = 2; i < argc; i++) {
        if (argv[1][0] == '+') {
            if (argv[1][1] == 'w') {
                f_chmod(argv[i], F_ADDW);
            } else if (argv[1][1] == 'r') {
                f_chmod(argv[i], F_ADDR);
            } else if (argv[1][1] == 'x') {
                f_chmod(argv[i], F_ADDX);
            }
        } else if (argv[1][0] == '-') {
            if (argv[1][1] == 'w') {
                f_chmod(argv[i], F_SUBW);
            } else if (argv[1][1] == 'r') {
                f_chmod(argv[i], F_SUBR);
            } else if (argv[1][1] == 'x') {
                f_chmod(argv[i], F_SUBX);
            }
        }
    }
}

void busy() {
    if (checkValidRedirects() == -1){
        return;
    }
    while (1) {
    }
}

void ps() {
    if (checkValidRedirects() == -1){
        return;
    }
    printf("PID \t PPID \t PRI \t STAT \t CMD\n");
    printQueueProcesses(highest, 'R');
    printQueueProcesses(medium, 'R');
    printQueueProcesses(lowest, 'R');
    printQueueProcesses(blocked, 'B');
    printQueueProcesses(stopped, 'S');
}

void killBuiltin(char *argv[], int argc) {
    // 1 accounts for the - in the signal
    pid_t pid;
    if (argc < 2 || argc > 3) {
        p_errno = p_ARGS;
    }
    if (argv[1][0] == '-') {
        pid = atoi(argv[2]);
        if (strcmp(argv[1], "-term") == 0) {
            p_kill(pid, S_SIGTERM);
        } else if (strcmp(argv[1], "-stop") == 0) {
            p_kill(pid, S_SIGSTOP);
        } else if (strcmp(argv[1], "-cont") == 0) {
            p_kill(pid, S_SIGCONT);
        }
    } else {
        // no signal specified, so pid is argv[1]
        pid = atoi(argv[1]);
        p_kill(pid, S_SIGTERM);
    }
}

void orphan_child() {
    while (1)
        ;
}
void zombie_child() { return; }

/**
 * Spawns an orphan child process
 * @return the pid of the orphan child
 */
void orphanify(char *argv[], int argc) {
    p_spawn(orphan_child, (char *[]){"orphan_child"}, argc, STDIN_FILENO,
            STDOUT_FILENO, false);
    return;
}

/**
 * Spawns a zombie child process.
 * @return the pid of the zombie child
 */
void zombify(char *argv[], int argc) {
    p_spawn(zombie_child, (char *[]){"zombie_child"}, 1, STDIN_FILENO,
            STDOUT_FILENO, false);  // zombie_child is a background process for
                                    // now, might want to change later
    while (1)
        ;
    return;
}

void manBuiltin() {
    if (checkValidRedirects() == -1){
        return;
    }
    print(MAN_STRING);
}

void *getBuiltIn(char *funcName) {
    if (strcmp(funcName, "echo") == 0) {
        return echo;
    } else if (strcmp(funcName, "sleep") == 0) {
        return sleepBuiltin;
    } else if (strcmp(funcName, "ps") == 0) {
        return ps;
    } else if (strcmp(funcName, "kill") == 0) {
        return killBuiltin;
    } else if (strcmp(funcName, "orphanify") == 0) {
        return orphanify;
    } else if (strcmp(funcName, "zombify") == 0) {
        return zombify;
    } else if (strcmp(funcName, "busy") == 0) {
        return busy;
    } else if (strcmp(funcName, "mount") == 0) {
        return mountBuiltin;
    } else if (strcmp(funcName, "cat") == 0) {
        return catBuiltin;
    } else if (strcmp(funcName, "ls") == 0) {
        return lsBuiltin;
    } else if (strcmp(funcName, "touch") == 0) {
        return touchBuiltin;
    } else if (strcmp(funcName, "mv") == 0) {
        return mvBuiltin;
    } else if (strcmp(funcName, "cp") == 0) {
        return cpBuiltin;
    } else if (strcmp(funcName, "rm") == 0) {
        return rmBuiltin;
    } else if (strcmp(funcName, "chmod") == 0) {
        return chmodBuiltin;
    } else if (strcmp(funcName, "hang") == 0) {
        return hang;
    } else if (strcmp(funcName, "nohang") == 0) {
        return nohang;
    } else if (strcmp(funcName, "recur") == 0) {
        return recur;
    }
    return NULL;
}
