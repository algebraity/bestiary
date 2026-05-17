#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include "ast.h"

/* ---------- Helper methods ---------- */

// Duplicate a C string (returns NULL on NULL input)
static char* dupstr(const char* s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char* r = malloc(n + 1);
    if (!r) return NULL;
    memcpy(r, s, n + 1);
    return r;
}

// Allocate a zero-initialized AstNode with the given kind and source position
static AstNode* newNode(AstKind kind, size_t line, size_t col) {
    AstNode* n = calloc(1, sizeof(AstNode));
    n->kind = kind;
    n->line = line;
    n->col  = col;
    return n;
}

// Pad with `n` spaces (used by astPrint)
static void pad(int n) { for (int i = 0; i < n; i++) putchar(' '); }

// Stringify an operator tag for printing
static const char* opStr(AstOp op, const char* name) {
    switch (op) {
        case OP_ADD: return "+";
        case OP_SUB: return "-";
        case OP_MUL: return "*";
        case OP_DIV: return "/";
        case OP_NEG: return "-u";
        case OP_POS: return "+u";
        case OP_CMD: return name ? name : "\\?";
    }
    return "?";
}

/* ---------- Construct methods ---------- */

AstNode* astNumber(long long v, size_t line, size_t col) {
    AstNode* n = newNode(AST_NUMBER, line, col);
    n->as.number = v;
    return n;
}

AstNode* astDecimal(long double v, size_t line, size_t col) {
    AstNode* n = newNode(AST_DECIMAL, line, col);
    n->as.decimal = v;
    return n;
}

AstNode* astString(const char* s, size_t line, size_t col) {
    AstNode* n = newNode(AST_STRING, line, col);
    n->as.ident = dupstr(s);
    return n;
}

AstNode* astIdent(const char* name, size_t line, size_t col) {
    AstNode* n = newNode(AST_IDENT, line, col);
    n->as.ident = dupstr(name);
    return n;
}

AstNode* astBinop(AstOp op, const char* opname, AstNode* lhs, AstNode* rhs) {
    AstNode* n = newNode(AST_BINOP, lhs ? lhs->line : 0, lhs ? lhs->col : 0);
    n->as.binop.op     = op;
    n->as.binop.opname = opname ? dupstr(opname) : NULL;
    n->as.binop.lhs    = lhs;
    n->as.binop.rhs    = rhs;
    return n;
}

AstNode* astUnary(AstOp op, AstNode* rand) {
    AstNode* n = newNode(AST_UNARY, rand ? rand->line : 0, rand ? rand->col : 0);
    n->as.unary.op   = op;
    n->as.unary.rand = rand;
    return n;
}

AstNode* astPower(AstNode* base, AstNode* exp) {
    AstNode* n = newNode(AST_POWER, base ? base->line : 0, base ? base->col : 0);
    n->as.power.base = base;
    n->as.power.exp  = exp;
    return n;
}

AstNode* astSubscript(AstNode* base, AstNode* sub) {
    AstNode* n = newNode(AST_SUBSCRIPT, base ? base->line : 0, base ? base->col : 0);
    n->as.subscript.base = base;
    n->as.subscript.sub  = sub;
    return n;
}

AstNode* astCall(const char* name, AstNode** args, size_t nargs, size_t line, size_t col) {
    AstNode* n = newNode(AST_CALL, line, col);
    n->as.call.name  = dupstr(name);
    n->as.call.args  = args;
    n->as.call.nargs = nargs;
    return n;
}

AstNode* astTuple(AstNode** items, size_t count) {
    AstNode* n = newNode(AST_TUPLE, 0, 0);
    n->as.tuple.items = items;
    n->as.tuple.n     = count;
    return n;
}

AstNode* astSet(AstNode** items, size_t count, size_t line, size_t col) {
    AstNode* n = newNode(AST_SET, line, col);
    n->as.tuple.items = items;
    n->as.tuple.n     = count;
    return n;
}

AstNode* astMatrix(const char* tag, AstNode** flat, size_t* rowlens, size_t nrows,
                   size_t line, size_t col) {
    AstNode* n = newNode(AST_MATRIX, line, col);
    n->as.matrix.tag     = dupstr(tag);
    n->as.matrix.flat    = flat;
    n->as.matrix.rowlens = rowlens;
    n->as.matrix.nrows   = nrows;
    return n;
}

AstNode* astAssign(const char* name, AstNode* rhs) {
    AstNode* n = newNode(AST_ASSIGN, rhs ? rhs->line : 0, rhs ? rhs->col : 0);
    n->as.assign.name = dupstr(name);
    n->as.assign.rhs  = rhs;
    return n;
}

AstNode* astIndexAssign(const char* name, AstNode* index, AstNode* rhs) {
    AstNode* n = newNode(AST_INDEX_ASSIGN, rhs ? rhs->line : 0, rhs ? rhs->col : 0);
    n->as.indexAssign.name  = dupstr(name);
    n->as.indexAssign.index = index;
    n->as.indexAssign.rhs   = rhs;
    return n;
}

AstNode* astSeq(AstNode** stmts, size_t count) {
    AstNode* n = newNode(AST_SEQ, 0, 0);
    n->as.seq.stmts = stmts;
    n->as.seq.n     = count;
    return n;
}

/* ---------- Free ---------- */

// Recursively free an AST subtree (safe on NULL)
void astFree(AstNode* n) {
    if (!n) return;
    switch (n->kind) {
        case AST_NUMBER:
        case AST_DECIMAL:
            break;
        case AST_STRING:
        case AST_IDENT:
            free(n->as.ident);
            break;
        case AST_BINOP:
            free(n->as.binop.opname);
            astFree(n->as.binop.lhs);
            astFree(n->as.binop.rhs);
            break;
        case AST_UNARY:
            astFree(n->as.unary.rand);
            break;
        case AST_POWER:
            astFree(n->as.power.base);
            astFree(n->as.power.exp);
            break;
        case AST_SUBSCRIPT:
            astFree(n->as.subscript.base);
            astFree(n->as.subscript.sub);
            break;
        case AST_CALL:
            free(n->as.call.name);
            for (size_t i = 0; i < n->as.call.nargs; i++) astFree(n->as.call.args[i]);
            free(n->as.call.args);
            break;
        case AST_TUPLE:
        case AST_SET:
            for (size_t i = 0; i < n->as.tuple.n; i++) astFree(n->as.tuple.items[i]);
            free(n->as.tuple.items);
            break;
        case AST_MATRIX: {
            size_t total = 0;
            for (size_t r = 0; r < n->as.matrix.nrows; r++) total += n->as.matrix.rowlens[r];
            for (size_t i = 0; i < total; i++) astFree(n->as.matrix.flat[i]);
            free(n->as.matrix.flat);
            free(n->as.matrix.rowlens);
            free(n->as.matrix.tag);
            break;
        }
        case AST_ASSIGN:
            free(n->as.assign.name);
            astFree(n->as.assign.rhs);
            break;
        case AST_INDEX_ASSIGN:
            free(n->as.indexAssign.name);
            astFree(n->as.indexAssign.index);
            astFree(n->as.indexAssign.rhs);
            break;
        case AST_SEQ:
            for (size_t i = 0; i < n->as.seq.n; i++) astFree(n->as.seq.stmts[i]);
            free(n->as.seq.stmts);
            break;
    }
    free(n);
}

/* ---------- Clone ---------- */

// Clone an array of AST child nodes
static AstNode** cloneNodeArray(AstNode** nodes, size_t count) {
    if (count == 0) return NULL;
    AstNode** out = calloc(count, sizeof(AstNode*));
    if (!out) return NULL;
    for (size_t i = 0; i < count; i++) {
        out[i] = astClone(nodes[i]);
        if (!out[i]) {
            for (size_t j = 0; j < i; j++) astFree(out[j]);
            free(out);
            return NULL;
        }
    }
    return out;
}

// Recursively clone an AST subtree
AstNode* astClone(const AstNode* n) {
    if (!n) return NULL;

    switch (n->kind) {
        case AST_NUMBER:
            return astNumber(n->as.number, n->line, n->col);
        case AST_DECIMAL:
            return astDecimal(n->as.decimal, n->line, n->col);
        case AST_STRING:
            return astString(n->as.ident, n->line, n->col);
        case AST_IDENT:
            return astIdent(n->as.ident, n->line, n->col);
        case AST_BINOP:
            return astBinop(n->as.binop.op, n->as.binop.opname,
                            astClone(n->as.binop.lhs),
                            astClone(n->as.binop.rhs));
        case AST_UNARY:
            return astUnary(n->as.unary.op, astClone(n->as.unary.rand));
        case AST_POWER:
            return astPower(astClone(n->as.power.base), astClone(n->as.power.exp));
        case AST_SUBSCRIPT:
            return astSubscript(astClone(n->as.subscript.base), astClone(n->as.subscript.sub));
        case AST_CALL:
            return astCall(n->as.call.name,
                           cloneNodeArray(n->as.call.args, n->as.call.nargs),
                           n->as.call.nargs, n->line, n->col);
        case AST_TUPLE:
            return astTuple(cloneNodeArray(n->as.tuple.items, n->as.tuple.n),
                            n->as.tuple.n);
        case AST_SET:
            return astSet(cloneNodeArray(n->as.tuple.items, n->as.tuple.n),
                          n->as.tuple.n, n->line, n->col);
        case AST_MATRIX: {
            size_t total = 0;
            for (size_t r = 0; r < n->as.matrix.nrows; r++) total += n->as.matrix.rowlens[r];

            size_t* rowlens = NULL;
            if (n->as.matrix.nrows > 0) {
                rowlens = malloc(n->as.matrix.nrows * sizeof(size_t));
                if (!rowlens) return NULL;
                memcpy(rowlens, n->as.matrix.rowlens, n->as.matrix.nrows * sizeof(size_t));
            }

            AstNode** flat = cloneNodeArray(n->as.matrix.flat, total);
            if (!flat && total > 0) {
                free(rowlens);
                return NULL;
            }

            return astMatrix(n->as.matrix.tag, flat, rowlens, n->as.matrix.nrows,
                             n->line, n->col);
        }
        case AST_ASSIGN:
            return astAssign(n->as.assign.name, astClone(n->as.assign.rhs));
        case AST_INDEX_ASSIGN:
            return astIndexAssign(n->as.indexAssign.name,
                                  astClone(n->as.indexAssign.index),
                                  astClone(n->as.indexAssign.rhs));
        case AST_SEQ:
            return astSeq(cloneNodeArray(n->as.seq.stmts, n->as.seq.n),
                          n->as.seq.n);
    }
    return NULL;
}

/* ---------- Pretty-print ---------- */

// Print the AST with `ind` leading spaces (debug helper)
void astPrint(AstNode* n, int ind) {
    if (!n) { pad(ind); printf("<null>\n"); return; }
    pad(ind);
    switch (n->kind) {
        case AST_NUMBER:   printf("Num %lld\n", n->as.number); break;
        case AST_DECIMAL:  printf("Dec %Lg\n", n->as.decimal); break;
        case AST_STRING:   printf("String \"%s\"\n", n->as.ident); break;
        case AST_IDENT:    printf("Ident %s\n", n->as.ident); break;
        case AST_BINOP:
            printf("Binop %s\n", opStr(n->as.binop.op, n->as.binop.opname));
            astPrint(n->as.binop.lhs, ind + 2);
            astPrint(n->as.binop.rhs, ind + 2);
            break;
        case AST_UNARY:
            printf("Unary %s\n", opStr(n->as.unary.op, NULL));
            astPrint(n->as.unary.rand, ind + 2);
            break;
        case AST_POWER:
            printf("Power\n");
            astPrint(n->as.power.base, ind + 2);
            astPrint(n->as.power.exp, ind + 2);
            break;
        case AST_SUBSCRIPT:
            printf("Subscript\n");
            astPrint(n->as.subscript.base, ind + 2);
            astPrint(n->as.subscript.sub, ind + 2);
            break;
        case AST_CALL:
            printf("Call \\%s (%zu args)\n", n->as.call.name, n->as.call.nargs);
            for (size_t i = 0; i < n->as.call.nargs; i++) astPrint(n->as.call.args[i], ind + 2);
            break;
        case AST_TUPLE:
            printf("Tuple (%zu)\n", n->as.tuple.n);
            for (size_t i = 0; i < n->as.tuple.n; i++) astPrint(n->as.tuple.items[i], ind + 2);
            break;
        case AST_SET:
            printf("Set (%zu)\n", n->as.tuple.n);
            for (size_t i = 0; i < n->as.tuple.n; i++) astPrint(n->as.tuple.items[i], ind + 2);
            break;
        case AST_MATRIX: {
            printf("Matrix[%s] (%zu rows)\n",
                   n->as.matrix.tag ? n->as.matrix.tag : "?",
                   n->as.matrix.nrows);
            size_t k = 0;
            for (size_t r = 0; r < n->as.matrix.nrows; r++) {
                pad(ind + 2); printf("row %zu:\n", r);
                for (size_t c = 0; c < n->as.matrix.rowlens[r]; c++)
                    astPrint(n->as.matrix.flat[k++], ind + 4);
            }
            break;
        }
        case AST_ASSIGN:
            printf("Assign %s =\n", n->as.assign.name);
            astPrint(n->as.assign.rhs, ind + 2);
            break;
        case AST_INDEX_ASSIGN:
            printf("IndexAssign %s[] =\n", n->as.indexAssign.name);
            astPrint(n->as.indexAssign.index, ind + 2);
            astPrint(n->as.indexAssign.rhs, ind + 2);
            break;
        case AST_SEQ:
            printf("Seq (%zu)\n", n->as.seq.n);
            for (size_t i = 0; i < n->as.seq.n; i++) astPrint(n->as.seq.stmts[i], ind + 2);
            break;
    }
}
