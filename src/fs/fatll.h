#pragma once

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

typedef struct fatll {
    int index;
    struct fatll *next;
} fatll;