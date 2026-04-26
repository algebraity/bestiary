#ifndef BESTIARY_LINENOISE_H
#define BESTIARY_LINENOISE_H

#include<stddef.h>

typedef struct linenoiseCompletions {
    size_t len;
    char** cvec;
} linenoiseCompletions;

typedef void(linenoiseCompletionCallback)(const char* line, linenoiseCompletions* lc);

void  linenoiseSetCompletionCallback(linenoiseCompletionCallback* fn);
void  linenoiseAddCompletion(linenoiseCompletions* lc, const char* str);
char* linenoise(const char* prompt);
int   linenoiseHistoryAdd(const char* line);
void  linenoiseFree(void* ptr);

#endif
