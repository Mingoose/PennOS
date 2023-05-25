#pragma once

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
#define MAXLINELENGTH 4096
#define ENTRYLENGTH 64

typedef struct directoryEntry {
    char name[32]; // null-terminated file name. 
    // name[0] = 0 --> end of directory
    // name[0] = 1 --> deleted entry and file is deleted
    // name[0] = 2 --> deleted entry, file is still being used
    uint32_t size; // number of bytes in file
    uint16_t firstBlock; // first block number of file. undefined if size = 0
    uint8_t type; // 0 = unknown, 1 = regular, 2 = directory file, 4 = symbolic link
    uint8_t perm; // 0 = none, 2 = write, 4 = read, 5 = read & exec, 6 = read & write, 7 = read, write, exec
    time_t mtime; // creation/modification time (use time(2))s
} directoryEntry;


typedef struct fileSystem {
    uint16_t *fat;
    uint16_t *dataRegion;
} fileSystem;

void mkfs(char *filename, uint8_t blocks_in_fat, uint8_t block_size_config);

fileSystem * mount(char* fsName);

void unmount();

fatll * getBlocks(int start);

int findInDirectory(char* name);

int getBlockSize();

int getFatSize();

void clearBlocks(char* fileName);

void rm(char* fileName);

void changeName(char* newName, char* fileName);

int findFreeFatSpot();

void addToFile(char* fileName, char* toWrite);

directoryEntry* createDirectoryElement(char* name, uint32_t size, uint16_t firstBlock, uint8_t type, uint8_t perm, int mtime);

void touch(char* name);

void printFile(char* name);

void writeFileToOS(char* name, char* OSname);

void copyFromFile(char* name, char* targetName);

void ls();

char *ls2();

void copyFromOSFile(char* name, char* OSname);

void modifyPerms(char* name, char* perms);

void writeAtOffset(char* name, const char* toWrite, int position, int n);

int readFromOffset(char* name, char* buffer, int position, int n);

int checkInDirectory(char* name);