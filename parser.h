#ifndef BESTIARY_PARSER_H
#define BESTIARY_PARSER_H

#include "token.h"
#include "ast.h"

/* ---------- Parse error struct ---------- */

typedef struct {
    const char* msg;
    size_t line, col;
} ParseError;

/* ---------- Prototypes ---------- */

// Consume a flat token array (as produced by bstLex) and return an
// AST_SEQ-rooted tree. On failure returns NULL; *err (if non-NULL) is
// populated with a diagnostic. Caller owns the returned tree.
AstNode* bstParse(Token* tokens, size_t ntokens, ParseError* err);

#endif
