#ifndef SOKKO_H
#define SOKKO_H
#include <stdbool.h>
#include <stddef.h>
#include "hebi.h"

/* ---------- Definition of structs ---------- */
typedef struct MatrixElement {
    union {
        long double real;
        ComplexNumber complex;
    } value;
    bool isComplex;
} MatrixElement;

typedef struct LU {
    MatrixElement* data;    // L stored below diagonal, U on and above
    int* perm;              // perm[i] = which original row is now at row i
    int sign;               // +1 or -1, parity of permutation
    size_t n;
} LU;

typedef struct Matrix {
    size_t numRows;
    size_t numCols;
    MatrixElement *data;
    LU * cachedLU;  // The LU struct representing the LU decomp of Matrix
} Matrix;

typedef Matrix Vector;

/* ---------- Helper methods ---------- */
bool isSquare(Matrix* matrix);

/* ---------- Free methods ---------- */
void freeLU(LU* lu);
void resetMatrixCache(Matrix* matrix);
void freeMatrix(Matrix* matrix);

/* ---------- Accessors ---------- */
MatrixElement getEntry(const Matrix* matrix, size_t i, size_t j);
void setEntry(Matrix* matrix, size_t i, size_t j, MatrixElement x);

/* ---------- MatrixElement operations ---------- */
MatrixElement elemFromReal(long double x);
MatrixElement elemFromComplex(ComplexNumber c);
ComplexNumber elemToComplex(MatrixElement a);
MatrixElement elemAdd(MatrixElement a, MatrixElement b);
MatrixElement elemSub(MatrixElement a, MatrixElement b);
MatrixElement elemMul(MatrixElement a, MatrixElement b);
MatrixElement elemDiv(MatrixElement a, MatrixElement b);
MatrixElement elemNeg(MatrixElement a);
MatrixElement elemConj(MatrixElement a);
long double elemAbs(MatrixElement a);
bool elemIsZero(MatrixElement a, long double tol);
bool elemIsNan(MatrixElement a);
bool elemEq(MatrixElement a, MatrixElement b, long double tol);

/* ---------- LU methods ----------- */
MatrixElement luDet(LU* lu);
LU* luDecompose(Matrix* matrix);
void cacheLU(Matrix* matrix);
Matrix* luSolve(LU* lu, Matrix* b);
Matrix* luInverse(LU* lu);

/* ----------- Main methods ---------- */
Matrix* constructMatrix(size_t numRows, size_t numCols);
Matrix* constructMatrixFromMatrix(size_t numRows, size_t numCols, MatrixElement** data, size_t lenData, size_t colLenData);
Matrix* constructMatrixFromArray(size_t numRows, size_t numCols, MatrixElement* data, size_t lenData);
void printMatrix(Matrix* matrix);
Matrix* copyMatrix(Matrix* matrix);
bool matrixComp(Matrix* A, Matrix* B, long double tol);
Matrix* idMatrix(size_t n);

/* ---------- Basic operations ---------- */
MatrixElement simpleDotProduct(MatrixElement* v, MatrixElement* w, size_t vlen, size_t wlen);
MatrixElement dotProduct(Matrix* v, Matrix* w);
Matrix* applyMatrix(Matrix* A, Matrix* v);
Matrix* multByConstant(Matrix* matrix, MatrixElement c);
Matrix* matrixPow(Matrix* matrix, int n);
Matrix* addMatrices(Matrix* A, Matrix* B);
Matrix* subtractMatrices(Matrix* A, Matrix* B);
Matrix* multiplyMatrices(Matrix* A, Matrix* B);
Matrix* tensorMatrices(Matrix* A, Matrix* B);
Matrix* transpose(Matrix* matrix);
Matrix* adjoint(Matrix* matrix);

/* ---------- Vector methods ---------- */
void freeVector(Vector* vector);
Vector* constructVector(size_t dim);
Vector* constructVectorFromArray(size_t dim, MatrixElement* data, size_t dataLen);
Vector* constructVector2(MatrixElement x, MatrixElement y);
Vector* constructVector3(MatrixElement x, MatrixElement y, MatrixElement z);
Vector* addVectors(Vector* v1, Vector* v2);
MatrixElement vectorDotProduct(Vector* v1, Vector* v2);
MatrixElement hermitianDotProduct(Vector* v1, Vector* v2);
Vector* crossProduct(Vector* v1, Vector* v2);
long double l2Norm(Vector* vect);
Vector* negativeVector(Vector* vect);
Vector* scaleVector(Vector* vect, MatrixElement k);
Vector* subtractVectors(Vector* v1, Vector* v2);
Vector* normalizeVector(Vector* vect);
long double vectorDistance(Vector* v1, Vector* v2);
long double vectorAngle(Vector* v1, Vector* v2);
Vector* vectorProjectOnto(Vector* v1, Vector* v2);

/* ---------- Matrix invariants ---------- */
bool isSymmetric(Matrix* matrix);
bool isAntisymmetric(Matrix* matrix);
bool isOrthogonal(Matrix* matrix);
bool isUnitary(Matrix* matrix);
int rank(Matrix* matrix);
int nullity(Matrix* matrix);
MatrixElement trace(Matrix* matrix);
long double frobeniusNorm(Matrix* matrix);
MatrixElement determinant(Matrix* matrix);

/* ---------- Matrix computations ---------- */
Matrix* solveLinEq(Matrix* matrix, Matrix* b);
Matrix* invertMatrix(Matrix* matrix);
Matrix* reduceRows(Matrix* matrix);
Matrix* reduceColumns(Matrix* matrix);
Vector** columnSpace(Matrix* matrix, size_t* count);
Vector** rowSpace(Matrix* matrix, size_t* count);
MatrixElement* eigenvalues2x2(Matrix* matrix);
MatrixElement* eigenvalues3x3(Matrix* matrix);
Matrix** eigenvectors2x2(Matrix* matrix);
Matrix** eigenvectors3x3(Matrix* matrix);
MatrixElement* eigenvalues(Matrix* matrix);
Matrix** eigenvectors(Matrix* matrix);

#endif
