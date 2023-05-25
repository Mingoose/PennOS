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





bool catLoop; 
bool isMounted;
fileSystem* currFS;
int fs_fd;

// will be used for "mkfs"
void mkfs(char *fileSystemName, uint8_t blocks_in_fat, uint8_t block_size_config) {
   fileSystem *fs = calloc(1, sizeof(fileSystem));
 
   int fs_fd = open(fileSystemName, O_RDWR|O_CREAT, 0644);
 
   int blockSize = 0;
   if (block_size_config == 0)
    blockSize = 256;
   else if (block_size_config == 1)
       blockSize = 512;
   else if (block_size_config == 2)
       blockSize = 1024;
   else if (block_size_config == 3)
       blockSize = 2048;
   else if (block_size_config == 4)
       blockSize = 4096;
   else {
       printf("block size not in range 1 - 4\n");
       blockSize = 0;
   }
 
   int numEntries = (blocks_in_fat * blockSize) / 2;
   int fat_size = blocks_in_fat * blockSize;

   uint16_t metadata = (blocks_in_fat << 8) | block_size_config;
 
   fs->fat = calloc(1, fat_size);
   fs->dataRegion = calloc(1, blockSize * (numEntries - 1));
   fs->fat[0] = metadata;
   fs->fat[1] = 0xFFFF;

   if (write(fs_fd, fs->fat, fat_size) == -1) {
       write(STDOUT_FILENO, "write error", sizeof("write error"));
   };
   if (write(fs_fd, fs->dataRegion, blockSize * (numEntries - 1)) == -1) {
       write(STDOUT_FILENO, "write error", sizeof("write error"));
   }
   close(fs_fd);
   free(fs);
}

int getBlockSize() {
    uint16_t metadata = currFS->fat[0];
    int block_size_config = metadata & 0xFF;
    int blockSize = 0;
    if (block_size_config == 0)
        blockSize = 256;
    else if (block_size_config == 1)
        blockSize = 512;
    else if (block_size_config == 2)
        blockSize = 1024;
    else if (block_size_config == 3)
        blockSize = 2048;
    else if (block_size_config == 4)
        blockSize = 4096;
    else {
        printf("block size not in range 1 - 4\n");
        blockSize = 0;
    }
    return blockSize;
}

int getFatSize() {
    uint16_t metadata = currFS->fat[0];
    int block_size_config = metadata & 0xFF;
    int blockSize = 0;
    if (block_size_config == 0)
        blockSize = 256;
    else if (block_size_config == 1)
        blockSize = 512;
    else if (block_size_config == 2)
        blockSize = 1024;
    else if (block_size_config == 3)
        blockSize = 2048;
    else if (block_size_config == 4)
        blockSize = 4096;
    else {
        printf("block size not in range 1 - 4\n");
        blockSize = 0;
    }
    return blockSize * (metadata >> 8);
}
 
fileSystem * mount(char* fsName) {
    fs_fd = open(fsName, O_RDWR, 0644);
    char metaBytes[4];
    if (read(fs_fd, metaBytes, 4) == -1) {
        perror("read failed!");
    }; 
    uint16_t metadata = (int) metaBytes[0] | ((int) metaBytes[1] << 8) | ((int) metaBytes[2] << 16) | ((int) metaBytes[3] << 24);
    int block_size_config = metadata & 0xFF;

    int blockSize = 0;
    if (block_size_config == 0)
        blockSize = 256;
    else if (block_size_config == 1)
        blockSize = 512;
    else if (block_size_config == 2)
        blockSize = 1024;
    else if (block_size_config == 3)
        blockSize = 2048;
    else if (block_size_config == 4)
        blockSize = 4096;
    else {
        printf("block size not in range 1 - 4\n");
        blockSize = 0;
    }
    
    int fat_size = blockSize * (metadata >> 8);

    if (lseek(fs_fd, 0, SEEK_SET) == -1) {
        perror("lseek failed!");
    }

    currFS->fat = mmap(NULL, fat_size, PROT_READ | PROT_WRITE, MAP_SHARED, fs_fd, 0);
    if (currFS->fat == MAP_FAILED) {
        perror("mount failed!");
    }
    
    isMounted = true;
    return currFS;
}

void unmount() {
    munmap(currFS->fat, getFatSize());
    close(fs_fd);
    isMounted = false;
}


/*
* pass in file system and starting index in fat of file. Returns linked list of block numbers (in order) for that file.
*/

fatll * getBlocks(int start) {
    fatll * head = malloc(sizeof(fatll));
    head->index = start;
    head->next = NULL;
    if (currFS->fat[start] == 0xFFFF) {
        return head;
    }
    fatll * curr = head;
    while (currFS->fat[curr->index] != 0xFFFF) {
        fatll * next = malloc(sizeof(fatll));
        next->index = currFS->fat[curr->index];
        next->next = NULL;
        curr->next = next;
        curr = next;
    }
    return head;
}


 
int findInDirectory(char* name) {
    int blockSize = getBlockSize();
    int fatSize = getFatSize();
    fatll* head = getBlocks(1);
    fatll* currBlock = head;
    directoryEntry* entry = calloc(1, ENTRYLENGTH);
    while (currBlock != NULL) {
        for (int counter = 0; counter < blockSize / ENTRYLENGTH; counter++) {
            lseek(fs_fd, fatSize + ((currBlock->index - 1) * blockSize) + counter * ENTRYLENGTH, SEEK_SET);
            read(fs_fd, entry, ENTRYLENGTH);
            if (strcmp(entry->name, name) == 0) {
                return fatSize + ((currBlock->index - 1) * blockSize) + counter * ENTRYLENGTH;
            }
        }
        currBlock = currBlock->next;
    }
    return -1;
}

void clearBlocks(char* fileName) {
    int offset = findInDirectory(fileName);
    lseek(fs_fd, offset + 36, SEEK_SET);
    uint16_t firstBlock;
    read(fs_fd, &firstBlock, 2);
    if (firstBlock == 0) {
        return;
    }
    lseek(fs_fd, offset + 32, SEEK_SET);
    for (int i = 0; i < 4; i++) {
        write(fs_fd, "\0", 1);
    }
    int fatSize = getFatSize();
    int blockSize = getBlockSize();
    fatll* currBlock = getBlocks(firstBlock);
    currFS->fat[currBlock->index] = 0xFFFF;
    lseek(fs_fd, fatSize + (currBlock->index - 1) * blockSize, SEEK_SET);
    for (int i = 0; i < blockSize; i++) {
        write(fs_fd, "\0", 1);
    }
    currBlock = currBlock->next;
    while (currBlock != NULL) {
        currFS->fat[currBlock->index] = 0;
        lseek(fs_fd, fatSize + (currBlock->index - 1) * blockSize, SEEK_SET);
        for (int i = 0; i < blockSize; i++) {
            write(fs_fd, "\0", 1);
        }
        currBlock = currBlock->next;
    }
}

void rm(char* fileName) {
    int offset = findInDirectory(fileName);
    if (offset == -1) {
        printf("file does not exist!\n");
        return;
    }
    clearBlocks(fileName);
    uint16_t firstBlock;
    lseek(fs_fd, offset + 36, SEEK_SET);
    read(fs_fd, &firstBlock, 2);
    if (firstBlock != 0) {
        currFS->fat[firstBlock] = 0;
    }
    lseek(fs_fd, offset, SEEK_SET);
    write(fs_fd, "\1", 1);
}

void changeName(char* newName, char* fileName) {
    int offset = findInDirectory(fileName);
    if (offset == -1) {
        printf("file does not exist!\n");
        return;
    }
    lseek(fs_fd, offset, SEEK_SET);
    for (int i = 0; i < 32; i++) {
        write(fs_fd, "\0", 1);
    }
    lseek(fs_fd, -32, SEEK_CUR);
    write(fs_fd, newName, strlen(newName));
}

/*
* Returns index of first free spot in FAT (where value is not initialized aka 0).
*/
int findFreeFatSpot() {
    for (int i = 0; i < getFatSize() / 2; i++) {
        if (currFS->fat[i] == 0) {
            return i;
        }
    }
    return -1;
}


void addToFile(char* fileName, char* toWrite) {
    int offset = findInDirectory(fileName);
    int blockSize = getBlockSize();
    lseek(fs_fd, offset + 36, SEEK_SET);
    uint16_t firstBlock;
    read(fs_fd, &firstBlock, 2);
    if (firstBlock != 0) {
        fatll* currBlock = getBlocks(firstBlock);
        while (currBlock->next != NULL) {
            currBlock = currBlock->next;
        }
        uint32_t size;
        lseek(fs_fd, offset + 32, SEEK_SET);
        read(fs_fd, &size, 4);
        int sizeInBlock = size;
        if (size > blockSize) {
            sizeInBlock = size % blockSize;
        }
        lseek(fs_fd, getFatSize() + (currBlock->index - 1) * blockSize + sizeInBlock, SEEK_SET);    
        if (sizeInBlock + strlen(toWrite) <= blockSize) {
            write(fs_fd, toWrite, strlen(toWrite));
        } else {
            write(fs_fd, toWrite, blockSize - sizeInBlock);
            int nextFreeBlock =  findFreeFatSpot();
            if (nextFreeBlock == -1) {
                printf("fat is full!\n");
                return;
            }
            currFS->fat[currBlock->index] = nextFreeBlock;
            currFS->fat[nextFreeBlock] = 0xFFFF;
            lseek(fs_fd, getFatSize() + (nextFreeBlock - 1) * blockSize, SEEK_SET);
            int bytesWritten = blockSize - sizeInBlock;
            int bytesToWrite = strlen(toWrite) - bytesWritten;
            while (bytesToWrite > 0) {
                if (bytesToWrite > blockSize) {
                    write(fs_fd, toWrite + bytesWritten, blockSize);
                    bytesWritten += blockSize;
                    bytesToWrite -= blockSize;
                    int currBlock = nextFreeBlock;
                    nextFreeBlock = findFreeFatSpot();
                    if (nextFreeBlock == -1) {
                        printf("fat is full!\n");
                        return;
                    }
                    currFS->fat[currBlock] = nextFreeBlock;
                    currFS->fat[nextFreeBlock] = 0xFFFF;
                    lseek(fs_fd, getFatSize() + (nextFreeBlock - 1) * blockSize, SEEK_SET);
                } else {
                    write(fs_fd, toWrite + bytesWritten, bytesToWrite);
                    bytesWritten += bytesToWrite;
                    bytesToWrite = 0;
                }
            }
        }
        size += strlen(toWrite);
        lseek(fs_fd, offset + 32, SEEK_SET);
        write(fs_fd, &size, sizeof(size));
    } else {
        int nextFreeBlock = findFreeFatSpot();
        if (nextFreeBlock == -1) {
            printf("fat is full!\n");
            return;
        }
        currFS->fat[nextFreeBlock] = 0xFFFF;
        lseek(fs_fd, offset + 36, SEEK_SET);
        write(fs_fd, &nextFreeBlock, 2);
        lseek(fs_fd, getFatSize() + (nextFreeBlock - 1) * blockSize, SEEK_SET);
        int bytesWritten = 0;
        int bytesToWrite = strlen(toWrite);
        while (bytesToWrite > 0) {
            if (bytesToWrite > blockSize) {
                write(fs_fd, toWrite + bytesWritten, blockSize);
                bytesWritten += blockSize;
                bytesToWrite -= blockSize;
                int currBlock = nextFreeBlock;
                nextFreeBlock = findFreeFatSpot();
                if (nextFreeBlock == -1) {
                    printf("fat is full!\n");
                    return;
                }
                currFS->fat[currBlock] = nextFreeBlock;
                currFS->fat[nextFreeBlock] = 0xFFFF;
                lseek(fs_fd, getFatSize() + (nextFreeBlock - 1) * blockSize, SEEK_SET);
            } else {
                write(fs_fd, toWrite + bytesWritten, bytesToWrite);
                bytesWritten += bytesToWrite;
                bytesToWrite = 0;
            }
        }
        uint32_t size = strlen(toWrite);
        lseek(fs_fd, offset + 32, SEEK_SET);
        write(fs_fd, &size, sizeof(size));
    }
}

//function for writing string with a null character
void addNullToFile(char* fileName, char* toWrite, int ishaan) {
    int offset = findInDirectory(fileName);
    int blockSize = getBlockSize();
    lseek(fs_fd, offset + 36, SEEK_SET);
    uint16_t firstBlock;
    read(fs_fd, &firstBlock, 2);
    if (firstBlock != 0) {
        fatll* currBlock = getBlocks(firstBlock);
        while (currBlock->next != NULL) {
            currBlock = currBlock->next;
        }
        uint32_t size;
        lseek(fs_fd, offset + 32, SEEK_SET);
        read(fs_fd, &size, 4);
        int sizeInBlock = size;
        if (size > blockSize) {
            sizeInBlock = size % blockSize;
        }
        lseek(fs_fd, getFatSize() + (currBlock->index - 1) * blockSize + sizeInBlock, SEEK_SET);    
        if (sizeInBlock +  ishaan <= blockSize) {
            write(fs_fd, toWrite, ishaan);
        } else {
            write(fs_fd, toWrite, blockSize - sizeInBlock);
            int nextFreeBlock =  findFreeFatSpot();
            if (nextFreeBlock == -1) {
                printf("fat is full!\n");
                return;
            }
            currFS->fat[currBlock->index] = nextFreeBlock;
            currFS->fat[nextFreeBlock] = 0xFFFF;
            lseek(fs_fd, getFatSize() + (nextFreeBlock - 1) * blockSize, SEEK_SET);
            int bytesWritten = blockSize - sizeInBlock;
            int bytesToWrite = ishaan - bytesWritten;
            while (bytesToWrite > 0) {
                if (bytesToWrite > blockSize) {
                    write(fs_fd, toWrite + bytesWritten, blockSize);
                    bytesWritten += blockSize;
                    bytesToWrite -= blockSize;
                    int currBlock = nextFreeBlock;
                    nextFreeBlock = findFreeFatSpot();
                    if (nextFreeBlock == -1) {
                        printf("fat is full!\n");
                        return;
                    }
                    currFS->fat[currBlock] = nextFreeBlock;
                    currFS->fat[nextFreeBlock] = 0xFFFF;
                    lseek(fs_fd, getFatSize() + (nextFreeBlock - 1) * blockSize, SEEK_SET);
                } else {
                    write(fs_fd, toWrite + bytesWritten, bytesToWrite);
                    bytesWritten += bytesToWrite;
                    bytesToWrite = 0;
                }
            }
        }
        size += ishaan;
        lseek(fs_fd, offset + 32, SEEK_SET);
        write(fs_fd, &size, sizeof(size));
    } else {
        int nextFreeBlock = findFreeFatSpot();
        if (nextFreeBlock == -1) {
            printf("fat is full!\n");
            return;
        }
        currFS->fat[nextFreeBlock] = 0xFFFF;
        lseek(fs_fd, offset + 36, SEEK_SET);
        write(fs_fd, &nextFreeBlock, 2);
        lseek(fs_fd, getFatSize() + (nextFreeBlock - 1) * blockSize, SEEK_SET);
        int bytesWritten = 0;
        int bytesToWrite = ishaan;
        while (bytesToWrite > 0) {
            if (bytesToWrite > blockSize) {
                write(fs_fd, toWrite + bytesWritten, blockSize);
                bytesWritten += blockSize;
                bytesToWrite -= blockSize;
                int currBlock = nextFreeBlock;
                nextFreeBlock = findFreeFatSpot();
                if (nextFreeBlock == -1) {
                    printf("fat is full!\n");
                    return;
                }
                currFS->fat[currBlock] = nextFreeBlock;
                currFS->fat[nextFreeBlock] = 0xFFFF;
                lseek(fs_fd, getFatSize() + (nextFreeBlock - 1) * blockSize, SEEK_SET);
            } else {
                write(fs_fd, toWrite + bytesWritten, bytesToWrite);
                bytesWritten += bytesToWrite;
                bytesToWrite = 0;
            }
        }
        uint32_t size = ishaan;
        lseek(fs_fd, offset + 32, SEEK_SET);
        write(fs_fd, &size, sizeof(size));
    }
}

directoryEntry* createDirectoryElement(char* name, uint32_t size, uint16_t firstBlock, uint8_t type, uint8_t perm, int mtime) {
   // create new entry
   directoryEntry *entry = calloc(1, ENTRYLENGTH);
   entry->size = size;
   entry->firstBlock = firstBlock;
   entry->type = type;
   entry->perm = perm;
   entry->mtime = mtime;
 
   strcpy(entry->name, name);
   return entry;
}


 
void touch(char* name) {
    int offset = findInDirectory(name);
    int blockSize = getBlockSize();
    int fatSize = getFatSize();
    if (offset == -1) {
        fatll* head = getBlocks(1);
        directoryEntry* currEntry = calloc(1, ENTRYLENGTH);
        lseek(fs_fd, fatSize + ((head->index - 1) * blockSize), SEEK_SET);
        read(fs_fd, currEntry, ENTRYLENGTH);
        int counter = 0;
        while (head->next != NULL) {
            while (currEntry->name[0] != '\0' && currEntry->name[0] != '\1' && counter < blockSize / ENTRYLENGTH) {
                read(fs_fd, currEntry, ENTRYLENGTH);
                counter++;
            }
            if (currEntry->name[0] == '\0' || currEntry->name[0] == '\1') {
                break;
            } else {
                head = head->next;
                counter = 0;
                lseek(fs_fd, fatSize + ((head->index - 1) * blockSize), SEEK_SET);
                read(fs_fd, currEntry, ENTRYLENGTH);
            }
        }
        while (currEntry->name[0] != '\0' && currEntry->name[0] != '\1' && counter < blockSize / ENTRYLENGTH) {
            read(fs_fd, currEntry, ENTRYLENGTH);
            counter++;
        }
        directoryEntry* newEntry;
        if (counter == blockSize / ENTRYLENGTH) {
            int newFileBlock = findFreeFatSpot();
            if (newFileBlock == -1) {
                printf("fat is full!\n");
                return;
            }
            (currFS->fat)[head->index] = newFileBlock;
            (currFS->fat)[newFileBlock] = 0xFFFF;
            lseek(fs_fd, fatSize + (newFileBlock - 1) * blockSize, SEEK_SET);
            newEntry = createDirectoryElement(name, 0, 0, 1, 6, time(NULL));
            write(fs_fd, newEntry, ENTRYLENGTH);
        } else {
            newEntry = createDirectoryElement(name, 0, 0, 1, 6, time(NULL));
            lseek(fs_fd, -ENTRYLENGTH, SEEK_CUR);
            write(fs_fd, newEntry, ENTRYLENGTH);
        }
    } else {
        lseek(fs_fd, offset + 40, SEEK_SET);
        time_t currTime = time(NULL);
        write(fs_fd, &currTime, sizeof(time_t));
    }
}

void printFile(char* name) {
    int offset = findInDirectory(name);
    if (offset == -1) {
        printf("file not found!\n");
    }
    int blockSize = getBlockSize();
    int fatSize = getFatSize();
    lseek(fs_fd, offset + 36, SEEK_SET);
    uint16_t firstBlock;
    read(fs_fd, &firstBlock, 2);
    if (firstBlock == 0) {
        return;
    }
    fatll* currBlock = getBlocks(firstBlock);
    while (currBlock != NULL) {
        int blockNum = currBlock->index;
        char* blockValue = malloc(blockSize + 1);
        lseek(fs_fd, fatSize + (blockNum - 1) * blockSize, SEEK_SET);
        read(fs_fd, blockValue, blockSize);
        blockValue[blockSize+1] = '\0';
        printf("%s", blockValue);
        currBlock = currBlock -> next;
    }
    printf("\n");
}

void writeFileToOS(char* name, char* OSname) {
    int offset = findInDirectory(name);
    int blockSize = getBlockSize();
    int fatSize = getFatSize();
    lseek(fs_fd, offset + 36, SEEK_SET);
    uint16_t firstBlock;
    read(fs_fd, &firstBlock, 2);
    if (firstBlock == 0) {
        return;
    }
    fatll* currBlock = getBlocks(firstBlock);
    int OSfd = open(OSname, O_RDWR | O_CREAT, 0644);
    while (currBlock != NULL) {
        int blockNum = currBlock->index;
        char* blockValue = malloc(blockSize + 1);
        lseek(fs_fd, fatSize + (blockNum - 1) * blockSize, SEEK_SET);
        read(fs_fd, blockValue, blockSize);
        blockValue[blockSize+1] = '\0';
        write(OSfd, blockValue, blockSize);
        currBlock = currBlock -> next;
    }
}

int checkInDirectory(char* name) {
    int offset = findInDirectory(name);
    return offset;
}

void copyFromFile(char* name, char* targetName) {
    int offset = findInDirectory(name);
    if (offset == -1) {
        printf("file not found!\n");
        return;
    }
    int blockSize = getBlockSize();
    int fatSize = getFatSize();
    lseek(fs_fd, offset + 36, SEEK_SET);
    uint16_t firstBlock;
    read(fs_fd, &firstBlock, 2);
    if (firstBlock == 0) {
        return;
    }
    fatll* currBlock = getBlocks(firstBlock);
    while (currBlock != NULL) {
        int blockNum = currBlock->index;
        char* blockValue = malloc(blockSize + 1);
        lseek(fs_fd, fatSize + (blockNum - 1) * blockSize, SEEK_SET);
        read(fs_fd, blockValue, blockSize);
        blockValue[blockSize+1] = '\0';
        addToFile(targetName, blockValue);
        currBlock = currBlock -> next;
    }
}
 
void ls() {
    int blockSize = getBlockSize();
    int fatSize = getFatSize();
    fatll* head = getBlocks(1);
    fatll* currBlock = head;
    directoryEntry* entry = calloc(1, ENTRYLENGTH);
    while (currBlock != NULL) {
        for (int counter = 0; counter < blockSize / ENTRYLENGTH; counter++) {
            lseek(fs_fd, fatSize + ((currBlock->index - 1) * blockSize) + counter * ENTRYLENGTH, SEEK_SET);
            read(fs_fd, entry, ENTRYLENGTH);
            if (entry->name[0] != '\0' && entry->name[0] != '\1' && entry->name[0] != '\2') {
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
                printf("%3d %s %3d %s %s\n", entry->firstBlock, perms, entry->size, buffer, entry->name);
            }
        }
        currBlock = currBlock->next;
    }
}

char *ls2() {
    int blockSize = getBlockSize();
    int fatSize = getFatSize();
    fatll* head = getBlocks(1);
    fatll* currBlock = head;
    directoryEntry* entry = calloc(1, ENTRYLENGTH);
    char* currStr = malloc(1);
    for (int i = 0; i < strlen(currStr); i++) {
        currStr[i] = '\0';
    }
    while (currBlock != NULL) {
        for (int counter = 0; counter < blockSize / ENTRYLENGTH; counter++) {

            lseek(fs_fd, fatSize + ((currBlock->index - 1) * blockSize) + counter * ENTRYLENGTH, SEEK_SET);
            read(fs_fd, entry, ENTRYLENGTH);
            if (entry->name[0] != '\0' && entry->name[0] != '\1' && entry->name[0] != '\2') {
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
                char ans[160];
                sprintf(ans, "%3d %s %3d %s %s\n", entry->firstBlock, perms, entry->size, buffer, entry->name);
                char* copyCurrStr = malloc(strlen(currStr) + 1);
                strcpy(copyCurrStr, currStr);
                free(currStr);
                currStr = malloc(strlen(copyCurrStr) + strlen(ans)+2);
                for (int i = 0; i < strlen(copyCurrStr) + strlen(ans)+2; i++) {
                    currStr[i] = '\0';
                }
                for (int i = 0; i < strlen(copyCurrStr); i++) {
                    currStr[i] = copyCurrStr[i];
                }
                for (int i = 0; i < strlen(ans); i++) {
                    currStr[i+strlen(copyCurrStr)] = ans[i];
                }
                currStr[strlen(copyCurrStr) + strlen(ans)] = '\0';
                free(copyCurrStr);

            }
        }
        currBlock = currBlock->next;
    }
    return currStr;
}

void copyFromOSFile(char* name, char* OSname) {
    int OSfd = open(OSname, O_RDONLY, 0644);
    struct stat stats;
    fstat(OSfd, &stats);
    off_t lengthOfFile = stats.st_size;
    char* fileContents = malloc(lengthOfFile + 1);
    read(OSfd, fileContents, lengthOfFile);
    fileContents[lengthOfFile] = '\0';
    addNullToFile(name, fileContents, lengthOfFile);
    free(fileContents);
}

void modifyPerms(char* name, char* perms) {
    int offset = findInDirectory(name);
    uint8_t currPerms;
    lseek(fs_fd, offset + 39, SEEK_SET);
    read(fs_fd, &currPerms, 1);
    if (perms[0] == '+') {
        if (perms[1] == 'w') {
            if (currPerms == 0 || currPerms == 4 || currPerms == 5) {
                currPerms+=2;
            }
        } else if (perms[1] == 'r') {
            if (currPerms == 0 || currPerms == 2) {
                currPerms+=4;
            }
        } else if (perms[1] == 'x') {
            if (currPerms == 4 || currPerms == 6) {
                currPerms += 1;
            }
        }
    } else {
        if (perms[1] == 'w') {
            if (currPerms == 2 || currPerms == 6 || currPerms == 7) {
                currPerms -= 2;
            }
        } else if (perms[1] == 'r') {
            if (currPerms == 4 || currPerms == 6) {
                currPerms -= 4;
            }
        } else if (perms[1] == 'x') {
            if (currPerms == 5 || currPerms == 7) {
                currPerms -= 1;
            }
        }
    }
    lseek(fs_fd, offset + 39, SEEK_SET);
    write(fs_fd, &currPerms, 1);
}

void writeAtOffset(char* name, const char* toWrite, int position, int n) {
    int offset = findInDirectory(name);
    int fatSize = getFatSize();
    int blockSize = getBlockSize();
    char stringToWrite[n];
    for (int i = 0; i < n; i++) {
        stringToWrite[n] = '\0';
    }
    strncpy(stringToWrite, toWrite, n);
    uint16_t firstBlock;
    lseek(fs_fd, offset + 36, SEEK_SET);
    read(fs_fd, &firstBlock, 2);
    lseek(fs_fd, offset + 32, SEEK_SET);
    uint32_t size;
    read(fs_fd, &size, 4);
    if (firstBlock == 0) {
        addToFile(name, stringToWrite);
    } else {
        fatll* currBlock = getBlocks(firstBlock);
        while (position >= blockSize) {
            position -= blockSize;
            currBlock = currBlock->next;
        }
        int bytesLeft = blockSize - position;
        int amountWritten = 0;
        int amountToWrite = n;
        lseek(fs_fd, fatSize + (currBlock->index - 1) * blockSize + position, SEEK_SET);
        if (amountToWrite < bytesLeft) {
            write(fs_fd, stringToWrite, amountToWrite);
            amountWritten += amountToWrite;
            amountToWrite = 0;
        } else {
            write(fs_fd, stringToWrite, bytesLeft);
            amountWritten += bytesLeft;
            amountToWrite -= bytesLeft;
        }
        while (amountToWrite > 0) {
            int nextBlock;
            if (currBlock->next != NULL) {
                currBlock = currBlock->next;
                nextBlock = currBlock->index;
            } else {
                nextBlock =  findFreeFatSpot();
                if (nextBlock == -1) {
                    printf("fat is full!\n");
                    return;
                }
                currFS->fat[currBlock->index] = nextBlock;
                currFS->fat[nextBlock] = 0xFFFF;
            }
            lseek(fs_fd, fatSize + (nextBlock - 1) * blockSize, SEEK_SET);
            if (amountToWrite > blockSize) {
                write(fs_fd, stringToWrite + amountWritten, blockSize);
                amountToWrite -= blockSize;
                amountWritten += blockSize;
            } else {
                write (fs_fd, stringToWrite + amountWritten, amountToWrite);
                amountToWrite -= blockSize;
                amountWritten += blockSize;
            }
        }
        size += amountWritten;
        lseek(fs_fd, offset + 32, SEEK_SET);
        write(fs_fd, &size, sizeof(size));
    }
}

int readFromOffset(char* name, char* buffer, int position, int n) {
    int offset = findInDirectory(name);
    int fatSize = getFatSize();
    int blockSize = getBlockSize();
    int amountRead = 0;
    int amountToRead = n;
    uint16_t firstBlock;
    lseek(fs_fd, offset + 36, SEEK_SET);
    read(fs_fd, &firstBlock, 2);
    uint32_t size;
    lseek(fs_fd, offset + 32, SEEK_SET);
    read(fs_fd, &size, 4);
    if (position + amountToRead > size) {
        amountToRead = size - position;
    }
    if (firstBlock == 0) {
        return 0;
    } else {
        fatll* currBlock = getBlocks(firstBlock);
        while (position >= blockSize) {
            position -= blockSize;
            currBlock = currBlock->next;
        }
        lseek(fs_fd, fatSize + (currBlock->index - 1) * blockSize + position, SEEK_SET);
        if (blockSize - position > amountToRead) {
            read(fs_fd, buffer, amountToRead);
            amountRead = amountToRead;
            amountToRead = 0;
        } else {
            read(fs_fd, buffer, blockSize - position);
            amountRead = blockSize - position;
            amountToRead -= blockSize - position;
        }
        while (amountToRead > 0) {
            if (amountToRead > blockSize) {
                currBlock = currBlock->next;
                lseek(fs_fd, fatSize + (currBlock->index - 1) * blockSize, SEEK_SET);
                read(fs_fd, buffer + amountRead, blockSize);
                amountRead += blockSize;
                amountToRead -= blockSize;
            } else {
                currBlock = currBlock->next;
                lseek(fs_fd, fatSize + (currBlock->index - 1) * blockSize, SEEK_SET);
                read(fs_fd, buffer + amountRead, amountToRead);
                amountRead += blockSize;
                amountToRead -= blockSize;
            }
        }
        return amountRead;
    }
}
