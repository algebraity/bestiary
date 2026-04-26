#ifndef BST_PLATFORM_WINDOWS
#define _POSIX_C_SOURCE 200809L
#endif

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<signal.h>
#ifndef BST_PLATFORM_WINDOWS
#include<setjmp.h>
#include<unistd.h>
#endif
#include "lexer.h"
#include "parser.h"
#include "ast.h"
#include "eval.h"
#include "line_input.h"
#include "script.h"
#include "value.h"

/* ---------- Bestiary REPL: lex -> parse -> eval ---------- */

static int opt_dump_tokens = 0;
static int opt_dump_ast    = 0;
static int opt_plain_input = 0;

typedef enum {
    REPL_STATE_IDLE = 0,
    REPL_STATE_READLINE,
    REPL_STATE_EXEC
} ReplState;

static volatile sig_atomic_t g_sigint_seen = 0;
static volatile sig_atomic_t g_repl_state = REPL_STATE_IDLE;
#ifndef BST_PLATFORM_WINDOWS
static volatile sig_atomic_t g_sigint_from_readline = 0;
static sigjmp_buf g_repl_jmp;
#endif

static char* g_active_line = NULL;
static Token* g_active_toks = NULL;
static size_t g_active_ntok = 0;
static AstNode* g_active_ast = NULL;

/* ---------- Helper methods ---------- */

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
#ifndef BST_PLATFORM_WINDOWS
    if (g_repl_state == REPL_STATE_READLINE) {
        g_sigint_from_readline = 1;
        siglongjmp(g_repl_jmp, 1);
    }
    write(STDOUT_FILENO, "\n", 1);
    siglongjmp(g_repl_jmp, 1);
#else
    putchar('\n');
    fflush(stdout);
#endif
}

/* ---------- Main ---------- */

int main(int argc, char** argv) {
#ifndef BST_PLATFORM_WINDOWS
    struct sigaction sa;
#endif
    const char* script_filename = NULL;

    for (int i = 1; i < argc; i++) {
        if      (strcmp(argv[i], "--tokens") == 0) opt_dump_tokens = 1;
        else if (strcmp(argv[i], "--ast")    == 0) opt_dump_ast    = 1;
        else if (strcmp(argv[i], "--plain")  == 0) opt_plain_input = 1;
        else if (!script_filename) script_filename = argv[i];
        else {
            fprintf(stderr, "usage: %s [--tokens] [--ast] [--plain] [script-file]\n", argv[0]);
            return 2;
        }
    }

#ifndef BST_PLATFORM_WINDOWS
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handleSigint;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
#else
    signal(SIGINT, handleSigint);
#endif

    registerBuiltins();
    EvalContext* ctx = evalCtxNew();

    if (script_filename) {
        ScriptRunOptions options = {opt_dump_tokens, opt_dump_ast, 1};
        ScriptRunResult result = {0, 0, 0};
        char error[512] = {0};
        int ok = bstRunScriptFile(ctx, script_filename, &options, &result, error, sizeof(error));
        if (!ok) {
            fprintf(stderr, "%s\n", error[0] ? error : "failed to run script");
            evalCtxFree(ctx);
            return 1;
        }
        evalCtxFree(ctx);
        return result.errors ? 1 : 0;
    }

    bstLineInputInit(NULL);
    bstLineInputSetPlainMode(opt_plain_input);

    puts("Bestiary v0.0.1 (flags: --tokens --ast)  -- Ctrl+C cancels, Ctrl-D quits");
    for (;;) {
#ifndef BST_PLATFORM_WINDOWS
        if (sigsetjmp(g_repl_jmp, 1) != 0) {
            if (g_sigint_from_readline) {
                bstLineInputRecoverInterrupt();
                g_sigint_from_readline = 0;
            }
            clearActiveObjects();
            g_repl_state = REPL_STATE_IDLE;
            g_sigint_seen = 0;
        }
#else
        if (g_sigint_seen) {
            clearActiveObjects();
            g_repl_state = REPL_STATE_IDLE;
            g_sigint_seen = 0;
        }
#endif

        g_repl_state = REPL_STATE_READLINE;
        char* line = bstReadLine("> ");
        g_active_line = line;
        g_repl_state = REPL_STATE_IDLE;

        if (!line) { putchar('\n'); break; }     // Ctrl-D
        if (*line == '\0') {
            clearActiveObjects();
            continue;
        }
        bstAddHistory(line);

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
    bstLineInputShutdown();
    evalCtxFree(ctx);
    return 0;
}
