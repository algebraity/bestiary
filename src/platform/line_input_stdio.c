#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include "line_input.h"

void bstLineInputInit(BestiaryCompletionFn completionFn) {
    (void)completionFn;
}

void bstLineInputShutdown(void) {
}

void bstLineInputSetPlainMode(int enabled) {
    (void)enabled;
}

void bstLineInputSetEvalContext(EvalContext* ctx) {
    (void)ctx;
}

void bstLineInputRecoverInterrupt(void) {
    putchar('\n');
    fflush(stdout);
}

char* bstReadLine(const char* prompt) {
    return bstReadLineWithInitial(prompt, NULL);
}

char* bstReadLineWithInitial(const char* prompt, const char* initial) {
    BestiaryLineContext context = {
        .initial = initial,
        .protectedLen = 0,
        .blockLines = NULL,
        .blockLineCount = 0
    };
    return bstReadLineWithContext(prompt, &context, NULL);
}

char* bstReadLineWithContext(const char* prompt, const BestiaryLineContext* context, int* selectedBlockLine) {
    char stackBuf[1024];
    char* out = NULL;
    size_t len = 0;
    size_t cap = 0;
    const char* initial = context ? context->initial : NULL;

    if (selectedBlockLine) *selectedBlockLine = -1;

    if (prompt) {
        fputs(prompt, stdout);
        fflush(stdout);
    }
    if (initial && *initial) {
        fputs(initial, stdout);
        fflush(stdout);
    }

    for (;;) {
        if (!fgets(stackBuf, sizeof(stackBuf), stdin)) {
            free(out);
            return NULL;
        }

        size_t chunkLen = strlen(stackBuf);
        if (len + chunkLen + 1 > cap) {
            size_t nextCap = cap ? cap * 2 : 1024;
            while (nextCap < len + chunkLen + 1) nextCap *= 2;
            char* next = realloc(out, nextCap);
            if (!next) {
                free(out);
                return NULL;
            }
            out = next;
            cap = nextCap;
        }

        memcpy(out + len, stackBuf, chunkLen);
        len += chunkLen;
        out[len] = '\0';

        if (len > 0 && out[len - 1] == '\n') {
            out[--len] = '\0';
            if (len > 0 && out[len - 1] == '\r') out[--len] = '\0';
            return out;
        }

        if (chunkLen < sizeof(stackBuf) - 1) return out;
    }
}

void bstAddHistory(const char* line) {
    (void)line;
}
