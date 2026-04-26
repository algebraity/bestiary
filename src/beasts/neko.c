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

    return expr;
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

NekoOde* nekoOdeSecondOrderConst(double a, double b, double c, double x0, double y0, double dy0) {
    if (!isfinite(a) || !isfinite(b) || !isfinite(c) || !isfinite(x0)
            || !isfinite(y0) || !isfinite(dy0) || fabs(a) <= 1e-12) {
        return NULL;
    }

    NekoOde* ode = calloc(1, sizeof(NekoOde));
    if (!ode) return NULL;
    ode->kind = NEKO_ODE_SECOND_ORDER_LINEAR_CONST;
    ode->as.secondOrder.a = a;
    ode->as.secondOrder.b = b;
    ode->as.secondOrder.c = c;
    ode->as.secondOrder.x0 = x0;
    ode->as.secondOrder.y0 = y0;
    ode->as.secondOrder.dy0 = dy0;
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

static NekoOdeResult evalSecondOrderConst(const NekoOde* ode, double x) {
    NekoOdeResult r = { .status = NEKO_OK, .x = x, .value = NAN, .iterations = 0 };
    double a = ode->as.secondOrder.a;
    double b = ode->as.secondOrder.b;
    double c = ode->as.secondOrder.c;
    double y0 = ode->as.secondOrder.y0;
    double v0 = ode->as.secondOrder.dy0;
    double t = x - ode->as.secondOrder.x0;
    double disc = b * b - 4.0 * a * c;

    if (disc > 1e-12) {
        double s = sqrt(disc);
        double r1 = (-b + s) / (2.0 * a);
        double r2 = (-b - s) / (2.0 * a);
        double A = (v0 - r2 * y0) / (r1 - r2);
        double B = y0 - A;
        r.value = A * exp(r1 * t) + B * exp(r2 * t);
    } else if (disc < -1e-12) {
        double alpha = -b / (2.0 * a);
        double beta = sqrt(-disc) / (2.0 * fabs(a));
        double A = y0;
        double B = (v0 - alpha * y0) / beta;
        r.value = exp(alpha * t) * (A * cos(beta * t) + B * sin(beta * t));
    } else {
        double root = -b / (2.0 * a);
        double A = y0;
        double B = v0 - root * y0;
        r.value = (A + B * t) * exp(root * t);
    }

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
        case NEKO_ODE_SECOND_ORDER_LINEAR_CONST:
            return evalSecondOrderConst(ode, x);
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

static const char* binaryName(NekoExprKind kind) {
    switch (kind) {
        case NEKO_EXPR_ADD: return "+";
        case NEKO_EXPR_SUB: return "-";
        case NEKO_EXPR_MUL: return "*";
        case NEKO_EXPR_DIV: return "/";
        case NEKO_EXPR_POW: return "^";
        default: return "?";
    }
}

void nekoPrintExpr(const NekoExpr* expr) {
    if (!expr) {
        printf("<null>");
        return;
    }

    switch (expr->kind) {
        case NEKO_EXPR_CONST:
            printf("%g", expr->as.constant);
            break;
        case NEKO_EXPR_VAR:
            printf("%s", expr->as.var ? expr->as.var : "?");
            break;
        case NEKO_EXPR_ADD:
        case NEKO_EXPR_SUB:
        case NEKO_EXPR_MUL:
        case NEKO_EXPR_DIV:
        case NEKO_EXPR_POW:
            putchar('(');
            nekoPrintExpr(expr->as.binary.lhs);
            printf(" %s ", binaryName(expr->kind));
            nekoPrintExpr(expr->as.binary.rhs);
            putchar(')');
            break;
        case NEKO_EXPR_NEG:
            printf("(-");
            nekoPrintExpr(expr->as.unary.arg);
            putchar(')');
            break;
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
            nekoPrintExpr(expr->as.unary.arg);
            putchar(')');
            break;
        case NEKO_EXPR_CALL:
            printf("%s(", expr->as.call.name ? expr->as.call.name : "?");
            for (int i = 0; i < expr->as.call.nargs; i++) {
                if (i) printf(", ");
                nekoPrintExpr(expr->as.call.args[i]);
            }
            putchar(')');
            break;
    }
}
