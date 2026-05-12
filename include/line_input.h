#ifndef BESTIARY_LINE_INPUT_H
#define BESTIARY_LINE_INPUT_H

#include<stddef.h>

typedef struct EvalContext EvalContext;
typedef int (*BestiaryCompletionFn)(const char* line, int cursor);

typedef struct {
    const char* initial;
    size_t protectedLen;
    const char* const* blockLines;
    size_t blockLineCount;
} BestiaryLineContext;

void  bstLineInputInit(BestiaryCompletionFn completionFn);
void  bstLineInputShutdown(void);
void  bstLineInputSetPlainMode(int enabled);
void  bstLineInputSetEvalContext(EvalContext* ctx);
void  bstLineInputRecoverInterrupt(void);
char* bstReadLine(const char* prompt);
char* bstReadLineWithInitial(const char* prompt, const char* initial);
char* bstReadLineWithContext(const char* prompt, const BestiaryLineContext* context, int* selectedBlockLine);
void  bstAddHistory(const char* line);

#endif
