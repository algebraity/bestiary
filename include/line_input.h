#ifndef BESTIARY_LINE_INPUT_H
#define BESTIARY_LINE_INPUT_H

typedef int (*BestiaryCompletionFn)(const char* line, int cursor);

void  bstLineInputInit(BestiaryCompletionFn completionFn);
void  bstLineInputShutdown(void);
void  bstLineInputSetPlainMode(int enabled);
void  bstLineInputRecoverInterrupt(void);
char* bstReadLine(const char* prompt);
void  bstAddHistory(const char* line);

#endif
