#pragma once

#include <unistd.h>

#include "../fs/pennfat.h"

void echo(char *argv[], int argc);

void sleepBuiltin(char *argv[], int argc);

void mountBuiltin(char *argv[], int argc);

void catBuiltin(char *argv[], int argc);

void lsBuiltin(char *argv[], int argc);

void touchBuiltin(char *argv[], int argc);

void mvBuiltin(char *argv[], int argc);

void cpBuiltin(char *argv[], int argc);

void rmBuiltin(char *argv[], int argc);

void chmodBuiltin(char *argv[], int argc);

void busy();

void ps();

void killBuiltin(char *argv[], int argc);

void orphanify(char *argv[], int argc);

void manBuiltin();

void zombify(char *argv[], int argc);

void *getBuiltIn(char *funcName);

void hang(void);

void nohang(void);

void recur(void);

void fileDescriptorCleanup(void);
