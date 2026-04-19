
/* AI GENERATED TEST SUITE */

// Everything here looks good to me, but note this was generated my an LLM.
// More tests might be warranted if using for production.

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "hebi.h"
#include "sokko.h"

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
static bool approx(double a, double b, double tol) {
    if (isnan(a) && isnan(b)) return true;
    return fabs(a - b) <= tol;
}

/* ---------- Helper tests ---------- */

void testComp() {
    SECTION("comp");
    double a = 1.0, b = 2.0, c = 1.0;
    CHECK(comp(&a, &b) < 0, "comp a < b");
    CHECK(comp(&b, &a) > 0, "comp b > a");
    CHECK(comp(&a, &c) == 0, "comp a == c");

    double arr[] = {3.0, 1.0, 4.0, 1.0, 5.0, 9.0, 2.0, 6.0};
    qsort(arr, 8, sizeof(double), comp);
    CHECK(arr[0] == 1.0 && arr[7] == 9.0, "qsort with comp");
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
    setEntry(m, 0, 0, 4); setEntry(m, 0, 1, 3);
    setEntry(m, 1, 0, 6); setEntry(m, 1, 1, 3);
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
    setEntry(m2, 0, 0, 1); setEntry(m2, 1, 1, 1);
    cacheLU(m2);
    freeMatrix(m2);
    CHECK(true, "freeMatrix with cached LU doesn't crash");
}

void testResetEntryCache() {
    SECTION("resetMatrixCache");
    Matrix* m = constructMatrix(2, 2);
    setEntry(m, 0, 0, 1); setEntry(m, 1, 1, 1);
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
    setEntry(m, 0, 0, 1.5);
    setEntry(m, 2, 3, -7.25);
    setEntry(m, 1, 2, 42);
    CHECK(getEntry(m, 0, 0) == 1.5, "get/set (0,0)");
    CHECK(getEntry(m, 2, 3) == -7.25, "get/set (2,3)");
    CHECK(getEntry(m, 1, 2) == 42, "get/set (1,2)");
    CHECK(getEntry(m, 0, 1) == 0, "uninitialized is 0");

    // setEntry should invalidate cache
    Matrix* sq = constructMatrix(2, 2);
    setEntry(sq, 0, 0, 1); setEntry(sq, 1, 1, 1);
    cacheLU(sq);
    CHECK(sq->cachedLU != NULL, "cache exists before set");
    setEntry(sq, 0, 0, 5);
    CHECK(sq->cachedLU == NULL, "setEntry invalidates cache");
    freeMatrix(m);
    freeMatrix(sq);
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
    setEntry(sing, 0, 0, 1); setEntry(sing, 0, 1, 2);
    setEntry(sing, 1, 0, 2); setEntry(sing, 1, 1, 4);
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
    Matrix* m = constructMatrix(3, 3);
    setEntry(m, 0, 0, 2); setEntry(m, 0, 1, 1); setEntry(m, 0, 2, 1);
    setEntry(m, 1, 0, 4); setEntry(m, 1, 1, 3); setEntry(m, 1, 2, 3);
    setEntry(m, 2, 0, 8); setEntry(m, 2, 1, 7); setEntry(m, 2, 2, 9);
    LU* lu2 = luDecompose(m);
    CHECK(lu2 != NULL, "3x3 decomposes");
    freeLU(lu2);
    freeMatrix(m);
}

void testLuDet() {
    SECTION("luDet");
    Matrix* id = idMatrix(5);
    LU* lu = luDecompose(id);
    CHECK(approx(luDet(lu), 1.0, 1e-10), "det(I) = 1");
    freeLU(lu);
    freeMatrix(id);

    Matrix* m = constructMatrix(2, 2);
    setEntry(m, 0, 0, 3); setEntry(m, 0, 1, 8);
    setEntry(m, 1, 0, 4); setEntry(m, 1, 1, 6);
    LU* lu2 = luDecompose(m);
    CHECK(approx(luDet(lu2), -14.0, 1e-10), "det 2x2");
    freeLU(lu2);
    freeMatrix(m);
}

void testCacheLU() {
    SECTION("cacheLU");
    Matrix* m = constructMatrix(2, 2);
    setEntry(m, 0, 0, 1); setEntry(m, 0, 1, 2);
    setEntry(m, 1, 0, 3); setEntry(m, 1, 1, 4);
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
	    if (getEntry(m, i, j) != 0) { CHECK(false, "all zeros"); freeMatrix(m); return; }
	}
    }
    CHECK(true, "all entries zero");
    freeMatrix(m);
}

void testConstructMatrixFromMatrix() {
    SECTION("constructMatrixFromMatrix");
    double row0[] = {1, 2, 3};
    double row1[] = {4, 5, 6};
    double* rows[] = {row0, row1};

    CHECK(constructMatrixFromMatrix(2, 3, rows, 3, 3) == NULL, "lenData mismatch");
    CHECK(constructMatrixFromMatrix(2, 3, rows, 2, 4) == NULL, "colLenData mismatch");

    Matrix* m = constructMatrixFromMatrix(2, 3, rows, 2, 3);
    CHECK(m != NULL, "valid construct");
    CHECK(getEntry(m, 0, 0) == 1 && getEntry(m, 1, 2) == 6, "values correct");
    freeMatrix(m);
}

void testConstructMatrixFromArray() {
    SECTION("constructMatrixFromArray");
    double arr[] = {1, 2, 3, 4, 5, 6};

    CHECK(constructMatrixFromArray(2, 3, arr, 5) == NULL, "wrong lenData");
    CHECK(constructMatrixFromArray(2, 3, arr, 7) == NULL, "wrong lenData 2");

    Matrix* m = constructMatrixFromArray(2, 3, arr, 6);
    CHECK(m != NULL, "valid construct");
    CHECK(getEntry(m, 0, 0) == 1, "(0,0)");
    CHECK(getEntry(m, 0, 2) == 3, "(0,2)");
    CHECK(getEntry(m, 1, 0) == 4, "(1,0)");
    CHECK(getEntry(m, 1, 2) == 6, "(1,2)");
    freeMatrix(m);
}

void testPrintMatrix() {
    SECTION("printMatrix");
    Matrix* m = constructMatrix(2, 2);
    setEntry(m, 0, 0, 1); setEntry(m, 0, 1, 2);
    setEntry(m, 1, 0, 3); setEntry(m, 1, 1, 4);
    printf("  Visual check (should see 2x2 with 1,2,3,4):\n");
    printMatrix(m);
    CHECK(true, "printMatrix doesn't crash");
    freeMatrix(m);
}

void testCopyMatrix() {
    SECTION("copyMatrix");
    CHECK(copyMatrix(NULL) == NULL, "copy NULL returns NULL");

    Matrix* m = constructMatrix(2, 3);
    setEntry(m, 0, 0, 1.5); setEntry(m, 0, 1, 2.5); setEntry(m, 0, 2, 3.5);
    setEntry(m, 1, 0, 4.5); setEntry(m, 1, 1, 5.5); setEntry(m, 1, 2, 6.5);

    Matrix* c = copyMatrix(m);
    CHECK(c != NULL, "copy succeeds");
    CHECK(c != m, "different pointer");
    CHECK(c->data != m->data, "different data buffer");
    CHECK(matrixComp(m, c, 1e-10), "values equal");

    // Modifying copy doesn't affect original
    setEntry(c, 0, 0, 999);
    CHECK(getEntry(m, 0, 0) == 1.5, "deep copy");
    freeMatrix(m);
    freeMatrix(c);
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

    setEntry(a, 0, 0, 1.0);
    setEntry(c, 0, 0, 1.0 + 1e-15);
    CHECK(matrixComp(a, c, 1e-10), "within tol equal");
    setEntry(c, 0, 0, 1.5);
    CHECK(!matrixComp(a, c, 1e-10), "outside tol not equal");
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
	    double exp = (i == j) ? 1.0 : 0.0;
	    if (getEntry(id, i, j) != exp) { CHECK(false, "id values"); freeMatrix(id); return; }
	}
    }
    CHECK(true, "id values correct");
    freeMatrix(id);
}

/* ---------- Operation tests ---------- */

void testSimpleDotProduct() {
    SECTION("simpleDotProduct");
    double v[] = {1, 2, 3};
    double w[] = {4, 5, 6};
    CHECK(isnan(simpleDotProduct(v, w, 3, 4)), "length mismatch NAN");
    CHECK(simpleDotProduct(v, w, 3, 3) == 32, "1*4+2*5+3*6 = 32");

    double zero[] = {0, 0, 0};
    CHECK(simpleDotProduct(v, zero, 3, 3) == 0, "dot with zero");
}

void testDotProduct() {
    SECTION("dotProduct");
    double va[] = {1, 2, 3};
    double wa[] = {4, 5, 6};
    Matrix* v = constructMatrixFromArray(3, 1, va, 3);
    Matrix* w = constructMatrixFromArray(3, 1, wa, 3);
    CHECK(dotProduct(v, w) == 32, "column dot product");

    Matrix* bad = constructMatrix(3, 2);
    CHECK(isnan(dotProduct(bad, w)), "non-column NAN");
    freeMatrix(bad);

    Matrix* mismatch = constructMatrix(4, 1);
    CHECK(isnan(dotProduct(v, mismatch)), "row mismatch NAN");
    freeMatrix(mismatch);
    freeMatrix(v);
    freeMatrix(w);
}

void testMultByConstant() {
    SECTION("multByConstant");
    double arr[] = {1, 2, 3, 4};
    Matrix* m = constructMatrixFromArray(2, 2, arr, 4);
    Matrix* r = multByConstant(m, 2.5);
    CHECK(getEntry(r, 0, 0) == 2.5, "(0,0) scaled");
    CHECK(getEntry(r, 1, 1) == 10.0, "(1,1) scaled");
    CHECK(getEntry(m, 0, 0) == 1, "original unchanged");

    Matrix* z = multByConstant(m, 0);
    for (int i = 0; i < 4; i++) {
	if (z->data[i] != 0) { CHECK(false, "mult by 0"); freeMatrix(z); freeMatrix(r); freeMatrix(m); return; }
    }
    CHECK(true, "mult by 0 zeros all");
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

    double ad[] = {1, 2, 3, 4};
    double bd[] = {5, 6, 7, 8};
    Matrix* A = constructMatrixFromArray(2, 2, ad, 4);
    Matrix* B = constructMatrixFromArray(2, 2, bd, 4);
    Matrix* S = addMatrices(A, B);
    CHECK(getEntry(S, 0, 0) == 6 && getEntry(S, 1, 1) == 12, "addition correct");

    freeMatrix(a);
    freeMatrix(A);
    freeMatrix(B);
    freeMatrix(S);
}

void testSubtractMatrices() {
    SECTION("subtractMatrices");
    CHECK(subtractMatrices(NULL, NULL) == NULL, "NULL inputs");

    double ad[] = {5, 6, 7, 8};
    double bd[] = {1, 2, 3, 4};
    Matrix* A = constructMatrixFromArray(2, 2, ad, 4);
    Matrix* B = constructMatrixFromArray(2, 2, bd, 4);
    Matrix* D = subtractMatrices(A, B);
    CHECK(getEntry(D, 0, 0) == 4 && getEntry(D, 1, 1) == 4, "subtraction correct");

    Matrix* Z = subtractMatrices(A, A);
    for (int i = 0; i < 4; i++) {
	if (Z->data[i] != 0) { CHECK(false, "self-sub zero"); freeMatrix(Z); freeMatrix(D); freeMatrix(A); freeMatrix(B); return; }
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
    double ad[] = {1, 2, 3, 4};
    Matrix* A = constructMatrixFromArray(2, 2, ad, 4);
    Matrix* I = idMatrix(2);
    Matrix* AI = multiplyMatrices(A, I);
    CHECK(matrixComp(A, AI, 1e-10), "A * I = A");

    // Concrete product
    double xd[] = {1, 2, 3, 4, 5, 6};  // 2x3
    double yd[] = {7, 8, 9, 10, 11, 12};  // 3x2
    Matrix* X = constructMatrixFromArray(2, 3, xd, 6);
    Matrix* Y = constructMatrixFromArray(3, 2, yd, 6);
    Matrix* P = multiplyMatrices(X, Y);
    CHECK(P->numRows == 2 && P->numCols == 2, "product dims");
    CHECK(getEntry(P, 0, 0) == 58, "(0,0) = 1*7+2*9+3*11");
    CHECK(getEntry(P, 0, 1) == 64, "(0,1) = 1*8+2*10+3*12");
    CHECK(getEntry(P, 1, 0) == 139, "(1,0) = 4*7+5*9+6*11");
    CHECK(getEntry(P, 1, 1) == 154, "(1,1) = 4*8+5*10+6*12");

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

    double ad[] = {1, 2, 3, 4};  // 2x2
    double bd[] = {0, 5, 6, 7};  // 2x2
    Matrix* A = constructMatrixFromArray(2, 2, ad, 4);
    Matrix* B = constructMatrixFromArray(2, 2, bd, 4);
    Matrix* T = tensorMatrices(A, B);
    CHECK(T->numRows == 4 && T->numCols == 4, "tensor dims 4x4");
    // Top-left block = 1 * B
    CHECK(getEntry(T, 0, 0) == 0 && getEntry(T, 0, 1) == 5, "block (0,0)");
    CHECK(getEntry(T, 1, 0) == 6 && getEntry(T, 1, 1) == 7, "block (0,0) row 2");
    // Top-right block = 2 * B
    CHECK(getEntry(T, 0, 2) == 0 && getEntry(T, 0, 3) == 10, "block (0,1)");
    // Bottom-right = 4 * B
    CHECK(getEntry(T, 3, 3) == 28, "block (1,1) corner");

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
    double arr[] = {1, 2, 3, 4, 5, 6};
    Matrix* m = constructMatrixFromArray(2, 3, arr, 6);
    Matrix* t = transpose(m);
    CHECK(t->numRows == 3 && t->numCols == 2, "transpose dims");
    CHECK(getEntry(t, 0, 0) == 1, "(0,0)");
    CHECK(getEntry(t, 1, 0) == 2, "(1,0)");
    CHECK(getEntry(t, 2, 1) == 6, "(2,1)");

    // Double transpose returns original
    Matrix* tt = transpose(t);
    CHECK(matrixComp(m, tt, 1e-10), "(A^T)^T = A");

    freeMatrix(m);
    freeMatrix(t);
    freeMatrix(tt);
}

/* ---------- Solve / inverse tests ---------- */

void testLuSolve() {
    SECTION("luSolve");
    CHECK(luSolve(NULL, NULL) == NULL, "NULL inputs");

    // Solve Ax = b for known A, b
    double ad[] = {4, 3, 6, 3};
    Matrix* A = constructMatrixFromArray(2, 2, ad, 4);
    LU* lu = luDecompose(A);

    double bd[] = {10, 12};
    Matrix* b = constructMatrixFromArray(2, 1, bd, 2);
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
    double ad[] = {4, 3, 6, 3};
    Matrix* A = constructMatrixFromArray(2, 2, ad, 4);
    LU* lu2 = luDecompose(A);
    Matrix* invA = luInverse(lu2);
    Matrix* prod = multiplyMatrices(A, invA);
    Matrix* I2 = idMatrix(2);
    CHECK(matrixComp(prod, I2, 1e-9), "A * inv(A) = I (2x2)");
    freeMatrix(A); freeMatrix(invA); freeMatrix(prod); freeMatrix(I2);
    freeLU(lu2);

    // 4x4 case
    double bd[] = {
	2, 1, 0, 0,
	1, 2, 1, 0,
	0, 1, 2, 1,
	0, 0, 1, 2
    };
    Matrix* B = constructMatrixFromArray(4, 4, bd, 16);
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
    CHECK(isnan(determinant(nsq)), "non-square NAN");
    freeMatrix(nsq);

    // 1x1 fast path
    double a1[] = {7.5};
    Matrix* m1 = constructMatrixFromArray(1, 1, a1, 1);
    CHECK(determinant(m1) == 7.5, "1x1 det");
    freeMatrix(m1);

    // 2x2 fast path
    double a2[] = {3, 8, 4, 6};
    Matrix* m2 = constructMatrixFromArray(2, 2, a2, 4);
    CHECK(approx(determinant(m2), -14.0, 1e-10), "2x2 det");
    freeMatrix(m2);

    // 3x3 via LU
    double a3[] = {6, 1, 1, 4, -2, 5, 2, 8, 7};
    Matrix* m3 = constructMatrixFromArray(3, 3, a3, 9);
    CHECK(approx(determinant(m3), -306.0, 1e-9), "3x3 det");
    freeMatrix(m3);

    // Identity
    Matrix* id = idMatrix(5);
    CHECK(approx(determinant(id), 1.0, 1e-10), "det(I_5) = 1");
    freeMatrix(id);
}

void testSolveLinEq() {
    SECTION("solveLinEq");
    CHECK(solveLinEq(NULL, NULL) == NULL, "NULL inputs");

    double ad[] = {3, 2, 1, 2};
    Matrix* A = constructMatrixFromArray(2, 2, ad, 4);
    double bd[] = {5, 5};
    Matrix* b = constructMatrixFromArray(2, 1, bd, 2);
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
    double sd[] = {1, 2, 2, 4};
    Matrix* sing = constructMatrixFromArray(2, 2, sd, 4);
    CHECK(invertMatrix(sing) == NULL, "singular NULL");
    freeMatrix(sing);

    // Real inversion
    double ad[] = {4, 7, 2, 6};
    Matrix* A = constructMatrixFromArray(2, 2, ad, 4);
    Matrix* invA = invertMatrix(A);
    CHECK(invA != NULL, "inverse computed");
    Matrix* prod = multiplyMatrices(A, invA);
    Matrix* I = idMatrix(2);
    CHECK(matrixComp(prod, I, 1e-9), "A * inv(A) = I");
    freeMatrix(A); freeMatrix(invA); freeMatrix(prod); freeMatrix(I);

    // Larger case — use inverse to solve a linear system, cross-check
    double bd[] = {
	1, 2, 3,
	0, 1, 4,
	5, 6, 0
    };
    Matrix* B = constructMatrixFromArray(3, 3, bd, 9);
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
    double id2d[] = {1, 0, 0, 1};
    Matrix* I2 = constructMatrixFromArray(2, 2, id2d, 4);
    double vd[] = {3, 4};
    Matrix* v = constructMatrixFromArray(2, 1, vd, 2);
    Matrix* r = applyMatrix(I2, v);
    CHECK(r != NULL, "apply succeeds");
    CHECK(approx(getEntry(r, 0, 0), 3.0, 1e-10) && approx(getEntry(r, 1, 0), 4.0, 1e-10), "I*v = v");
    freeMatrix(r);
    freeMatrix(I2);
    freeMatrix(v);

    // Diagonal 2x2
    double ad[] = {2, 0, 0, 3};
    Matrix* D = constructMatrixFromArray(2, 2, ad, 4);
    double v2d[] = {1, 2};
    Matrix* v2 = constructMatrixFromArray(2, 1, v2d, 2);
    Matrix* r2 = applyMatrix(D, v2);
    CHECK(approx(getEntry(r2, 0, 0), 2.0, 1e-10) && approx(getEntry(r2, 1, 0), 6.0, 1e-10), "diag * v correct");
    freeMatrix(D); freeMatrix(v2); freeMatrix(r2);

    // Non-square A: 2x3
    double nsad[] = {1, 2, 3, 4, 5, 6};
    Matrix* nsA = constructMatrixFromArray(2, 3, nsad, 6);
    double v3d[] = {1, 0, 1};
    Matrix* v3 = constructMatrixFromArray(3, 1, v3d, 3);
    Matrix* r3 = applyMatrix(nsA, v3);
    CHECK(r3 != NULL && r3->numRows == 2 && r3->numCols == 1, "non-square result shape");
    CHECK(approx(getEntry(r3, 0, 0), 4.0, 1e-10) && approx(getEntry(r3, 1, 0), 10.0, 1e-10), "non-square Av correct");
    freeMatrix(nsA); freeMatrix(v3); freeMatrix(r3);
}

void testMatrixPow() {
    SECTION("matrixPow");
    CHECK(matrixPow(NULL, 2) == NULL, "NULL input");

    Matrix* nsq = constructMatrix(2, 3);
    CHECK(matrixPow(nsq, 2) == NULL, "non-square returns NULL");
    freeMatrix(nsq);

    double ad[] = {1, 2, 3, 4};
    Matrix* A = constructMatrixFromArray(2, 2, ad, 4);

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
    double sd[] = {1, 2, 3, 2, 5, 6, 3, 6, 9};
    Matrix* S = constructMatrixFromArray(3, 3, sd, 9);
    CHECK(isSymmetric(S) == true, "symmetric 3x3");
    freeMatrix(S);

    // Non-symmetric
    double nd[] = {1, 2, 3, 4};
    Matrix* N = constructMatrixFromArray(2, 2, nd, 4);
    CHECK(isSymmetric(N) == false, "non-symmetric 2x2");
    freeMatrix(N);

    Matrix* id = idMatrix(4);
    CHECK(isSymmetric(id) == true, "identity is symmetric");
    freeMatrix(id);

    Matrix* zero = constructMatrix(3, 3);
    CHECK(isSymmetric(zero) == true, "zero matrix is symmetric");
    freeMatrix(zero);
}

void testIsAntisymmetric() {
    SECTION("isAntisymmetric");
    CHECK(isAntisymmetric(NULL) == false, "NULL returns false");

    Matrix* nsq = constructMatrix(2, 3);
    CHECK(isAntisymmetric(nsq) == false, "non-square returns false");
    freeMatrix(nsq);

    // Antisymmetric [[0,1],[-1,0]]
    double ad[] = {0, 1, -1, 0};
    Matrix* A = constructMatrixFromArray(2, 2, ad, 4);
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
    double a3d[] = {0, 2, -3, -2, 0, 1, 3, -1, 0};
    Matrix* A3 = constructMatrixFromArray(3, 3, a3d, 9);
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
    double c = 0.0, s = 1.0;
    double rotd[] = {c, -s, s, c};
    Matrix* R = constructMatrixFromArray(2, 2, rotd, 4);
    CHECK(isOrthogonal(R) == true, "90 deg rotation is orthogonal");
    freeMatrix(R);

    // 45-degree rotation
    double c45 = sqrt(2.0) / 2.0;
    double rot45d[] = {c45, -c45, c45, c45};
    Matrix* R45 = constructMatrixFromArray(2, 2, rot45d, 4);
    CHECK(isOrthogonal(R45) == true, "45 deg rotation is orthogonal");
    freeMatrix(R45);

    // Non-orthogonal
    double nd[] = {2, 0, 0, 1};
    Matrix* N = constructMatrixFromArray(2, 2, nd, 4);
    CHECK(isOrthogonal(N) == false, "scaling matrix not orthogonal");
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
    double rd[] = {1, 2, 2, 4};
    Matrix* R = constructMatrixFromArray(2, 2, rd, 4);
    CHECK(rank(R) == 1, "rank-deficient 2x2 = 1");
    freeMatrix(R);

    // Square with one zero row: rank 2
    double r3d[] = {1, 0, 0, 0, 1, 0, 0, 0, 0};
    Matrix* R3 = constructMatrixFromArray(3, 3, r3d, 9);
    CHECK(rank(R3) == 2, "3x3 rank 2");
    freeMatrix(R3);

    // Non-square full rank: 2x3
    double nsd[] = {1, 0, 0, 0, 1, 0};
    Matrix* NS = constructMatrixFromArray(2, 3, nsd, 6);
    CHECK(rank(NS) == 2, "2x3 full row rank = 2");
    freeMatrix(NS);

    // Non-square rank-deficient: 3x2 rank 1
    double rnd[] = {1, 2, 2, 4, 3, 6};
    Matrix* RN = constructMatrixFromArray(3, 2, rnd, 6);
    CHECK(rank(RN) == 1, "3x2 rank 1");
    freeMatrix(RN);
}

void testNullity() {
    SECTION("nullity");
    CHECK(nullity(NULL) == -1, "NULL returns -1");

    Matrix* id3 = idMatrix(3);
    CHECK(nullity(id3) == 0, "nullity(I_3) = 0");
    freeMatrix(id3);

    // [[1,2],[2,4]]: rank 1, nullity = 2 - 1 = 1
    double rd[] = {1, 2, 2, 4};
    Matrix* R = constructMatrixFromArray(2, 2, rd, 4);
    CHECK(nullity(R) == 1, "nullity of rank-1 2x2 = 1");
    freeMatrix(R);

    // 2x3 full row rank: nullity = 3 - 2 = 1
    double nsd[] = {1, 0, 0, 0, 1, 0};
    Matrix* NS = constructMatrixFromArray(2, 3, nsd, 6);
    CHECK(nullity(NS) == 1, "2x3 nullity = 1");
    freeMatrix(NS);

    Matrix* zero = constructMatrix(3, 4);
    CHECK(nullity(zero) == 4, "zero 3x4: nullity = 4");
    freeMatrix(zero);
}

void testTrace() {
    SECTION("trace");
    CHECK(isnan(trace(NULL)), "NULL returns NAN");

    Matrix* nsq = constructMatrix(2, 3);
    CHECK(isnan(trace(nsq)), "non-square returns NAN");
    freeMatrix(nsq);

    Matrix* id4 = idMatrix(4);
    CHECK(approx(trace(id4), 4.0, 1e-10), "trace(I_4) = 4");
    freeMatrix(id4);

    double ad[] = {1, 2, 3, 4};
    Matrix* A = constructMatrixFromArray(2, 2, ad, 4);
    CHECK(approx(trace(A), 5.0, 1e-10), "trace([[1,2],[3,4]]) = 5");
    freeMatrix(A);

    // Float entries: verify no integer truncation
    double fd[] = {1.5, 0, 0, 2.5};
    Matrix* F = constructMatrixFromArray(2, 2, fd, 4);
    CHECK(approx(trace(F), 4.0, 1e-10), "trace with float diagonal = 4.0");
    freeMatrix(F);

    // Trace equals sum of eigenvalues (2x2 check)
    double ed[] = {3, 1, 0, 5};
    Matrix* E = constructMatrixFromArray(2, 2, ed, 4);
    double* eigs = eigenvalues2x2(E);
    double eigsum = eigs[0] + eigs[1];
    CHECK(approx(trace(E), eigsum, 1e-9), "trace = sum of eigenvalues");
    freeMatrix(E); free(eigs);
}

void testFrobeniusNorm() {
    SECTION("frobeniusNorm");
    CHECK(isnan(frobeniusNorm(NULL)), "NULL returns NAN");

    Matrix* zero = constructMatrix(3, 3);
    CHECK(approx(frobeniusNorm(zero), 0.0, 1e-10), "zero matrix norm = 0");
    freeMatrix(zero);

    Matrix* id3 = idMatrix(3);
    CHECK(approx(frobeniusNorm(id3), sqrt(3.0), 1e-10), "||I_3||_F = sqrt(3)");
    freeMatrix(id3);

    // [[1,2],[3,4]]: sqrt(1+4+9+16) = sqrt(30)
    double ad[] = {1, 2, 3, 4};
    Matrix* A = constructMatrixFromArray(2, 2, ad, 4);
    CHECK(approx(frobeniusNorm(A), sqrt(30.0), 1e-10), "||[[1,2],[3,4]]||_F = sqrt(30)");
    freeMatrix(A);

    // Non-square works: 1x3 [3,4,0] → norm = 5
    double vd[] = {3, 4, 0};
    Matrix* V = constructMatrixFromArray(1, 3, vd, 3);
    CHECK(approx(frobeniusNorm(V), 5.0, 1e-10), "1x3 [3,4,0] norm = 5");
    freeMatrix(V);
}

void testEigenvalues2x2() {
    SECTION("eigenvalues2x2");
    CHECK(eigenvalues2x2(NULL) == NULL, "NULL returns NULL");

    Matrix* nsq = constructMatrix(3, 3);
    CHECK(eigenvalues2x2(nsq) == NULL, "non-2x2 returns NULL");
    freeMatrix(nsq);

    // Identity: both eigenvalues = 1
    Matrix* id = idMatrix(2);
    double* eig_id = eigenvalues2x2(id);
    CHECK(approx(eig_id[0], 1.0, 1e-10) &&
          approx(eig_id[1], 1.0, 1e-10), "eigs(I_2) = {1, 1}");
    freeMatrix(id); free(eig_id);

    // Diagonal [[3,0],[0,2]]: eigs = {3, 2}
    double dd[] = {3, 0, 0, 2};
    Matrix* D = constructMatrixFromArray(2, 2, dd, 4);
    double* eig_d = eigenvalues2x2(D);
    CHECK(approx(eig_d[0], 3.0, 1e-10) &&
          approx(eig_d[1], 2.0, 1e-10), "eigs(diag(3,2)) = {3, 2}");
    free(eig_d);

    // [[5,2],[2,5]]: eigs = {7, 3}; verify via trace/det
    double sd[] = {5, 2, 2, 5};
    Matrix* S = constructMatrixFromArray(2, 2, sd, 4);
    double* eig_s = eigenvalues2x2(S);
    double esum = eig_s[0] + eig_s[1];
    double eprod = eig_s[0] * eig_s[1];
    CHECK(approx(esum, trace(S), 1e-9), "sum of eigs = trace");
    CHECK(approx(eprod, determinant(S), 1e-9), "product of eigs = det");
    freeMatrix(S); free(eig_s);
    freeMatrix(D);

    // Complex eigenvalues: [[0,-1],[1,0]] (rotation 90 deg)
    double cd[] = {0, -1, 1, 0};
    Matrix* C = constructMatrixFromArray(2, 2, cd, 4);
    double* eig_c = eigenvalues2x2(C);
    CHECK(isnan(eig_c[0]) && isnan(eig_c[1]), "complex eigs stored as NAN");
    freeMatrix(C); free(eig_c);

    // Verify each real eigenvalue satisfies det(A - lambda*I) = 0
    double vd[] = {4, 1, 2, 3};
    Matrix* V = constructMatrixFromArray(2, 2, vd, 4);
    double* eig_v = eigenvalues2x2(V);
    for (int k = 0; k < 2; k++) {
        double lam = eig_v[k];
        if (!isnan(lam)) {
            Matrix* lI = multByConstant(idMatrix(2), lam);
            Matrix* AmL = subtractMatrices(V, lI);
            CHECK(approx(determinant(AmL), 0.0, 1e-8), "det(A - lambda*I) = 0");
            freeMatrix(lI); freeMatrix(AmL);
        }
    }
    freeMatrix(V); free(eig_v);
}

void testEigenvalues3x3() {
    SECTION("eigenvalues3x3");
    CHECK(eigenvalues3x3(NULL) == NULL, "NULL returns NULL");

    Matrix* nsq = constructMatrix(2, 2);
    CHECK(eigenvalues3x3(nsq) == NULL, "non-3x3 returns NULL");
    freeMatrix(nsq);

    // Identity: all eigs = 1
    Matrix* id = idMatrix(3);
    double* eig_id = eigenvalues3x3(id);
    CHECK(approx(eig_id[0], 1.0, 1e-9) &&
          approx(eig_id[1], 1.0, 1e-9) &&
          approx(eig_id[2], 1.0, 1e-9), "eigs(I_3) = {1,1,1}");
    freeMatrix(id); free(eig_id);

    // Triple root: 2*I_3, all eigs = 2
    double tid[] = {2,0,0, 0,2,0, 0,0,2};
    Matrix* T = constructMatrixFromArray(3, 3, tid, 9);
    double* eig_t = eigenvalues3x3(T);
    CHECK(approx(eig_t[0], 2.0, 1e-9) &&
          approx(eig_t[1], 2.0, 1e-9) &&
          approx(eig_t[2], 2.0, 1e-9), "triple root 2*I eigs = {2,2,2}");
    freeMatrix(T); free(eig_t);

    // Diagonal [[1,0,0],[0,2,0],[0,0,3]]: verify sum=trace, product=det
    double dd[] = {1,0,0, 0,2,0, 0,0,3};
    Matrix* D = constructMatrixFromArray(3, 3, dd, 9);
    double* eig_d = eigenvalues3x3(D);
    double esum = eig_d[0] + eig_d[1] + eig_d[2];
    double eprod = eig_d[0] * eig_d[1] * eig_d[2];
    CHECK(approx(esum, trace(D), 1e-9), "sum of eigs = trace(diag)");
    CHECK(approx(eprod, determinant(D), 1e-9), "product of eigs = det(diag)");
    // Verify each satisfies det(A - lambda*I) = 0
    for (int k = 0; k < 3; k++) {
        double lam = eig_d[k];
        if (!isnan(lam)) {
            Matrix* lI = multByConstant(idMatrix(3), lam);
            Matrix* AmL = subtractMatrices(D, lI);
            CHECK(approx(determinant(AmL), 0.0, 1e-7), "det(D - lambda*I) = 0");
            freeMatrix(lI); freeMatrix(AmL);
        }
    }
    freeMatrix(D); free(eig_d);

    // One real + two complex: [[1,-1,0],[1,1,0],[0,0,2]] — real root = 2
    double cd[] = {1,-1,0, 1,1,0, 0,0,2};
    Matrix* C = constructMatrixFromArray(3, 3, cd, 9);
    double* eig_c = eigenvalues3x3(C);
    CHECK(approx(eig_c[0], 2.0, 1e-9), "one-real-root case: real root = 2");
    CHECK(isnan(eig_c[1]) && isnan(eig_c[2]), "complex roots stored as NAN");
    freeMatrix(C); free(eig_c);
}

// Free a NULL-tolerant eigenvector array of length n
static void freeEvects(Matrix** evects, int n) {
    if (!evects) return;
    for (int i = 0; i < n; i++) freeMatrix(evects[i]);
    free(evects);
}

// Check Av = lambda*v for a non-NULL eigenvector
static bool checkEigenvector(Matrix* A, double lambda, Matrix* v) {
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
    double* eigs_id = eigenvalues2x2(id);
    Matrix** ev_id = eigenvectors2x2(id);
    CHECK(ev_id != NULL, "identity evects allocated");
    for (int k = 0; k < 2; k++)
        CHECK(checkEigenvector(id, eigs_id[k], ev_id[k]), "identity Av = lambda*v");
    free(eigs_id); freeEvects(ev_id, 2); freeMatrix(id);

    // Diagonal [[3,0],[0,2]]: distinct eigenvalues, axis-aligned eigenvectors
    double dd[] = {3, 0, 0, 2};
    Matrix* D = constructMatrixFromArray(2, 2, dd, 4);
    double* eigs_d = eigenvalues2x2(D);
    Matrix** ev_d = eigenvectors2x2(D);
    CHECK(ev_d != NULL, "diagonal evects allocated");
    CHECK(ev_d[0] != NULL && ev_d[1] != NULL, "both slots non-NULL");
    CHECK(checkEigenvector(D, eigs_d[0], ev_d[0]), "diag ev0: Av = lambda*v");
    CHECK(checkEigenvector(D, eigs_d[1], ev_d[1]), "diag ev1: Av = lambda*v");
    free(eigs_d); freeEvects(ev_d, 2); freeMatrix(D);

    // Symmetric [[5,2],[2,5]]: eigenvalues 7 and 3, evects along [1,1] and [1,-1]
    double sd[] = {5, 2, 2, 5};
    Matrix* S = constructMatrixFromArray(2, 2, sd, 4);
    double* eigs_s = eigenvalues2x2(S);
    Matrix** ev_s = eigenvectors2x2(S);
    CHECK(ev_s != NULL && ev_s[0] != NULL && ev_s[1] != NULL, "symmetric evects non-NULL");
    CHECK(checkEigenvector(S, eigs_s[0], ev_s[0]), "sym ev0: Av = lambda*v");
    CHECK(checkEigenvector(S, eigs_s[1], ev_s[1]), "sym ev1: Av = lambda*v");
    free(eigs_s); freeEvects(ev_s, 2); freeMatrix(S);

    // Upper triangular [[3,1],[0,2]]: eigenvectors non-trivial
    double td[] = {3, 1, 0, 2};
    Matrix* T = constructMatrixFromArray(2, 2, td, 4);
    double* eigs_t = eigenvalues2x2(T);
    Matrix** ev_t = eigenvectors2x2(T);
    CHECK(checkEigenvector(T, eigs_t[0], ev_t[0]), "triangular ev0: Av = lambda*v");
    CHECK(checkEigenvector(T, eigs_t[1], ev_t[1]), "triangular ev1: Av = lambda*v");
    free(eigs_t); freeEvects(ev_t, 2); freeMatrix(T);

    // Complex eigenvalues: both evect slots should be NULL
    double cd[] = {0, -1, 1, 0};
    Matrix* C = constructMatrixFromArray(2, 2, cd, 4);
    Matrix** ev_c = eigenvectors2x2(C);
    CHECK(ev_c != NULL, "complex: array allocated");
    CHECK(ev_c[0] == NULL && ev_c[1] == NULL, "complex: both slots NULL");
    freeEvects(ev_c, 2); freeMatrix(C);
}

void testEigenvectors3x3() {
    SECTION("eigenvectors3x3");
    CHECK(eigenvectors3x3(NULL) == NULL, "NULL returns NULL");

    Matrix* nsq = constructMatrix(2, 2);
    CHECK(eigenvectors3x3(nsq) == NULL, "non-3x3 returns NULL");
    freeMatrix(nsq);

    // Identity: triple eigenvalue 1, A-I = 0, falls back to [1,0,0]
    Matrix* id = idMatrix(3);
    double* eigs_id = eigenvalues3x3(id);
    Matrix** ev_id = eigenvectors3x3(id);
    CHECK(ev_id != NULL, "identity evects allocated");
    for (int k = 0; k < 3; k++)
        CHECK(checkEigenvector(id, eigs_id[k], ev_id[k]), "identity Av = lambda*v");
    free(eigs_id); freeEvects(ev_id, 3); freeMatrix(id);

    // Diagonal [[1,0,0],[0,2,0],[0,0,3]]: axis-aligned eigenvectors
    double dd[] = {1,0,0, 0,2,0, 0,0,3};
    Matrix* D = constructMatrixFromArray(3, 3, dd, 9);
    double* eigs_d = eigenvalues3x3(D);
    Matrix** ev_d = eigenvectors3x3(D);
    CHECK(ev_d != NULL, "diagonal evects allocated");
    CHECK(ev_d[0] != NULL && ev_d[1] != NULL && ev_d[2] != NULL, "all slots non-NULL");
    CHECK(checkEigenvector(D, eigs_d[0], ev_d[0]), "diag ev0: Av = lambda*v");
    CHECK(checkEigenvector(D, eigs_d[1], ev_d[1]), "diag ev1: Av = lambda*v");
    CHECK(checkEigenvector(D, eigs_d[2], ev_d[2]), "diag ev2: Av = lambda*v");
    free(eigs_d); freeEvects(ev_d, 3); freeMatrix(D);

    // Symmetric [[4,1,0],[1,4,1],[0,1,4]]: three real eigenvalues
    double sym[] = {4,1,0, 1,4,1, 0,1,4};
    Matrix* Sym = constructMatrixFromArray(3, 3, sym, 9);
    double* eigs_sym = eigenvalues3x3(Sym);
    Matrix** ev_sym = eigenvectors3x3(Sym);
    CHECK(ev_sym != NULL, "symmetric evects allocated");
    for (int k = 0; k < 3; k++) {
        if (!isnan(eigs_sym[k]))
            CHECK(checkEigenvector(Sym, eigs_sym[k], ev_sym[k]), "sym Av = lambda*v");
    }
    free(eigs_sym); freeEvects(ev_sym, 3); freeMatrix(Sym);

    // Upper triangular [[2,1,3],[0,4,2],[0,0,6]]: eigenvalues on diagonal
    double tri[] = {2,1,3, 0,4,2, 0,0,6};
    Matrix* Tri = constructMatrixFromArray(3, 3, tri, 9);
    double* eigs_tri = eigenvalues3x3(Tri);
    Matrix** ev_tri = eigenvectors3x3(Tri);
    CHECK(ev_tri != NULL, "triangular evects allocated");
    for (int k = 0; k < 3; k++) {
        if (!isnan(eigs_tri[k]))
            CHECK(checkEigenvector(Tri, eigs_tri[k], ev_tri[k]), "triangular Av = lambda*v");
    }
    free(eigs_tri); freeEvects(ev_tri, 3); freeMatrix(Tri);

    // One real + two complex: real eigenvector valid, other two NULL
    double cd[] = {1,-1,0, 1,1,0, 0,0,2};
    Matrix* C = constructMatrixFromArray(3, 3, cd, 9);
    double* eigs_c = eigenvalues3x3(C);
    Matrix** ev_c = eigenvectors3x3(C);
    CHECK(ev_c != NULL, "complex: array allocated");
    CHECK(ev_c[0] != NULL, "complex: real evect non-NULL");
    CHECK(checkEigenvector(C, eigs_c[0], ev_c[0]), "complex: real Av = lambda*v");
    CHECK(ev_c[1] == NULL && ev_c[2] == NULL, "complex: complex slots NULL");
    free(eigs_c); freeEvects(ev_c, 3); freeMatrix(C);
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
    CHECK(getEntry(v, 0, 0) == 0 && getEntry(v, 2, 0) == 0, "entries zero-initialized");
    freeVector(v);

    Vector* big = constructVector(10);
    CHECK(big != NULL && big->numRows == 10, "dim 10 works (no artificial cap)");
    freeVector(big);
}

void testConstructVectorFromArray() {
    SECTION("constructVectorFromArray");
    double data[] = {1.5, -2.5, 3.5};
    CHECK(constructVectorFromArray(0, data, 0) == NULL, "dim 0 NULL");
    CHECK(constructVectorFromArray(3, NULL, 3) == NULL, "NULL data NULL");
    CHECK(constructVectorFromArray(3, data, 2) == NULL, "dataLen mismatch NULL");
    CHECK(constructVectorFromArray(3, data, 4) == NULL, "dataLen mismatch NULL (too big)");

    Vector* v = constructVectorFromArray(3, data, 3);
    CHECK(v != NULL, "valid construct");
    CHECK(v->numRows == 3 && v->numCols == 1, "shape 3x1");
    CHECK(getEntry(v, 0, 0) == 1.5, "entry 0");
    CHECK(getEntry(v, 1, 0) == -2.5, "entry 1");
    CHECK(getEntry(v, 2, 0) == 3.5, "entry 2");
    freeVector(v);
}

void testConstructVector2() {
    SECTION("constructVector2");
    CHECK(constructVector2(NAN, 1.0) == NULL, "NAN x returns NULL");
    CHECK(constructVector2(1.0, NAN) == NULL, "NAN y returns NULL");

    Vector* v = constructVector2(3.0, -4.0);
    CHECK(v != NULL, "valid construct");
    CHECK(v->numRows == 2 && v->numCols == 1, "shape 2x1");
    CHECK(getEntry(v, 0, 0) == 3.0 && getEntry(v, 1, 0) == -4.0, "values correct");
    freeVector(v);
}

void testConstructVector3() {
    SECTION("constructVector3");
    CHECK(constructVector3(NAN, 1.0, 2.0) == NULL, "NAN x returns NULL");
    CHECK(constructVector3(1.0, NAN, 2.0) == NULL, "NAN y returns NULL");
    CHECK(constructVector3(1.0, 2.0, NAN) == NULL, "NAN z returns NULL");

    Vector* v = constructVector3(1.0, 2.0, 3.0);
    CHECK(v != NULL, "valid construct");
    CHECK(v->numRows == 3 && v->numCols == 1, "shape 3x1");
    CHECK(getEntry(v, 0, 0) == 1.0 && getEntry(v, 1, 0) == 2.0 && getEntry(v, 2, 0) == 3.0,
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

    Vector* a = constructVector3(1.0, 2.0, 3.0);
    Vector* b = constructVector3(4.0, 5.0, 6.0);
    Vector* s = addVectors(a, b);
    CHECK(s != NULL, "sum allocated");
    CHECK(approx(getEntry(s, 0, 0), 5.0, 1e-10) &&
          approx(getEntry(s, 1, 0), 7.0, 1e-10) &&
          approx(getEntry(s, 2, 0), 9.0, 1e-10), "sum values correct");

    // v + (-v) = 0
    Vector* neg = negativeVector(a);
    Vector* zero = addVectors(a, neg);
    CHECK(approx(l2Norm(zero), 0.0, 1e-10), "v + (-v) = 0");

    freeVector(a); freeVector(b); freeVector(s);
    freeVector(neg); freeVector(zero);
}

void testVectorDotProduct() {
    SECTION("vectorDotProduct");
    CHECK(isnan(vectorDotProduct(NULL, NULL)), "NULL inputs NAN");

    Vector* v2 = constructVector(2);
    Vector* v3 = constructVector(3);
    CHECK(isnan(vectorDotProduct(v2, v3)), "dim mismatch NAN");
    freeVector(v2);

    Matrix* row = constructMatrix(1, 3);
    CHECK(isnan(vectorDotProduct(row, v3)), "row matrix rejected");
    freeMatrix(row);
    freeVector(v3);

    Vector* a = constructVector3(1.0, 2.0, 3.0);
    Vector* b = constructVector3(4.0, 5.0, 6.0);
    CHECK(approx(vectorDotProduct(a, b), 32.0, 1e-10), "1*4 + 2*5 + 3*6 = 32");

    // Commutative
    CHECK(approx(vectorDotProduct(a, b), vectorDotProduct(b, a), 1e-10), "commutative");

    // v . v = |v|^2
    CHECK(approx(vectorDotProduct(a, a), 14.0, 1e-10), "v . v = sum of squares");

    // Orthogonal
    Vector* e1 = constructVector3(1.0, 0.0, 0.0);
    Vector* e2 = constructVector3(0.0, 1.0, 0.0);
    CHECK(approx(vectorDotProduct(e1, e2), 0.0, 1e-10), "orthogonal basis vectors dot = 0");

    freeVector(a); freeVector(b); freeVector(e1); freeVector(e2);
}

void testCrossProduct() {
    SECTION("crossProduct");
    CHECK(crossProduct(NULL, NULL) == NULL, "NULL inputs");

    Vector* v2 = constructVector2(1.0, 2.0);
    Vector* v3 = constructVector3(1.0, 2.0, 3.0);
    CHECK(crossProduct(v2, v3) == NULL, "dim mismatch NULL");
    CHECK(crossProduct(v2, v2) == NULL, "2D vectors not allowed");
    freeVector(v2);

    // Standard basis: e1 x e2 = e3
    Vector* e1 = constructVector3(1.0, 0.0, 0.0);
    Vector* e2 = constructVector3(0.0, 1.0, 0.0);
    Vector* e3 = constructVector3(0.0, 0.0, 1.0);

    Vector* e1xe2 = crossProduct(e1, e2);
    CHECK(approx(getEntry(e1xe2, 0, 0), 0.0, 1e-10) &&
          approx(getEntry(e1xe2, 1, 0), 0.0, 1e-10) &&
          approx(getEntry(e1xe2, 2, 0), 1.0, 1e-10), "e1 x e2 = e3");

    // e2 x e3 = e1
    Vector* e2xe3 = crossProduct(e2, e3);
    CHECK(approx(getEntry(e2xe3, 0, 0), 1.0, 1e-10) &&
          approx(getEntry(e2xe3, 1, 0), 0.0, 1e-10) &&
          approx(getEntry(e2xe3, 2, 0), 0.0, 1e-10), "e2 x e3 = e1");

    // e3 x e1 = e2
    Vector* e3xe1 = crossProduct(e3, e1);
    CHECK(approx(getEntry(e3xe1, 0, 0), 0.0, 1e-10) &&
          approx(getEntry(e3xe1, 1, 0), 1.0, 1e-10) &&
          approx(getEntry(e3xe1, 2, 0), 0.0, 1e-10), "e3 x e1 = e2");

    // Anti-commutative: a x b = -(b x a)
    Vector* a = constructVector3(1.0, 2.0, 3.0);
    Vector* b = constructVector3(4.0, 5.0, 6.0);
    Vector* axb = crossProduct(a, b);
    Vector* bxa = crossProduct(b, a);
    Vector* negBxa = negativeVector(bxa);
    CHECK(matrixComp(axb, negBxa, 1e-10), "a x b = -(b x a)");

    // Cross product is orthogonal to both inputs
    CHECK(approx(vectorDotProduct(axb, a), 0.0, 1e-10), "(a x b) . a = 0");
    CHECK(approx(vectorDotProduct(axb, b), 0.0, 1e-10), "(a x b) . b = 0");

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
    Vector* v = constructVector2(3.0, 4.0);
    CHECK(approx(l2Norm(v), 5.0, 1e-10), "||[3,4]|| = 5");
    freeVector(v);

    // 3D: sqrt(1 + 4 + 9) = sqrt(14)
    Vector* v3 = constructVector3(1.0, 2.0, 3.0);
    CHECK(approx(l2Norm(v3), sqrt(14.0), 1e-10), "||[1,2,3]|| = sqrt(14)");
    freeVector(v3);

    // Non-integer components — exercises the int-truncation bug that used to exist
    Vector* vf = constructVector2(0.3, 0.4);
    CHECK(approx(l2Norm(vf), 0.5, 1e-10), "||[0.3, 0.4]|| = 0.5");
    freeVector(vf);

    // Negative components
    Vector* vn = constructVector3(-1.0, -2.0, -2.0);
    CHECK(approx(l2Norm(vn), 3.0, 1e-10), "||[-1,-2,-2]|| = 3");
    freeVector(vn);
}

void testNegativeVector() {
    SECTION("negativeVector");
    CHECK(negativeVector(NULL) == NULL, "NULL returns NULL");

    Vector* v = constructVector3(1.0, -2.0, 3.0);
    Vector* neg = negativeVector(v);
    CHECK(neg != NULL, "allocated");
    CHECK(approx(getEntry(neg, 0, 0), -1.0, 1e-10) &&
          approx(getEntry(neg, 1, 0),  2.0, 1e-10) &&
          approx(getEntry(neg, 2, 0), -3.0, 1e-10), "components negated");

    // Double negation returns original
    Vector* negNeg = negativeVector(neg);
    CHECK(matrixComp(v, negNeg, 1e-10), "-(-v) = v");

    // Original unchanged
    CHECK(getEntry(v, 0, 0) == 1.0, "original unchanged");

    freeVector(v); freeVector(neg); freeVector(negNeg);
}

void testScaleVector() {
    SECTION("scaleVector");
    CHECK(scaleVector(NULL, 2.0) == NULL, "NULL vector NULL");

    Vector* v = constructVector3(1.0, 2.0, 3.0);
    CHECK(scaleVector(v, NAN) == NULL, "NAN scalar NULL");

    Vector* twice = scaleVector(v, 2.0);
    CHECK(approx(getEntry(twice, 0, 0), 2.0, 1e-10) &&
          approx(getEntry(twice, 1, 0), 4.0, 1e-10) &&
          approx(getEntry(twice, 2, 0), 6.0, 1e-10), "scale by 2");

    Vector* zero = scaleVector(v, 0.0);
    CHECK(approx(l2Norm(zero), 0.0, 1e-10), "scale by 0 = zero vector");

    Vector* negOne = scaleVector(v, -1.0);
    Vector* neg = negativeVector(v);
    CHECK(matrixComp(negOne, neg, 1e-10), "scale by -1 = negativeVector");

    // Original unchanged
    CHECK(getEntry(v, 0, 0) == 1.0, "original unchanged");

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

    Vector* a = constructVector3(5.0, 7.0, 9.0);
    Vector* b = constructVector3(1.0, 2.0, 3.0);
    Vector* d = subtractVectors(a, b);
    CHECK(approx(getEntry(d, 0, 0), 4.0, 1e-10) &&
          approx(getEntry(d, 1, 0), 5.0, 1e-10) &&
          approx(getEntry(d, 2, 0), 6.0, 1e-10), "a - b correct");

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
    Vector* e1 = constructVector3(1.0, 0.0, 0.0);
    Vector* u1 = normalizeVector(e1);
    CHECK(approx(l2Norm(u1), 1.0, 1e-10), "unit stays unit");
    CHECK(matrixComp(e1, u1, 1e-10), "unit vector unchanged");
    freeVector(e1); freeVector(u1);

    // 3-4-5 triangle normalizes to (0.6, 0.8)
    Vector* v = constructVector2(3.0, 4.0);
    Vector* u = normalizeVector(v);
    CHECK(approx(l2Norm(u), 1.0, 1e-10), "norm of result = 1");
    CHECK(approx(getEntry(u, 0, 0), 0.6, 1e-10) &&
          approx(getEntry(u, 1, 0), 0.8, 1e-10), "[3,4] normalized");
    freeVector(v); freeVector(u);

    // Direction preserved
    Vector* w = constructVector3(2.0, -4.0, 4.0);
    Vector* uw = normalizeVector(w);
    CHECK(approx(l2Norm(uw), 1.0, 1e-10), "norm 1");
    // w / 6 should equal uw (since |w| = 6)
    Vector* wDiv6 = scaleVector(w, 1.0 / 6.0);
    CHECK(matrixComp(uw, wDiv6, 1e-10), "direction preserved");
    freeVector(w); freeVector(uw); freeVector(wDiv6);
}

void testVectorDistance() {
    SECTION("vectorDistance");
    CHECK(isnan(vectorDistance(NULL, NULL)), "NULL inputs NAN");

    Vector* v2 = constructVector(2);
    Vector* v3 = constructVector(3);
    CHECK(isnan(vectorDistance(v2, v3)), "dim mismatch NAN");
    freeVector(v2); freeVector(v3);

    // Distance to self is 0
    Vector* p = constructVector3(1.0, 2.0, 3.0);
    CHECK(approx(vectorDistance(p, p), 0.0, 1e-10), "dist(p, p) = 0");

    // 3-4-5: from origin to (3,4)
    Vector* origin2 = constructVector2(0.0, 0.0);
    Vector* p34 = constructVector2(3.0, 4.0);
    CHECK(approx(vectorDistance(origin2, p34), 5.0, 1e-10), "dist origin to [3,4] = 5");

    // Symmetric
    CHECK(approx(vectorDistance(origin2, p34), vectorDistance(p34, origin2), 1e-10),
          "dist symmetric");

    // (1,2,3) to (4,6,3): dx=3, dy=4, dz=0 → 5
    Vector* q = constructVector3(4.0, 6.0, 3.0);
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

    Vector* zero = constructVector3(0.0, 0.0, 0.0);
    Vector* nonzero = constructVector3(1.0, 0.0, 0.0);
    CHECK(isnan(vectorAngle(zero, nonzero)), "zero vector NAN");
    freeVector(zero); freeVector(v3);

    // Parallel: angle = 0
    Vector* a = constructVector3(1.0, 2.0, 3.0);
    Vector* a2 = scaleVector(a, 2.5);
    CHECK(approx(vectorAngle(a, a2), 0.0, 1e-9), "parallel: angle 0");
    freeVector(a2);

    // Anti-parallel: angle = pi
    Vector* aNeg = negativeVector(a);
    CHECK(approx(vectorAngle(a, aNeg), M_PI, 1e-9), "anti-parallel: angle pi");
    freeVector(aNeg);

    // Perpendicular: angle = pi/2
    Vector* e1 = constructVector3(1.0, 0.0, 0.0);
    Vector* e2 = constructVector3(0.0, 1.0, 0.0);
    CHECK(approx(vectorAngle(e1, e2), M_PI / 2.0, 1e-9), "perpendicular: pi/2");

    // Symmetric
    CHECK(approx(vectorAngle(e1, e2), vectorAngle(e2, e1), 1e-10), "angle symmetric");

    // 45 degrees: (1,0) vs (1,1)
    Vector* w1 = constructVector2(1.0, 0.0);
    Vector* w2 = constructVector2(1.0, 1.0);
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

    Vector* zero = constructVector3(0.0, 0.0, 0.0);
    Vector* nonzero = constructVector3(1.0, 1.0, 1.0);
    CHECK(vectorProjectOnto(nonzero, zero) == NULL, "project onto zero NULL");
    freeVector(zero);

    // proj of (3, 4) onto x-axis = (3, 0)
    Vector* v = constructVector2(3.0, 4.0);
    Vector* xAxis = constructVector2(1.0, 0.0);
    Vector* p = vectorProjectOnto(v, xAxis);
    CHECK(approx(getEntry(p, 0, 0), 3.0, 1e-10) &&
          approx(getEntry(p, 1, 0), 0.0, 1e-10), "proj onto x-axis");
    freeVector(p);

    // Projection onto a non-unit vector: scale-invariant on target
    Vector* xScaled = constructVector2(5.0, 0.0);
    Vector* pScaled = vectorProjectOnto(v, xScaled);
    CHECK(approx(getEntry(pScaled, 0, 0), 3.0, 1e-10) &&
          approx(getEntry(pScaled, 1, 0), 0.0, 1e-10), "proj invariant under target scaling");
    freeVector(pScaled);
    freeVector(xScaled);

    // Projection is parallel to target
    Vector* a = constructVector3(1.0, 2.0, 3.0);
    Vector* b = constructVector3(2.0, 1.0, 0.0);
    Vector* projAB = vectorProjectOnto(a, b);
    Vector* crossPB = crossProduct(projAB, b);
    CHECK(approx(l2Norm(crossPB), 0.0, 1e-9), "projection is parallel to target");
    freeVector(crossPB);

    // Residual (a - proj) is orthogonal to b
    Vector* resid = subtractVectors(a, projAB);
    CHECK(approx(vectorDotProduct(resid, b), 0.0, 1e-9), "a - proj_b(a) is perpendicular to b");
    freeVector(resid);

    // Projecting a vector onto itself returns itself
    Vector* projSelf = vectorProjectOnto(a, a);
    CHECK(matrixComp(projSelf, a, 1e-10), "proj of v onto v = v");
    freeVector(projSelf);

    freeVector(v); freeVector(xAxis);
    freeVector(nonzero); freeVector(a); freeVector(b); freeVector(projAB);
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
    testCrossProduct();
    testL2Norm();
    testNegativeVector();
    testScaleVector();
    testSubtractVectors();
    testNormalizeVector();
    testVectorDistance();
    testVectorAngle();
    testVectorProjectOnto();

    printf("\n========================================\n");
    printf("Results: %d / %d tests passed\n", testsPassed, testsRun);
    printf("========================================\n");

    return (testsPassed == testsRun) ? 0 : 1;
}
