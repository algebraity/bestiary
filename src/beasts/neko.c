#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<math.h>
#include "neko.h"

/* ---------- Helper methods ---------- */

static char* dupstr(const char* s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char* out = malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static NekoExpr* newExpr(NekoExprKind kind) {
    NekoExpr* expr = calloc(1, sizeof(NekoExpr));
    if (expr) expr->kind = kind;
    return expr;
}

static bool isConst(const NekoExpr* expr, double value) {
    return expr
        && expr->kind == NEKO_EXPR_CONST
        && fabs(expr->as.constant - value) <= 1e-12;
}

static bool sameVar(const NekoExpr* expr, const char* var) {
    return expr
        && expr->kind == NEKO_EXPR_VAR
        && expr->as.var
        && var
        && strcmp(expr->as.var, var) == 0;
}

static bool isUnaryKind(NekoExprKind kind) {
    switch (kind) {
        case NEKO_EXPR_NEG:
        case NEKO_EXPR_SIN:
        case NEKO_EXPR_COS:
        case NEKO_EXPR_TAN:
        case NEKO_EXPR_ASIN:
        case NEKO_EXPR_ACOS:
        case NEKO_EXPR_ATAN:
        case NEKO_EXPR_EXP:
        case NEKO_EXPR_LOG:
        case NEKO_EXPR_SQRT:
        case NEKO_EXPR_ABS:
            return true;
        default:
            return false;
    }
}

static bool isBinaryKind(NekoExprKind kind) {
    switch (kind) {
        case NEKO_EXPR_ADD:
        case NEKO_EXPR_SUB:
        case NEKO_EXPR_MUL:
        case NEKO_EXPR_DIV:
        case NEKO_EXPR_POW:
            return true;
        default:
            return false;
    }
}

static bool exprEqual(const NekoExpr* a, const NekoExpr* b);

/* ---------- Constructors ---------- */

NekoExpr* nekoConst(double c) {
    NekoExpr* expr = newExpr(NEKO_EXPR_CONST);
    if (expr) expr->as.constant = c;
    return expr;
}

NekoExpr* nekoVar(const char* name) {
    NekoExpr* expr = newExpr(NEKO_EXPR_VAR);
    if (expr) expr->as.var = dupstr(name ? name : "x");
    return expr;
}

NekoExpr* nekoUnary(NekoExprKind kind, NekoExpr* arg) {
    if (!arg || !isUnaryKind(kind)) {
        nekoFreeExpr(arg);
        return NULL;
    }
    NekoExpr* expr = newExpr(kind);
    if (!expr) {
        nekoFreeExpr(arg);
        return NULL;
    }
    expr->as.unary.arg = arg;
    return expr;
}

NekoExpr* nekoBinary(NekoExprKind kind, NekoExpr* lhs, NekoExpr* rhs) {
    if (!lhs || !rhs || !isBinaryKind(kind)) {
        nekoFreeExpr(lhs);
        nekoFreeExpr(rhs);
        return NULL;
    }
    NekoExpr* expr = newExpr(kind);
    if (!expr) {
        nekoFreeExpr(lhs);
        nekoFreeExpr(rhs);
        return NULL;
    }
    expr->as.binary.lhs = lhs;
    expr->as.binary.rhs = rhs;
    return expr;
}

NekoExpr* nekoCall(const char* name, NekoExpr** args, int nargs) {
    if (!name || nargs < 0) return NULL;
    NekoExpr* expr = newExpr(NEKO_EXPR_CALL);
    if (!expr) return NULL;
    expr->as.call.name = dupstr(name);
    expr->as.call.args = args;
    expr->as.call.nargs = nargs;
    return expr;
}

NekoExpr* nekoAdd(NekoExpr* lhs, NekoExpr* rhs) { return nekoBinary(NEKO_EXPR_ADD, lhs, rhs); }
NekoExpr* nekoSub(NekoExpr* lhs, NekoExpr* rhs) { return nekoBinary(NEKO_EXPR_SUB, lhs, rhs); }
NekoExpr* nekoMul(NekoExpr* lhs, NekoExpr* rhs) { return nekoBinary(NEKO_EXPR_MUL, lhs, rhs); }
NekoExpr* nekoDiv(NekoExpr* lhs, NekoExpr* rhs) { return nekoBinary(NEKO_EXPR_DIV, lhs, rhs); }
NekoExpr* nekoPow(NekoExpr* lhs, NekoExpr* rhs) { return nekoBinary(NEKO_EXPR_POW, lhs, rhs); }
NekoExpr* nekoNeg(NekoExpr* arg) { return nekoUnary(NEKO_EXPR_NEG, arg); }
NekoExpr* nekoSin(NekoExpr* arg) { return nekoUnary(NEKO_EXPR_SIN, arg); }
NekoExpr* nekoCos(NekoExpr* arg) { return nekoUnary(NEKO_EXPR_COS, arg); }
NekoExpr* nekoTan(NekoExpr* arg) { return nekoUnary(NEKO_EXPR_TAN, arg); }
NekoExpr* nekoAsin(NekoExpr* arg) { return nekoUnary(NEKO_EXPR_ASIN, arg); }
NekoExpr* nekoAcos(NekoExpr* arg) { return nekoUnary(NEKO_EXPR_ACOS, arg); }
NekoExpr* nekoAtan(NekoExpr* arg) { return nekoUnary(NEKO_EXPR_ATAN, arg); }
NekoExpr* nekoExp(NekoExpr* arg) { return nekoUnary(NEKO_EXPR_EXP, arg); }
NekoExpr* nekoLog(NekoExpr* arg) { return nekoUnary(NEKO_EXPR_LOG, arg); }
NekoExpr* nekoSqrt(NekoExpr* arg) { return nekoUnary(NEKO_EXPR_SQRT, arg); }
NekoExpr* nekoAbs(NekoExpr* arg) { return nekoUnary(NEKO_EXPR_ABS, arg); }

/* ---------- Free and clone ---------- */

void nekoFreeExpr(NekoExpr* expr) {
    if (!expr) return;
    switch (expr->kind) {
        case NEKO_EXPR_VAR:
            free(expr->as.var);
            break;
        case NEKO_EXPR_ADD:
        case NEKO_EXPR_SUB:
        case NEKO_EXPR_MUL:
        case NEKO_EXPR_DIV:
        case NEKO_EXPR_POW:
            nekoFreeExpr(expr->as.binary.lhs);
            nekoFreeExpr(expr->as.binary.rhs);
            break;
        case NEKO_EXPR_NEG:
        case NEKO_EXPR_SIN:
        case NEKO_EXPR_COS:
        case NEKO_EXPR_TAN:
        case NEKO_EXPR_ASIN:
        case NEKO_EXPR_ACOS:
        case NEKO_EXPR_ATAN:
        case NEKO_EXPR_EXP:
        case NEKO_EXPR_LOG:
        case NEKO_EXPR_SQRT:
        case NEKO_EXPR_ABS:
            nekoFreeExpr(expr->as.unary.arg);
            break;
        case NEKO_EXPR_CALL:
            free(expr->as.call.name);
            for (int i = 0; i < expr->as.call.nargs; i++)
                nekoFreeExpr(expr->as.call.args[i]);
            free(expr->as.call.args);
            break;
        case NEKO_EXPR_CONST:
            break;
    }
    free(expr);
}

NekoExpr* nekoCloneExpr(const NekoExpr* expr) {
    if (!expr) return NULL;
    switch (expr->kind) {
        case NEKO_EXPR_CONST:
            return nekoConst(expr->as.constant);
        case NEKO_EXPR_VAR:
            return nekoVar(expr->as.var);
        case NEKO_EXPR_ADD:
        case NEKO_EXPR_SUB:
        case NEKO_EXPR_MUL:
        case NEKO_EXPR_DIV:
        case NEKO_EXPR_POW:
            return nekoBinary(expr->kind,
                              nekoCloneExpr(expr->as.binary.lhs),
                              nekoCloneExpr(expr->as.binary.rhs));
        case NEKO_EXPR_NEG:
        case NEKO_EXPR_SIN:
        case NEKO_EXPR_COS:
        case NEKO_EXPR_TAN:
        case NEKO_EXPR_ASIN:
        case NEKO_EXPR_ACOS:
        case NEKO_EXPR_ATAN:
        case NEKO_EXPR_EXP:
        case NEKO_EXPR_LOG:
        case NEKO_EXPR_SQRT:
        case NEKO_EXPR_ABS:
            return nekoUnary(expr->kind, nekoCloneExpr(expr->as.unary.arg));
        case NEKO_EXPR_CALL: {
            NekoExpr** args = NULL;
            if (expr->as.call.nargs > 0) {
                args = calloc((size_t)expr->as.call.nargs, sizeof(NekoExpr*));
                if (!args) return NULL;
                for (int i = 0; i < expr->as.call.nargs; i++)
                    args[i] = nekoCloneExpr(expr->as.call.args[i]);
            }
            return nekoCall(expr->as.call.name, args, expr->as.call.nargs);
        }
    }
    return NULL;
}

/* ---------- Evaluation ---------- */

double nekoEvalExpr(const NekoExpr* expr, const char* var, double x) {
    if (!expr) return NAN;
    switch (expr->kind) {
        case NEKO_EXPR_CONST:
            return expr->as.constant;
        case NEKO_EXPR_VAR:
            return (!var || strcmp(expr->as.var, var) == 0) ? x : NAN;
        case NEKO_EXPR_ADD:
            return nekoEvalExpr(expr->as.binary.lhs, var, x)
                 + nekoEvalExpr(expr->as.binary.rhs, var, x);
        case NEKO_EXPR_SUB:
            return nekoEvalExpr(expr->as.binary.lhs, var, x)
                 - nekoEvalExpr(expr->as.binary.rhs, var, x);
        case NEKO_EXPR_MUL:
            return nekoEvalExpr(expr->as.binary.lhs, var, x)
                 * nekoEvalExpr(expr->as.binary.rhs, var, x);
        case NEKO_EXPR_DIV:
            return nekoEvalExpr(expr->as.binary.lhs, var, x)
                 / nekoEvalExpr(expr->as.binary.rhs, var, x);
        case NEKO_EXPR_POW:
            return pow(nekoEvalExpr(expr->as.binary.lhs, var, x),
                       nekoEvalExpr(expr->as.binary.rhs, var, x));
        case NEKO_EXPR_NEG:
            return -nekoEvalExpr(expr->as.unary.arg, var, x);
        case NEKO_EXPR_SIN:
            return sin(nekoEvalExpr(expr->as.unary.arg, var, x));
        case NEKO_EXPR_COS:
            return cos(nekoEvalExpr(expr->as.unary.arg, var, x));
        case NEKO_EXPR_TAN:
            return tan(nekoEvalExpr(expr->as.unary.arg, var, x));
        case NEKO_EXPR_ASIN:
            return asin(nekoEvalExpr(expr->as.unary.arg, var, x));
        case NEKO_EXPR_ACOS:
            return acos(nekoEvalExpr(expr->as.unary.arg, var, x));
        case NEKO_EXPR_ATAN:
            return atan(nekoEvalExpr(expr->as.unary.arg, var, x));
        case NEKO_EXPR_EXP:
            return exp(nekoEvalExpr(expr->as.unary.arg, var, x));
        case NEKO_EXPR_LOG:
            return log(nekoEvalExpr(expr->as.unary.arg, var, x));
        case NEKO_EXPR_SQRT:
            return sqrt(nekoEvalExpr(expr->as.unary.arg, var, x));
        case NEKO_EXPR_ABS:
            return fabs(nekoEvalExpr(expr->as.unary.arg, var, x));
        case NEKO_EXPR_CALL:
            return NAN;
    }
    return NAN;
}

/* ---------- Simplification ---------- */

typedef struct {
    NekoExpr** items;
    size_t len;
    size_t cap;
} ExprVec;

typedef struct {
    int degree;
    NekoExpr* coeff;
} PolyTerm;

typedef struct {
    PolyTerm* items;
    size_t len;
    size_t cap;
} PolyVec;

static bool exprVecPush(ExprVec* vec, NekoExpr* expr) {
    if (!vec || !expr) return false;
    if (vec->len == vec->cap) {
        size_t nextCap = vec->cap ? vec->cap * 2 : 4;
        NekoExpr** next = realloc(vec->items, nextCap * sizeof(NekoExpr*));
        if (!next) {
            nekoFreeExpr(expr);
            return false;
        }
        vec->items = next;
        vec->cap = nextCap;
    }
    vec->items[vec->len++] = expr;
    return true;
}

static void exprVecFree(ExprVec* vec) {
    if (!vec) return;
    for (size_t i = 0; i < vec->len; i++) nekoFreeExpr(vec->items[i]);
    free(vec->items);
    vec->items = NULL;
    vec->len = 0;
    vec->cap = 0;
}

static bool polyVecPush(PolyVec* vec, int degree, NekoExpr* coeff) {
    if (!vec || !coeff) {
        nekoFreeExpr(coeff);
        return false;
    }
    if (vec->len == vec->cap) {
        size_t nextCap = vec->cap ? vec->cap * 2 : 4;
        PolyTerm* next = realloc(vec->items, nextCap * sizeof(PolyTerm));
        if (!next) {
            nekoFreeExpr(coeff);
            return false;
        }
        vec->items = next;
        vec->cap = nextCap;
    }
    vec->items[vec->len++] = (PolyTerm){ .degree = degree, .coeff = coeff };
    return true;
}

static void polyVecFree(PolyVec* vec) {
    if (!vec) return;
    for (size_t i = 0; i < vec->len; i++) nekoFreeExpr(vec->items[i].coeff);
    free(vec->items);
    vec->items = NULL;
    vec->len = 0;
    vec->cap = 0;
}

static int exprDependsOnVar(const NekoExpr* expr, const char* var) {
    if (!expr || !var) return 0;

    switch (expr->kind) {
        case NEKO_EXPR_CONST:
            return 0;
        case NEKO_EXPR_VAR:
            return expr->as.var && strcmp(expr->as.var, var) == 0;
        case NEKO_EXPR_ADD:
        case NEKO_EXPR_SUB:
        case NEKO_EXPR_MUL:
        case NEKO_EXPR_DIV:
        case NEKO_EXPR_POW:
            return exprDependsOnVar(expr->as.binary.lhs, var)
                || exprDependsOnVar(expr->as.binary.rhs, var);
        case NEKO_EXPR_NEG:
        case NEKO_EXPR_SIN:
        case NEKO_EXPR_COS:
        case NEKO_EXPR_TAN:
        case NEKO_EXPR_ASIN:
        case NEKO_EXPR_ACOS:
        case NEKO_EXPR_ATAN:
        case NEKO_EXPR_EXP:
        case NEKO_EXPR_LOG:
        case NEKO_EXPR_SQRT:
        case NEKO_EXPR_ABS:
            return exprDependsOnVar(expr->as.unary.arg, var);
        case NEKO_EXPR_CALL:
            for (int i = 0; i < expr->as.call.nargs; i++) {
                if (exprDependsOnVar(expr->as.call.args[i], var)) return 1;
            }
            return 0;
    }
    return 0;
}

static bool constNonNegativeInteger(const NekoExpr* expr, int* out) {
    if (!expr || expr->kind != NEKO_EXPR_CONST) return false;
    double rounded = round(expr->as.constant);
    if (expr->as.constant < -1e-12 || fabs(expr->as.constant - rounded) > 1e-9) return false;
    if (out) *out = (int)rounded;
    return true;
}

static bool splitPolynomialMonomial(const NekoExpr* expr, const char* var, int* degree, NekoExpr** coeff) {
    if (!expr || !var || !degree || !coeff) return false;

    if (!exprDependsOnVar(expr, var)) {
        *degree = 0;
        *coeff = nekoCloneExpr(expr);
        return *coeff != NULL;
    }

    if (sameVar(expr, var)) {
        *degree = 1;
        *coeff = nekoConst(1.0);
        return *coeff != NULL;
    }

    switch (expr->kind) {
        case NEKO_EXPR_NEG: {
            NekoExpr* innerCoeff = NULL;
            int innerDegree = 0;
            if (!splitPolynomialMonomial(expr->as.unary.arg, var, &innerDegree, &innerCoeff)) return false;
            *degree = innerDegree;
            *coeff = nekoSimplify(nekoNeg(innerCoeff));
            return *coeff != NULL;
        }
        case NEKO_EXPR_POW: {
            int power = 0;
            if (!sameVar(expr->as.binary.lhs, var) || !constNonNegativeInteger(expr->as.binary.rhs, &power)) {
                return false;
            }
            *degree = power;
            *coeff = nekoConst(1.0);
            return *coeff != NULL;
        }
        case NEKO_EXPR_MUL: {
            int leftDegree = 0, rightDegree = 0;
            NekoExpr* leftCoeff = NULL;
            NekoExpr* rightCoeff = NULL;
            if (!splitPolynomialMonomial(expr->as.binary.lhs, var, &leftDegree, &leftCoeff)
                    || !splitPolynomialMonomial(expr->as.binary.rhs, var, &rightDegree, &rightCoeff)) {
                nekoFreeExpr(leftCoeff);
                nekoFreeExpr(rightCoeff);
                return false;
            }
            *degree = leftDegree + rightDegree;
            *coeff = nekoSimplify(nekoMul(leftCoeff, rightCoeff));
            return *coeff != NULL;
        }
        case NEKO_EXPR_DIV:
            if (!exprDependsOnVar(expr->as.binary.rhs, var)) {
                int numDegree = 0;
                NekoExpr* numCoeff = NULL;
                if (!splitPolynomialMonomial(expr->as.binary.lhs, var, &numDegree, &numCoeff)) return false;
                *degree = numDegree;
                *coeff = nekoSimplify(nekoDiv(numCoeff, nekoCloneExpr(expr->as.binary.rhs)));
                return *coeff != NULL;
            }
            return false;
        default:
            return false;
    }
}

static bool polyVecAddCoeff(PolyVec* vec, int degree, NekoExpr* coeff) {
    if (!vec || !coeff) {
        nekoFreeExpr(coeff);
        return false;
    }

    for (size_t i = 0; i < vec->len; i++) {
        if (vec->items[i].degree == degree) {
            NekoExpr* merged = nekoSimplify(nekoAdd(vec->items[i].coeff, coeff));
            vec->items[i].coeff = merged;
            return merged != NULL;
        }
    }

    return polyVecPush(vec, degree, coeff);
}

static bool collectPolynomialTerms(const NekoExpr* expr, const char* var, int sign, PolyVec* terms) {
    if (!expr || !var || !terms) return false;

    switch (expr->kind) {
        case NEKO_EXPR_ADD:
            return collectPolynomialTerms(expr->as.binary.lhs, var, sign, terms)
                && collectPolynomialTerms(expr->as.binary.rhs, var, sign, terms);
        case NEKO_EXPR_SUB:
            return collectPolynomialTerms(expr->as.binary.lhs, var, sign, terms)
                && collectPolynomialTerms(expr->as.binary.rhs, var, -sign, terms);
        case NEKO_EXPR_NEG:
            return collectPolynomialTerms(expr->as.unary.arg, var, -sign, terms);
        default: {
            int degree = 0;
            NekoExpr* coeff = NULL;
            if (!splitPolynomialMonomial(expr, var, &degree, &coeff)) return false;
            if (sign < 0) coeff = nekoSimplify(nekoNeg(coeff));
            return coeff ? polyVecAddCoeff(terms, degree, coeff) : false;
        }
    }
}

static int polyTermCompare(const void* lhs, const void* rhs) {
    const PolyTerm* left = (const PolyTerm*)lhs;
    const PolyTerm* right = (const PolyTerm*)rhs;
    return (left->degree > right->degree) - (left->degree < right->degree);
}

static NekoExpr* buildPolynomialTermOwned(const char* var, int degree, NekoExpr* coeff) {
    if (!coeff) return NULL;
    if (degree == 0) return coeff;

    NekoExpr* power = degree == 1
        ? nekoVar(var)
        : nekoPow(nekoVar(var), nekoConst((double)degree));
    if (!power) {
        nekoFreeExpr(coeff);
        return NULL;
    }

    if (isConst(coeff, 1.0)) {
        nekoFreeExpr(coeff);
        return power;
    }
    if (isConst(coeff, -1.0)) {
        nekoFreeExpr(coeff);
        return nekoNeg(power);
    }
    return nekoSimplify(nekoMul(coeff, power));
}

static NekoExpr* rebuildPolynomialExpr(const char* var, PolyVec* terms) {
    if (!terms || !var) return NULL;
    if (terms->len == 0) return nekoConst(0.0);

    qsort(terms->items, terms->len, sizeof(PolyTerm), polyTermCompare);

    NekoExpr* out = NULL;
    for (size_t i = 0; i < terms->len; i++) {
        NekoExpr* coeff = terms->items[i].coeff;
        terms->items[i].coeff = NULL;
        if (!coeff || isConst(coeff, 0.0)) {
            nekoFreeExpr(coeff);
            continue;
        }

        NekoExpr* term = buildPolynomialTermOwned(var, terms->items[i].degree, coeff);
        if (!term) {
            nekoFreeExpr(out);
            return NULL;
        }
        out = out ? nekoAdd(out, term) : term;
    }

    return out ? out : nekoConst(0.0);
}

static NekoExpr* normalizePolynomialOwned(NekoExpr* expr, const char* var) {
    if (!expr || !var || !exprDependsOnVar(expr, var)) return expr;

    PolyVec terms = {0};
    if (!collectPolynomialTerms(expr, var, 1, &terms)) {
        polyVecFree(&terms);
        return expr;
    }

    NekoExpr* rebuilt = rebuildPolynomialExpr(var, &terms);
    polyVecFree(&terms);
    if (!rebuilt) return expr;

    nekoFreeExpr(expr);
    return rebuilt;
}

static NekoExpr* normalizePolynomialDefaultOwned(NekoExpr* expr) {
    if (!expr) return NULL;
    if (exprDependsOnVar(expr, "x")) return normalizePolynomialOwned(expr, "x");
    if (exprDependsOnVar(expr, "y")) return normalizePolynomialOwned(expr, "y");
    return expr;
}

static bool collectProductPiecesOwned(NekoExpr* expr, double* coeff, ExprVec* factors) {
    if (!expr || !coeff || !factors) {
        nekoFreeExpr(expr);
        return false;
    }

    switch (expr->kind) {
        case NEKO_EXPR_CONST:
            *coeff *= expr->as.constant;
            nekoFreeExpr(expr);
            return true;
        case NEKO_EXPR_NEG: {
            NekoExpr* arg = expr->as.unary.arg;
            expr->as.unary.arg = NULL;
            free(expr);
            *coeff = -*coeff;
            return collectProductPiecesOwned(arg, coeff, factors);
        }
        case NEKO_EXPR_MUL: {
            NekoExpr* lhs = expr->as.binary.lhs;
            NekoExpr* rhs = expr->as.binary.rhs;
            expr->as.binary.lhs = NULL;
            expr->as.binary.rhs = NULL;
            free(expr);
            if (!collectProductPiecesOwned(lhs, coeff, factors)) {
                nekoFreeExpr(rhs);
                return false;
            }
            if (!collectProductPiecesOwned(rhs, coeff, factors)) {
                exprVecFree(factors);
                return false;
            }
            return true;
        }
        case NEKO_EXPR_DIV:
            if (expr->as.binary.rhs && expr->as.binary.rhs->kind == NEKO_EXPR_CONST) {
                double denom = expr->as.binary.rhs->as.constant;
                NekoExpr* lhs = expr->as.binary.lhs;
                expr->as.binary.lhs = NULL;
                nekoFreeExpr(expr->as.binary.rhs);
                expr->as.binary.rhs = NULL;
                free(expr);
                *coeff /= denom;
                return collectProductPiecesOwned(lhs, coeff, factors);
            }
            break;
        default:
            break;
    }

    return exprVecPush(factors, expr);
}

static NekoExpr* rebuildCollectedProduct(double coeff, ExprVec* factors) {
    if (!factors) return NULL;
    if (fabs(coeff) <= 1e-12) {
        exprVecFree(factors);
        return nekoConst(0.0);
    }
    if (factors->len == 0) {
        free(factors->items);
        factors->items = NULL;
        factors->len = 0;
        factors->cap = 0;
        return nekoConst(coeff);
    }

    size_t index = 0;
    NekoExpr* out = NULL;
    if (fabs(coeff - 1.0) <= 1e-12) {
        out = factors->items[index++];
    } else if (fabs(coeff + 1.0) <= 1e-12 && factors->len == 1) {
        out = nekoNeg(factors->items[index++]);
    } else {
        out = nekoMul(nekoConst(coeff), factors->items[index++]);
    }

    for (; index < factors->len; index++) out = nekoMul(out, factors->items[index]);

    free(factors->items);
    factors->items = NULL;
    factors->len = 0;
    factors->cap = 0;
    return out;
}

NekoExpr* nekoSimplify(NekoExpr* expr) {
    if (!expr) return NULL;

    if (isUnaryKind(expr->kind)) {
        expr->as.unary.arg = nekoSimplify(expr->as.unary.arg);
        NekoExpr* arg = expr->as.unary.arg;

        if (expr->kind == NEKO_EXPR_NEG) {
            if (arg && arg->kind == NEKO_EXPR_CONST) {
                double v = -arg->as.constant;
                nekoFreeExpr(expr);
                return nekoConst(v);
            }
            if (arg && arg->kind == NEKO_EXPR_NEG) {
                NekoExpr* out = arg->as.unary.arg;
                arg->as.unary.arg = NULL;
                nekoFreeExpr(expr);
                return out;
            }
        }
        if (arg && arg->kind == NEKO_EXPR_CONST) {
            double v = nekoEvalExpr(expr, "x", 0.0);
            if (isfinite(v)) {
                nekoFreeExpr(expr);
                return nekoConst(v);
            }
        }
        return expr;
    }

    if (!isBinaryKind(expr->kind)) return expr;

    expr->as.binary.lhs = nekoSimplify(expr->as.binary.lhs);
    expr->as.binary.rhs = nekoSimplify(expr->as.binary.rhs);

    NekoExpr* lhs = expr->as.binary.lhs;
    NekoExpr* rhs = expr->as.binary.rhs;
    if (!lhs || !rhs) return expr;

    if (lhs->kind == NEKO_EXPR_CONST && rhs->kind == NEKO_EXPR_CONST) {
        double v = nekoEvalExpr(expr, "x", 0.0);
        if (isfinite(v)) {
            nekoFreeExpr(expr);
            return nekoConst(v);
        }
    }

    switch (expr->kind) {
        case NEKO_EXPR_ADD:
            if (isConst(lhs, 0.0)) {
                expr->as.binary.rhs = NULL;
                nekoFreeExpr(expr);
                return rhs;
            }
            if (isConst(rhs, 0.0)) {
                expr->as.binary.lhs = NULL;
                nekoFreeExpr(expr);
                return lhs;
            }
            break;
        case NEKO_EXPR_SUB:
            if (isConst(rhs, 0.0)) {
                expr->as.binary.lhs = NULL;
                nekoFreeExpr(expr);
                return lhs;
            }
            if (isConst(lhs, 0.0)) {
                expr->as.binary.rhs = NULL;
                NekoExpr* out = nekoNeg(rhs);
                nekoFreeExpr(expr);
                return nekoSimplify(out);
            }
            break;
        case NEKO_EXPR_MUL:
            if (isConst(lhs, 0.0) || isConst(rhs, 0.0)) {
                nekoFreeExpr(expr);
                return nekoConst(0.0);
            }
            if (isConst(lhs, 1.0)) {
                expr->as.binary.rhs = NULL;
                nekoFreeExpr(expr);
                return rhs;
            }
            if (isConst(rhs, 1.0)) {
                expr->as.binary.lhs = NULL;
                nekoFreeExpr(expr);
                return lhs;
            }
            if (isConst(lhs, -1.0)) {
                expr->as.binary.rhs = NULL;
                NekoExpr* out = nekoNeg(rhs);
                nekoFreeExpr(expr);
                return nekoSimplify(out);
            }
            if (isConst(rhs, -1.0)) {
                expr->as.binary.lhs = NULL;
                NekoExpr* out = nekoNeg(lhs);
                nekoFreeExpr(expr);
                return nekoSimplify(out);
            }
            {
                ExprVec factors = {0};
                double coeff = 1.0;
                if (collectProductPiecesOwned(expr, &coeff, &factors)) {
                    return rebuildCollectedProduct(coeff, &factors);
                }
                exprVecFree(&factors);
                return NULL;
            }
            break;
        case NEKO_EXPR_DIV:
            if (isConst(lhs, 0.0)) {
                nekoFreeExpr(expr);
                return nekoConst(0.0);
            }
            if (isConst(rhs, 1.0)) {
                expr->as.binary.lhs = NULL;
                nekoFreeExpr(expr);
                return lhs;
            }
            if (exprEqual(lhs, rhs)) {
                nekoFreeExpr(expr);
                return nekoConst(1.0);
            }
            if (rhs->kind == NEKO_EXPR_CONST) {
                ExprVec factors = {0};
                double coeff = 1.0;
                NekoExpr* lhsOwned = lhs;
                expr->as.binary.lhs = NULL;
                expr->as.binary.rhs = NULL;
                double denom = rhs->as.constant;
                nekoFreeExpr(rhs);
                free(expr);
                coeff /= denom;
                if (collectProductPiecesOwned(lhsOwned, &coeff, &factors)) {
                    return rebuildCollectedProduct(coeff, &factors);
                }
                exprVecFree(&factors);
                return NULL;
            }
            break;
        case NEKO_EXPR_POW:
            if (isConst(rhs, 0.0)) {
                nekoFreeExpr(expr);
                return nekoConst(1.0);
            }
            if (isConst(rhs, 1.0)) {
                expr->as.binary.lhs = NULL;
                nekoFreeExpr(expr);
                return lhs;
            }
            if (isConst(lhs, 0.0)) {
                nekoFreeExpr(expr);
                return nekoConst(0.0);
            }
            if (isConst(lhs, 1.0)) {
                nekoFreeExpr(expr);
                return nekoConst(1.0);
            }
            break;
        default:
            break;
    }

    return normalizePolynomialDefaultOwned(expr);
}

/* ---------- Differentiation ---------- */

static NekoDiffResult diffOk(NekoExpr* expr) {
    NekoDiffResult r = { .status = expr ? NEKO_OK : NEKO_ERR_INVALID_ARG, .expr = expr };
    return r;
}

static NekoDiffResult diffErr(NekoStatus status) {
    NekoDiffResult r = { .status = status, .expr = NULL };
    return r;
}

static NekoExpr* derivOrFree(const NekoExpr* expr, const char* var, bool* ok) {
    NekoDiffResult d = nekoDifferentiateExpr(expr, var);
    if (d.status != NEKO_OK) {
        *ok = false;
        nekoFreeExpr(d.expr);
        return NULL;
    }
    return d.expr;
}

NekoDiffResult nekoDifferentiateExpr(const NekoExpr* expr, const char* var) {
    if (!expr || !var) return diffErr(NEKO_ERR_INVALID_ARG);

    switch (expr->kind) {
        case NEKO_EXPR_CONST:
            return diffOk(nekoConst(0.0));

        case NEKO_EXPR_VAR:
            return diffOk(nekoConst(strcmp(expr->as.var, var) == 0 ? 1.0 : 0.0));

        case NEKO_EXPR_ADD: {
            bool ok = true;
            NekoExpr* dl = derivOrFree(expr->as.binary.lhs, var, &ok);
            NekoExpr* dr = derivOrFree(expr->as.binary.rhs, var, &ok);
            if (!ok) {
                nekoFreeExpr(dl);
                nekoFreeExpr(dr);
                return diffErr(NEKO_ERR_UNSUPPORTED);
            }
            return diffOk(nekoSimplify(nekoAdd(dl, dr)));
        }

        case NEKO_EXPR_SUB: {
            bool ok = true;
            NekoExpr* dl = derivOrFree(expr->as.binary.lhs, var, &ok);
            NekoExpr* dr = derivOrFree(expr->as.binary.rhs, var, &ok);
            if (!ok) {
                nekoFreeExpr(dl);
                nekoFreeExpr(dr);
                return diffErr(NEKO_ERR_UNSUPPORTED);
            }
            return diffOk(nekoSimplify(nekoSub(dl, dr)));
        }

        case NEKO_EXPR_MUL: {
            bool ok = true;
            NekoExpr* f = nekoCloneExpr(expr->as.binary.lhs);
            NekoExpr* g = nekoCloneExpr(expr->as.binary.rhs);
            NekoExpr* df = derivOrFree(expr->as.binary.lhs, var, &ok);
            NekoExpr* dg = derivOrFree(expr->as.binary.rhs, var, &ok);
            if (!ok) {
                nekoFreeExpr(f); nekoFreeExpr(g); nekoFreeExpr(df); nekoFreeExpr(dg);
                return diffErr(NEKO_ERR_UNSUPPORTED);
            }
            return diffOk(nekoSimplify(nekoAdd(nekoMul(df, g), nekoMul(f, dg))));
        }

        case NEKO_EXPR_DIV: {
            bool ok = true;
            NekoExpr* f = nekoCloneExpr(expr->as.binary.lhs);
            NekoExpr* g = nekoCloneExpr(expr->as.binary.rhs);
            NekoExpr* df = derivOrFree(expr->as.binary.lhs, var, &ok);
            NekoExpr* dg = derivOrFree(expr->as.binary.rhs, var, &ok);
            if (!ok) {
                nekoFreeExpr(f); nekoFreeExpr(g); nekoFreeExpr(df); nekoFreeExpr(dg);
                return diffErr(NEKO_ERR_UNSUPPORTED);
            }
            NekoExpr* numerator = nekoSub(nekoMul(df, nekoCloneExpr(expr->as.binary.rhs)),
                                          nekoMul(f, dg));
            NekoExpr* denominator = nekoPow(g, nekoConst(2.0));
            return diffOk(nekoSimplify(nekoDiv(numerator, denominator)));
        }

        case NEKO_EXPR_POW: {
            bool ok = true;
            const NekoExpr* base = expr->as.binary.lhs;
            const NekoExpr* exponent = expr->as.binary.rhs;

            if (exponent->kind == NEKO_EXPR_CONST) {
                double n = exponent->as.constant;
                NekoExpr* db = derivOrFree(base, var, &ok);
                if (!ok) {
                    nekoFreeExpr(db);
                    return diffErr(NEKO_ERR_UNSUPPORTED);
                }
                NekoExpr* out = nekoMul(nekoMul(nekoConst(n),
                                                nekoPow(nekoCloneExpr(base), nekoConst(n - 1.0))),
                                        db);
                return diffOk(nekoSimplify(out));
            }

            NekoExpr* f = nekoCloneExpr(base);
            NekoExpr* g = nekoCloneExpr(exponent);
            NekoExpr* df = derivOrFree(base, var, &ok);
            NekoExpr* dg = derivOrFree(exponent, var, &ok);
            if (!ok) {
                nekoFreeExpr(f); nekoFreeExpr(g); nekoFreeExpr(df); nekoFreeExpr(dg);
                return diffErr(NEKO_ERR_UNSUPPORTED);
            }

            NekoExpr* inner = nekoAdd(nekoMul(dg, nekoLog(nekoCloneExpr(base))),
                                      nekoMul(g, nekoDiv(df, nekoCloneExpr(base))));
            NekoExpr* out = nekoMul(nekoPow(f, nekoCloneExpr(exponent)), inner);
            return diffOk(nekoSimplify(out));
        }

        case NEKO_EXPR_NEG: {
            NekoDiffResult d = nekoDifferentiateExpr(expr->as.unary.arg, var);
            if (d.status != NEKO_OK) return d;
            return diffOk(nekoSimplify(nekoNeg(d.expr)));
        }

        case NEKO_EXPR_SIN: {
            bool ok = true;
            NekoExpr* du = derivOrFree(expr->as.unary.arg, var, &ok);
            if (!ok) return diffErr(NEKO_ERR_UNSUPPORTED);
            return diffOk(nekoSimplify(nekoMul(nekoCos(nekoCloneExpr(expr->as.unary.arg)), du)));
        }

        case NEKO_EXPR_COS: {
            bool ok = true;
            NekoExpr* du = derivOrFree(expr->as.unary.arg, var, &ok);
            if (!ok) return diffErr(NEKO_ERR_UNSUPPORTED);
            return diffOk(nekoSimplify(nekoMul(nekoNeg(nekoSin(nekoCloneExpr(expr->as.unary.arg))), du)));
        }

        case NEKO_EXPR_TAN: {
            bool ok = true;
            NekoExpr* du = derivOrFree(expr->as.unary.arg, var, &ok);
            if (!ok) return diffErr(NEKO_ERR_UNSUPPORTED);
            NekoExpr* sec2 = nekoDiv(nekoConst(1.0),
                                     nekoPow(nekoCos(nekoCloneExpr(expr->as.unary.arg)), nekoConst(2.0)));
            return diffOk(nekoSimplify(nekoMul(sec2, du)));
        }

        case NEKO_EXPR_ASIN: {
            bool ok = true;
            NekoExpr* u = nekoCloneExpr(expr->as.unary.arg);
            NekoExpr* du = derivOrFree(expr->as.unary.arg, var, &ok);
            if (!ok) {
                nekoFreeExpr(u); nekoFreeExpr(du);
                return diffErr(NEKO_ERR_UNSUPPORTED);
            }
            return diffOk(nekoSimplify(nekoDiv(du, nekoSqrt(nekoSub(nekoConst(1.0), nekoPow(u, nekoConst(2.0)))))));
        }

        case NEKO_EXPR_ACOS: {
            bool ok = true;
            NekoExpr* u = nekoCloneExpr(expr->as.unary.arg);
            NekoExpr* du = derivOrFree(expr->as.unary.arg, var, &ok);
            if (!ok) {
                nekoFreeExpr(u); nekoFreeExpr(du);
                return diffErr(NEKO_ERR_UNSUPPORTED);
            }
            return diffOk(nekoSimplify(nekoNeg(nekoDiv(du, nekoSqrt(nekoSub(nekoConst(1.0), nekoPow(u, nekoConst(2.0))))))));
        }

        case NEKO_EXPR_ATAN: {
            bool ok = true;
            NekoExpr* u = nekoCloneExpr(expr->as.unary.arg);
            NekoExpr* du = derivOrFree(expr->as.unary.arg, var, &ok);
            if (!ok) {
                nekoFreeExpr(u); nekoFreeExpr(du);
                return diffErr(NEKO_ERR_UNSUPPORTED);
            }
            return diffOk(nekoSimplify(nekoDiv(du, nekoAdd(nekoConst(1.0), nekoPow(u, nekoConst(2.0))))));
        }

        case NEKO_EXPR_EXP: {
            bool ok = true;
            NekoExpr* du = derivOrFree(expr->as.unary.arg, var, &ok);
            if (!ok) return diffErr(NEKO_ERR_UNSUPPORTED);
            return diffOk(nekoSimplify(nekoMul(nekoExp(nekoCloneExpr(expr->as.unary.arg)), du)));
        }

        case NEKO_EXPR_LOG: {
            bool ok = true;
            NekoExpr* du = derivOrFree(expr->as.unary.arg, var, &ok);
            if (!ok) return diffErr(NEKO_ERR_UNSUPPORTED);
            return diffOk(nekoSimplify(nekoDiv(du, nekoCloneExpr(expr->as.unary.arg))));
        }

        case NEKO_EXPR_SQRT: {
            bool ok = true;
            NekoExpr* du = derivOrFree(expr->as.unary.arg, var, &ok);
            if (!ok) return diffErr(NEKO_ERR_UNSUPPORTED);
            return diffOk(nekoSimplify(nekoDiv(du, nekoMul(nekoConst(2.0), nekoSqrt(nekoCloneExpr(expr->as.unary.arg))))));
        }

        case NEKO_EXPR_ABS: {
            bool ok = true;
            NekoExpr* u = nekoCloneExpr(expr->as.unary.arg);
            NekoExpr* du = derivOrFree(expr->as.unary.arg, var, &ok);
            if (!ok) {
                nekoFreeExpr(u); nekoFreeExpr(du);
                return diffErr(NEKO_ERR_UNSUPPORTED);
            }
            return diffOk(nekoSimplify(nekoMul(du, nekoDiv(u, nekoAbs(nekoCloneExpr(expr->as.unary.arg))))));
        }

        case NEKO_EXPR_CALL:
            return diffErr(NEKO_ERR_UNSUPPORTED);
    }

    return diffErr(NEKO_ERR_UNSUPPORTED);
}

/* ---------- Function wrappers ---------- */

NekoFunc* nekoFuncFromExpr(const NekoExpr* expr) {
    if (!expr) return NULL;
    NekoFunc* func = calloc(1, sizeof(NekoFunc));
    if (!func) return NULL;
    func->expr = nekoCloneExpr(expr);
    if (!func->expr) {
        free(func);
        return NULL;
    }
    return func;
}

NekoFunc* nekoFuncFromCallback(NekoEvalFn callback, void* userdata) {
    if (!callback) return NULL;
    NekoFunc* func = calloc(1, sizeof(NekoFunc));
    if (!func) return NULL;
    func->callback = callback;
    func->userdata = userdata;
    return func;
}

void nekoFreeFunc(NekoFunc* func) {
    if (!func) return;
    nekoFreeExpr(func->expr);
    free(func);
}

double nekoEvalFunc(const NekoFunc* func, double x) {
    if (!func) return NAN;
    if (func->callback) return func->callback(x, func->userdata);
    if (func->expr) return nekoEvalExpr(func->expr, "x", x);
    return NAN;
}

/* ---------- Symbolic integration ---------- */

static NekoIntegralResult integOk(NekoExpr* expr) {
    NekoIntegralResult r = { .status = expr ? NEKO_OK : NEKO_ERR_INVALID_ARG, .expr = expr };
    return r;
}

static NekoIntegralResult integErr(NekoStatus status) {
    NekoIntegralResult r = { .status = status, .expr = NULL };
    return r;
}

static NekoExpr* integOrFree(const NekoExpr* expr, const char* var, bool* ok) {
    NekoIntegralResult r = nekoIntegrateExpr(expr, var);
    if (r.status != NEKO_OK) {
        *ok = false;
        nekoFreeExpr(r.expr);
        return NULL;
    }
    return r.expr;
}

static bool linearCoeff(const NekoExpr* expr, const char* var, double* a, double* b) {
    if (!expr || !a || !b) return false;
    if (expr->kind == NEKO_EXPR_CONST) {
        *a = 0.0;
        *b = expr->as.constant;
        return true;
    }
    if (sameVar(expr, var)) {
        *a = 1.0;
        *b = 0.0;
        return true;
    }
    if (expr->kind == NEKO_EXPR_NEG) {
        double ca, cb;
        if (!linearCoeff(expr->as.unary.arg, var, &ca, &cb)) return false;
        *a = -ca;
        *b = -cb;
        return true;
    }
    if (expr->kind == NEKO_EXPR_ADD || expr->kind == NEKO_EXPR_SUB) {
        double la, lb, ra, rb;
        if (!linearCoeff(expr->as.binary.lhs, var, &la, &lb)) return false;
        if (!linearCoeff(expr->as.binary.rhs, var, &ra, &rb)) return false;
        *a = expr->kind == NEKO_EXPR_ADD ? la + ra : la - ra;
        *b = expr->kind == NEKO_EXPR_ADD ? lb + rb : lb - rb;
        return true;
    }
    if (expr->kind == NEKO_EXPR_MUL) {
        const NekoExpr* lhs = expr->as.binary.lhs;
        const NekoExpr* rhs = expr->as.binary.rhs;
        double ca, cb;
        if (lhs->kind == NEKO_EXPR_CONST && linearCoeff(rhs, var, &ca, &cb)) {
            *a = lhs->as.constant * ca;
            *b = lhs->as.constant * cb;
            return true;
        }
        if (rhs->kind == NEKO_EXPR_CONST && linearCoeff(lhs, var, &ca, &cb)) {
            *a = rhs->as.constant * ca;
            *b = rhs->as.constant * cb;
            return true;
        }
    }
    return false;
}

static bool splitConstMultiple(const NekoExpr* expr, NekoExpr** inner, double* coeff) {
    if (!expr || !inner || !coeff || expr->kind != NEKO_EXPR_MUL) return false;
    if (expr->as.binary.lhs->kind == NEKO_EXPR_CONST) {
        *coeff = expr->as.binary.lhs->as.constant;
        *inner = nekoCloneExpr(expr->as.binary.rhs);
        return true;
    }
    if (expr->as.binary.rhs->kind == NEKO_EXPR_CONST) {
        *coeff = expr->as.binary.rhs->as.constant;
        *inner = nekoCloneExpr(expr->as.binary.lhs);
        return true;
    }
    return false;
}

static bool exprEqual(const NekoExpr* a, const NekoExpr* b) {
    if (!a || !b || a->kind != b->kind) return false;
    switch (a->kind) {
        case NEKO_EXPR_CONST:
            return fabs(a->as.constant - b->as.constant) <= 1e-9;
        case NEKO_EXPR_VAR:
            return a->as.var && b->as.var && strcmp(a->as.var, b->as.var) == 0;
        case NEKO_EXPR_ADD:
        case NEKO_EXPR_SUB:
        case NEKO_EXPR_MUL:
        case NEKO_EXPR_DIV:
        case NEKO_EXPR_POW:
            return exprEqual(a->as.binary.lhs, b->as.binary.lhs)
                && exprEqual(a->as.binary.rhs, b->as.binary.rhs);
        case NEKO_EXPR_NEG:
        case NEKO_EXPR_SIN:
        case NEKO_EXPR_COS:
        case NEKO_EXPR_TAN:
        case NEKO_EXPR_ASIN:
        case NEKO_EXPR_ACOS:
        case NEKO_EXPR_ATAN:
        case NEKO_EXPR_EXP:
        case NEKO_EXPR_LOG:
        case NEKO_EXPR_SQRT:
        case NEKO_EXPR_ABS:
            return exprEqual(a->as.unary.arg, b->as.unary.arg);
        case NEKO_EXPR_CALL:
            if (!a->as.call.name || !b->as.call.name || strcmp(a->as.call.name, b->as.call.name) != 0
                    || a->as.call.nargs != b->as.call.nargs) return false;
            for (int i = 0; i < a->as.call.nargs; i++)
                if (!exprEqual(a->as.call.args[i], b->as.call.args[i])) return false;
            return true;
    }
    return false;
}

static bool splitConstBorrowed(const NekoExpr* expr, const NekoExpr** inner, double* coeff) {
    if (!expr || !inner || !coeff) return false;
    if (expr->kind == NEKO_EXPR_CONST) {
        *inner = NULL;
        *coeff = expr->as.constant;
        return true;
    }
    if (expr->kind == NEKO_EXPR_MUL) {
        if (expr->as.binary.lhs->kind == NEKO_EXPR_CONST) {
            *coeff = expr->as.binary.lhs->as.constant;
            *inner = expr->as.binary.rhs;
            return true;
        }
        if (expr->as.binary.rhs->kind == NEKO_EXPR_CONST) {
            *coeff = expr->as.binary.rhs->as.constant;
            *inner = expr->as.binary.lhs;
            return true;
        }
    }
    *coeff = 1.0;
    *inner = expr;
    return true;
}

static bool proportionalTo(const NekoExpr* a, const NekoExpr* b, double* coeff) {
    if (!a || !b || !coeff) return false;
    NekoExpr* sa = nekoSimplify(nekoCloneExpr(a));
    NekoExpr* sb = nekoSimplify(nekoCloneExpr(b));
    const NekoExpr* ia = NULL;
    const NekoExpr* ib = NULL;
    double ca = 1.0, cb = 1.0;
    splitConstBorrowed(sa, &ia, &ca);
    splitConstBorrowed(sb, &ib, &cb);

    bool ok = false;
    if (!ia && !ib && fabs(cb) > 1e-12) {
        *coeff = ca / cb;
        ok = true;
    } else if (ia && ib && exprEqual(ia, ib) && fabs(cb) > 1e-12) {
        *coeff = ca / cb;
        ok = true;
    }
    nekoFreeExpr(sa);
    nekoFreeExpr(sb);
    return ok;
}

static NekoExpr* integratePowerOfLinear(const NekoExpr* base, double exponent, const char* var) {
    double a, b;
    if (!linearCoeff(base, var, &a, &b) || fabs(a) <= 1e-12) return NULL;
    if (fabs(exponent + 1.0) <= 1e-12) {
        return nekoDiv(nekoLog(nekoAbs(nekoCloneExpr(base))), nekoConst(a));
    }
    return nekoDiv(nekoPow(nekoCloneExpr(base), nekoConst(exponent + 1.0)),
                   nekoConst(a * (exponent + 1.0)));
}

static bool positiveIntegerPower(double x, int* n) {
    double r = round(x);
    if (fabs(x - r) > 1e-9 || r < 0.0 || r > 64.0) return false;
    if (n) *n = (int)r;
    return true;
}

static NekoExpr* integrateTrigPowerLinear(NekoExprKind trigKind, const NekoExpr* arg, int n, const char* var) {
    double a, b;
    if (!linearCoeff(arg, var, &a, &b) || fabs(a) <= 1e-12) return NULL;
    (void)b;
    if (n == 0) return nekoVar(var);
    if (n == 1) {
        if (trigKind == NEKO_EXPR_SIN) return nekoDiv(nekoNeg(nekoCos(nekoCloneExpr(arg))), nekoConst(a));
        if (trigKind == NEKO_EXPR_COS) return nekoDiv(nekoSin(nekoCloneExpr(arg)), nekoConst(a));
        if (trigKind == NEKO_EXPR_TAN) return nekoDiv(nekoNeg(nekoLog(nekoCos(nekoCloneExpr(arg)))), nekoConst(a));
    }

    NekoExpr* prev = integrateTrigPowerLinear(trigKind, arg, n - 2, var);
    if (!prev) return NULL;
    NekoExpr* term = NULL;
    if (trigKind == NEKO_EXPR_SIN) {
        term = nekoDiv(nekoNeg(nekoMul(nekoPow(nekoSin(nekoCloneExpr(arg)), nekoConst((double)n - 1.0)),
                                      nekoCos(nekoCloneExpr(arg)))),
                       nekoConst(a * (double)n));
        return nekoAdd(term, nekoMul(nekoConst(((double)n - 1.0) / (double)n), prev));
    }
    if (trigKind == NEKO_EXPR_COS) {
        term = nekoDiv(nekoMul(nekoSin(nekoCloneExpr(arg)),
                               nekoPow(nekoCos(nekoCloneExpr(arg)), nekoConst((double)n - 1.0))),
                       nekoConst(a * (double)n));
        return nekoAdd(term, nekoMul(nekoConst(((double)n - 1.0) / (double)n), prev));
    }
    if (trigKind == NEKO_EXPR_TAN) {
        if (n == 2) {
            nekoFreeExpr(prev);
            return nekoSub(nekoDiv(nekoTan(nekoCloneExpr(arg)), nekoConst(a)), nekoVar(var));
        }
        term = nekoDiv(nekoPow(nekoTan(nekoCloneExpr(arg)), nekoConst((double)n - 1.0)),
                       nekoConst(a * ((double)n - 1.0)));
        return nekoSub(term, prev);
    }
    nekoFreeExpr(prev);
    return NULL;
}

static NekoExpr* tryTrigPowerIntegral(const NekoExpr* expr, const char* var) {
    if (!expr || expr->kind != NEKO_EXPR_POW || expr->as.binary.rhs->kind != NEKO_EXPR_CONST) return NULL;
    const NekoExpr* base = expr->as.binary.lhs;
    if (!base || (base->kind != NEKO_EXPR_SIN && base->kind != NEKO_EXPR_COS && base->kind != NEKO_EXPR_TAN)) return NULL;
    int n = 0;
    if (!positiveIntegerPower(expr->as.binary.rhs->as.constant, &n)) return NULL;
    return integrateTrigPowerLinear(base->kind, base->as.unary.arg, n, var);
}

static NekoExpr* integrateUSubPair(const NekoExpr* factor, const NekoExpr* outer, const char* var) {
    if (!factor || !outer) return NULL;
    const NekoExpr* inner = NULL;
    if (isUnaryKind(outer->kind)) {
        inner = outer->as.unary.arg;
    } else if (outer->kind == NEKO_EXPR_POW) {
        inner = outer->as.binary.lhs;
    } else if (outer->kind == NEKO_EXPR_DIV && isConst(outer->as.binary.lhs, 1.0)) {
        inner = outer->as.binary.rhs;
    } else {
        return NULL;
    }

    NekoDiffResult d = nekoDifferentiateExpr(inner, var);
    if (d.status != NEKO_OK) {
        nekoFreeExpr(d.expr);
        return NULL;
    }
    double coeff = 0.0;
    bool ok = proportionalTo(factor, d.expr, &coeff);
    nekoFreeExpr(d.expr);
    if (!ok) return NULL;

    NekoExpr* antiderivative = NULL;
    switch (outer->kind) {
        case NEKO_EXPR_SIN:
            antiderivative = nekoNeg(nekoCos(nekoCloneExpr(inner)));
            break;
        case NEKO_EXPR_COS:
            antiderivative = nekoSin(nekoCloneExpr(inner));
            break;
        case NEKO_EXPR_TAN:
            antiderivative = nekoNeg(nekoLog(nekoCos(nekoCloneExpr(inner))));
            break;
        case NEKO_EXPR_EXP:
            antiderivative = nekoExp(nekoCloneExpr(inner));
            break;
        case NEKO_EXPR_LOG:
            antiderivative = nekoSub(nekoMul(nekoCloneExpr(inner), nekoLog(nekoCloneExpr(inner))),
                                     nekoCloneExpr(inner));
            break;
        case NEKO_EXPR_POW:
            if (outer->as.binary.rhs->kind == NEKO_EXPR_CONST) {
                double n = outer->as.binary.rhs->as.constant;
                antiderivative = fabs(n + 1.0) <= 1e-12
                    ? nekoLog(nekoAbs(nekoCloneExpr(inner)))
                    : nekoDiv(nekoPow(nekoCloneExpr(inner), nekoConst(n + 1.0)), nekoConst(n + 1.0));
            }
            break;
        case NEKO_EXPR_DIV:
            antiderivative = nekoLog(nekoAbs(nekoCloneExpr(inner)));
            break;
        default:
            break;
    }
    return antiderivative ? nekoMul(nekoConst(coeff), antiderivative) : NULL;
}

NekoIntegralResult nekoIntegrateUSubExpr(const NekoExpr* expr, const char* var) {
    if (!expr || !var) return integErr(NEKO_ERR_INVALID_ARG);
    if (expr->kind != NEKO_EXPR_MUL) return integErr(NEKO_ERR_UNSUPPORTED);

    NekoExpr* out = integrateUSubPair(expr->as.binary.lhs, expr->as.binary.rhs, var);
    if (!out) out = integrateUSubPair(expr->as.binary.rhs, expr->as.binary.lhs, var);
    return out ? integOk(nekoSimplify(out)) : integErr(NEKO_ERR_UNSUPPORTED);
}

bool nekoCanIntegrateUSub(const NekoExpr* expr, const char* var) {
    NekoIntegralResult r = nekoIntegrateUSubExpr(expr, var);
    bool ok = r.status == NEKO_OK;
    nekoFreeExpr(r.expr);
    return ok;
}

NekoIntegralResult nekoIntegrateExpr(const NekoExpr* expr, const char* var) {
    if (!expr || !var) return integErr(NEKO_ERR_INVALID_ARG);

    switch (expr->kind) {
        case NEKO_EXPR_CONST:
            return integOk(nekoSimplify(nekoMul(nekoConst(expr->as.constant), nekoVar(var))));

        case NEKO_EXPR_VAR:
            if (!sameVar(expr, var)) return integErr(NEKO_ERR_UNSUPPORTED);
            return integOk(nekoSimplify(nekoDiv(nekoPow(nekoVar(var), nekoConst(2.0)), nekoConst(2.0))));

        case NEKO_EXPR_ADD:
        case NEKO_EXPR_SUB: {
            bool ok = true;
            NekoExpr* il = integOrFree(expr->as.binary.lhs, var, &ok);
            NekoExpr* ir = integOrFree(expr->as.binary.rhs, var, &ok);
            if (!ok) {
                nekoFreeExpr(il);
                nekoFreeExpr(ir);
                return integErr(NEKO_ERR_UNSUPPORTED);
            }
            return integOk(nekoSimplify(expr->kind == NEKO_EXPR_ADD ? nekoAdd(il, ir) : nekoSub(il, ir)));
        }

        case NEKO_EXPR_NEG: {
            NekoIntegralResult r = nekoIntegrateExpr(expr->as.unary.arg, var);
            if (r.status != NEKO_OK) return r;
            return integOk(nekoSimplify(nekoNeg(r.expr)));
        }

        case NEKO_EXPR_MUL: {
            NekoIntegralResult usub = nekoIntegrateUSubExpr(expr, var);
            if (usub.status == NEKO_OK) return usub;
            NekoExpr* inner = NULL;
            double coeff = 0.0;
            if (!splitConstMultiple(expr, &inner, &coeff)) return integErr(NEKO_ERR_UNSUPPORTED);
            NekoIntegralResult r = nekoIntegrateExpr(inner, var);
            nekoFreeExpr(inner);
            if (r.status != NEKO_OK) return r;
            return integOk(nekoSimplify(nekoMul(nekoConst(coeff), r.expr)));
        }

        case NEKO_EXPR_DIV: {
            if (expr->as.binary.lhs->kind == NEKO_EXPR_CONST) {
                NekoExpr* reciprocal = NULL;
                if (expr->as.binary.rhs->kind == NEKO_EXPR_POW
                        && expr->as.binary.rhs->as.binary.rhs->kind == NEKO_EXPR_CONST) {
                    reciprocal = nekoPow(
                        nekoCloneExpr(expr->as.binary.rhs->as.binary.lhs),
                        nekoConst(-expr->as.binary.rhs->as.binary.rhs->as.constant)
                    );
                } else {
                    reciprocal = nekoPow(nekoCloneExpr(expr->as.binary.rhs), nekoConst(-1.0));
                }
                NekoIntegralResult r = nekoIntegrateExpr(reciprocal, var);
                nekoFreeExpr(reciprocal);
                if (r.status != NEKO_OK) return r;
                return integOk(nekoSimplify(nekoMul(nekoConst(expr->as.binary.lhs->as.constant), r.expr)));
            }
            return integErr(NEKO_ERR_UNSUPPORTED);
        }

        case NEKO_EXPR_POW:
            if (expr->as.binary.rhs->kind == NEKO_EXPR_CONST) {
                NekoExpr* trig = tryTrigPowerIntegral(expr, var);
                if (trig) return integOk(nekoSimplify(trig));
                NekoExpr* out = integratePowerOfLinear(expr->as.binary.lhs, expr->as.binary.rhs->as.constant, var);
                return out ? integOk(nekoSimplify(out)) : integErr(NEKO_ERR_UNSUPPORTED);
            }
            return integErr(NEKO_ERR_UNSUPPORTED);

        case NEKO_EXPR_SIN: {
            double a, b;
            if (!linearCoeff(expr->as.unary.arg, var, &a, &b) || fabs(a) <= 1e-12) return integErr(NEKO_ERR_UNSUPPORTED);
            (void)b;
            return integOk(nekoSimplify(nekoDiv(nekoNeg(nekoCos(nekoCloneExpr(expr->as.unary.arg))), nekoConst(a))));
        }

        case NEKO_EXPR_COS: {
            double a, b;
            if (!linearCoeff(expr->as.unary.arg, var, &a, &b) || fabs(a) <= 1e-12) return integErr(NEKO_ERR_UNSUPPORTED);
            (void)b;
            return integOk(nekoSimplify(nekoDiv(nekoSin(nekoCloneExpr(expr->as.unary.arg)), nekoConst(a))));
        }

        case NEKO_EXPR_TAN: {
            double a, b;
            if (!linearCoeff(expr->as.unary.arg, var, &a, &b) || fabs(a) <= 1e-12) return integErr(NEKO_ERR_UNSUPPORTED);
            (void)b;
            return integOk(nekoSimplify(nekoDiv(nekoNeg(nekoLog(nekoCos(nekoCloneExpr(expr->as.unary.arg)))), nekoConst(a))));
        }

        case NEKO_EXPR_EXP: {
            double a, b;
            if (!linearCoeff(expr->as.unary.arg, var, &a, &b) || fabs(a) <= 1e-12) return integErr(NEKO_ERR_UNSUPPORTED);
            (void)b;
            return integOk(nekoSimplify(nekoDiv(nekoExp(nekoCloneExpr(expr->as.unary.arg)), nekoConst(a))));
        }

        case NEKO_EXPR_LOG:
            if (sameVar(expr->as.unary.arg, var)) {
                return integOk(nekoSimplify(nekoSub(nekoMul(nekoVar(var), nekoLog(nekoVar(var))), nekoVar(var))));
            }
            return integErr(NEKO_ERR_UNSUPPORTED);

        case NEKO_EXPR_ASIN:
            if (sameVar(expr->as.unary.arg, var)) {
                NekoExpr* out = nekoAdd(nekoMul(nekoVar(var), nekoAsin(nekoVar(var))),
                                        nekoSqrt(nekoSub(nekoConst(1.0), nekoPow(nekoVar(var), nekoConst(2.0)))));
                return integOk(nekoSimplify(out));
            }
            return integErr(NEKO_ERR_UNSUPPORTED);

        case NEKO_EXPR_ACOS:
            if (sameVar(expr->as.unary.arg, var)) {
                NekoExpr* out = nekoSub(nekoMul(nekoVar(var), nekoAcos(nekoVar(var))),
                                        nekoSqrt(nekoSub(nekoConst(1.0), nekoPow(nekoVar(var), nekoConst(2.0)))));
                return integOk(nekoSimplify(out));
            }
            return integErr(NEKO_ERR_UNSUPPORTED);

        case NEKO_EXPR_ATAN:
            if (sameVar(expr->as.unary.arg, var)) {
                NekoExpr* out = nekoSub(nekoMul(nekoVar(var), nekoAtan(nekoVar(var))),
                                        nekoMul(nekoConst(0.5),
                                                nekoLog(nekoAdd(nekoConst(1.0), nekoPow(nekoVar(var), nekoConst(2.0))))));
                return integOk(nekoSimplify(out));
            }
            return integErr(NEKO_ERR_UNSUPPORTED);

        case NEKO_EXPR_SQRT: {
            NekoExpr* asPower = nekoPow(nekoCloneExpr(expr->as.unary.arg), nekoConst(0.5));
            NekoIntegralResult r = nekoIntegrateExpr(asPower, var);
            nekoFreeExpr(asPower);
            return r;
        }

        case NEKO_EXPR_ABS:
        case NEKO_EXPR_CALL:
            return integErr(NEKO_ERR_UNSUPPORTED);
    }

    return integErr(NEKO_ERR_UNSUPPORTED);
}

/* ---------- Numerical integration and applications ---------- */

static double simpsonRaw(const NekoFunc* func, double a, double b) {
    double c = 0.5 * (a + b);
    return (b - a) * (nekoEvalFunc(func, a) + 4.0 * nekoEvalFunc(func, c) + nekoEvalFunc(func, b)) / 6.0;
}

static double adaptiveSimpsonRecur(const NekoFunc* func, double a, double b,
                                   double eps, double whole, int depth) {
    double c = 0.5 * (a + b);
    double left = simpsonRaw(func, a, c);
    double right = simpsonRaw(func, c, b);
    double delta = left + right - whole;
    if (depth <= 0 || fabs(delta) <= 15.0 * eps) return left + right + delta / 15.0;
    return adaptiveSimpsonRecur(func, a, c, eps * 0.5, left, depth - 1)
         + adaptiveSimpsonRecur(func, c, b, eps * 0.5, right, depth - 1);
}

NekoNumericResult nekoIntegrateNumeric(const NekoFunc* func, double a, double b,
                                       NekoIntegrateMethod method, int intervals, double tol) {
    NekoNumericResult r = { .status = NEKO_OK, .value = NAN, .intervals = intervals };
    if (!func || !isfinite(a) || !isfinite(b)) {
        r.status = NEKO_ERR_INVALID_ARG;
        return r;
    }
    if (a == b) {
        r.value = 0.0;
        r.intervals = 0;
        return r;
    }

    double sign = 1.0;
    if (b < a) {
        double tmp = a;
        a = b;
        b = tmp;
        sign = -1.0;
    }

    if (method == NEKO_INTEGRATE_ADAPTIVE_SIMPSON) {
        double eps = tol > 0.0 ? tol : 1e-8;
        double whole = simpsonRaw(func, a, b);
        r.value = sign * adaptiveSimpsonRecur(func, a, b, eps, whole, 20);
        r.intervals = 0;
        return r;
    }

    if (intervals < 1) intervals = 1024;
    if (method == NEKO_INTEGRATE_SIMPSON && intervals % 2) intervals++;
    r.intervals = intervals;

    double h = (b - a) / intervals;
    double sum = 0.0;
    if (method == NEKO_INTEGRATE_TRAPEZOID) {
        sum = 0.5 * (nekoEvalFunc(func, a) + nekoEvalFunc(func, b));
        for (int i = 1; i < intervals; i++) sum += nekoEvalFunc(func, a + i * h);
        r.value = sign * h * sum;
        return r;
    }

    sum = nekoEvalFunc(func, a) + nekoEvalFunc(func, b);
    for (int i = 1; i < intervals; i++) {
        sum += (i % 2 ? 4.0 : 2.0) * nekoEvalFunc(func, a + i * h);
    }
    r.value = sign * h * sum / 3.0;
    return r;
}

typedef struct {
    const NekoFunc* f;
    const NekoFunc* g;
} AreaBetweenData;

static double areaBetweenEval(double x, void* userdata) {
    AreaBetweenData* data = userdata;
    return fabs(nekoEvalFunc(data->f, x) - nekoEvalFunc(data->g, x));
}

NekoNumericResult nekoAreaBetween(const NekoFunc* f, const NekoFunc* g, double a, double b,
                                  NekoIntegrateMethod method, int intervals, double tol) {
    if (!f || !g) {
        NekoNumericResult r = { .status = NEKO_ERR_INVALID_ARG, .value = NAN, .intervals = intervals };
        return r;
    }
    AreaBetweenData data = { .f = f, .g = g };
    NekoFunc wrapped = { .callback = areaBetweenEval, .userdata = &data };
    return nekoIntegrateNumeric(&wrapped, a, b, method, intervals, tol);
}

static double optEval(const NekoFunc* func, double x, NekoOptGoal goal) {
    double y = nekoEvalFunc(func, x);
    return goal == NEKO_OPT_MINIMIZE ? y : -y;
}

static NekoOptResult goldenSection(const NekoFunc* func, double a, double b,
                                   NekoOptGoal goal, double tol, int maxIter) {
    NekoOptResult r = { .status = NEKO_OK, .x = NAN, .value = NAN, .iterations = 0 };
    if (!func || !isfinite(a) || !isfinite(b) || a > b) {
        r.status = NEKO_ERR_INVALID_ARG;
        return r;
    }
    if (a == b) {
        r.x = a;
        r.value = nekoEvalFunc(func, a);
        return r;
    }
    if (tol <= 0.0) tol = 1e-8;
    if (maxIter < 1) maxIter = 128;

    const double invphi = (sqrt(5.0) - 1.0) / 2.0;
    double c = b - invphi * (b - a);
    double d = a + invphi * (b - a);
    double fc = optEval(func, c, goal);
    double fd = optEval(func, d, goal);

    for (int i = 0; i < maxIter && fabs(b - a) > tol; i++) {
        if (fc < fd) {
            b = d;
            d = c;
            fd = fc;
            c = b - invphi * (b - a);
            fc = optEval(func, c, goal);
        } else {
            a = c;
            c = d;
            fc = fd;
            d = a + invphi * (b - a);
            fd = optEval(func, d, goal);
        }
        r.iterations = i + 1;
    }

    r.x = 0.5 * (a + b);
    r.value = nekoEvalFunc(func, r.x);
    if (!isfinite(r.value)) r.status = NEKO_ERR_DOMAIN;
    return r;
}

NekoOptResult nekoFindMinimum(const NekoFunc* func, double a, double b, double tol, int maxIter) {
    return goldenSection(func, a, b, NEKO_OPT_MINIMIZE, tol, maxIter);
}

NekoOptResult nekoFindMaximum(const NekoFunc* func, double a, double b, double tol, int maxIter) {
    return goldenSection(func, a, b, NEKO_OPT_MAXIMIZE, tol, maxIter);
}

static bool feasibleAt(double x, const NekoConstraint* constraints, int nconstraints) {
    for (int i = 0; i < nconstraints; i++) {
        if (!constraints[i].func) return false;
        double v = nekoEvalFunc(constraints[i].func, x);
        if (!isfinite(v) || v < -1e-10) return false;
    }
    return true;
}

NekoOptResult nekoOptimize(const NekoFunc* objective, double a, double b, NekoOptGoal goal,
                           const NekoConstraint* constraints, int nconstraints,
                           int samples, double tol, int maxIter) {
    NekoOptResult best = { .status = NEKO_ERR_NO_FEASIBLE_POINT, .x = NAN, .value = NAN, .iterations = 0 };
    if (!objective || !isfinite(a) || !isfinite(b) || a > b || nconstraints < 0
            || (nconstraints > 0 && !constraints)) {
        best.status = NEKO_ERR_INVALID_ARG;
        return best;
    }
    if (samples < 8) samples = 256;

    bool haveBest = false;
    double segStart = NAN;
    bool inSeg = false;
    double prevX = a;
    bool prevFeasible = feasibleAt(prevX, constraints, nconstraints);
    if (prevFeasible) {
        segStart = a;
        inSeg = true;
    }

    for (int i = 1; i <= samples; i++) {
        double x = a + (b - a) * (double)i / (double)samples;
        bool feasible = feasibleAt(x, constraints, nconstraints);

        if (feasible && !inSeg) {
            segStart = prevX;
            inSeg = true;
        }
        if ((!feasible || i == samples) && inSeg) {
            double segEnd = feasible ? x : prevX;
            if (segEnd >= segStart) {
                NekoOptResult candidate = goldenSection(objective, segStart, segEnd, goal, tol, maxIter);
                if (candidate.status == NEKO_OK && feasibleAt(candidate.x, constraints, nconstraints)) {
                    if (!haveBest
                            || (goal == NEKO_OPT_MINIMIZE && candidate.value < best.value)
                            || (goal == NEKO_OPT_MAXIMIZE && candidate.value > best.value)) {
                        best = candidate;
                        haveBest = true;
                    }
                }
            }
            inSeg = false;
        }

        prevX = x;
        prevFeasible = feasible;
        (void)prevFeasible;
    }

    if (haveBest) best.status = NEKO_OK;
    return best;
}

/* ---------- ODE solvers ---------- */

static NekoFunc* cloneFunc(const NekoFunc* func) {
    if (!func) return NULL;
    if (func->expr) return nekoFuncFromExpr(func->expr);
    if (func->callback) return nekoFuncFromCallback(func->callback, func->userdata);
    return NULL;
}

static NekoSolveResult solveOk(NekoExpr* expr) {
    NekoSolveResult r = { .status = expr ? NEKO_OK : NEKO_ERR_INVALID_ARG, .expr = expr };
    return r;
}

static NekoSolveResult solveErr(NekoStatus status) {
    NekoSolveResult r = { .status = status, .expr = NULL };
    return r;
}

static double factorialDouble(int n) {
    double out = 1.0;
    for (int i = 2; i <= n; i++) out *= (double)i;
    return out;
}

static NekoExpr* scaleExpr(double coeff, NekoExpr* expr) {
    if (!expr) return NULL;
    if (fabs(coeff) <= 1e-12) {
        nekoFreeExpr(expr);
        return nekoConst(0.0);
    }
    if (fabs(coeff - 1.0) <= 1e-12) return expr;
    if (fabs(coeff + 1.0) <= 1e-12) return nekoNeg(expr);
    return nekoMul(nekoConst(coeff), expr);
}

static NekoExpr* xShiftExpr(double x0) {
    if (fabs(x0) <= 1e-12) return nekoVar("x");
    return nekoSub(nekoVar("x"), nekoConst(x0));
}

static NekoExpr* constantSymbolExpr(int index) {
    char name[16];
    snprintf(name, sizeof(name), "C%d", index);
    return nekoVar(name);
}

static NekoExpr* generalBasisTerm(int degree, int constantIndex) {
    if (degree == 0) return constantSymbolExpr(constantIndex);

    NekoExpr* power = degree == 1
        ? nekoVar("x")
        : nekoPow(nekoVar("x"), nekoConst((double)degree));
    return scaleExpr(1.0 / factorialDouble(degree),
                     nekoMul(constantSymbolExpr(constantIndex), power));
}

static NekoExpr* shiftedBasisTerm(int degree, double x0, double coeff) {
    if (fabs(coeff) <= 1e-12) return nekoConst(0.0);
    if (degree == 0) return nekoConst(coeff);

    NekoExpr* power = xShiftExpr(x0);
    if (degree > 1) power = nekoPow(power, nekoConst((double)degree));
    return scaleExpr(coeff / factorialDouble(degree), power);
}

static bool splitScalarFactorExpr(const NekoExpr* expr, double* coeff, NekoExpr** core) {
    if (!expr || !coeff || !core) return false;

    switch (expr->kind) {
        case NEKO_EXPR_CONST:
            *coeff = expr->as.constant;
            *core = nekoConst(1.0);
            return *core != NULL;
        case NEKO_EXPR_NEG: {
            double innerCoeff = 0.0;
            NekoExpr* innerCore = NULL;
            if (!splitScalarFactorExpr(expr->as.unary.arg, &innerCoeff, &innerCore)) return false;
            *coeff = -innerCoeff;
            *core = innerCore;
            return true;
        }
        case NEKO_EXPR_MUL: {
            double leftCoeff = 0.0, rightCoeff = 0.0;
            NekoExpr* leftCore = NULL;
            NekoExpr* rightCore = NULL;
            if (!splitScalarFactorExpr(expr->as.binary.lhs, &leftCoeff, &leftCore)
                    || !splitScalarFactorExpr(expr->as.binary.rhs, &rightCoeff, &rightCore)) {
                nekoFreeExpr(leftCore);
                nekoFreeExpr(rightCore);
                return false;
            }
            *coeff = leftCoeff * rightCoeff;
            *core = nekoSimplify(nekoMul(leftCore, rightCore));
            return *core != NULL;
        }
        case NEKO_EXPR_DIV:
            if (expr->as.binary.rhs->kind == NEKO_EXPR_CONST) {
                double numCoeff = 0.0;
                NekoExpr* numCore = NULL;
                if (!splitScalarFactorExpr(expr->as.binary.lhs, &numCoeff, &numCore)) return false;
                *coeff = numCoeff / expr->as.binary.rhs->as.constant;
                *core = numCore;
                return true;
            }
            break;
        default:
            break;
    }

    *coeff = 1.0;
    *core = nekoCloneExpr(expr);
    return *core != NULL;
}

static NekoExpr* integrateRepeatedlyExpr(const NekoExpr* expr, int times, const char* var) {
    NekoExpr* current = nekoCloneExpr(expr);
    if (!current) return NULL;

    for (int i = 0; i < times; i++) {
        double coeff = 1.0;
        NekoExpr* core = NULL;
        if (!splitScalarFactorExpr(current, &coeff, &core)) {
            nekoFreeExpr(current);
            return NULL;
        }
        NekoIntegralResult r = nekoIntegrateExpr(core, var);
        nekoFreeExpr(core);
        nekoFreeExpr(current);
        if (r.status != NEKO_OK) {
            nekoFreeExpr(r.expr);
            return NULL;
        }
        current = nekoSimplify(scaleExpr(coeff, r.expr));
    }
    return nekoSimplify(current);
}

static NekoExpr* differentiateRepeatedlyExpr(const NekoExpr* expr, int times, const char* var) {
    NekoExpr* current = nekoCloneExpr(expr);
    if (!current) return NULL;

    for (int i = 0; i < times; i++) {
        NekoDiffResult r = nekoDifferentiateExpr(current, var);
        nekoFreeExpr(current);
        if (r.status != NEKO_OK) {
            nekoFreeExpr(r.expr);
            return NULL;
        }
        current = r.expr;
    }
    return nekoSimplify(current);
}

static NekoExpr* buildSecondOrderParticular(double p, double q, double r, NekoExpr* arg) {
    if (fabs(r) <= 1e-12) {
        nekoFreeExpr(arg);
        return nekoConst(0.0);
    }
    if (fabs(q) > 1e-12) {
        nekoFreeExpr(arg);
        return nekoConst(r / q);
    }
    if (fabs(p) > 1e-12) return scaleExpr(r / p, arg);
    return scaleExpr(0.5 * r, nekoPow(arg, nekoConst(2.0)));
}

static double secondOrderParticularAtZero(double p, double q, double r) {
    (void)p;
    if (fabs(r) <= 1e-12) return 0.0;
    if (fabs(q) > 1e-12) return r / q;
    return 0.0;
}

static double secondOrderParticularDerivAtZero(double p, double q, double r) {
    if (fabs(r) <= 1e-12 || fabs(q) > 1e-12) return 0.0;
    if (fabs(p) > 1e-12) return r / p;
    return 0.0;
}

static NekoExpr* solveFirstOrderGeneralExpr(const NekoOde* ode) {
    double k = ode->as.firstOrder.b / ode->as.firstOrder.a;
    return nekoSimplify(nekoMul(constantSymbolExpr(1),
                                nekoExp(scaleExpr(k, nekoVar("x")))));
}

static NekoExpr* solveFirstOrderInitialExpr(const NekoOde* ode) {
    double k = ode->as.firstOrder.b / ode->as.firstOrder.a;
    return nekoSimplify(scaleExpr(ode->as.firstOrder.y0,
                                  nekoExp(scaleExpr(k, xShiftExpr(ode->as.firstOrder.x0)))));
}

static NekoExpr* solveSecondOrderGeneralExpr(const NekoOde* ode) {
    double a = ode->as.secondOrder.a;
    double b = ode->as.secondOrder.b;
    double c = ode->as.secondOrder.c;
    double d = ode->as.secondOrder.d;
    double p = b / a;
    double q = c / a;
    double r = d / a;
    double disc = b * b - 4.0 * a * c;
    NekoExpr* hom = NULL;

    if (disc > 1e-12) {
        double s = sqrt(disc);
        double r1 = (-b + s) / (2.0 * a);
        double r2 = (-b - s) / (2.0 * a);
        NekoExpr* x = nekoVar("x");
        NekoExpr* t1 = nekoMul(constantSymbolExpr(1), nekoExp(scaleExpr(r1, nekoCloneExpr(x))));
        NekoExpr* t2 = nekoMul(constantSymbolExpr(2), nekoExp(scaleExpr(r2, x)));
        hom = nekoAdd(t1, t2);
    } else if (disc < -1e-12) {
        double alpha = -b / (2.0 * a);
        double beta = sqrt(-disc) / (2.0 * fabs(a));
        NekoExpr* x = nekoVar("x");
        NekoExpr* cosTerm = nekoMul(constantSymbolExpr(1),
                                    nekoCos(scaleExpr(beta, nekoCloneExpr(x))));
        NekoExpr* sinTerm = nekoMul(constantSymbolExpr(2),
                                    nekoSin(scaleExpr(beta, x)));
        NekoExpr* inner = nekoAdd(cosTerm, sinTerm);
        hom = nekoMul(nekoExp(scaleExpr(alpha, nekoVar("x"))), inner);
    } else {
        double root = -b / (2.0 * a);
        NekoExpr* x = nekoVar("x");
        NekoExpr* inner = nekoAdd(constantSymbolExpr(1),
                                  nekoMul(constantSymbolExpr(2), nekoCloneExpr(x)));
        hom = nekoMul(inner, nekoExp(scaleExpr(root, x)));
    }

    return nekoSimplify(nekoAdd(hom, buildSecondOrderParticular(p, q, r, nekoVar("x"))));
}

static NekoExpr* solveSecondOrderInitialExpr(const NekoOde* ode) {
    double a = ode->as.secondOrder.a;
    double b = ode->as.secondOrder.b;
    double c = ode->as.secondOrder.c;
    double d = ode->as.secondOrder.d;
    double y0 = ode->as.secondOrder.y0;
    double dy0 = ode->as.secondOrder.dy0;
    double x0 = ode->as.secondOrder.x0;
    double p = b / a;
    double q = c / a;
    double r = d / a;
    double disc = b * b - 4.0 * a * c;
    double yShift = y0 - secondOrderParticularAtZero(p, q, r);
    double dyShift = dy0 - secondOrderParticularDerivAtZero(p, q, r);
    NekoExpr* hom = NULL;

    if (disc > 1e-12) {
        double s = sqrt(disc);
        double r1 = (-b + s) / (2.0 * a);
        double r2 = (-b - s) / (2.0 * a);
        double A = (dyShift - r2 * yShift) / (r1 - r2);
        double B = yShift - A;
        NekoExpr* t = xShiftExpr(x0);
        NekoExpr* left = scaleExpr(A, nekoExp(scaleExpr(r1, nekoCloneExpr(t))));
        NekoExpr* right = scaleExpr(B, nekoExp(scaleExpr(r2, t)));
        hom = nekoAdd(left, right);
    } else if (disc < -1e-12) {
        double alpha = -b / (2.0 * a);
        double beta = sqrt(-disc) / (2.0 * fabs(a));
        double A = yShift;
        double B = (dyShift - alpha * yShift) / beta;
        NekoExpr* t = xShiftExpr(x0);
        NekoExpr* left = scaleExpr(A, nekoCos(scaleExpr(beta, nekoCloneExpr(t))));
        NekoExpr* right = scaleExpr(B, nekoSin(scaleExpr(beta, t)));
        NekoExpr* inner = nekoAdd(left, right);
        hom = nekoMul(nekoExp(scaleExpr(alpha, xShiftExpr(x0))), inner);
    } else {
        double root = -b / (2.0 * a);
        double A = yShift;
        double B = dyShift - root * yShift;
        NekoExpr* t = xShiftExpr(x0);
        NekoExpr* inner = nekoAdd(nekoConst(A), scaleExpr(B, nekoCloneExpr(t)));
        hom = nekoMul(inner, nekoExp(scaleExpr(root, t)));
    }

    return nekoSimplify(nekoAdd(hom, buildSecondOrderParticular(p, q, r, xShiftExpr(x0))));
}

static NekoExpr* solveNthOrderGeneralExpr(const NekoOde* ode) {
    NekoExpr* scaledRhs = scaleExpr(1.0 / ode->as.nthOrder.a, nekoCloneExpr(ode->as.nthOrder.rhs));
    NekoExpr* solution = integrateRepeatedlyExpr(scaledRhs, ode->as.nthOrder.order, "x");
    nekoFreeExpr(scaledRhs);
    if (!solution) return NULL;

    for (int i = 0; i < ode->as.nthOrder.order; i++) {
        solution = nekoSimplify(nekoAdd(solution, generalBasisTerm(i, i + 1)));
    }
    return solution;
}

static NekoExpr* solveNthOrderInitialExpr(const NekoOde* ode) {
    if (!ode->as.nthOrder.initialValues) return NULL;

    NekoExpr* scaledRhs = scaleExpr(1.0 / ode->as.nthOrder.a, nekoCloneExpr(ode->as.nthOrder.rhs));
    NekoExpr* solution = integrateRepeatedlyExpr(scaledRhs, ode->as.nthOrder.order, "x");
    nekoFreeExpr(scaledRhs);
    if (!solution) return NULL;

    for (int i = 0; i < ode->as.nthOrder.order; i++) {
        NekoExpr* deriv = differentiateRepeatedlyExpr(solution, i, "x");
        double solvedValue = deriv ? nekoEvalExpr(deriv, "x", ode->as.nthOrder.x0) : NAN;
        double correction = ode->as.nthOrder.initialValues[i] - solvedValue;
        nekoFreeExpr(deriv);
        if (!isfinite(correction)) {
            nekoFreeExpr(solution);
            return NULL;
        }
        solution = nekoSimplify(nekoAdd(solution,
                                        shiftedBasisTerm(i, ode->as.nthOrder.x0, correction)));
    }
    return solution;
}

NekoOde* nekoOdeBernoulli(const NekoFunc* P, const NekoFunc* Q, double n, double x0, double y0) {
    if (!P || !Q || !isfinite(n) || !isfinite(x0) || !isfinite(y0)) return NULL;

    NekoOde* ode = calloc(1, sizeof(NekoOde));
    if (!ode) return NULL;
    ode->kind = NEKO_ODE_BERNOULLI;
    ode->as.bernoulli.P = cloneFunc(P);
    ode->as.bernoulli.Q = cloneFunc(Q);
    ode->as.bernoulli.n = n;
    ode->as.bernoulli.x0 = x0;
    ode->as.bernoulli.y0 = y0;
    if (!ode->as.bernoulli.P || !ode->as.bernoulli.Q) {
        nekoFreeOde(ode);
        return NULL;
    }
    return ode;
}

NekoOde* nekoOdeFirstOrderLinearConst(double a, double b, double x0, double y0) {
    if (!isfinite(a) || !isfinite(b) || !isfinite(x0) || !isfinite(y0) || fabs(a) <= 1e-12) {
        return NULL;
    }

    NekoOde* ode = calloc(1, sizeof(NekoOde));
    if (!ode) return NULL;
    ode->kind = NEKO_ODE_FIRST_ORDER_LINEAR_CONST;
    ode->as.firstOrder.a = a;
    ode->as.firstOrder.b = b;
    ode->as.firstOrder.x0 = x0;
    ode->as.firstOrder.y0 = y0;
    return ode;
}

NekoOde* nekoOdeSecondOrderConst(double a, double b, double c, double x0, double y0, double dy0) {
    return nekoOdeSecondOrderConstForced(a, b, c, 0.0, x0, y0, dy0);
}

NekoOde* nekoOdeSecondOrderConstForced(double a, double b, double c, double d, double x0, double y0, double dy0) {
    if (!isfinite(a) || !isfinite(b) || !isfinite(c) || !isfinite(x0)
            || !isfinite(d) || !isfinite(y0) || !isfinite(dy0) || fabs(a) <= 1e-12) {
        return NULL;
    }

    NekoOde* ode = calloc(1, sizeof(NekoOde));
    if (!ode) return NULL;
    ode->kind = NEKO_ODE_SECOND_ORDER_LINEAR_CONST;
    ode->as.secondOrder.a = a;
    ode->as.secondOrder.b = b;
    ode->as.secondOrder.c = c;
    ode->as.secondOrder.d = d;
    ode->as.secondOrder.x0 = x0;
    ode->as.secondOrder.y0 = y0;
    ode->as.secondOrder.dy0 = dy0;
    return ode;
}

NekoOde* nekoOdeNthOrderIntegrable(int order, double a, const NekoExpr* rhs, double x0, const double* initialValues) {
    if (order < 1 || !rhs || !isfinite(a) || fabs(a) <= 1e-12 || !isfinite(x0)) return NULL;

    NekoOde* ode = calloc(1, sizeof(NekoOde));
    if (!ode) return NULL;
    ode->kind = NEKO_ODE_NTH_ORDER_INTEGRABLE;
    ode->as.nthOrder.order = order;
    ode->as.nthOrder.a = a;
    ode->as.nthOrder.x0 = x0;
    ode->as.nthOrder.rhs = nekoCloneExpr(rhs);
    if (!ode->as.nthOrder.rhs) {
        nekoFreeOde(ode);
        return NULL;
    }

    if (initialValues) {
        ode->as.nthOrder.initialValues = malloc((size_t)order * sizeof(double));
        if (!ode->as.nthOrder.initialValues) {
            nekoFreeOde(ode);
            return NULL;
        }
        for (int i = 0; i < order; i++) {
            if (!isfinite(initialValues[i])) {
                nekoFreeOde(ode);
                return NULL;
            }
            ode->as.nthOrder.initialValues[i] = initialValues[i];
        }
    }
    return ode;
}

NekoOde* nekoOdeLinearSystemConst(const double* A, const double* y0, int dim, double x0) {
    if (!A || !y0 || dim < 1 || !isfinite(x0)) return NULL;

    NekoOde* ode = calloc(1, sizeof(NekoOde));
    if (!ode) return NULL;
    ode->kind = NEKO_ODE_LINEAR_SYSTEM_CONST;
    ode->as.linearSystem.dim = dim;
    ode->as.linearSystem.x0 = x0;
    ode->as.linearSystem.A = malloc((size_t)dim * (size_t)dim * sizeof(double));
    ode->as.linearSystem.y0 = malloc((size_t)dim * sizeof(double));
    if (!ode->as.linearSystem.A || !ode->as.linearSystem.y0) {
        nekoFreeOde(ode);
        return NULL;
    }

    for (int i = 0; i < dim * dim; i++) {
        if (!isfinite(A[i])) {
            nekoFreeOde(ode);
            return NULL;
        }
        ode->as.linearSystem.A[i] = A[i];
    }
    for (int i = 0; i < dim; i++) {
        if (!isfinite(y0[i])) {
            nekoFreeOde(ode);
            return NULL;
        }
        ode->as.linearSystem.y0[i] = y0[i];
    }
    return ode;
}

void nekoFreeOde(NekoOde* ode) {
    if (!ode) return;
    switch (ode->kind) {
        case NEKO_ODE_BERNOULLI:
            nekoFreeFunc(ode->as.bernoulli.P);
            nekoFreeFunc(ode->as.bernoulli.Q);
            break;
        case NEKO_ODE_FIRST_ORDER_LINEAR_CONST:
            break;
        case NEKO_ODE_NTH_ORDER_INTEGRABLE:
            nekoFreeExpr(ode->as.nthOrder.rhs);
            free(ode->as.nthOrder.initialValues);
            break;
        case NEKO_ODE_LINEAR_SYSTEM_CONST:
            free(ode->as.linearSystem.A);
            free(ode->as.linearSystem.y0);
            break;
        case NEKO_ODE_SECOND_ORDER_LINEAR_CONST:
            break;
    }
    free(ode);
}

bool nekoMatchOdePattern(const NekoOde* ode, NekoOdeKind kind) {
    return ode && ode->kind == kind;
}

NekoSolveResult nekoSolveOdeGeneral(const NekoOde* ode) {
    if (!ode) return solveErr(NEKO_ERR_INVALID_ARG);

    switch (ode->kind) {
        case NEKO_ODE_FIRST_ORDER_LINEAR_CONST:
            return solveOk(solveFirstOrderGeneralExpr(ode));
        case NEKO_ODE_SECOND_ORDER_LINEAR_CONST:
            return solveOk(solveSecondOrderGeneralExpr(ode));
        case NEKO_ODE_NTH_ORDER_INTEGRABLE:
            return solveOk(solveNthOrderGeneralExpr(ode));
        case NEKO_ODE_BERNOULLI:
        case NEKO_ODE_LINEAR_SYSTEM_CONST:
            return solveErr(NEKO_ERR_UNSUPPORTED);
    }
    return solveErr(NEKO_ERR_UNSUPPORTED);
}

NekoSolveResult nekoSolveOdeInitialValue(const NekoOde* ode) {
    if (!ode) return solveErr(NEKO_ERR_INVALID_ARG);

    switch (ode->kind) {
        case NEKO_ODE_FIRST_ORDER_LINEAR_CONST:
            return solveOk(solveFirstOrderInitialExpr(ode));
        case NEKO_ODE_SECOND_ORDER_LINEAR_CONST:
            return solveOk(solveSecondOrderInitialExpr(ode));
        case NEKO_ODE_NTH_ORDER_INTEGRABLE:
            return solveOk(solveNthOrderInitialExpr(ode));
        case NEKO_ODE_BERNOULLI:
        case NEKO_ODE_LINEAR_SYSTEM_CONST:
            return solveErr(NEKO_ERR_UNSUPPORTED);
    }
    return solveErr(NEKO_ERR_UNSUPPORTED);
}

static NekoOdeResult odeScalarError(NekoStatus status, double x) {
    NekoOdeResult r = { .status = status, .x = x, .value = NAN, .iterations = 0 };
    return r;
}

static double bernoulliRhs(const NekoOde* ode, double x, double y) {
    double p = nekoEvalFunc(ode->as.bernoulli.P, x);
    double q = nekoEvalFunc(ode->as.bernoulli.Q, x);
    double n = ode->as.bernoulli.n;
    if (!isfinite(p) || !isfinite(q)) return NAN;
    if (y < 0.0 && fabs(n - floor(n)) > 1e-12) return NAN;
    return q * pow(y, n) - p * y;
}

static NekoOdeResult evalBernoulli(const NekoOde* ode, double x, int steps) {
    NekoOdeResult r = { .status = NEKO_OK, .x = x, .value = ode->as.bernoulli.y0, .iterations = 0 };
    if (steps < 1) steps = 1024;
    double t = ode->as.bernoulli.x0;
    double y = ode->as.bernoulli.y0;
    if (x == t) return r;

    double h = (x - t) / (double)steps;
    for (int i = 0; i < steps; i++) {
        double k1 = bernoulliRhs(ode, t, y);
        double k2 = bernoulliRhs(ode, t + 0.5 * h, y + 0.5 * h * k1);
        double k3 = bernoulliRhs(ode, t + 0.5 * h, y + 0.5 * h * k2);
        double k4 = bernoulliRhs(ode, t + h, y + h * k3);
        if (!isfinite(k1) || !isfinite(k2) || !isfinite(k3) || !isfinite(k4)) {
            r.status = NEKO_ERR_DOMAIN;
            r.value = NAN;
            r.iterations = i;
            return r;
        }
        y += h * (k1 + 2.0 * k2 + 2.0 * k3 + k4) / 6.0;
        t += h;
        r.iterations = i + 1;
    }

    r.value = y;
    return r;
}

static NekoOdeResult evalClosedFormOde(const NekoOde* ode, double x) {
    NekoSolveResult solved = nekoSolveOdeInitialValue(ode);
    if (solved.status != NEKO_OK || !solved.expr) {
        nekoFreeExpr(solved.expr);
        return odeScalarError(solved.status, x);
    }

    NekoOdeResult r = { .status = NEKO_OK, .x = x, .value = nekoEvalExpr(solved.expr, "x", x), .iterations = 0 };
    nekoFreeExpr(solved.expr);
    if (!isfinite(r.value)) r.status = NEKO_ERR_DOMAIN;
    return r;
}

static void matVecMul(const double* A, const double* y, double* out, int dim) {
    for (int i = 0; i < dim; i++) {
        double sum = 0.0;
        for (int j = 0; j < dim; j++) sum += A[i * dim + j] * y[j];
        out[i] = sum;
    }
}

NekoOdeResult nekoEvalOde(const NekoOde* ode, double x, int steps) {
    if (!ode || !isfinite(x)) return odeScalarError(NEKO_ERR_INVALID_ARG, x);
    switch (ode->kind) {
        case NEKO_ODE_BERNOULLI:
            return evalBernoulli(ode, x, steps);
        case NEKO_ODE_FIRST_ORDER_LINEAR_CONST:
        case NEKO_ODE_SECOND_ORDER_LINEAR_CONST:
        case NEKO_ODE_NTH_ORDER_INTEGRABLE:
            return evalClosedFormOde(ode, x);
        case NEKO_ODE_LINEAR_SYSTEM_CONST:
            return odeScalarError(NEKO_ERR_INVALID_ARG, x);
    }
    return odeScalarError(NEKO_ERR_UNSUPPORTED, x);
}

NekoOdeSystemResult nekoEvalOdeSystem(const NekoOde* ode, double x, int steps) {
    NekoOdeSystemResult r = { .status = NEKO_OK, .x = x, .values = NULL, .dim = 0, .iterations = 0 };
    if (!ode || ode->kind != NEKO_ODE_LINEAR_SYSTEM_CONST || !isfinite(x)) {
        r.status = NEKO_ERR_INVALID_ARG;
        return r;
    }
    if (steps < 1) steps = 1024;

    int dim = ode->as.linearSystem.dim;
    r.dim = dim;
    r.values = malloc((size_t)dim * sizeof(double));
    double* k1 = malloc((size_t)dim * sizeof(double));
    double* k2 = malloc((size_t)dim * sizeof(double));
    double* k3 = malloc((size_t)dim * sizeof(double));
    double* k4 = malloc((size_t)dim * sizeof(double));
    double* tmp = malloc((size_t)dim * sizeof(double));
    if (!r.values || !k1 || !k2 || !k3 || !k4 || !tmp) {
        free(r.values); free(k1); free(k2); free(k3); free(k4); free(tmp);
        r.values = NULL;
        r.dim = 0;
        r.status = NEKO_ERR_INVALID_ARG;
        return r;
    }

    for (int i = 0; i < dim; i++) r.values[i] = ode->as.linearSystem.y0[i];
    double h = (x - ode->as.linearSystem.x0) / (double)steps;

    for (int s = 0; s < steps; s++) {
        matVecMul(ode->as.linearSystem.A, r.values, k1, dim);
        for (int i = 0; i < dim; i++) tmp[i] = r.values[i] + 0.5 * h * k1[i];
        matVecMul(ode->as.linearSystem.A, tmp, k2, dim);
        for (int i = 0; i < dim; i++) tmp[i] = r.values[i] + 0.5 * h * k2[i];
        matVecMul(ode->as.linearSystem.A, tmp, k3, dim);
        for (int i = 0; i < dim; i++) tmp[i] = r.values[i] + h * k3[i];
        matVecMul(ode->as.linearSystem.A, tmp, k4, dim);

        for (int i = 0; i < dim; i++) {
            r.values[i] += h * (k1[i] + 2.0 * k2[i] + 2.0 * k3[i] + k4[i]) / 6.0;
            if (!isfinite(r.values[i])) {
                r.status = NEKO_ERR_DOMAIN;
                r.iterations = s;
                free(k1); free(k2); free(k3); free(k4); free(tmp);
                return r;
            }
        }
        r.iterations = s + 1;
    }

    free(k1); free(k2); free(k3); free(k4); free(tmp);
    return r;
}

void nekoFreeOdeSystemResult(NekoOdeSystemResult result) {
    free(result.values);
}

/* ---------- Print ---------- */

static const char* unaryName(NekoExprKind kind) {
    switch (kind) {
        case NEKO_EXPR_NEG: return "-";
        case NEKO_EXPR_SIN: return "sin";
        case NEKO_EXPR_COS: return "cos";
        case NEKO_EXPR_TAN: return "tan";
        case NEKO_EXPR_ASIN: return "asin";
        case NEKO_EXPR_ACOS: return "acos";
        case NEKO_EXPR_ATAN: return "atan";
        case NEKO_EXPR_EXP: return "exp";
        case NEKO_EXPR_LOG: return "log";
        case NEKO_EXPR_SQRT: return "sqrt";
        case NEKO_EXPR_ABS: return "abs";
        default: return "?";
    }
}

static int exprPrecedence(const NekoExpr* expr) {
    if (!expr) return 100;
    switch (expr->kind) {
        case NEKO_EXPR_ADD:
        case NEKO_EXPR_SUB:
            return 10;
        case NEKO_EXPR_MUL:
        case NEKO_EXPR_DIV:
            return 20;
        case NEKO_EXPR_NEG:
            return 30;
        case NEKO_EXPR_POW:
            return 40;
        default:
            return 50;
    }
}

typedef struct {
    const NekoExpr* expr;
    int sign;
} PrintTerm;

typedef struct {
    PrintTerm* items;
    size_t len;
    size_t cap;
} PrintTermVec;

static bool printTermVecPush(PrintTermVec* vec, const NekoExpr* expr, int sign) {
    if (!vec || !expr) return false;
    if (vec->len == vec->cap) {
        size_t nextCap = vec->cap ? vec->cap * 2 : 4;
        PrintTerm* next = realloc(vec->items, nextCap * sizeof(PrintTerm));
        if (!next) return false;
        vec->items = next;
        vec->cap = nextCap;
    }
    vec->items[vec->len++] = (PrintTerm){ .expr = expr, .sign = sign };
    return true;
}

static void printTermVecFree(PrintTermVec* vec) {
    if (!vec) return;
    free(vec->items);
    vec->items = NULL;
    vec->len = 0;
    vec->cap = 0;
}

static bool collectPrintTerms(const NekoExpr* expr, int sign, PrintTermVec* vec) {
    if (!expr || !vec) return false;
    switch (expr->kind) {
        case NEKO_EXPR_ADD:
            return collectPrintTerms(expr->as.binary.lhs, sign, vec)
                && collectPrintTerms(expr->as.binary.rhs, sign, vec);
        case NEKO_EXPR_SUB:
            return collectPrintTerms(expr->as.binary.lhs, sign, vec)
                && collectPrintTerms(expr->as.binary.rhs, -sign, vec);
        case NEKO_EXPR_NEG:
            return collectPrintTerms(expr->as.unary.arg, -sign, vec);
        default:
            return printTermVecPush(vec, expr, sign);
    }
}

typedef struct {
    const NekoExpr** items;
    size_t len;
    size_t cap;
} ExprRefVec;

static bool exprRefVecPush(ExprRefVec* vec, const NekoExpr* expr) {
    if (!vec || !expr) return false;
    if (vec->len == vec->cap) {
        size_t nextCap = vec->cap ? vec->cap * 2 : 4;
        const NekoExpr** next = realloc(vec->items, nextCap * sizeof(NekoExpr*));
        if (!next) return false;
        vec->items = next;
        vec->cap = nextCap;
    }
    vec->items[vec->len++] = expr;
    return true;
}

static void exprRefVecFree(ExprRefVec* vec) {
    if (!vec) return;
    free(vec->items);
    vec->items = NULL;
    vec->len = 0;
    vec->cap = 0;
}

static bool collectMulFactors(const NekoExpr* expr, ExprRefVec* vec) {
    if (!expr || !vec) return false;
    if (expr->kind == NEKO_EXPR_MUL) {
        return collectMulFactors(expr->as.binary.lhs, vec)
            && collectMulFactors(expr->as.binary.rhs, vec);
    }
    return exprRefVecPush(vec, expr);
}

static void printExprPrec(const NekoExpr* expr, int parentPrec);

static void printAdditiveExpr(const NekoExpr* expr, int parentPrec) {
    PrintTermVec terms = {0};
    if (!collectPrintTerms(expr, 1, &terms) || terms.len == 0) {
        printTermVecFree(&terms);
        printf("<null>");
        return;
    }

    int needsParens = parentPrec > 10;
    if (needsParens) putchar('(');

    for (size_t i = 0; i < terms.len; i++) {
        if (i == 0) {
            if (terms.items[i].sign < 0) {
                putchar('-');
                printExprPrec(terms.items[i].expr, 30);
            } else {
                printExprPrec(terms.items[i].expr, 10);
            }
        } else {
            printf(terms.items[i].sign < 0 ? " - " : " + ");
            printExprPrec(terms.items[i].expr, 10);
        }
    }

    if (needsParens) putchar(')');
    printTermVecFree(&terms);
}

static void printMultiplicativeExpr(const NekoExpr* expr, int parentPrec) {
    ExprRefVec factors = {0};
    if (!collectMulFactors(expr, &factors) || factors.len == 0) {
        exprRefVecFree(&factors);
        printf("<null>");
        return;
    }

    int needsParens = parentPrec > 20;
    if (needsParens) putchar('(');
    for (size_t i = 0; i < factors.len; i++) {
        if (i) printf(" * ");
        int childPrec = exprPrecedence(factors.items[i]);
        int childNeedsParens = childPrec < 20 || factors.items[i]->kind == NEKO_EXPR_NEG;
        if (childNeedsParens) putchar('(');
        printExprPrec(factors.items[i], childNeedsParens ? 0 : 20);
        if (childNeedsParens) putchar(')');
    }
    if (needsParens) putchar(')');
    exprRefVecFree(&factors);
}

static void printExprPrec(const NekoExpr* expr, int parentPrec) {
    if (!expr) {
        printf("<null>");
        return;
    }

    switch (expr->kind) {
        case NEKO_EXPR_CONST:
            printf("%g", expr->as.constant);
            return;
        case NEKO_EXPR_VAR:
            printf("%s", expr->as.var ? expr->as.var : "?");
            return;
        case NEKO_EXPR_ADD:
        case NEKO_EXPR_SUB:
            printAdditiveExpr(expr, parentPrec);
            return;
        case NEKO_EXPR_MUL:
            printMultiplicativeExpr(expr, parentPrec);
            return;
        case NEKO_EXPR_DIV: {
            int needsParens = parentPrec > 20;
            if (needsParens) putchar('(');
            printExprPrec(expr->as.binary.lhs, 20);
            printf(" / ");
            printExprPrec(expr->as.binary.rhs, 21);
            if (needsParens) putchar(')');
            return;
        }
        case NEKO_EXPR_POW: {
            int needsParens = parentPrec > 40;
            if (needsParens) putchar('(');
            printExprPrec(expr->as.binary.lhs, 40);
            printf(" ^ ");
            printExprPrec(expr->as.binary.rhs, 41);
            if (needsParens) putchar(')');
            return;
        }
        case NEKO_EXPR_NEG:
            if (parentPrec > 30) putchar('(');
            putchar('-');
            printExprPrec(expr->as.unary.arg, 30);
            if (parentPrec > 30) putchar(')');
            return;
        case NEKO_EXPR_SIN:
        case NEKO_EXPR_COS:
        case NEKO_EXPR_TAN:
        case NEKO_EXPR_ASIN:
        case NEKO_EXPR_ACOS:
        case NEKO_EXPR_ATAN:
        case NEKO_EXPR_EXP:
        case NEKO_EXPR_LOG:
        case NEKO_EXPR_SQRT:
        case NEKO_EXPR_ABS:
            printf("%s(", unaryName(expr->kind));
            printExprPrec(expr->as.unary.arg, 0);
            putchar(')');
            return;
        case NEKO_EXPR_CALL:
            printf("%s(", expr->as.call.name ? expr->as.call.name : "?");
            for (int i = 0; i < expr->as.call.nargs; i++) {
                if (i) printf(", ");
                printExprPrec(expr->as.call.args[i], 0);
            }
            putchar(')');
            return;
    }
}

void nekoPrintExpr(const NekoExpr* expr) {
    printExprPrec(expr, 0);
}
