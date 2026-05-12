#define _POSIX_C_SOURCE 200809L

#include<ctype.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<readline/readline.h>
#include<readline/history.h>
#include<readline/keymaps.h>
#include "eval.h"
#include "line_input.h"

static BestiaryCompletionFn g_completionFn = NULL;
static EvalContext* g_completionCtx = NULL;
static int g_plainMode = 0;
static const char* g_initialText = NULL;
static size_t g_initialProtectedLen = 0;
static size_t g_protectedLen = 0;
static const char* const* g_blockLines = NULL;
static size_t g_blockLineCount = 0;
static int g_blockSelection = -1;
static char* g_blockDraft = NULL;

static int blockNavigateUp(int count, int key);
static int blockNavigateDown(int count, int key);
static void bindReadlineEditingKeys(void);
static void bindBlockNavigationKeys(rl_command_func_t* up, rl_command_func_t* down);

static int compareCompletionNames(const void* lhs, const void* rhs) {
    const char* const* a = lhs;
    const char* const* b = rhs;
    return strcmp(*a, *b);
}

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

    qsort(matches, count, sizeof(char*), compareCompletionNames);
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

    qsort(matches, count, sizeof(char*), compareCompletionNames);
    return matches;
}

static size_t sharedCompletionPrefix(const char** matches, size_t count) {
    size_t shared = strlen(matches[0]);
    for (size_t i = 1; i < count; i++) {
        size_t j = 0;
        while (j < shared && matches[i][j] && matches[0][j] == matches[i][j]) j++;
        shared = j;
    }
    return shared;
}

static int completeNameFromMatches(int prefix_start, char close_char, int displayBackslash,
                                   const char** matches, size_t match_count) {
    size_t shared = sharedCompletionPrefix(matches, match_count);
    size_t completion_len = (match_count == 1) ? strlen(matches[0]) : shared;

    if (completion_len > 0) {
        size_t close_len = (match_count == 1 && close_char) ? 1 : 0;
        char* completion = malloc(completion_len + close_len + 1);
        if (completion) {
            memcpy(completion, matches[0], completion_len);
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
        if (displayBackslash) printf("\\%s\n", matches[i]);
        else printf("%s\n", matches[i]);
    }
    rl_on_new_line();
    rl_redisplay();

    free(matches);
    return 0;
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
    int displayBackslash = prefix_start >= 0;
    int includeUserFunctions = displayBackslash;
    if (prefix_start < 0) {
        prefix_start = helpArgumentPrefixStart(rl_line_buffer, rl_point, &close_char);
    }
    int variableCompletion = 0;
    if (prefix_start < 0) {
        prefix_start = variablePrefixStart(rl_line_buffer, rl_point);
        variableCompletion = 1;
    }

    size_t prefix_len = (size_t)(rl_point - prefix_start);
    char* prefix = malloc(prefix_len + 1);
    if (!prefix) return 0;
    memcpy(prefix, rl_line_buffer + prefix_start, prefix_len);
    prefix[prefix_len] = '\0';

    size_t match_count = 0;
    const char** matches = variableCompletion
        ? collectVariableCompletionMatches(prefix, &match_count)
        : collectCommandCompletionMatches(prefix, includeUserFunctions, &match_count);
    free(prefix);
    if (!matches) {
        rl_ding();
        return 0;
    }

    return completeNameFromMatches(prefix_start, close_char, displayBackslash, matches, match_count);
}

static int prepareReadlineInput(void) {
    bindReadlineEditingKeys();
    bindBlockNavigationKeys(g_blockLineCount > 0 ? blockNavigateUp : rl_get_previous_history,
                            g_blockLineCount > 0 ? blockNavigateDown : rl_get_next_history);
    if (g_initialText && *g_initialText) rl_insert_text(g_initialText);
    g_protectedLen = g_initialProtectedLen;
    g_initialText = NULL;
    g_initialProtectedLen = 0;
    rl_pre_input_hook = NULL;
    rl_redisplay();
    return 0;
}

static size_t leadingWhitespaceLen(const char* text) {
    size_t n = 0;
    while (text && text[n] && isspace((unsigned char)text[n])) n++;
    return n;
}

static char* dupstrLocal(const char* text) {
    size_t n = strlen(text ? text : "");
    char* out = malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, text ? text : "", n + 1);
    return out;
}

static void replaceReadlineText(const char* text, size_t protectedLen) {
    rl_replace_line(text ? text : "", 0);
    rl_point = rl_end;
    g_protectedLen = protectedLen;
    rl_redisplay();
}

static int insertCloseBrace(int count, int key) {
    (void)key;

    int onlyIndent = 1;
    for (int i = 0; i < rl_end; i++) {
        if (!isspace((unsigned char)rl_line_buffer[i])) {
            onlyIndent = 0;
            break;
        }
    }
    if (onlyIndent && rl_end > 0) {
        rl_delete_text(0, rl_end);
        rl_point = 0;
        rl_end = 0;
        g_protectedLen = 0;
    }

    for (int i = 0; i < count; i++) rl_insert_text("}");
    return 0;
}

static int protectedBackspace(int count, int key) {
    (void)key;
    if (count < 1) count = 1;
    if (rl_point <= (int)g_protectedLen) {
        rl_ding();
        return 0;
    }
    if (rl_point - count < (int)g_protectedLen) count = rl_point - (int)g_protectedLen;
    if (count < 1) {
        rl_ding();
        return 0;
    }
    rl_delete_text(rl_point - count, rl_point);
    rl_point -= count;
    rl_redisplay();
    return 0;
}

static int blockNavigateUp(int count, int key) {
    (void)count; (void)key;
    if (!g_blockLines || g_blockLineCount == 0) {
        rl_ding();
        return 0;
    }

    if (g_blockSelection < 0) {
        free(g_blockDraft);
        g_blockDraft = dupstrLocal(rl_line_buffer);
        g_blockSelection = (int)g_blockLineCount - 1;
    } else if (g_blockSelection > 0) {
        g_blockSelection--;
    } else {
        rl_ding();
        return 0;
    }

    const char* line = g_blockLines[g_blockSelection] ? g_blockLines[g_blockSelection] : "";
    replaceReadlineText(line, leadingWhitespaceLen(line));
    return 0;
}

static int blockNavigateDown(int count, int key) {
    (void)count; (void)key;
    if (!g_blockLines || g_blockSelection < 0) {
        rl_ding();
        return 0;
    }

    if ((size_t)(g_blockSelection + 1) < g_blockLineCount) {
        g_blockSelection++;
        const char* line = g_blockLines[g_blockSelection] ? g_blockLines[g_blockSelection] : "";
        replaceReadlineText(line, leadingWhitespaceLen(line));
        return 0;
    }

    g_blockSelection = -1;
    replaceReadlineText(g_blockDraft ? g_blockDraft : (g_initialText ? g_initialText : ""),
                        g_blockDraft ? leadingWhitespaceLen(g_blockDraft) : g_initialProtectedLen);
    return 0;
}

static void bindReadlineEditingKeys(void) {
    rl_bind_key('\t', completeCommandName);
    rl_bind_key('}', insertCloseBrace);
    rl_bind_key(127, protectedBackspace);
    rl_bind_key('\b', protectedBackspace);
    rl_bind_keyseq("\177", protectedBackspace);
    rl_bind_keyseq("\010", protectedBackspace);
    rl_bind_key_in_map(127, protectedBackspace, emacs_standard_keymap);
    rl_bind_key_in_map('\b', protectedBackspace, emacs_standard_keymap);
    rl_bind_key_in_map(127, protectedBackspace, vi_insertion_keymap);
    rl_bind_key_in_map('\b', protectedBackspace, vi_insertion_keymap);
}

static void bindBlockNavigationKeys(rl_command_func_t* up, rl_command_func_t* down) {
    rl_bind_keyseq("\033[A", up);
    rl_bind_keyseq("\033[B", down);
    rl_bind_keyseq("\033OA", up);
    rl_bind_keyseq("\033OB", down);
    rl_bind_keyseq_in_map("\033[A", up, emacs_standard_keymap);
    rl_bind_keyseq_in_map("\033[B", down, emacs_standard_keymap);
    rl_bind_keyseq_in_map("\033OA", up, emacs_standard_keymap);
    rl_bind_keyseq_in_map("\033OB", down, emacs_standard_keymap);
    rl_bind_keyseq_in_map("\033[A", up, vi_insertion_keymap);
    rl_bind_keyseq_in_map("\033[B", down, vi_insertion_keymap);
    rl_bind_keyseq_in_map("\033OA", up, vi_insertion_keymap);
    rl_bind_keyseq_in_map("\033OB", down, vi_insertion_keymap);
}

void bstLineInputInit(BestiaryCompletionFn completionFn) {
    g_completionFn = completionFn;
    using_history();
    rl_catch_signals = 0;
    bindReadlineEditingKeys();
}

void bstLineInputShutdown(void) {
    g_completionFn = NULL;
    g_completionCtx = NULL;
}

void bstLineInputSetPlainMode(int enabled) {
    g_plainMode = enabled ? 1 : 0;
}

void bstLineInputSetEvalContext(EvalContext* ctx) {
    g_completionCtx = ctx;
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
    return bstReadLineWithContext(prompt, NULL, NULL);
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
    size_t protectedLen = context ? context->protectedLen : 0;
    if (selectedBlockLine) *selectedBlockLine = -1;

    if (g_plainMode || !isatty(STDIN_FILENO)) {
        char stackBuf[1024];
        char* out = NULL;
        size_t len = 0;
        size_t cap = 0;

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

    g_protectedLen = 0;
    g_initialProtectedLen = protectedLen;
    g_blockLines = context ? context->blockLines : NULL;
    g_blockLineCount = context ? context->blockLineCount : 0;
    g_blockSelection = -1;
    free(g_blockDraft);
    g_blockDraft = NULL;

    if (initial && *initial) {
        g_initialText = initial;
    }
    bindReadlineEditingKeys();
    bindBlockNavigationKeys(g_blockLineCount > 0 ? blockNavigateUp : rl_get_previous_history,
                            g_blockLineCount > 0 ? blockNavigateDown : rl_get_next_history);
    rl_pre_input_hook = prepareReadlineInput;

    char* out = readline(prompt);
    if (rl_pre_input_hook == prepareReadlineInput) rl_pre_input_hook = NULL;
    g_initialText = NULL;
    g_initialProtectedLen = 0;
    g_protectedLen = 0;
    if (selectedBlockLine) *selectedBlockLine = g_blockSelection;
    g_blockLines = NULL;
    g_blockLineCount = 0;
    g_blockSelection = -1;
    free(g_blockDraft);
    g_blockDraft = NULL;
    return out;
}

void bstAddHistory(const char* line) {
    if (g_plainMode) return;
    if (line && *line) add_history(line);
}
