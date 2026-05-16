#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "neko.h"

static int testsRun = 0;
static int testsPassed = 0;

#define CHECK_CLOSE(actual, expected, tol, name) do { \
    testsRun++; \
    long double _a = (actual); \
    long double _e = (expected); \
    if (isfinite(_a) && fabsl(_a - _e) <= (tol)) { \
        testsPassed++; \
        printf("  [PASS] %s\n", name); \
    } else { \
        printf("  [FAIL] %s: got %.12Lg expected %.12Lg (line %d)\n", name, _a, _e, __LINE__); \
    } \
} while (0)

#define CHECK(cond, name) do { \
    testsRun++; \
    if (cond) { \
        testsPassed++; \
        printf("  [PASS] %s\n", name); \
    } else { \
        printf("  [FAIL] %s (line %d)\n", name, __LINE__); \
    } \
} while (0)

static void section(const char* name) {
    printf("\n=== %s ===\n", name);
}

static NekoExpr* diffOrNull(NekoExpr* expr) {
    NekoDiffResult d = nekoDifferentiateExpr(expr, "x");
    CHECK(d.status == NEKO_OK && d.expr != NULL, "differentiate returned expression");
    return d.expr;
}

static NekoExpr* integrateOrNull(NekoExpr* expr) {
    NekoIntegralResult r = nekoIntegrateExpr(expr, "x");
    CHECK(r.status == NEKO_OK && r.expr != NULL, "integrate returned expression");
    return r.expr;
}

static NekoExpr* solveGeneralOrNull(const NekoOde* ode) {
    NekoSolveResult r = nekoSolveOdeGeneral(ode);
    CHECK(r.status == NEKO_OK && r.expr != NULL, "general ODE solve returned expression");
    return r.expr;
}

static NekoExpr* solveInitialOrNull(const NekoOde* ode) {
    NekoSolveResult r = nekoSolveOdeInitialValue(ode);
    CHECK(r.status == NEKO_OK && r.expr != NULL, "initial-value ODE solve returned expression");
    return r.expr;
}

static int exprEqualTest(const NekoExpr* a, const NekoExpr* b) {
    if (!a || !b || a->kind != b->kind) return 0;

    switch (a->kind) {
        case NEKO_EXPR_CONST:
            return fabsl(a->as.constant - b->as.constant) <= 1e-9;
        case NEKO_EXPR_VAR:
            return a->as.var && b->as.var && strcmp(a->as.var, b->as.var) == 0;
        case NEKO_EXPR_ADD:
        case NEKO_EXPR_SUB:
        case NEKO_EXPR_MUL:
        case NEKO_EXPR_DIV:
        case NEKO_EXPR_POW:
            return exprEqualTest(a->as.binary.lhs, b->as.binary.lhs)
                && exprEqualTest(a->as.binary.rhs, b->as.binary.rhs);
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
        case NEKO_EXPR_ERF:
        case NEKO_EXPR_EI:
        case NEKO_EXPR_STEP:
            return exprEqualTest(a->as.unary.arg, b->as.unary.arg);
        case NEKO_EXPR_CALL:
            if (!a->as.call.name || !b->as.call.name
                    || strcmp(a->as.call.name, b->as.call.name) != 0
                    || a->as.call.nargs != b->as.call.nargs) {
                return 0;
            }
            for (int i = 0; i < a->as.call.nargs; i++) {
                if (!exprEqualTest(a->as.call.args[i], b->as.call.args[i])) return 0;
            }
            return 1;
    }

    return 0;
}

static void testPolynomialDerivative(void) {
    section("polynomial differentiation");

    // f(x) = 3x^4 - 2x^2 + 7x - 5
    NekoExpr* f = nekoSub(
        nekoAdd(
            nekoSub(
                nekoMul(nekoConst(3.0), nekoPow(nekoVar("x"), nekoConst(4.0))),
                nekoMul(nekoConst(2.0), nekoPow(nekoVar("x"), nekoConst(2.0)))
            ),
            nekoMul(nekoConst(7.0), nekoVar("x"))
        ),
        nekoConst(5.0)
    );
    NekoExpr* df = diffOrNull(f);

    for (int i = -3; i <= 3; i++) {
        long double x = (long double)i / 2.0;
        long double expected = 12.0 * x * x * x - 4.0 * x + 7.0;
        CHECK_CLOSE(nekoEvalExpr(df, "x", x), expected, 1e-9, "polynomial derivative value");
    }

    nekoFreeExpr(df);
    nekoFreeExpr(f);
}

static void testProductQuotientRules(void) {
    section("product and quotient rules");

    // f(x) = (x^2 + 1) sinl(x)
    NekoExpr* f = nekoMul(
        nekoAdd(nekoPow(nekoVar("x"), nekoConst(2.0)), nekoConst(1.0)),
        nekoSin(nekoVar("x"))
    );
    NekoExpr* df = diffOrNull(f);
    long double x = 0.7;
    long double expected = 2.0 * x * sinl(x) + (x * x + 1.0) * cosl(x);
    CHECK_CLOSE(nekoEvalExpr(df, "x", x), expected, 1e-9, "product rule value");
    nekoFreeExpr(df);
    nekoFreeExpr(f);

    // g(x) = expl(x) / (x^2 + 1)
    NekoExpr* g = nekoDiv(
        nekoExp(nekoVar("x")),
        nekoAdd(nekoPow(nekoVar("x"), nekoConst(2.0)), nekoConst(1.0))
    );
    NekoExpr* dg = diffOrNull(g);
    x = 1.25;
    expected = expl(x) * (x * x + 1.0 - 2.0 * x) / ((x * x + 1.0) * (x * x + 1.0));
    CHECK_CLOSE(nekoEvalExpr(dg, "x", x), expected, 1e-9, "quotient rule value");
    nekoFreeExpr(dg);
    nekoFreeExpr(g);
}

static void testTrigExpLogChainRules(void) {
    section("trig, exp, log, and chain rules");

    NekoExpr* logSin = nekoLog(nekoSin(nekoVar("x")));
    NekoExpr* dLogSin = diffOrNull(logSin);
    long double x = 0.8;
    CHECK_CLOSE(nekoEvalExpr(dLogSin, "x", x), cosl(x) / sinl(x), 1e-9, "d logl(sinl(x))");
    nekoFreeExpr(dLogSin);
    nekoFreeExpr(logSin);

    NekoExpr* expPoly = nekoExp(nekoAdd(nekoPow(nekoVar("x"), nekoConst(2.0)),
                                        nekoMul(nekoConst(3.0), nekoVar("x"))));
    NekoExpr* dExpPoly = diffOrNull(expPoly);
    x = -0.4;
    long double inner = x * x + 3.0 * x;
    CHECK_CLOSE(nekoEvalExpr(dExpPoly, "x", x), expl(inner) * (2.0 * x + 3.0), 1e-9, "d expl(x^2 + 3x)");
    nekoFreeExpr(dExpPoly);
    nekoFreeExpr(expPoly);

    NekoExpr* tanExpr = nekoTan(nekoMul(nekoConst(2.0), nekoVar("x")));
    NekoExpr* dTan = diffOrNull(tanExpr);
    x = 0.2;
    long double c = cosl(2.0 * x);
    CHECK_CLOSE(nekoEvalExpr(dTan, "x", x), 2.0 / (c * c), 1e-9, "d tanl(2x)");
    nekoFreeExpr(dTan);
    nekoFreeExpr(tanExpr);

    NekoExpr* erfExpr = nekoErf(nekoMul(nekoConst(2.0), nekoVar("x")));
    NekoExpr* dErf = diffOrNull(erfExpr);
    x = 0.25;
    CHECK_CLOSE(nekoEvalExpr(erfExpr, "x", x), erfl(2.0L * x), 1e-9, "erf expression value");
    CHECK_CLOSE(nekoEvalExpr(dErf, "x", x), (4.0L / sqrtl(M_PI)) * expl(-4.0L * x * x), 1e-9, "d erf(2x)");
    nekoFreeExpr(dErf);
    nekoFreeExpr(erfExpr);

    NekoExpr* eiExpr = nekoEi(nekoAdd(nekoVar("x"), nekoConst(1.0)));
    NekoExpr* dEi = diffOrNull(eiExpr);
    x = 0.5;
    CHECK_CLOSE(nekoEvalExpr(dEi, "x", x), expl(x + 1.0L) / (x + 1.0L), 1e-9, "d Ei(x+1)");
    nekoFreeExpr(dEi);
    nekoFreeExpr(eiExpr);

    NekoExpr* stepExpr = nekoStep(nekoSub(nekoVar("x"), nekoConst(1.0)));
    CHECK_CLOSE(nekoEvalExpr(stepExpr, "x", 0.5), 0.0, 1e-12, "step expression below threshold");
    CHECK_CLOSE(nekoEvalExpr(stepExpr, "x", 1.5), 1.0, 1e-12, "step expression above threshold");
    nekoFreeExpr(stepExpr);
}

static void testGeneralPowerRule(void) {
    section("general power rule");

    // f(x) = x^x, f'(x) = x^x(logl(x) + 1)
    NekoExpr* f = nekoPow(nekoVar("x"), nekoVar("x"));
    NekoExpr* df = diffOrNull(f);
    long double x = 2.3;
    CHECK_CLOSE(nekoEvalExpr(df, "x", x), powl(x, x) * (logl(x) + 1.0), 1e-9, "d x^x");
    nekoFreeExpr(df);
    nekoFreeExpr(f);
}

static void testMultivariableDifferentiation(void) {
    section("multivariable differentiation");

    NekoExpr* fy = nekoAdd(
        nekoPow(nekoVar("y"), nekoConst(3.0)),
        nekoMul(nekoConst(2.0), nekoVar("y"))
    );
    NekoDiffResult dy = nekoDifferentiateExpr(fy, "y");
    CHECK(dy.status == NEKO_OK && dy.expr != NULL, "differentiate with respect to y");
    CHECK_CLOSE(nekoEvalExpr(dy.expr, "y", 2.0), 14.0, 1e-9, "d/dy (y^3 + 2y)");
    nekoFreeExpr(dy.expr);
    nekoFreeExpr(fy);

    NekoExpr* product = nekoMul(nekoVar("x"), nekoVar("y"));
    NekoDiffResult dx = nekoDifferentiateExpr(product, "x");
    CHECK(dx.status == NEKO_OK && dx.expr != NULL, "mixed derivative first step");
    NekoDiffResult dxy = nekoDifferentiateExpr(dx.expr, "y");
    CHECK(dxy.status == NEKO_OK && dxy.expr != NULL, "mixed derivative second step");
    CHECK_CLOSE(nekoEvalExpr(dxy.expr, "x", 7.0), 1.0, 1e-9, "d/dy d/dx (xy)");
    nekoFreeExpr(dxy.expr);
    nekoFreeExpr(dx.expr);
    nekoFreeExpr(product);

    NekoExpr* lap = nekoAdd(
        nekoPow(nekoVar("x"), nekoConst(2.0)),
        nekoPow(nekoVar("y"), nekoConst(2.0))
    );
    NekoDiffResult dxx1 = nekoDifferentiateExpr(lap, "x");
    CHECK(dxx1.status == NEKO_OK && dxx1.expr != NULL, "laplacian first x derivative");
    NekoDiffResult dxx2 = nekoDifferentiateExpr(dxx1.expr, "x");
    CHECK(dxx2.status == NEKO_OK && dxx2.expr != NULL, "laplacian second x derivative");
    CHECK_CLOSE(nekoEvalExpr(dxx2.expr, "x", 3.0), 2.0, 1e-9, "d^2/dx^2 (x^2 + y^2)");
    nekoFreeExpr(dxx2.expr);
    nekoFreeExpr(dxx1.expr);

    NekoDiffResult dyy1 = nekoDifferentiateExpr(lap, "y");
    CHECK(dyy1.status == NEKO_OK && dyy1.expr != NULL, "laplacian first y derivative");
    NekoDiffResult dyy2 = nekoDifferentiateExpr(dyy1.expr, "y");
    CHECK(dyy2.status == NEKO_OK && dyy2.expr != NULL, "laplacian second y derivative");
    CHECK_CLOSE(nekoEvalExpr(dyy2.expr, "y", -2.0), 2.0, 1e-9, "d^2/dy^2 (x^2 + y^2)");
    nekoFreeExpr(dyy2.expr);
    nekoFreeExpr(dyy1.expr);
    nekoFreeExpr(lap);
}

static void testSymbolicIntegration(void) {
    section("symbolic integration");

    // f(x) = 4x^3 - 3/x^2 + 2/x
    NekoExpr* f = nekoAdd(
        nekoSub(
            nekoMul(nekoConst(4.0), nekoPow(nekoVar("x"), nekoConst(3.0))),
            nekoDiv(nekoConst(3.0), nekoPow(nekoVar("x"), nekoConst(2.0)))
        ),
        nekoDiv(nekoConst(2.0), nekoVar("x"))
    );
    NekoExpr* F = integrateOrNull(f);
    NekoExpr* dF = diffOrNull(F);
    long double x = 2.0;
    CHECK_CLOSE(nekoEvalExpr(dF, "x", x), nekoEvalExpr(f, "x", x), 1e-8, "integral of polynomial and reciprocal powers");
    nekoFreeExpr(dF);
    nekoFreeExpr(F);
    nekoFreeExpr(f);

    NekoExpr* trig = nekoAdd(nekoSin(nekoMul(nekoConst(2.0), nekoVar("x"))),
                             nekoCos(nekoSub(nekoVar("x"), nekoConst(1.0))));
    NekoExpr* Itrig = integrateOrNull(trig);
    NekoExpr* dItrig = diffOrNull(Itrig);
    x = 0.4;
    CHECK_CLOSE(nekoEvalExpr(dItrig, "x", x), nekoEvalExpr(trig, "x", x), 1e-8, "integral of linear trig functions");
    nekoFreeExpr(dItrig);
    nekoFreeExpr(Itrig);
    nekoFreeExpr(trig);

    NekoExpr* ex = nekoExp(nekoAdd(nekoMul(nekoConst(3.0), nekoVar("x")), nekoConst(1.0)));
    NekoExpr* Iex = integrateOrNull(ex);
    NekoExpr* dIex = diffOrNull(Iex);
    x = -0.2;
    CHECK_CLOSE(nekoEvalExpr(dIex, "x", x), nekoEvalExpr(ex, "x", x), 1e-8, "integral of expl(ax+b)");
    nekoFreeExpr(dIex);
    nekoFreeExpr(Iex);
    nekoFreeExpr(ex);

    NekoExpr* invTrig = nekoAdd(nekoAsin(nekoVar("x")), nekoAtan(nekoVar("x")));
    NekoExpr* IinvTrig = integrateOrNull(invTrig);
    NekoExpr* dIinvTrig = diffOrNull(IinvTrig);
    x = 0.3;
    CHECK_CLOSE(nekoEvalExpr(dIinvTrig, "x", x), nekoEvalExpr(invTrig, "x", x), 1e-8, "integral of inverse trig functions");
    nekoFreeExpr(dIinvTrig);
    nekoFreeExpr(IinvTrig);
    nekoFreeExpr(invTrig);

    NekoExpr* sin2 = nekoPow(nekoSin(nekoVar("x")), nekoConst(2.0));
    NekoExpr* Isin2 = integrateOrNull(sin2);
    NekoExpr* dIsin2 = diffOrNull(Isin2);
    x = 0.9;
    CHECK_CLOSE(nekoEvalExpr(dIsin2, "x", x), nekoEvalExpr(sin2, "x", x), 1e-8, "integral of sinl(x)^2");
    nekoFreeExpr(dIsin2);
    nekoFreeExpr(Isin2);
    nekoFreeExpr(sin2);

    NekoExpr* usub = nekoMul(
        nekoMul(nekoConst(2.0), nekoVar("x")),
        nekoCos(nekoPow(nekoVar("x"), nekoConst(2.0)))
    );
    CHECK(nekoCanIntegrateUSub(usub, "x"), "u-sub pattern detected");
    NekoExpr* Iusub = integrateOrNull(usub);
    NekoExpr* dIusub = diffOrNull(Iusub);
    x = 0.6;
    CHECK_CLOSE(nekoEvalExpr(dIusub, "x", x), nekoEvalExpr(usub, "x", x), 1e-8, "u-sub integral derivative");
    nekoFreeExpr(dIusub);
    nekoFreeExpr(Iusub);
    nekoFreeExpr(usub);
}

static void testNumericalApplications(void) {
    section("numerical integration and applications");

    NekoExpr* sq = nekoPow(nekoVar("x"), nekoConst(2.0));
    NekoFunc* fsq = nekoFuncFromExpr(sq);
    NekoNumericResult integral = nekoIntegrateNumeric(fsq, 0.0, 1.0, NEKO_INTEGRATE_SIMPSON, 200, 1e-10);
    CHECK(integral.status == NEKO_OK, "numeric integral status");
    CHECK_CLOSE(integral.value, 1.0 / 3.0, 1e-8, "numeric integral x^2 on [0,1]");

    NekoExpr* bowl = nekoAdd(nekoPow(nekoSub(nekoVar("x"), nekoConst(2.0)), nekoConst(2.0)), nekoConst(1.0));
    NekoFunc* fbowl = nekoFuncFromExpr(bowl);
    NekoOptResult min = nekoFindMinimum(fbowl, -5.0, 5.0, 1e-8, 200);
    CHECK(min.status == NEKO_OK, "minimum status");
    CHECK_CLOSE(min.x, 2.0, 1e-5, "minimum x");
    CHECK_CLOSE(min.value, 1.0, 1e-8, "minimum value");

    NekoExpr* hill = nekoSub(nekoConst(4.0), nekoPow(nekoSub(nekoVar("x"), nekoConst(1.0)), nekoConst(2.0)));
    NekoFunc* fhill = nekoFuncFromExpr(hill);
    NekoOptResult max = nekoFindMaximum(fhill, -2.0, 4.0, 1e-8, 200);
    CHECK(max.status == NEKO_OK, "maximum status");
    CHECK_CLOSE(max.x, 1.0, 1e-5, "maximum x");
    CHECK_CLOSE(max.value, 4.0, 1e-8, "maximum value");

    NekoExpr* line = nekoVar("x");
    NekoFunc* fline = nekoFuncFromExpr(line);
    NekoNumericResult area = nekoAreaBetween(fline, fsq, 0.0, 1.0, NEKO_INTEGRATE_SIMPSON, 200, 1e-10);
    CHECK(area.status == NEKO_OK, "area status");
    CHECK_CLOSE(area.value, 1.0 / 6.0, 1e-8, "area between x and x^2");

    // Minimize (x-2)^2 + 1 subject to x - 1 >= 0 and 3 - x >= 0
    NekoExpr* c1Expr = nekoSub(nekoVar("x"), nekoConst(1.0));
    NekoExpr* c2Expr = nekoSub(nekoConst(3.0), nekoVar("x"));
    NekoFunc* c1 = nekoFuncFromExpr(c1Expr);
    NekoFunc* c2 = nekoFuncFromExpr(c2Expr);
    NekoConstraint constraints[] = {
        { .func = c1 },
        { .func = c2 }
    };
    NekoOptResult constrained = nekoOptimize(fbowl, 0.0, 5.0, NEKO_OPT_MINIMIZE,
                                             constraints, 2, 256, 1e-8, 200);
    CHECK(constrained.status == NEKO_OK, "constrained optimization status");
    CHECK_CLOSE(constrained.x, 2.0, 1e-4, "constrained optimum x");
    CHECK_CLOSE(constrained.value, 1.0, 1e-8, "constrained optimum value");

    nekoFreeFunc(c1);
    nekoFreeFunc(c2);
    nekoFreeFunc(fline);
    nekoFreeFunc(fhill);
    nekoFreeFunc(fbowl);
    nekoFreeFunc(fsq);
    nekoFreeExpr(c2Expr);
    nekoFreeExpr(c1Expr);
    nekoFreeExpr(line);
    nekoFreeExpr(hill);
    nekoFreeExpr(bowl);
    nekoFreeExpr(sq);
}

static void testSimplification(void) {
    section("simplification");

    NekoExpr* orderedPolynomial = nekoSimplify(
        nekoAdd(
            nekoPow(nekoVar("x"), nekoConst(3.0)),
            nekoAdd(
                nekoConst(1.0),
                nekoMul(nekoConst(2.0), nekoVar("x"))
            )
        )
    );
    NekoExpr* expectedOrderedPolynomial = nekoAdd(
        nekoAdd(
            nekoConst(1.0),
            nekoMul(nekoConst(2.0), nekoVar("x"))
        ),
        nekoPow(nekoVar("x"), nekoConst(3.0))
    );
    CHECK(exprEqualTest(orderedPolynomial, expectedOrderedPolynomial), "polynomial terms ordered by degree");
    nekoFreeExpr(orderedPolynomial);
    nekoFreeExpr(expectedOrderedPolynomial);

    NekoExpr* normalizedProduct = nekoSimplify(
        nekoMul(
            nekoConst(0.5),
            nekoMul(nekoVar("C1"), nekoVar("x"))
        )
    );
    NekoExpr* expectedProduct = nekoMul(
        nekoMul(nekoConst(0.5), nekoVar("C1")),
        nekoVar("x")
    );
    CHECK(exprEqualTest(normalizedProduct, expectedProduct), "scalar factor moved to readable coefficient position");
    nekoFreeExpr(normalizedProduct);
    nekoFreeExpr(expectedProduct);

    NekoExpr* normalizedDivision = nekoSimplify(
        nekoMul(
            nekoConst(1.0 / 24.0),
            nekoDiv(nekoPow(nekoVar("x"), nekoConst(5.0)), nekoConst(5.0))
        )
    );
    NekoExpr* expectedDivision = nekoMul(
        nekoConst(1.0 / 120.0),
        nekoPow(nekoVar("x"), nekoConst(5.0))
    );
    CHECK(exprEqualTest(normalizedDivision, expectedDivision), "scalar factor combines through division");
    nekoFreeExpr(normalizedDivision);
    nekoFreeExpr(expectedDivision);

    NekoOde* zeroFifth = nekoOdeNthOrderIntegrable(5, 1.0, nekoConst(0.0), 0.0, NULL);
    CHECK(zeroFifth != NULL, "fifth-order zero ODE constructed");
    NekoExpr* zeroFifthGeneral = solveGeneralOrNull(zeroFifth);
    NekoExpr* expectedGeneral = nekoAdd(
        nekoAdd(
            nekoAdd(
                nekoAdd(
                    nekoVar("C1"),
                    nekoMul(nekoVar("C2"), nekoVar("x"))
                ),
                nekoMul(
                    nekoMul(nekoConst(0.5), nekoVar("C3")),
                    nekoPow(nekoVar("x"), nekoConst(2.0))
                )
            ),
            nekoMul(
                nekoMul(nekoConst(1.0 / 6.0), nekoVar("C4")),
                nekoPow(nekoVar("x"), nekoConst(3.0))
            )
        ),
        nekoMul(
            nekoMul(nekoConst(1.0 / 24.0), nekoVar("C5")),
            nekoPow(nekoVar("x"), nekoConst(4.0))
        )
    );
    CHECK(exprEqualTest(zeroFifthGeneral, expectedGeneral), "general repeated-integration basis terms stay normalized");
    nekoFreeExpr(expectedGeneral);
    nekoFreeExpr(zeroFifthGeneral);
    nekoFreeOde(zeroFifth);

    NekoOde* quarticGeneralOde = nekoOdeNthOrderIntegrable(4, 1.0, nekoVar("x"), 0.0, NULL);
    CHECK(quarticGeneralOde != NULL, "fourth-order polynomial ODE constructed");
    NekoExpr* quarticGeneral = solveGeneralOrNull(quarticGeneralOde);
    NekoExpr* expectedQuarticGeneral = nekoAdd(
        nekoAdd(
            nekoAdd(
                nekoAdd(
                    nekoVar("C1"),
                    nekoMul(nekoVar("C2"), nekoVar("x"))
                ),
                nekoMul(
                    nekoMul(nekoConst(0.5), nekoVar("C3")),
                    nekoPow(nekoVar("x"), nekoConst(2.0))
                )
            ),
            nekoMul(
                nekoMul(nekoConst(1.0 / 6.0), nekoVar("C4")),
                nekoPow(nekoVar("x"), nekoConst(3.0))
            )
        ),
        nekoMul(nekoConst(1.0 / 120.0), nekoPow(nekoVar("x"), nekoConst(5.0)))
    );
    CHECK(exprEqualTest(quarticGeneral, expectedQuarticGeneral), "general integrated polynomial solution ordered by degree");
    nekoFreeExpr(expectedQuarticGeneral);
    nekoFreeExpr(quarticGeneral);
    nekoFreeOde(quarticGeneralOde);
}

static void testOdeSolvers(void) {
    section("ODE solvers");

    // Bernoulli pattern: y' + 0*y = 1*y^2, y(0)=1 => y = 1/(1-x)
    NekoExpr* zeroExpr = nekoConst(0.0);
    NekoExpr* oneExpr = nekoConst(1.0);
    NekoFunc* zero = nekoFuncFromExpr(zeroExpr);
    NekoFunc* one = nekoFuncFromExpr(oneExpr);
    NekoOde* bern = nekoOdeBernoulli(zero, one, 2.0, 0.0, 1.0);
    CHECK(bern != NULL, "Bernoulli ODE constructed");
    CHECK(nekoMatchOdePattern(bern, NEKO_ODE_BERNOULLI), "Bernoulli pattern matched");
    NekoOdeResult bernEval = nekoEvalOde(bern, 0.5, 4096);
    CHECK(bernEval.status == NEKO_OK, "Bernoulli eval status");
    CHECK_CLOSE(bernEval.value, 2.0, 1e-6, "Bernoulli y'=y^2 value");

    // y'' + y = 0, y(0)=0, y'(0)=1 => sinl(x)
    NekoOde* osc = nekoOdeSecondOrderConst(1.0, 0.0, 1.0, 0.0, 0.0, 1.0);
    CHECK(osc != NULL, "second-order ODE constructed");
    CHECK(nekoMatchOdePattern(osc, NEKO_ODE_SECOND_ORDER_LINEAR_CONST), "second-order pattern matched");
    NekoOdeResult oscEval = nekoEvalOde(osc, M_PI / 2.0, 0);
    CHECK(oscEval.status == NEKO_OK, "second-order eval status");
    CHECK_CLOSE(oscEval.value, 1.0, 1e-10, "second-order oscillator value");

    NekoExpr* oscGeneral = solveGeneralOrNull(osc);
    nekoFreeExpr(oscGeneral);

    // 2 y' = 6 y, y(1)=5 => y = 5 expl(3(x-1))
    NekoOde* first = nekoOdeFirstOrderLinearConst(2.0, 6.0, 1.0, 5.0);
    CHECK(first != NULL, "first-order linear ODE constructed");
    CHECK(nekoMatchOdePattern(first, NEKO_ODE_FIRST_ORDER_LINEAR_CONST), "first-order pattern matched");
    NekoExpr* firstSolved = solveInitialOrNull(first);
    CHECK_CLOSE(nekoEvalExpr(firstSolved, "x", 1.5), 5.0 * expl(1.5), 1e-10, "first-order symbolic solution value");
    NekoOdeResult firstEval = nekoEvalOde(first, 1.5, 0);
    CHECK(firstEval.status == NEKO_OK, "first-order eval status");
    CHECK_CLOSE(firstEval.value, 5.0 * expl(1.5), 1e-10, "first-order evaluator value");
    nekoFreeExpr(firstSolved);

    // y'' + y = 1, y(0)=2, y'(0)=0 => y = 1 + cosl(x)
    NekoOde* forced1 = nekoOdeSecondOrderConstForced(1.0, 0.0, 1.0, 1.0, 0.0, 2.0, 0.0);
    CHECK(forced1 != NULL, "forced second-order ODE constructed");
    NekoExpr* forced1Solved = solveInitialOrNull(forced1);
    CHECK_CLOSE(nekoEvalExpr(forced1Solved, "x", M_PI), 0.0, 1e-10, "forced second-order constant particular value");
    NekoOdeResult forced1Eval = nekoEvalOde(forced1, M_PI, 0);
    CHECK(forced1Eval.status == NEKO_OK, "forced second-order eval status");
    CHECK_CLOSE(forced1Eval.value, 0.0, 1e-10, "forced second-order evaluator value");
    nekoFreeExpr(forced1Solved);

    // y'' = 2, y(0)=1, y'(0)=3 => y = 1 + 3x + x^2
    NekoOde* forced2 = nekoOdeSecondOrderConstForced(1.0, 0.0, 0.0, 2.0, 0.0, 1.0, 3.0);
    CHECK(forced2 != NULL, "double-integral ODE constructed");
    NekoExpr* forced2Solved = solveInitialOrNull(forced2);
    CHECK_CLOSE(nekoEvalExpr(forced2Solved, "x", 2.0), 11.0, 1e-10, "double-integral symbolic solution value");
    NekoOdeResult forced2Eval = nekoEvalOde(forced2, 2.0, 0);
    CHECK(forced2Eval.status == NEKO_OK, "double-integral eval status");
    CHECK_CLOSE(forced2Eval.value, 11.0, 1e-10, "double-integral evaluator value");
    nekoFreeExpr(forced2Solved);

    // y' = 3x^2 - 4x + 1, y(0)=2 => y = x^3 - 2x^2 + x + 2
    NekoExpr* polyRhs = nekoAdd(
        nekoSub(
            nekoMul(nekoConst(3.0), nekoPow(nekoVar("x"), nekoConst(2.0))),
            nekoMul(nekoConst(4.0), nekoVar("x"))
        ),
        nekoConst(1.0)
    );
    long double polyInit[] = {2.0};
    NekoOde* poly = nekoOdeNthOrderIntegrable(1, 1.0, polyRhs, 0.0, polyInit);
    CHECK(poly != NULL, "first-order integrable ODE constructed");
    CHECK(nekoMatchOdePattern(poly, NEKO_ODE_NTH_ORDER_INTEGRABLE), "integrable ODE pattern matched");
    NekoExpr* polySolved = solveInitialOrNull(poly);
    CHECK_CLOSE(nekoEvalExpr(polySolved, "x", 3.0), 14.0, 1e-9, "integrated polynomial ODE value");
    NekoOdeResult polyEval = nekoEvalOde(poly, 3.0, 0);
    CHECK(polyEval.status == NEKO_OK, "integrated polynomial eval status");
    CHECK_CLOSE(polyEval.value, 14.0, 1e-9, "integrated polynomial evaluator value");
    nekoFreeExpr(polySolved);

    // y'''' = x should integrate four times and differentiate back to x
    long double quarticInit[] = {1.0, 2.0, 3.0, 4.0};
    NekoOde* quartic = nekoOdeNthOrderIntegrable(4, 1.0, nekoVar("x"), 0.0, quarticInit);
    CHECK(quartic != NULL, "fourth-order integrable ODE constructed");
    NekoExpr* quarticGeneral = solveGeneralOrNull(quartic);
    NekoExpr* quarticFourthDeriv = quarticGeneral;
    for (int i = 0; i < 4; i++) quarticFourthDeriv = diffOrNull(quarticFourthDeriv);
    CHECK_CLOSE(nekoEvalExpr(quarticFourthDeriv, "x", 2.0), 2.0, 1e-8, "fourth derivative of general repeated-integral solution");
    nekoFreeExpr(quarticFourthDeriv);

    NekoExpr* quarticSolved = solveInitialOrNull(quartic);
    CHECK_CLOSE(nekoEvalExpr(quarticSolved, "x", 2.0), 16.6, 1e-9, "fourth-order repeated integration value");
    NekoOdeResult quarticEval = nekoEvalOde(quartic, 2.0, 0);
    CHECK(quarticEval.status == NEKO_OK, "fourth-order eval status");
    CHECK_CLOSE(quarticEval.value, 16.6, 1e-9, "fourth-order evaluator value");
    nekoFreeExpr(quarticSolved);

    // y' = A y, A = [[0,-1],[1,0]], y(0)=(1,0) => (cos x, sin x)
    long double A[] = {0.0, -1.0, 1.0, 0.0};
    long double y0[] = {1.0, 0.0};
    NekoOde* sys = nekoOdeLinearSystemConst(A, y0, 2, 0.0);
    CHECK(sys != NULL, "linear system ODE constructed");
    CHECK(nekoMatchOdePattern(sys, NEKO_ODE_LINEAR_SYSTEM_CONST), "linear system pattern matched");
    NekoOdeSystemResult sysEval = nekoEvalOdeSystem(sys, M_PI / 2.0, 4096);
    CHECK(sysEval.status == NEKO_OK, "linear system eval status");
    CHECK(sysEval.dim == 2, "linear system dimension");
    CHECK_CLOSE(sysEval.values[0], 0.0, 1e-6, "linear system x component");
    CHECK_CLOSE(sysEval.values[1], 1.0, 1e-6, "linear system y component");

    nekoFreeOdeSystemResult(sysEval);
    nekoFreeOde(sys);
    nekoFreeOde(quartic);
    nekoFreeOde(poly);
    nekoFreeOde(forced2);
    nekoFreeOde(forced1);
    nekoFreeOde(first);
    nekoFreeOde(osc);
    nekoFreeOde(bern);
    nekoFreeExpr(polyRhs);
    nekoFreeFunc(one);
    nekoFreeFunc(zero);
    nekoFreeExpr(oneExpr);
    nekoFreeExpr(zeroExpr);
}

static void testGraphingSupport(void) {
    section("graphing support");

    // Two-variable evaluation substitutes x and y independently
    NekoExpr* implicit = nekoSub(
        nekoAdd(nekoPow(nekoVar("x"), nekoConst(2.0L)),
                nekoPow(nekoVar("y"), nekoConst(2.0L))),
        nekoConst(1.0L)
    );
    CHECK_CLOSE(nekoEvalExpr2D(implicit, "x", 3.0L, "y", 4.0L), 24.0L, 1e-12, "two-variable graph evaluation");

    // Explicit sampling evaluates y = x^2 on an interval
    NekoExpr* square = nekoPow(nekoVar("x"), nekoConst(2.0L));
    NekoExplicitGraphSample explicitSample = nekoSampleExplicitGraph(square, -1.0L, 1.0L, 3);
    CHECK(explicitSample.status == NEKO_OK && explicitSample.count == 3, "explicit graph sample constructed");
    CHECK_CLOSE(explicitSample.points[0].y, 1.0L, 1e-12, "explicit sample left value");
    CHECK_CLOSE(explicitSample.points[1].y, 0.0L, 1e-12, "explicit sample middle value");
    CHECK_CLOSE(explicitSample.points[2].y, 1.0L, 1e-12, "explicit sample right value");
    nekoFreeExplicitGraphSample(explicitSample);

    // Implicit sampling finds contour segments for the unit circle
    NekoImplicitGraphSample implicitSample = nekoSampleImplicitGraph(implicit, -1.5L, 1.5L, -1.5L, 1.5L, 24, 24);
    CHECK(implicitSample.status == NEKO_OK, "implicit graph sample status");
    CHECK(implicitSample.count > 0, "implicit graph sample has contour segments");
    nekoFreeImplicitGraphSample(implicitSample);

    // Serialization round-trips a graphable expression
    char* serialized = nekoSerializeExpr(implicit);
    NekoExpr* decoded = serialized ? nekoDeserializeExpr(serialized) : NULL;
    CHECK(decoded != NULL && exprEqualTest(implicit, decoded), "graph expression serialization round-trip");
    free(serialized);
    nekoFreeExpr(decoded);

    nekoFreeExpr(square);
    nekoFreeExpr(implicit);
}

int main(void) {
    printf("Running neko tests...\n");
    testPolynomialDerivative();
    testProductQuotientRules();
    testTrigExpLogChainRules();
    testGeneralPowerRule();
    testMultivariableDifferentiation();
    testSymbolicIntegration();
    testNumericalApplications();
    testSimplification();
    testOdeSolvers();
    testGraphingSupport();

    printf("\n========================================\n");
    printf("Results: %d / %d tests passed\n", testsPassed, testsRun);
    printf("========================================\n");
    return testsPassed == testsRun ? 0 : 1;
}
