#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<ctype.h>
#include<signal.h>
#include<setjmp.h>
#include<unistd.h>
#include<readline/readline.h>
#include<readline/history.h>
#include "lexer.h"
#include "parser.h"
#include "ast.h"
#include "eval.h"
#include "value.h"

/* ---------- Bestiary REPL: lex -> parse -> eval ---------- */

static int opt_dump_tokens = 0;
static int opt_dump_ast    = 0;

typedef enum {
    REPL_STATE_IDLE = 0,
    REPL_STATE_READLINE,
    REPL_STATE_EXEC
} ReplState;

static volatile sig_atomic_t g_sigint_seen = 0;
static volatile sig_atomic_t g_repl_state = REPL_STATE_IDLE;
static sigjmp_buf g_repl_jmp;

static char* g_active_line = NULL;
static Token* g_active_toks = NULL;
static size_t g_active_ntok = 0;
static AstNode* g_active_ast = NULL;

/* ---------- Helper methods ---------- */

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
    int prefix_start = commandPrefixStart(rl_line_buffer, rl_point);
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
        char* completion = malloc(completion_len + 1);
        if (completion) {
            memcpy(completion, matches[0]->name, completion_len);
            completion[completion_len] = '\0';
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

// Map a TokenKind to a printable tag
static const char* kindName(TokenKind k) {
    switch (k) {
        case TOK_IDENT:        return "IDENT";
        case TOK_NUMBER:       return "NUMBER";
        case TOK_DECIMAL:      return "DECIMAL";
        case TOK_EQUALS:       return "EQUALS";
        case TOK_LBRACE:       return "LBRACE";
        case TOK_RBRACE:       return "RBRACE";
        case TOK_COMMAND:      return "COMMAND";
        case TOK_AMP:          return "AMP";
        case TOK_DBLBACKSLASH: return "DBLBACKSLASH";
        case TOK_PLUS:         return "PLUS";
        case TOK_MINUS:        return "MINUS";
        case TOK_STAR:         return "STAR";
        case TOK_SLASH:        return "SLASH";
        case TOK_LPAREN:       return "LPAREN";
        case TOK_RPAREN:       return "RPAREN";
        case TOK_COMMA:        return "COMMA";
        case TOK_DOT:          return "DOT";
        case TOK_CARET:        return "CARET";
        case TOK_UNDERSCORE:   return "UNDERSCORE";
        case TOK_COLON:        return "COLON";
        case TOK_SEMICOLON:    return "SEMICOLON";
        case TOK_PIPE:         return "PIPE";
        case TOK_LESS:         return "LESS";
        case TOK_GREATER:      return "GREATER";
        case TOK_PRIME:        return "PRIME";
        case TOK_DOLLAR:       return "DOLLAR";
        case TOK_PERCENT:      return "PERCENT";
        case TOK_TILDE:        return "TILDE";
        case TOK_HASH:         return "HASH";
        case TOK_LBRACK:       return "LBRACK";
        case TOK_RBRACK:       return "RBRACK";
        case TOK_EOF:          return "EOF";
        default:               return "?";
    }
}

// Print the token stream in a readable form
static void dumpTokens(Token* toks, size_t n) {
    for (size_t i = 0; i < n; i++) {
        const char* text = toks[i].text ? toks[i].text : "";
        printf("  [%2zu] %-13s line=%zu col=%-3zu text=\"%s\"\n",
               i, kindName(toks[i].kind), toks[i].line, toks[i].col, text);
    }
}

// Free token strings and the token array itself
static void freeTokens(Token* toks, size_t n) {
    for (size_t i = 0; i < n; i++) free(toks[i].text);
    free(toks);
}

static void clearActiveObjects(void) {
    if (g_active_ast) {
        astFree(g_active_ast);
        g_active_ast = NULL;
    }
    if (g_active_toks) {
        freeTokens(g_active_toks, g_active_ntok);
        g_active_toks = NULL;
        g_active_ntok = 0;
    }
    if (g_active_line) {
        free(g_active_line);
        g_active_line = NULL;
    }
}

static void handleSigint(int signo) {
    (void)signo;
    g_sigint_seen = 1;
    write(STDOUT_FILENO, "\n", 1);
    if (g_repl_state == REPL_STATE_READLINE) {
        rl_done = 1;
        return;
    }
    siglongjmp(g_repl_jmp, 1);
}

/* ---------- Main ---------- */

int main(int argc, char** argv) {
    struct sigaction sa;

    for (int i = 1; i < argc; i++) {
        if      (strcmp(argv[i], "--tokens") == 0) opt_dump_tokens = 1;
        else if (strcmp(argv[i], "--ast")    == 0) opt_dump_ast    = 1;
    }

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handleSigint;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    registerBuiltins();
    EvalContext* ctx = evalCtxNew();

    // Arrow-key history, Emacs-style line editing, etc. via GNU readline
    using_history();
    rl_catch_signals = 0;
    rl_bind_key('\t', completeCommandName);

    puts("Bestiary v0.0.1 (flags: --tokens --ast)  -- Ctrl+C cancels, Ctrl-D quits");
    for (;;) {
        if (sigsetjmp(g_repl_jmp, 1) != 0) {
            clearActiveObjects();
            g_repl_state = REPL_STATE_IDLE;
            g_sigint_seen = 0;
        }

        g_repl_state = REPL_STATE_READLINE;
        char* line = readline("> ");
        g_active_line = line;
        g_repl_state = REPL_STATE_IDLE;

        if (g_sigint_seen) {
            g_sigint_seen = 0;
            clearActiveObjects();
            rl_on_new_line();
            continue;
        }

        if (!line) { putchar('\n'); break; }     // Ctrl-D
        if (*line == '\0') {
            clearActiveObjects();
            continue;
        }
        add_history(line);

        g_repl_state = REPL_STATE_EXEC;
        size_t ntok = 0;
        Token* toks = bstLex(line, &ntok);
        g_active_toks = toks;
        g_active_ntok = ntok;
        if (opt_dump_tokens) { puts("-- tokens --"); dumpTokens(toks, ntok); }

        ParseError perr = {0};
        AstNode* ast = bstParse(toks, ntok, &perr);
        g_active_ast = ast;
        if (!ast) {
            fprintf(stderr, "parse error at %zu:%zu: %s\n",
                    perr.line, perr.col, perr.msg ? perr.msg : "unknown");
            clearActiveObjects();
            g_repl_state = REPL_STATE_IDLE;
            continue;
        }
        if (opt_dump_ast) { puts("-- ast --"); astPrint(ast, 2); }

        Value r = eval(ctx, ast);
        g_repl_state = REPL_STATE_IDLE;
        fputs("=> ", stdout); valPrint(r); putchar('\n');
        valFree(r);

        clearActiveObjects();
    }

    clearActiveObjects();
    evalCtxFree(ctx);
    return 0;
}
