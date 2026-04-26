#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<ctype.h>
#include "ast.h"
#include "eval.h"
#include "lexer.h"
#include "parser.h"
#include "script.h"
#include "value.h"

static const ScriptRunOptions DEFAULT_OPTIONS = {0, 0, 1};

static const char* kindName(TokenKind k) {
    switch (k) {
        case TOK_IDENT:        return "IDENT";
        case TOK_NUMBER:       return "NUMBER";
        case TOK_DECIMAL:      return "DECIMAL";
        case TOK_EQUALS:       return "EQUALS";
        case TOK_EQEQ:         return "EQEQ";
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
        case TOK_BANG:         return "BANG";
        case TOK_LBRACK:       return "LBRACK";
        case TOK_RBRACK:       return "RBRACK";
        case TOK_STRING:       return "STRING";
        case TOK_EOF:          return "EOF";
        default:               return "?";
    }
}

static void dumpTokens(Token* toks, size_t n) {
    for (size_t i = 0; i < n; i++) {
        const char* text = toks[i].text ? toks[i].text : "";
        printf("  [%2zu] %-13s line=%zu col=%-3zu text=\"%s\"\n",
               i, kindName(toks[i].kind), toks[i].line, toks[i].col, text);
    }
}

static void freeTokens(Token* toks, size_t n) {
    if (!toks) return;
    for (size_t i = 0; i < n; i++) free(toks[i].text);
    free(toks);
}

static char* readLine(FILE* fp) {
    char stackBuf[1024];
    char* out = NULL;
    size_t len = 0;
    size_t cap = 0;

    for (;;) {
        if (!fgets(stackBuf, sizeof(stackBuf), fp)) {
            if (len > 0) return out;
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
        if (len > 0 && out[len - 1] == '\n') return out;
        if (chunkLen < sizeof(stackBuf) - 1) return out;
    }
}

static int lineIsBlank(const char* line) {
    if (!line) return 1;
    while (*line) {
        if (!isspace((unsigned char)*line)) return 0;
        line++;
    }
    return 1;
}

static void stripLineEnding(char* line) {
    size_t len = strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
        line[--len] = '\0';
    }
}

static int evalScriptLine(EvalContext* ctx,
                          char* line,
                          const char* filename,
                          size_t lineNo,
                          const ScriptRunOptions* options) {
    size_t ntok = 0;
    Token* toks = bstLex(line, &ntok);
    if (options->dumpTokens) {
        printf("-- tokens: %s:%zu --\n", filename, lineNo);
        dumpTokens(toks, ntok);
    }

    ParseError perr = {0};
    AstNode* ast = bstParse(toks, ntok, &perr);
    if (!ast) {
        fprintf(stderr, "%s:%zu: parse error at %zu:%zu: %s\n",
                filename, lineNo, perr.line, perr.col, perr.msg ? perr.msg : "unknown");
        freeTokens(toks, ntok);
        return 0;
    }
    if (options->dumpAst) {
        printf("-- ast: %s:%zu --\n", filename, lineNo);
        astPrint(ast, 2);
    }

    Value value = eval(ctx, ast);
    if (options->printResults) {
        fputs("=> ", stdout);
        valPrint(value);
        putchar('\n');
    }

    int ok = value.kind != VAL_ERROR;
    valFree(value);
    astFree(ast);
    freeTokens(toks, ntok);
    return ok;
}

int bstRunScriptFile(EvalContext* ctx,
                     const char* filename,
                     const ScriptRunOptions* options,
                     ScriptRunResult* result,
                     char* error,
                     size_t errorSize) {
    ScriptRunOptions opts = options ? *options : DEFAULT_OPTIONS;
    ScriptRunResult local = {0, 0, 0};
    FILE* fp = NULL;

    if (result) *result = local;
    if (!ctx || !filename || !*filename) {
        if (error && errorSize) snprintf(error, errorSize, "missing script filename");
        return 0;
    }

    fp = fopen(filename, "r");
    if (!fp) {
        if (error && errorSize) snprintf(error, errorSize, "could not open script: %s", filename);
        return 0;
    }

    for (;;) {
        char* line = readLine(fp);
        if (!line) break;
        local.linesRead++;
        stripLineEnding(line);
        if (!lineIsBlank(line)) {
            local.linesEvaluated++;
            if (!evalScriptLine(ctx, line, filename, local.linesRead, &opts)) local.errors++;
        }
        free(line);
    }

    fclose(fp);
    if (result) *result = local;
    if (error && errorSize) error[0] = '\0';
    return 1;
}
