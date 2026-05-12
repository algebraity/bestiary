#include<ctype.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include "eval.h"
#include "line_input.h"
#include "linenoise.h"

static BestiaryCompletionFn g_completionFn = NULL;
static EvalContext* g_completionCtx = NULL;

static int completionNameChar(int ch) {
    return isalnum((unsigned char)ch);
}

static int commandPrefixStart(const char* line, int cursor) {
    int start = cursor;
    while (start > 0 && completionNameChar(line[start - 1])) start--;
    if (start > 0 && line[start - 1] == '\\') return start;
    return -1;
}

static int variablePrefixStart(const char* line, int cursor) {
    int start = cursor;
    while (start > 0 && completionNameChar(line[start - 1])) start--;
    if (start < cursor && start > 0 && line[start - 1] == '\\') return -1;
    return start;
}

static int helpArgumentPrefixStart(const char* line, int cursor, char* out_close) {
    int start = cursor;
    while (start > 0 && completionNameChar(line[start - 1])) start--;
    if (start < 6) return -1;

    int open = start - 1;
    while (open >= 0 && isspace((unsigned char)line[open])) open--;
    if (open < 0 || (line[open] != '{' && line[open] != '(')) return -1;

    char close = line[open] == '{' ? '}' : ')';
    int cmd_end = open - 1;
    while (cmd_end >= 0 && isspace((unsigned char)line[cmd_end])) cmd_end--;
    int cmd_start = cmd_end + 1;
    while (cmd_start > 0 && completionNameChar(line[cmd_start - 1])) cmd_start--;
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

static int completionMatchExists(const char** matches, size_t count, const char* name) {
    for (size_t i = 0; i < count; i++) {
        if (strcmp(matches[i], name) == 0) return 1;
    }
    return 0;
}

static int pushCompletionMatch(const char*** matches, size_t* count, size_t* cap, const char* name) {
    if (!name || completionMatchExists(*matches, *count, name)) return 1;
    if (*count == *cap) {
        size_t nextCap = *cap ? *cap * 2 : 16;
        const char** next = realloc((void*)*matches, nextCap * sizeof(char*));
        if (!next) return 0;
        *matches = next;
        *cap = nextCap;
    }
    (*matches)[(*count)++] = name;
    return 1;
}

static const char** collectCommandCompletionMatches(const char* prefix, int includeUserFunctions, size_t* out_count) {
    size_t prefix_len = strlen(prefix);
    size_t count = 0;
    size_t cap = 0;
    const char** matches = NULL;

    for (const CommandEntry* c = commandRegistry(); c; c = c->next) {
        if (strncmp(c->name, "__", 2) == 0) continue;
        if (strncmp(c->name, prefix, prefix_len) != 0) continue;
        if (!pushCompletionMatch(&matches, &count, &cap, c->name)) goto fail;
    }

    if (includeUserFunctions && g_completionCtx) {
        for (UserFunction* fn = g_completionCtx->functions; fn; fn = fn->next) {
            if (!fn->name || strncmp(fn->name, prefix, prefix_len) != 0) continue;
            if (!pushCompletionMatch(&matches, &count, &cap, fn->name)) goto fail;
        }
    }

    *out_count = count;
    if (!count) {
        free(matches);
        return NULL;
    }
    return matches;

fail:
    free(matches);
    *out_count = 0;
    return NULL;
}

static const char** collectVariableCompletionMatches(const char* prefix, size_t* out_count) {
    if (!g_completionCtx || !g_completionCtx->env) {
        *out_count = 0;
        return NULL;
    }

    size_t prefix_len = strlen(prefix);
    size_t count = 0;
    size_t cap = 0;
    const char** matches = NULL;

    for (Env* env = g_completionCtx->env; env; env = env->parent) {
        for (EnvEntry* entry = env->head; entry; entry = entry->next) {
            if (!entry->name || strncmp(entry->name, prefix, prefix_len) != 0) continue;
            if (!pushCompletionMatch(&matches, &count, &cap, entry->name)) {
                free(matches);
                *out_count = 0;
                return NULL;
            }
        }
    }

    *out_count = count;
    if (!count) {
        free(matches);
        return NULL;
    }
    return matches;
}

static void addCompletionCandidate(const char* line, int prefix_start, int cursor,
                                   const char* name, char close_char,
                                   linenoiseCompletions* lc) {
    size_t line_len = strlen(line);
    size_t name_len = strlen(name);
    size_t close_len = close_char ? 1 : 0;
    size_t candidate_len = (size_t)prefix_start + name_len + close_len + (line_len - (size_t)cursor);
    char* candidate = malloc(candidate_len + 1);
    if (!candidate) return;
    memcpy(candidate, line, (size_t)prefix_start);
    memcpy(candidate + prefix_start, name, name_len);
    if (close_char) candidate[prefix_start + name_len] = close_char;
    memcpy(candidate + prefix_start + name_len + close_len, line + cursor, line_len - (size_t)cursor);
    candidate[candidate_len] = '\0';
    linenoiseAddCompletion(lc, candidate);
    free(candidate);
}

static void addCommandCompletions(const char* line, linenoiseCompletions* lc) {
    int cursor = (int)strlen(line);
    if (g_completionFn && !g_completionFn(line, cursor)) return;

    int prefix_start = commandPrefixStart(line, cursor);
    char close_char = '\0';
    int includeUserFunctions = prefix_start >= 0;
    if (prefix_start < 0) {
        prefix_start = helpArgumentPrefixStart(line, cursor, &close_char);
    }
    int variableCompletion = 0;
    if (prefix_start < 0) {
        prefix_start = variablePrefixStart(line, cursor);
        variableCompletion = 1;
    }

    size_t prefix_len = (size_t)(cursor - prefix_start);
    char* prefix = malloc(prefix_len + 1);
    if (!prefix) return;
    memcpy(prefix, line + prefix_start, prefix_len);
    prefix[prefix_len] = '\0';

    size_t matchCount = 0;
    const char** matches = variableCompletion
        ? collectVariableCompletionMatches(prefix, &matchCount)
        : collectCommandCompletionMatches(prefix, includeUserFunctions, &matchCount);
    free(prefix);
    if (!matches) return;

    for (size_t i = 0; i < matchCount; i++) {
        addCompletionCandidate(line, prefix_start, cursor, matches[i], close_char, lc);
    }
    free(matches);
}

void bstLineInputInit(BestiaryCompletionFn completionFn) {
    g_completionFn = completionFn;
    linenoiseSetCompletionCallback(addCommandCompletions);
}

void bstLineInputShutdown(void) {
    g_completionFn = NULL;
    g_completionCtx = NULL;
    linenoiseSetCompletionCallback(NULL);
}

void bstLineInputSetPlainMode(int enabled) {
    (void)enabled;
}

void bstLineInputSetEvalContext(EvalContext* ctx) {
    g_completionCtx = ctx;
}

void bstLineInputRecoverInterrupt(void) {
    putchar('\n');
    fflush(stdout);
}

char* bstReadLine(const char* prompt) {
    return linenoise(prompt);
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
    const char* initial = context ? context->initial : NULL;
    size_t promptLen = prompt ? strlen(prompt) : 0;
    size_t initialLen = initial ? strlen(initial) : 0;
    if (selectedBlockLine) *selectedBlockLine = -1;
    char* combined = malloc(promptLen + initialLen + 1);
    if (!combined) return linenoise(prompt);
    if (promptLen) memcpy(combined, prompt, promptLen);
    if (initialLen) memcpy(combined + promptLen, initial, initialLen);
    combined[promptLen + initialLen] = '\0';
    char* out = linenoise(combined);
    free(combined);
    return out;
}

void bstAddHistory(const char* line) {
    linenoiseHistoryAdd(line);
}
