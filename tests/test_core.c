#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ast.h"
#include "eval.h"
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
    ScriptRunOptions opts = {0, 0, 0};
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

int main(void) {
    registerBuiltins();

    testListConstruction();
    testListIndexing();
    testListMutation();
    testListSorting();
    testKumaValueOwnership();
    testListPrinting();
    testConditionals();
    testMultilineBraceBlocks();
    testScriptMultilineBraceBlocks();
    testPrintCommand();
    testLoops();
    testCompoundAssignments();
    testUserFunctions();

    printf("\nCore tests: %d/%d passed\n", testsPassed, testsRun);
    return testsPassed == testsRun ? 0 : 1;
}
