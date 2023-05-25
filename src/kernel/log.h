void logToFile(char *processName, char *event, int priority, int processId);

void logClickTick();

void logNice(char *processName, int oldPriority, int newPriority,
             int processId);