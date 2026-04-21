#ifndef BESTIARY_TOKEN_H
#define BESTIARY_TOKEN_H

#include<stddef.h>

typedef enum {
    TOK_IDENT, TOK_NUMBER, TOK_EQUALS, TOK_LBRACE, TOK_RBRACE,
    TOK_COMMAND,         // e.g. \det or \otimes
    TOK_AMP, TOK_DBLBACKSLASH,
    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH,    // Math operations
    TOK_LPAREN, TOK_RPAREN,  // parentehses
    TOK_COMMA,
    TOK_DOT,
    TOK_CARET,
    TOK_UNDERSCORE,
    TOK_COLON,
    TOK_SEMICOLON,
    TOK_PIPE,
    TOK_LESS, TOK_GREATER,  // This guy: ' (:3)
    TOK_PRIME,
    TOK_DOLLAR,
    TOK_PERCENT,
    TOK_TILDE,
    TOK_HASH,
    TOK_NEWLINE,
    TOK_EOF,
    TOK_NONE=0
    /* ... */
} TokenKind;

typedef struct {
    TokenKind kind;
    char* text;
    size_t line, col;
} Token;

#endif
