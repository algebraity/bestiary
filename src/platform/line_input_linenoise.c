#include<ctype.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include "eval.h"
#include "line_input.h"
#include "linenoise.h"

static BestiaryCompletionFn g_completionFn = NULL;

static int commandPrefixStart(const char* line, int cursor) {
    int start = cursor;
    while (start > 0 && isalnum((unsigned char)line[start - 1])) start--;
    if (start > 0 && line[start - 1] == '\\') return start;
    return -1;
}

static int helpArgumentPrefixStart(const char* line, int cursor, char* out_close) {
    int start = cursor;
    while (start > 0 && isalnum((unsigned char)line[start - 1])) start--;
    if (start < 6) return -1;

    int open = start - 1;
    while (open >= 0 && isspace((unsigned char)line[open])) open--;
    if (open < 0 || (line[open] != '{' && line[open] != '(')) return -1;

    char close = line[open] == '{' ? '}' : ')';
    int cmd_end = open - 1;
    while (cmd_end >= 0 && isspace((unsigned char)line[cmd_end])) cmd_end--;
    int cmd_start = cmd_end + 1;
    while (cmd_start > 0 && isalnum((unsigned char)line[cmd_start - 1])) cmd_start--;
    if (cmd_start <= 0 || line[cmd_start - 1] != '\\') return -1;
    if ((size_t)(cmd_end - cmd_start + 1) != strlen("help")) return -1;
    if (strncmp(line + cmd_start, "help", strlen("help")) != 0) return -1;

    int has_close = 0;
    for (int i = cursor; line[i]; i++) {
        if (line[i] == close) {
            has_close = 1;
            break;
        }
    }
    *out_close = has_close ? '\0' : close;
    return start;
}

static void addCommandCompletions(const char* line, linenoiseCompletions* lc) {
    int cursor = (int)strlen(line);
    if (g_completionFn && !g_completionFn(line, cursor)) return;

    int prefix_start = commandPrefixStart(line, cursor);
    char close_char = '\0';
    if (prefix_start < 0) {
        prefix_start = helpArgumentPrefixStart(line, cursor, &close_char);
    }
    if (prefix_start < 0) return;

    size_t prefix_len = (size_t)(cursor - prefix_start);
    size_t line_len = strlen(line);
    char* candidate = NULL;

    for (const CommandEntry* c = commandRegistry(); c; c = c->next) {
        if (strncmp(c->name, "__", 2) == 0) continue;
        if (strncmp(c->name, line + prefix_start, prefix_len) != 0) continue;

        size_t command_len = strlen(c->name);
        size_t close_len = close_char ? 1 : 0;
        size_t candidate_len = (size_t)prefix_start + command_len + close_len + (line_len - (size_t)cursor);
        candidate = malloc(candidate_len + 1);
        if (!candidate) return;
        memcpy(candidate, line, (size_t)prefix_start);
        memcpy(candidate + prefix_start, c->name, command_len);
        if (close_char) candidate[prefix_start + command_len] = close_char;
        memcpy(candidate + prefix_start + command_len + close_len, line + cursor, line_len - (size_t)cursor);
        candidate[candidate_len] = '\0';
        linenoiseAddCompletion(lc, candidate);
        free(candidate);
    }
}

void bstLineInputInit(BestiaryCompletionFn completionFn) {
    g_completionFn = completionFn;
    linenoiseSetCompletionCallback(addCommandCompletions);
}

void bstLineInputShutdown(void) {
    g_completionFn = NULL;
    linenoiseSetCompletionCallback(NULL);
}

void bstLineInputSetPlainMode(int enabled) {
    (void)enabled;
}

void bstLineInputRecoverInterrupt(void) {
    putchar('\n');
    fflush(stdout);
}

char* bstReadLine(const char* prompt) {
    return linenoise(prompt);
}

void bstAddHistory(const char* line) {
    linenoiseHistoryAdd(line);
}
