#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ast.h"
#include "eval.h"
#include "lexer.h"
#include "parser.h"
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

static void testQuaternionicCommands(void) {
    SECTION("Quaternionic commands");
    EvalContext* ctx = evalCtxNew();
    char buf[256];

    Value out = evalLine(ctx, "\\mathbb{H} == \\HH");
    CHECK(out.kind == VAL_BOOL && out.as.b, "mathbb H maps to HH");
    valFree(out);

    out = evalLine(ctx, "\\mathbb{O} == \\OO");
    CHECK(out.kind == VAL_BOOL && out.as.b, "mathbb O maps to OO");
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

    out = evalLine(ctx, "\\cdLeftIdeal{j}");
    CHECK(out.kind == VAL_CD_IDEAL, "cdLeftIdeal constructs a CD ideal");
    valFree(out);

    out = evalLine(ctx, "\\cdRightIdeal{j}");
    CHECK(out.kind == VAL_CD_IDEAL, "cdRightIdeal constructs a CD ideal");
    valFree(out);

    out = evalLine(ctx, "\\cdTwoSidedIdeal{j}");
    CHECK(out.kind == VAL_CD_IDEAL, "cdTwoSidedIdeal constructs a CD ideal");
    valFree(out);

    out = evalLine(ctx, "\\leftIdeal{j}");
    CHECK(out.kind == VAL_CD_IDEAL, "leftIdeal constructs a CD ideal from CD generators");
    valFree(out);

    out = evalLine(ctx, "\\rightIdeal{j}");
    CHECK(out.kind == VAL_CD_IDEAL, "rightIdeal constructs a CD ideal from CD generators");
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

    out = evalLine(ctx, "j \\in \\cdTwoSidedIdeal{j}");
    CHECK(out.kind == VAL_BOOL && out.as.b, "infix in tests membership in CD ideals");
    valFree(out);

    out = evalLine(ctx, "k \\in \\cdSubalgebra{j}");
    CHECK(out.kind == VAL_BOOL && !out.as.b, "infix in tests membership in CD subalgebras");
    valFree(out);

    out = evalLine(ctx, "\\cdLeftIdeal{j} \\cap \\cdLeftIdeal{k}");
    CHECK(out.kind == VAL_CD_IDEAL, "cap intersects CD ideals");
    valFree(out);

    out = evalLine(ctx, "\\intersect{\\cdSubalgebra{j}}{\\cdSubalgebra{k}}");
    CHECK(out.kind == VAL_CD_SUBALGEBRA, "intersect command intersects CD subalgebras");
    valFree(out);

    out = evalLine(ctx, "\\cdLeftIdeal{j} + \\cdLeftIdeal{k}");
    CHECK(out.kind == VAL_CD_IDEAL, "CD ideals add through plus");
    valFree(out);

    out = evalLine(ctx, "\\cdLeftIdeal{j} - \\cdLeftIdeal{k}");
    CHECK(out.kind == VAL_CD_IDEAL, "CD ideals subtract through minus");
    valFree(out);

    out = evalLine(ctx, "2 * \\cdLeftIdeal{j}");
    CHECK(out.kind == VAL_CD_IDEAL, "CD ideals support scalar multiplication from the left");
    valFree(out);

    out = evalLine(ctx, "\\cdLeftIdeal{j} * 2");
    CHECK(out.kind == VAL_CD_IDEAL, "CD ideals support scalar multiplication from the right");
    valFree(out);

    out = evalLine(ctx, "k * \\cdLeftIdeal{j}");
    CHECK(out.kind == VAL_CD_IDEAL, "CD ideals support left multiplication by elements");
    valFree(out);

    out = evalLine(ctx, "\\cdLeftIdeal{j} * k");
    CHECK(out.kind == VAL_CD_IDEAL, "CD ideals support right multiplication by elements");
    valFree(out);

    out = evalLine(ctx, "\\cdToVector{x}");
    CHECK(out.kind == VAL_VECTOR, "cdToVector returns a vector");
    valFree(out);

    out = evalLine(ctx, "\\cdFromVector{A}{\\cdToVector{x}} == x");
    CHECK(out.kind == VAL_BOOL && out.as.b, "cdFromVector inverts cdToVector");
    valFree(out);

    out = evalLine(ctx, "\\cdSpanBasis{j}{k}");
    CHECK(out.kind == VAL_LIST && out.as.list.n == 2, "cdSpanBasis returns a list basis");
    valFree(out);

    out = evalLine(ctx, "\\cdInSpan{j}{\\list{j,k}}");
    CHECK(out.kind == VAL_BOOL && out.as.b, "cdInSpan detects span membership");
    valFree(out);

    out = evalLine(ctx, "\\cdBasisElement{A}{2} == j");
    CHECK(out.kind == VAL_BOOL && out.as.b, "cdBasisElement returns zero-based basis elements");
    valFree(out);

    out = evalLine(ctx, "\\cdStandardBasis{A}");
    CHECK(out.kind == VAL_LIST && out.as.list.n == 4, "cdStandardBasis returns the full basis");
    valFree(out);

    out = evalLine(ctx, "\\quaternionMatrixRep{A}");
    CHECK(out.kind == VAL_QUATERNION_MATRIX_REP, "quaternionMatrixRep constructs a representation value");
    valFree(out);

    out = evalLine(ctx, "\\quaternionToMatrix{x}");
    CHECK(out.kind == VAL_MATRIX, "quaternionToMatrix constructs a matrix automatically");
    valFree(out);

    out = evalLine(ctx, "rep = \\quaternionMatrixRep{A}; \\quaternionToMatrix{x}{rep}");
    CHECK(out.kind == VAL_MATRIX, "quaternionToMatrix accepts an explicit representation");
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

int main(void) {
    registerBuiltins();
    testQuaternionicCommands();

    printf("\nQuaternionic tests: %d/%d passed\n", testsPassed, testsRun);
    return testsPassed == testsRun ? 0 : 1;
}
