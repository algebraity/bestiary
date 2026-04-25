#ifndef SOKKO_H
#define SOKKO_H
#include <stdbool.h>
#include "hebi.h"

/* ---------- Definition of structs ---------- */
typedef struct MatrixElement {
    union {
        double real;
        ComplexNumber complex;
    } value;
    bool isComplex;
} MatrixElement;

typedef struct LU {
    MatrixElement* data;    // L stored below diagonal, U on and above
    int* perm;              // perm[i] = which original row is now at row i
    int sign;               // +1 or -1, parity of permutation
    int n;
} LU;

typedef struct Matrix {
    int numRows;
    int numCols;
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
MatrixElement getEntry(const Matrix* matrix, int i, int j);
void setEntry(Matrix* matrix, int i, int j, MatrixElement x);

/* ---------- MatrixElement operations ---------- */
MatrixElement elemFromReal(double x);
MatrixElement elemFromComplex(ComplexNumber c);
ComplexNumber elemToComplex(MatrixElement a);
MatrixElement elemAdd(MatrixElement a, MatrixElement b);
MatrixElement elemSub(MatrixElement a, MatrixElement b);
MatrixElement elemMul(MatrixElement a, MatrixElement b);
MatrixElement elemDiv(MatrixElement a, MatrixElement b);
MatrixElement elemNeg(MatrixElement a);
MatrixElement elemConj(MatrixElement a);
double elemAbs(MatrixElement a);
bool elemIsZero(MatrixElement a, double tol);
bool elemIsNan(MatrixElement a);
bool elemEq(MatrixElement a, MatrixElement b, double tol);

/* ---------- LU methods ----------- */
MatrixElement luDet(LU* lu);
LU* luDecompose(Matrix* matrix);
void cacheLU(Matrix* matrix);
Matrix* luSolve(LU* lu, Matrix* b);
Matrix* luInverse(LU* lu);

/* ----------- Main methods ---------- */
Matrix* constructMatrix(int numRows, int numCols);
Matrix* constructMatrixFromMatrix(int numRows, int numCols, MatrixElement** data, int lenData, int colLenData);
Matrix* constructMatrixFromArray(int numRows, int numCols, MatrixElement* data, int lenData);
void printMatrix(Matrix* matrix);
Matrix* copyMatrix(Matrix* matrix);
bool matrixComp(Matrix* A, Matrix* B, double tol);
Matrix* idMatrix(int n);

/* ---------- Basic operations ---------- */
MatrixElement simpleDotProduct(MatrixElement* v, MatrixElement* w, int vlen, int wlen);
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
Vector* constructVector(int dim);
Vector* constructVectorFromArray(int dim, MatrixElement* data, int dataLen);
Vector* constructVector2(MatrixElement x, MatrixElement y);
Vector* constructVector3(MatrixElement x, MatrixElement y, MatrixElement z);
Vector* addVectors(Vector* v1, Vector* v2);
MatrixElement vectorDotProduct(Vector* v1, Vector* v2);
MatrixElement hermitianDotProduct(Vector* v1, Vector* v2);
Vector* crossProduct(Vector* v1, Vector* v2);
double l2Norm(Vector* vect);
Vector* negativeVector(Vector* vect);
Vector* scaleVector(Vector* vect, MatrixElement k);
Vector* subtractVectors(Vector* v1, Vector* v2);
Vector* normalizeVector(Vector* vect);
double vectorDistance(Vector* v1, Vector* v2);
double vectorAngle(Vector* v1, Vector* v2);
Vector* vectorProjectOnto(Vector* v1, Vector* v2);

/* ---------- Matrix invariants ---------- */
bool isSymmetric(Matrix* matrix);
bool isAntisymmetric(Matrix* matrix);
bool isOrthogonal(Matrix* matrix);
bool isUnitary(Matrix* matrix);
int rank(Matrix* matrix);
int nullity(Matrix* matrix);
MatrixElement trace(Matrix* matrix);
double frobeniusNorm(Matrix* matrix);
MatrixElement determinant(Matrix* matrix);

/* ---------- Matrix computations ---------- */
Matrix* solveLinEq(Matrix* matrix, Matrix* b);
Matrix* invertMatrix(Matrix* matrix);
Matrix* reduceRows(Matrix* matrix);
Matrix* reduceColumns(Matrix* matrix);
Vector** columnSpace(Matrix* matrix, int* count);
Vector** rowSpace(Matrix* matrix, int* count);
MatrixElement* eigenvalues2x2(Matrix* matrix);
MatrixElement* eigenvalues3x3(Matrix* matrix);
Matrix** eigenvectors2x2(Matrix* matrix);
Matrix** eigenvectors3x3(Matrix* matrix);
MatrixElement* eigenvalues(Matrix* matrix);
Matrix** eigenvectors(Matrix* matrix);

#endif
