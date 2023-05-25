#include <string.h>
#include "inputParse.h"

// gets number of tokens delimeted by white space 
int getNumTokens(char cmd[MAXLINELENGTH]) {
    int numTok = 0;
      int isSpace = 1;
      for (int i = 0; i < MAXLINELENGTH; i++) {
         if (cmd[i] == '\0') {
            break;
         }
         if (cmd[i] == ' ' || cmd[i] == '\t'){
            isSpace = 1;
         } else {
            if (isSpace == 1) {
               isSpace = 0;
               numTok += 1;
            }
         }
      }
    return numTok;
}

// gets tokens delimeted by white space using strtok()
void getTokens(int numTokens, char* tokens[numTokens+1], char cmd[4096]) {
   char *delim = " ";
   int i = 0;
   tokens[i] = strtok(cmd, delim);
   while( tokens[i] != NULL ) {
      i++;
      tokens[i] = strtok(NULL, delim);
   }
}