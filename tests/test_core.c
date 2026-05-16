#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ast.h"
#include "eval.h"
#include "hebi.h"
#include "kuma.h"
#include "lexer.h"
#include "parser.h"
#include "script.h"
#include "value.h"

static int testsRun = 0;
static int testsPassed = 0;

#define CHECK(cond, name) do { \
    testsRun++; \
    if (cond) { testsPassed++; printf("  [PASS] %s\n", name); } \
    else { printf("  [FAIL] %s (line %d)\n", name, __LINE__); } \
} while (0)

#define SECTION(name) printf("\n=== %s ===\n", name)

static void freeTokens(Token* toks, size_t n) {
    if (!toks) return;
    for (size_t i = 0; i < n; i++) free(toks[i].text);
    free(toks);
}

static Value evalLine(EvalContext* ctx, const char* text) {
    char buf[512];
    snprintf(buf, sizeof(buf), "%s", text ? text : "");

    size_t ntok = 0;
    Token* toks = bstLex(buf, &ntok);
    ParseError err = {0};
    AstNode* ast = bstParse(toks, ntok, &err);
    if (!ast) {
        freeTokens(toks, ntok);
        return valError(err.msg ? err.msg : "parse failed");
    }

    Value out = eval(ctx, ast);
    astFree(ast);
    freeTokens(toks, ntok);
    return out;
}

static int envList(EvalContext* ctx, const char* name, Value* out) {
    Value value;
    if (!envGet(ctx->env, name, &value) || value.kind != VAL_LIST) return 0;
    if (out) *out = value;
    return 1;
}

static int valueIsRoot(Value value, long double real, long double imag) {
    if (value.kind == VAL_COMPLEX) {
        return fabsl(value.as.cplx.real - real) <= 1e-8L
            && fabsl(value.as.cplx.imag - imag) <= 1e-8L;
    }
    if (imag != 0.0L || !valIsNumeric(value)) return 0;
    return fabsl(valToDouble(value) - real) <= 1e-8L;
}

static int valueNumericEquals(Value value, long double expected) {
    if (!valIsNumeric(value)) return 0;
    return fabsl(valToDouble(value) - expected) <= 1e-8L;
}

static int listHasRoot(Value list, long double real, long double imag) {
    if (list.kind != VAL_LIST) return 0;
    for (size_t i = 0; i < list.as.list.n; i++) {
        if (valueIsRoot(list.as.list.items[i], real, imag)) return 1;
    }
    return 0;
}

static int printValueToBuffer(Value value, char* buf, size_t bufSize) {
    if (!buf || bufSize == 0) return 0;
    buf[0] = '\0';

    FILE* tmp = tmpfile();
    if (!tmp) return 0;

    fflush(stdout);
    int saved = dup(STDOUT_FILENO);
    if (saved < 0) {
        fclose(tmp);
        return 0;
    }

    if (dup2(fileno(tmp), STDOUT_FILENO) < 0) {
        close(saved);
        fclose(tmp);
        return 0;
    }

    valPrint(value);
    fflush(stdout);

    if (dup2(saved, STDOUT_FILENO) < 0) {
        close(saved);
        fclose(tmp);
        return 0;
    }
    close(saved);

    fseek(tmp, 0, SEEK_SET);
    size_t nread = fread(buf, 1, bufSize - 1, tmp);
    buf[nread] = '\0';
    fclose(tmp);
    return 1;
}

static int evalLineToBuffer(EvalContext* ctx, const char* text, char* buf, size_t bufSize, Value* out) {
    if (!buf || bufSize == 0 || !out) return 0;
    buf[0] = '\0';

    FILE* tmp = tmpfile();
    if (!tmp) return 0;

    fflush(stdout);
    int saved = dup(STDOUT_FILENO);
    if (saved < 0) {
        fclose(tmp);
        return 0;
    }

    if (dup2(fileno(tmp), STDOUT_FILENO) < 0) {
        close(saved);
        fclose(tmp);
        return 0;
    }

    *out = evalLine(ctx, text);
    fflush(stdout);

    if (dup2(saved, STDOUT_FILENO) < 0) {
        close(saved);
        fclose(tmp);
        return 0;
    }
    close(saved);

    fseek(tmp, 0, SEEK_SET);
    size_t nread = fread(buf, 1, bufSize - 1, tmp);
    buf[nread] = '\0';
    fclose(tmp);
    return 1;
}

static void testListConstruction(void) {
    SECTION("list construction");
    EvalContext* ctx = evalCtxNew();

    Value out = evalLine(ctx, "x = \\list{1,2,3}");
    CHECK(out.kind == VAL_NONE, "list assignment returns none");
    valFree(out);

    Value list;
    CHECK(envList(ctx, "x", &list), "list is stored in env");
    CHECK(list.as.list.n == 3, "list has three items");
    CHECK(list.as.list.items[0].kind == VAL_INT && list.as.list.items[0].as.i == 1, "first item is 1");
    CHECK(list.as.list.items[2].kind == VAL_INT && list.as.list.items[2].as.i == 3, "third item is 3");

    out = evalLine(ctx, "\\list{}");
    CHECK(out.kind == VAL_LIST && out.as.list.n == 0, "empty list construction");
    valFree(out);

    out = evalLine(ctx, "\\len{x}");
    CHECK(out.kind == VAL_INT && out.as.i == 3, "len returns list length");
    valFree(out);

    out = evalLine(ctx, "\\len{\\list{}}");
    CHECK(out.kind == VAL_INT && out.as.i == 0, "len handles empty lists");
    valFree(out);

    out = evalLine(ctx, "\\len{1}");
    CHECK(out.kind == VAL_ERROR, "len rejects non-list input");
    valFree(out);

    evalCtxFree(ctx);
}

static void testListIndexing(void) {
    SECTION("list indexing");
    EvalContext* ctx = evalCtxNew();

    Value out = evalLine(ctx, "x = \\list{10,20,30}");
    valFree(out);

    out = evalLine(ctx, "x[0]");
    CHECK(out.kind == VAL_INT && out.as.i == 10, "zero-based index");
    valFree(out);

    out = evalLine(ctx, "x[2]");
    CHECK(out.kind == VAL_INT && out.as.i == 30, "last positive index");
    valFree(out);

    out = evalLine(ctx, "x[-1]");
    CHECK(out.kind == VAL_INT && out.as.i == 30, "negative index");
    valFree(out);

    out = evalLine(ctx, "x[3]");
    CHECK(out.kind == VAL_ERROR, "out-of-range index fails");
    valFree(out);

    evalCtxFree(ctx);
}

static void testMatrixIndexing(void) {
    SECTION("matrix indexing");
    EvalContext* ctx = evalCtxNew();

    Value out = evalLine(ctx, "A = [1,2;3,4]");
    CHECK(out.kind == VAL_NONE, "matrix assignment returns none");
    valFree(out);

    out = evalLine(ctx, "A[0]");
    CHECK(out.kind == VAL_LIST && out.as.list.n == 2, "matrix row indexing returns row list");
    CHECK(out.kind == VAL_LIST && out.as.list.items[0].kind == VAL_DECIMAL
          && fabsl(out.as.list.items[0].as.d - 1.0L) < 1e-12L,
          "matrix row list contains first row");
    valFree(out);

    out = evalLine(ctx, "A[0][1]");
    CHECK(out.kind == VAL_DECIMAL && fabsl(out.as.d - 2.0L) < 1e-12L,
          "matrix double index reads row 0 column 1");
    valFree(out);

    out = evalLine(ctx, "A[1][0]");
    CHECK(out.kind == VAL_DECIMAL && fabsl(out.as.d - 3.0L) < 1e-12L,
          "matrix double index reads row 1 column 0");
    valFree(out);

    out = evalLine(ctx, "A[-1][-1]");
    CHECK(out.kind == VAL_DECIMAL && fabsl(out.as.d - 4.0L) < 1e-12L,
          "matrix double index supports negative indices");
    valFree(out);

    out = evalLine(ctx, "A[2]");
    CHECK(out.kind == VAL_ERROR, "matrix row out-of-range index fails");
    valFree(out);

    out = evalLine(ctx, "A[0][2]");
    CHECK(out.kind == VAL_ERROR, "matrix column out-of-range index fails");
    valFree(out);

    out = evalLine(ctx, "v = [7,8,9]");
    CHECK(out.kind == VAL_NONE, "vector assignment returns none");
    valFree(out);

    out = evalLine(ctx, "v[1]");
    CHECK(out.kind == VAL_DECIMAL && fabsl(out.as.d - 8.0L) < 1e-12L,
          "vector indexing returns element");
    valFree(out);

    out = evalLine(ctx, "C = [1,i;3,4]; C[0][1]");
    CHECK(out.kind == VAL_COMPLEX && fabsl(out.as.cplx.real) < 1e-12L
          && fabsl(out.as.cplx.imag - 1.0L) < 1e-12L,
          "matrix indexing preserves complex entries");
    valFree(out);

    evalCtxFree(ctx);
}

static void testListMutation(void) {
    SECTION("list mutation");
    EvalContext* ctx = evalCtxNew();

    Value out = evalLine(ctx, "x = \\list{1,2,3}");
    valFree(out);

    out = evalLine(ctx, "\\append{x}{4}");
    CHECK(out.kind == VAL_LIST && out.as.list.n == 4, "append returns grown list");
    valFree(out);

    Value list;
    CHECK(envList(ctx, "x", &list), "append keeps list in env");
    CHECK(list.as.list.n == 4, "env list has four items");
    CHECK(list.as.list.items[3].kind == VAL_INT && list.as.list.items[3].as.i == 4, "appended item is stored");

    out = evalLine(ctx, "\\remove{x}{2}");
    CHECK(out.kind == VAL_LIST && out.as.list.n == 3, "remove returns shortened list");
    valFree(out);

    CHECK(envList(ctx, "x", &list), "remove keeps list in env");
    CHECK(list.as.list.n == 3, "env list has three items after remove");
    CHECK(list.as.list.items[1].kind == VAL_INT && list.as.list.items[1].as.i == 3, "remove shifts later items");

    out = evalLine(ctx, "\\remove{x}{99}");
    CHECK(out.kind == VAL_LIST && out.as.list.n == 3, "remove absent item is no-op");
    valFree(out);

    out = evalLine(ctx, "x[1] = \\frac{3}{4}");
    CHECK(out.kind == VAL_NONE, "indexed assignment returns none");
    valFree(out);

    CHECK(envList(ctx, "x", &list), "indexed assignment keeps list in env");
    CHECK(list.as.list.items[1].kind == VAL_FRACTION
          && list.as.list.items[1].as.frac.num == 3
          && list.as.list.items[1].as.frac.denom == 4,
          "indexed assignment replaces existing item");

    out = evalLine(ctx, "x[4] = 10");
    CHECK(out.kind == VAL_ERROR, "indexed assignment rejects missing index");
    valFree(out);

    evalCtxFree(ctx);
}

static void testListSorting(void) {
    SECTION("list sorting");
    EvalContext* ctx = evalCtxNew();

    Value out = evalLine(ctx, "xs = \\list{1,2}; ys = \\copy{xs}; ys[0] = 9; xs");
    CHECK(out.kind == VAL_LIST
          && out.as.list.n == 2
          && out.as.list.items[0].kind == VAL_INT && out.as.list.items[0].as.i == 1
          && out.as.list.items[1].kind == VAL_INT && out.as.list.items[1].as.i == 2,
          "copy creates independent list");
    valFree(out);

    out = evalLine(ctx, "ys");
    CHECK(out.kind == VAL_LIST
          && out.as.list.n == 2
          && out.as.list.items[0].kind == VAL_INT && out.as.list.items[0].as.i == 9,
          "copied list can be mutated independently");
    valFree(out);

    out = evalLine(ctx, "z = \\copy{42}; z");
    CHECK(out.kind == VAL_INT && out.as.i == 42, "copy handles scalar values");
    valFree(out);

    out = evalLine(ctx, "A = [1,2;3,4]; B = \\copy{A}; B");
    CHECK(out.kind == VAL_MATRIX, "copy still handles matrices");
    valFree(out);

    out = evalLine(ctx, "xs = \\list{3, \\frac{1,2}, 2.5, -1}; \\sort{xs}");
    CHECK(out.kind == VAL_LIST && out.as.list.n == 4, "sort returns sorted list");
    CHECK(out.kind == VAL_LIST
          && out.as.list.items[0].kind == VAL_INT && out.as.list.items[0].as.i == -1
          && out.as.list.items[1].kind == VAL_FRACTION && out.as.list.items[1].as.frac.num == 1
          && out.as.list.items[1].as.frac.denom == 2
          && out.as.list.items[2].kind == VAL_DECIMAL && out.as.list.items[2].as.d == 2.5L
          && out.as.list.items[3].kind == VAL_INT && out.as.list.items[3].as.i == 3,
          "sort orders mixed real numeric values");
    valFree(out);

    Value list;
    CHECK(envList(ctx, "xs", &list), "sort keeps list in env");
    CHECK(list.as.list.n == 4
          && list.as.list.items[0].kind == VAL_INT && list.as.list.items[0].as.i == -1
          && list.as.list.items[3].kind == VAL_INT && list.as.list.items[3].as.i == 3,
          "sort mutates named list");

    out = evalLine(ctx, "\\sort{\\list{2,1}}");
    CHECK(out.kind == VAL_LIST
          && out.as.list.n == 2
          && out.as.list.items[0].kind == VAL_INT && out.as.list.items[0].as.i == 1
          && out.as.list.items[1].kind == VAL_INT && out.as.list.items[1].as.i == 2,
          "sort handles inline lists");
    valFree(out);

    out = evalLine(ctx, "xs = \\list{3,1,2}; ys = \\sortedCopy{xs}; xs");
    CHECK(out.kind == VAL_LIST
          && out.as.list.n == 3
          && out.as.list.items[0].kind == VAL_INT && out.as.list.items[0].as.i == 3
          && out.as.list.items[1].kind == VAL_INT && out.as.list.items[1].as.i == 1
          && out.as.list.items[2].kind == VAL_INT && out.as.list.items[2].as.i == 2,
          "sortedCopy does not mutate original list");
    valFree(out);

    out = evalLine(ctx, "ys");
    CHECK(out.kind == VAL_LIST
          && out.as.list.n == 3
          && out.as.list.items[0].kind == VAL_INT && out.as.list.items[0].as.i == 1
          && out.as.list.items[1].kind == VAL_INT && out.as.list.items[1].as.i == 2
          && out.as.list.items[2].kind == VAL_INT && out.as.list.items[2].as.i == 3,
          "sortedCopy returns sorted list");
    valFree(out);

    out = evalLine(ctx, "\\sort{\\list{1,\"bad\"}}");
    CHECK(out.kind == VAL_ERROR, "sort rejects nonnumeric list item");
    valFree(out);

    out = evalLine(ctx, "\\sort{7}");
    CHECK(out.kind == VAL_ERROR, "sort rejects non-list input");
    valFree(out);

    evalCtxFree(ctx);
}

static void testKumaValueOwnership(void) {
    SECTION("KUMA value ownership");

    ProbabilityDistribution* dist = constructBernoulliDistribution(constructNumberFromFraction(constructFraction(1, 3)));
    Value wrappedDist = valPtr(VAL_PROBABILITY_DISTRIBUTION, dist);
    Value copiedDist = valClone(wrappedDist);
    CHECK(copiedDist.kind == VAL_PROBABILITY_DISTRIBUTION
          && copiedDist.as.ptr
          && copiedDist.as.ptr != wrappedDist.as.ptr,
          "probability distribution values clone deeply");
    CHECK(copiedDist.kind == VAL_PROBABILITY_DISTRIBUTION
          && probabilityMean((ProbabilityDistribution*)copiedDist.as.ptr).type == NUMBER_FRACTION,
          "probability distribution clone remains usable");
    valFree(copiedDist);
    valFree(wrappedDist);

    ProbabilityDistribution* rvDist = constructBernoulliDistribution(constructNumberFromFraction(constructFraction(1, 4)));
    RandomVariable* rv = constructRandomVariable("X", rvDist, true);
    Value wrappedRv = valPtr(VAL_RANDOM_VARIABLE, rv);
    Value copiedRv = valClone(wrappedRv);
    CHECK(copiedRv.kind == VAL_RANDOM_VARIABLE
          && copiedRv.as.ptr
          && copiedRv.as.ptr != wrappedRv.as.ptr,
          "random variable values clone deeply");
    CHECK(copiedRv.kind == VAL_RANDOM_VARIABLE
          && rvExpectedValue((RandomVariable*)copiedRv.as.ptr).type == NUMBER_FRACTION,
          "random variable clone remains usable");
    valFree(copiedRv);
    valFree(wrappedRv);
}

static void testKumaCommands(void) {
    SECTION("KUMA commands");
    EvalContext* ctx = evalCtxNew();

    Value out = evalLine(ctx, "\\factorial{5}");
    CHECK(out.kind == VAL_INT && out.as.i == 120, "KUMA factorial command");
    valFree(out);

    out = evalLine(ctx, "\\ncr{5}{2}");
    CHECK(out.kind == VAL_INT && out.as.i == 10, "ncr command");
    valFree(out);

    out = evalLine(ctx, "\\npr{5}{2}");
    CHECK(out.kind == VAL_INT && out.as.i == 20, "npr command");
    valFree(out);

    out = evalLine(ctx, "\\multinomial{5}{\\list{2,3}}");
    CHECK(out.kind == VAL_INT && out.as.i == 10, "multinomial list command");
    valFree(out);

    out = evalLine(ctx, "\\multinomial{5}{2}{2}{1}");
    CHECK(out.kind == VAL_INT && out.as.i == 30, "multinomial variadic command");
    valFree(out);

    out = evalLine(ctx, "\\sum{\\list{1,2,3,4}}");
    CHECK(out.kind == VAL_INT && out.as.i == 10, "sum command");
    valFree(out);

    out = evalLine(ctx, "\\product{\\list{1,2,3,4}}");
    CHECK(out.kind == VAL_INT && out.as.i == 24, "product command");
    valFree(out);

    out = evalLine(ctx, "\\mean{\\list{1,2,3,4}}");
    CHECK(valueNumericEquals(out, 2.5L), "mean command");
    valFree(out);

    out = evalLine(ctx, "\\median{\\list{1,2,3,4}}");
    CHECK(valueNumericEquals(out, 2.5L), "median command");
    valFree(out);

    out = evalLine(ctx, "\\mode{\\list{3,1,2,2,3}}");
    CHECK(out.kind == VAL_INT && out.as.i == 2, "mode returns smallest mode");
    valFree(out);

    out = evalLine(ctx, "\\modes{\\list{3,1,2,2,3}}");
    CHECK(out.kind == VAL_LIST && out.as.list.n == 2
          && out.as.list.items[0].kind == VAL_INT && out.as.list.items[0].as.i == 2
          && out.as.list.items[1].kind == VAL_INT && out.as.list.items[1].as.i == 3,
          "modes returns all modal values");
    valFree(out);

    out = evalLine(ctx, "\\min{\\list{3,1,2}}");
    CHECK(out.kind == VAL_INT && out.as.i == 1, "min command");
    valFree(out);

    out = evalLine(ctx, "\\max{\\list{3,1,2}}");
    CHECK(out.kind == VAL_INT && out.as.i == 3, "max command");
    valFree(out);

    out = evalLine(ctx, "\\range{\\list{3,1,2}}");
    CHECK(out.kind == VAL_INT && out.as.i == 2, "range command");
    valFree(out);

    out = evalLine(ctx, "\\variance{\\list{1,2,3}}");
    CHECK(valueNumericEquals(out, 2.0L / 3.0L), "variance command");
    valFree(out);

    out = evalLine(ctx, "\\sampleVariance{\\list{1,2,3}}");
    CHECK(out.kind == VAL_INT && out.as.i == 1, "sample variance command");
    valFree(out);

    out = evalLine(ctx, "\\stddev{\\list{1,2,3}}");
    CHECK(valueNumericEquals(out, sqrtl(2.0L / 3.0L)), "stddev command");
    valFree(out);

    out = evalLine(ctx, "\\sampleStddev{\\list{1,2,3}}");
    CHECK(valueNumericEquals(out, 1.0L), "sample stddev command");
    valFree(out);

    out = evalLine(ctx, "\\meanAbsDev{\\list{1,2,3}}");
    CHECK(valueNumericEquals(out, 2.0L / 3.0L), "mean absolute deviation command");
    valFree(out);

    out = evalLine(ctx, "\\medianAbsDev{\\list{1,2,3}}");
    CHECK(out.kind == VAL_INT && out.as.i == 1, "median absolute deviation command");
    valFree(out);

    out = evalLine(ctx, "\\percentile{\\list{1,2,3,4}}{50}");
    CHECK(valueNumericEquals(out, 2.5L), "percentile command");
    valFree(out);

    out = evalLine(ctx, "\\quartile{\\list{1,2,3,4}}{2}");
    CHECK(valueNumericEquals(out, 2.5L), "quartile command");
    valFree(out);

    out = evalLine(ctx, "\\iqr{\\list{1,2,3,4}}");
    CHECK(valueNumericEquals(out, 1.5L), "iqr command");
    valFree(out);

    out = evalLine(ctx, "\\geometricMean{\\list{1,4,16}}");
    CHECK(valueNumericEquals(out, 4.0L), "geometric mean command");
    valFree(out);

    out = evalLine(ctx, "\\harmonicMean{\\list{1,2,4}}");
    CHECK(valueNumericEquals(out, 12.0L / 7.0L), "harmonic mean command");
    valFree(out);

    out = evalLine(ctx, "\\frequency{\\list{1,2,2,3}}{2}");
    CHECK(out.kind == VAL_INT && out.as.i == 2, "frequency command");
    valFree(out);

    out = evalLine(ctx, "\\countDistinct{\\list{1,2,2,3}}");
    CHECK(out.kind == VAL_INT && out.as.i == 3, "count distinct command");
    valFree(out);

    out = evalLine(ctx, "\\frequencies{\\list{1,2,2,3}}");
    CHECK(out.kind == VAL_LIST && out.as.list.n == 3
          && out.as.list.items[1].kind == VAL_LIST
          && out.as.list.items[1].as.list.items[0].kind == VAL_INT
          && out.as.list.items[1].as.list.items[0].as.i == 2
          && out.as.list.items[1].as.list.items[1].kind == VAL_INT
          && out.as.list.items[1].as.list.items[1].as.i == 2,
          "frequencies returns value-count pairs");
    valFree(out);

    out = evalLine(ctx, "\\covariance{\\list{1,2,3}}{\\list{2,4,6}}");
    CHECK(valueNumericEquals(out, 4.0L / 3.0L), "covariance command");
    valFree(out);

    out = evalLine(ctx, "\\sampleCovariance{\\list{1,2,3}}{\\list{2,4,6}}");
    CHECK(out.kind == VAL_INT && out.as.i == 2, "sample covariance command");
    valFree(out);

    out = evalLine(ctx, "\\correlation{\\list{1,2,3}}{\\list{2,4,6}}");
    CHECK(valueNumericEquals(out, 1.0L), "correlation command");
    valFree(out);

    out = evalLine(ctx, "\\linearRegressionSlope{\\list{1,2,3}}{\\list{2,4,6}}");
    CHECK(valueNumericEquals(out, 2.0L), "linear regression slope command");
    valFree(out);

    out = evalLine(ctx, "\\linearRegressionIntercept{\\list{1,2,3}}{\\list{2,4,6}}");
    CHECK(valueNumericEquals(out, 0.0L), "linear regression intercept command");
    valFree(out);

    out = evalLine(ctx, "\\linearRegressionPredict{2}{0}{4}");
    CHECK(out.kind == VAL_INT && out.as.i == 8, "linear regression predict command");
    valFree(out);

    out = evalLine(ctx, "\\mean{\\list{1 + i}}");
    CHECK(out.kind == VAL_ERROR, "KUMA stats reject complex data");
    valFree(out);

    out = evalLine(ctx, "\\bernoulli{\\frac{1}{3}}");
    CHECK(out.kind == VAL_PROBABILITY_DISTRIBUTION, "bernoulli command constructs a distribution");
    valFree(out);

    out = evalLine(ctx, "\\binomial{4}{\\frac{1}{2}}");
    CHECK(out.kind == VAL_PROBABILITY_DISTRIBUTION, "binomial command constructs a distribution");
    valFree(out);

    out = evalLine(ctx, "\\geometric{\\frac{1}{2}}");
    CHECK(out.kind == VAL_PROBABILITY_DISTRIBUTION, "geometric command constructs a distribution");
    valFree(out);

    out = evalLine(ctx, "\\poisson{3}");
    CHECK(out.kind == VAL_PROBABILITY_DISTRIBUTION, "poisson command constructs a distribution");
    valFree(out);

    out = evalLine(ctx, "\\discreteUniform{2}{5}");
    CHECK(out.kind == VAL_PROBABILITY_DISTRIBUTION, "discreteUniform command constructs a distribution");
    valFree(out);

    out = evalLine(ctx, "\\continuousUniform{0}{2}");
    CHECK(out.kind == VAL_PROBABILITY_DISTRIBUTION, "continuousUniform command constructs a distribution");
    valFree(out);

    out = evalLine(ctx, "\\normal{0}{1}");
    CHECK(out.kind == VAL_PROBABILITY_DISTRIBUTION, "normal command constructs a distribution");
    valFree(out);

    out = evalLine(ctx, "\\exponential{2}");
    CHECK(out.kind == VAL_PROBABILITY_DISTRIBUTION, "exponential command constructs a distribution");
    valFree(out);

    out = evalLine(ctx, "\\pmf{\\bernoulli{\\frac{1}{3}}}{1}");
    CHECK(valueNumericEquals(out, 1.0L / 3.0L), "pmf evaluates a discrete distribution");
    valFree(out);

    out = evalLine(ctx, "\\cdf{\\bernoulli{\\frac{1}{3}}}{0}");
    CHECK(valueNumericEquals(out, 2.0L / 3.0L), "cdf evaluates a discrete distribution");
    valFree(out);

    out = evalLine(ctx, "\\pdf{\\bernoulli{\\frac{1}{3}}}{1}");
    CHECK(out.kind == VAL_ERROR, "pdf rejects distributions without densities");
    valFree(out);

    out = evalLine(ctx, "\\pdf{\\normal{0}{1}}{0}");
    CHECK(valueNumericEquals(out, 1.0L / sqrtl(2.0L * acosl(-1.0L))), "pdf evaluates a normal distribution");
    valFree(out);

    out = evalLine(ctx, "\\cdf{\\normal{0}{1}}{0}");
    CHECK(valueNumericEquals(out, 0.5L), "cdf evaluates a normal distribution");
    valFree(out);

    out = evalLine(ctx, "\\pdfFunc{\\normal{0}{1}}");
    CHECK(out.kind == VAL_NEKO_EXPR, "pdfFunc returns a graphable NEKO expression for supported distributions");
    valFree(out);

    out = evalLine(ctx, "\\cdfFunc{\\exponential{2}}");
    CHECK(out.kind == VAL_NEKO_EXPR, "cdfFunc returns a graphable NEKO expression for supported distributions");
    valFree(out);

    out = evalLine(ctx, "\\pdfFunc{\\bernoulli{\\frac{1}{3}}}");
    CHECK(out.kind == VAL_ERROR, "pdfFunc rejects unsupported distributions");
    valFree(out);

    out = evalLine(ctx, "\\customDiscrete{\\list{1,2}}{\\list{\\frac{1}{4},\\frac{3}{4}}}");
    CHECK(out.kind == VAL_PROBABILITY_DISTRIBUTION, "customDiscrete constructs a finite discrete distribution");
    valFree(out);

    out = evalLine(ctx, "\\pmf{\\customDiscrete{\\list{1,2}}{\\list{\\frac{1}{4},\\frac{3}{4}}}}{2}");
    CHECK(valueNumericEquals(out, 0.75L), "customDiscrete PMF is available through pmf");
    valFree(out);

    out = evalLine(ctx, "\\pmf{\\affineDistribution{\\bernoulli{\\frac{1}{2}}}{2}{3}}{5}");
    CHECK(valueNumericEquals(out, 0.5L), "affineDistribution transforms finite discrete support");
    valFree(out);

    out = evalLine(ctx, "\\pmf{\\sumIndependentDistributions{\\bernoulli{\\frac{1}{2}}}{\\bernoulli{\\frac{1}{2}}}}{1}");
    CHECK(valueNumericEquals(out, 0.5L), "sumIndependentDistributions evaluates finite discrete sums");
    valFree(out);

    out = evalLine(ctx, "\\pmf{\\productIndependentDistributions{\\bernoulli{\\frac{1}{2}}}{\\bernoulli{\\frac{1}{2}}}}{1}");
    CHECK(valueNumericEquals(out, 0.25L), "productIndependentDistributions evaluates finite discrete products");
    valFree(out);

    out = evalLine(ctx, "\\expectedValue{\\bernoulli{\\frac{1}{3}}}");
    CHECK(valueNumericEquals(out, 1.0L / 3.0L), "expectedValue works for distributions");
    valFree(out);

    out = evalLine(ctx, "\\mean{\\bernoulli{\\frac{1}{3}}}");
    CHECK(valueNumericEquals(out, 1.0L / 3.0L), "mean works as an expected-value alias for distributions");
    valFree(out);

    out = evalLine(ctx, "\\variance{\\bernoulli{\\frac{1}{2}}}");
    CHECK(valueNumericEquals(out, 0.25L), "variance works for distributions");
    valFree(out);

    seedPRG(9090ULL);
    out = evalLine(ctx, "\\sample{\\bernoulli{\\frac{1}{2}}}");
    CHECK(out.kind == VAL_INT && (out.as.i == 0 || out.as.i == 1), "sample works for distributions");
    valFree(out);

    out = evalLine(ctx, "\\randomVariable{X}{\\bernoulli{\\frac{1}{3}}}");
    CHECK(out.kind == VAL_RANDOM_VARIABLE, "randomVariable constructs an RV");
    valFree(out);

    out = evalLine(ctx, "\\rv{Y}{\\normal{0}{1}}");
    CHECK(out.kind == VAL_RANDOM_VARIABLE, "rv alias constructs an RV");
    valFree(out);

    out = evalLine(ctx, "\\rvExpectedValue{\\rv{X}{\\bernoulli{\\frac{1}{3}}}}");
    CHECK(valueNumericEquals(out, 1.0L / 3.0L), "rvExpectedValue works for RVs");
    valFree(out);

    out = evalLine(ctx, "\\rvPMF{\\rv{X}{\\bernoulli{\\frac{1}{3}}}}{1}");
    CHECK(valueNumericEquals(out, 1.0L / 3.0L), "rvPMF evaluates an RV");
    valFree(out);

    out = evalLine(ctx, "\\rvPDF{\\rv{Y}{\\normal{0}{1}}}{0}");
    CHECK(valueNumericEquals(out, 1.0L / sqrtl(2.0L * acosl(-1.0L))), "rvPDF evaluates an RV");
    valFree(out);

    out = evalLine(ctx, "\\rvCDF{\\rv{Y}{\\normal{0}{1}}}{0}");
    CHECK(valueNumericEquals(out, 0.5L), "rvCDF evaluates an RV");
    valFree(out);

    out = evalLine(ctx, "\\pdfFunc{\\rv{Y}{\\normal{0}{1}}}");
    CHECK(out.kind == VAL_NEKO_EXPR, "pdfFunc works for supported RVs");
    valFree(out);

    out = evalLine(ctx, "\\rvExpectedValue{\\rvScale{\\rv{X}{\\bernoulli{\\frac{1}{2}}}}{2}}");
    CHECK(valueNumericEquals(out, 1.0L), "rvScale transforms RVs");
    valFree(out);

    out = evalLine(ctx, "\\rvExpectedValue{\\rvShift{\\rv{X}{\\bernoulli{\\frac{1}{2}}}}{3}}");
    CHECK(valueNumericEquals(out, 3.5L), "rvShift transforms RVs");
    valFree(out);

    out = evalLine(ctx, "\\rvExpectedValue{\\rvAffine{\\rv{X}{\\bernoulli{\\frac{1}{2}}}}{2}{3}}");
    CHECK(valueNumericEquals(out, 4.0L), "rvAffine transforms RVs");
    valFree(out);

    out = evalLine(ctx, "\\rvPMF{\\rvSumIndependent{\\rv{X}{\\bernoulli{\\frac{1}{2}}}}{\\rv{Y}{\\bernoulli{\\frac{1}{2}}}}}{1}");
    CHECK(valueNumericEquals(out, 0.5L), "rvSumIndependent transforms RVs");
    valFree(out);

    out = evalLine(ctx, "\\rvPMF{\\rvProductIndependent{\\rv{X}{\\bernoulli{\\frac{1}{2}}}}{\\rv{Y}{\\bernoulli{\\frac{1}{2}}}}}{1}");
    CHECK(valueNumericEquals(out, 0.25L), "rvProductIndependent transforms RVs");
    valFree(out);

    seedPRG(9191ULL);
    out = evalLine(ctx, "\\rvSample{\\rv{X}{\\bernoulli{\\frac{1}{2}}}}");
    CHECK(out.kind == VAL_INT && (out.as.i == 0 || out.as.i == 1), "rvSample works for RVs");
    valFree(out);

    evalCtxFree(ctx);
}

static void testRandomCommands(void) {
    SECTION("random commands");
    EvalContext* ctx = evalCtxNew();

    seedPRG(1001ULL);
    Value out = evalLine(ctx, "\\randInt{-3}{3}");
    CHECK(out.kind == VAL_INT && out.as.i >= -3 && out.as.i <= 3, "randInt returns integer in range");
    long long firstInt = out.kind == VAL_INT ? out.as.i : 0;
    valFree(out);

    seedPRG(1001ULL);
    out = evalLine(ctx, "\\randInt{-3}{3}");
    CHECK(out.kind == VAL_INT && out.as.i == firstInt, "randInt is deterministic after seed");
    valFree(out);

    out = evalLine(ctx, "\\randInt{4}{3}");
    CHECK(out.kind == VAL_ERROR, "randInt rejects reversed bounds");
    valFree(out);

    seedPRG(2002ULL);
    out = evalLine(ctx, "\\randFrac{-4}{4}{2}{5}");
    long double fracValue = out.kind == VAL_FRACTION
        ? (long double)out.as.frac.num / (long double)out.as.frac.denom
        : NAN;
    CHECK(out.kind == VAL_FRACTION && fracValue >= -2.0L && fracValue <= 2.0L, "randFrac returns fraction in numeric range");
    valFree(out);

    out = evalLine(ctx, "\\randFrac{1}{2}{0}{0}");
    CHECK(out.kind == VAL_ERROR, "randFrac rejects zero-only denominator range");
    valFree(out);

    seedPRG(3003ULL);
    out = evalLine(ctx, "\\randReal{-1}{2}");
    CHECK(out.kind == VAL_DECIMAL && out.as.d >= -1.0L && out.as.d <= 2.0L, "randReal returns decimal in range");
    valFree(out);

    out = evalLine(ctx, "\\randReal{2}{-1}");
    CHECK(out.kind == VAL_ERROR, "randReal rejects reversed bounds");
    valFree(out);

    seedPRG(4004ULL);
    out = evalLine(ctx, "\\randComplexComp{-1}{1}{2}{4}");
    CHECK(out.kind == VAL_COMPLEX
          && out.as.cplx.real >= -1.0L && out.as.cplx.real <= 1.0L
          && out.as.cplx.imag >= 2.0L && out.as.cplx.imag <= 4.0L,
          "randComplexComp returns complex in component ranges");
    valFree(out);

    out = evalLine(ctx, "\\randComplexComp{1}{-1}{0}{1}");
    CHECK(out.kind == VAL_ERROR, "randComplexComp rejects reversed component bounds");
    valFree(out);

    seedPRG(5005ULL);
    out = evalLine(ctx, "\\randComplexMod{2}{5}");
    long double mod = out.kind == VAL_COMPLEX ? complexAbs(out.as.cplx) : NAN;
    CHECK(out.kind == VAL_COMPLEX && mod >= 2.0L && mod <= 5.0L, "randComplexMod returns complex in modulus range");
    valFree(out);

    out = evalLine(ctx, "\\randComplexMod{-1}{1}");
    CHECK(out.kind == VAL_ERROR, "randComplexMod rejects negative lower modulus");
    valFree(out);

    seedPRG(6006ULL);
    out = evalLine(ctx, "xs = \\list{1,2,3,4}; \\shuffle{xs}");
    CHECK(out.kind == VAL_LIST && out.as.list.n == 4, "shuffle returns list");
    bool seen[5] = {0};
    if (out.kind == VAL_LIST && out.as.list.n == 4) {
        for (size_t i = 0; i < out.as.list.n; i++) {
            if (out.as.list.items[i].kind == VAL_INT
                    && out.as.list.items[i].as.i >= 1
                    && out.as.list.items[i].as.i <= 4) {
                seen[out.as.list.items[i].as.i] = true;
            }
        }
    }
    CHECK(seen[1] && seen[2] && seen[3] && seen[4], "shuffle preserves list elements");
    valFree(out);

    Value list;
    CHECK(envList(ctx, "xs", &list), "shuffle keeps list in env");
    CHECK(list.as.list.n == 4, "shuffle mutates named list");

    out = evalLine(ctx, "\\shuffle{7}");
    CHECK(out.kind == VAL_ERROR, "shuffle rejects non-list input");
    valFree(out);

    evalCtxFree(ctx);
}

static void testPolynomialSolveCommands(void) {
    SECTION("polynomial solve commands");
    EvalContext* ctx = evalCtxNew();

    Value out = evalLine(ctx, "\\solveQuadratic{1,-3,2}");
    CHECK(out.kind == VAL_LIST && out.as.list.n == 2
          && listHasRoot(out, 1.0L, 0.0L)
          && listHasRoot(out, 2.0L, 0.0L),
          "solveQuadratic accepts coefficient arguments");
    valFree(out);

    out = evalLine(ctx, "\\solveQuadratic{\"x^2 + 1\"}");
    CHECK(out.kind == VAL_LIST && out.as.list.n == 2
          && listHasRoot(out, 0.0L, -1.0L)
          && listHasRoot(out, 0.0L, 1.0L),
          "solveQuadratic accepts polynomial strings");
    valFree(out);

    out = evalLine(ctx, "\\solveCubic{\\list{1,-6,11,-6}}");
    CHECK(out.kind == VAL_LIST && out.as.list.n == 3
          && listHasRoot(out, 1.0L, 0.0L)
          && listHasRoot(out, 2.0L, 0.0L)
          && listHasRoot(out, 3.0L, 0.0L),
          "solveCubic accepts coefficient lists");
    valFree(out);

    out = evalLine(ctx, "\\solveQuartic{1,0,-5,0,4}");
    CHECK(out.kind == VAL_LIST && out.as.list.n == 4
          && listHasRoot(out, -2.0L, 0.0L)
          && listHasRoot(out, -1.0L, 0.0L)
          && listHasRoot(out, 1.0L, 0.0L)
          && listHasRoot(out, 2.0L, 0.0L),
          "solveQuartic accepts zero middle coefficients");
    valFree(out);

    out = evalLine(ctx, "\\solveQuartic{}");
    CHECK(out.kind == VAL_INT && out.as.i == 0, "empty polynomial solver input returns zero");
    valFree(out);

    out = evalLine(ctx, "\\solveCubic{1,\"bad\",2,3}");
    CHECK(out.kind == VAL_ERROR, "polynomial solvers reject nonnumeric coefficients");
    valFree(out);

    evalCtxFree(ctx);
}

static void testListPrinting(void) {
    SECTION("list printing");
    EvalContext* ctx = evalCtxNew();

    Value out = evalLine(ctx, "\\list{1,2,\"a\"}");
    char buf[128];
    CHECK(printValueToBuffer(out, buf, sizeof(buf)), "list print captured");
    CHECK(strcmp(buf, "[1, 2, \"a\"]") == 0, "list prints with brackets");
    valFree(out);

    evalCtxFree(ctx);
}

static void testConditionals(void) {
    SECTION("conditionals");
    EvalContext* ctx = evalCtxNew();

    Value out = evalLine(ctx, "\\if{1 == 1}{7}{9}");
    CHECK(out.kind == VAL_INT && out.as.i == 7, "if true takes result branch");
    valFree(out);

    out = evalLine(ctx, "\\if{1 == 2}{7}{9}");
    CHECK(out.kind == VAL_INT && out.as.i == 9, "if false takes else branch");
    valFree(out);

    out = evalLine(ctx, "\\if{1 == 2}{bad[0]}");
    CHECK(out.kind == VAL_NONE, "lone false if returns none");
    valFree(out);

    out = evalLine(ctx, "\\if{1 == 1}{5}{bad[0]}");
    CHECK(out.kind == VAL_INT && out.as.i == 5, "if does not evaluate unused else");
    valFree(out);

    out = evalLine(ctx, "\\if{1 == 2}{bad[0]}{6}");
    CHECK(out.kind == VAL_INT && out.as.i == 6, "if does not evaluate unused result");
    valFree(out);

    evalCtxFree(ctx);
}

static void testMultilineBraceBlocks(void) {
    SECTION("multiline brace blocks");
    EvalContext* ctx = evalCtxNew();

    Value out = evalLine(ctx,
        "\\if{1 == 1}{\n"
        "x = \\list{1,2}\n"
        "\\append{x}{3}\n"
        "x[0] = 9\n"
        "x\n"
        "}{0}");
    CHECK(out.kind == VAL_LIST && out.as.list.n == 3, "multiline if returns final block value");
    CHECK(out.kind == VAL_LIST
          && out.as.list.items[0].kind == VAL_INT && out.as.list.items[0].as.i == 9
          && out.as.list.items[2].kind == VAL_INT && out.as.list.items[2].as.i == 3,
          "newlines inside braces separate statements");
    valFree(out);

    Value list;
    CHECK(envList(ctx, "x", &list), "multiline block assignments persist");
    CHECK(list.as.list.n == 3, "multiline block list has three items");

    out = evalLine(ctx, "\\list{1,\n2,\n3}");
    CHECK(out.kind == VAL_LIST && out.as.list.n == 3, "multiline comma list works");
    valFree(out);

    out = evalLine(ctx, "{1,\n2}");
    CHECK(out.kind == VAL_COMBSET, "multiline comma set works");
    valFree(out);

    evalCtxFree(ctx);
}

static void testScriptMultilineBraceBlocks(void) {
    SECTION("script multiline brace blocks");
    char path[] = "/tmp/bestiary-core-script-XXXXXX";
    int fd = mkstemp(path);
    CHECK(fd >= 0, "temporary script opened");
    if (fd < 0) return;

    FILE* file = fdopen(fd, "w");
    CHECK(file != NULL, "temporary script stream opened");
    if (!file) {
        close(fd);
        unlink(path);
        return;
    }

    fputs("\\if{1 == 1}{\n", file);
    fputs("xs = \\list{4,5}\n", file);
    fputs("\\append{xs}{6}\n", file);
    fputs("}\n", file);
    fclose(file);

    EvalContext* ctx = evalCtxNew();
    ScriptRunOptions opts = {0, 0, 0, 0, 0, 0};
    ScriptRunResult result = {0, 0, 0};
    char error[256] = {0};
    int ok = bstRunScriptFile(ctx, path, &opts, &result, error, sizeof(error));
    CHECK(ok && result.errors == 0, "script multiline block runs");
    CHECK(result.linesRead == 4, "script reads physical lines");
    CHECK(result.linesEvaluated == 1, "script evaluates multiline block once");

    Value list;
    CHECK(envList(ctx, "xs", &list), "script block assignment persists");
    CHECK(list.as.list.n == 3
          && list.as.list.items[2].kind == VAL_INT
          && list.as.list.items[2].as.i == 6,
          "script block append persists");

    evalCtxFree(ctx);
    unlink(path);
}

static void testRestoreScriptMode(void) {
    SECTION("restore script mode");
    char path[] = "/tmp/bestiary-restore-XXXXXX";
    int fd = mkstemp(path);
    CHECK(fd >= 0, "restore temp script created");
    FILE* file = fd >= 0 ? fdopen(fd, "w") : NULL;
    CHECK(file != NULL, "restore temp script opened");
    if (file) {
        fputs("x = \\list{1,2}\n", file);
        fputs("\\graph{x^2}\n", file);
        fputs("\\graph3D{x^2 + y^2}\n", file);
        fputs("bad = 1 / 0\n", file);
        fclose(file);
    } else if (fd >= 0) {
        close(fd);
    }

    EvalContext* ctx = evalCtxNew();
    ScriptRunOptions opts = {0, 0, 0, 1, 1, 0};
    ScriptRunResult result = {0, 0, 0};
    char error[256] = {0};
    int ok = bstRunScriptFile(ctx, path, &opts, &result, error, sizeof(error));
    CHECK(ok && result.errors == 0, "restore script stores lazy assignments without graph reload errors");

    Value out = evalLine(ctx, "x");
    CHECK(out.kind == VAL_LIST && out.as.list.n == 2, "lazy restore assignment is forced on access");
    valFree(out);

    out = evalLine(ctx, "bad");
    CHECK(out.kind == VAL_ERROR, "lazy restore delays failing computations until access");
    valFree(out);

    char exportPath[] = "/tmp/bestiary-restore-export-XXXXXX";
    int exportFd = mkstemp(exportPath);
    CHECK(exportFd >= 0, "restore export temp file created");
    if (exportFd >= 0) close(exportFd);
    char command[256];
    snprintf(command, sizeof(command), "\\export{\"%s\"}", exportPath);
    out = evalLine(ctx, command);
    CHECK(out.kind == VAL_STRING, "restore export command succeeds");
    valFree(out);

    FILE* exported = fopen(exportPath, "r");
    CHECK(exported != NULL, "restore export file can be read");
    char buf[512] = {0};
    size_t nread = exported ? fread(buf, 1, sizeof(buf) - 1, exported) : 0;
    if (exported) fclose(exported);
    CHECK(nread > 0 && strstr(buf, "\\graph{x^2}") != NULL,
          "restore records skipped graph commands for export");

    unlink(path);
    unlink(exportPath);
    evalCtxFree(ctx);
}

static void testPrintCommand(void) {
    SECTION("print command");
    EvalContext* ctx = evalCtxNew();
    char buf[256];
    Value out;

    CHECK(evalLineToBuffer(ctx, "\\print{\"thing in quotes\"}", buf, sizeof(buf), &out), "print string captured");
    CHECK(out.kind == VAL_NONE, "print string returns none");
    CHECK(strcmp(buf, "thing in quotes\n") == 0, "print string uses raw contents");
    valFree(out);

    out = evalLine(ctx, "xs = \\list{1,2,3}");
    valFree(out);

    CHECK(evalLineToBuffer(ctx, "\\print{xs}", buf, sizeof(buf), &out), "print object captured");
    CHECK(out.kind == VAL_NONE, "print object returns none");
    CHECK(strcmp(buf, "[1, 2, 3]\n") == 0, "print object uses value formatting");
    valFree(out);

    CHECK(evalLineToBuffer(ctx, "\\print{1 + 2}", buf, sizeof(buf), &out), "print expression captured");
    CHECK(out.kind == VAL_NONE, "print expression returns none");
    CHECK(strcmp(buf, "3\n") == 0, "print expression prints result");
    valFree(out);

    out = evalLine(ctx, "name = \"Kuma\"; n = 7");
    valFree(out);

    CHECK(evalLineToBuffer(ctx, "\\print{\"$name has $n samples\"}", buf, sizeof(buf), &out),
          "print interpolation captured");
    CHECK(out.kind == VAL_NONE, "print interpolation returns none");
    CHECK(strcmp(buf, "Kuma has 7 samples\n") == 0, "print substitutes variables inside strings");
    valFree(out);

    CHECK(evalLineToBuffer(ctx, "\\print{\"missing=$missing\"}", buf, sizeof(buf), &out),
          "print missing interpolation captured");
    CHECK(out.kind == VAL_NONE, "print missing interpolation returns none");
    CHECK(strcmp(buf, "missing=$missing\n") == 0, "print leaves missing variables raw");
    valFree(out);

    out = evalLine(ctx, "\\print{\"a\", n}");
    CHECK(out.kind == VAL_ERROR, "print rejects comma-separated brace arguments");
    valFree(out);

    out = evalLine(ctx, "\\frac{3,4}");
    CHECK(out.kind == VAL_FRACTION && out.as.frac.num == 3 && out.as.frac.denom == 4,
          "comma-separated brace arguments still feed multi-argument commands");
    valFree(out);

    evalCtxFree(ctx);
}

static void testLoops(void) {
    SECTION("loops");
    EvalContext* ctx = evalCtxNew();

    Value out = evalLine(ctx, "i = 0; \\while{i < 3}{i++}; i");
    CHECK(out.kind == VAL_INT && out.as.i == 3, "while loop repeats until condition fails");
    valFree(out);

    out = evalLine(ctx, "x = 0; \\while{x < 3}{x += 1}; x");
    CHECK(out.kind == VAL_INT && out.as.i == 3, "compound assignment works inside loop body");
    valFree(out);

    out = evalLine(ctx, "xs = \\list{}; i = 0; \\while{i < 3}{\\append{xs}{i}; i++}; xs");
    CHECK(out.kind == VAL_LIST && out.as.list.n == 3, "while loop mutates list");
    CHECK(out.kind == VAL_LIST
          && out.as.list.items[0].kind == VAL_INT && out.as.list.items[0].as.i == 0
          && out.as.list.items[2].kind == VAL_INT && out.as.list.items[2].as.i == 2,
          "while loop body sees incrementing variable");
    valFree(out);

    out = evalLine(ctx, "n = 4; xs = \\list{}; \\for{i = 0; i < n; i++}{\\append{xs}{i}}; xs");
    CHECK(out.kind == VAL_LIST && out.as.list.n == 4, "for loop repeats with header step");
    CHECK(out.kind == VAL_LIST
          && out.as.list.items[0].kind == VAL_INT && out.as.list.items[0].as.i == 0
          && out.as.list.items[3].kind == VAL_INT && out.as.list.items[3].as.i == 3,
          "for loop stores each index");
    valFree(out);

    out = evalLine(ctx, "xs = \\list{}; \\for{i = 0; i < 5; i++}{\\if{i == 2}{\\continue}{\\append{xs}{i}}}; xs");
    CHECK(out.kind == VAL_LIST && out.as.list.n == 4, "continue skips one body branch");
    CHECK(out.kind == VAL_LIST
          && out.as.list.items[0].kind == VAL_INT && out.as.list.items[0].as.i == 0
          && out.as.list.items[2].kind == VAL_INT && out.as.list.items[2].as.i == 3,
          "continue still runs the for step");
    valFree(out);

    out = evalLine(ctx, "xs = \\list{}; \\for{i = 0; i < 5; i++}{\\if{i == 3}{\\break}{\\append{xs}{i}}}; xs");
    CHECK(out.kind == VAL_LIST && out.as.list.n == 3, "break exits the nearest loop");
    CHECK(out.kind == VAL_LIST
          && out.as.list.items[0].kind == VAL_INT && out.as.list.items[0].as.i == 0
          && out.as.list.items[2].kind == VAL_INT && out.as.list.items[2].as.i == 2,
          "break stops before later values");
    valFree(out);

    out = evalLine(ctx, "\\break");
    CHECK(out.kind == VAL_ERROR, "break outside loop fails");
    valFree(out);

    evalCtxFree(ctx);
}

static void testCompoundAssignments(void) {
    SECTION("compound assignments");
    EvalContext* ctx = evalCtxNew();

    Value out = evalLine(ctx, "x = 1; x += 2; x");
    CHECK(out.kind == VAL_INT && out.as.i == 3, "plus-equals lowers through addition");
    valFree(out);

    out = evalLine(ctx, "x = 10; x -= 3; x");
    CHECK(out.kind == VAL_INT && out.as.i == 7, "minus-equals lowers through subtraction");
    valFree(out);

    out = evalLine(ctx, "x = 6; x *= 7; x");
    CHECK(out.kind == VAL_INT && out.as.i == 42, "times-equals lowers through multiplication");
    valFree(out);

    out = evalLine(ctx, "x = 7; x /= 2; x");
    CHECK(out.kind == VAL_FRACTION && out.as.frac.num == 7 && out.as.frac.denom == 2,
          "divide-equals lowers through division");
    valFree(out);

    out = evalLine(ctx, "x = 17; x %= 5; x");
    CHECK(out.kind == VAL_INT && out.as.i == 2, "mod-equals lowers through remainder");
    valFree(out);

    out = evalLine(ctx, "x = 1; x %= 0");
    CHECK(out.kind == VAL_ERROR, "modulo by zero fails");
    valFree(out);

    out = evalLine(ctx, "z = 1 + i; z += 2; z");
    CHECK(out.kind == VAL_COMPLEX && out.as.cplx.real == 3.0 && out.as.cplx.imag == 1.0,
          "plus-equals reuses complex addition");
    valFree(out);

    out = evalLine(ctx, "v = [1,2,3]; v *= 2; v");
    CHECK(out.kind == VAL_VECTOR, "times-equals reuses vector scalar multiplication");
    valFree(out);

    out = evalLine(ctx, "A = [1,2;3,4]; A += [1,1;1,1]; A");
    CHECK(out.kind == VAL_MATRIX, "plus-equals reuses matrix addition");
    valFree(out);

    out = evalLine(ctx, "S = {1,2}; S += {3}; S");
    char buf[128];
    CHECK(out.kind == VAL_COMBSET && printValueToBuffer(out, buf, sizeof(buf)),
          "plus-equals reuses CombSet addition");
    CHECK(strcmp(buf, "{4, 5}") == 0, "CombSet compound addition has expected result");
    valFree(out);

    evalCtxFree(ctx);
}

static void testUsagiCommands(void) {
    SECTION("USAGI commands");
    EvalContext* ctx = evalCtxNew();

    Value out = evalLine(ctx, "F = \\FFRing{2}{3}; \\cardinality{F}");
    CHECK(out.kind == VAL_INT && out.as.i == 8, "FFRing p,n form constructs F_8");
    valFree(out);

    out = evalLine(ctx, "K = \\FFRing{2^3}; \\cardinality{K}");
    CHECK(out.kind == VAL_INT && out.as.i == 8, "FFRing prime-power form constructs F_8");
    valFree(out);

    out = evalLine(ctx, "\\FFRing{12}");
    CHECK(out.kind == VAL_ERROR, "FFRing rejects non-prime-power orders");
    valFree(out);

    out = evalLine(ctx, "G = \\Q8; H = \\groupCenter{G}; Q = G / H; \\cardinality{Q}");
    CHECK(out.kind == VAL_INT && out.as.i == 4, "Q8 quotient command constructs order-4 quotient");
    valFree(out);

    out = evalLine(ctx, "\\isCommutativeGroup{Q}");
    CHECK(out.kind == VAL_BOOL && out.as.b, "Q8 center quotient is commutative");
    valFree(out);

    out = evalLine(ctx, "\\help{FFRing}");
    CHECK(out.kind == VAL_STRING && strstr(out.as.str, "prime-power") != NULL,
          "help documents FFRing forms");
    valFree(out);

    evalCtxFree(ctx);
}

#if 0
static void testQuaternionicCommands(void) {
    SECTION("Quaternionic commands");
    EvalContext* ctx = evalCtxNew();
    char buf[256];

    Value out = evalLine(ctx, "F = \\QQ; a = \\fieldElement{F}{1}; b = \\fieldElement{F}{\\frac{1}{2}}; a + b");
    CHECK(out.kind == VAL_FIELD_ELEMENT && printValueToBuffer(out, buf, sizeof(buf))
          && strcmp(buf, "3/2") == 0,
          "field elements add through operator dispatch");
    valFree(out);

    out = evalLine(ctx, "\\mathbb{Q} == \\QQ");
    CHECK(out.kind == VAL_BOOL && out.as.b, "mathbb Q maps to QQ");
    valFree(out);

    out = evalLine(ctx, "\\mathbb{R} == \\RR");
    CHECK(out.kind == VAL_BOOL && out.as.b, "mathbb R maps to RR");
    valFree(out);

    out = evalLine(ctx, "\\mathbb{C} == \\CC");
    CHECK(out.kind == VAL_BOOL && out.as.b, "mathbb C maps to CC");
    valFree(out);

    out = evalLine(ctx, "\\mathbb{H} == \\HH");
    CHECK(out.kind == VAL_BOOL && out.as.b, "mathbb H maps to HH");
    valFree(out);

    out = evalLine(ctx, "\\mathbb{O} == \\OO");
    CHECK(out.kind == VAL_BOOL && out.as.b, "mathbb O maps to OO");
    valFree(out);

    out = evalLine(ctx, "Q = 17; \\mathbb{Q} == \\QQ");
    CHECK(out.kind == VAL_BOOL && out.as.b, "mathbb reads raw symbols instead of variables");
    valFree(out);

    out = evalLine(ctx, "\\mathbb{Z}");
    CHECK(out.kind == VAL_ERROR, "mathbb rejects unsupported blackboard names");
    valFree(out);

    out = evalLine(ctx, "A = \\quaternionAlgebra{\\RR}{-1}{-1}");
    CHECK(out.kind == VAL_NONE, "quaternion algebra assignment succeeds");
    valFree(out);

    out = evalLine(ctx, "x = \\quaternion{A}{1}{2}{3}{4}");
    CHECK(out.kind == VAL_NONE, "quaternion assignment succeeds");
    valFree(out);

    out = evalLine(ctx, "y = \\quaternion{A}{4}{3}{2}{1}; x + y");
    CHECK(out.kind == VAL_CD_ELEMENT, "CD elements add with infix plus");
    valFree(out);

    out = evalLine(ctx, "\\cdAdd{x}{y}");
    CHECK(out.kind == VAL_CD_ELEMENT, "CD elements add with explicit command");
    valFree(out);

    out = evalLine(ctx, "|x|");
    CHECK(out.kind == VAL_FIELD_ELEMENT && printValueToBuffer(out, buf, sizeof(buf))
          && strcmp(buf, "30") == 0,
          "single bars return the CD norm");
    valFree(out);

    out = evalLine(ctx, "||x||");
    CHECK(out.kind == VAL_FIELD_ELEMENT && printValueToBuffer(out, buf, sizeof(buf))
          && strcmp(buf, "30") == 0,
          "double bars return the CD norm");
    valFree(out);

    out = evalLine(ctx, "x * x^{-1} == \\quaternion{A}{1}{0}{0}{0}");
    CHECK(out.kind == VAL_BOOL && out.as.b, "CD inverse works through exponent -1");
    valFree(out);

    out = evalLine(ctx, "x / x == \\quaternion{A}{1}{0}{0}{0}");
    CHECK(out.kind == VAL_BOOL && out.as.b, "CD division uses right division");
    valFree(out);

    out = evalLine(ctx, "\\cdRightDivide{A}{x}{x} == \\quaternion{A}{1}{0}{0}{0}");
    CHECK(out.kind == VAL_BOOL && out.as.b, "explicit CD right division works");
    valFree(out);

    out = evalLine(ctx, "2 * x");
    CHECK(out.kind == VAL_CD_ELEMENT, "CD scalar multiplication works from the left");
    valFree(out);

    out = evalLine(ctx, "\\cdLeftMatrix{x}");
    CHECK(out.kind == VAL_MATRIX, "CD left multiplication matrix returns a matrix");
    valFree(out);

    out = evalLine(ctx, "F5 = \\GF{5}; AF5 = \\quaternionAlgebra{F5}{-1}{-1}; qF5 = \\quaternion{AF5}{1}{2}{3}{4}; MF5 = \\cdLeftMatrix{qF5}");
    CHECK(out.kind == VAL_NONE, "finite-field CD matrix assignment succeeds");
    valFree(out);

    out = evalLine(ctx, "MF5[0][0]");
    CHECK(out.kind == VAL_FIELD_ELEMENT, "finite-field matrix indexing returns a field element");
    valFree(out);

    out = evalLine(ctx, "\\det{MF5}");
    CHECK(out.kind == VAL_FIELD_ELEMENT, "finite-field matrix determinant returns a field element");
    valFree(out);

    out = evalLine(ctx, "\\rref{MF5}");
    CHECK(out.kind == VAL_MATRIX, "finite-field CD matrix supports row reduction");
    valFree(out);

    out = evalLine(ctx, "\\eigenvalues{MF5}");
    CHECK(out.kind == VAL_ERROR, "finite-field eigenvalues are rejected as numeric-only");
    valFree(out);

    out = evalLine(ctx, "EF5 = \\quadraticExtension{F5}{2}; AEF5 = \\quaternionAlgebra{EF5}{-1}{-1}; qEF5 = \\quaternion{AEF5}{1}{2}{3}{4}; MEF5 = \\cdRightMatrix{qEF5}; \\rank{MEF5}");
    CHECK(out.kind == VAL_INT, "finite-field extension matrix supports rank");
    valFree(out);

    out = evalLine(ctx, "K = \\quadraticExtension{\\QQ}{2}; AK = \\quaternionAlgebra{K}{-1}{-1}; qK = \\quaternion{AK}{1}{2}{3}{4}; MK = \\cdCommutatorMatrix{qK}; \\nullity{MK}");
    CHECK(out.kind == VAL_INT, "number-field matrix supports nullity");
    valFree(out);

    out = evalLine(ctx, "\\octonionAlgebra{\\RR}{-1}{-1}{-1}");
    CHECK(out.kind == VAL_CD_ALGEBRA, "octonion algebra command constructs an algebra");
    valFree(out);

    out = evalLine(ctx, "\\HH");
    CHECK(out.kind == VAL_CD_ALGEBRA, "HH constructs the standard Hamilton algebra");
    valFree(out);

    out = evalLine(ctx, "\\OO");
    CHECK(out.kind == VAL_CD_ALGEBRA, "OO constructs the standard octonion algebra");
    valFree(out);

    out = evalLine(ctx, "i");
    CHECK(out.kind == VAL_COMPLEX, "bare i remains the complex unit");
    valFree(out);

    out = evalLine(ctx, "1 + j");
    CHECK(out.kind == VAL_CD_ELEMENT, "1 plus j promotes to a Hamilton quaternion");
    valFree(out);

    out = evalLine(ctx, "1 + i + j");
    CHECK(out.kind == VAL_CD_ELEMENT, "complex partial expression promotes when j appears");
    valFree(out);

    out = evalLine(ctx, "1 + i*j");
    CHECK(out.kind == VAL_CD_ELEMENT, "i*j promotes to Hamilton multiplication");
    valFree(out);

    out = evalLine(ctx, "\\cdTwoSidedIdeal{j}");
    CHECK(out.kind == VAL_CD_IDEAL, "cdTwoSidedIdeal constructs a CD ideal");
    valFree(out);

    out = evalLine(ctx, "\\cdSubalgebra{j}");
    CHECK(out.kind == VAL_CD_SUBALGEBRA, "cdSubalgebra constructs a CD subalgebra");
    valFree(out);

    out = evalLine(ctx, "\\cdTwoSidedIdeal{j} == \\cdTwoSidedIdeal{j}");
    CHECK(out.kind == VAL_BOOL && out.as.b, "CD ideal equality works");
    valFree(out);

    out = evalLine(ctx, "\\cdSubalgebra{j} == \\cdSubalgebra{j}");
    CHECK(out.kind == VAL_BOOL && out.as.b, "CD subalgebra equality works");
    valFree(out);

    out = evalLine(ctx, "\\cdCommutatorMatrix{j}");
    CHECK(out.kind == VAL_MATRIX, "CD commutator matrix returns a matrix");
    valFree(out);

    out = evalLine(ctx, "\\cdAssociatorMatrix{j}{k}");
    CHECK(out.kind == VAL_MATRIX, "CD associator matrix returns a matrix");
    valFree(out);

    out = evalLine(ctx, "\\cdLeftAnnihilator{j}");
    CHECK(out.kind == VAL_CD_IDEAL, "CD left annihilator returns a CD ideal");
    valFree(out);

    out = evalLine(ctx, "\\cdRightAnnihilator{j}");
    CHECK(out.kind == VAL_CD_IDEAL, "CD right annihilator returns a CD ideal");
    valFree(out);

    out = evalLine(ctx, "\\center{\\HH}");
    CHECK(out.kind == VAL_CD_SUBALGEBRA, "center returns a CD subalgebra");
    valFree(out);

    out = evalLine(ctx, "\\nucleus{\\HH}");
    CHECK(out.kind == VAL_CD_SUBALGEBRA, "nucleus returns a CD subalgebra");
    valFree(out);

    out = evalLine(ctx, "\\commutator{j}{k}");
    CHECK(out.kind == VAL_CD_ELEMENT, "generic commutator works for CD elements");
    valFree(out);

    out = evalLine(ctx, "\\associator{j}{k}{j}");
    CHECK(out.kind == VAL_CD_ELEMENT, "generic associator works for CD elements");
    valFree(out);

    out = evalLine(ctx, "\\isLeftZeroDivisor{j}");
    CHECK(out.kind == VAL_BOOL && !out.as.b, "left zero-divisor test works for Hamilton unit");
    valFree(out);

    out = evalLine(ctx, "\\isRightZeroDivisor{j}");
    CHECK(out.kind == VAL_BOOL && !out.as.b, "right zero-divisor test works for Hamilton unit");
    valFree(out);

    out = evalLine(ctx, "R = \\ZnRing{4}; r = \\getElement{R,\"2\"}; \\isLeftZeroDivisor{r}");
    CHECK(out.kind == VAL_BOOL && out.as.b, "left zero-divisor test delegates to rings");
    valFree(out);

    out = evalLine(ctx, "\\isRightZeroDivisor{r}");
    CHECK(out.kind == VAL_BOOL && out.as.b, "right zero-divisor test delegates to rings");
    valFree(out);

    out = evalLine(ctx, "\\commutator{r}{r}");
    CHECK(out.kind == VAL_RING_ELEMENT, "generic commutator works for ring elements");
    valFree(out);

    evalCtxFree(ctx);
}
#endif

static void testUserFunctions(void) {
    SECTION("user functions");
    EvalContext* ctx = evalCtxNew();
    char buf[256];

    Value out = evalLine(ctx, "\\def{square}{x}{\\return{x * x}}");
    CHECK(out.kind == VAL_NONE, "defining a function returns none");
    valFree(out);

    out = evalLine(ctx, "\\square{5}");
    CHECK(out.kind == VAL_INT && out.as.i == 25, "single-argument function returns a value");
    valFree(out);

    out = evalLine(ctx, "\\def{add}{x, y}{\\return{x + y}}; \\add{2,3}");
    CHECK(out.kind == VAL_INT && out.as.i == 5, "tuple call arguments unpack for multi-argument functions");
    valFree(out);

    out = evalLine(ctx, "\\add{2}{4}");
    CHECK(out.kind == VAL_INT && out.as.i == 6, "separate call groups work for multi-argument functions");
    valFree(out);

    out = evalLine(ctx, "\\def{noop}{x}{x + 1}; \\noop{4}");
    CHECK(out.kind == VAL_NONE, "function with no return returns none");
    valFree(out);

    out = evalLine(ctx, "x = 10; \\def{localSet}{}{x = 3; \\return{x}}; \\localSet{}");
    CHECK(out.kind == VAL_INT && out.as.i == 3, "function locals can be assigned");
    valFree(out);

    out = evalLine(ctx, "x");
    CHECK(out.kind == VAL_INT && out.as.i == 10, "function local assignment does not overwrite globals");
    valFree(out);

    out = evalLine(ctx, "\\def{readX}{}{\\return{x}}; \\readX{}");
    CHECK(out.kind == VAL_SYMBOL && out.as.str && strcmp(out.as.str, "x") == 0,
          "function body does not read global variables");
    valFree(out);

    out = evalLine(ctx, "\\def{early}{x}{\\return{x}; x = 100}; \\early{7}");
    CHECK(out.kind == VAL_INT && out.as.i == 7, "return exits the function body early");
    valFree(out);

    out = evalLine(ctx, "\\def{loopReturn}{}{i = 0; \\while{i < 5}{\\if{i == 3}{\\return{i}}{i++}}}; \\loopReturn{}");
    CHECK(out.kind == VAL_INT && out.as.i == 3, "return exits through loops");
    valFree(out);

    CHECK(evalLineToBuffer(ctx, "\\def{showPair}{a, b}{\\print{\"a=$a b=$b\"}}; \\showPair{1}{2}",
                           buf, sizeof(buf), &out),
          "function print interpolation captured");
    CHECK(out.kind == VAL_NONE, "function with print and no return returns none");
    CHECK(strcmp(buf, "a=1 b=2\n") == 0, "print interpolation reads function locals");
    valFree(out);

    out = evalLine(ctx, "\\square{2,3}");
    CHECK(out.kind == VAL_ERROR, "wrong function arity fails");
    valFree(out);

    out = evalLine(ctx, "\\def{sin}{x}{\\return{x}}");
    CHECK(out.kind == VAL_ERROR && out.as.str && strstr(out.as.str, "cannot override existing function"),
          "user function cannot override a built-in");
    valFree(out);

    out = evalLine(ctx, "\\def{square}{x}{\\return{x + 1}}");
    CHECK(out.kind == VAL_ERROR && out.as.str && strstr(out.as.str, "cannot override existing function"),
          "user function cannot override another user function");
    valFree(out);

    out = evalLine(ctx, "\\return{1}");
    CHECK(out.kind == VAL_ERROR, "return outside a user function fails");
    valFree(out);

    evalCtxFree(ctx);
}

static void testGraphCommandAxes(void) {
    SECTION("graph command axes");
    EvalContext* ctx = evalCtxNew();
    char buf[1024];

    Value out = evalLine(ctx, "x = \\list{1,2}");
    CHECK(out.kind == VAL_NONE, "x can be assigned before graphing");
    valFree(out);

    CHECK(evalLineToBuffer(ctx, "\\graph{x^2}", buf, sizeof(buf), &out),
          "graph output captured");
    CHECK(out.kind == VAL_STRING, "graph keeps x as an axis even when x is assigned");
    CHECK(strstr(buf, "BESTIARY_GRAPH") && strstr(buf, "x ^ 2"),
          "graph emits x-axis expression");
    valFree(out);

    CHECK(evalLineToBuffer(ctx, "\\graph{x^2 + y^2 = 9}", buf, sizeof(buf), &out),
          "implicit graph output captured");
    CHECK(out.kind == VAL_STRING, "graph accepts implicit equations with assigned x");
    CHECK(strstr(buf, "BESTIARY_GRAPH") && strstr(buf, "-9 + y ^ 2 + x ^ 2"),
          "graph emits implicit equation expression");
    valFree(out);

    CHECK(evalLineToBuffer(ctx, "\\graph3D{x^2 + y^2}", buf, sizeof(buf), &out),
          "explicit 3D graph output captured");
    CHECK(out.kind == VAL_STRING, "graph3D keeps x and y as axes");
    CHECK(strstr(buf, "BESTIARY_GRAPH3D") && strstr(buf, "x ^ 2") && strstr(buf, "y ^ 2"),
          "graph3D emits explicit surface expression");
    valFree(out);

    CHECK(evalLineToBuffer(ctx, "\\graph3D{x + t}{2}", buf, sizeof(buf), &out),
          "3D graph t-axis output captured");
    CHECK(out.kind == VAL_STRING, "graph3D accepts t as the second horizontal axis");
    CHECK(strstr(buf, "BESTIARY_GRAPH3D") && strstr(buf, "\t2\tt\t") && strstr(buf, "t + x"),
          "graph3D emits target and t-axis expression");
    valFree(out);

    CHECK(evalLineToBuffer(ctx, "\\graph3D{x^2 + y^2 + z^2 = 9}", buf, sizeof(buf), &out),
          "implicit 3D graph output captured");
    CHECK(out.kind == VAL_STRING, "graph3D accepts implicit equations with z");
    CHECK(strstr(buf, "BESTIARY_GRAPH3D") && strstr(buf, "z ^ 2 + -9 + y ^ 2 + x ^ 2"),
          "graph3D emits implicit surface expression");
    valFree(out);

    evalCtxFree(ctx);
}

static void testExportCommand(void) {
    SECTION("export command");
    EvalContext* ctx = evalCtxNew();
    char path[] = "/tmp/bestiary-export-XXXXXX";
    int fd = mkstemp(path);
    CHECK(fd >= 0, "export temp file created");
    if (fd >= 0) close(fd);

    evalCtxRecordInput(ctx, "x = 1");
    evalCtxRecordInput(ctx, "y = x + 2");
    evalCtxRecordInput(ctx, "\\export{\"ignored.bsy\"}");

    char command[256];
    snprintf(command, sizeof(command), "\\export{\"%s\"}", path);
    Value out = evalLine(ctx, command);
    CHECK(out.kind == VAL_STRING, "export command returns summary");
    valFree(out);

    FILE* fp = fopen(path, "r");
    CHECK(fp != NULL, "export file can be read");
    char buf[256] = {0};
    size_t nread = fp ? fread(buf, 1, sizeof(buf) - 1, fp) : 0;
    if (fp) fclose(fp);
    CHECK(nread > 0 && strstr(buf, "x = 1") && strstr(buf, "y = x + 2"),
          "export writes recorded inputs");
    CHECK(strstr(buf, "\\export") == NULL, "export omits export commands");
    unlink(path);
    evalCtxFree(ctx);
}

int main(void) {
    registerBuiltins();

    testListConstruction();
    testListIndexing();
    testMatrixIndexing();
    testListMutation();
    testListSorting();
    testKumaValueOwnership();
    testKumaCommands();
    testRandomCommands();
    testPolynomialSolveCommands();
    testListPrinting();
    testConditionals();
    testMultilineBraceBlocks();
    testScriptMultilineBraceBlocks();
    testRestoreScriptMode();
    testPrintCommand();
    testLoops();
    testCompoundAssignments();
    testUsagiCommands();
    testUserFunctions();
    testGraphCommandAxes();
    testExportCommand();

    printf("\nCore tests: %d/%d passed\n", testsPassed, testsRun);
    return testsPassed == testsRun ? 0 : 1;
}
