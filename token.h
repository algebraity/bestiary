#include<stddef.h>

typedef enum {
    TOK_IDENT, TOK_NUMBER, TOK_EQUALS, TOK_LBRACE, TOK_RBRACE,
    TOK_COMMAND,         // e.g. \det or \otimes
    TOK_AMP, TOK_DBLBACKSLASH,
    TOK_EOF,
    TOK_NONE=0
    /* ... */
} TokenKind;

typedef struct {
    TokenKind kind;
    char* text;         
    size_t line, col;
} Token;

