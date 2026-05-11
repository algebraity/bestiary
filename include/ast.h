#ifndef BESTIARY_AST_H
#define BESTIARY_AST_H

#include<stddef.h>

/* ---------- AST node kinds ---------- */

typedef enum {
    AST_NUMBER,      // integer literal
    AST_DECIMAL,     // floating literal
    AST_STRING,      // quoted string literal
    AST_IDENT,       // bare identifier / variable reference
    AST_BINOP,       // a <op> b
    AST_UNARY,       // prefix -/+
    AST_POWER,       // a ^ b
    AST_SUBSCRIPT,   // a _ b
    AST_CALL,        // \cmd{a1}...{an}   (zero args allowed, e.g. \pi)
    AST_TUPLE,       // (a, b, c)         -- also used for argument lists
    AST_SET,         // {a, b, c}         -- CombSet literal in primary position
    AST_MATRIX,      // [a,b;c,d]  or  \begin{env}..\end{env}
    AST_ASSIGN,      // name = expr
    AST_SEQ          // stmt ; stmt ; stmt
} AstKind;

/* ---------- Binary/unary operator tags ---------- */

typedef enum {
    OP_ADD, OP_SUB, OP_MUL, OP_DIV,
    OP_NEG, OP_POS,
    OP_CMD                               // infix \cmd; opname carries the command
} AstOp;

/* ---------- AST node struct ---------- */

typedef struct AstNode AstNode;

struct AstNode {
    AstKind kind;
    size_t line, col;
    union {
        long long number;
        long double    decimal;
        char*     ident;
        struct { AstOp op; char* opname; AstNode* lhs; AstNode* rhs; } binop;
        struct { AstOp op; AstNode* rand; } unary;
        struct { AstNode* base; AstNode* exp; } power;
        struct { AstNode* base; AstNode* sub; } subscript;
        struct { char* name; AstNode** args; size_t nargs; } call;
        struct { AstNode** items; size_t n; } tuple;
        // rowlens[r] cells live contiguously in flat[] row after row.
        // tag is the environment name (e.g. "pmatrix") or "matrix" for [..].
        struct { char* tag; AstNode** flat; size_t* rowlens; size_t nrows; } matrix;
        struct { char* name; AstNode* rhs; } assign;
        struct { AstNode** stmts; size_t n; } seq;
    } as;
};

/* ---------- Construct methods ---------- */

AstNode* astNumber(long long n, size_t line, size_t col);
AstNode* astDecimal(long double d, size_t line, size_t col);
AstNode* astString(const char* s, size_t line, size_t col);
AstNode* astIdent(const char* name, size_t line, size_t col);
AstNode* astBinop(AstOp op, const char* opname, AstNode* lhs, AstNode* rhs);
AstNode* astUnary(AstOp op, AstNode* rand);
AstNode* astPower(AstNode* base, AstNode* exp);
AstNode* astSubscript(AstNode* base, AstNode* sub);
AstNode* astCall(const char* name, AstNode** args, size_t nargs, size_t line, size_t col);
AstNode* astTuple(AstNode** items, size_t n);
AstNode* astSet(AstNode** items, size_t n, size_t line, size_t col);
AstNode* astMatrix(const char* tag, AstNode** flat, size_t* rowlens, size_t nrows, size_t line, size_t col);
AstNode* astAssign(const char* name, AstNode* rhs);
AstNode* astSeq(AstNode** stmts, size_t n);

/* ---------- Free and print ---------- */

void astFree(AstNode* node);
void astPrint(AstNode* node, int indent);

#endif
