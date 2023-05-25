# PennOS
This is PennOS, the second project of CIS 3800. It contains a mock OS that can run as a process on a host OS
## Names of Group Members

Names: Andrew Jiang (ajay54), Ishaan Lal (ilal), Nathaniel Lao (nlao), Ethan Chee (echee9)

The above are pennkeys, not GitHub usernames.

## Submitted source files 
#### generated by tree(1):
```src
|-- fs
|   |-- fatll.h
|   |-- filell.h
|   |-- inputParse.c
|   |-- inputParse.h
|   |-- pennfat.c
|   |-- pennfat.h
|   |-- shell.c
|   |-- system.c
|   `-- system.h
|-- kernel
|   |-- builtins.c
|   |-- builtins.h
|   |-- job_handler.c
|   |-- job_handler.h
|   |-- kernel-system.c
|   |-- kernel-system.h
|   |-- kernel.c
|   |-- kernel.h
|   |-- log.c
|   |-- log.h
|   |-- parser-aarch64.o
|   |-- parser-x86_64.o
|   |-- parser.h
|   |-- pcb.h
|   |-- penn-shell.c
|   |-- penn-shell.h
|   |-- pipe.c
|   |-- pipe.h
|   |-- queue.c
|   |-- queue.h
|   |-- scheduler.c
|   |-- scheduler.h
|   |-- stress.c
|   |-- stress.h
|   |-- user.c
|   `-- user.h
|-- macros.h
|-- p_errno.c
`-- p_errno.h
```

## Extra Credit Answers
No extra credit was attempted for this project.

## Compilation Instructions
`make [-B]` to make all binaries (`pennos`, `pennfat`)

`make pennos` for just `pennos`

`make pennfat` for just `pennfat`
binary files will be compiled to the `bin` folder.

## Overview of Work Accomplished
### Kernel
We implemented a unix-style kernel with round-robin scheduling w/ 3 priority queues. This kernel implements all of the system calls needed (e.g. `fork = p_spawn`, `kill = p_kill`, etc.) 
Instead of spawning new processes, this kernel uses the `ucontext` library to spawn threads. Therefore, all of this kernel runs as one process under the host OS. In addition, we implemented a lot of built-in shell commands, which can be found under the `builtins.c/h` files. 

In addition to this kernel, we also implemented a logging functionality which will document the statues of a process's lifecycle, in addition to other things. 
### File System
### PennOS (as a whole)
Putting these two parts together, we have a OS which can run as a process under a host OS. It has a shell with job control (albeit no pipes) and redirection, and a file system under 1 root directory which can do reads and writes.

## Description of Code and Code Layout
`macros.h` and `perrno.c/h` contain some common code that is imported in multiple places (i.e. both fs and kernel)

### Kernel
In the kernel, most of the functionality is in `kernel.c`. It handles all the behind-the-scenes work about the kernel including the main method for pennos. `kernel.c` is supported by  `kernel-system.c`, which includes the `k_` system calls. 

`builtins.h/c` handle the shell builtin functions that are listed in the spec. 

`job_handler.h/c` handle job control in the shell.

`log.h/c` handle logging for the scheduler/kernel.

`pcb.h` contains the Pcb struct used throughout the project.

`penn-shell.h/c` contains the shell process (including `read` in a while loop)

`pipe.h/c` contains the (poorly named, see comments in .h file) `runPipes` command, which executes a parsed command (called from the shell). It also interacts with `job_handler.h` for job control purposes.

`queue.h/c` contains the queue struct that is used throughout the kernel (e.g. for child, zombie, etc. queue)

`scheduler.h/c` contains the scheduler and some helpful functions to interact with i and some helpful functions to interact with it.

`stress.h/c` are the provided demo test files.

`user.h/c` implement the user system calls (i.e. `p_`) used elsewhere except in the kernel.

### File System
For the file system, most of the functions utilized when running the standalone file system are housed in `pennfat.c`. System calls can be found in `system.c`. 

`fatll.h` custom data structure used when querying the FAT and obtaining blocks used by file.

`filell.h` custom data structure to keep attributes of files.

`pennfat.h/.c` contains many of the standalone file system functions including touch, rm, ls, etc.

`shell.c` contins the shell used when running standalone pennfat

`system.c/.h` contains all custom written system functions to interact with file system.
## General Comments

