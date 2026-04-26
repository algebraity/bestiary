#ifndef NEKO_H
#define NEKO_H

#include <stdbool.h>

/* ---------- Definitions of structs ---------- */

typedef struct NekoExpr NekoExpr;
typedef struct NekoOde NekoOde;

typedef enum {
    NEKO_EXPR_CONST,
    NEKO_EXPR_VAR,

    NEKO_EXPR_ADD,
    NEKO_EXPR_SUB,
    NEKO_EXPR_MUL,
    NEKO_EXPR_DIV,
    NEKO_EXPR_POW,

    NEKO_EXPR_NEG,

    NEKO_EXPR_SIN,
    NEKO_EXPR_COS,
    NEKO_EXPR_TAN,
    NEKO_EXPR_ASIN,
    NEKO_EXPR_ACOS,
    NEKO_EXPR_ATAN,

    NEKO_EXPR_EXP,
    NEKO_EXPR_LOG,
    NEKO_EXPR_SQRT,
    NEKO_EXPR_ABS,

    NEKO_EXPR_CALL
} NekoExprKind;

typedef enum {
    NEKO_OK = 0,
    NEKO_ERR_INVALID_ARG,
    NEKO_ERR_UNSUPPORTED,
    NEKO_ERR_DOMAIN,
    NEKO_ERR_NO_CONVERGENCE,
    NEKO_ERR_NO_FEASIBLE_POINT
} NekoStatus;

typedef enum {
    NEKO_INTEGRATE_TRAPEZOID,
    NEKO_INTEGRATE_SIMPSON,
    NEKO_INTEGRATE_ADAPTIVE_SIMPSON
} NekoIntegrateMethod;

typedef enum {
    NEKO_OPT_MINIMIZE,
    NEKO_OPT_MAXIMIZE
} NekoOptGoal;

typedef enum {
    NEKO_ODE_BERNOULLI,
    NEKO_ODE_SECOND_ORDER_LINEAR_CONST,
    NEKO_ODE_LINEAR_SYSTEM_CONST
} NekoOdeKind;

struct NekoExpr {
    NekoExprKind kind;
    union {
        double constant;
        char* var;
        struct {
            NekoExpr* lhs;
            NekoExpr* rhs;
        } binary;
        struct {
            NekoExpr* arg;
        } unary;
        struct {
            char* name;
            NekoExpr** args;
            int nargs;
        } call;
    } as;
};

typedef double (*NekoEvalFn)(double x, void* userdata);

typedef struct NekoFunc {
    NekoExpr* expr;
    NekoEvalFn callback;
    void* userdata;
} NekoFunc;

typedef struct NekoConstraint {
    NekoFunc* func;
} NekoConstraint;

typedef struct NekoDiffResult {
    NekoStatus status;
    NekoExpr* expr;
} NekoDiffResult;

typedef struct NekoIntegralResult {
    NekoStatus status;
    NekoExpr* expr;
} NekoIntegralResult;

typedef struct NekoNumericResult {
    NekoStatus status;
    double value;
    int intervals;
} NekoNumericResult;

typedef struct NekoOptResult {
    NekoStatus status;
    double x;
    double value;
    int iterations;
} NekoOptResult;

typedef struct NekoOdeResult {
    NekoStatus status;
    double x;
    double value;
    int iterations;
} NekoOdeResult;

typedef struct NekoOdeSystemResult {
    NekoStatus status;
    double x;
    double* values;
    int dim;
    int iterations;
} NekoOdeSystemResult;

struct NekoOde {
    NekoOdeKind kind;
    union {
        struct {
            NekoFunc* P;
            NekoFunc* Q;
            double n;
            double x0;
            double y0;
        } bernoulli;
        struct {
            double a;
            double b;
            double c;
            double x0;
            double y0;
            double dy0;
        } secondOrder;
        struct {
            int dim;
            double* A;
            double* y0;
            double x0;
        } linearSystem;
    } as;
};

/* ---------- Expression construction and manipulation ---------- */
NekoExpr* nekoConst(double c);
NekoExpr* nekoVar(const char* name);
NekoExpr* nekoUnary(NekoExprKind kind, NekoExpr* arg);
NekoExpr* nekoBinary(NekoExprKind kind, NekoExpr* lhs, NekoExpr* rhs);
NekoExpr* nekoCall(const char* name, NekoExpr** args, int nargs);
NekoExpr* nekoAdd(NekoExpr* lhs, NekoExpr* rhs);
NekoExpr* nekoSub(NekoExpr* lhs, NekoExpr* rhs);
NekoExpr* nekoMul(NekoExpr* lhs, NekoExpr* rhs);
NekoExpr* nekoDiv(NekoExpr* lhs, NekoExpr* rhs);
NekoExpr* nekoPow(NekoExpr* lhs, NekoExpr* rhs);
NekoExpr* nekoNeg(NekoExpr* arg);
NekoExpr* nekoSin(NekoExpr* arg);
NekoExpr* nekoCos(NekoExpr* arg);
NekoExpr* nekoTan(NekoExpr* arg);
NekoExpr* nekoAsin(NekoExpr* arg);
NekoExpr* nekoAcos(NekoExpr* arg);
NekoExpr* nekoAtan(NekoExpr* arg);
NekoExpr* nekoExp(NekoExpr* arg);
NekoExpr* nekoLog(NekoExpr* arg);
NekoExpr* nekoSqrt(NekoExpr* arg);
NekoExpr* nekoAbs(NekoExpr* arg);
NekoExpr* nekoCloneExpr(const NekoExpr* expr);
void nekoFreeExpr(NekoExpr* expr);
void nekoPrintExpr(const NekoExpr* expr);
double nekoEvalExpr(const NekoExpr* expr, const char* var, double x);
NekoExpr* nekoSimplify(NekoExpr* expr);
NekoDiffResult nekoDifferentiateExpr(const NekoExpr* expr, const char* var);
NekoFunc* nekoFuncFromExpr(const NekoExpr* expr);
NekoFunc* nekoFuncFromCallback(NekoEvalFn callback, void* userdata);
void nekoFreeFunc(NekoFunc* func);
double nekoEvalFunc(const NekoFunc* func, double x);

/* ---------- Integration and applications ---------- */
bool nekoCanIntegrateUSub(const NekoExpr* expr, const char* var);
NekoIntegralResult nekoIntegrateUSubExpr(const NekoExpr* expr, const char* var);
NekoIntegralResult nekoIntegrateExpr(const NekoExpr* expr, const char* var);
NekoNumericResult nekoIntegrateNumeric(const NekoFunc* func, double a, double b, NekoIntegrateMethod method, int intervals, double tol);
NekoNumericResult nekoAreaBetween(const NekoFunc* f, const NekoFunc* g, double a, double b, NekoIntegrateMethod method, int intervals, double tol);
NekoOptResult nekoFindMinimum(const NekoFunc* func, double a, double b, double tol, int maxIter);
NekoOptResult nekoFindMaximum(const NekoFunc* func, double a, double b, double tol, int maxIter);
NekoOptResult nekoOptimize(const NekoFunc* objective, double a, double b, NekoOptGoal goal, const NekoConstraint* constraints, int nconstraints, int samples, double tol, int maxIter);

/* ---------- ODE solvers ---------- */
NekoOde* nekoOdeBernoulli(const NekoFunc* P, const NekoFunc* Q, double n, double x0, double y0);
NekoOde* nekoOdeSecondOrderConst(double a, double b, double c, double x0, double y0, double dy0);
NekoOde* nekoOdeLinearSystemConst(const double* A, const double* y0, int dim, double x0);
void nekoFreeOde(NekoOde* ode);
bool nekoMatchOdePattern(const NekoOde* ode, NekoOdeKind kind);
NekoOdeResult nekoEvalOde(const NekoOde* ode, double x, int steps);
NekoOdeSystemResult nekoEvalOdeSystem(const NekoOde* ode, double x, int steps);
void nekoFreeOdeSystemResult(NekoOdeSystemResult result);


#endif
