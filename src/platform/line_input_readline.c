#define _POSIX_C_SOURCE 200809L

#include<ctype.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<readline/readline.h>
#include<readline/history.h>
#include "eval.h"
#include "line_input.h"

static BestiaryCompletionFn g_completionFn = NULL;
static int g_plainMode = 0;

static int compareCompletionEntries(const void* lhs, const void* rhs) {
    const CommandEntry* const* a = lhs;
    const CommandEntry* const* b = rhs;
    return strcmp((*a)->name, (*b)->name);
}

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

static const CommandEntry** collectCompletionMatches(const char* prefix, size_t* out_count) {
    size_t prefix_len = strlen(prefix);
    size_t count = 0;
    const CommandEntry** matches = NULL;

    for (const CommandEntry* c = commandRegistry(); c; c = c->next) {
        if (strncmp(c->name, prefix, prefix_len) == 0) count++;
    }
    *out_count = count;
    if (!count) return NULL;

    matches = calloc(count, sizeof(CommandEntry*));
    if (!matches) {
        *out_count = 0;
        return NULL;
    }

    size_t index = 0;
    for (const CommandEntry* c = commandRegistry(); c; c = c->next) {
        if (strncmp(c->name, prefix, prefix_len) == 0) {
            matches[index++] = c;
        }
    }
    qsort(matches, count, sizeof(CommandEntry*), compareCompletionEntries);
    return matches;
}

static size_t sharedCompletionPrefix(const CommandEntry** matches, size_t count) {
    size_t shared = strlen(matches[0]->name);
    for (size_t i = 1; i < count; i++) {
        size_t j = 0;
        while (j < shared && matches[i]->name[j] && matches[0]->name[j] == matches[i]->name[j]) j++;
        shared = j;
    }
    return shared;
}

static int completeCommandName(int count, int key) {
    (void)count;
    (void)key;
    if (g_completionFn && !g_completionFn(rl_line_buffer, rl_point)) {
        rl_ding();
        return 0;
    }

    char close_char = '\0';
    int prefix_start = commandPrefixStart(rl_line_buffer, rl_point);
    if (prefix_start < 0) {
        prefix_start = helpArgumentPrefixStart(rl_line_buffer, rl_point, &close_char);
    }
    if (prefix_start < 0) {
        rl_ding();
        return 0;
    }

    size_t prefix_len = (size_t)(rl_point - prefix_start);
    char* prefix = malloc(prefix_len + 1);
    if (!prefix) return 0;
    memcpy(prefix, rl_line_buffer + prefix_start, prefix_len);
    prefix[prefix_len] = '\0';

    size_t match_count = 0;
    const CommandEntry** matches = collectCompletionMatches(prefix, &match_count);
    free(prefix);
    if (!matches) {
        rl_ding();
        return 0;
    }

    size_t shared = sharedCompletionPrefix(matches, match_count);
    size_t completion_len = (match_count == 1) ? strlen(matches[0]->name) : shared;

    if (completion_len > 0) {
        size_t close_len = (match_count == 1 && close_char) ? 1 : 0;
        char* completion = malloc(completion_len + close_len + 1);
        if (completion) {
            memcpy(completion, matches[0]->name, completion_len);
            if (close_len) completion[completion_len] = close_char;
            completion[completion_len + close_len] = '\0';
            rl_delete_text(prefix_start, rl_point);
            rl_point = prefix_start;
            rl_insert_text(completion);
            free(completion);
        }
    }

    if (match_count == 1) {
        rl_redisplay();
        free(matches);
        return 0;
    }

    putchar('\n');
    for (size_t i = 0; i < match_count; i++) {
        printf("\\%s\n", matches[i]->name);
    }
    rl_on_new_line();
    rl_redisplay();

    free(matches);
    return 0;
}

void bstLineInputInit(BestiaryCompletionFn completionFn) {
    g_completionFn = completionFn;
    using_history();
    rl_catch_signals = 0;
    rl_bind_key('\t', completeCommandName);
}

void bstLineInputShutdown(void) {
    g_completionFn = NULL;
}

void bstLineInputSetPlainMode(int enabled) {
    g_plainMode = enabled ? 1 : 0;
}

void bstLineInputRecoverInterrupt(void) {
    if (g_plainMode) {
        putchar('\n');
        fflush(stdout);
        return;
    }
    rl_free_line_state();
    rl_cleanup_after_signal();
    putchar('\n');
    fflush(stdout);
    rl_reset_after_signal();
}

char* bstReadLine(const char* prompt) {
    if (g_plainMode || !isatty(STDIN_FILENO)) {
        char stackBuf[1024];
        char* out = NULL;
        size_t len = 0;
        size_t cap = 0;

        if (prompt) {
            fputs(prompt, stdout);
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
    return readline(prompt);
}

void bstAddHistory(const char* line) {
    if (g_plainMode) return;
    if (line && *line) add_history(line);
}
