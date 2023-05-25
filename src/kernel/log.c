#include <stdio.h>

#include "kernel.h"
#include "string.h"  // strlen
void logToFile(char *processName, char *event, int priority, int processId) {
    extern int ticks;
    FILE *fp;
    fp = fopen(logFile, "a");
    if (strlen(event) > 7) {
        fprintf(fp, "[%d]\t%s\t%d\t%d\t%s\n", ticks, event, processId, priority,
                processName);
    } else {
        fprintf(fp, "[%d]\t%s\t\t%d\t%d\t%s\n", ticks, event, processId,
                priority, processName);
    }
    fclose(fp);
}

void logNice(char *processName, int oldPriority, int newPriority,
             int processId) {
    extern int ticks;
    FILE *fp;
    fp = fopen(logFile, "a");
    fprintf(fp, "[%d]\t%s\t%d\t%d\t%d\t%s\n", ticks, "NICE", processId,
            oldPriority, newPriority, processName);
    fclose(fp);
}

void logClickTick() {
    extern int ticks;
    FILE *fp;
    fp = fopen(logFile, "a");
    fprintf(fp, "[%d]\t%s\n", ticks, "CLICK");
    fclose(fp);
}
