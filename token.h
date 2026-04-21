#ifndef BESTIARY_TOKEN_H
#define BESTIARY_TOKEN_H

#include<stddef.h>

typedef enum {
    TOK_NONE=0,
    TOK_IDENT, TOK_NUMBER, TOK_DECIMAL, TOK_EQUALS, TOK_LBRACE, TOK_RBRACE,
    TOK_COMMAND,         // e.g. \det or \otimes
    TOK_AMP, TOK_DBLBACKSLASH,
    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH,    // Math operations
    TOK_LPAREN, TOK_RPAREN,  // parentehses
    TOK_LBRACK, TOK_RBRACK,  // left and right brackets
    TOK_COMMA,
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
    TOK_DOT,    // .
    TOK_HASH,
    TOK_EOF
    /* ... */
} TokenKind;

typedef struct {
    TokenKind kind;
    char* text;
    size_t line, col;
} Token;

#endif
