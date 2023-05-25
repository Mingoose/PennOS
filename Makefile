######################################################################
#
#                       Author: Hannah Pan
#                       Date:   01/31/2021
#
# The autograder will run the following command to build the program:
#
#       make -B
#
######################################################################

# name of the program to build
#
PROG=pennos
BUILD_DEST= bin/
PROMPT='"$$ "'
LOG_FILE='"log/log"'
# Remove -DNDEBUG during development if assert(3) is used
#
# override CPPFLAGS += -DNDEBUG -DPROMPT=$(PROMPT)
override CPPFLAGS += -DNDEBUG -DLOG_FILE=$(LOG_FILE) -DPROMPT=$(PROMPT)

CC = clang	

# Replace -O1 with -g for a debug version during development
#
# TODO: change to O1
# CFLAGS = -Wall -Werror -O1
CFLAGS = -Wall -Werror -g

#SRCS = $(wildcard *.c)
SRCS = $(wildcard src/*.c)
KERNEL_SRCS = $(wildcard src/kernel/*.c)
OBJS = $(SRCS:.c=.o)
KERNEL_OBJS = $(KERNEL_SRCS:.c=.o)
FS_SRCS = $(wildcard src/fs/*.c)
FS_OBJS = $(FS_SRCS:.c=.o)
FS_NON_SHELL_SRCS = $(filter-out src/fs/shell.c, $(FS_SRCS))
FS_NON_SHELL_OBJ = $(FS_NON_SHELL_SRCS:.c=.o)
COMMON_SRCS = $(wildcard src/*.c)
COMMON_OBJS = $(COMMON_SRCS:.c=.o)
CREATE_DIR = mkdir -p

.PHONY : clean

all: log bin $(PROG) pennfat
	make log bin $(PROG) pennfat 

bin:
	$(CREATE_DIR) $@

log:
	$(CREATE_DIR) $@

$(PROG) : $(KERNEL_OBJS) $(FS_NON_SHELL_OBJ) $(COMMON_OBJS)
	$(CC) -o $(BUILD_DEST)$@ $^ src/kernel/parser-$(shell uname -p).o
# scheduler: $(KERNEL_OBJS) $(FS_NON_SHELL_OBJ) $(COMMON_OBJS)
# 	$(CC) -o $@ $^ src/kernel/parser-$(shell uname -p).o
pennfat: $(FS_OBJS) $(COMMON_OBJS)
	$(CC) -o $(BUILD_DEST)$@ $^

	
clean :
	$(RM) $(OBJS) $(KERNEL_OBJS) $(BUILD_DEST)$(PROG) $(FS_OBJS) $(BUILD_DEST)pennfat
