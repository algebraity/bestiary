/* ---------- AI generated repl for testing ---------- */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"

static const char* kindName(TokenKind k) {
    switch (k) {
        case TOK_IDENT:       return "IDENT";
        case TOK_NUMBER:      return "NUMBER";
        case TOK_DECIMAL:     return "DECIMAL";
        case TOK_EQUALS:      return "EQUALS";
        case TOK_LBRACE:      return "LBRACE";
        case TOK_RBRACE:      return "RBRACE";
        case TOK_COMMAND:     return "COMMAND";
        case TOK_AMP:         return "AMP";
        case TOK_DBLBACKSLASH: return "DBLBACKSLASH";
        case TOK_PLUS:        return "PLUS";
        case TOK_MINUS:       return "MINUS";
        case TOK_STAR:        return "STAR";
        case TOK_SLASH:       return "SLASH";
        case TOK_LPAREN:      return "LPAREN";
        case TOK_RPAREN:      return "RPAREN";
        case TOK_COMMA:       return "COMMA";
        case TOK_DOT:         return "DOT";
        case TOK_CARET:       return "CARET";
        case TOK_UNDERSCORE:  return "UNDERSCORE";
        case TOK_COLON:       return "COLON";
        case TOK_SEMICOLON:   return "SEMICOLON";
        case TOK_PIPE:        return "PIPE";
        case TOK_LESS:        return "LESS";
        case TOK_GREATER:     return "GREATER";
        case TOK_PRIME:       return "PRIME";
        case TOK_DOLLAR:      return "DOLLAR";
        case TOK_PERCENT:     return "PERCENT";
        case TOK_TILDE:       return "TILDE";
        case TOK_HASH:        return "HASH";
        case TOK_LBRACK:      return "LBRACK";
        case TOK_RBRACK:      return "RBRACK";
        case TOK_EOF:         return "EOF";
        default:              return "?";
    }
}

int main(void) {
    char line[4096];
    puts("bestiary lexer REPL — type LaTeX, empty line or Ctrl-D to quit");
    for (;;) {
        fputs("> ", stdout);
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) { putchar('\n'); break; }
        size_t n = strlen(line);
        if (n && line[n-1] == '\n') line[--n] = '\0';
        if (n == 0) break;

        size_t count = 0;
        Token* toks = bstLex(line, &count);
        for (size_t i = 0; i < count; i++) {
            const char* text = toks[i].text ? toks[i].text : "";
            printf("  [%2zu] %-13s line=%zu col=%-3zu text=\"%s\"\n",
                   i, kindName(toks[i].kind),
                   toks[i].line, toks[i].col, text);
            free(toks[i].text);
        }
        free(toks);
    }
    return 0;
}
