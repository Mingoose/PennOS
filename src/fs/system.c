#include "pennfat.h"
#include "filell.h"
#include "system.h"
#include "../p_errno.h"

filell* fdList;
extern fileSystem* currFS;
extern int fs_fd;



filell* createFD(int fd, int mode, int offset) {
    filell *entry = calloc(1, sizeof(filell));
    entry->fd = fd;
    entry->refCount = 1;
    entry->mode = mode;
    entry->offset = offset;
    entry->position = 0;
    entry->next = NULL;
    entry->prev = NULL;
    return entry;
}

filell* findFDByName(const char *fname, int mode) {
    filell* curr = fdList;
    directoryEntry* currEntry = calloc(1, ENTRYLENGTH);
    while (curr != NULL) {
        if (curr->mode == mode) {
            lseek(fs_fd, curr->offset, SEEK_SET);
            read(fs_fd, currEntry, ENTRYLENGTH);
            if (strcmp(fname, currEntry->name) == 0) {
                free(currEntry);
                return curr;
            }
        }
        curr = curr->next;
    }
    free(currEntry);
    return NULL;
}

filell* findFD(int fd) {
    filell* curr = fdList;
    while (curr != NULL) {
        if (curr->fd == fd) {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

void f_lseek(int fd, int offset, int whence) {
    filell* curr = findFD(fd);
    if (whence == F_SEEK_SET) {
        curr->position = offset;
    } else if (whence == F_SEEK_CUR) {
        curr->position += offset;
    } else if (whence == F_SEEK_END) {
        directoryEntry* currEntry = calloc(1, ENTRYLENGTH);
        lseek(fs_fd, curr->offset, SEEK_SET);
        read(fs_fd, currEntry, ENTRYLENGTH);
        curr->position = currEntry->size + offset;
        free(currEntry);
    }
}

int f_open(const char *fname, int mode) {
    char name[32];
    strcpy(name, fname);
    if (mode == F_WRITE || mode == F_APPEND) {
        touch(name);
    }
    if (mode == F_WRITE) {
        clearBlocks(name);
    }
    filell* curr = findFDByName(fname, mode);
    if (curr == NULL) {
        int offset = findInDirectory(name);
        if (offset == -1) {
            reset_p_errno();
            p_errno = p_FILEINV;
            return -1;
        }
        filell* list = fdList;
        while (list->next != NULL) {
            list = list->next;
        }
        filell* newNode = createFD((list->fd) + 1, mode, offset);
        newNode->prev = list;
        list->next = newNode;
        if (mode == F_APPEND) {
            f_lseek((list->fd) + 1, 0, F_SEEK_END);
        }
        return ((list->fd) + 1);
    } else {
        if (mode == F_WRITE) {
            reset_p_errno();
            p_errno = p_FDWRITE;
            return -1;
        }
        curr->refCount++;
        return curr->fd;
    }
}

int f_read(int fd, int n, char *buf) {
    filell* curr = findFD(fd);
    if (curr == NULL) {
        reset_p_errno();
        p_errno = p_FDINV;
    }
    if (curr->offset < getFatSize()) {
        size_t line = 0;
        char* buff = NULL;
        int bytesRead = getline(&buff, &line, stdin);
        strncpy(buf, buff, n); 
        if (n < bytesRead) {
            return n;
        }
        return bytesRead;
    } else {
        char name[32];
        lseek(fs_fd, curr->offset, SEEK_SET);
        read(fs_fd, name, 32);
        int ans = readFromOffset(name, buf, curr->position, n);
        curr->position += ans;
        return ans;
    }
}

int f_getLine(int fd, char **lineptr, size_t *n, FILE *stream) {
    filell* curr = findFD(fd);
    directoryEntry* currEntry = calloc(1, ENTRYLENGTH);
    lseek(fs_fd, curr->offset, SEEK_SET);
    read(fs_fd, currEntry, ENTRYLENGTH);
    return 0;
}

int f_write(int fd, const char *str, int n) {
    filell* curr = findFD(fd);
    if (curr == NULL) {
        reset_p_errno();
        p_errno = p_FDINV;
        return -1;
    }
    if (curr->offset < getFatSize()) {
        char buff[n + 1];
        for (int i = 0; i < n+1; i++) {
            buff[i] = '\0';
        }
        strncpy(buff, str, n);
        buff[n] = '\0';
        printf("%s", buff);
        return n;
    } else {
        char name[32];
        lseek(fs_fd, curr->offset, SEEK_SET);
        read(fs_fd, name, 32);
        if (curr->mode == F_READ) {
            reset_p_errno();
            p_errno = p_PERMS;
            return -1;
        }
        writeAtOffset(name, str, curr->position, n);
        curr->position += n;
        return n;
    }
}

int f_close(int fd) {
    filell* curr = findFD(fd);
    if (curr == NULL) {
        reset_p_errno();
        p_errno = p_FDINV;
        return -1;
    }
    if (fd <= 2) {
        reset_p_errno();
        p_errno = p_INVCLOSE;
        return -1;
    }
    if (curr->refCount == 1) {
        //note that prevNode will always exist since head will always be STDIN fd
        (curr->prev)->next = curr->next;
        if (curr->next != NULL) {
            (curr->next)->prev = curr->prev;
        }
        //check the entry to see if file has been removed and is waiting on close
        directoryEntry* currEntry = calloc(1, ENTRYLENGTH);
        lseek(fs_fd, curr->offset, SEEK_SET);
        read(fs_fd, currEntry, ENTRYLENGTH);
        if (currEntry->name[0] == '\2') {
            if (findFDByName(currEntry->name, F_WRITE) == NULL && findFDByName(currEntry->name, F_APPEND) == NULL && findFDByName(currEntry->name, F_READ) == NULL) {
                rm(currEntry->name);
            }
        }
        free(curr);
        free(currEntry);
        return 0;
    } else {
        curr->refCount--;
        return 0;
    }
}

void f_unlink(const char *fname) {
    bool exists = false;
    filell* curr = fdList;
    directoryEntry* currEntry = calloc(1, ENTRYLENGTH);
    char name[32];
    strcpy(name, fname);
    if (findInDirectory(name) == -1) {
        return;
    }
    while (curr != NULL) {
        if (curr->fd > 2) {
            lseek(fs_fd, curr->offset, SEEK_SET);
            read(fs_fd, currEntry, ENTRYLENGTH);
            if (strcmp(name, currEntry->name) == 0) {
                exists = true;
            }
        }
        curr = curr->next;
    }
    if (exists) {
        lseek(fs_fd, findInDirectory(name), SEEK_SET);
        write(fs_fd, "\2", 1);
    } else {
        rm(name);
    }
    free(currEntry);
}

char* f_ls(const char *fileName) {
    if (fileName == NULL) {
        return ls2();
    } else {
        char name[32];
        strcpy(name, fileName);
        directoryEntry* entry = calloc(1, ENTRYLENGTH);
        lseek(fs_fd, findInDirectory(name), SEEK_SET);
        read(fs_fd, entry, ENTRYLENGTH);
        char* perms;
        if (entry->perm == 0) {
            perms = "---";
        } else if (entry->perm == 2) {
            perms = "--w";
        } else if (entry->perm == 4) {
            perms = "-r-";
        } else if (entry->perm == 5) {
            perms = "xr-";
        } else if (entry->perm == 6) {
            perms = "-rw";
        } else if (entry->perm == 7) {
            perms = "xrw";
        } else {
            perms = "---";
        }
        char buffer[80];
        strftime(buffer, 80, "%b %d %H:%M", localtime(&entry->mtime));
        char* ans = malloc(160);

        sprintf(ans, "%3d %s %3d %s %s\n", entry->firstBlock, perms, entry->size, buffer, entry->name);
        return ans;
    }
}

void f_rename(const char* oldName, const char* newName) {
    char oName[32];
    char nName[32];
    strcpy(oName, oldName);
    strcpy(nName, newName);
    changeName(nName, oName);
}


void f_chmod(const char* fileName, int mode) {
    char name[32];
    strncpy(name, fileName, 32);
    if (mode == F_ADDR) {
        modifyPerms(name, "+r");
    } else if (mode == F_ADDW) {
        modifyPerms(name, "+w");
    } else if (mode == F_ADDX) {
        modifyPerms(name, "+x");
    } else if (mode == F_SUBR) {
        modifyPerms(name, "-r");
    } else if (mode == F_SUBW) {
        modifyPerms(name, "-w");
    } else if (mode == F_SUBX) {
        modifyPerms(name, "-x");
    }
    return;
}