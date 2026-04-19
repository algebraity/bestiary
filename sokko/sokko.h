#ifndef SOKKO_H
#define SOKKO_H
#include <stdbool.h>
#include "hebi.h"

/* ---------- Definition of structs ---------- */
typedef struct LU {
    double* data;    // L stored below diagonal, U on and above
    int* perm;       // perm[i] = which original row is now at row i
    int sign;        // +1 or -1, parity of permutation
    int n;
} LU;

typedef struct Matrix {
    int numRows;
    int numCols;
    double *data;
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
double getEntry(const Matrix* matrix, int i, int j);
void setEntry(Matrix* matrix, int i, int j, double x);

/* ---------- LU methods ----------- */
double luDet(LU* lu);
LU* luDecompose(Matrix* matrix);
void cacheLU(Matrix* matrix);
Matrix* luSolve(LU* lu, Matrix* b);
Matrix* luInverse(LU* lu);

/* ----------- Main methods ---------- */
Matrix* constructMatrix(int numRows, int numCols);
Matrix* constructMatrixFromMatrix(int numRows, int numCols, double** data, int lenData, int colLenData);
Matrix* constructMatrixFromArray(int numRows, int numCols, double* data, int lenData);
void printMatrix(Matrix* matrix);
Matrix* copyMatrix(Matrix* matrix);
bool matrixComp(Matrix* A, Matrix* B, double tol);
Matrix* idMatrix(int n);

/* ---------- Basic operations ---------- */
double simpleDotProduct(double* v, double* w, int vlen, int wlen);
double dotProduct(Matrix* v, Matrix* w);
Matrix* applyMatrix(Matrix* A, Matrix* v);
Matrix* multByConstant(Matrix* matrix, double c);
Matrix* matrixPow(Matrix* matrix, int n);
Matrix* addMatrices(Matrix* A, Matrix* B);
Matrix* subtractMatrices(Matrix* A, Matrix* B);
Matrix* multiplyMatrices(Matrix* A, Matrix* B);
Matrix* tensorMatrices(Matrix* A, Matrix* B);
Matrix* transpose(Matrix* matrix);

/* ---------- Vector methods ---------- */
void freeVector(Vector* vector);
Vector* constructVector(int dim);
Vector* constructVectorFromArray(int dim, double* data, int dataLen);
Vector* constructVector2(double x, double y);
Vector* constructVector3(double x, double y, double z);
Vector* addVectors(Vector* v1, Vector* v2);
double vectorDotProduct(Vector* v1, Vector* v2);
Vector* crossProduct(Vector* v1, Vector* v2);
double l2Norm(Vector* vect);
Vector* negativeVector(Vector* vect);
Vector* scaleVector(Vector* vect, double k);
Vector* subtractVectors(Vector* v1, Vector* v2);
Vector* normalizeVector(Vector* vect);
double vectorDistance(Vector* v1, Vector* v2);
double vectorAngle(Vector* v1, Vector* v2);
Vector* vectorProjectOnto(Vector* v1, Vector* v2);

/* ---------- Matrix invariants ---------- */
bool isSymmetric(Matrix* matrix);
bool isAntisymmetric(Matrix* matrix);
bool isOrthogonal(Matrix* matrix);
int rank(Matrix* matrix);
int nullity(Matrix* matrix);
double trace(Matrix* matrix);
double frobeniusNorm(Matrix* matrix);
double determinant(Matrix* matrix);

/* ---------- Matrix computations ---------- */
Matrix* solveLinEq(Matrix* matrix, Matrix* b);
Matrix* invertMatrix(Matrix* matrix);
double* eigenvalues2x2(Matrix* matrix);
double* eigenvalues3x3(Matrix* matrix);
Matrix** eigenvectors2x2(Matrix* matrix);
Matrix** eigenvectors3x3(Matrix* matrix);

#endif
