
/* AI GENERATED TEST SUITE */

// Everything here looks good to me, but note this was generated my an LLM.
// More tests might be warranted if using for production.

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "hebi.h"
#include "sokko.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ---------- Test infrastructure ---------- */

static int testsRun = 0;
static int testsPassed = 0;

#define CHECK(cond, name) do { \
    testsRun++; \
    if (cond) { testsPassed++; printf("  [PASS] %s\n", name); } \
    else { printf("  [FAIL] %s (line %d)\n", name, __LINE__); } \
} while (0)

#define SECTION(name) printf("\n=== %s ===\n", name)

// Approximate equality for doubles
static bool approx(long double a, long double b, long double tol) {
    if (isnan(a) && isnan(b)) return true;
    return fabsl(a - b) <= tol;
}

// Approximate equality between a MatrixElement and an expected (re, im) pair
static bool approxElem(MatrixElement e, long double re, long double im, long double tol) {
    long double actualRe = e.isComplex ? e.value.complex.real : e.value.real;
    long double actualIm = e.isComplex ? e.value.complex.imag : 0.0;
    if (isnan(actualRe) && isnan(re)) return true;
    return fabsl(actualRe - re) <= tol && fabsl(actualIm - im) <= tol;
}

// Approximate equality between a MatrixElement and an expected real value
static bool approxReal(MatrixElement e, long double expected, long double tol) {
    return approxElem(e, expected, 0.0, tol);
}

// Shorthands for building MatrixElements in tests
static MatrixElement R(long double x) { return elemFromReal(x); }
static MatrixElement C(long double re, long double im) {
    ComplexNumber c = {re, im};
    return elemFromComplex(c);
}

// Build a matrix from a plain long double array for readability
static Matrix* makeRealMatrix(int rows, int cols, const long double* data) {
    int n = rows * cols;
    MatrixElement* buf = malloc(n * sizeof(MatrixElement));
    for (int i = 0; i < n; i++) buf[i] = R(data[i]);
    Matrix* m = constructMatrixFromArray(rows, cols, buf, n);
    free(buf);
    return m;
}

static Vector* makeRealVector(int dim, const long double* data) {
    return makeRealMatrix(dim, 1, data);
}

/* ---------- Helper tests ---------- */

void testComp() {
    SECTION("comp");
    long long a = 1, b = 2, c = 1;
    CHECK(comp(&a, &b) < 0, "comp a < b");
    CHECK(comp(&b, &a) > 0, "comp b > a");
    CHECK(comp(&a, &c) == 0, "comp a == c");

    long long arr[] = {3, 1, 4, 1, 5, 9, 2, 6};
    qsort(arr, 8, sizeof(long long), comp);
    CHECK(arr[0] == 1 && arr[7] == 9, "qsort with comp");
}

void testIsSquare() {
    SECTION("isSquare");
    Matrix* sq = constructMatrix(3, 3);
    Matrix* nsq = constructMatrix(2, 5);
    CHECK(isSquare(sq) == true, "3x3 is square");
    CHECK(isSquare(nsq) == false, "2x5 is not square");
    freeMatrix(sq);
    freeMatrix(nsq);
}

/* ---------- Free method tests ---------- */

void testFreeLU() {
    SECTION("freeLU");
    freeLU(NULL);
    CHECK(true, "freeLU(NULL) doesn't crash");

    Matrix* m = constructMatrix(2, 2);
    setEntry(m, 0, 0, R(4)); setEntry(m, 0, 1, R(3));
    setEntry(m, 1, 0, R(6)); setEntry(m, 1, 1, R(3));
    LU* lu = luDecompose(m);
    freeLU(lu);
    CHECK(true, "freeLU on real LU doesn't crash");
    freeMatrix(m);
}

void testFreeMatrix() {
    SECTION("freeMatrix");
    freeMatrix(NULL);
    CHECK(true, "freeMatrix(NULL) doesn't crash");

    Matrix* m = constructMatrix(3, 3);
    freeMatrix(m);
    CHECK(true, "freeMatrix on fresh matrix doesn't crash");

    // Matrix with cached LU
    Matrix* m2 = constructMatrix(2, 2);
    setEntry(m2, 0, 0, R(1)); setEntry(m2, 1, 1, R(1));
    cacheLU(m2);
    freeMatrix(m2);
    CHECK(true, "freeMatrix with cached LU doesn't crash");
}

void testResetEntryCache() {
    SECTION("resetMatrixCache");
    Matrix* m = constructMatrix(2, 2);
    setEntry(m, 0, 0, R(1)); setEntry(m, 1, 1, R(1));
    cacheLU(m);
    CHECK(m->cachedLU != NULL, "cache populated");
    resetMatrixCache(m);
    CHECK(m->cachedLU == NULL, "cache cleared");
    resetMatrixCache(m);
    CHECK(m->cachedLU == NULL, "second reset is no-op");
    freeMatrix(m);
}

/* ---------- Accessor tests ---------- */

void testGetSetEntry() {
    SECTION("getEntry / setEntry");
    Matrix* m = constructMatrix(3, 4);
    setEntry(m, 0, 0, R(1.5));
    setEntry(m, 2, 3, R(-7.25));
    setEntry(m, 1, 2, R(42));
    CHECK(approxReal(getEntry(m, 0, 0), 1.5, 0), "get/set (0,0)");
    CHECK(approxReal(getEntry(m, 2, 3), -7.25, 0), "get/set (2,3)");
    CHECK(approxReal(getEntry(m, 1, 2), 42, 0), "get/set (1,2)");
    CHECK(approxReal(getEntry(m, 0, 1), 0, 0), "uninitialized is 0");

    // Complex entries
    setEntry(m, 0, 1, C(1, -2));
    MatrixElement got = getEntry(m, 0, 1);
    CHECK(got.isComplex, "complex entry stays complex");
    CHECK(approxElem(got, 1.0, -2.0, 1e-12), "complex entry value preserved");

    // setEntry should invalidate cache
    Matrix* sq = constructMatrix(2, 2);
    setEntry(sq, 0, 0, R(1)); setEntry(sq, 1, 1, R(1));
    cacheLU(sq);
    CHECK(sq->cachedLU != NULL, "cache exists before set");
    setEntry(sq, 0, 0, R(5));
    CHECK(sq->cachedLU == NULL, "setEntry invalidates cache");
    freeMatrix(m);
    freeMatrix(sq);
}

/* ---------- Complex arithmetic tests ---------- */

void testComplexArith() {
    SECTION("complex arithmetic");
    ComplexNumber a = {1, 2};
    ComplexNumber b = {3, -4};

    ComplexNumber sum = complexAdd(a, b);
    CHECK(approx(sum.real, 4, 1e-12) && approx(sum.imag, -2, 1e-12), "add");

    ComplexNumber diff = complexSub(a, b);
    CHECK(approx(diff.real, -2, 1e-12) && approx(diff.imag, 6, 1e-12), "sub");

    // (1+2i)(3-4i) = 3 - 4i + 6i - 8i^2 = 3 + 2i + 8 = 11 + 2i
    ComplexNumber prod = complexMul(a, b);
    CHECK(approx(prod.real, 11, 1e-12) && approx(prod.imag, 2, 1e-12), "mul");

    // (1+2i)/(3-4i) = (1+2i)(3+4i)/((3)^2 + (4)^2) = (3 + 4i + 6i + 8i^2)/25 = (-5 + 10i)/25 = -0.2 + 0.4i
    ComplexNumber q = complexDiv(a, b);
    CHECK(approx(q.real, -0.2, 1e-12) && approx(q.imag, 0.4, 1e-12), "div");

    ComplexNumber n = complexNeg(a);
    CHECK(approx(n.real, -1, 1e-12) && approx(n.imag, -2, 1e-12), "neg");

    ComplexNumber cj = complexConj(a);
    CHECK(approx(cj.real, 1, 1e-12) && approx(cj.imag, -2, 1e-12), "conj");

    CHECK(approx(complexAbs((ComplexNumber){3, 4}), 5.0, 1e-12), "abs 3+4i = 5");
    CHECK(approx(complexAbs((ComplexNumber){0, 0}), 0.0, 1e-12), "abs 0 = 0");

    CHECK(approx(complexArg((ComplexNumber){1, 0}), 0.0, 1e-12), "arg(1) = 0");
    CHECK(approx(complexArg((ComplexNumber){0, 1}), M_PI/2, 1e-12), "arg(i) = pi/2");
    CHECK(approx(complexArg((ComplexNumber){-1, 0}), M_PI, 1e-12), "arg(-1) = pi");

    // sqrtl(-1) = i (thanks to real fast path)
    ComplexNumber sqrtNeg1 = complexSqrt((ComplexNumber){-1, 0});
    CHECK(sqrtNeg1.real == 0.0 && approx(sqrtNeg1.imag, 1.0, 1e-12), "sqrtl(-1) = i exactly");

    // sqrtl(4) = 2 (real fast path, no imag noise)
    ComplexNumber sqrt4 = complexSqrt((ComplexNumber){4, 0});
    CHECK(sqrt4.real == 2.0 && sqrt4.imag == 0.0, "sqrtl(4) = 2 exactly");

    // sqrtl(-4) = 2i
    ComplexNumber sqrtNeg4 = complexSqrt((ComplexNumber){-4, 0});
    CHECK(sqrtNeg4.real == 0.0 && approx(sqrtNeg4.imag, 2.0, 1e-12), "sqrtl(-4) = 2i");

    // sqrtl(3 + 4i) = 2 + i (check 2+i squared = 4 + 4i - 1 = 3 + 4i)
    ComplexNumber sqrtComplex = complexSqrt((ComplexNumber){3, 4});
    CHECK(approx(sqrtComplex.real, 2.0, 1e-12) && approx(sqrtComplex.imag, 1.0, 1e-12), "sqrtl(3+4i) = 2+i");

    // cbrtl(8) = 2 (real fast path)
    ComplexNumber cbrt8 = complexCbrt((ComplexNumber){8, 0});
    CHECK(cbrt8.real == 2.0 && cbrt8.imag == 0.0, "cbrtl(8) = 2 exactly");

    // cbrtl(-8) = -2 (real fast path)
    ComplexNumber cbrtNeg8 = complexCbrt((ComplexNumber){-8, 0});
    CHECK(cbrtNeg8.real == -2.0 && cbrtNeg8.imag == 0.0, "cbrtl(-8) = -2 exactly");

    // cbrtl(i) should have abs 1 and arg pi/6
    ComplexNumber cbrtI = complexCbrt((ComplexNumber){0, 1});
    CHECK(approx(complexAbs(cbrtI), 1.0, 1e-12), "|cbrtl(i)| = 1");
    // cbrtl(i) = cosl(pi/6) + i sinl(pi/6) = sqrtl(3)/2 + i/2
    CHECK(approx(cbrtI.real, sqrtl(3)/2, 1e-12), "Re(cbrtl(i)) = sqrtl(3)/2");
    CHECK(approx(cbrtI.imag, 0.5, 1e-12), "Im(cbrtl(i)) = 1/2");

    ComplexNumber expIpi = complexExp((ComplexNumber){0, M_PI});
    CHECK(approx(expIpi.real, -1.0, 1e-12) && approx(expIpi.imag, 0.0, 1e-12), "expl(i*pi) = -1");

    ComplexNumber logNeg1 = complexLog((ComplexNumber){-1, 0});
    CHECK(approx(logNeg1.real, 0.0, 1e-12) && approx(logNeg1.imag, M_PI, 1e-12), "logl(-1) = i*pi");

    ComplexNumber sinI = complexSin((ComplexNumber){0, 1});
    CHECK(approx(sinI.real, 0.0, 1e-12) && approx(sinI.imag, sinh(1.0), 1e-12), "sinl(i) = i sinh(1)");

    ComplexNumber cosI = complexCos((ComplexNumber){0, 1});
    CHECK(approx(cosI.real, cosh(1.0), 1e-12) && approx(cosI.imag, 0.0, 1e-12), "cosl(i) = cosh(1)");

    ComplexNumber tanI = complexTan((ComplexNumber){0, 1});
    CHECK(approx(tanI.real, 0.0, 1e-12) && approx(tanI.imag, tanh(1.0), 1e-12), "tanl(i) = i tanh(1)");

    CHECK(complexEq((ComplexNumber){1, 2}, (ComplexNumber){1 + 1e-15, 2}, 1e-12), "complexEq within tol");
    CHECK(!complexEq((ComplexNumber){1, 2}, (ComplexNumber){1, 3}, 1e-12), "complexEq beyond tol");
}

/* ---------- MatrixElement operation tests ---------- */

void testMatrixElementOps() {
    SECTION("MatrixElement operations");

    MatrixElement r = elemFromReal(2.5);
    CHECK(!r.isComplex, "elemFromReal stays real");
    CHECK(r.value.real == 2.5, "elemFromReal value");

    MatrixElement rFromC = elemFromComplex((ComplexNumber){3, 0});
    CHECK(!rFromC.isComplex, "elemFromComplex collapses imag=0 to real");
    CHECK(rFromC.value.real == 3.0, "collapsed value correct");

    MatrixElement c = elemFromComplex((ComplexNumber){1, 2});
    CHECK(c.isComplex, "elemFromComplex keeps complex when imag != 0");
    CHECK(approxElem(c, 1, 2, 0), "complex value preserved");

    ComplexNumber rToC = elemToComplex(R(5));
    CHECK(rToC.real == 5.0 && rToC.imag == 0.0, "real -> complex");
    ComplexNumber cToC = elemToComplex(C(1, -1));
    CHECK(cToC.real == 1.0 && cToC.imag == -1.0, "complex -> complex");

    // Arithmetic with two reals stays real (cheap path)
    MatrixElement rr = elemAdd(R(2), R(3));
    CHECK(!rr.isComplex && rr.value.real == 5.0, "real + real stays real");
    CHECK(!elemSub(R(2), R(3)).isComplex, "real - real stays real");
    CHECK(!elemMul(R(2), R(3)).isComplex, "real * real stays real");
    CHECK(!elemDiv(R(6), R(3)).isComplex, "real / real stays real");
    CHECK(!elemNeg(R(5)).isComplex, "neg of real stays real");

    // Mixed arithmetic promotes to complex when needed
    MatrixElement rc = elemAdd(R(1), C(0, 1));
    CHECK(rc.isComplex, "real + complex -> complex");
    CHECK(approxElem(rc, 1, 1, 1e-12), "1 + i value");

    // i * i = -1 should collapse back to real
    MatrixElement ii = elemMul(C(0, 1), C(0, 1));
    CHECK(!ii.isComplex, "i*i collapses to real");
    CHECK(approxReal(ii, -1, 1e-12), "i*i = -1");

    // (1+i) + (1-i) = 2 should collapse
    MatrixElement conjSum = elemAdd(C(1, 1), C(1, -1));
    CHECK(!conjSum.isComplex, "conjugate pair sum collapses");
    CHECK(approxReal(conjSum, 2.0, 1e-12), "conjugate pair sum = 2*real");

    // (1+i) * (1-i) = 1 - i^2 = 2 should collapse
    MatrixElement conjProd = elemMul(C(1, 1), C(1, -1));
    CHECK(!conjProd.isComplex, "|a|^2 style product collapses");
    CHECK(approxReal(conjProd, 2.0, 1e-12), "(1+i)(1-i) = 2");

    // Division by complex denominator
    MatrixElement div = elemDiv(C(1, 0), C(0, 1));  // 1/i = -i
    CHECK(div.isComplex, "1/i is complex");
    CHECK(approxElem(div, 0, -1, 1e-12), "1/i = -i");

    CHECK(approxElem(elemNeg(C(3, -4)), -3, 4, 1e-12), "neg of complex");

    CHECK(elemConj(R(5)).isComplex == false, "conj of real is real");
    CHECK(approxReal(elemConj(R(5)), 5, 0), "conj of real is itself");
    MatrixElement conj = elemConj(C(1, -2));
    CHECK(conj.isComplex, "conj of complex is complex");
    CHECK(approxElem(conj, 1, 2, 1e-12), "conj(1-2i) = 1+2i");

    CHECK(elemAbs(R(-3)) == 3.0, "|−3| = 3");
    CHECK(approx(elemAbs(C(3, 4)), 5.0, 1e-12), "|3+4i| = 5");

    CHECK(elemIsZero(R(0), 0), "real zero is zero");
    CHECK(elemIsZero(R(1e-13), 1e-12), "tiny real within tol");
    CHECK(!elemIsZero(R(1), 1e-12), "1 is not zero");
    CHECK(elemIsZero(C(0, 0), 0), "complex zero is zero");
    CHECK(!elemIsZero(C(0, 1), 1e-12), "i is not zero");
    CHECK(elemIsZero(C(1e-13, 1e-13), 1e-12), "tiny complex within tol");

    CHECK(elemIsNan(R(NAN)), "real NAN detected");
    CHECK(!elemIsNan(R(1)), "finite real not NAN");
    CHECK(elemIsNan(C(NAN, 0)), "complex with NAN real");
    CHECK(elemIsNan(C(0, NAN)), "complex with NAN imag");

    CHECK(elemEq(R(1), R(1 + 1e-15), 1e-12), "elemEq within tol");
    CHECK(!elemEq(R(1), R(2), 1e-12), "elemEq beyond tol");
    CHECK(elemEq(R(1), C(1, 0), 1e-12), "real 1 equals complex 1+0i");
    CHECK(elemEq(C(1, 2), C(1, 2), 0), "complex exact equality");
    CHECK(!elemEq(C(1, 2), C(1, -2), 1e-12), "different imag fails eq");
}

/* ---------- LU tests ---------- */

void testLuDecompose() {
    SECTION("luDecompose");

    // Non-square returns NULL
    Matrix* nsq = constructMatrix(2, 3);
    CHECK(luDecompose(nsq) == NULL, "non-square returns NULL");
    freeMatrix(nsq);

    // Singular returns NULL
    Matrix* sing = constructMatrix(2, 2);
    setEntry(sing, 0, 0, R(1)); setEntry(sing, 0, 1, R(2));
    setEntry(sing, 1, 0, R(2)); setEntry(sing, 1, 1, R(4));
    CHECK(luDecompose(sing) == NULL, "singular returns NULL");
    freeMatrix(sing);

    // Valid decomp on identity
    Matrix* id = idMatrix(4);
    LU* lu = luDecompose(id);
    CHECK(lu != NULL, "identity decomposes");
    CHECK(lu->n == 4, "n correct");
    CHECK(lu->sign == 1, "identity sign +1");
    freeLU(lu);
    freeMatrix(id);

    // Decomp of a non-trivial matrix
    long double md[] = {2, 1, 1,  4, 3, 3,  8, 7, 9};
    Matrix* m = makeRealMatrix(3, 3, md);
    LU* lu2 = luDecompose(m);
    CHECK(lu2 != NULL, "3x3 decomposes");
    freeLU(lu2);
    freeMatrix(m);

    // Complex-entry matrix can be decomposed
    MatrixElement cd[4] = { C(1, 1), R(2), R(-1), C(2, -1) };
    Matrix* cm = constructMatrixFromArray(2, 2, cd, 4);
    LU* luc = luDecompose(cm);
    CHECK(luc != NULL, "complex matrix decomposes");
    freeLU(luc);
    freeMatrix(cm);
}

void testLuDet() {
    SECTION("luDet");
    Matrix* id = idMatrix(5);
    LU* lu = luDecompose(id);
    CHECK(approxReal(luDet(lu), 1.0, 1e-10), "det(I) = 1");
    freeLU(lu);
    freeMatrix(id);

    long double md[] = {3, 8, 4, 6};
    Matrix* m = makeRealMatrix(2, 2, md);
    LU* lu2 = luDecompose(m);
    CHECK(approxReal(luDet(lu2), -14.0, 1e-10), "det 2x2");
    freeLU(lu2);
    freeMatrix(m);

    // Determinant of a complex matrix: diag(1+i, 2) has det = 2(1+i) = 2 + 2i
    MatrixElement dcd[4] = { C(1, 1), R(0), R(0), R(2) };
    Matrix* dcm = constructMatrixFromArray(2, 2, dcd, 4);
    LU* lu3 = luDecompose(dcm);
    CHECK(approxElem(luDet(lu3), 2.0, 2.0, 1e-10), "complex det = 2+2i");
    freeLU(lu3);
    freeMatrix(dcm);
}

void testCacheLU() {
    SECTION("cacheLU");
    long double md[] = {1, 2, 3, 4};
    Matrix* m = makeRealMatrix(2, 2, md);
    cacheLU(m);
    CHECK(m->cachedLU != NULL, "cache populated");
    cacheLU(m);
    CHECK(m->cachedLU != NULL, "re-cache works");
    freeMatrix(m);
}

/* ---------- Constructor tests ---------- */

void testConstructMatrix() {
    SECTION("constructMatrix");
    CHECK(constructMatrix(0, 5) == NULL, "rows < 1 returns NULL");
    CHECK(constructMatrix(5, 0) == NULL, "cols < 1 returns NULL");
    CHECK(constructMatrix(-1, 3) == NULL, "negative rows returns NULL");

    Matrix* m = constructMatrix(3, 4);
    CHECK(m != NULL, "valid construct");
    CHECK(m->numRows == 3 && m->numCols == 4, "dimensions correct");
    CHECK(m->cachedLU == NULL, "cache initially NULL");
    for (int i = 0; i < 3; i++) {
	for (int j = 0; j < 4; j++) {
	    if (!approxReal(getEntry(m, i, j), 0, 0)) { CHECK(false, "all zeros"); freeMatrix(m); return; }
	}
    }
    CHECK(true, "all entries zero");
    freeMatrix(m);
}

void testConstructMatrixFromMatrix() {
    SECTION("constructMatrixFromMatrix");
    MatrixElement row0[] = {R(1), R(2), R(3)};
    MatrixElement row1[] = {R(4), R(5), R(6)};
    MatrixElement* rows[] = {row0, row1};

    CHECK(constructMatrixFromMatrix(2, 3, rows, 3, 3) == NULL, "lenData mismatch");
    CHECK(constructMatrixFromMatrix(2, 3, rows, 2, 4) == NULL, "colLenData mismatch");

    Matrix* m = constructMatrixFromMatrix(2, 3, rows, 2, 3);
    CHECK(m != NULL, "valid construct");
    CHECK(approxReal(getEntry(m, 0, 0), 1, 0) && approxReal(getEntry(m, 1, 2), 6, 0), "values correct");
    freeMatrix(m);

    // Constructing with mixed real/complex entries
    MatrixElement mrow0[] = { R(1), C(0, 1) };
    MatrixElement mrow1[] = { C(2, -1), R(3) };
    MatrixElement* mrows[] = {mrow0, mrow1};
    Matrix* mc = constructMatrixFromMatrix(2, 2, mrows, 2, 2);
    CHECK(!getEntry(mc, 0, 0).isComplex, "real stored real");
    CHECK(getEntry(mc, 0, 1).isComplex, "complex stored complex");
    CHECK(approxElem(getEntry(mc, 1, 0), 2, -1, 0), "(2-i) preserved");
    freeMatrix(mc);
}

void testConstructMatrixFromArray() {
    SECTION("constructMatrixFromArray");
    MatrixElement arr[] = {R(1), R(2), R(3), R(4), R(5), R(6)};

    CHECK(constructMatrixFromArray(2, 3, arr, 5) == NULL, "wrong lenData");
    CHECK(constructMatrixFromArray(2, 3, arr, 7) == NULL, "wrong lenData 2");

    Matrix* m = constructMatrixFromArray(2, 3, arr, 6);
    CHECK(m != NULL, "valid construct");
    CHECK(approxReal(getEntry(m, 0, 0), 1, 0), "(0,0)");
    CHECK(approxReal(getEntry(m, 0, 2), 3, 0), "(0,2)");
    CHECK(approxReal(getEntry(m, 1, 0), 4, 0), "(1,0)");
    CHECK(approxReal(getEntry(m, 1, 2), 6, 0), "(1,2)");
    freeMatrix(m);

    // With complex entries
    MatrixElement carr[] = { C(1, 1), R(2), C(0, -1), R(4) };
    Matrix* mc = constructMatrixFromArray(2, 2, carr, 4);
    CHECK(getEntry(mc, 0, 0).isComplex, "complex at (0,0)");
    CHECK(approxElem(getEntry(mc, 0, 0), 1, 1, 0), "1+i at (0,0)");
    CHECK(approxElem(getEntry(mc, 1, 0), 0, -1, 0), "-i at (1,0)");
    freeMatrix(mc);
}

void testPrintMatrix() {
    SECTION("printMatrix");
    long double md[] = {1, 2, 3, 4};
    Matrix* m = makeRealMatrix(2, 2, md);
    printf("  Visual check (should see 2x2 with 1,2,3,4):\n");
    printMatrix(m);
    CHECK(true, "printMatrix doesn't crash");
    freeMatrix(m);

    // Complex printing
    MatrixElement cd[] = { C(1, 1), C(2, -3), R(0), C(0, 1) };
    Matrix* cm = constructMatrixFromArray(2, 2, cd, 4);
    printf("  Visual check complex (should see 1+1i, 2-3i / 0, 0+1i):\n");
    printMatrix(cm);
    CHECK(true, "printMatrix with complex doesn't crash");
    freeMatrix(cm);
}

void testCopyMatrix() {
    SECTION("copyMatrix");
    CHECK(copyMatrix(NULL) == NULL, "copy NULL returns NULL");

    Matrix* m = constructMatrix(2, 3);
    setEntry(m, 0, 0, R(1.5)); setEntry(m, 0, 1, R(2.5)); setEntry(m, 0, 2, R(3.5));
    setEntry(m, 1, 0, R(4.5)); setEntry(m, 1, 1, R(5.5)); setEntry(m, 1, 2, R(6.5));

    Matrix* c = copyMatrix(m);
    CHECK(c != NULL, "copy succeeds");
    CHECK(c != m, "different pointer");
    CHECK(c->data != m->data, "different data buffer");
    CHECK(matrixComp(m, c, 1e-10), "values equal");

    // Modifying copy doesn't affect original
    setEntry(c, 0, 0, R(999));
    CHECK(approxReal(getEntry(m, 0, 0), 1.5, 0), "deep copy");
    freeMatrix(m);
    freeMatrix(c);

    // Copying a complex matrix preserves complex entries
    MatrixElement cd[] = { C(1, 1), R(2), R(3), C(-1, 1) };
    Matrix* cm = constructMatrixFromArray(2, 2, cd, 4);
    Matrix* cc = copyMatrix(cm);
    CHECK(matrixComp(cm, cc, 1e-12), "complex copy equal");
    CHECK(getEntry(cc, 0, 0).isComplex, "complex flag preserved");
    freeMatrix(cm);
    freeMatrix(cc);
}

void testMatrixComp() {
    SECTION("matrixComp");
    CHECK(matrixComp(NULL, NULL, 1e-10) == false, "NULL inputs false");

    Matrix* a = constructMatrix(2, 2);
    Matrix* b = constructMatrix(2, 3);
    CHECK(matrixComp(a, b, 1e-10) == false, "different dims false");
    freeMatrix(b);

    Matrix* c = constructMatrix(2, 2);
    CHECK(matrixComp(a, c, 1e-10), "two zeros equal");

    setEntry(a, 0, 0, R(1.0));
    setEntry(c, 0, 0, R(1.0 + 1e-15));
    CHECK(matrixComp(a, c, 1e-10), "within tol equal");
    setEntry(c, 0, 0, R(1.5));
    CHECK(!matrixComp(a, c, 1e-10), "outside tol not equal");

    // Real 1 and complex 1+0i compare equal
    setEntry(c, 0, 0, C(1.0, 0));  // collapses to real internally
    CHECK(matrixComp(a, c, 1e-10), "stored complex-as-real equals real");

    freeMatrix(a);
    freeMatrix(c);
}

void testIdMatrix() {
    SECTION("idMatrix");
    Matrix* id = idMatrix(4);
    CHECK(id != NULL, "id constructed");
    CHECK(id->numRows == 4 && id->numCols == 4, "dimensions");
    for (int i = 0; i < 4; i++) {
	for (int j = 0; j < 4; j++) {
	    long double exp = (i == j) ? 1.0 : 0.0;
	    if (!approxReal(getEntry(id, i, j), exp, 0)) { CHECK(false, "id values"); freeMatrix(id); return; }
	}
    }
    CHECK(true, "id values correct");
    freeMatrix(id);
}

/* ---------- Operation tests ---------- */

void testSimpleDotProduct() {
    SECTION("simpleDotProduct");
    MatrixElement v[] = {R(1), R(2), R(3)};
    MatrixElement w[] = {R(4), R(5), R(6)};
    CHECK(elemIsNan(simpleDotProduct(v, w, 3, 4)), "length mismatch NAN");
    CHECK(approxReal(simpleDotProduct(v, w, 3, 3), 32.0, 1e-10), "1*4+2*5+3*6 = 32");

    MatrixElement zero[] = {R(0), R(0), R(0)};
    CHECK(approxReal(simpleDotProduct(v, zero, 3, 3), 0.0, 0), "dot with zero");

    // Complex entries: (1+i)(1-i) + (i)(i) = 2 + (-1) = 1
    MatrixElement cv[] = { C(1, 1), C(0, 1) };
    MatrixElement cw[] = { C(1, -1), C(0, 1) };
    MatrixElement result = simpleDotProduct(cv, cw, 2, 2);
    CHECK(approxReal(result, 1.0, 1e-10), "complex dot = 1 (collapses to real)");
}

void testDotProduct() {
    SECTION("dotProduct");
    long double va[] = {1, 2, 3};
    long double wa[] = {4, 5, 6};
    Matrix* v = makeRealVector(3, va);
    Matrix* w = makeRealVector(3, wa);
    CHECK(approxReal(dotProduct(v, w), 32.0, 0), "column dot product");

    Matrix* bad = constructMatrix(3, 2);
    CHECK(elemIsNan(dotProduct(bad, w)), "non-column NAN");
    freeMatrix(bad);

    Matrix* mismatch = constructMatrix(4, 1);
    CHECK(elemIsNan(dotProduct(v, mismatch)), "row mismatch NAN");
    freeMatrix(mismatch);
    freeMatrix(v);
    freeMatrix(w);
}

void testMultByConstant() {
    SECTION("multByConstant");
    long double a[] = {1, 2, 3, 4};
    Matrix* m = makeRealMatrix(2, 2, a);
    Matrix* r = multByConstant(m, R(2.5));
    CHECK(approxReal(getEntry(r, 0, 0), 2.5, 1e-12), "(0,0) scaled");
    CHECK(approxReal(getEntry(r, 1, 1), 10.0, 1e-12), "(1,1) scaled");
    CHECK(approxReal(getEntry(m, 0, 0), 1, 0), "original unchanged");

    Matrix* z = multByConstant(m, R(0));
    for (int i = 0; i < 4; i++) {
	if (!approxReal(z->data[i], 0, 0)) { CHECK(false, "mult by 0"); freeMatrix(z); freeMatrix(r); freeMatrix(m); return; }
    }
    CHECK(true, "mult by 0 zeros all");

    // Multiplying a real matrix by i gives a fully complex matrix
    Matrix* ir = multByConstant(m, C(0, 1));
    CHECK(getEntry(ir, 0, 0).isComplex, "mult by i makes entries complex");
    CHECK(approxElem(getEntry(ir, 0, 0), 0, 1, 1e-12), "1*i = i");
    CHECK(approxElem(getEntry(ir, 1, 1), 0, 4, 1e-12), "4*i = 4i");
    freeMatrix(ir);

    freeMatrix(m);
    freeMatrix(r);
    freeMatrix(z);
}

void testAddMatrices() {
    SECTION("addMatrices");
    CHECK(addMatrices(NULL, NULL) == NULL, "NULL inputs");

    Matrix* a = constructMatrix(2, 3);
    Matrix* b = constructMatrix(3, 2);
    CHECK(addMatrices(a, b) == NULL, "dim mismatch");
    freeMatrix(b);

    long double ad[] = {1, 2, 3, 4};
    long double bd[] = {5, 6, 7, 8};
    Matrix* A = makeRealMatrix(2, 2, ad);
    Matrix* B = makeRealMatrix(2, 2, bd);
    Matrix* S = addMatrices(A, B);
    CHECK(approxReal(getEntry(S, 0, 0), 6, 0) && approxReal(getEntry(S, 1, 1), 12, 0), "addition correct");

    // Complex + real = complex
    MatrixElement cd[] = { C(1, 1), R(0), R(0), C(0, 1) };
    Matrix* Cmat = constructMatrixFromArray(2, 2, cd, 4);
    Matrix* SC = addMatrices(A, Cmat);
    CHECK(getEntry(SC, 0, 0).isComplex, "real+complex -> complex");
    CHECK(approxElem(getEntry(SC, 0, 0), 2, 1, 1e-12), "1 + (1+i) = 2+i");
    freeMatrix(Cmat);
    freeMatrix(SC);

    freeMatrix(a);
    freeMatrix(A);
    freeMatrix(B);
    freeMatrix(S);
}

void testSubtractMatrices() {
    SECTION("subtractMatrices");
    CHECK(subtractMatrices(NULL, NULL) == NULL, "NULL inputs");

    long double ad[] = {5, 6, 7, 8};
    long double bd[] = {1, 2, 3, 4};
    Matrix* A = makeRealMatrix(2, 2, ad);
    Matrix* B = makeRealMatrix(2, 2, bd);
    Matrix* D = subtractMatrices(A, B);
    CHECK(approxReal(getEntry(D, 0, 0), 4, 0) && approxReal(getEntry(D, 1, 1), 4, 0), "subtraction correct");

    Matrix* Z = subtractMatrices(A, A);
    for (int i = 0; i < 4; i++) {
	if (!approxReal(Z->data[i], 0, 0)) { CHECK(false, "self-sub zero"); freeMatrix(Z); freeMatrix(D); freeMatrix(A); freeMatrix(B); return; }
    }
    CHECK(true, "A - A = 0");

    freeMatrix(A);
    freeMatrix(B);
    freeMatrix(D);
    freeMatrix(Z);
}

void testMultiplyMatrices() {
    SECTION("multiplyMatrices");
    CHECK(multiplyMatrices(NULL, NULL) == NULL, "NULL inputs");

    Matrix* a = constructMatrix(2, 3);
    Matrix* b = constructMatrix(4, 2);
    CHECK(multiplyMatrices(a, b) == NULL, "incompatible dims");
    freeMatrix(a);
    freeMatrix(b);

    // Multiply by identity
    long double ad[] = {1, 2, 3, 4};
    Matrix* A = makeRealMatrix(2, 2, ad);
    Matrix* I = idMatrix(2);
    Matrix* AI = multiplyMatrices(A, I);
    CHECK(matrixComp(A, AI, 1e-10), "A * I = A");

    // Concrete product
    long double xd[] = {1, 2, 3, 4, 5, 6};  // 2x3
    long double yd[] = {7, 8, 9, 10, 11, 12};  // 3x2
    Matrix* X = makeRealMatrix(2, 3, xd);
    Matrix* Y = makeRealMatrix(3, 2, yd);
    Matrix* P = multiplyMatrices(X, Y);
    CHECK(P->numRows == 2 && P->numCols == 2, "product dims");
    CHECK(approxReal(getEntry(P, 0, 0), 58, 0), "(0,0) = 1*7+2*9+3*11");
    CHECK(approxReal(getEntry(P, 0, 1), 64, 0), "(0,1) = 1*8+2*10+3*12");
    CHECK(approxReal(getEntry(P, 1, 0), 139, 0), "(1,0) = 4*7+5*9+6*11");
    CHECK(approxReal(getEntry(P, 1, 1), 154, 0), "(1,1) = 4*8+5*10+6*12");

    // Complex multiplication: [[i, 0], [0, i]] * [[i, 0], [0, i]] = -I
    MatrixElement iI_d[] = { C(0, 1), R(0), R(0), C(0, 1) };
    Matrix* iI = constructMatrixFromArray(2, 2, iI_d, 4);
    Matrix* negI = multiplyMatrices(iI, iI);
    CHECK(approxReal(getEntry(negI, 0, 0), -1, 1e-12), "(iI)^2 (0,0) = -1");
    CHECK(approxReal(getEntry(negI, 1, 1), -1, 1e-12), "(iI)^2 (1,1) = -1");
    CHECK(approxReal(getEntry(negI, 0, 1), 0, 1e-12), "(iI)^2 off-diag = 0");
    freeMatrix(iI);
    freeMatrix(negI);

    freeMatrix(A);
    freeMatrix(I);
    freeMatrix(AI);
    freeMatrix(X);
    freeMatrix(Y);
    freeMatrix(P);
}

void testTensorMatrices() {
    SECTION("tensorMatrices");
    CHECK(tensorMatrices(NULL, NULL) == NULL, "NULL inputs");

    long double ad[] = {1, 2, 3, 4};  // 2x2
    long double bd[] = {0, 5, 6, 7};  // 2x2
    Matrix* A = makeRealMatrix(2, 2, ad);
    Matrix* B = makeRealMatrix(2, 2, bd);
    Matrix* T = tensorMatrices(A, B);
    CHECK(T->numRows == 4 && T->numCols == 4, "tensor dims 4x4");
    // Top-left block = 1 * B
    CHECK(approxReal(getEntry(T, 0, 0), 0, 0) && approxReal(getEntry(T, 0, 1), 5, 0), "block (0,0)");
    CHECK(approxReal(getEntry(T, 1, 0), 6, 0) && approxReal(getEntry(T, 1, 1), 7, 0), "block (0,0) row 2");
    // Top-right block = 2 * B
    CHECK(approxReal(getEntry(T, 0, 2), 0, 0) && approxReal(getEntry(T, 0, 3), 10, 0), "block (0,1)");
    // Bottom-right = 4 * B
    CHECK(approxReal(getEntry(T, 3, 3), 28, 0), "block (1,1) corner");

    // Tensor with identity preserves original-ish shape
    Matrix* I1 = idMatrix(1);
    Matrix* TI = tensorMatrices(I1, A);
    CHECK(matrixComp(TI, A, 1e-10), "I_1 ⊗ A = A");

    freeMatrix(A);
    freeMatrix(B);
    freeMatrix(T);
    freeMatrix(I1);
    freeMatrix(TI);
}

void testTranspose() {
    SECTION("transpose");
    long double arr[] = {1, 2, 3, 4, 5, 6};
    Matrix* m = makeRealMatrix(2, 3, arr);
    Matrix* t = transpose(m);
    CHECK(t->numRows == 3 && t->numCols == 2, "transpose dims");
    CHECK(approxReal(getEntry(t, 0, 0), 1, 0), "(0,0)");
    CHECK(approxReal(getEntry(t, 1, 0), 2, 0), "(1,0)");
    CHECK(approxReal(getEntry(t, 2, 1), 6, 0), "(2,1)");

    // Double transpose returns original
    Matrix* tt = transpose(t);
    CHECK(matrixComp(m, tt, 1e-10), "(A^T)^T = A");

    freeMatrix(m);
    freeMatrix(t);
    freeMatrix(tt);
}

void testAdjoint() {
    SECTION("adjoint");
    CHECK(adjoint(NULL) == NULL, "NULL returns NULL");

    // Real matrices: adjoint equals transpose
    long double rd[] = {1, 2, 3, 4, 5, 6};
    Matrix* Rm = makeRealMatrix(2, 3, rd);
    Matrix* Rt = transpose(Rm);
    Matrix* Ra = adjoint(Rm);
    CHECK(matrixComp(Ra, Rt, 1e-12), "real adjoint = transpose");
    freeMatrix(Rm); freeMatrix(Rt); freeMatrix(Ra);

    // Complex matrix: conjugate transpose
    MatrixElement cd[] = { C(1, 2), C(3, -4), R(5), C(-2, 1) };
    Matrix* Cm = constructMatrixFromArray(2, 2, cd, 4);
    Matrix* Ca = adjoint(Cm);
    CHECK(approxElem(getEntry(Ca, 0, 0), 1.0, -2.0, 1e-12), "(0,0) conjugated");
    CHECK(approxElem(getEntry(Ca, 0, 1), 5.0, 0.0, 1e-12), "(0,1) transposed real");
    CHECK(approxElem(getEntry(Ca, 1, 0), 3.0, 4.0, 1e-12), "(1,0) transposed+conjugated");
    CHECK(approxElem(getEntry(Ca, 1, 1), -2.0, -1.0, 1e-12), "(1,1) conjugated");
    freeMatrix(Cm); freeMatrix(Ca);
}

/* ---------- Solve / inverse tests ---------- */

void testLuSolve() {
    SECTION("luSolve");
    CHECK(luSolve(NULL, NULL) == NULL, "NULL inputs");

    // Solve Ax = b for known A, b
    long double ad[] = {4, 3, 6, 3};
    Matrix* A = makeRealMatrix(2, 2, ad);
    LU* lu = luDecompose(A);

    long double bd[] = {10, 12};
    Matrix* b = makeRealVector(2, bd);
    Matrix* x = luSolve(lu, b);
    // Verify Ax = b
    Matrix* check = multiplyMatrices(A, x);
    CHECK(matrixComp(check, b, 1e-9), "Ax = b verified");

    // Wrong dim b
    Matrix* bad = constructMatrix(3, 1);
    CHECK(luSolve(lu, bad) == NULL, "wrong dim b returns NULL");

    freeMatrix(A);
    freeMatrix(b);
    freeMatrix(x);
    freeMatrix(check);
    freeMatrix(bad);
    freeLU(lu);
}

void testLuInverse() {
    SECTION("luInverse");
    CHECK(luInverse(NULL) == NULL, "NULL input");

    // Inverse of identity is identity
    Matrix* id = idMatrix(4);
    LU* lu = luDecompose(id);
    Matrix* inv = luInverse(lu);
    CHECK(matrixComp(inv, id, 1e-10), "inv(I) = I");
    freeMatrix(inv);
    freeLU(lu);

    // A * inv(A) = I for non-trivial A
    long double ad[] = {4, 3, 6, 3};
    Matrix* A = makeRealMatrix(2, 2, ad);
    LU* lu2 = luDecompose(A);
    Matrix* invA = luInverse(lu2);
    Matrix* prod = multiplyMatrices(A, invA);
    Matrix* I2 = idMatrix(2);
    CHECK(matrixComp(prod, I2, 1e-9), "A * inv(A) = I (2x2)");
    freeMatrix(A); freeMatrix(invA); freeMatrix(prod); freeMatrix(I2);
    freeLU(lu2);

    // 4x4 case
    long double bd[] = {
	2, 1, 0, 0,
	1, 2, 1, 0,
	0, 1, 2, 1,
	0, 0, 1, 2
    };
    Matrix* B = makeRealMatrix(4, 4, bd);
    LU* lu3 = luDecompose(B);
    Matrix* invB = luInverse(lu3);
    Matrix* prod2 = multiplyMatrices(B, invB);
    Matrix* I4 = idMatrix(4);
    CHECK(matrixComp(prod2, I4, 1e-9), "B * inv(B) = I (4x4)");
    freeMatrix(B); freeMatrix(invB); freeMatrix(prod2); freeMatrix(I4);
    freeLU(lu3);

    freeMatrix(id);
}

/* ---------- High-level tests ---------- */

void testDeterminant() {
    SECTION("determinant");
    Matrix* nsq = constructMatrix(2, 3);
    CHECK(elemIsNan(determinant(nsq)), "non-square NAN");
    freeMatrix(nsq);

    // 1x1 fast path
    long double a1[] = {7.5};
    Matrix* m1 = makeRealMatrix(1, 1, a1);
    CHECK(approxReal(determinant(m1), 7.5, 0), "1x1 det");
    freeMatrix(m1);

    // 2x2 fast path
    long double a2[] = {3, 8, 4, 6};
    Matrix* m2 = makeRealMatrix(2, 2, a2);
    CHECK(approxReal(determinant(m2), -14.0, 1e-10), "2x2 det");
    freeMatrix(m2);

    // 3x3 via LU
    long double a3[] = {6, 1, 1, 4, -2, 5, 2, 8, 7};
    Matrix* m3 = makeRealMatrix(3, 3, a3);
    CHECK(approxReal(determinant(m3), -306.0, 1e-9), "3x3 det");
    freeMatrix(m3);

    // Identity
    Matrix* id = idMatrix(5);
    CHECK(approxReal(determinant(id), 1.0, 1e-10), "det(I_5) = 1");
    freeMatrix(id);

    // Complex 2x2 det: [[i, 1], [0, i]] det = i*i - 0 = -1
    MatrixElement cd[] = { C(0, 1), R(1), R(0), C(0, 1) };
    Matrix* cm = constructMatrixFromArray(2, 2, cd, 4);
    CHECK(approxReal(determinant(cm), -1.0, 1e-10), "det([[i,1],[0,i]]) = -1");
    freeMatrix(cm);
}

void testSolveLinEq() {
    SECTION("solveLinEq");
    CHECK(solveLinEq(NULL, NULL) == NULL, "NULL inputs");

    long double ad[] = {3, 2, 1, 2};
    Matrix* A = makeRealMatrix(2, 2, ad);
    long double bd[] = {5, 5};
    Matrix* b = makeRealVector(2, bd);
    Matrix* x = solveLinEq(A, b);
    CHECK(x != NULL, "solve returned");
    Matrix* check = multiplyMatrices(A, x);
    CHECK(matrixComp(check, b, 1e-9), "Ax = b");
    freeMatrix(check);
    freeMatrix(x);

    // Solve again — should use cached LU
    Matrix* x2 = solveLinEq(A, b);
    CHECK(x2 != NULL, "second solve works");
    freeMatrix(x2);

    // Bad dimensions
    Matrix* badB = constructMatrix(3, 1);
    CHECK(solveLinEq(A, badB) == NULL, "wrong rows in b");
    freeMatrix(badB);

    Matrix* multiCol = constructMatrix(2, 2);
    CHECK(solveLinEq(A, multiCol) == NULL, "b not column vec");
    freeMatrix(multiCol);

    Matrix* nsq = constructMatrix(2, 3);
    CHECK(solveLinEq(nsq, b) == NULL, "non-square A");
    freeMatrix(nsq);

    freeMatrix(A);
    freeMatrix(b);
}

void testInvertMatrix() {
    SECTION("invertMatrix");
    Matrix* nsq = constructMatrix(2, 3);
    CHECK(invertMatrix(nsq) == NULL, "non-square NULL");
    freeMatrix(nsq);

    // Singular
    long double sd[] = {1, 2, 2, 4};
    Matrix* sing = makeRealMatrix(2, 2, sd);
    CHECK(invertMatrix(sing) == NULL, "singular NULL");
    freeMatrix(sing);

    // Real inversion
    long double ad[] = {4, 7, 2, 6};
    Matrix* A = makeRealMatrix(2, 2, ad);
    Matrix* invA = invertMatrix(A);
    CHECK(invA != NULL, "inverse computed");
    Matrix* prod = multiplyMatrices(A, invA);
    Matrix* I = idMatrix(2);
    CHECK(matrixComp(prod, I, 1e-9), "A * inv(A) = I");
    freeMatrix(A); freeMatrix(invA); freeMatrix(prod); freeMatrix(I);

    // Larger case — use inverse to solve a linear system, cross-check
    long double bd[] = {
	1, 2, 3,
	0, 1, 4,
	5, 6, 0
    };
    Matrix* B = makeRealMatrix(3, 3, bd);
    Matrix* invB = invertMatrix(B);
    Matrix* prod2 = multiplyMatrices(B, invB);
    Matrix* I3 = idMatrix(3);
    CHECK(matrixComp(prod2, I3, 1e-9), "3x3 B * inv(B) = I");
    freeMatrix(B); freeMatrix(invB); freeMatrix(prod2); freeMatrix(I3);
}

/* ---------- New function tests ---------- */

void testApplyMatrix() {
    SECTION("applyMatrix");
    CHECK(applyMatrix(NULL, NULL) == NULL, "NULL inputs");

    Matrix* A = constructMatrix(2, 3);
    Matrix* badV = constructMatrix(2, 1);
    CHECK(applyMatrix(A, badV) == NULL, "col mismatch returns NULL");
    freeMatrix(badV);

    Matrix* notCol = constructMatrix(3, 2);
    CHECK(applyMatrix(A, notCol) == NULL, "non-column v returns NULL");
    freeMatrix(notCol);
    freeMatrix(A);

    // Identity application
    long double id2d[] = {1, 0, 0, 1};
    Matrix* I2 = makeRealMatrix(2, 2, id2d);
    long double vd[] = {3, 4};
    Matrix* v = makeRealVector(2, vd);
    Matrix* r = applyMatrix(I2, v);
    CHECK(r != NULL, "apply succeeds");
    CHECK(approxReal(getEntry(r, 0, 0), 3.0, 1e-10) && approxReal(getEntry(r, 1, 0), 4.0, 1e-10), "I*v = v");
    freeMatrix(r);
    freeMatrix(I2);
    freeMatrix(v);

    // Diagonal 2x2
    long double ad[] = {2, 0, 0, 3};
    Matrix* D = makeRealMatrix(2, 2, ad);
    long double v2d[] = {1, 2};
    Matrix* v2 = makeRealVector(2, v2d);
    Matrix* r2 = applyMatrix(D, v2);
    CHECK(approxReal(getEntry(r2, 0, 0), 2.0, 1e-10) && approxReal(getEntry(r2, 1, 0), 6.0, 1e-10), "diag * v correct");
    freeMatrix(D); freeMatrix(v2); freeMatrix(r2);

    // Non-square A: 2x3
    long double nsad[] = {1, 2, 3, 4, 5, 6};
    Matrix* nsA = makeRealMatrix(2, 3, nsad);
    long double v3d[] = {1, 0, 1};
    Matrix* v3 = makeRealVector(3, v3d);
    Matrix* r3 = applyMatrix(nsA, v3);
    CHECK(r3 != NULL && r3->numRows == 2 && r3->numCols == 1, "non-square result shape");
    CHECK(approxReal(getEntry(r3, 0, 0), 4.0, 1e-10) && approxReal(getEntry(r3, 1, 0), 10.0, 1e-10), "non-square Av correct");
    freeMatrix(nsA); freeMatrix(v3); freeMatrix(r3);

    // Complex A and complex v: A = [[i, 0], [0, 1]], v = [1, i] → Av = [i, i]
    MatrixElement Ad[] = { C(0, 1), R(0), R(0), R(1) };
    Matrix* cA = constructMatrixFromArray(2, 2, Ad, 4);
    MatrixElement vd2[] = { R(1), C(0, 1) };
    Matrix* cv = constructMatrixFromArray(2, 1, vd2, 2);
    Matrix* cr = applyMatrix(cA, cv);
    CHECK(approxElem(getEntry(cr, 0, 0), 0, 1, 1e-12), "complex Av[0] = i");
    CHECK(approxElem(getEntry(cr, 1, 0), 0, 1, 1e-12), "complex Av[1] = i");
    freeMatrix(cA); freeMatrix(cv); freeMatrix(cr);
}

void testMatrixPow() {
    SECTION("matrixPow");
    CHECK(matrixPow(NULL, 2) == NULL, "NULL input");

    Matrix* nsq = constructMatrix(2, 3);
    CHECK(matrixPow(nsq, 2) == NULL, "non-square returns NULL");
    freeMatrix(nsq);

    long double ad[] = {1, 2, 3, 4};
    Matrix* A = makeRealMatrix(2, 2, ad);

    // A^0 = I
    Matrix* A0 = matrixPow(A, 0);
    Matrix* I2 = idMatrix(2);
    CHECK(matrixComp(A0, I2, 1e-10), "A^0 = I");
    freeMatrix(A0); freeMatrix(I2);

    // A^1 = A
    Matrix* A1 = matrixPow(A, 1);
    CHECK(matrixComp(A1, A, 1e-10), "A^1 = A");
    freeMatrix(A1);

    // A^2 = A*A
    Matrix* A2 = matrixPow(A, 2);
    Matrix* AA = multiplyMatrices(A, A);
    CHECK(matrixComp(A2, AA, 1e-10), "A^2 = A*A");
    freeMatrix(A2); freeMatrix(AA);

    // A^(-1) matches invertMatrix
    Matrix* Am1 = matrixPow(A, -1);
    Matrix* invA = invertMatrix(A);
    CHECK(matrixComp(Am1, invA, 1e-9), "A^(-1) = inv(A)");
    freeMatrix(invA);

    // A^2 * A^(-2) = I
    Matrix* A2b = matrixPow(A, 2);
    Matrix* Am2 = matrixPow(A, -2);
    Matrix* prod = multiplyMatrices(A2b, Am2);
    Matrix* I2b = idMatrix(2);
    CHECK(matrixComp(prod, I2b, 1e-9), "A^2 * A^-2 = I");
    freeMatrix(A2b); freeMatrix(Am1); freeMatrix(Am2); freeMatrix(prod); freeMatrix(I2b);
    freeMatrix(A);
}

void testIsSymmetric() {
    SECTION("isSymmetric");
    CHECK(isSymmetric(NULL) == false, "NULL returns false");

    Matrix* nsq = constructMatrix(2, 3);
    CHECK(isSymmetric(nsq) == false, "non-square returns false");
    freeMatrix(nsq);

    // Symmetric
    long double sd[] = {1, 2, 3, 2, 5, 6, 3, 6, 9};
    Matrix* S = makeRealMatrix(3, 3, sd);
    CHECK(isSymmetric(S) == true, "symmetric 3x3");
    freeMatrix(S);

    // Non-symmetric
    long double nd[] = {1, 2, 3, 4};
    Matrix* N = makeRealMatrix(2, 2, nd);
    CHECK(isSymmetric(N) == false, "non-symmetric 2x2");
    freeMatrix(N);

    Matrix* id = idMatrix(4);
    CHECK(isSymmetric(id) == true, "identity is symmetric");
    freeMatrix(id);

    Matrix* zero = constructMatrix(3, 3);
    CHECK(isSymmetric(zero) == true, "zero matrix is symmetric");
    freeMatrix(zero);

    // Complex symmetric (A = A^T, not Hermitian)
    MatrixElement cd[] = { C(1, 1), C(2, 0), C(2, 0), C(3, -1) };
    Matrix* Cs = constructMatrixFromArray(2, 2, cd, 4);
    CHECK(isSymmetric(Cs) == true, "complex symmetric detected");
    freeMatrix(Cs);
}

void testIsAntisymmetric() {
    SECTION("isAntisymmetric");
    CHECK(isAntisymmetric(NULL) == false, "NULL returns false");

    Matrix* nsq = constructMatrix(2, 3);
    CHECK(isAntisymmetric(nsq) == false, "non-square returns false");
    freeMatrix(nsq);

    // Antisymmetric [[0,1],[-1,0]]
    long double ad[] = {0, 1, -1, 0};
    Matrix* A = makeRealMatrix(2, 2, ad);
    CHECK(isAntisymmetric(A) == true, "[[0,1],[-1,0]] antisymmetric");
    freeMatrix(A);

    // Zero matrix is antisymmetric
    Matrix* zero = constructMatrix(3, 3);
    CHECK(isAntisymmetric(zero) == true, "zero matrix is antisymmetric");
    freeMatrix(zero);

    // Identity is not antisymmetric
    Matrix* id = idMatrix(3);
    CHECK(isAntisymmetric(id) == false, "identity is not antisymmetric");
    freeMatrix(id);

    // 3x3 antisymmetric
    long double a3d[] = {0, 2, -3, -2, 0, 1, 3, -1, 0};
    Matrix* A3 = makeRealMatrix(3, 3, a3d);
    CHECK(isAntisymmetric(A3) == true, "3x3 antisymmetric");
    freeMatrix(A3);
}

void testIsOrthogonal() {
    SECTION("isOrthogonal");
    CHECK(isOrthogonal(NULL) == false, "NULL returns false");

    Matrix* nsq = constructMatrix(2, 3);
    CHECK(isOrthogonal(nsq) == false, "non-square returns false");
    freeMatrix(nsq);

    Matrix* id = idMatrix(3);
    CHECK(isOrthogonal(id) == true, "identity is orthogonal");
    freeMatrix(id);

    // 90-degree rotation
    long double c = 0.0, s = 1.0;
    long double rotd[] = {c, -s, s, c};
    Matrix* R_ = makeRealMatrix(2, 2, rotd);
    CHECK(isOrthogonal(R_) == true, "90 deg rotation is orthogonal");
    freeMatrix(R_);

    // 45-degree rotation
    long double c45 = sqrtl(2.0) / 2.0;
    long double rot45d[] = {c45, -c45, c45, c45};
    Matrix* R45 = makeRealMatrix(2, 2, rot45d);
    CHECK(isOrthogonal(R45) == true, "45 deg rotation is orthogonal");
    freeMatrix(R45);

    // Non-orthogonal
    long double nd[] = {2, 0, 0, 1};
    Matrix* N = makeRealMatrix(2, 2, nd);
    CHECK(isOrthogonal(N) == false, "scaling matrix not orthogonal");
    freeMatrix(N);
}

void testIsUnitary() {
    SECTION("isUnitary");
    CHECK(isUnitary(NULL) == false, "NULL returns false");

    Matrix* nsq = constructMatrix(2, 3);
    CHECK(isUnitary(nsq) == false, "non-square returns false");
    freeMatrix(nsq);

    Matrix* id = idMatrix(3);
    CHECK(isUnitary(id) == true, "identity is unitary");
    freeMatrix(id);

    // Real orthogonal matrices are unitary
    long double c45 = sqrtl(2.0) / 2.0;
    long double rot45d[] = {c45, -c45, c45, c45};
    Matrix* R45 = makeRealMatrix(2, 2, rot45d);
    CHECK(isUnitary(R45) == true, "real rotation is unitary");
    freeMatrix(R45);

    // Complex diagonal with unit-modulus entries is unitary
    MatrixElement ud[] = { C(0, 1), R(0), R(0), C(0, -1) };
    Matrix* U = constructMatrixFromArray(2, 2, ud, 4);
    CHECK(isUnitary(U) == true, "diag(i,-i) is unitary");
    freeMatrix(U);

    // Non-unit-modulus entry breaks unitarity
    MatrixElement nd[] = { C(2, 0), R(0), R(0), R(1) };
    Matrix* N = constructMatrixFromArray(2, 2, nd, 4);
    CHECK(isUnitary(N) == false, "diag(2,1) is not unitary");
    freeMatrix(N);
}

void testRank() {
    SECTION("rank");
    CHECK(rank(NULL) == -1, "NULL returns -1");

    Matrix* id3 = idMatrix(3);
    CHECK(rank(id3) == 3, "rank(I_3) = 3");
    freeMatrix(id3);

    Matrix* zero = constructMatrix(3, 3);
    CHECK(rank(zero) == 0, "rank(zero 3x3) = 0");
    freeMatrix(zero);

    // Rank-deficient square: [[1,2],[2,4]] has rank 1
    long double rd[] = {1, 2, 2, 4};
    Matrix* Rm = makeRealMatrix(2, 2, rd);
    CHECK(rank(Rm) == 1, "rank-deficient 2x2 = 1");
    freeMatrix(Rm);

    // Square with one zero row: rank 2
    long double r3d[] = {1, 0, 0, 0, 1, 0, 0, 0, 0};
    Matrix* R3 = makeRealMatrix(3, 3, r3d);
    CHECK(rank(R3) == 2, "3x3 rank 2");
    freeMatrix(R3);

    // Non-square full rank: 2x3
    long double nsd[] = {1, 0, 0, 0, 1, 0};
    Matrix* NS = makeRealMatrix(2, 3, nsd);
    CHECK(rank(NS) == 2, "2x3 full row rank = 2");
    freeMatrix(NS);

    // Non-square rank-deficient: 3x2 rank 1
    long double rnd[] = {1, 2, 2, 4, 3, 6};
    Matrix* RN = makeRealMatrix(3, 2, rnd);
    CHECK(rank(RN) == 1, "3x2 rank 1");
    freeMatrix(RN);

    // Complex full-rank 2x2 (det = 1, non-zero)
    MatrixElement cd[] = { C(1, 1), R(0), R(0), R(1) };
    Matrix* Cm = constructMatrixFromArray(2, 2, cd, 4);
    CHECK(rank(Cm) == 2, "complex full rank 2");
    freeMatrix(Cm);
}

void testNullity() {
    SECTION("nullity");
    CHECK(nullity(NULL) == -1, "NULL returns -1");

    Matrix* id3 = idMatrix(3);
    CHECK(nullity(id3) == 0, "nullity(I_3) = 0");
    freeMatrix(id3);

    // [[1,2],[2,4]]: rank 1, nullity = 2 - 1 = 1
    long double rd[] = {1, 2, 2, 4};
    Matrix* Rm = makeRealMatrix(2, 2, rd);
    CHECK(nullity(Rm) == 1, "nullity of rank-1 2x2 = 1");
    freeMatrix(Rm);

    // 2x3 full row rank: nullity = 3 - 2 = 1
    long double nsd[] = {1, 0, 0, 0, 1, 0};
    Matrix* NS = makeRealMatrix(2, 3, nsd);
    CHECK(nullity(NS) == 1, "2x3 nullity = 1");
    freeMatrix(NS);

    Matrix* zero = constructMatrix(3, 4);
    CHECK(nullity(zero) == 4, "zero 3x4: nullity = 4");
    freeMatrix(zero);
}

void testTrace() {
    SECTION("trace");
    CHECK(elemIsNan(trace(NULL)), "NULL returns NAN");

    Matrix* nsq = constructMatrix(2, 3);
    CHECK(elemIsNan(trace(nsq)), "non-square returns NAN");
    freeMatrix(nsq);

    Matrix* id4 = idMatrix(4);
    CHECK(approxReal(trace(id4), 4.0, 1e-10), "trace(I_4) = 4");
    freeMatrix(id4);

    long double ad[] = {1, 2, 3, 4};
    Matrix* A = makeRealMatrix(2, 2, ad);
    CHECK(approxReal(trace(A), 5.0, 1e-10), "trace([[1,2],[3,4]]) = 5");
    freeMatrix(A);

    // Float entries: verify no integer truncation
    long double fd[] = {1.5, 0, 0, 2.5};
    Matrix* F = makeRealMatrix(2, 2, fd);
    CHECK(approxReal(trace(F), 4.0, 1e-10), "trace with float diagonal = 4.0");
    freeMatrix(F);

    // Trace equals sum of eigenvalues (2x2 check)
    long double ed[] = {3, 1, 0, 5};
    Matrix* E = makeRealMatrix(2, 2, ed);
    MatrixElement* eigs = eigenvalues2x2(E);
    MatrixElement eigsum = elemAdd(eigs[0], eigs[1]);
    CHECK(elemEq(trace(E), eigsum, 1e-9), "trace = sum of eigenvalues");
    freeMatrix(E); free(eigs);

    // Trace of complex matrix
    MatrixElement cd[] = { C(1, 1), R(0), R(0), C(2, -1) };
    Matrix* Cm = constructMatrixFromArray(2, 2, cd, 4);
    MatrixElement ct = trace(Cm);
    CHECK(approxReal(ct, 3.0, 1e-12), "complex diag sum collapses to 3");
    freeMatrix(Cm);
}

void testFrobeniusNorm() {
    SECTION("frobeniusNorm");
    CHECK(isnan(frobeniusNorm(NULL)), "NULL returns NAN");

    Matrix* zero = constructMatrix(3, 3);
    CHECK(approx(frobeniusNorm(zero), 0.0, 1e-10), "zero matrix norm = 0");
    freeMatrix(zero);

    Matrix* id3 = idMatrix(3);
    CHECK(approx(frobeniusNorm(id3), sqrtl(3.0), 1e-10), "||I_3||_F = sqrtl(3)");
    freeMatrix(id3);

    // [[1,2],[3,4]]: sqrtl(1+4+9+16) = sqrtl(30)
    long double ad[] = {1, 2, 3, 4};
    Matrix* A = makeRealMatrix(2, 2, ad);
    CHECK(approx(frobeniusNorm(A), sqrtl(30.0), 1e-10), "||[[1,2],[3,4]]||_F = sqrtl(30)");
    freeMatrix(A);

    // Non-square works: 1x3 [3,4,0] → norm = 5
    long double vd[] = {3, 4, 0};
    Matrix* V = makeRealMatrix(1, 3, vd);
    CHECK(approx(frobeniusNorm(V), 5.0, 1e-10), "1x3 [3,4,0] norm = 5");
    freeMatrix(V);

    // Complex matrix: 2x2 with entries 1+i, 0, 0, 1-i → sum |a|^2 = 2+0+0+2 = 4 → norm = 2
    MatrixElement cd[] = { C(1, 1), R(0), R(0), C(1, -1) };
    Matrix* Cm = constructMatrixFromArray(2, 2, cd, 4);
    CHECK(approx(frobeniusNorm(Cm), 2.0, 1e-10), "complex Frobenius uses |a|^2");
    freeMatrix(Cm);
}

void testEigenvalues2x2() {
    SECTION("eigenvalues2x2");
    CHECK(eigenvalues2x2(NULL) == NULL, "NULL returns NULL");

    Matrix* nsq = constructMatrix(3, 3);
    CHECK(eigenvalues2x2(nsq) == NULL, "non-2x2 returns NULL");
    freeMatrix(nsq);

    // Identity: both eigenvalues = 1
    Matrix* id = idMatrix(2);
    MatrixElement* eig_id = eigenvalues2x2(id);
    CHECK(approxReal(eig_id[0], 1.0, 1e-10) &&
          approxReal(eig_id[1], 1.0, 1e-10), "eigs(I_2) = {1, 1}");
    freeMatrix(id); free(eig_id);

    // Diagonal [[3,0],[0,2]]: eigs = {3, 2}
    long double dd[] = {3, 0, 0, 2};
    Matrix* D = makeRealMatrix(2, 2, dd);
    MatrixElement* eig_d = eigenvalues2x2(D);
    CHECK(approxReal(eig_d[0], 3.0, 1e-10) &&
          approxReal(eig_d[1], 2.0, 1e-10), "eigs(diag(3,2)) = {3, 2}");
    free(eig_d);

    // [[5,2],[2,5]]: eigs = {7, 3}; verify via trace/det
    long double sd[] = {5, 2, 2, 5};
    Matrix* S = makeRealMatrix(2, 2, sd);
    MatrixElement* eig_s = eigenvalues2x2(S);
    MatrixElement esum = elemAdd(eig_s[0], eig_s[1]);
    MatrixElement eprod = elemMul(eig_s[0], eig_s[1]);
    CHECK(elemEq(esum, trace(S), 1e-9), "sum of eigs = trace");
    CHECK(elemEq(eprod, determinant(S), 1e-9), "product of eigs = det");
    freeMatrix(S); free(eig_s);
    freeMatrix(D);

    // Complex eigenvalues: [[0,-1],[1,0]] (rotation 90 deg)  →  eigs = ±i
    long double cd[] = {0, -1, 1, 0};
    Matrix* Cr = makeRealMatrix(2, 2, cd);
    MatrixElement* eig_c = eigenvalues2x2(Cr);
    CHECK(eig_c[0].isComplex && eig_c[1].isComplex, "eigs are complex");
    // Eigenvalues are pure imaginary ±i
    CHECK(approxElem(eig_c[0], 0.0, 1.0, 1e-9) || approxElem(eig_c[0], 0.0, -1.0, 1e-9),
          "eig0 is +/- i");
    CHECK(approxElem(eig_c[1], 0.0, 1.0, 1e-9) || approxElem(eig_c[1], 0.0, -1.0, 1e-9),
          "eig1 is +/- i");
    // Eigs are complex conjugates: imag parts sum to 0
    ComplexNumber e0 = eig_c[0].value.complex;
    ComplexNumber e1 = eig_c[1].value.complex;
    CHECK(approx(e0.imag + e1.imag, 0.0, 1e-9), "complex eigs are conjugates");
    freeMatrix(Cr); free(eig_c);

    // Verify each eigenvalue (real or complex) satisfies det(A - lambda*I) = 0
    long double vd[] = {4, 1, 2, 3};
    Matrix* V = makeRealMatrix(2, 2, vd);
    MatrixElement* eig_v = eigenvalues2x2(V);
    for (int k = 0; k < 2; k++) {
        MatrixElement lam = eig_v[k];
        Matrix* I = idMatrix(2);
        Matrix* lI = multByConstant(I, lam);
        Matrix* AmL = subtractMatrices(V, lI);
        CHECK(approx(elemAbs(determinant(AmL)), 0.0, 1e-8), "det(A - lambda*I) = 0");
        freeMatrix(I); freeMatrix(lI); freeMatrix(AmL);
    }
    freeMatrix(V); free(eig_v);

    // Complex-entry 2x2: diag(1+i, 2) eigs = {1+i, 2}
    MatrixElement cmd[] = { C(1, 1), R(0), R(0), R(2) };
    Matrix* Cm = constructMatrixFromArray(2, 2, cmd, 4);
    MatrixElement* eig_m = eigenvalues2x2(Cm);
    bool found_1i = approxElem(eig_m[0], 1, 1, 1e-9) || approxElem(eig_m[1], 1, 1, 1e-9);
    bool found_2  = approxReal(eig_m[0], 2, 1e-9) || approxReal(eig_m[1], 2, 1e-9);
    CHECK(found_1i, "complex diag yields 1+i eigenvalue");
    CHECK(found_2, "complex diag yields 2 eigenvalue");
    freeMatrix(Cm); free(eig_m);
}

void testEigenvalues3x3() {
    SECTION("eigenvalues3x3");
    CHECK(eigenvalues3x3(NULL) == NULL, "NULL returns NULL");

    Matrix* nsq = constructMatrix(2, 2);
    CHECK(eigenvalues3x3(nsq) == NULL, "non-3x3 returns NULL");
    freeMatrix(nsq);

    // Identity: all eigs = 1
    Matrix* id = idMatrix(3);
    MatrixElement* eig_id = eigenvalues3x3(id);
    CHECK(approxReal(eig_id[0], 1.0, 1e-9) &&
          approxReal(eig_id[1], 1.0, 1e-9) &&
          approxReal(eig_id[2], 1.0, 1e-9), "eigs(I_3) = {1,1,1}");
    freeMatrix(id); free(eig_id);

    // Triple root: 2*I_3, all eigs = 2
    long double tid[] = {2,0,0, 0,2,0, 0,0,2};
    Matrix* T = makeRealMatrix(3, 3, tid);
    MatrixElement* eig_t = eigenvalues3x3(T);
    CHECK(approxReal(eig_t[0], 2.0, 1e-9) &&
          approxReal(eig_t[1], 2.0, 1e-9) &&
          approxReal(eig_t[2], 2.0, 1e-9), "triple root 2*I eigs = {2,2,2}");
    freeMatrix(T); free(eig_t);

    // Diagonal [[1,0,0],[0,2,0],[0,0,3]]
    long double dd[] = {1,0,0, 0,2,0, 0,0,3};
    Matrix* D = makeRealMatrix(3, 3, dd);
    MatrixElement* eig_d = eigenvalues3x3(D);
    MatrixElement esum = elemAdd(elemAdd(eig_d[0], eig_d[1]), eig_d[2]);
    MatrixElement eprod = elemMul(elemMul(eig_d[0], eig_d[1]), eig_d[2]);
    CHECK(elemEq(esum, trace(D), 1e-9), "sum of eigs = trace(diag)");
    CHECK(elemEq(eprod, determinant(D), 1e-9), "product of eigs = det(diag)");
    // All eigs should be real (Cardano cleanup)
    CHECK(!eig_d[0].isComplex && !eig_d[1].isComplex && !eig_d[2].isComplex,
          "real inputs yield real eigs for 3-real-root case");
    // Verify each satisfies det(A - lambda*I) = 0
    for (int k = 0; k < 3; k++) {
        MatrixElement lam = eig_d[k];
        Matrix* I = idMatrix(3);
        Matrix* lI = multByConstant(I, lam);
        Matrix* AmL = subtractMatrices(D, lI);
        CHECK(approx(elemAbs(determinant(AmL)), 0.0, 1e-7), "det(D - lambda*I) = 0");
        freeMatrix(I); freeMatrix(lI); freeMatrix(AmL);
    }
    freeMatrix(D); free(eig_d);

    // One real + two complex: [[1,-1,0],[1,1,0],[0,0,2]] — real root = 2, complex = 1±i
    long double cd[] = {1,-1,0, 1,1,0, 0,0,2};
    Matrix* Cr = makeRealMatrix(3, 3, cd);
    MatrixElement* eig_c = eigenvalues3x3(Cr);
    // One eigenvalue must be 2
    bool found_2 = false, found_1pi = false, found_1mi = false;
    for (int k = 0; k < 3; k++) {
        if (approxReal(eig_c[k], 2.0, 1e-9)) found_2 = true;
        else if (approxElem(eig_c[k], 1, 1, 1e-9)) found_1pi = true;
        else if (approxElem(eig_c[k], 1, -1, 1e-9)) found_1mi = true;
    }
    CHECK(found_2, "real eigenvalue 2 found");
    CHECK(found_1pi, "complex eigenvalue 1+i found");
    CHECK(found_1mi, "complex eigenvalue 1-i found");
    freeMatrix(Cr); free(eig_c);
}

// Free a NULL-tolerant eigenvector array of length n
static void freeEvects(Matrix** evects, int n) {
    if (!evects) return;
    for (int i = 0; i < n; i++) freeMatrix(evects[i]);
    free(evects);
}

// Check Av = lambda*v for a non-NULL eigenvector; handles complex lambdas
static bool checkEigenvector(Matrix* A, MatrixElement lambda, Matrix* v) {
    if (!v) return false;
    Matrix* Av = applyMatrix(A, v);
    Matrix* lv = multByConstant(v, lambda);
    bool ok = matrixComp(Av, lv, 1e-8);
    freeMatrix(Av);
    freeMatrix(lv);
    return ok;
}

void testEigenvectors2x2() {
    SECTION("eigenvectors2x2");
    CHECK(eigenvectors2x2(NULL) == NULL, "NULL returns NULL");

    Matrix* nsq = constructMatrix(3, 3);
    CHECK(eigenvectors2x2(nsq) == NULL, "non-2x2 returns NULL");
    freeMatrix(nsq);

    // Identity: eigenvalue = 1 (repeated), A-I = 0, evect = [1,0]
    Matrix* id = idMatrix(2);
    MatrixElement* eigs_id = eigenvalues2x2(id);
    Matrix** ev_id = eigenvectors2x2(id);
    CHECK(ev_id != NULL, "identity evects allocated");
    for (int k = 0; k < 2; k++)
        CHECK(checkEigenvector(id, eigs_id[k], ev_id[k]), "identity Av = lambda*v");
    free(eigs_id); freeEvects(ev_id, 2); freeMatrix(id);

    // Diagonal [[3,0],[0,2]]: distinct eigenvalues, axis-aligned eigenvectors
    long double dd[] = {3, 0, 0, 2};
    Matrix* D = makeRealMatrix(2, 2, dd);
    MatrixElement* eigs_d = eigenvalues2x2(D);
    Matrix** ev_d = eigenvectors2x2(D);
    CHECK(ev_d != NULL, "diagonal evects allocated");
    CHECK(ev_d[0] != NULL && ev_d[1] != NULL, "both slots non-NULL");
    CHECK(checkEigenvector(D, eigs_d[0], ev_d[0]), "diag ev0: Av = lambda*v");
    CHECK(checkEigenvector(D, eigs_d[1], ev_d[1]), "diag ev1: Av = lambda*v");
    free(eigs_d); freeEvects(ev_d, 2); freeMatrix(D);

    // Symmetric [[5,2],[2,5]]: eigenvalues 7 and 3, evects along [1,1] and [1,-1]
    long double sd[] = {5, 2, 2, 5};
    Matrix* S = makeRealMatrix(2, 2, sd);
    MatrixElement* eigs_s = eigenvalues2x2(S);
    Matrix** ev_s = eigenvectors2x2(S);
    CHECK(ev_s != NULL && ev_s[0] != NULL && ev_s[1] != NULL, "symmetric evects non-NULL");
    CHECK(checkEigenvector(S, eigs_s[0], ev_s[0]), "sym ev0: Av = lambda*v");
    CHECK(checkEigenvector(S, eigs_s[1], ev_s[1]), "sym ev1: Av = lambda*v");
    free(eigs_s); freeEvects(ev_s, 2); freeMatrix(S);

    // Upper triangular [[3,1],[0,2]]
    long double td[] = {3, 1, 0, 2};
    Matrix* T = makeRealMatrix(2, 2, td);
    MatrixElement* eigs_t = eigenvalues2x2(T);
    Matrix** ev_t = eigenvectors2x2(T);
    CHECK(checkEigenvector(T, eigs_t[0], ev_t[0]), "triangular ev0: Av = lambda*v");
    CHECK(checkEigenvector(T, eigs_t[1], ev_t[1]), "triangular ev1: Av = lambda*v");
    free(eigs_t); freeEvects(ev_t, 2); freeMatrix(T);

    // Complex eigenvalues: eigenvectors are also complex
    long double cd[] = {0, -1, 1, 0};
    Matrix* Cr = makeRealMatrix(2, 2, cd);
    MatrixElement* eigs_c = eigenvalues2x2(Cr);
    Matrix** ev_c = eigenvectors2x2(Cr);
    CHECK(ev_c != NULL, "complex: array allocated");
    CHECK(ev_c[0] != NULL && ev_c[1] != NULL, "complex evects non-NULL");
    CHECK(checkEigenvector(Cr, eigs_c[0], ev_c[0]), "complex ev0: Av = lambda*v");
    CHECK(checkEigenvector(Cr, eigs_c[1], ev_c[1]), "complex ev1: Av = lambda*v");
    // Each eigenvector should have at least one complex entry (since eig is complex)
    bool ev0_hascx = getEntry(ev_c[0], 0, 0).isComplex || getEntry(ev_c[0], 1, 0).isComplex;
    bool ev1_hascx = getEntry(ev_c[1], 0, 0).isComplex || getEntry(ev_c[1], 1, 0).isComplex;
    CHECK(ev0_hascx && ev1_hascx, "complex evects contain complex entries");
    free(eigs_c); freeEvects(ev_c, 2); freeMatrix(Cr);
}

void testEigenvectors3x3() {
    SECTION("eigenvectors3x3");
    CHECK(eigenvectors3x3(NULL) == NULL, "NULL returns NULL");

    Matrix* nsq = constructMatrix(2, 2);
    CHECK(eigenvectors3x3(nsq) == NULL, "non-3x3 returns NULL");
    freeMatrix(nsq);

    // Identity: triple eigenvalue 1, A-I = 0, falls back to [1,0,0]
    Matrix* id = idMatrix(3);
    MatrixElement* eigs_id = eigenvalues3x3(id);
    Matrix** ev_id = eigenvectors3x3(id);
    CHECK(ev_id != NULL, "identity evects allocated");
    for (int k = 0; k < 3; k++)
        CHECK(checkEigenvector(id, eigs_id[k], ev_id[k]), "identity Av = lambda*v");
    free(eigs_id); freeEvects(ev_id, 3); freeMatrix(id);

    // Diagonal [[1,0,0],[0,2,0],[0,0,3]]: axis-aligned eigenvectors
    long double dd[] = {1,0,0, 0,2,0, 0,0,3};
    Matrix* D = makeRealMatrix(3, 3, dd);
    MatrixElement* eigs_d = eigenvalues3x3(D);
    Matrix** ev_d = eigenvectors3x3(D);
    CHECK(ev_d != NULL, "diagonal evects allocated");
    CHECK(ev_d[0] != NULL && ev_d[1] != NULL && ev_d[2] != NULL, "all slots non-NULL");
    CHECK(checkEigenvector(D, eigs_d[0], ev_d[0]), "diag ev0: Av = lambda*v");
    CHECK(checkEigenvector(D, eigs_d[1], ev_d[1]), "diag ev1: Av = lambda*v");
    CHECK(checkEigenvector(D, eigs_d[2], ev_d[2]), "diag ev2: Av = lambda*v");
    free(eigs_d); freeEvects(ev_d, 3); freeMatrix(D);

    // Symmetric [[4,1,0],[1,4,1],[0,1,4]]: three real eigenvalues
    long double sym[] = {4,1,0, 1,4,1, 0,1,4};
    Matrix* Sym = makeRealMatrix(3, 3, sym);
    MatrixElement* eigs_sym = eigenvalues3x3(Sym);
    Matrix** ev_sym = eigenvectors3x3(Sym);
    CHECK(ev_sym != NULL, "symmetric evects allocated");
    for (int k = 0; k < 3; k++) {
        CHECK(checkEigenvector(Sym, eigs_sym[k], ev_sym[k]), "sym Av = lambda*v");
    }
    free(eigs_sym); freeEvects(ev_sym, 3); freeMatrix(Sym);

    // Upper triangular [[2,1,3],[0,4,2],[0,0,6]]: eigenvalues on diagonal
    long double tri[] = {2,1,3, 0,4,2, 0,0,6};
    Matrix* Tri = makeRealMatrix(3, 3, tri);
    MatrixElement* eigs_tri = eigenvalues3x3(Tri);
    Matrix** ev_tri = eigenvectors3x3(Tri);
    CHECK(ev_tri != NULL, "triangular evects allocated");
    for (int k = 0; k < 3; k++) {
        CHECK(checkEigenvector(Tri, eigs_tri[k], ev_tri[k]), "triangular Av = lambda*v");
    }
    free(eigs_tri); freeEvects(ev_tri, 3); freeMatrix(Tri);

    // One real + two complex: real eigenvector valid, complex eigenvectors also populated
    long double cd[] = {1,-1,0, 1,1,0, 0,0,2};
    Matrix* Cr = makeRealMatrix(3, 3, cd);
    MatrixElement* eigs_c = eigenvalues3x3(Cr);
    Matrix** ev_c = eigenvectors3x3(Cr);
    CHECK(ev_c != NULL, "complex: array allocated");
    for (int k = 0; k < 3; k++) {
        CHECK(ev_c[k] != NULL, "all evect slots populated (complex or real)");
        CHECK(checkEigenvector(Cr, eigs_c[k], ev_c[k]), "Av = lambda*v (complex or real)");
    }
    free(eigs_c); freeEvects(ev_c, 3); freeMatrix(Cr);
}

/* ---------- Vector tests ---------- */

void testFreeVector() {
    SECTION("freeVector");
    freeVector(NULL);
    CHECK(true, "freeVector(NULL) doesn't crash");

    Vector* v = constructVector(3);
    freeVector(v);
    CHECK(true, "freeVector on valid vector doesn't crash");
}

void testConstructVector() {
    SECTION("constructVector");
    CHECK(constructVector(0) == NULL, "dim 0 returns NULL");
    CHECK(constructVector(-1) == NULL, "negative dim returns NULL");

    Vector* v = constructVector(3);
    CHECK(v != NULL, "dim 3 construct succeeds");
    CHECK(v->numRows == 3 && v->numCols == 1, "shape 3x1");
    CHECK(approxReal(getEntry(v, 0, 0), 0, 0) && approxReal(getEntry(v, 2, 0), 0, 0), "entries zero-initialized");
    freeVector(v);

    Vector* big = constructVector(10);
    CHECK(big != NULL && big->numRows == 10, "dim 10 works (no artificial cap)");
    freeVector(big);
}

void testConstructVectorFromArray() {
    SECTION("constructVectorFromArray");
    MatrixElement data[] = {R(1.5), R(-2.5), R(3.5)};
    CHECK(constructVectorFromArray(0, data, 0) == NULL, "dim 0 NULL");
    CHECK(constructVectorFromArray(3, NULL, 3) == NULL, "NULL data NULL");
    CHECK(constructVectorFromArray(3, data, 2) == NULL, "dataLen mismatch NULL");
    CHECK(constructVectorFromArray(3, data, 4) == NULL, "dataLen mismatch NULL (too big)");

    Vector* v = constructVectorFromArray(3, data, 3);
    CHECK(v != NULL, "valid construct");
    CHECK(v->numRows == 3 && v->numCols == 1, "shape 3x1");
    CHECK(approxReal(getEntry(v, 0, 0), 1.5, 0), "entry 0");
    CHECK(approxReal(getEntry(v, 1, 0), -2.5, 0), "entry 1");
    CHECK(approxReal(getEntry(v, 2, 0), 3.5, 0), "entry 2");
    freeVector(v);

    // Complex entry vector
    MatrixElement cdata[] = { C(1, 1), R(2), C(0, -1) };
    Vector* cv = constructVectorFromArray(3, cdata, 3);
    CHECK(cv != NULL, "complex vector constructed");
    CHECK(getEntry(cv, 0, 0).isComplex, "complex entry preserved");
    CHECK(approxElem(getEntry(cv, 2, 0), 0, -1, 0), "-i preserved");
    freeVector(cv);
}

void testConstructVector2() {
    SECTION("constructVector2");
    CHECK(constructVector2(R(NAN), R(1.0)) == NULL, "NAN x returns NULL");
    CHECK(constructVector2(R(1.0), R(NAN)) == NULL, "NAN y returns NULL");

    Vector* v = constructVector2(R(3.0), R(-4.0));
    CHECK(v != NULL, "valid construct");
    CHECK(v->numRows == 2 && v->numCols == 1, "shape 2x1");
    CHECK(approxReal(getEntry(v, 0, 0), 3.0, 0) && approxReal(getEntry(v, 1, 0), -4.0, 0), "values correct");
    freeVector(v);

    // Complex-valued vector2
    Vector* cv = constructVector2(C(1, 1), C(0, -1));
    CHECK(cv != NULL, "complex construct");
    CHECK(getEntry(cv, 0, 0).isComplex, "(0,0) complex");
    freeVector(cv);
}

void testConstructVector3() {
    SECTION("constructVector3");
    CHECK(constructVector3(R(NAN), R(1.0), R(2.0)) == NULL, "NAN x returns NULL");
    CHECK(constructVector3(R(1.0), R(NAN), R(2.0)) == NULL, "NAN y returns NULL");
    CHECK(constructVector3(R(1.0), R(2.0), R(NAN)) == NULL, "NAN z returns NULL");

    Vector* v = constructVector3(R(1.0), R(2.0), R(3.0));
    CHECK(v != NULL, "valid construct");
    CHECK(v->numRows == 3 && v->numCols == 1, "shape 3x1");
    CHECK(approxReal(getEntry(v, 0, 0), 1.0, 0) && approxReal(getEntry(v, 1, 0), 2.0, 0) && approxReal(getEntry(v, 2, 0), 3.0, 0),
          "values correct");
    freeVector(v);
}

void testAddVectors() {
    SECTION("addVectors");
    CHECK(addVectors(NULL, NULL) == NULL, "NULL inputs");

    Vector* v2 = constructVector(2);
    Vector* v3 = constructVector(3);
    CHECK(addVectors(v2, v3) == NULL, "dim mismatch returns NULL");
    freeVector(v2);

    // Reject non-column "vector"
    Matrix* row = constructMatrix(1, 3);
    CHECK(addVectors(row, v3) == NULL, "row matrix rejected");
    freeMatrix(row);
    freeVector(v3);

    Vector* a = constructVector3(R(1.0), R(2.0), R(3.0));
    Vector* b = constructVector3(R(4.0), R(5.0), R(6.0));
    Vector* s = addVectors(a, b);
    CHECK(s != NULL, "sum allocated");
    CHECK(approxReal(getEntry(s, 0, 0), 5.0, 1e-10) &&
          approxReal(getEntry(s, 1, 0), 7.0, 1e-10) &&
          approxReal(getEntry(s, 2, 0), 9.0, 1e-10), "sum values correct");

    // v + (-v) = 0
    Vector* neg = negativeVector(a);
    Vector* zero = addVectors(a, neg);
    CHECK(approx(l2Norm(zero), 0.0, 1e-10), "v + (-v) = 0");

    freeVector(a); freeVector(b); freeVector(s);
    freeVector(neg); freeVector(zero);
}

void testVectorDotProduct() {
    SECTION("vectorDotProduct");
    CHECK(elemIsNan(vectorDotProduct(NULL, NULL)), "NULL inputs NAN");

    Vector* v2 = constructVector(2);
    Vector* v3 = constructVector(3);
    CHECK(elemIsNan(vectorDotProduct(v2, v3)), "dim mismatch NAN");
    freeVector(v2);

    Matrix* row = constructMatrix(1, 3);
    CHECK(elemIsNan(vectorDotProduct(row, v3)), "row matrix rejected");
    freeMatrix(row);
    freeVector(v3);

    Vector* a = constructVector3(R(1.0), R(2.0), R(3.0));
    Vector* b = constructVector3(R(4.0), R(5.0), R(6.0));
    CHECK(approxReal(vectorDotProduct(a, b), 32.0, 1e-10), "1*4 + 2*5 + 3*6 = 32");

    // Commutative
    CHECK(elemEq(vectorDotProduct(a, b), vectorDotProduct(b, a), 1e-10), "commutative");

    // v . v = |v|^2
    CHECK(approxReal(vectorDotProduct(a, a), 14.0, 1e-10), "v . v = sum of squares");

    // Orthogonal
    Vector* e1 = constructVector3(R(1.0), R(0.0), R(0.0));
    Vector* e2 = constructVector3(R(0.0), R(1.0), R(0.0));
    CHECK(approxReal(vectorDotProduct(e1, e2), 0.0, 1e-10), "orthogonal basis vectors dot = 0");

    // Complex dot product: (i)(i) + (1)(1) = -1 + 1 = 0
    Vector* ca = constructVector2(C(0, 1), R(1));
    Vector* cb = constructVector2(C(0, 1), R(1));
    CHECK(approxReal(vectorDotProduct(ca, cb), 0.0, 1e-12), "linear (non-Hermitian) complex dot");
    freeVector(ca); freeVector(cb);

    freeVector(a); freeVector(b); freeVector(e1); freeVector(e2);
}

void testHermitianDotProduct() {
    SECTION("hermitianDotProduct");
    CHECK(elemIsNan(hermitianDotProduct(NULL, NULL)), "NULL inputs NAN");

    Vector* v2 = constructVector(2);
    Vector* v3 = constructVector(3);
    CHECK(elemIsNan(hermitianDotProduct(v2, v3)), "dim mismatch NAN");
    freeVector(v2); freeVector(v3);

    // <v,v> should be real and nonnegative
    Vector* v = constructVector2(C(0, 1), R(1));
    MatrixElement vv = hermitianDotProduct(v, v);
    CHECK(!vv.isComplex, "<v,v> collapses to real");
    CHECK(approxReal(vv, 2.0, 1e-12), "<v,v> = |i|^2 + |1|^2 = 2");

    // Contrast with bilinear dot used elsewhere
    CHECK(approxReal(vectorDotProduct(v, v), 0.0, 1e-12), "bilinear v.v differs from Hermitian");

    // Conjugate symmetry: <x,y> = conj(<y,x>)
    Vector* x = constructVector2(C(1, 2), C(-1, 1));
    Vector* y = constructVector2(C(3, -1), C(2, 4));
    MatrixElement xy = hermitianDotProduct(x, y);
    MatrixElement yx = hermitianDotProduct(y, x);
    CHECK(elemEq(xy, elemConj(yx), 1e-10), "conjugate symmetry");

    freeVector(v);
    freeVector(x);
    freeVector(y);
}

void testCrossProduct() {
    SECTION("crossProduct");
    CHECK(crossProduct(NULL, NULL) == NULL, "NULL inputs");

    Vector* v2 = constructVector2(R(1.0), R(2.0));
    Vector* v3 = constructVector3(R(1.0), R(2.0), R(3.0));
    CHECK(crossProduct(v2, v3) == NULL, "dim mismatch NULL");
    CHECK(crossProduct(v2, v2) == NULL, "2D vectors not allowed");
    freeVector(v2);
    freeVector(v3);

    // Standard basis: e1 x e2 = e3
    Vector* e1 = constructVector3(R(1.0), R(0.0), R(0.0));
    Vector* e2 = constructVector3(R(0.0), R(1.0), R(0.0));
    Vector* e3 = constructVector3(R(0.0), R(0.0), R(1.0));

    Vector* e1xe2 = crossProduct(e1, e2);
    CHECK(approxReal(getEntry(e1xe2, 0, 0), 0.0, 1e-10) &&
          approxReal(getEntry(e1xe2, 1, 0), 0.0, 1e-10) &&
          approxReal(getEntry(e1xe2, 2, 0), 1.0, 1e-10), "e1 x e2 = e3");

    // e2 x e3 = e1
    Vector* e2xe3 = crossProduct(e2, e3);
    CHECK(approxReal(getEntry(e2xe3, 0, 0), 1.0, 1e-10) &&
          approxReal(getEntry(e2xe3, 1, 0), 0.0, 1e-10) &&
          approxReal(getEntry(e2xe3, 2, 0), 0.0, 1e-10), "e2 x e3 = e1");

    // e3 x e1 = e2
    Vector* e3xe1 = crossProduct(e3, e1);
    CHECK(approxReal(getEntry(e3xe1, 0, 0), 0.0, 1e-10) &&
          approxReal(getEntry(e3xe1, 1, 0), 1.0, 1e-10) &&
          approxReal(getEntry(e3xe1, 2, 0), 0.0, 1e-10), "e3 x e1 = e2");

    // Anti-commutative: a x b = -(b x a)
    Vector* a = constructVector3(R(1.0), R(2.0), R(3.0));
    Vector* b = constructVector3(R(4.0), R(5.0), R(6.0));
    Vector* axb = crossProduct(a, b);
    Vector* bxa = crossProduct(b, a);
    Vector* negBxa = negativeVector(bxa);
    CHECK(matrixComp(axb, negBxa, 1e-10), "a x b = -(b x a)");

    // Cross product is orthogonal to both inputs
    CHECK(approxReal(vectorDotProduct(axb, a), 0.0, 1e-10), "(a x b) . a = 0");
    CHECK(approxReal(vectorDotProduct(axb, b), 0.0, 1e-10), "(a x b) . b = 0");

    // Parallel vectors: a x a = 0
    Vector* axa = crossProduct(a, a);
    CHECK(approx(l2Norm(axa), 0.0, 1e-10), "a x a = 0");

    freeVector(e1); freeVector(e2); freeVector(e3);
    freeVector(e1xe2); freeVector(e2xe3); freeVector(e3xe1);
    freeVector(a); freeVector(b);
    freeVector(axb); freeVector(bxa); freeVector(negBxa); freeVector(axa);
}

void testL2Norm() {
    SECTION("l2Norm");
    CHECK(isnan(l2Norm(NULL)), "NULL returns NAN");

    Vector* zero = constructVector(3);
    CHECK(approx(l2Norm(zero), 0.0, 1e-10), "norm of zero vector = 0");
    freeVector(zero);

    // 3-4-5 triangle
    Vector* v = constructVector2(R(3.0), R(4.0));
    CHECK(approx(l2Norm(v), 5.0, 1e-10), "||[3,4]|| = 5");
    freeVector(v);

    // 3D: sqrtl(1 + 4 + 9) = sqrtl(14)
    Vector* v3 = constructVector3(R(1.0), R(2.0), R(3.0));
    CHECK(approx(l2Norm(v3), sqrtl(14.0), 1e-10), "||[1,2,3]|| = sqrtl(14)");
    freeVector(v3);

    // Non-integer components
    Vector* vf = constructVector2(R(0.3), R(0.4));
    CHECK(approx(l2Norm(vf), 0.5, 1e-10), "||[0.3, 0.4]|| = 0.5");
    freeVector(vf);

    // Negative components
    Vector* vn = constructVector3(R(-1.0), R(-2.0), R(-2.0));
    CHECK(approx(l2Norm(vn), 3.0, 1e-10), "||[-1,-2,-2]|| = 3");
    freeVector(vn);

    // Complex components: sum of |v_i|^2, not v_i^2.
    // v = [3+4i, 0] → |v|^2 = 25 → ||v|| = 5
    Vector* vc = constructVector2(C(3, 4), R(0));
    CHECK(approx(l2Norm(vc), 5.0, 1e-10), "||[3+4i, 0]|| = 5 (uses |v_i|^2)");
    freeVector(vc);

    // v = [i, i] → |v|^2 = 1+1 = 2 → ||v|| = sqrtl(2)
    Vector* vii = constructVector2(C(0, 1), C(0, 1));
    CHECK(approx(l2Norm(vii), sqrtl(2.0), 1e-10), "||[i, i]|| = sqrtl(2)");
    freeVector(vii);
}

void testNegativeVector() {
    SECTION("negativeVector");
    CHECK(negativeVector(NULL) == NULL, "NULL returns NULL");

    Vector* v = constructVector3(R(1.0), R(-2.0), R(3.0));
    Vector* neg = negativeVector(v);
    CHECK(neg != NULL, "allocated");
    CHECK(approxReal(getEntry(neg, 0, 0), -1.0, 1e-10) &&
          approxReal(getEntry(neg, 1, 0),  2.0, 1e-10) &&
          approxReal(getEntry(neg, 2, 0), -3.0, 1e-10), "components negated");

    // Double negation returns original
    Vector* negNeg = negativeVector(neg);
    CHECK(matrixComp(v, negNeg, 1e-10), "-(-v) = v");

    // Original unchanged
    CHECK(approxReal(getEntry(v, 0, 0), 1.0, 0), "original unchanged");

    freeVector(v); freeVector(neg); freeVector(negNeg);
}

void testScaleVector() {
    SECTION("scaleVector");
    CHECK(scaleVector(NULL, R(2.0)) == NULL, "NULL vector NULL");

    Vector* v = constructVector3(R(1.0), R(2.0), R(3.0));
    CHECK(scaleVector(v, R(NAN)) == NULL, "NAN scalar NULL");

    Vector* twice = scaleVector(v, R(2.0));
    CHECK(approxReal(getEntry(twice, 0, 0), 2.0, 1e-10) &&
          approxReal(getEntry(twice, 1, 0), 4.0, 1e-10) &&
          approxReal(getEntry(twice, 2, 0), 6.0, 1e-10), "scale by 2");

    Vector* zero = scaleVector(v, R(0.0));
    CHECK(approx(l2Norm(zero), 0.0, 1e-10), "scale by 0 = zero vector");

    Vector* negOne = scaleVector(v, R(-1.0));
    Vector* neg = negativeVector(v);
    CHECK(matrixComp(negOne, neg, 1e-10), "scale by -1 = negativeVector");

    // Complex scalar: v * i where v is real
    Vector* iv = scaleVector(v, C(0, 1));
    CHECK(getEntry(iv, 0, 0).isComplex, "scaling real by i yields complex");
    CHECK(approxElem(getEntry(iv, 0, 0), 0, 1, 1e-12), "1 * i = i");
    CHECK(approxElem(getEntry(iv, 2, 0), 0, 3, 1e-12), "3 * i = 3i");
    freeVector(iv);

    // Original unchanged
    CHECK(approxReal(getEntry(v, 0, 0), 1.0, 0), "original unchanged");

    freeVector(v); freeVector(twice); freeVector(zero);
    freeVector(negOne); freeVector(neg);
}

void testSubtractVectors() {
    SECTION("subtractVectors");
    CHECK(subtractVectors(NULL, NULL) == NULL, "NULL inputs");

    Vector* v2 = constructVector(2);
    Vector* v3 = constructVector(3);
    CHECK(subtractVectors(v2, v3) == NULL, "dim mismatch NULL");
    freeVector(v2); freeVector(v3);

    Vector* a = constructVector3(R(5.0), R(7.0), R(9.0));
    Vector* b = constructVector3(R(1.0), R(2.0), R(3.0));
    Vector* d = subtractVectors(a, b);
    CHECK(approxReal(getEntry(d, 0, 0), 4.0, 1e-10) &&
          approxReal(getEntry(d, 1, 0), 5.0, 1e-10) &&
          approxReal(getEntry(d, 2, 0), 6.0, 1e-10), "a - b correct");

    // v - v = 0
    Vector* zero = subtractVectors(a, a);
    CHECK(approx(l2Norm(zero), 0.0, 1e-10), "v - v = 0");

    // a - b = -(b - a)
    Vector* dRev = subtractVectors(b, a);
    Vector* negDRev = negativeVector(dRev);
    CHECK(matrixComp(d, negDRev, 1e-10), "a - b = -(b - a)");

    freeVector(a); freeVector(b); freeVector(d);
    freeVector(zero); freeVector(dRev); freeVector(negDRev);
}

void testNormalizeVector() {
    SECTION("normalizeVector");
    CHECK(normalizeVector(NULL) == NULL, "NULL returns NULL");

    Vector* zero = constructVector(3);
    CHECK(normalizeVector(zero) == NULL, "zero vector has no unit NULL");
    freeVector(zero);

    // Already-unit vector stays unit
    Vector* e1 = constructVector3(R(1.0), R(0.0), R(0.0));
    Vector* u1 = normalizeVector(e1);
    CHECK(approx(l2Norm(u1), 1.0, 1e-10), "unit stays unit");
    CHECK(matrixComp(e1, u1, 1e-10), "unit vector unchanged");
    freeVector(e1); freeVector(u1);

    // 3-4-5 triangle normalizes to (0.6, 0.8)
    Vector* v = constructVector2(R(3.0), R(4.0));
    Vector* u = normalizeVector(v);
    CHECK(approx(l2Norm(u), 1.0, 1e-10), "norm of result = 1");
    CHECK(approxReal(getEntry(u, 0, 0), 0.6, 1e-10) &&
          approxReal(getEntry(u, 1, 0), 0.8, 1e-10), "[3,4] normalized");
    freeVector(v); freeVector(u);

    // Direction preserved
    Vector* w = constructVector3(R(2.0), R(-4.0), R(4.0));
    Vector* uw = normalizeVector(w);
    CHECK(approx(l2Norm(uw), 1.0, 1e-10), "norm 1");
    // w / 6 should equal uw (since |w| = 6)
    Vector* wDiv6 = scaleVector(w, R(1.0 / 6.0));
    CHECK(matrixComp(uw, wDiv6, 1e-10), "direction preserved");
    freeVector(w); freeVector(uw); freeVector(wDiv6);

    // Complex vector: [3+4i, 0] has norm 5, normalized = [(3+4i)/5, 0]
    Vector* vc = constructVector2(C(3, 4), R(0));
    Vector* uc = normalizeVector(vc);
    CHECK(approx(l2Norm(uc), 1.0, 1e-10), "complex vector normalizes to unit");
    CHECK(approxElem(getEntry(uc, 0, 0), 0.6, 0.8, 1e-10), "(3+4i)/5 = 0.6+0.8i");
    freeVector(vc); freeVector(uc);
}

void testVectorDistance() {
    SECTION("vectorDistance");
    CHECK(isnan(vectorDistance(NULL, NULL)), "NULL inputs NAN");

    Vector* v2 = constructVector(2);
    Vector* v3 = constructVector(3);
    CHECK(isnan(vectorDistance(v2, v3)), "dim mismatch NAN");
    freeVector(v2); freeVector(v3);

    // Distance to self is 0
    Vector* p = constructVector3(R(1.0), R(2.0), R(3.0));
    CHECK(approx(vectorDistance(p, p), 0.0, 1e-10), "dist(p, p) = 0");

    // 3-4-5: from origin to (3,4)
    Vector* origin2 = constructVector2(R(0.0), R(0.0));
    Vector* p34 = constructVector2(R(3.0), R(4.0));
    CHECK(approx(vectorDistance(origin2, p34), 5.0, 1e-10), "dist origin to [3,4] = 5");

    // Symmetric
    CHECK(approx(vectorDistance(origin2, p34), vectorDistance(p34, origin2), 1e-10),
          "dist symmetric");

    // (1,2,3) to (4,6,3): dx=3, dy=4, dz=0 → 5
    Vector* q = constructVector3(R(4.0), R(6.0), R(3.0));
    CHECK(approx(vectorDistance(p, q), 5.0, 1e-10), "3D distance = 5");

    freeVector(p); freeVector(origin2); freeVector(p34); freeVector(q);
}

void testVectorAngle() {
    SECTION("vectorAngle");
    CHECK(isnan(vectorAngle(NULL, NULL)), "NULL inputs NAN");

    Vector* v2 = constructVector(2);
    Vector* v3 = constructVector(3);
    CHECK(isnan(vectorAngle(v2, v3)), "dim mismatch NAN");
    freeVector(v2);

    Vector* zero = constructVector3(R(0.0), R(0.0), R(0.0));
    Vector* nonzero = constructVector3(R(1.0), R(0.0), R(0.0));
    CHECK(isnan(vectorAngle(zero, nonzero)), "zero vector NAN");
    freeVector(zero); freeVector(v3);

    // Parallel: angle = 0
    Vector* a = constructVector3(R(1.0), R(2.0), R(3.0));
    Vector* a2 = scaleVector(a, R(2.5));
    CHECK(approx(vectorAngle(a, a2), 0.0, 1e-9), "parallel: angle 0");
    freeVector(a2);

    // Anti-parallel: angle = pi
    Vector* aNeg = negativeVector(a);
    CHECK(approx(vectorAngle(a, aNeg), M_PI, 1e-9), "anti-parallel: angle pi");
    freeVector(aNeg);

    // Perpendicular: angle = pi/2
    Vector* e1 = constructVector3(R(1.0), R(0.0), R(0.0));
    Vector* e2 = constructVector3(R(0.0), R(1.0), R(0.0));
    CHECK(approx(vectorAngle(e1, e2), M_PI / 2.0, 1e-9), "perpendicular: pi/2");

    // Symmetric
    CHECK(approx(vectorAngle(e1, e2), vectorAngle(e2, e1), 1e-10), "angle symmetric");

    // 45 degrees: (1,0) vs (1,1)
    Vector* w1 = constructVector2(R(1.0), R(0.0));
    Vector* w2 = constructVector2(R(1.0), R(1.0));
    CHECK(approx(vectorAngle(w1, w2), M_PI / 4.0, 1e-9), "45 degrees");

    freeVector(nonzero); freeVector(a);
    freeVector(e1); freeVector(e2); freeVector(w1); freeVector(w2);
}

void testVectorProjectOnto() {
    SECTION("vectorProjectOnto");
    CHECK(vectorProjectOnto(NULL, NULL) == NULL, "NULL inputs");

    Vector* v2 = constructVector(2);
    Vector* v3 = constructVector(3);
    CHECK(vectorProjectOnto(v2, v3) == NULL, "dim mismatch NULL");
    freeVector(v2); freeVector(v3);

    Vector* zero = constructVector3(R(0.0), R(0.0), R(0.0));
    Vector* nonzero = constructVector3(R(1.0), R(1.0), R(1.0));
    CHECK(vectorProjectOnto(nonzero, zero) == NULL, "project onto zero NULL");
    freeVector(zero);

    // proj of (3, 4) onto x-axis = (3, 0)
    Vector* v = constructVector2(R(3.0), R(4.0));
    Vector* xAxis = constructVector2(R(1.0), R(0.0));
    Vector* p = vectorProjectOnto(v, xAxis);
    CHECK(approxReal(getEntry(p, 0, 0), 3.0, 1e-10) &&
          approxReal(getEntry(p, 1, 0), 0.0, 1e-10), "proj onto x-axis");
    freeVector(p);

    // Projection onto a non-unit vector: scale-invariant on target
    Vector* xScaled = constructVector2(R(5.0), R(0.0));
    Vector* pScaled = vectorProjectOnto(v, xScaled);
    CHECK(approxReal(getEntry(pScaled, 0, 0), 3.0, 1e-10) &&
          approxReal(getEntry(pScaled, 1, 0), 0.0, 1e-10), "proj invariant under target scaling");
    freeVector(pScaled);
    freeVector(xScaled);

    // Projection is parallel to target
    Vector* a = constructVector3(R(1.0), R(2.0), R(3.0));
    Vector* b = constructVector3(R(2.0), R(1.0), R(0.0));
    Vector* projAB = vectorProjectOnto(a, b);
    Vector* crossPB = crossProduct(projAB, b);
    CHECK(approx(l2Norm(crossPB), 0.0, 1e-9), "projection is parallel to target");
    freeVector(crossPB);

    // Residual (a - proj) is orthogonal to b
    Vector* resid = subtractVectors(a, projAB);
    CHECK(approxReal(vectorDotProduct(resid, b), 0.0, 1e-9), "a - proj_b(a) is perpendicular to b");
    freeVector(resid);

    // Projecting a vector onto itself returns itself
    Vector* projSelf = vectorProjectOnto(a, a);
    CHECK(matrixComp(projSelf, a, 1e-10), "proj of v onto v = v");
    freeVector(projSelf);

    freeVector(v); freeVector(xAxis);
    freeVector(nonzero); freeVector(a); freeVector(b); freeVector(projAB);
}

/* ---------- Integration tests for complex matrices ---------- */

void testComplexMatrixIntegration() {
    SECTION("complex matrix end-to-end");

    // A = [[1+i, 2], [3, 1-i]], det = (1+i)(1-i) - 2*3 = 2 - 6 = -4 (real)
    MatrixElement ad[] = { C(1, 1), R(2), R(3), C(1, -1) };
    Matrix* A = constructMatrixFromArray(2, 2, ad, 4);

    MatrixElement det = determinant(A);
    CHECK(approxReal(det, -4.0, 1e-10), "complex matrix det = -4");

    // A * inv(A) = I
    Matrix* invA = invertMatrix(A);
    CHECK(invA != NULL, "complex inverse computed");
    Matrix* prod = multiplyMatrices(A, invA);
    Matrix* I = idMatrix(2);
    CHECK(matrixComp(prod, I, 1e-9), "A * inv(A) = I (complex)");
    freeMatrix(prod); freeMatrix(I);

    // Solve Ax = b where b is complex
    MatrixElement bd[] = { C(1, 1), R(0) };
    Matrix* b = constructMatrixFromArray(2, 1, bd, 2);
    Matrix* x = solveLinEq(A, b);
    CHECK(x != NULL, "complex solve produces x");
    Matrix* check = multiplyMatrices(A, x);
    CHECK(matrixComp(check, b, 1e-9), "Ax = b for complex system");
    freeMatrix(b); freeMatrix(x); freeMatrix(check);

    // Trace of A = (1+i) + (1-i) = 2 (collapses to real)
    MatrixElement tr = trace(A);
    CHECK(!tr.isComplex, "trace of conjugate-pair diagonal collapses to real");
    CHECK(approxReal(tr, 2.0, 1e-12), "tr(A) = 2");

    // matrixPow A^2
    Matrix* A2 = matrixPow(A, 2);
    Matrix* A2_direct = multiplyMatrices(A, A);
    CHECK(matrixComp(A2, A2_direct, 1e-9), "A^2 via pow = A*A (complex)");
    freeMatrix(A2); freeMatrix(A2_direct);

    freeMatrix(A); freeMatrix(invA);
}

/* ---------- Reduction and subspace tests ---------- */

void testReduceRows() {
    printf("\n[testReduceRows]\n");

    CHECK(reduceRows(NULL) == NULL, "reduceRows(NULL) is NULL");

    // 2x3, full row rank: pivots in cols 0 and 1, last col is the dependent column
    long double d1[] = { 1, 2, 3,
                    2, 5, 7 };
    Matrix* A = makeRealMatrix(2, 3, d1);
    Matrix* R = reduceRows(A);
    CHECK(R != NULL, "RREF built");
    CHECK(approxReal(getEntry(R, 0, 0), 1.0, 1e-9), "R[0][0] = 1");
    CHECK(approxReal(getEntry(R, 0, 1), 0.0, 1e-9), "R[0][1] = 0 (eliminated above)");
    CHECK(approxReal(getEntry(R, 0, 2), 1.0, 1e-9), "R[0][2] = 1");
    CHECK(approxReal(getEntry(R, 1, 0), 0.0, 1e-9), "R[1][0] = 0");
    CHECK(approxReal(getEntry(R, 1, 1), 1.0, 1e-9), "R[1][1] = 1");
    CHECK(approxReal(getEntry(R, 1, 2), 1.0, 1e-9), "R[1][2] = 1");
    freeMatrix(A); freeMatrix(R);

    // 3x3 rank-deficient: third row is row1 + row2 -> RREF has bottom row of zeros
    long double d2[] = { 1, 2, 3,
                    4, 5, 6,
                    5, 7, 9 };
    A = makeRealMatrix(3, 3, d2);
    R = reduceRows(A);
    CHECK(R != NULL, "rank-deficient RREF built");
    bool zeroRow = approxReal(getEntry(R, 2, 0), 0.0, 1e-9)
                && approxReal(getEntry(R, 2, 1), 0.0, 1e-9)
                && approxReal(getEntry(R, 2, 2), 0.0, 1e-9);
    CHECK(zeroRow, "rank-deficient: bottom row is zero");
    freeMatrix(A); freeMatrix(R);

    // RREF of identity is identity
    Matrix* I = idMatrix(4);
    R = reduceRows(I);
    CHECK(matrixComp(R, I, 1e-12), "RREF(I) = I");
    freeMatrix(I); freeMatrix(R);

    // RREF is idempotent
    long double d3[] = { 0, 1, 2,
                    1, 0, 3,
                    2, 4, 6 };
    A = makeRealMatrix(3, 3, d3);
    R = reduceRows(A);
    Matrix* R2 = reduceRows(R);
    CHECK(matrixComp(R, R2, 1e-9), "RREF is idempotent");
    freeMatrix(A); freeMatrix(R); freeMatrix(R2);
}

void testReduceColumns() {
    printf("\n[testReduceColumns]\n");

    CHECK(reduceColumns(NULL) == NULL, "reduceColumns(NULL) is NULL");

    // reduceColumns(A) == transpose(reduceRows(transpose(A)))
    long double d[] = { 1, 2, 3,
                   4, 5, 6,
                   7, 8, 10 };
    Matrix* A = makeRealMatrix(3, 3, d);
    Matrix* C = reduceColumns(A);
    Matrix* T = transpose(A);
    Matrix* RT = reduceRows(T);
    Matrix* expected = transpose(RT);
    CHECK(matrixComp(C, expected, 1e-9), "reduceColumns = T(rref(T(A)))");
    freeMatrix(A); freeMatrix(C); freeMatrix(T); freeMatrix(RT); freeMatrix(expected);

    // reduceColumns of identity is identity
    Matrix* I = idMatrix(3);
    Matrix* CI = reduceColumns(I);
    CHECK(matrixComp(CI, I, 1e-12), "reduceColumns(I) = I");
    freeMatrix(I); freeMatrix(CI);

    // Rank preservation: rank(A) == rank(reduceColumns(A))
    long double d2[] = { 1, 2, 3,
                    2, 4, 6 };
    A = makeRealMatrix(2, 3, d2);
    C = reduceColumns(A);
    CHECK(rank(A) == rank(C), "reduceColumns preserves rank");
    CHECK(rank(A) == 1, "rank check sanity");
    freeMatrix(A); freeMatrix(C);
}

void testColumnSpace() {
    printf("\n[testColumnSpace]\n");

    size_t count = 0;
    CHECK(columnSpace(NULL, &count) == NULL, "NULL matrix");

    // 3x3 with rank 2: columns c0, c1 are independent; c2 = c0 + c1
    long double d[] = { 1, 0, 1,
                   2, 1, 3,
                   3, 2, 5 };
    Matrix* A = makeRealMatrix(3, 3, d);
    Vector** basis = columnSpace(A, &count);
    CHECK(basis != NULL, "basis returned");
    CHECK(count == 2, "rank 2 -> 2 basis vectors");
    CHECK(count == rank(A), "count matches rank");

    // Shape: each basis vector has shape (m, 1)
    bool shapeOk = true;
    for (size_t k = 0; k < count; k++) {
        if (basis[k]->numRows != A->numRows) shapeOk = false;
        if (basis[k]->numCols != 1) shapeOk = false;
    }
    CHECK(shapeOk, "basis vectors have shape (m, 1)");

    // Each basis vector should equal one of the original columns of A
    bool fromOriginal = true;
    for (size_t k = 0; k < count; k++) {
        bool matched = false;
        for (size_t c = 0; c < A->numCols && !matched; c++) {
            bool eq = true;
            for (size_t i = 0; i < A->numRows; i++) {
                MatrixElement b = getEntry(basis[k], i, 0);
                MatrixElement a = getEntry(A, i, c);
                if (!elemEq(a, b, 1e-12)) { eq = false; break; }
            }
            if (eq) matched = true;
        }
        if (!matched) fromOriginal = false;
    }
    CHECK(fromOriginal, "each basis vector equals an original column of A");

    // The dependent column (c2 = c0 + c1) should be expressible from the two basis cols.
    Vector* sum = addVectors(basis[0], basis[1]);
    bool depMatches = true;
    for (int i = 0; i < 3; i++) {
        MatrixElement s = getEntry(sum, i, 0);
        MatrixElement c2 = getEntry(A, i, 2);
        if (!elemEq(s, c2, 1e-9)) depMatches = false;
    }
    CHECK(depMatches, "basis[0] + basis[1] = column 2 of A");
    freeVector(sum);

    for (size_t k = 0; k < count; k++) freeVector(basis[k]);
    free(basis);
    freeMatrix(A);

    // Identity: column space basis is the n standard basis vectors
    Matrix* I = idMatrix(3);
    basis = columnSpace(I, &count);
    CHECK(count == 3, "I_3 has 3 col-space basis vectors");
    bool isStdBasis = true;
    for (int k = 0; k < 3; k++) {
        for (int i = 0; i < 3; i++) {
            long double expected = (i == k) ? 1.0 : 0.0;
            if (!approxReal(getEntry(basis[k], i, 0), expected, 1e-12)) isStdBasis = false;
        }
    }
    CHECK(isStdBasis, "I_3 column space basis = standard basis");
    for (size_t k = 0; k < count; k++) freeVector(basis[k]);
    free(basis);
    freeMatrix(I);

    // Zero matrix: column space is trivial
    Matrix* Z = constructMatrix(3, 4);
    basis = columnSpace(Z, &count);
    CHECK(basis == NULL && count == 0, "zero matrix: empty basis");
    freeMatrix(Z);
}

void testRowSpace() {
    printf("\n[testRowSpace]\n");

    size_t count = 0;
    CHECK(rowSpace(NULL, &count) == NULL, "NULL matrix");

    // 3x3 with rank 2: row 2 = row 0 + row 1
    long double d[] = { 1, 2, 3,
                   0, 1, 4,
                   1, 3, 7 };
    Matrix* A = makeRealMatrix(3, 3, d);
    Vector** basis = rowSpace(A, &count);
    CHECK(basis != NULL, "basis returned");
    CHECK(count == 2, "rank 2 -> 2 row-space basis vectors");
    CHECK(count == rank(A), "count matches rank");

    // Each basis vector has length n (= numCols of A)
    bool shapeOk = true;
    for (size_t k = 0; k < count; k++) {
        if (basis[k]->numRows != A->numCols) shapeOk = false;
        if (basis[k]->numCols != 1) shapeOk = false;
    }
    CHECK(shapeOk, "row-space basis vectors have shape (n, 1)");

    for (size_t k = 0; k < count; k++) freeVector(basis[k]);
    free(basis);
    freeMatrix(A);

    // Identity: row-space basis = n standard basis vectors
    Matrix* I = idMatrix(4);
    basis = rowSpace(I, &count);
    CHECK(count == 4, "I_4 has 4 row-space basis vectors");
    for (size_t k = 0; k < count; k++) freeVector(basis[k]);
    free(basis);
    freeMatrix(I);

    // dim(row space) == dim(col space)
    long double d2[] = { 1, 2, 3, 4,
                    2, 4, 6, 8,
                    1, 1, 1, 1 };
    A = makeRealMatrix(3, 4, d2);
    size_t cR = 0, cC = 0;
    Vector** rs = rowSpace(A, &cR);
    Vector** cs = columnSpace(A, &cC);
    CHECK(cR == cC, "dim row space = dim col space");
    for (size_t k = 0; k < cR; k++) freeVector(rs[k]);
    for (size_t k = 0; k < cC; k++) freeVector(cs[k]);
    free(rs); free(cs);
    freeMatrix(A);
}

/* ---------- Main ---------- */

int main(void) {
    printf("Running sokko tests...\n");

    testComp();
    testIsSquare();
    testFreeLU();
    testFreeMatrix();
    testResetEntryCache();
    testGetSetEntry();
    testComplexArith();
    testMatrixElementOps();
    testLuDecompose();
    testLuDet();
    testCacheLU();
    testConstructMatrix();
    testConstructMatrixFromMatrix();
    testConstructMatrixFromArray();
    testPrintMatrix();
    testCopyMatrix();
    testMatrixComp();
    testIdMatrix();
    testSimpleDotProduct();
    testDotProduct();
    testMultByConstant();
    testAddMatrices();
    testSubtractMatrices();
    testMultiplyMatrices();
    testTensorMatrices();
    testTranspose();
    testAdjoint();
    testLuSolve();
    testLuInverse();
    testDeterminant();
    testSolveLinEq();
    testInvertMatrix();
    testApplyMatrix();
    testMatrixPow();
    testIsSymmetric();
    testIsAntisymmetric();
    testIsOrthogonal();
    testIsUnitary();
    testRank();
    testNullity();
    testTrace();
    testFrobeniusNorm();
    testEigenvalues2x2();
    testEigenvalues3x3();
    testEigenvectors2x2();
    testEigenvectors3x3();
    testFreeVector();
    testConstructVector();
    testConstructVectorFromArray();
    testConstructVector2();
    testConstructVector3();
    testAddVectors();
    testVectorDotProduct();
    testHermitianDotProduct();
    testCrossProduct();
    testL2Norm();
    testNegativeVector();
    testScaleVector();
    testSubtractVectors();
    testNormalizeVector();
    testVectorDistance();
    testVectorAngle();
    testVectorProjectOnto();
    testComplexMatrixIntegration();
    testReduceRows();
    testReduceColumns();
    testColumnSpace();
    testRowSpace();

    printf("\n========================================\n");
    printf("Results: %d / %d tests passed\n", testsPassed, testsRun);
    printf("========================================\n");

    return (testsPassed == testsRun) ? 0 : 1;
}
