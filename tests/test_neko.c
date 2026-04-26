#include <stdio.h>
#include <math.h>
#include "neko.h"

static int testsRun = 0;
static int testsPassed = 0;

#define CHECK_CLOSE(actual, expected, tol, name) do { \
    testsRun++; \
    double _a = (actual); \
    double _e = (expected); \
    if (isfinite(_a) && fabs(_a - _e) <= (tol)) { \
        testsPassed++; \
        printf("  [PASS] %s\n", name); \
    } else { \
        printf("  [FAIL] %s: got %.12g expected %.12g (line %d)\n", name, _a, _e, __LINE__); \
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
        double x = (double)i / 2.0;
        double expected = 12.0 * x * x * x - 4.0 * x + 7.0;
        CHECK_CLOSE(nekoEvalExpr(df, "x", x), expected, 1e-9, "polynomial derivative value");
    }

    nekoFreeExpr(df);
    nekoFreeExpr(f);
}

static void testProductQuotientRules(void) {
    section("product and quotient rules");

    // f(x) = (x^2 + 1) sin(x)
    NekoExpr* f = nekoMul(
        nekoAdd(nekoPow(nekoVar("x"), nekoConst(2.0)), nekoConst(1.0)),
        nekoSin(nekoVar("x"))
    );
    NekoExpr* df = diffOrNull(f);
    double x = 0.7;
    double expected = 2.0 * x * sin(x) + (x * x + 1.0) * cos(x);
    CHECK_CLOSE(nekoEvalExpr(df, "x", x), expected, 1e-9, "product rule value");
    nekoFreeExpr(df);
    nekoFreeExpr(f);

    // g(x) = exp(x) / (x^2 + 1)
    NekoExpr* g = nekoDiv(
        nekoExp(nekoVar("x")),
        nekoAdd(nekoPow(nekoVar("x"), nekoConst(2.0)), nekoConst(1.0))
    );
    NekoExpr* dg = diffOrNull(g);
    x = 1.25;
    expected = exp(x) * (x * x + 1.0 - 2.0 * x) / ((x * x + 1.0) * (x * x + 1.0));
    CHECK_CLOSE(nekoEvalExpr(dg, "x", x), expected, 1e-9, "quotient rule value");
    nekoFreeExpr(dg);
    nekoFreeExpr(g);
}

static void testTrigExpLogChainRules(void) {
    section("trig, exp, log, and chain rules");

    NekoExpr* logSin = nekoLog(nekoSin(nekoVar("x")));
    NekoExpr* dLogSin = diffOrNull(logSin);
    double x = 0.8;
    CHECK_CLOSE(nekoEvalExpr(dLogSin, "x", x), cos(x) / sin(x), 1e-9, "d log(sin(x))");
    nekoFreeExpr(dLogSin);
    nekoFreeExpr(logSin);

    NekoExpr* expPoly = nekoExp(nekoAdd(nekoPow(nekoVar("x"), nekoConst(2.0)),
                                        nekoMul(nekoConst(3.0), nekoVar("x"))));
    NekoExpr* dExpPoly = diffOrNull(expPoly);
    x = -0.4;
    double inner = x * x + 3.0 * x;
    CHECK_CLOSE(nekoEvalExpr(dExpPoly, "x", x), exp(inner) * (2.0 * x + 3.0), 1e-9, "d exp(x^2 + 3x)");
    nekoFreeExpr(dExpPoly);
    nekoFreeExpr(expPoly);

    NekoExpr* tanExpr = nekoTan(nekoMul(nekoConst(2.0), nekoVar("x")));
    NekoExpr* dTan = diffOrNull(tanExpr);
    x = 0.2;
    double c = cos(2.0 * x);
    CHECK_CLOSE(nekoEvalExpr(dTan, "x", x), 2.0 / (c * c), 1e-9, "d tan(2x)");
    nekoFreeExpr(dTan);
    nekoFreeExpr(tanExpr);
}

static void testGeneralPowerRule(void) {
    section("general power rule");

    // f(x) = x^x, f'(x) = x^x(log(x) + 1)
    NekoExpr* f = nekoPow(nekoVar("x"), nekoVar("x"));
    NekoExpr* df = diffOrNull(f);
    double x = 2.3;
    CHECK_CLOSE(nekoEvalExpr(df, "x", x), pow(x, x) * (log(x) + 1.0), 1e-9, "d x^x");
    nekoFreeExpr(df);
    nekoFreeExpr(f);
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
    double x = 2.0;
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
    CHECK_CLOSE(nekoEvalExpr(dIex, "x", x), nekoEvalExpr(ex, "x", x), 1e-8, "integral of exp(ax+b)");
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
    CHECK_CLOSE(nekoEvalExpr(dIsin2, "x", x), nekoEvalExpr(sin2, "x", x), 1e-8, "integral of sin(x)^2");
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

    // Minimize (x-2)^2 + 1 subject to x - 1 >= 0 and 3 - x >= 0.
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

static void testOdeSolvers(void) {
    section("ODE solvers");

    // Bernoulli pattern: y' + 0*y = 1*y^2, y(0)=1 => y = 1/(1-x).
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

    // y'' + y = 0, y(0)=0, y'(0)=1 => sin(x).
    NekoOde* osc = nekoOdeSecondOrderConst(1.0, 0.0, 1.0, 0.0, 0.0, 1.0);
    CHECK(osc != NULL, "second-order ODE constructed");
    CHECK(nekoMatchOdePattern(osc, NEKO_ODE_SECOND_ORDER_LINEAR_CONST), "second-order pattern matched");
    NekoOdeResult oscEval = nekoEvalOde(osc, M_PI / 2.0, 0);
    CHECK(oscEval.status == NEKO_OK, "second-order eval status");
    CHECK_CLOSE(oscEval.value, 1.0, 1e-10, "second-order oscillator value");

    // y' = A y, A = [[0,-1],[1,0]], y(0)=(1,0) => (cos x, sin x).
    double A[] = {0.0, -1.0, 1.0, 0.0};
    double y0[] = {1.0, 0.0};
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
    nekoFreeOde(osc);
    nekoFreeOde(bern);
    nekoFreeFunc(one);
    nekoFreeFunc(zero);
    nekoFreeExpr(oneExpr);
    nekoFreeExpr(zeroExpr);
}

int main(void) {
    printf("Running neko tests...\n");
    testPolynomialDerivative();
    testProductQuotientRules();
    testTrigExpLogChainRules();
    testGeneralPowerRule();
    testSymbolicIntegration();
    testNumericalApplications();
    testOdeSolvers();

    printf("\n========================================\n");
    printf("Results: %d / %d tests passed\n", testsPassed, testsRun);
    printf("========================================\n");
    return testsPassed == testsRun ? 0 : 1;
}
