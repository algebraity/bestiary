#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<limits.h>
#include "parser.h"
#include "ast.h"
#include "token.h"

/* ---------- Helper methods ---------- */

// Duplicate a C string (returns NULL on NULL input)
static char* dupstr(const char* s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char* r = malloc(n + 1);
    memcpy(r, s, n + 1);
    return r;
}

/* ---------- Growable AstNode* buffer ---------- */

typedef struct {
    AstNode** data;
    size_t len, cap;
} NodeBuf;

// Init an empty buffer
static void nbInit(NodeBuf* b) { b->data = NULL; b->len = b->cap = 0; }

// Append an entry, growing by doubling
static void nbPush(NodeBuf* b, AstNode* n) {
    if (b->len == b->cap) {
        b->cap = b->cap ? b->cap * 2 : 4;
        b->data = realloc(b->data, b->cap * sizeof(AstNode*));
    }
    b->data[b->len++] = n;
}

/* ---------- Parser state ---------- */

typedef struct {
    Token* toks;
    size_t n;
    size_t pos;
    ParseError* err;
    int failed;
} P;

// Peek at the current token
static Token* peek(P* p) { return &p->toks[p->pos]; }

// Peek `off` tokens ahead (clamps to last)
static Token* peekAt(P* p, size_t off) {
    size_t i = p->pos + off;
    return i < p->n ? &p->toks[i] : &p->toks[p->n - 1];
}

// Consume and return the current token
static Token* advance(P* p) { return &p->toks[p->pos++]; }

// True iff current token matches kind k
static int check(P* p, TokenKind k) { return peek(p)->kind == k; }

// Consume if current token matches kind k; return 1 on match
static int match(P* p, TokenKind k) {
    if (check(p, k)) { p->pos++; return 1; }
    return 0;
}

// True iff we are at TOK_EOF
static int atEnd(P* p) { return check(p, TOK_EOF); }

// Record a fatal parse error (only the first sticks)
static void parseError(P* p, const char* msg) {
    if (p->failed) return;
    p->failed = 1;
    Token* t = peek(p);
    if (p->err) { p->err->msg = msg; p->err->line = t->line; p->err->col = t->col; }
}

// True if the token is a TOK_COMMAND whose text equals `name`
static int isCmdNamed(Token* t, const char* name) {
    return t->kind == TOK_COMMAND && t->text && strcmp(t->text, name) == 0;
}

/* ---------- Infix-command table ---------- */

// Commands that behave like infix binary operators. Add entries here to
// expose new infix operators; the evaluator dispatches them by name
// through the ordinary command registry, so the implementation goes in
// eval.c (via registerCommand).
typedef struct {
    const char* name;
    int prec;
    int rightAssoc;
} InfixCmd;

static const InfixCmd INFIX_CMDS[] = {
    { "cdot",   20, 0 },
    { "times",  20, 0 },
    { "otimes", 20, 0 },
    { "cup",    20, 0 },
    { "cap",    20, 0 },
    { "oplus",  10, 0 },
    { NULL,      0, 0 }
};

// Return the table entry for `name`, or NULL if not an infix command
static const InfixCmd* findInfixCmd(const char* name) {
    if (!name) return NULL;
    for (const InfixCmd* c = INFIX_CMDS; c->name; c++)
        if (strcmp(c->name, name) == 0) return c;
    return NULL;
}

/* ---------- Matrix-environment table ---------- */

// LaTeX environments that parse as matrices. The tag string is stored
// on the AST node so the evaluator can dispatch e.g. __pmatrix__ with
// __matrix__ as a fallback.
static int isMatrixEnv(const char* name) {
    if (!name) return 0;
    return !strcmp(name, "matrix")
        || !strcmp(name, "pmatrix")
        || !strcmp(name, "bmatrix")
        || !strcmp(name, "Bmatrix")
        || !strcmp(name, "vmatrix")
        || !strcmp(name, "Vmatrix")
        || !strcmp(name, "smallmatrix");
}

/* ---------- peekBinop: decide if current token is an infix op ---------- */

typedef struct {
    int present;
    int prec;
    int rightAssoc;
    AstOp op;
    const char* name;     // for OP_CMD
} BinInfo;

// Look at the current token and classify as infix operator (or not)
static BinInfo peekBinop(P* p) {
    BinInfo b = {0};
    Token* t = peek(p);
    switch (t->kind) {
        case TOK_EQEQ: b = (BinInfo){1,  5, 0, OP_CMD, "=="}; break;
        case TOK_PLUS:  b = (BinInfo){1, 10, 0, OP_ADD, NULL}; break;
        case TOK_MINUS: b = (BinInfo){1, 10, 0, OP_SUB, NULL}; break;
        case TOK_STAR:  b = (BinInfo){1, 20, 0, OP_MUL, NULL}; break;
        case TOK_SLASH: b = (BinInfo){1, 20, 0, OP_DIV, NULL}; break;
        case TOK_COMMAND: {
            const InfixCmd* c = findInfixCmd(t->text);
            if (c) b = (BinInfo){1, c->prec, c->rightAssoc, OP_CMD, t->text};
            break;
        }
        default: break;
    }
    return b;
}

/* ---------- Literal parsing helpers ---------- */

// Parse a base-10 integer literal text (the lexer guarantees it's digits)
static int parseUnsignedMagnitude(const char* s, unsigned long long limit, unsigned long long* out) {
    unsigned long long v = 0;
    if (!s || !*s || !out) return 0;
    for (; *s; s++) {
        unsigned digit = (unsigned)(*s - '0');
        if (v > (limit - digit) / 10ULL) return 0;
        v = v * 10ULL + digit;
    }
    *out = v;
    return 1;
}

static int parseIntLit(const char* s, long long* out) {
    unsigned long long v;
    if (!parseUnsignedMagnitude(s, (unsigned long long)LLONG_MAX, &v)) return 0;
    *out = (long long)v;
    return 1;
}

static int parseNegIntLit(const char* s, long long* out) {
    unsigned long long v;
    unsigned long long limit = (unsigned long long)LLONG_MAX + 1ULL;
    if (!parseUnsignedMagnitude(s, limit, &v)) return 0;
    if (v == limit) *out = LLONG_MIN;
    else *out = -(long long)v;
    return 1;
}

// Parse a decimal literal text via strtod
static double parseDecLit(const char* s) { return strtod(s, NULL); }

/* ---------- Forward declarations ---------- */

static AstNode* parseStmt(P* p);
static AstNode* parseExpr(P* p);
static AstNode* parseBinop(P* p, int minprec);
static AstNode* parseUnary(P* p);
static AstNode* parsePostfix(P* p);
static AstNode* parsePrimary(P* p);
static AstNode* parseCommandCall(P* p);
static AstNode* parseRunCall(P* p, Token* cmd);
static AstNode* parseIntegralCall(P* p, Token* cmd);
static AstNode* parseBoundedSingleArgCall(P* p, Token* cmd, const char* outName);
static AstNode* parseCallArgList(P* p);
static AstNode* parseBracketArg(P* p);
static AstNode* parseBraceGroup(P* p);
static AstNode* parseSetLiteral(P* p);
static AstNode* parseScriptOperand(P* p);
static AstNode* parseIntegralScriptOperand(P* p);
static AstNode* parseMatrixLit(P* p);
static AstNode* parseParenOrTuple(P* p);
static AstNode* parseAnglePair(P* p);
static AstNode* parsePipeGroup(P* p);
static AstNode* parseEnvironment(P* p, size_t beginLine, size_t beginCol);
static char*    readBraceIdent(P* p);
static AstNode* stripDifferential(AstNode* n);

static const char* tokenRawText(Token* t, char* scratch, size_t scratchSize) {
    switch (t->kind) {
        case TOK_IDENT:
        case TOK_NUMBER:
        case TOK_DECIMAL:
        case TOK_STRING:
            return t->text ? t->text : "";
        case TOK_COMMAND:
            snprintf(scratch, scratchSize, "\\%s", t->text ? t->text : "");
            return scratch;
        case TOK_DBLBACKSLASH: return "\\\\";
        case TOK_EQUALS:       return "=";
        case TOK_EQEQ:         return "==";
        case TOK_LBRACE:       return "{";
        case TOK_RBRACE:       return "}";
        case TOK_AMP:          return "&";
        case TOK_PLUS:         return "+";
        case TOK_MINUS:        return "-";
        case TOK_STAR:         return "*";
        case TOK_SLASH:        return "/";
        case TOK_LPAREN:       return "(";
        case TOK_RPAREN:       return ")";
        case TOK_LBRACK:       return "[";
        case TOK_RBRACK:       return "]";
        case TOK_COMMA:        return ",";
        case TOK_CARET:        return "^";
        case TOK_UNDERSCORE:   return "_";
        case TOK_BANG:         return "!";
        case TOK_COLON:        return ":";
        case TOK_SEMICOLON:    return ";";
        case TOK_PIPE:         return "|";
        case TOK_LESS:         return "<";
        case TOK_GREATER:      return ">";
        case TOK_PRIME:        return "'";
        case TOK_DOLLAR:       return "$";
        case TOK_PERCENT:      return "%";
        case TOK_TILDE:        return "~";
        case TOK_DOT:          return ".";
        case TOK_HASH:         return "#";
        default:               return "";
    }
}

static int rawPush(char** out, size_t* len, size_t* cap, const char* text) {
    size_t add = strlen(text);
    if (*len + add + 1 > *cap) {
        size_t nextCap = *cap ? *cap * 2 : 64;
        while (nextCap < *len + add + 1) nextCap *= 2;
        char* next = realloc(*out, nextCap);
        if (!next) return 0;
        *out = next;
        *cap = nextCap;
    }
    memcpy(*out + *len, text, add);
    *len += add;
    (*out)[*len] = '\0';
    return 1;
}

/* ---------- Primary-starter predicate ---------- */

// Token kinds that can begin a primary expression
static int tokStartsPrimary(TokenKind k) {
    switch (k) {
        case TOK_NUMBER: case TOK_DECIMAL: case TOK_STRING: case TOK_IDENT:
        case TOK_LPAREN: case TOK_LBRACE:  case TOK_LBRACK:
        case TOK_LESS:
        case TOK_COMMAND:
            return 1;
        default:
            return 0;
    }
}

/* ---------- Primary expressions ---------- */

// Parse the tightest expression: literal, ident, paren, brace group,
// matrix literal, or command invocation.
static AstNode* parsePrimary(P* p) {
    Token* t = peek(p);
    switch (t->kind) {
        case TOK_NUMBER: {
            Token* x = advance(p);
            long long value;
            if (!parseIntLit(x->text ? x->text : "0", &value)) {
                parseError(p, "integer literal exceeds signed 64-bit range");
                return NULL;
            }
            return astNumber(value, x->line, x->col);
        }
        case TOK_DECIMAL: {
            Token* x = advance(p);
            return astDecimal(parseDecLit(x->text ? x->text : "0"), x->line, x->col);
        }
        case TOK_STRING: {
            Token* x = advance(p);
            return astString(x->text ? x->text : "", x->line, x->col);
        }
        case TOK_IDENT: {
            Token* x = advance(p);
            return astIdent(x->text ? x->text : "", x->line, x->col);
        }
        case TOK_LPAREN:  return parseParenOrTuple(p);
        case TOK_LBRACE:  return parseSetLiteral(p);
        case TOK_LBRACK:  return parseMatrixLit(p);
        case TOK_LESS:    return parseAnglePair(p);
        case TOK_PIPE:    return parsePipeGroup(p);
        case TOK_COMMAND: return parseCommandCall(p);
        default:
            parseError(p, "expected expression");
            return NULL;
    }
}

// Parse '| expr |' and lower it to \l2norm{expr}
static AstNode* parsePipeGroup(P* p) {
    Token* pipe = peek(p);
    if (!match(p, TOK_PIPE)) { parseError(p, "expected '|'"); return NULL; }
    NodeBuf args; nbInit(&args);
    nbPush(&args, parseExpr(p));
    if (!match(p, TOK_PIPE)) { parseError(p, "expected closing '|' "); }
    return astCall("l2norm", args.data, args.len, pipe->line, pipe->col);
}

// Parse '{ expr }'; returns the inner expression (the braces are grouping)
static AstNode* parseBraceGroup(P* p) {
    if (!match(p, TOK_LBRACE)) { parseError(p, "expected '{'"); return NULL; }
    if (check(p, TOK_RBRACE)) {
        advance(p);
        return astSet(NULL, 0, 0, 0);
    }
    AstNode* inner = parseExpr(p);
    if (check(p, TOK_COMMA)) {
        NodeBuf items; nbInit(&items);
        nbPush(&items, inner);
        while (match(p, TOK_COMMA)) nbPush(&items, parseExpr(p));
        if (!match(p, TOK_RBRACE)) { parseError(p, "expected '}'"); }
        return astTuple(items.data, items.len);
    }
    if (!match(p, TOK_RBRACE)) { parseError(p, "expected '}'"); astFree(inner); return NULL; }
    return inner;
}

// Parse a primary-position '{...}' group as a CombSet literal. Command
// arguments and script operands still use parseBraceGroup instead.
static AstNode* parseSetLiteral(P* p) {
    Token* lbrace = peek(p);
    if (!match(p, TOK_LBRACE)) { parseError(p, "expected '{'"); return NULL; }
    if (check(p, TOK_RBRACE)) {
        advance(p);
        return astSet(NULL, 0, lbrace->line, lbrace->col);
    }
    NodeBuf items; nbInit(&items);
    nbPush(&items, parseExpr(p));
    while (match(p, TOK_COMMA)) nbPush(&items, parseExpr(p));
    if (!match(p, TOK_RBRACE)) { parseError(p, "expected '}'"); }
    return astSet(items.data, items.len, lbrace->line, lbrace->col);
}

// Parse the operand of ^ or _. Braced operands keep their historical
// grouping semantics so expressions like A^{-1} continue to work.
static AstNode* parseScriptOperand(P* p) {
    if (check(p, TOK_LBRACE)) return parseBraceGroup(p);
    if (match(p, TOK_MINUS)) {
        if (check(p, TOK_NUMBER)) {
            Token* x = advance(p);
            long long value;
            if (!parseNegIntLit(x->text ? x->text : "0", &value)) {
                parseError(p, "integer literal exceeds signed 64-bit range");
                return NULL;
            }
            return astNumber(value, x->line, x->col);
        }
        return astUnary(OP_NEG, parsePrimary(p));
    }
    if (match(p, TOK_PLUS)) return astUnary(OP_POS, parsePrimary(p));
    if (check(p, TOK_COMMAND)) {
        Token* cmd = advance(p);
        return astCall(cmd->text ? cmd->text : "", NULL, 0, cmd->line, cmd->col);
    }
    return parsePrimary(p);
}

// Integral limits like \int_0^1(...) should consume only the single
// unbraced limit token; braced limits still allow full expressions.
static AstNode* parseIntegralScriptOperand(P* p) {
    if (check(p, TOK_LBRACE)) return parseBraceGroup(p);
    if (match(p, TOK_MINUS)) {
        if (check(p, TOK_NUMBER)) {
            Token* x = advance(p);
            long long value;
            if (!parseNegIntLit(x->text ? x->text : "0", &value)) {
                parseError(p, "integer literal exceeds signed 64-bit range");
                return NULL;
            }
            return astNumber(value, x->line, x->col);
        }
        return astUnary(OP_NEG, parsePrimary(p));
    }
    if (match(p, TOK_PLUS)) return astUnary(OP_POS, parsePrimary(p));
    if (check(p, TOK_COMMAND)) {
        Token* cmd = advance(p);
        return astCall(cmd->text ? cmd->text : "", NULL, 0, cmd->line, cmd->col);
    }
    return parsePrimary(p);
}

// Parse '( expr )' or '( a, b, c )'
static AstNode* parseParenOrTuple(P* p) {
    if (!match(p, TOK_LPAREN)) { parseError(p, "expected '('"); return NULL; }
    AstNode* first = parseExpr(p);
    if (check(p, TOK_COMMA)) {
        NodeBuf items; nbInit(&items);
        nbPush(&items, first);
        while (match(p, TOK_COMMA)) nbPush(&items, parseExpr(p));
        if (!match(p, TOK_RPAREN)) { parseError(p, "expected ')'"); }
        return astTuple(items.data, items.len);
    }
    if (!match(p, TOK_RPAREN)) { parseError(p, "expected ')'"); }
    return first;
}

static AstNode* parseCallArgList(P* p) {
    return parseParenOrTuple(p);
}

// Parse '< expr , expr >' and lower it to '\charIP{expr}{expr}'
static AstNode* parseAnglePair(P* p) {
    Token* less = peek(p);
    if (!match(p, TOK_LESS)) { parseError(p, "expected '<'"); return NULL; }

    AstNode* lhs = parseExpr(p);
    if (!match(p, TOK_COMMA)) {
        parseError(p, "expected ',' in angle pair");
        astFree(lhs);
        return NULL;
    }
    AstNode* rhs = parseExpr(p);
    if (!match(p, TOK_GREATER)) {
        parseError(p, "expected '>'");
        astFree(lhs);
        astFree(rhs);
        return NULL;
    }

    AstNode** args = malloc(2 * sizeof(AstNode*));
    if (!args) {
        parseError(p, "out of memory");
        astFree(lhs);
        astFree(rhs);
        return NULL;
    }
    args[0] = lhs;
    args[1] = rhs;
    return astCall("charIP", args, 2, less->line, less->col);
}

// Parse '[ r0c0 , r0c1 ; r1c0 , r1c1 ]' (short-form matrix literal)
static AstNode* parseMatrixLit(P* p) {
    Token* lbrack = peek(p);
    if (!match(p, TOK_LBRACK)) { parseError(p, "expected '['"); return NULL; }

    NodeBuf flat; nbInit(&flat);
    size_t* rowlens = NULL;
    size_t nrows = 0, rowcap = 0;

    // Empty matrix: [ ]
    if (check(p, TOK_RBRACK)) {
        advance(p);
        return astMatrix("matrix", flat.data, rowlens, 0, lbrack->line, lbrack->col);
    }

    for (;;) {
        size_t rowcount = 0;
        nbPush(&flat, parseExpr(p)); rowcount++;
        while (match(p, TOK_COMMA)) {
            nbPush(&flat, parseExpr(p));
            rowcount++;
        }
        if (nrows == rowcap) {
            rowcap = rowcap ? rowcap * 2 : 4;
            rowlens = realloc(rowlens, rowcap * sizeof(size_t));
        }
        rowlens[nrows++] = rowcount;

        if (match(p, TOK_SEMICOLON)) continue;        // another row
        if (check(p, TOK_RBRACK))    break;
        parseError(p, "expected ',' ';' or ']' in matrix");
        break;
    }
    match(p, TOK_RBRACK);
    return astMatrix("matrix", flat.data, rowlens, nrows, lbrack->line, lbrack->col);
}

// Read a '{ ident }' group and return a freshly allocated copy of the ident
static char* readBraceIdent(P* p) {
    if (!match(p, TOK_LBRACE)) { parseError(p, "expected '{' after \\begin/\\end"); return NULL; }
    Token* t = peek(p);
    if (t->kind != TOK_IDENT) { parseError(p, "expected environment name"); return NULL; }
    advance(p);
    if (!match(p, TOK_RBRACE)) { parseError(p, "expected '}' after environment name"); return NULL; }
    return dupstr(t->text ? t->text : "");
}

// Parse the body of \begin{env}...\end{env}. The leading \begin and its
// {env} group have NOT been consumed yet -- we consume them here, parse
// cells separated by '&' / rows separated by '\\', then consume the
// closing \end{env}. Only matrix-like environments are currently
// supported; extend isMatrixEnv (and add new handlers) as needed.
static AstNode* parseEnvironment(P* p, size_t beginLine, size_t beginCol) {
    char* env = readBraceIdent(p);
    if (!env) return NULL;

    if (!isMatrixEnv(env)) {
        parseError(p, "unknown \\begin environment");
        free(env);
        return NULL;
    }

    NodeBuf flat; nbInit(&flat);
    size_t* rowlens = NULL;
    size_t nrows = 0, rowcap = 0;
    size_t rowcount = 0;

    // Read cells until we hit \end
    while (!atEnd(p) && !p->failed && !isCmdNamed(peek(p), "end")) {
        nbPush(&flat, parseExpr(p));
        rowcount++;

        if (match(p, TOK_AMP)) continue;              // next cell in same row
        if (match(p, TOK_DBLBACKSLASH)) {             // row separator
            if (nrows == rowcap) {
                rowcap = rowcap ? rowcap * 2 : 4;
                rowlens = realloc(rowlens, rowcap * sizeof(size_t));
            }
            rowlens[nrows++] = rowcount;
            rowcount = 0;
            continue;
        }
        if (isCmdNamed(peek(p), "end")) break;
        parseError(p, "expected '&', '\\\\' or \\end in environment");
        break;
    }

    // Final row (if a trailing '\\' wasn't given)
    if (rowcount > 0) {
        if (nrows == rowcap) {
            rowcap = rowcap ? rowcap * 2 : 4;
            rowlens = realloc(rowlens, rowcap * sizeof(size_t));
        }
        rowlens[nrows++] = rowcount;
    }

    // Consume \end{env} and verify the name matches
    if (!p->failed) {
        if (!isCmdNamed(peek(p), "end")) {
            parseError(p, "expected \\end");
        } else {
            advance(p);
            char* endName = readBraceIdent(p);
            if (endName) {
                if (strcmp(endName, env) != 0) parseError(p, "\\end name doesn't match \\begin");
                free(endName);
            }
        }
    }

    AstNode* out = astMatrix(env, flat.data, rowlens, nrows, beginLine, beginCol);
    free(env);
    return out;
}

// Parse a command invocation. Two shapes:
//   1) \begin{env} ... \end{env}   -- diverted to parseEnvironment
//   2) \cmd{a1}...{an}             -- zero or more {..} / [..] arg groups
// A zero-arg \cmd (like \pi) produces AST_CALL with nargs == 0.
static AstNode* parseCommandCall(P* p) {
    Token* cmd = advance(p);
    if (cmd->kind != TOK_COMMAND) { parseError(p, "expected command"); return NULL; }

    const char* name = cmd->text ? cmd->text : "";

    if (strcmp(name, "begin") == 0) return parseEnvironment(p, cmd->line, cmd->col);
    if (strcmp(name, "end") == 0) {
        parseError(p, "unexpected \\end without matching \\begin");
        return NULL;
    }
    if (strcmp(name, "run") == 0) return parseRunCall(p, cmd);
    if (strcmp(name, "int") == 0 || strcmp(name, "integral") == 0) {
        return parseIntegralCall(p, cmd);
    }
    if (strcmp(name, "funcArea") == 0) {
        return parseBoundedSingleArgCall(p, cmd, "funcArea");
    }

    NodeBuf args; nbInit(&args);
    for (;;) {
        if (check(p, TOK_LBRACE)) {
            nbPush(&args, parseBraceGroup(p));
            continue;
        }
        if (check(p, TOK_LPAREN)) {
            nbPush(&args, parseParenOrTuple(p));
            continue;
        }
        if (check(p, TOK_LBRACK)) {
            nbPush(&args, parseBracketArg(p));
            continue;
        }
        break;
    }
    return astCall(name, args.data, args.len, cmd->line, cmd->col);
}

static AstNode* parseRunCall(P* p, Token* cmd) {
    TokenKind closeKind = TOK_NONE;
    if (match(p, TOK_LBRACE)) closeKind = TOK_RBRACE;
    else if (match(p, TOK_LPAREN)) closeKind = TOK_RPAREN;
    else {
        return astCall("run", NULL, 0, cmd->line, cmd->col);
    }

    Token* first = peek(p);
    if (first->kind == TOK_STRING && peekAt(p, 1)->kind == closeKind) {
        advance(p);
        advance(p);
        AstNode** args = malloc(sizeof(AstNode*));
        if (!args) { parseError(p, "out of memory"); return NULL; }
        args[0] = astString(first->text ? first->text : "", first->line, first->col);
        return astCall("run", args, 1, cmd->line, cmd->col);
    }

    char* raw = NULL;
    size_t len = 0;
    size_t cap = 0;
    while (!atEnd(p) && !check(p, closeKind)) {
        char scratch[128];
        const char* text = tokenRawText(peek(p), scratch, sizeof(scratch));
        if (!rawPush(&raw, &len, &cap, text)) {
            free(raw);
            parseError(p, "out of memory");
            return NULL;
        }
        advance(p);
    }
    if (!match(p, closeKind)) {
        free(raw);
        parseError(p, "expected closing delimiter after \\run filename");
        return NULL;
    }

    AstNode** args = malloc(sizeof(AstNode*));
    if (!args) {
        free(raw);
        parseError(p, "out of memory");
        return NULL;
    }
    args[0] = astString(raw ? raw : "", cmd->line, cmd->col);
    free(raw);
    return astCall("run", args, 1, cmd->line, cmd->col);
}

static AstNode* parseIntegralArg(P* p) {
    if (check(p, TOK_LBRACE)) return parseBraceGroup(p);
    if (check(p, TOK_LPAREN)) return parseParenOrTuple(p);
    if (check(p, TOK_LBRACK)) return parseBracketArg(p);
    parseError(p, "expected integral argument");
    return NULL;
}

static AstNode* parseIntegralCall(P* p, Token* cmd) {
    AstNode* lower = NULL;
    AstNode* upper = NULL;

    for (int i = 0; i < 2; i++) {
        if (!lower && match(p, TOK_UNDERSCORE)) {
            lower = parseIntegralScriptOperand(p);
            continue;
        }
        if (!upper && match(p, TOK_CARET)) {
            upper = parseIntegralScriptOperand(p);
            continue;
        }
        break;
    }

    AstNode* integrand = stripDifferential(parseIntegralArg(p));
    NodeBuf args; nbInit(&args);
    nbPush(&args, integrand);

    if ((lower && !upper) || (!lower && upper)) {
        astFree(lower);
        astFree(upper);
        parseError(p, "integral bounds require both lower and upper limits");
        return astCall("int", args.data, args.len, cmd->line, cmd->col);
    }

    if (lower && upper) {
        nbPush(&args, lower);
        nbPush(&args, upper);
    }
    return astCall("int", args.data, args.len, cmd->line, cmd->col);
}

static AstNode* parseBoundedSingleArgCall(P* p, Token* cmd, const char* outName) {
    AstNode* lower = NULL;
    AstNode* upper = NULL;

    for (int i = 0; i < 2; i++) {
        if (!lower && match(p, TOK_UNDERSCORE)) {
            lower = parseIntegralScriptOperand(p);
            continue;
        }
        if (!upper && match(p, TOK_CARET)) {
            upper = parseIntegralScriptOperand(p);
            continue;
        }
        break;
    }

    AstNode* body = parseIntegralArg(p);
    NodeBuf args; nbInit(&args);
    nbPush(&args, body);

    if ((lower && !upper) || (!lower && upper)) {
        astFree(lower);
        astFree(upper);
        parseError(p, "bounded command requires both lower and upper limits");
        return astCall(outName, args.data, args.len, cmd->line, cmd->col);
    }
    if (lower && upper) {
        nbPush(&args, lower);
        nbPush(&args, upper);
    }
    return astCall(outName, args.data, args.len, cmd->line, cmd->col);
}

static AstNode* parseBracketArg(P* p) {
    if (!match(p, TOK_LBRACK)) { parseError(p, "expected '['"); return NULL; }

    if (check(p, TOK_RBRACK)) {
        advance(p);
        NodeBuf empty; nbInit(&empty);
        return astTuple(empty.data, 0);
    }

    AstNode* first = parseExpr(p);
    if (match(p, TOK_COLON)) {
        AstNode* second = parseExpr(p);
        NodeBuf pairs; nbInit(&pairs);

        AstNode** pairItems = malloc(2 * sizeof(AstNode*));
        pairItems[0] = first;
        pairItems[1] = second;
        nbPush(&pairs, astTuple(pairItems, 2));

        while (match(p, TOK_COMMA)) {
            AstNode* lhs = parseExpr(p);
            if (!match(p, TOK_COLON)) { parseError(p, "expected ':' in mapping entry"); }
            AstNode* rhs = parseExpr(p);
            AstNode** items = malloc(2 * sizeof(AstNode*));
            items[0] = lhs;
            items[1] = rhs;
            nbPush(&pairs, astTuple(items, 2));
        }
        if (!match(p, TOK_RBRACK)) { parseError(p, "expected ']'"); }
        return astTuple(pairs.data, pairs.len);
    }

    if (match(p, TOK_COMMA)) {
        NodeBuf items; nbInit(&items);
        nbPush(&items, first);
        do {
            nbPush(&items, parseExpr(p));
        } while (match(p, TOK_COMMA));
        if (!match(p, TOK_RBRACK)) { parseError(p, "expected ']'"); }
        return astTuple(items.data, items.len);
    }

    if (!match(p, TOK_RBRACK)) { parseError(p, "expected ']'"); }
    return first;
}

static int isIdentNamed(const AstNode* n, const char* name) {
    return n && n->kind == AST_IDENT && n->as.ident && strcmp(n->as.ident, name) == 0;
}

static AstNode* detachMulSide(AstNode* n, AstNode* keep, AstNode* drop) {
    n->as.binop.lhs = NULL;
    n->as.binop.rhs = NULL;
    free(n->as.binop.opname);
    free(n);
    astFree(drop);
    return stripDifferential(keep);
}

static AstNode* stripDifferential(AstNode* n) {
    if (!n) return NULL;
    if (n->kind == AST_IDENT && n->as.ident) {
        size_t len = strlen(n->as.ident);
        if (len > 2 && strcmp(n->as.ident + len - 2, "dx") == 0) {
            n->as.ident[len - 2] = '\0';
        } else if (len == 2 && strcmp(n->as.ident, "dx") == 0) {
            size_t line = n->line, col = n->col;
            astFree(n);
            return astNumber(1, line, col);
        }
        return n;
    }
    if (n->kind == AST_BINOP && n->as.binop.op == OP_MUL) {
        AstNode* lhs = n->as.binop.lhs;
        AstNode* rhs = n->as.binop.rhs;
        if (isIdentNamed(rhs, "dx")) return detachMulSide(n, lhs, rhs);
        if (isIdentNamed(lhs, "dx")) return detachMulSide(n, rhs, lhs);
    }
    return n;
}

/* ---------- Postfix (^, _, implicit multiplication) ---------- */

// Parse a primary then any trailing ^expr / _expr and implicit-mul
// chains (e.g. 2\pi, (x+1)(y+2)). Stops at binop tokens, \end, and
// anything that isn't a primary-starter.
static AstNode* parsePostfix(P* p) {
    AstNode* left = parsePrimary(p);
    for (;;) {
        if (match(p, TOK_CARET)) {
            AstNode* r = parseScriptOperand(p);
            left = astPower(left, r);
            continue;
        }
        if (match(p, TOK_UNDERSCORE)) {
            AstNode* r = parseScriptOperand(p);
            left = astSubscript(left, r);
            continue;
        }
        if (check(p, TOK_BANG)) {
            Token* bang = advance(p);
            AstNode** args = malloc(sizeof(AstNode*));
            if (!args) {
                parseError(p, "out of memory");
                astFree(left);
                return NULL;
            }
            args[0] = left;
            left = astCall("factorial", args, 1, bang->line, bang->col);
            continue;
        }
        if (check(p, TOK_PRIME)) {
            Token* prime = advance(p);
            AstNode** args = malloc(sizeof(AstNode*));
            if (!args) {
                parseError(p, "out of memory");
                astFree(left);
                return NULL;
            }
            args[0] = left;
            left = astCall("derivative", args, 1, prime->line, prime->col);
            continue;
        }
        if (check(p, TOK_LPAREN) && left && left->kind == AST_IDENT) {
            AstNode* arg = parseCallArgList(p);
            AstNode** args = malloc(2 * sizeof(AstNode*));
            if (!args) {
                parseError(p, "out of memory");
                astFree(left);
                astFree(arg);
                return NULL;
            }
            args[0] = left;
            args[1] = arg;
            left = astCall("eval", args, 2, left->line, left->col);
            continue;
        }
        Token* nx = peek(p);
        if (tokStartsPrimary(nx->kind)) {
            // Don't consume infix commands via implicit-mul...
            if (nx->kind == TOK_COMMAND && findInfixCmd(nx->text)) break;
            // ...and treat \end as a hard boundary (closes an environment).
            if (isCmdNamed(nx, "end")) break;
            AstNode* r = parsePrimary(p);
            while (check(p, TOK_CARET) || check(p, TOK_UNDERSCORE)) {
                if (match(p, TOK_CARET))         r = astPower(r, parseScriptOperand(p));
                else { match(p, TOK_UNDERSCORE); r = astSubscript(r, parseScriptOperand(p)); }
            }
            left = astBinop(OP_MUL, NULL, left, r);
            continue;
        }
        break;
    }
    return left;
}

/* ---------- Unary prefix ---------- */

// Parse prefix -x / +x (right-associative through chains)
static AstNode* parseUnary(P* p) {
    if (match(p, TOK_MINUS)) {
        if (check(p, TOK_NUMBER)) {
            Token* x = advance(p);
            long long value;
            if (!parseNegIntLit(x->text ? x->text : "0", &value)) {
                parseError(p, "integer literal exceeds signed 64-bit range");
                return NULL;
            }
            return astNumber(value, x->line, x->col);
        }
        return astUnary(OP_NEG, parseUnary(p));
    }
    if (match(p, TOK_PLUS))  return astUnary(OP_POS, parseUnary(p));
    return parsePostfix(p);
}

/* ---------- Binary ops via precedence climbing ---------- */

// Parse an expression whose leading operator has precedence >= minprec
static AstNode* parseBinop(P* p, int minprec) {
    AstNode* left = parseUnary(p);
    for (;;) {
        BinInfo b = peekBinop(p);
        if (!b.present || b.prec < minprec) break;
        const char* opname = b.name;
        advance(p);
        int nextmin = b.rightAssoc ? b.prec : b.prec + 1;
        AstNode* right = parseBinop(p, nextmin);
        left = astBinop(b.op, opname, left, right);
    }
    return left;
}

static AstNode* parseExpr(P* p) { return parseBinop(p, 0); }

/* ---------- Statements ---------- */

// A statement is either an assignment `ident = expr` (detected by a
// two-token lookahead) or an expression.
static AstNode* parseStmt(P* p) {
    if (check(p, TOK_IDENT) && peekAt(p, 1)->kind == TOK_EQUALS) {
        Token* name = advance(p);
        advance(p);         // consume '='
        AstNode* rhs = parseExpr(p);
        return astAssign(name->text ? name->text : "", rhs);
    }
    return parseExpr(p);
}

/* ---------- Entry point ---------- */

// Parse a token stream into an AST_SEQ root; caller frees with astFree
AstNode* bstParse(Token* tokens, size_t ntokens, ParseError* err) {
    P p = { .toks = tokens, .n = ntokens, .pos = 0, .err = err, .failed = 0 };
    if (err) { err->msg = NULL; err->line = 0; err->col = 0; }

    NodeBuf stmts; nbInit(&stmts);
    while (!atEnd(&p) && !p.failed) {
        AstNode* s = parseStmt(&p);
        if (!s) break;
        nbPush(&stmts, s);
        while (match(&p, TOK_SEMICOLON)) { /* soak extra separators */ }
    }

    if (p.failed) {
        for (size_t i = 0; i < stmts.len; i++) astFree(stmts.data[i]);
        free(stmts.data);
        return NULL;
    }
    return astSeq(stmts.data, stmts.len);
}
