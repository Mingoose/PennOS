#include "p_errno.h"

#include <stdio.h>  // printf, fprintf

int p_errno;

void p_perror(char *s) {
    switch (p_errno) {
        case 0:  // no error
            printf("no error\n");
            break;
        case p_INVALID:
            fprintf(stderr, "%s: invalid command\n", s);
            break;
        case p_NOTF:
            fprintf(stderr, "%s: process not found (e.g. kernel cleanup) \n",
                    s);
            break;
        case p_CNOTF:
            fprintf(stderr, "%s: child not found\n", s);
            break;
        case p_PNOTF:
            fprintf(stderr, "%s: parent not found\n", s);
            break;
        case p_ARGS:
            fprintf(stderr, "%s: invalid arguments\n", s);
            break;
        case p_FDINV:
            fprintf(stderr, "%s: file descriptor does not exist\n", s);
            break;
        case p_PERMS:
            fprintf(stderr, "%s: file descriptor does not have correct permissions\n", s);
            break;
        case p_FDWRITE:
            fprintf(stderr, "%s: file descriptor already opened in write mode\n", s);
            break;
        case p_FILEINV:
            fprintf(stderr, "%s: file does not exist\n", s);
            break;
        case p_INVCLOSE:
            fprintf(stderr, "%s: tried to close stdin, stdout, or stderr\n", s);
            break;
        default:
            fprintf(stderr, "%s: invalid value of p_errno\n", s);
            break;
    }
}

void reset_p_errno() { p_errno = 0; }