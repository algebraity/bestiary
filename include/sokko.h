#ifndef SOKKO_H
#define SOKKO_H
#include <stdbool.h>
#include <stddef.h>
#include "hebi.h"

typedef struct LU {
    Field* field;
    FieldElement* data;    // L stored below diagonal, U on and above
    size_t* perm;          // perm[i] = which original row is now at row i
    int sign;              // +1 or -1, parity of permutation
    size_t n;
} LU;

typedef struct Matrix {
    Field* field;
    bool ownsField;
    size_t numRows;
    size_t numCols;
    FieldElement* data;
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
FieldElement getEntry(const Matrix* matrix, size_t i, size_t j);
void setEntry(Matrix* matrix, size_t i, size_t j, FieldElement x);

/* ---------- FieldElement convenience operations ---------- */
Field* sokkoRealField(void);
Field* sokkoComplexField(void);
FieldElement elemFromReal(long double x);
FieldElement elemFromComplex(ComplexNumber c);
ComplexNumber elemToComplex(FieldElement a);
FieldElement elemAdd(FieldElement a, FieldElement b);
FieldElement elemSub(FieldElement a, FieldElement b);
FieldElement elemMul(FieldElement a, FieldElement b);
FieldElement elemDiv(FieldElement a, FieldElement b);
FieldElement elemNeg(FieldElement a);
FieldElement elemConj(FieldElement a);
long double elemAbs(FieldElement a);
bool elemIsZero(FieldElement a, long double tol);
bool elemIsComplex(FieldElement x);
bool elemIsNan(FieldElement a);
bool elemEq(FieldElement a, FieldElement b, long double tol);

/* ---------- LU methods ----------- */
FieldElement luDet(LU* lu);
LU* luDecompose(Matrix* matrix);
void cacheLU(Matrix* matrix);
Matrix* luSolve(LU* lu, Matrix* b);
Matrix* luInverse(LU* lu);

/* ----------- Main methods ---------- */
Matrix* constructMatrix(size_t numRows, size_t numCols);
Matrix* constructMatrixOverField(Field* field, size_t numRows, size_t numCols);
Matrix* constructMatrixFromMatrix(size_t numRows, size_t numCols, FieldElement** data, size_t lenData, size_t colLenData);
Matrix* constructMatrixFromMatrixOverField(Field* field, size_t numRows, size_t numCols, FieldElement** data, size_t lenData, size_t colLenData);
Matrix* constructMatrixFromArray(size_t numRows, size_t numCols, FieldElement* data, size_t lenData);
Matrix* constructMatrixFromArrayOverField(Field* field, size_t numRows, size_t numCols, FieldElement* data, size_t lenData);
void printMatrix(Matrix* matrix);
Matrix* copyMatrix(Matrix* matrix);
bool matrixComp(Matrix* A, Matrix* B, long double tol);
Matrix* idMatrix(size_t n);
Matrix* idMatrixOverField(Field* field, size_t n);
void freeVectorBasis(Vector** basis, size_t count);

/* ---------- Basic operations ---------- */
FieldElement simpleDotProduct(FieldElement* v, FieldElement* w, size_t vlen, size_t wlen);
FieldElement dotProduct(Matrix* v, Matrix* w);
Matrix* applyMatrix(Matrix* A, Matrix* v);
Matrix* multByConstant(Matrix* matrix, FieldElement c);
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
Vector* constructVectorOverField(Field* field, size_t dim);
Vector* constructVectorFromArray(size_t dim, FieldElement* data, size_t dataLen);
Vector* constructVectorFromArrayOverField(Field* field, size_t dim, FieldElement* data, size_t dataLen);
Vector* constructVector2(FieldElement x, FieldElement y);
Vector* constructVector3(FieldElement x, FieldElement y, FieldElement z);
Vector* addVectors(Vector* v1, Vector* v2);
FieldElement vectorDotProduct(Vector* v1, Vector* v2);
FieldElement hermitianDotProduct(Vector* v1, Vector* v2);
Vector* crossProduct(Vector* v1, Vector* v2);
long double l2Norm(Vector* vect);
Vector* negativeVector(Vector* vect);
Vector* scaleVector(Vector* vect, FieldElement k);
Vector* subtractVectors(Vector* v1, Vector* v2);
Vector* normalizeVector(Vector* vect);
long double vectorDistance(Vector* v1, Vector* v2);
long double vectorAngle(Vector* v1, Vector* v2);
Vector* vectorProjectOnto(Vector* v1, Vector* v2);
bool setMatrixColumn(Matrix* matrix, size_t column, Vector* vector);
bool setMatrixRowBlock(Matrix* target, size_t startRow, Matrix* block);
Matrix* matrixFromColumns(Vector** columns, size_t count);

/* ---------- Matrix invariants ---------- */
bool isSymmetric(Matrix* matrix);
bool isAntisymmetric(Matrix* matrix);
bool isOrthogonal(Matrix* matrix);
bool isUnitary(Matrix* matrix);
size_t rank(Matrix* matrix);
size_t nullity(Matrix* matrix);
FieldElement trace(Matrix* matrix);
long double frobeniusNorm(Matrix* matrix);
FieldElement determinant(Matrix* matrix);

/* ---------- Matrix computations ---------- */
Matrix* solveLinEq(Matrix* matrix, Matrix* b);
Matrix* invertMatrix(Matrix* matrix);
Matrix* reduceRows(Matrix* matrix);
Matrix* reduceColumns(Matrix* matrix);
Vector** nullSpace(Matrix* matrix, size_t* count);
Vector** columnSpace(Matrix* matrix, size_t* count);
Vector** rowSpace(Matrix* matrix, size_t* count);
FieldElement* eigenvalues2x2(Matrix* matrix);
FieldElement* eigenvalues3x3(Matrix* matrix);
Matrix** eigenvectors2x2(Matrix* matrix);
Matrix** eigenvectors3x3(Matrix* matrix);
FieldElement* eigenvalues(Matrix* matrix);
Matrix** eigenvectors(Matrix* matrix);

#endif
