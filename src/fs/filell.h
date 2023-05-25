#pragma once

#include "pennfat.h"


typedef struct filell {
    int fd;
    int refCount;
    int mode;
    int position;
    int offset;
    struct filell * next;
    struct filell * prev;
} filell;