#include "job_handler.h"
#include "parser.h"
#define MAXLINELENGTH 4096

/**
 * This function is named really poorly since we no longer handle pipes but for
 * backwards compatibility and because it still works, the name will remain.
 * A more accurate name would be `runParsedCommand`
 */
void runPipes(char cmdInput[MAXLINELENGTH], struct parsed_command* cmd,
              job_queue** head);
void signalHandler(int signum);
void poll_children(job_queue** head);