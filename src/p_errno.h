#pragma once

extern int p_errno;

#define p_INVALID 1
#define p_NOTF 2
#define p_PNOTF 3
#define p_CNOTF 4
#define p_ARGS 5
#define p_FDINV 6
#define p_PERMS 7
#define p_FDWRITE 8
#define p_FILEINV 9
#define p_INVCLOSE 10

/**
 * Prints error message based on the value of p_errno prepended by the string.
 * @param s string to prepend to error message
 */
void p_perror(char *s);

/**
 * resets the value of p_errno to 0
 */
void reset_p_errno();
