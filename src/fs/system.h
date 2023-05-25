#include "pennfat.h"
#include "filell.h"


#define F_SEEK_SET 0
#define F_SEEK_CUR 1
#define F_SEEK_END 2
#define F_READ 0
#define F_WRITE 1
#define F_APPEND 2
#define F_STDIN 0
#define F_STDOUT 1
#define F_STDERR 2
#define F_ADDR 0
#define F_ADDW 1
#define F_ADDX 2
#define F_SUBR 3
#define F_SUBW 4
#define F_SUBX 5


filell* createFD(int fd, int mode, int offset);

filell* findFDByName(const char *fname, int mode);

filell* findFD(int fd);

int f_open(const char *fname, int mode);

int f_read(int fd, int n, char *buf);

int f_write(int fd, const char *str, int n);

int f_close(int fd);

void f_unlink(const char *fname);

void f_lseek(int fd, int offset, int whence);

char* f_ls(const char *fileName);

void f_rename(const char* oldName, const char* newName);

void f_chmod(const char* fileName, int mode);