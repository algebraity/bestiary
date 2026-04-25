#include<stdlib.h>
#include<stdio.h>
#include<math.h>
#include "hebi.h"
#include "sokko.h"

/* ---------- Helper methods ---------- */

// Return true if the Matrix is square, false otherwise
bool isSquare(Matrix* matrix) {
    return (matrix->numRows == matrix->numCols);
}

/* ---------- Free methods ---------- */

// Free an LU struct
void freeLU(LU* lu) {
    if (!lu) return;
    free(lu->data);
    free(lu->perm);
    free(lu);
}

// Reset the cache for a Matrix struct
void resetMatrixCache(Matrix* matrix) {
    if (matrix->cachedLU) {
	freeLU(matrix->cachedLU);
	matrix->cachedLU = NULL;
    }
}

// Free the memory controlled by the Matrix object
void freeMatrix(Matrix* matrix) {
    if (!matrix) return;
    free(matrix->data);
    freeLU(matrix->cachedLU);
    free(matrix);
}

/* ---------- Accessors ---------- */

// Get the values at position (i, j) of the matrix, 0-indexed
MatrixElement getEntry(const Matrix* matrix, int i, int j) {
    return matrix->data[i * matrix->numCols + j];
}

// Set the value at position (i, j) of the matrix, 0-indexed
void setEntry(Matrix* matrix, int i, int j, MatrixElement x) {
    matrix->data[i * matrix->numCols + j] = x;
    resetMatrixCache(matrix);
}

/* ---------- MatrixElement operations ---------- */

// Wrap a double in a MatrixElement
MatrixElement elemFromReal(double x) {
    MatrixElement e;
    e.value.real = x;
    e.isComplex = false;
    return e;
}

// Wrap a ComplexNumber in a MatrixElement; collapses to real when imag == 0
MatrixElement elemFromComplex(ComplexNumber c) {
    MatrixElement e;
    if (c.imag == 0.0) {
        e.value.real = c.real;
        e.isComplex = false;
    } else {
        e.value.complex = c;
        e.isComplex = true;
    }
    return e;
}

// Promote a MatrixElement to a ComplexNumber
ComplexNumber elemToComplex(MatrixElement a) {
    ComplexNumber c;
    if (a.isComplex) {
        c = a.value.complex;
    } else {
        c.real = a.value.real;
        c.imag = 0.0;
    }
    return c;
}

// Add two MatrixElements
MatrixElement elemAdd(MatrixElement a, MatrixElement b) {
    if (!a.isComplex && !b.isComplex) {
        return elemFromReal(a.value.real + b.value.real);
    }
    return elemFromComplex(complexAdd(elemToComplex(a), elemToComplex(b)));
}

// Subtract two MatrixElements
MatrixElement elemSub(MatrixElement a, MatrixElement b) {
    if (!a.isComplex && !b.isComplex) {
        return elemFromReal(a.value.real - b.value.real);
    }
    return elemFromComplex(complexSub(elemToComplex(a), elemToComplex(b)));
}

// Multiply two MatrixElements
MatrixElement elemMul(MatrixElement a, MatrixElement b) {
    if (!a.isComplex && !b.isComplex) {
        return elemFromReal(a.value.real * b.value.real);
    }
    return elemFromComplex(complexMul(elemToComplex(a), elemToComplex(b)));
}

// Divide two MatrixElements
MatrixElement elemDiv(MatrixElement a, MatrixElement b) {
    if (!a.isComplex && !b.isComplex) {
        return elemFromReal(a.value.real / b.value.real);
    }
    return elemFromComplex(complexDiv(elemToComplex(a), elemToComplex(b)));
}

// Negate a MatrixElement
MatrixElement elemNeg(MatrixElement a) {
    if (!a.isComplex) return elemFromReal(-a.value.real);
    return elemFromComplex(complexNeg(a.value.complex));
}

// Return the complex conjugate (or the value itself for real)
MatrixElement elemConj(MatrixElement a) {
    if (!a.isComplex) return a;
    return elemFromComplex(complexConj(a.value.complex));
}

// Return |a|, always a real number
double elemAbs(MatrixElement a) {
    if (!a.isComplex) return fabs(a.value.real);
    return complexAbs(a.value.complex);
}

// Tell if a MatrixElement is within tol of zero
bool elemIsZero(MatrixElement a, double tol) {
    return elemAbs(a) <= tol;
}

// Tell if a MatrixElement carries NaN
bool elemIsNan(MatrixElement a) {
    if (!a.isComplex) return isnan(a.value.real);
    return isnan(a.value.complex.real) || isnan(a.value.complex.imag);
}

// Tell if two MatrixElements are equal up to some tolerance
bool elemEq(MatrixElement a, MatrixElement b, double tol) {
    return elemAbs(elemSub(a, b)) <= tol;
}

// Print a MatrixElement; local helper used by printMatrix
static void printElem(MatrixElement e) {
    if (!e.isComplex) {
        printf("%g", e.value.real);
        return;
    }
    double re = e.value.complex.real;
    double im = e.value.complex.imag;
    if (im >= 0) printf("%g+%gi", re, im);
    else printf("%g-%gi", re, -im);
}

/* ---------- LU methods ----------- */

// Compute the determinant of an LU
MatrixElement luDet(LU* lu) {
    MatrixElement det = elemFromReal((double) lu->sign);
    for (int i = 0; i < lu->n; i++) det = elemMul(det, lu->data[i*lu->n + i]);
    return det;
}

// Decompose a Matrix into the product of L and U, where
// L is lower triangular and U is upper triangular
LU* luDecompose(Matrix* matrix) {
    if (!isSquare(matrix)) return NULL;
    int n = matrix->numRows;

    LU* lu = malloc(sizeof(LU));
    if (!lu) return NULL;
    lu->n = n;
    lu->sign = 1;
    lu->data = malloc(n * n * sizeof(MatrixElement));
    lu->perm = malloc(n * sizeof(int));
    if (!lu->data || !lu->perm) { freeLU(lu); return NULL; }
    for (int i = 0; i < n * n; i++) lu->data[i] = matrix->data[i];

    // Initialize permutation: row i is originally at position i
    for (int i = 0; i < n; i++) lu->perm[i] = i;

    MatrixElement* a = lu->data;  // shorthand for clarity

    for (int k = 0; k < n; k++) {
        // Partial pivoting: find the row with the largest |a[i,k]| for i >= k
        int pivot = k;
        double maxVal = elemAbs(a[k * n + k]);
        for (int i = k + 1; i < n; i++) {
            double v = elemAbs(a[i * n + k]);
            if (v > maxVal) { maxVal = v; pivot = i; }
        }

        // Singular (or close enough): no valid LU exists
        if (maxVal < 1e-12) { freeLU(lu); return NULL; }

        // Swap row k with pivot row, if needed
        if (pivot != k) {
            for (int j = 0; j < n; j++) {
                MatrixElement tmp = a[k * n + j];
                a[k * n + j] = a[pivot * n + j];
                a[pivot * n + j] = tmp;
            }
            // Track the swap in the permutation array
            int tmpIdx = lu->perm[k];
            lu->perm[k] = lu->perm[pivot];
            lu->perm[pivot] = tmpIdx;
            lu->sign = -lu->sign;
        }

        // Eliminate below the pivot, storing the multipliers in L's position
        for (int i = k + 1; i < n; i++) {
            MatrixElement factor = elemDiv(a[i * n + k], a[k * n + k]);
            a[i * n + k] = factor;
            for (int j = k + 1; j < n; j++) {
                a[i * n + j] = elemSub(a[i * n + j], elemMul(factor, a[k * n + j]));
            }
        }
    }

    return lu;
}

// Cache the LU for future use
void cacheLU(Matrix* matrix) {
    resetMatrixCache(matrix);
    matrix->cachedLU = luDecompose(matrix);
}

// Solve the system Ax = b with LU
Matrix* luSolve(LU* lu, Matrix* b) {
    if (!lu || !b) return NULL;
    if (b->numRows != lu->n) return NULL;

    int n = lu->n;
    Matrix* x = constructMatrix(n, 1);
    if (!x) return NULL;

    // Apply the permutation to b, storing result in x
    for (int i = 0; i < n; i++) {
	x->data[i] = b->data[lu->perm[i]];
    }

    // Use forward-substitution for Ly = Pb
    for (int i = 0; i < n; i++) {
        MatrixElement sum = x->data[i];
        for (int j = 0; j < i; j++) {
            sum = elemSub(sum, elemMul(lu->data[i*n + j], x->data[j]));
        }
        x->data[i] = sum;
    }

    // Use backward-substitution for Ux = y
    for (int i = n-1; i >= 0; i--) {
	MatrixElement sum = x->data[i];
	for (int j = i+1; j < n; j++) {
	    sum = elemSub(sum, elemMul(lu->data[i*n + j], x->data[j]));
	}
	x->data[i] = elemDiv(sum, lu->data[i*n+i]);
    }

    return x;
}

// Compute the inverse of a Matrix with LU
Matrix* luInverse(LU* lu) {
    if (!lu) return NULL;
    int n = lu->n;

    // Get the column vectors of the identity matrix
    Matrix** cols = malloc(n * sizeof(Matrix*));
    if (!cols) return NULL;
    for (int i = 0; i < n; i++) {
	    cols[i] = constructMatrix(n, 1);
	    if (!cols[i]) {
	        for (int j = 0; j < i; j ++) freeMatrix(cols[j]);
	        free(cols);
	        return NULL;
	    }
	    setEntry(cols[i], i, 0, elemFromReal(1.0));
    }

    // Construct a matrix of the inverse column vectors
    Matrix** soln = malloc(n * sizeof(Matrix*));
    if (!soln) {
	for (int i = 0; i < n; i++) freeMatrix(cols[i]);
	free(cols);
	return NULL;
    }
    for (int i = 0; i < lu->n; i++) {
	    soln[i] = luSolve(lu, cols[i]);
	    if (!soln[i]) {
	        for (int j = 0; j < i; j++) freeMatrix(soln[j]);
	        for (int j = 0; j < n; j++) freeMatrix(cols[j]);
	        free(soln);
	        free(cols);
	        return NULL;
	    }
    }

    // Construct the inverse matrix
    Matrix* inverted = constructMatrix(n, n);
    if (!inverted) {
	for (int i = 0; i < n; i++) {
	    freeMatrix(cols[i]);
	    freeMatrix(soln[i]);
    	}
        free(cols);
        free(soln);
	return NULL;
    }
    for (int i = 0; i < n; i++) {
	for (int j = 0; j < n; j++) {
	    setEntry(inverted, i, j, getEntry(soln[j], i, 0));
	}
    }

    // Free memory
    for (int i = 0; i < n; i++) {
	freeMatrix(cols[i]);
	freeMatrix(soln[i]);
    }
    free(cols);
    free(soln);

    return inverted;
}

/* ----------- Main methods ---------- */

// Construct a Matrix object
Matrix* constructMatrix(int numRows, int numCols) {
    if (numRows < 1 || numCols < 1) return NULL;

    Matrix* matrix = calloc(1, sizeof(Matrix));
    if (!matrix) return NULL;
    matrix->numRows = numRows;
    matrix->numCols = numCols;

    matrix->data = malloc(matrix->numRows * matrix->numCols * sizeof(MatrixElement));
    if (!matrix->data) {
	free(matrix);
	return NULL;
    }
    MatrixElement zero = elemFromReal(0.0);
    for (int i = 0; i < numRows * numCols; i++) matrix->data[i] = zero;
    matrix->cachedLU = NULL;

    return matrix;
}

// Construct a matrix from a 2D data matrix
Matrix* constructMatrixFromMatrix(int numRows, int numCols, MatrixElement** data, int lenData, int colLenData) {
    if (lenData != numRows || colLenData != numCols) return NULL;

    Matrix* matrix = constructMatrix(numRows, numCols);
    if (!matrix) return NULL;
    for (int i = 0; i < numRows; i++) {
	for (int j = 0; j < numCols; j++) {
	    setEntry(matrix, i, j, data[i][j]);
	}
    }

    return matrix;
}

// Construct a matrix from a flattened data matrix
Matrix* constructMatrixFromArray(int numRows, int numCols, MatrixElement* data, int lenData) {
    if (lenData != numRows * numCols) return NULL;

    Matrix* matrix = constructMatrix(numRows, numCols);
    if (!matrix) return NULL;
    for (int i = 0; i < numRows; i++) {
	for (int j = 0; j < numCols; j++) {
	    setEntry(matrix, i, j, data[i*numCols + j]);
	}
    }

    return matrix;
}

// Print the Matrix
void printMatrix(Matrix* matrix) {
    for (int i = 0; i < matrix->numRows; i++) {
	printf("| ");
	for (int j = 0; j < matrix->numCols-1; j++) {
	    printElem(matrix->data[i*matrix->numCols + j]);
	    printf(", ");
	}
	printElem(matrix->data[i*matrix->numCols + matrix->numCols-1]);
	printf(" |\n");
    }
}

// Return a deep copy of a Matrix
Matrix* copyMatrix(Matrix* matrix) {
    if (!matrix) return NULL;
    Matrix* newMatrix = constructMatrix(matrix->numRows, matrix->numCols);
    if (!newMatrix) return NULL;

    for (int i = 0; i < matrix->numRows; i++) {
	for (int j = 0; j < matrix->numCols; j++) {
	    newMatrix->data[i*matrix->numCols + j] = matrix->data[i*matrix->numCols + j];
	}
    }

    newMatrix->cachedLU = NULL;
    return newMatrix;
}

// Tell if two matrices are equal up to some tolerance
bool matrixComp(Matrix* A, Matrix* B, double tol) {
    if (!A || !B) return false;
    if (A->numRows != B->numRows || A->numCols != B->numCols) return false;

    for (int i = 0; i < A->numRows; i++) {
	for (int j = 0; j < A->numCols; j++) {
	    if (!elemEq(getEntry(A, i, j), getEntry(B, i, j), tol)) return false;
	}
    }

    return true;
}

// Get the n x n identity matrix
Matrix* idMatrix(int n) {
    Matrix* id = constructMatrix(n, n);
    if (!id) return NULL;

    MatrixElement one = elemFromReal(1.0);
    for (int i = 0; i < n; i++) setEntry(id, i, i, one);

    return id;
}

/* ---------- Basic operations ---------- */

// Compute the dot product of two arrays
MatrixElement simpleDotProduct(MatrixElement* v, MatrixElement* w, int vlen, int wlen) {
    if (vlen != wlen) return elemFromReal(NAN);

    MatrixElement dp = elemFromReal(0.0);
    for (int i = 0; i < vlen; i++) dp = elemAdd(dp, elemMul(v[i], w[i]));
    return dp;
}

// Compute the dot product of two n x 1 matrices
MatrixElement dotProduct(Matrix* v, Matrix* w) {
    if (v->numCols != 1 || w->numCols != 1) return elemFromReal(NAN);
    if (v->numRows != w->numRows) return elemFromReal(NAN);

    MatrixElement dp = elemFromReal(0.0);
    for (int i = 0; i < v->numRows; i++) {
        dp = elemAdd(dp, elemMul(getEntry(v, i, 0), getEntry(w, i, 0)));
    }
    return dp;
}

// Compute Av for a matrix A and a vector v
Matrix* applyMatrix(Matrix* A, Matrix* v) {
    if (!A || !v) return NULL;
    if (A->numCols != v->numRows) return NULL;
    if (v->numCols != 1) return NULL;

    Matrix* toReturn = constructMatrix(A->numRows, 1);
    if (!toReturn) return NULL;

    for (int i = 0; i < A->numRows; i++) {
        MatrixElement entry = elemFromReal(0.0);
        for (int j = 0; j < A->numCols; j++) {
            entry = elemAdd(entry, elemMul(getEntry(A, i, j), getEntry(v, j, 0)));
        }
        setEntry(toReturn, i, 0, entry);
    }

    return toReturn;
}

// Multiply a Matrix by a constant c
Matrix* multByConstant(Matrix* matrix, MatrixElement c) {
    if (!matrix) return NULL;
    Matrix* toReturn = copyMatrix(matrix);
    if (!toReturn) return NULL;
    for (int i = 0; i < matrix->numRows * matrix->numCols; i++) {
	toReturn->data[i] = elemMul(toReturn->data[i], c);
    }

    return toReturn;
}

// Return matrix^n
Matrix* matrixPow(Matrix* matrix, int n) {
    if (!matrix) return NULL;
    if (!isSquare(matrix)) return NULL;

    if (n == 0) return idMatrix(matrix->numRows);

    Matrix* base;
    if (n > 0) {
        base = copyMatrix(matrix);
    } else {
        base = invertMatrix(matrix);
        n = -n;
    }
    if (!base) return NULL;

    Matrix* result = idMatrix(matrix->numRows);
    if (!result) { freeMatrix(base); return NULL; }

    while (n > 0) {
        Matrix* tmp = multiplyMatrices(result, base);
        freeMatrix(result);
        if (!tmp) { freeMatrix(base); return NULL; }
        result = tmp;
        n--;
    }

    freeMatrix(base);
    return result;
}

// Add two Matrices
Matrix* addMatrices(Matrix* A, Matrix* B) {
    if (!A || !B) return NULL;
    if (!(A->numRows == B->numRows && A->numCols == B->numCols)) return NULL;
    Matrix* C = constructMatrix(A->numRows, A->numCols);
    if (!C) return NULL;

    for (int i = 0; i < A->numRows * A->numCols; i++) {
	C->data[i] = elemAdd(A->data[i], B->data[i]);
    }

    return C;
}

// Subtract two Matrices
Matrix* subtractMatrices(Matrix* A, Matrix* B) {
    if (!A || !B) return NULL;
    if (!(A->numRows == B->numRows && A->numCols == B->numCols)) return NULL;
    Matrix* C = constructMatrix(A->numRows, A->numCols);
    if (!C) return NULL;

    for (int i = 0; i < A->numRows * A->numCols; i++) {
	C->data[i] = elemSub(A->data[i], B->data[i]);
    }

    return C;
}

// Get the product of two matrices if they can be multiplied
Matrix* multiplyMatrices(Matrix* A, Matrix* B) {
    if (!A || !B) return NULL;
    if (A->numCols != B->numRows) return NULL;
    Matrix* prod = constructMatrix(A->numRows, B->numCols);
    if (!prod) return NULL;

    for (int i = 0; i < A->numRows; i++) {
	    for (int j = 0; j < B->numCols; j++) {
	        MatrixElement s = elemFromReal(0.0);
	        for (int k = 0; k < A->numCols; k++) {
		        s = elemAdd(s, elemMul(getEntry(A, i, k), getEntry(B, k, j)));
	        }
	        setEntry(prod, i, j, s);
	    }
    }

    return prod;
}

// Get the tensor product of two matrices
Matrix* tensorMatrices(Matrix* A, Matrix* B) {
    if (!A || !B) return NULL;
    int m = A->numRows;
    int n = A->numCols;
    int p = B->numRows;
    int q = B->numCols;

    Matrix* C = constructMatrix(m*p, n*q);
    if (!C) return NULL;

    for (int i = 0; i < m; i++) {
	for (int j = 0; j < n; j++) {
	    MatrixElement aij = getEntry(A, i, j);
	    for (int r = 0; r < p; r++) {
		for (int s = 0; s < q; s++) {
		    MatrixElement brs = getEntry(B, r, s);
		    int I = i*p+r;
		    int J = j*q+s;
		    setEntry(C, I, J, elemMul(aij, brs));
		}
	    }
	}
    }

    return C;
}

// Get the transpose of a Matrix
Matrix* transpose(Matrix* matrix) {
    Matrix* trans = constructMatrix(matrix->numCols, matrix->numRows);
    if (!trans) return NULL;

    for (int i = 0; i < matrix->numRows; i++) {
	    for (int j = 0; j < matrix->numCols; j++) {
	        setEntry(trans, j, i, getEntry(matrix, i, j));
	    }
    }

    return trans;
}

// Get the conjugate transpose of a Matrix
Matrix* adjoint(Matrix* matrix) {
    if (!matrix) return NULL;

    Matrix* adj = constructMatrix(matrix->numCols, matrix->numRows);
    if (!adj) return NULL;

    for (int i = 0; i < matrix->numRows; i++) {
        for (int j = 0; j < matrix->numCols; j++) {
            setEntry(adj, j, i, elemConj(getEntry(matrix, i, j)));
        }
    }

    return adj;
}

/* ---------- Vector methods ---------- */

// Free the memory associated with a Vector
void freeVector(Vector* vector) {
	if (!vector) return;
	freeMatrix(vector);
}

// Construct a Vector of positive dim with no data
Vector* constructVector(int dim) {
    if (dim < 1) return NULL;

    Vector* vec = constructMatrix(dim, 1);
    if (!vec) return NULL;
    return vec;
}

// Construct a Vector of positive dim from an array
Vector* constructVectorFromArray(int dim, MatrixElement *data, int dataLen) {
    if (dim < 1) return NULL;
    if (!data) return NULL;
    if (dim != dataLen) return NULL;

    Vector* vec = constructMatrixFromArray(dim, 1, data, dataLen);
    if (!vec) return NULL;
    return vec;
}

// Construct a Vector of dim 2 given two inputs
Vector* constructVector2(MatrixElement x, MatrixElement y) {
    if (elemIsNan(x) || elemIsNan(y)) return NULL;

    Vector* vec = constructMatrix(2, 1);
    if (!vec) return NULL;
    setEntry(vec, 0, 0, x);
    setEntry(vec, 1, 0, y);

    return vec;
}

// Construct a Vector of dim 3 given three inputs
Vector* constructVector3(MatrixElement x, MatrixElement y, MatrixElement z) {
    if (elemIsNan(x) || elemIsNan(y) || elemIsNan(z)) return NULL;

    Vector* vec = constructMatrix(3, 1);
    if (!vec) return NULL;
    setEntry(vec, 0, 0, x);
    setEntry(vec, 1, 0, y);
    setEntry(vec, 2, 0, z);

    return vec;
}

// Adds two Vectors of the same dimension
Vector* addVectors(Vector* v1, Vector* v2) {
    if (!v1 || !v2) return NULL;
    if (v1->numRows != v2->numRows || v1->numCols != v2->numCols || v1->numCols != 1) return NULL;

    int dim = v1->numRows;
    Vector* sum = constructVector(dim);
    if (!sum) return NULL;

    for (int i = 0; i < dim; i++) {
        setEntry(sum, i, 0, elemAdd(getEntry(v1, i, 0), getEntry(v2, i, 0)));
    }
    return sum;
}

// Returns the dot product of two Vectors
MatrixElement vectorDotProduct(Vector* v1, Vector* v2) {
    if (!v1 || !v2) return elemFromReal(NAN);
    if (v1->numRows != v2->numRows || v1->numCols != v2->numCols || v1->numCols != 1) return elemFromReal(NAN);

    MatrixElement dot = elemFromReal(0.0);
    int dim = v1->numRows;
    for (int i = 0; i < dim; i++) {
        dot = elemAdd(dot, elemMul(getEntry(v1, i, 0), getEntry(v2, i, 0)));
    }

    return dot;
}

// Returns the Hermitian dot product <v1, v2> = sum conj(v1_i) * v2_i
MatrixElement hermitianDotProduct(Vector* v1, Vector* v2) {
    if (!v1 || !v2) return elemFromReal(NAN);
    if (v1->numRows != v2->numRows || v1->numCols != v2->numCols || v1->numCols != 1) return elemFromReal(NAN);

    MatrixElement dot = elemFromReal(0.0);
    int dim = v1->numRows;
    for (int i = 0; i < dim; i++) {
        MatrixElement lhs = elemConj(getEntry(v1, i, 0));
        MatrixElement rhs = getEntry(v2, i, 0);
        dot = elemAdd(dot, elemMul(lhs, rhs));
    }

    return dot;
}

// Returns the cross product of two 3D Vectors
Vector* crossProduct(Vector* v1, Vector* v2) {
    if (!v1 || !v2) return NULL;
    if (v1->numRows != v2->numRows || v1->numCols != v2->numCols || v1->numCols != 1) return NULL;
    if (v1->numRows != 3) return NULL;

    Vector* crossProd = constructVector(3);
    if (!crossProd) return NULL;

    MatrixElement v10 = getEntry(v1, 0, 0), v11 = getEntry(v1, 1, 0), v12 = getEntry(v1, 2, 0);
    MatrixElement v20 = getEntry(v2, 0, 0), v21 = getEntry(v2, 1, 0), v22 = getEntry(v2, 2, 0);

    setEntry(crossProd, 0, 0, elemSub(elemMul(v11, v22), elemMul(v12, v21)));
    setEntry(crossProd, 1, 0, elemSub(elemMul(v12, v20), elemMul(v10, v22)));
    setEntry(crossProd, 2, 0, elemSub(elemMul(v10, v21), elemMul(v11, v20)));

    return crossProd;
}

// Returns the L2 norm of a Vector (sqrt of sum of |v_i|^2, always real)
double l2Norm(Vector* vect) {
    if (!vect) return NAN;

    int dim = vect->numRows;
    double norm = 0;
    for (int i = 0; i < dim; i++) {
        double m = elemAbs(getEntry(vect, i, 0));
        norm += m * m;
    }

    return sqrt(norm);
}

// Return the opposite of a vector
Vector* negativeVector(Vector* vect) {
    if (!vect) return NULL;

    int dim = vect->numRows;
    Vector* neg = constructVector(dim);
    if (!neg) return NULL;
    for (int i = 0; i < dim; i++) {
        setEntry(neg, i, 0, elemNeg(getEntry(vect, i, 0)));
    }

    return neg;
}

// Scale a Vector by a constant k
Vector* scaleVector(Vector* vect, MatrixElement k) {
    if (!vect || elemIsNan(k)) return NULL;

    int dim = vect->numRows;
    Vector* scaled = constructVector(dim);
    if (!scaled) return NULL;
    for (int i = 0; i < dim; i++) {
        setEntry(scaled, i, 0, elemMul(getEntry(vect, i, 0), k));
    }

    return scaled;
}

// Compute v1 - v2
Vector* subtractVectors(Vector* v1, Vector* v2) {
    if (!v1 || !v2) return NULL;
	if (v1->numRows != v2->numRows || v1->numCols != v2->numCols || v1->numCols != 1) return NULL;

	int dim = v1->numRows;
	Vector* diff = constructVector(dim);
	if (!diff) return NULL;

	for (int i = 0; i < dim; i++) {
		setEntry(diff, i, 0, elemSub(getEntry(v1, i, 0), getEntry(v2, i, 0)));
	}
	return diff;
}

// Normalize a Vector v
Vector* normalizeVector(Vector* vect) {
	if (!vect) return NULL;

	double norm = l2Norm(vect);
	if (norm == 0) return NULL;

	return scaleVector(vect, elemFromReal(1.0 / norm));
}

// Compute the distance between two Vectors
double vectorDistance(Vector* v1, Vector* v2) {
	if (!v1 || !v2) return NAN;
	if (v1->numRows != v2->numRows || v1->numCols != v2->numCols || v1->numCols != 1) return NAN;

	Vector* diff = subtractVectors(v1, v2);
	if (!diff) return NAN;

	double dist = l2Norm(diff);
	freeMatrix(diff);
	return dist;
}

// Compute the angle between two Vectors in radians.
// Uses Re(<v1, v2>) / (||v1|| ||v2||); returns NAN if either vector has
// a strictly complex dot product component that prevents a real angle.
double vectorAngle(Vector* v1, Vector* v2) {
	if (!v1 || !v2) return NAN;
	if (v1->numRows != v2->numRows || v1->numCols != v2->numCols || v1->numCols != 1) return NAN;

	MatrixElement dot = vectorDotProduct(v1, v2);
	if (elemIsNan(dot)) return NAN;
	double normV1 = l2Norm(v1);
	double normV2 = l2Norm(v2);
	if (normV1 == 0 || normV2 == 0) return NAN;

	double reDot = dot.isComplex ? dot.value.complex.real : dot.value.real;
	double cosTheta = reDot / (normV1 * normV2);
	if (cosTheta > 1) cosTheta = 1;
	if (cosTheta < -1) cosTheta = -1;

	return acos(cosTheta);
}

// Compute the projection of v1 onto v2
Vector* vectorProjectOnto(Vector* v1, Vector* v2) {
	if (!v1 || !v2) return NULL;
	if (v1->numRows != v2->numRows || v1->numCols != v2->numCols || v1->numCols != 1) return NULL;

	MatrixElement dot = vectorDotProduct(v1, v2);
	double normV2 = l2Norm(v2);
	if (normV2 == 0) return NULL;

	MatrixElement scale = elemDiv(dot, elemFromReal(normV2 * normV2));
	return scaleVector(v2, scale);
}

/* ---------- Matrix invariants ---------- */

// Returns true if the Matrix is symmetric (A = A^T), else false
bool isSymmetric(Matrix* matrix) {
    if (!matrix) return false;
    if (!isSquare(matrix)) return false;

    Matrix* trans = transpose(matrix);
    if (!trans) return false;
    bool sym = matrixComp(matrix, trans, 1e-12);

    freeMatrix(trans);
    return sym;
}

// Returns true if the Matrix is antisymmetric (A = -A^T), else false
bool isAntisymmetric(Matrix* matrix) {
    if (!matrix) return false;
    if (!isSquare(matrix)) return false;

    Matrix* trans = transpose(matrix);
    if (!trans) return false;
    Matrix* negTrans = multByConstant(trans, elemFromReal(-1.0));
    if (!negTrans) {
        freeMatrix(trans);
        return false;
    }
    bool antisym = matrixComp(matrix, negTrans, 1e-12);

    freeMatrix(trans);
    freeMatrix(negTrans);
    return antisym;
}

// Returns true if the Matrix is orthogonal (A * A^T = I), else false
bool isOrthogonal(Matrix* matrix) {
    if (!matrix) return false;
    if (!isSquare(matrix)) return false;

    Matrix* trans = transpose(matrix);
    if (!trans) return false;
    Matrix* prod = multiplyMatrices(matrix, trans);
    if (!prod) {
        freeMatrix(trans);
        return false;
    }
    Matrix* id = idMatrix(matrix->numRows);
    if (!id) {
        freeMatrix(trans);
        freeMatrix(prod);
        return false;
    }
    bool orthog = matrixComp(prod, id, 1e-12);

    freeMatrix(trans);
    freeMatrix(prod);
    freeMatrix(id);
    return orthog;
}

// Returns true if the Matrix is unitary (A * A* = I), else false
bool isUnitary(Matrix* matrix) {
    if (!matrix) return false;
    if (!isSquare(matrix)) return false;

    Matrix* adj = adjoint(matrix);
    if (!adj) return false;
    Matrix* prod = multiplyMatrices(matrix, adj);
    if (!prod) {
        freeMatrix(adj);
        return false;
    }
    Matrix* id = idMatrix(matrix->numRows);
    if (!id) {
        freeMatrix(adj);
        freeMatrix(prod);
        return false;
    }

    double tol = 1e-12 * fmax(1.0, frobeniusNorm(matrix));
    bool unitary = matrixComp(prod, id, tol);

    freeMatrix(adj);
    freeMatrix(prod);
    freeMatrix(id);
    return unitary;
}

// Return the rank of a Matrix
int rank(Matrix* matrix) {
    if (!matrix) return -1;

    int m = matrix->numRows;
    int n = matrix->numCols;

    MatrixElement* a = malloc(m * n * sizeof(MatrixElement));
    if (!a) return -1;
    for (int k = 0; k < m * n; k++) a[k] = matrix->data[k];

    int rnk = 0;
    int row = 0;
    for (int col = 0; col < n && row < m; col++) {
        int pivot = -1;
        double maxVal = 1e-12;
        for (int i = row; i < m; i++) {
            double v = elemAbs(a[i * n + col]);
            if (v > maxVal) { maxVal = v; pivot = i; }
        }
        if (pivot == -1) continue;

        if (pivot != row) {
            for (int j = 0; j < n; j++) {
                MatrixElement tmp = a[row * n + j];
                a[row * n + j] = a[pivot * n + j];
                a[pivot * n + j] = tmp;
            }
        }

        for (int i = row + 1; i < m; i++) {
            MatrixElement factor = elemDiv(a[i * n + col], a[row * n + col]);
            for (int j = col; j < n; j++) {
                a[i * n + j] = elemSub(a[i * n + j], elemMul(factor, a[row * n + j]));
            }
        }

        rnk++;
        row++;
    }

    free(a);
    return rnk;
}

// Return the nullity of a Matrix
int nullity(Matrix* matrix) {
    if (!matrix) return -1;
    return matrix->numCols - rank(matrix);
}

// Return the trace of a Matrix
MatrixElement trace(Matrix* matrix) {
    if (!matrix) return elemFromReal(NAN);
    if (!isSquare(matrix)) return elemFromReal(NAN);

    MatrixElement trc = elemFromReal(0.0);
    for (int i = 0; i < matrix->numCols; i++) {
        trc = elemAdd(trc, getEntry(matrix, i, i));
    }

    return trc;
}

// Return the Frobenius norm of a matrix (always real)
double frobeniusNorm(Matrix* matrix) {
    if (!matrix) return NAN;

    double sum = 0.0;
    for (int i = 0; i < matrix->numRows * matrix->numCols; i++) {
        double m = elemAbs(matrix->data[i]);
        sum += m * m;
    }
    return sqrt(sum);
}

// Compute the determinant of a Matrix
MatrixElement determinant(Matrix* matrix) {
    if (!isSquare(matrix)) return elemFromReal(NAN);
    int n = matrix->numRows;

    // Hardcoded fast paths for tiny matrices to avoid LU overhead
    if (n == 1) return matrix->data[0];
    if (n == 2) {
        MatrixElement ad = elemMul(matrix->data[0], matrix->data[3]);
        MatrixElement bc = elemMul(matrix->data[1], matrix->data[2]);
        return elemSub(ad, bc);
    }

    cacheLU(matrix);
    if (!matrix->cachedLU) return elemFromReal(0.0);
    return luDet(matrix->cachedLU);
}

/* ---------- Matrix computations ---------- */

// Solves the equation Ax = b for x
Matrix* solveLinEq(Matrix* matrix, Matrix* b) {
    if (!matrix || !b) return NULL;
    if (!matrix->cachedLU) cacheLU(matrix);
    if (matrix->numRows != matrix->numCols) return NULL;
    if (matrix->numCols != b->numRows) return NULL;
    if (b->numCols != 1) return NULL;

    return luSolve(matrix->cachedLU, b);
}

// Find the inverse of A
Matrix* invertMatrix(Matrix* matrix) {
    if (!isSquare(matrix)) return NULL;
    cacheLU(matrix);
    if (!matrix->cachedLU) return NULL;
    return luInverse(matrix->cachedLU);
}

// Return the reduced row echelon form of a Matrix (Gauss-Jordan with partial pivoting).
Matrix* reduceRows(Matrix* matrix) {
    if (!matrix) return NULL;
    int m = matrix->numRows;
    int n = matrix->numCols;

    Matrix* R = copyMatrix(matrix);
    if (!R) return NULL;

    const double tol = 1e-12;
    int row = 0;
    for (int col = 0; col < n && row < m; col++) {
        // Partial pivot: largest |entry| in column col, rows [row, m)
        int pivot = -1;
        double maxVal = tol;
        for (int i = row; i < m; i++) {
            double v = elemAbs(getEntry(R, i, col));
            if (v > maxVal) { maxVal = v; pivot = i; }
        }
        if (pivot < 0) continue;

        if (pivot != row) {
            for (int j = 0; j < n; j++) {
                MatrixElement tmp = getEntry(R, row, j);
                setEntry(R, row, j, getEntry(R, pivot, j));
                setEntry(R, pivot, j, tmp);
            }
        }

        // Scale pivot row so the pivot is 1
        MatrixElement pv = getEntry(R, row, col);
        for (int j = col; j < n; j++) {
            setEntry(R, row, j, elemDiv(getEntry(R, row, j), pv));
        }
        // Force exact 1 to avoid rounding drift on the pivot
        setEntry(R, row, col, elemFromReal(1.0));

        // Eliminate every other row's entry in this column
        for (int i = 0; i < m; i++) {
            if (i == row) continue;
            MatrixElement factor = getEntry(R, i, col);
            if (elemIsZero(factor, tol)) continue;
            for (int j = col; j < n; j++) {
                MatrixElement v = elemSub(getEntry(R, i, j), elemMul(factor, getEntry(R, row, j)));
                setEntry(R, i, j, v);
            }
            setEntry(R, i, col, elemFromReal(0.0));
        }

        row++;
    }
    return R;
}

// Return the reduced column echelon form of a Matrix
// (column analogue of RREF: same pivots are 1, every other entry in the pivot row is 0).
Matrix* reduceColumns(Matrix* matrix) {
    if (!matrix) return NULL;
    Matrix* T = transpose(matrix);
    if (!T) return NULL;
    Matrix* R = reduceRows(T);
    freeMatrix(T);
    if (!R) return NULL;
    Matrix* result = transpose(R);
    freeMatrix(R);
    return result;
}

// Return a basis of the column space of the matrix as a heap-allocated array of column
// Vectors. Sets *count to the number of basis vectors (the rank). Caller frees each
// Vector via freeVector and the array via free.
Vector** columnSpace(Matrix* matrix, int* count) {
    if (!matrix || !count) return NULL;
    int m = matrix->numRows;
    int n = matrix->numCols;
    *count = 0;

    Matrix* R = reduceRows(matrix);
    if (!R) return NULL;

    const double tol = 1e-12;

    // Identify pivot columns of R: row r's pivot is the leftmost column with a 1 in that row.
    int* pivotCols = malloc(n * sizeof(int));
    if (!pivotCols) { freeMatrix(R); return NULL; }
    int numPivots = 0;
    int row = 0;
    for (int col = 0; col < n && row < m; col++) {
        MatrixElement v = getEntry(R, row, col);
        if (!elemIsZero(v, tol)) {
            pivotCols[numPivots++] = col;
            row++;
        }
    }
    freeMatrix(R);

    if (numPivots == 0) { free(pivotCols); return NULL; }

    Vector** basis = malloc(numPivots * sizeof(Vector*));
    if (!basis) { free(pivotCols); return NULL; }

    for (int k = 0; k < numPivots; k++) {
        basis[k] = constructVector(m);
        if (!basis[k]) {
            for (int j = 0; j < k; j++) freeVector(basis[j]);
            free(basis);
            free(pivotCols);
            return NULL;
        }
        int c = pivotCols[k];
        for (int i = 0; i < m; i++) setEntry(basis[k], i, 0, getEntry(matrix, i, c));
    }
    free(pivotCols);

    *count = numPivots;
    return basis;
}

// Return a basis of the row space of the matrix as column Vectors of length numCols.
// Sets *count to the number of basis vectors (the rank). The vectors are the non-zero
// rows of the RREF, transposed into column form. Caller frees each Vector and the array.
Vector** rowSpace(Matrix* matrix, int* count) {
    if (!matrix || !count) return NULL;
    int m = matrix->numRows;
    int n = matrix->numCols;
    *count = 0;

    Matrix* R = reduceRows(matrix);
    if (!R) return NULL;

    const double tol = 1e-12;
    int numPivots = 0;
    bool* nonzero = calloc(m, sizeof(bool));
    if (!nonzero) { freeMatrix(R); return NULL; }
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            if (!elemIsZero(getEntry(R, i, j), tol)) { nonzero[i] = true; numPivots++; break; }
        }
    }

    if (numPivots == 0) { freeMatrix(R); free(nonzero); return NULL; }

    Vector** basis = malloc(numPivots * sizeof(Vector*));
    if (!basis) { freeMatrix(R); free(nonzero); return NULL; }

    int k = 0;
    for (int i = 0; i < m; i++) {
        if (!nonzero[i]) continue;
        basis[k] = constructVector(n);
        if (!basis[k]) {
            for (int j = 0; j < k; j++) freeVector(basis[j]);
            free(basis);
            freeMatrix(R);
            free(nonzero);
            return NULL;
        }
        for (int j = 0; j < n; j++) setEntry(basis[k], j, 0, getEntry(R, i, j));
        k++;
    }
    free(nonzero);
    freeMatrix(R);

    *count = numPivots;
    return basis;
}

// Get the eigenvalues of a 2 x 2 matrix.
// Returns complex eigenvalues when the discriminant is negative or when
// input entries are complex; pure-real eigenvalues collapse to real MatrixElements.
MatrixElement* eigenvalues2x2(Matrix* matrix) {
    if (!matrix) return NULL;
    if (matrix->numRows != 2 || matrix->numCols != 2) return NULL;

    MatrixElement a = getEntry(matrix, 0, 0);
    MatrixElement b = getEntry(matrix, 0, 1);
    MatrixElement c = getEntry(matrix, 1, 0);
    MatrixElement d = getEntry(matrix, 1, 1);

    MatrixElement trc = elemAdd(a, d);
    MatrixElement det = elemSub(elemMul(a, d), elemMul(b, c));
    MatrixElement four = elemFromReal(4.0);
    MatrixElement discriminant = elemSub(elemMul(trc, trc), elemMul(four, det));

    // Take the principal square root in the complex plane so negative
    // discriminants produce purely imaginary sqrt terms.
    ComplexNumber sqrtDisc = complexSqrt(elemToComplex(discriminant));
    MatrixElement sqrtDiscElem = elemFromComplex(sqrtDisc);

    MatrixElement* eigs = malloc(2 * sizeof(MatrixElement));
    if (!eigs) return NULL;

    MatrixElement two = elemFromReal(2.0);
    eigs[0] = elemDiv(elemAdd(trc, sqrtDiscElem), two);
    eigs[1] = elemDiv(elemSub(trc, sqrtDiscElem), two);

    return eigs;
}

// Get the eigenvalues of a 3x3 matrix using the characteristic polynomial.
// All three roots (including complex conjugate pairs) are returned as MatrixElements.
MatrixElement* eigenvalues3x3(Matrix* matrix) {
    if (!matrix) return NULL;
    if (matrix->numRows != 3 || matrix->numCols != 3) return NULL;

    MatrixElement a = getEntry(matrix, 0, 0);
    MatrixElement b = getEntry(matrix, 0, 1);
    MatrixElement c = getEntry(matrix, 0, 2);
    MatrixElement d = getEntry(matrix, 1, 0);
    MatrixElement e = getEntry(matrix, 1, 1);
    MatrixElement f = getEntry(matrix, 1, 2);
    MatrixElement g = getEntry(matrix, 2, 0);
    MatrixElement h = getEntry(matrix, 2, 1);
    MatrixElement i = getEntry(matrix, 2, 2);

    // Characteristic polynomial: lambda^3 + p*lambda^2 + q*lambda + r = 0
    MatrixElement p = elemNeg(elemAdd(elemAdd(a, e), i));
    // q = ae + ai + ei - bd - cg - fh
    MatrixElement q = elemSub(
        elemSub(
            elemSub(
                elemAdd(elemAdd(elemMul(a, e), elemMul(a, i)), elemMul(e, i)),
                elemMul(b, d)),
            elemMul(c, g)),
        elemMul(f, h));
    // r = -(aei + bfg + cdh - ceg - bdi - afh)
    MatrixElement r = elemNeg(
        elemSub(
            elemSub(
                elemSub(
                    elemAdd(elemAdd(elemMul(elemMul(a, e), i), elemMul(elemMul(b, f), g)),
                            elemMul(elemMul(c, d), h)),
                    elemMul(elemMul(c, e), g)),
                elemMul(elemMul(b, d), i)),
            elemMul(elemMul(a, f), h)));

    // Depress the cubic: substitute lambda = t - p/3 to get t^3 + P*t + Q = 0
    // P = q - p^2/3
    // Q = 2p^3/27 - pq/3 + r
    ComplexNumber Pc, Qc;
    {
        ComplexNumber pc = elemToComplex(p);
        ComplexNumber qc = elemToComplex(q);
        ComplexNumber rc = elemToComplex(r);
        ComplexNumber three = {3.0, 0.0};
        ComplexNumber twentySeven = {27.0, 0.0};
        ComplexNumber two = {2.0, 0.0};
        ComplexNumber p2 = complexMul(pc, pc);
        ComplexNumber p3 = complexMul(p2, pc);
        Pc = complexSub(qc, complexDiv(p2, three));
        ComplexNumber term1 = complexDiv(complexMul(two, p3), twentySeven);
        ComplexNumber term2 = complexDiv(complexMul(pc, qc), three);
        Qc = complexAdd(complexSub(term1, term2), rc);
    }

    MatrixElement* eigs = malloc(3 * sizeof(MatrixElement));
    if (!eigs) return NULL;

    // Cardano in complex arithmetic:
    // u^3 = -Q/2 + sqrt(Q^2/4 + P^3/27), v = -P/(3u) when u != 0.
    // Roots of the depressed cubic are u+v, omega*u + omega^2*v, omega^2*u + omega*v.
    ComplexNumber two = {2.0, 0.0};
    ComplexNumber three = {3.0, 0.0};
    ComplexNumber four = {4.0, 0.0};
    ComplexNumber twentySeven = {27.0, 0.0};

    ComplexNumber Q2over4 = complexDiv(complexMul(Qc, Qc), four);
    ComplexNumber P3over27 = complexDiv(complexMul(complexMul(Pc, Pc), Pc), twentySeven);
    ComplexNumber under = complexAdd(Q2over4, P3over27);
    ComplexNumber sqrtUnder = complexSqrt(under);
    ComplexNumber minusQover2 = complexNeg(complexDiv(Qc, two));
    ComplexNumber u3 = complexAdd(minusQover2, sqrtUnder);

    ComplexNumber u, v;
    if (complexAbs(u3) < 1e-15) {
        // -Q/2 + sqrt(Q^2/4 + P^3/27) = 0 implies the root is cbrt(-Q).
        u = complexCbrt(complexNeg(Qc));
        v.real = 0.0; v.imag = 0.0;
    } else {
        u = complexCbrt(u3);
        if (complexAbs(Pc) < 1e-15) {
            v.real = 0.0; v.imag = 0.0;
        } else {
            v = complexDiv(complexNeg(Pc), complexMul(three, u));
        }
    }

    // omega = e^(2*pi*i/3), omega^2 = e^(-2*pi*i/3)
    ComplexNumber omega = { -0.5,  sqrt(3.0) / 2.0 };
    ComplexNumber omega2 = { -0.5, -sqrt(3.0) / 2.0 };

    ComplexNumber t1 = complexAdd(u, v);
    ComplexNumber t2 = complexAdd(complexMul(omega, u), complexMul(omega2, v));
    ComplexNumber t3 = complexAdd(complexMul(omega2, u), complexMul(omega, v));

    ComplexNumber pOver3 = complexDiv(elemToComplex(p), three);
    ComplexNumber lam1 = complexSub(t1, pOver3);
    ComplexNumber lam2 = complexSub(t2, pOver3);
    ComplexNumber lam3 = complexSub(t3, pOver3);

    // Cardano introduces tiny imaginary residuals when all roots of a real
    // polynomial are real; collapse them so real inputs yield real outputs.
    bool inputsReal = !a.isComplex && !b.isComplex && !c.isComplex &&
                      !d.isComplex && !e.isComplex && !f.isComplex &&
                      !g.isComplex && !h.isComplex && !i.isComplex;
    if (inputsReal) {
        double scale = fmax(fmax(complexAbs(lam1), complexAbs(lam2)), complexAbs(lam3));
        double tol = 1e-9 * scale + 1e-12;
        if (fabs(lam1.imag) < tol) lam1.imag = 0.0;
        if (fabs(lam2.imag) < tol) lam2.imag = 0.0;
        if (fabs(lam3.imag) < tol) lam3.imag = 0.0;
    }

    eigs[0] = elemFromComplex(lam1);
    eigs[1] = elemFromComplex(lam2);
    eigs[2] = elemFromComplex(lam3);

    return eigs;
}

// Return the eigenvectors of a 2x2 Matrix
Matrix** eigenvectors2x2(Matrix* matrix) {
    if (!matrix) return NULL;
    if (!isSquare(matrix)) return NULL;
    if (!(matrix->numRows == 2)) return NULL;

    // Get eigenvales and allocate memory
    MatrixElement* eigs = eigenvalues2x2(matrix);
    if (!eigs) return NULL;
    Matrix** evects = malloc(2 * sizeof(Matrix*));
    if (!evects) {
        free(eigs);
        return NULL;
    }
    for (int i = 0; i < 2; i++) {
        evects[i] = constructMatrix(2, 1);
        if (!evects[i]) {
            for (int j = 0; j < i; j++) freeMatrix(evects[j]);
            free(evects);
            free(eigs);
            return NULL;
        }
    }

    // Get the eigenvectors
    for (int i = 0; i < 2; i++) {
        MatrixElement lambda = eigs[i];

        if (elemIsNan(lambda)) {
            freeMatrix(evects[i]);
            evects[i] = NULL;
            continue;
        }

        // Rows of A - lambda*I
        MatrixElement a = elemSub(getEntry(matrix, 0, 0), lambda);
        MatrixElement b = getEntry(matrix, 0, 1);
        MatrixElement c = getEntry(matrix, 1, 0);
        MatrixElement d = elemSub(getEntry(matrix, 1, 1), lambda);

        if (!elemIsZero(b, 1e-12) || !elemIsZero(a, 1e-12)) {
            setEntry(evects[i], 0, 0, b);
            setEntry(evects[i], 1, 0, elemNeg(a));
        } else if (!elemIsZero(c, 1e-12) || !elemIsZero(d, 1e-12)) {
            setEntry(evects[i], 0, 0, d);
            setEntry(evects[i], 1, 0, elemNeg(c));
        } else {
            setEntry(evects[i], 0, 0, elemFromReal(1.0));
            setEntry(evects[i], 1, 0, elemFromReal(0.0));
        }
    }

    free(eigs);
    return evects;
}

// Return the eigenvectors of a 3x3 Matrix
Matrix** eigenvectors3x3(Matrix* matrix) {
    if (!matrix) return NULL;
    if (!isSquare(matrix)) return NULL;
    if (!(matrix->numRows == 3)) return NULL;

    // Get eigenvales and allocate memory
    MatrixElement* eigs = eigenvalues3x3(matrix);
    if (!eigs) return NULL;
    Matrix** evects = malloc(3 * sizeof(Matrix*));
    if (!evects) {
        free(eigs);
        return NULL;
    }
    for (int i = 0; i < 3; i++) {
        evects[i] = constructMatrix(3, 1);
        if (!evects[i]) {
            for (int j = 0; j < i; j++) freeMatrix(evects[j]);
            free(evects);
            free(eigs);
            return NULL;
        }
    }

    // Get the eigenvectors
    for (int i = 0; i < 3; i++) {
        MatrixElement lambda = eigs[i];

        if (elemIsNan(lambda)) {
            freeMatrix(evects[i]);
            evects[i] = NULL;
            continue;
        }

        // Rows of (A - eig*I)
        MatrixElement r00 = elemSub(getEntry(matrix, 0, 0), lambda);
        MatrixElement r01 = getEntry(matrix, 0, 1);
        MatrixElement r02 = getEntry(matrix, 0, 2);
        MatrixElement r10 = getEntry(matrix, 1, 0);
        MatrixElement r11 = elemSub(getEntry(matrix, 1, 1), lambda);
        MatrixElement r12 = getEntry(matrix, 1, 2);
        MatrixElement r20 = getEntry(matrix, 2, 0);
        MatrixElement r21 = getEntry(matrix, 2, 1);
        MatrixElement r22 = elemSub(getEntry(matrix, 2, 2), lambda);

        // Find null space vector via cross products of row pairs
        MatrixElement v0, v1, v2;
        double normSq;

        v0 = elemSub(elemMul(r01, r12), elemMul(r02, r11));
        v1 = elemSub(elemMul(r02, r10), elemMul(r00, r12));
        v2 = elemSub(elemMul(r00, r11), elemMul(r01, r10));
        normSq = elemAbs(v0)*elemAbs(v0) + elemAbs(v1)*elemAbs(v1) + elemAbs(v2)*elemAbs(v2);

        if (normSq <= 1e-24) {
            v0 = elemSub(elemMul(r01, r22), elemMul(r02, r21));
            v1 = elemSub(elemMul(r02, r20), elemMul(r00, r22));
            v2 = elemSub(elemMul(r00, r21), elemMul(r01, r20));
            normSq = elemAbs(v0)*elemAbs(v0) + elemAbs(v1)*elemAbs(v1) + elemAbs(v2)*elemAbs(v2);
        }

        if (normSq <= 1e-24) {
            v0 = elemSub(elemMul(r11, r22), elemMul(r12, r21));
            v1 = elemSub(elemMul(r12, r20), elemMul(r10, r22));
            v2 = elemSub(elemMul(r10, r21), elemMul(r11, r20));
            normSq = elemAbs(v0)*elemAbs(v0) + elemAbs(v1)*elemAbs(v1) + elemAbs(v2)*elemAbs(v2);
        }

        if (normSq <= 1e-24) {
            v0 = elemFromReal(1.0);
            v1 = elemFromReal(0.0);
            v2 = elemFromReal(0.0);
        }

        setEntry(evects[i], 0, 0, v0);
        setEntry(evects[i], 1, 0, v1);
        setEntry(evects[i], 2, 0, v2);
    }

    free(eigs);
    return evects;
}

/* ---------- General n x n eigenvalues / eigenvectors ----------
 * Algorithm: reduce to upper Hessenberg form via Givens similarity
 * transforms, then run the implicitly-shifted QR iteration (Wilkinson
 * shift) until subdiagonals decouple. Eigenvectors are recovered by
 * computing a null-space basis vector of (A - lambda*I) for each
 * computed eigenvalue, using complex Gaussian elimination with partial
 * pivoting.
 *
 * The QR iteration in complex arithmetic does not handle every
 * pathological matrix (e.g. defective non-diagonalizable cases produce
 * approximate, possibly degenerate eigenvectors), but is correct for
 * generic matrices.
 */

static const double EIGEN_TOL = 1e-12;
static const double EIGEN_REAL_SNAP = 1e-9;

// Complex Givens rotation that maps (x, y) -> (r, 0) for r >= 0 real.
// Uses c = conj(x)/r, s = conj(y)/r so G = [[c, s], [-conj(s), conj(c)]].
static void cGivens(ComplexNumber x, ComplexNumber y, ComplexNumber* c, ComplexNumber* s) {
    double ax = complexAbs(x);
    double ay = complexAbs(y);
    double r = sqrt(ax*ax + ay*ay);
    if (r < 1e-300) {
        c->real = 1.0; c->imag = 0.0;
        s->real = 0.0; s->imag = 0.0;
        return;
    }
    c->real = x.real / r;  c->imag = -x.imag / r;
    s->real = y.real / r;  s->imag = -y.imag / r;
}

// Apply G (left) to rows p, q across columns [j0, j1).
//   row_p   <-  c*row_p + s*row_q
//   row_q   <- -conj(s)*row_p + conj(c)*row_q
static void applyGivensLeft(ComplexNumber* H, int n, int p, int q,
                            ComplexNumber c, ComplexNumber s, int j0, int j1) {
    ComplexNumber cs = complexConj(s);
    ComplexNumber cc = complexConj(c);
    for (int j = j0; j < j1; j++) {
        ComplexNumber a = H[p*n + j];
        ComplexNumber b = H[q*n + j];
        H[p*n + j] = complexAdd(complexMul(c, a), complexMul(s, b));
        H[q*n + j] = complexAdd(complexNeg(complexMul(cs, a)), complexMul(cc, b));
    }
}

// Apply G^* (right) to cols p, q across rows [i0, i1).
//   col_p   <-  conj(c)*col_p + conj(s)*col_q
//   col_q   <- -s*col_p + c*col_q
static void applyGivensRight(ComplexNumber* H, int n, int p, int q,
                             ComplexNumber c, ComplexNumber s, int i0, int i1) {
    ComplexNumber cs = complexConj(s);
    ComplexNumber cc = complexConj(c);
    for (int i = i0; i < i1; i++) {
        ComplexNumber a = H[i*n + p];
        ComplexNumber b = H[i*n + q];
        H[i*n + p] = complexAdd(complexMul(cc, a), complexMul(cs, b));
        H[i*n + q] = complexAdd(complexNeg(complexMul(s, a)), complexMul(c, b));
    }
}

// Reduce H (n x n) to upper Hessenberg form by similarity (in place).
static void hessenbergReduce(ComplexNumber* H, int n) {
    for (int k = 0; k < n - 2; k++) {
        for (int i = k + 2; i < n; i++) {
            ComplexNumber c, s;
            cGivens(H[(k+1)*n + k], H[i*n + k], &c, &s);
            applyGivensLeft(H, n, k + 1, i, c, s, k, n);
            applyGivensRight(H, n, k + 1, i, c, s, 0, n);
        }
    }
}

// Run shifted QR iteration on Hessenberg H (n x n); fill eigs[0..n-1].
static void qrIterate(ComplexNumber* H, int n, ComplexNumber* eigs) {
    if (n == 0) return;
    if (n == 1) { eigs[0] = H[0]; return; }

    int p = n;
    int totalIter = 0;
    int sinceDeflate = 0;
    int maxIter = 500 * n + 500;

    while (p > 1 && totalIter < maxIter) {
        // Try to deflate any small subdiagonal in [1, p-1]
        int split = -1;
        for (int i = p - 1; i >= 1; i--) {
            ComplexNumber sub = H[i*n + (i-1)];
            ComplexNumber d1 = H[i*n + i];
            ComplexNumber d2 = H[(i-1)*n + (i-1)];
            double tol = EIGEN_TOL * (complexAbs(d1) + complexAbs(d2)) + 1e-300;
            if (complexAbs(sub) < tol) {
                H[i*n + (i-1)] = (ComplexNumber){0.0, 0.0};
                if (i == p - 1) {
                    eigs[p - 1] = d1;
                    p--;
                    split = -2; // deflated tail
                    break;
                }
                split = i;
                break;
            }
        }
        if (split == -2) { sinceDeflate = 0; continue; }

        if (p <= 1) break;

        // Operate on the active block [start, p) where start = split (or 0).
        int start = (split >= 0) ? split : 0;

        // Wilkinson shift: eigenvalue of trailing 2x2 closer to H[p-1,p-1]
        ComplexNumber a = H[(p-2)*n + (p-2)];
        ComplexNumber b = H[(p-2)*n + (p-1)];
        ComplexNumber cc = H[(p-1)*n + (p-2)];
        ComplexNumber d = H[(p-1)*n + (p-1)];
        ComplexNumber tr = complexAdd(a, d);
        ComplexNumber det = complexSub(complexMul(a, d), complexMul(b, cc));
        ComplexNumber two = {2.0, 0.0};
        ComplexNumber four = {4.0, 0.0};
        ComplexNumber disc = complexSqrt(complexSub(complexMul(tr, tr), complexMul(four, det)));
        ComplexNumber lam1 = complexDiv(complexAdd(tr, disc), two);
        ComplexNumber lam2 = complexDiv(complexSub(tr, disc), two);
        ComplexNumber mu = (complexAbs(complexSub(lam1, d)) <
                            complexAbs(complexSub(lam2, d))) ? lam1 : lam2;

        // Exceptional shift: if stuck (e.g. trailing 2x2 has zero trace and det,
        // making Wilkinson shift = 0), perturb with the subdiagonal magnitude.
        if (sinceDeflate > 0 && sinceDeflate % 10 == 0) {
            double bump = complexAbs(H[(p-1)*n + (p-2)]);
            if (sinceDeflate % 20 == 0) {
                mu.real += 1.5 * bump;
                mu.imag += 0.7 * bump;
            } else {
                mu.real += 0.75 * bump;
            }
        }

        // H[start:p, start:p] -= mu*I
        for (int i = start; i < p; i++) H[i*n + i] = complexSub(H[i*n + i], mu);

        // QR step: zero subdiagonals via Givens, accumulating rotations.
        int rotCount = p - 1 - start;
        ComplexNumber* cs = malloc((rotCount > 0 ? rotCount : 1) * sizeof(ComplexNumber));
        ComplexNumber* ss = malloc((rotCount > 0 ? rotCount : 1) * sizeof(ComplexNumber));
        if (!cs || !ss) {
            free(cs); free(ss);
            for (int i = start; i < p; i++) H[i*n + i] = complexAdd(H[i*n + i], mu);
            for (int i = 0; i < p; i++) eigs[i] = H[i*n + i];
            return;
        }
        for (int i = start; i < p - 1; i++) {
            int idx = i - start;
            cGivens(H[i*n + i], H[(i+1)*n + i], &cs[idx], &ss[idx]);
            applyGivensLeft(H, n, i, i + 1, cs[idx], ss[idx], i, n);
        }
        // Apply Q on the right (R*Q): apply each G_i^* to columns i, i+1
        for (int i = start; i < p - 1; i++) {
            int idx = i - start;
            applyGivensRight(H, n, i, i + 1, cs[idx], ss[idx], 0, p);
        }
        free(cs); free(ss);

        // H[start:p, start:p] += mu*I
        for (int i = start; i < p; i++) H[i*n + i] = complexAdd(H[i*n + i], mu);

        totalIter++;
        sinceDeflate++;
    }

    // Anything left along the diagonal of the unconverged block is best estimate
    for (int i = 0; i < p; i++) eigs[i] = H[i*n + i];
}

// Build a complex copy of A (n x n) packed row-major.
static ComplexNumber* matrixToComplex(Matrix* A) {
    int n = A->numRows;
    ComplexNumber* H = malloc(n * n * sizeof(ComplexNumber));
    if (!H) return NULL;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            H[i*n + j] = elemToComplex(getEntry(A, i, j));
    return H;
}

// Find a single null-space vector of the n x n complex matrix M (modified in place).
// Writes a normalized vector to v. If M is full-rank within tolerance, returns 0;
// otherwise returns 1 on success.
static int complexNullVector(ComplexNumber* M, int n, ComplexNumber* v) {
    int* pivotCol = malloc(n * sizeof(int));
    if (!pivotCol) return 0;
    for (int i = 0; i < n; i++) pivotCol[i] = -1;

    int row = 0;
    for (int col = 0; col < n && row < n; col++) {
        // Partial pivot: largest |M[r, col]| for r in [row, n)
        int piv = -1;
        double best = 0.0;
        for (int r = row; r < n; r++) {
            double a = complexAbs(M[r*n + col]);
            if (a > best) { best = a; piv = r; }
        }
        if (piv < 0 || best < EIGEN_TOL) continue;
        if (piv != row) {
            for (int c = 0; c < n; c++) {
                ComplexNumber t = M[row*n + c];
                M[row*n + c] = M[piv*n + c];
                M[piv*n + c] = t;
            }
        }
        // Eliminate above and below
        ComplexNumber pv = M[row*n + col];
        for (int r = 0; r < n; r++) {
            if (r == row) continue;
            ComplexNumber f = complexDiv(M[r*n + col], pv);
            if (complexAbs(f) < 1e-300) continue;
            for (int c = col; c < n; c++) {
                M[r*n + c] = complexSub(M[r*n + c], complexMul(f, M[row*n + c]));
            }
        }
        // Normalize pivot row to 1
        for (int c = col; c < n; c++) M[row*n + c] = complexDiv(M[row*n + c], pv);
        pivotCol[row] = col;
        row++;
    }

    // Find a free column
    int freeCol = -1;
    for (int c = 0; c < n; c++) {
        bool found = false;
        for (int r = 0; r < n; r++) if (pivotCol[r] == c) { found = true; break; }
        if (!found) { freeCol = c; break; }
    }
    if (freeCol < 0) { free(pivotCol); return 0; }

    // Build null vector: v[freeCol] = 1, v[other free] = 0,
    // v[pivotCol[r]] = -M[r, freeCol]
    for (int i = 0; i < n; i++) { v[i].real = 0.0; v[i].imag = 0.0; }
    v[freeCol].real = 1.0;
    for (int r = 0; r < n; r++) {
        if (pivotCol[r] >= 0) {
            v[pivotCol[r]] = complexNeg(M[r*n + freeCol]);
        }
    }
    free(pivotCol);

    // Normalize
    double s = 0.0;
    for (int i = 0; i < n; i++) {
        double a = complexAbs(v[i]);
        s += a * a;
    }
    s = sqrt(s);
    if (s < 1e-300) {
        v[0].real = 1.0; v[0].imag = 0.0;
        return 1;
    }
    for (int i = 0; i < n; i++) {
        v[i].real /= s;
        v[i].imag /= s;
    }
    return 1;
}

// Returns true iff every entry of A is a real (non-complex) MatrixElement.
static bool matrixIsReal(Matrix* A) {
    int n = A->numRows;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            if (getEntry(A, i, j).isComplex) return false;
    return true;
}

// Compute eigenvalues of an arbitrary square matrix.
// Returns a freshly allocated array of length matrix->numRows on success.
MatrixElement* eigenvalues(Matrix* matrix) {
    if (!matrix) return NULL;
    if (!isSquare(matrix)) return NULL;

    int n = matrix->numRows;
    if (n == 2) return eigenvalues2x2(matrix);
    if (n == 3) return eigenvalues3x3(matrix);

    MatrixElement* out = malloc(n * sizeof(MatrixElement));
    if (!out) return NULL;
    if (n == 1) {
        out[0] = getEntry(matrix, 0, 0);
        return out;
    }

    ComplexNumber* H = matrixToComplex(matrix);
    if (!H) { free(out); return NULL; }
    ComplexNumber* eigs = malloc(n * sizeof(ComplexNumber));
    if (!eigs) { free(H); free(out); return NULL; }

    hessenbergReduce(H, n);
    qrIterate(H, n, eigs);

    // If the input was real, snap tiny imaginary residuals to 0
    bool inputReal = matrixIsReal(matrix);
    if (inputReal) {
        double scale = 0.0;
        for (int i = 0; i < n; i++) {
            double a = complexAbs(eigs[i]);
            if (a > scale) scale = a;
        }
        double tol = EIGEN_REAL_SNAP * scale + 1e-12;
        for (int i = 0; i < n; i++)
            if (fabs(eigs[i].imag) < tol) eigs[i].imag = 0.0;
    }

    for (int i = 0; i < n; i++) out[i] = elemFromComplex(eigs[i]);
    free(eigs);
    free(H);
    return out;
}

// Compute eigenvectors of an arbitrary square matrix.
// Returns a freshly allocated array of n column matrices, parallel to eigenvalues().
Matrix** eigenvectors(Matrix* matrix) {
    if (!matrix) return NULL;
    if (!isSquare(matrix)) return NULL;

    int n = matrix->numRows;
    if (n == 2) return eigenvectors2x2(matrix);
    if (n == 3) return eigenvectors3x3(matrix);

    MatrixElement* eigs = eigenvalues(matrix);
    if (!eigs) return NULL;

    Matrix** evects = malloc(n * sizeof(Matrix*));
    if (!evects) { free(eigs); return NULL; }
    for (int i = 0; i < n; i++) {
        evects[i] = constructMatrix(n, 1);
        if (!evects[i]) {
            for (int j = 0; j < i; j++) freeMatrix(evects[j]);
            free(evects); free(eigs);
            return NULL;
        }
    }

    if (n == 1) {
        setEntry(evects[0], 0, 0, elemFromReal(1.0));
        free(eigs);
        return evects;
    }

    bool inputReal = matrixIsReal(matrix);

    ComplexNumber* M = malloc(n * n * sizeof(ComplexNumber));
    ComplexNumber* v = malloc(n * sizeof(ComplexNumber));
    if (!M || !v) {
        free(M); free(v);
        for (int i = 0; i < n; i++) freeMatrix(evects[i]);
        free(evects); free(eigs);
        return NULL;
    }

    for (int k = 0; k < n; k++) {
        if (elemIsNan(eigs[k])) {
            freeMatrix(evects[k]);
            evects[k] = NULL;
            continue;
        }
        ComplexNumber lam = elemToComplex(eigs[k]);

        // Build M = A - lam*I
        for (int i = 0; i < n; i++)
            for (int j = 0; j < n; j++)
                M[i*n + j] = elemToComplex(getEntry(matrix, i, j));
        for (int i = 0; i < n; i++) M[i*n + i] = complexSub(M[i*n + i], lam);

        if (!complexNullVector(M, n, v)) {
            // Fallback: e_0
            for (int i = 0; i < n; i++) { v[i].real = 0.0; v[i].imag = 0.0; }
            v[0].real = 1.0;
        }

        // Snap tiny imaginary parts to 0 for real inputs with real eigenvalue
        if (inputReal && fabs(lam.imag) < 1e-12) {
            double scale = 0.0;
            for (int i = 0; i < n; i++) {
                double a = complexAbs(v[i]);
                if (a > scale) scale = a;
            }
            double tol = EIGEN_REAL_SNAP * scale + 1e-12;
            for (int i = 0; i < n; i++)
                if (fabs(v[i].imag) < tol) v[i].imag = 0.0;
        }

        for (int i = 0; i < n; i++)
            setEntry(evects[k], i, 0, elemFromComplex(v[i]));
    }

    free(M); free(v); free(eigs);
    return evects;
}
