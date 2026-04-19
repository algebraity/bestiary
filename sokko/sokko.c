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
double getEntry(const Matrix* matrix, int i, int j) {
    return matrix->data[i * matrix->numCols + j];
}

// Set the value at position (i, j) of the matrix, 0-indexed
void setEntry(Matrix* matrix, int i, int j, double x) {
    matrix->data[i * matrix->numCols + j] = x;
    resetMatrixCache(matrix);
}

/* ---------- LU methods ----------- */

// Compute the determinant of an LU
double luDet(LU* lu) {
    double det = lu->sign;
    for (int i = 0; i < lu->n; i++) det *= lu->data[i*lu->n + i];
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
    lu->data = malloc(n * n * sizeof(double));
    lu->perm = malloc(n * sizeof(int));
    if (!lu->data || !lu->perm) { freeLU(lu); return NULL; }
    for (int i = 0; i < n * n; i++) lu->data[i] = matrix->data[i];
    
    // Initialize permutation: row i is originally at position i
    for (int i = 0; i < n; i++) lu->perm[i] = i;
    
    double* a = lu->data;  // shorthand for clarity
    
    for (int k = 0; k < n; k++) {
        // Partial pivoting: find the row with the largest |a[i,k]| for i >= k
        int pivot = k;
        double maxVal = fabs(a[k * n + k]);
        for (int i = k + 1; i < n; i++) {
            double v = fabs(a[i * n + k]);
            if (v > maxVal) { maxVal = v; pivot = i; }
        }
        
        // Singular (or close enough): no valid LU exists
        if (maxVal < 1e-12) { freeLU(lu); return NULL; }
        
        // Swap row k with pivot row, if needed
        if (pivot != k) {
            for (int j = 0; j < n; j++) {
                double tmp = a[k * n + j];
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
            double factor = a[i * n + k] / a[k * n + k];
            a[i * n + k] = factor;
            for (int j = k + 1; j < n; j++) {
                a[i * n + j] -= factor * a[k * n + j];
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
        double sum = x->data[i];
        for (int j = 0; j < i; j++) {
            sum -= lu->data[i*n + j] * x->data[j];
        }
        x->data[i] = sum;
    }

    // Use backward-substitution for Ux = y
    for (int i = n-1; i >= 0; i--) {
	double sum = x->data[i];
	for (int j = i+1; j < n; j++) {
	    sum -= lu->data[i*n + j] * x->data[j];
	}
	x->data[i] = sum / lu->data[i*n+i];
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
	    setEntry(cols[i], i, 0, 1);
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

    matrix->data = calloc(matrix->numRows * matrix->numCols, sizeof(double));
    if (!matrix->data) {
	free(matrix);
	return NULL;
    }
    matrix->cachedLU = NULL;

    return matrix;
}

// Construct a matrix from a 2D data matrix
Matrix* constructMatrixFromMatrix(int numRows, int numCols, double** data, int lenData, int colLenData) {
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
Matrix* constructMatrixFromArray(int numRows, int numCols, double* data, int lenData) {
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
	    printf("%g, ", matrix->data[i*matrix->numCols + j]);
	}
	printf("%g |\n", matrix->data[i*matrix->numCols + matrix->numCols-1]);
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
	    if (fabs(getEntry(A, i, j) - getEntry(B, i, j)) > tol) return false;
	}
    }

    return true;
}

// Get the n x n identity matrix
Matrix* idMatrix(int n) {
    Matrix* id = constructMatrix(n, n);
    if (!id) return NULL;

    for (int i = 0; i < n; i++) setEntry(id, i, i, 1);

    return id;
}

/* ---------- Basic operations ---------- */

// Compute the dot product of two arrays
double simpleDotProduct(double* v, double* w, int vlen, int wlen) {
    if (vlen != wlen) return NAN;

    double dp = 0;
    for (int i = 0; i < vlen; i++) dp += v[i] * w[i];
    return dp;
}

// Compute the dot product of two n x 1 matrices
double dotProduct(Matrix* v, Matrix* w) {
    if (v->numCols != 1 || w->numCols != 1) return NAN;
    if (v->numRows != w->numRows) return NAN;
    
    double dp = 0;
    for (int i = 0; i < v->numRows; i++) dp += getEntry(v, i, 0) * getEntry(w, i, 0);
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
        double entry = 0;
        for (int j = 0; j < A->numCols; j++) {
            entry += getEntry(A, i, j) * getEntry(v, j, 0);
        }
        setEntry(toReturn, i, 0, entry);
    }

    return toReturn;
}

// Multiply a Matrix by a constant c
Matrix* multByConstant(Matrix* matrix, double c) {
    if (!matrix) return NULL;
    Matrix* toReturn = copyMatrix(matrix);
    if (!toReturn) return NULL;
    for (int i = 0; i < matrix->numRows * matrix->numCols; i++) {
	toReturn->data[i] = toReturn->data[i] * c;
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
	C->data[i] = A->data[i] + B->data[i];
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
	C->data[i] = A->data[i] - B->data[i];
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
	        double s = 0;
	        for (int k = 0; k < A->numCols; k++) {
		        s += getEntry(A, i, k) * getEntry(B, k, j);
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
	    double aij = getEntry(A, i, j);
	    for (int r = 0; r < p; r++) {
		for (int s = 0; s < q; s++) {
		    double brs = getEntry(B, r, s);
		    int I = i*p+r;
		    int J = j*q+s;
		    setEntry(C, I, J, aij * brs);
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
Vector* constructVectorFromArray(int dim, double *data, int dataLen) {
    if (dim < 1) return NULL;
    if (!data) return NULL;
    if (dim != dataLen) return NULL;

    Vector* vec = constructMatrixFromArray(dim, 1, data, dataLen);
    if (!vec) return NULL;
    return vec;
}

// Construct a Vector of dim 2 given two inputs
Vector* constructVector2(double x, double y) {
    if (isnan(x) || isnan(y)) return NULL;

    Vector* vec = constructMatrix(2, 1);
    if (!vec) return NULL;
    setEntry(vec, 0, 0, x);
    setEntry(vec, 1, 0, y);

    return vec;
}

// Construct a Vector of dim 3 given three inputs
Vector* constructVector3(double x, double y, double z) {
    if (isnan(x) || isnan(y) || isnan(z)) return NULL;

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
        setEntry(sum, i, 0, getEntry(v1, i, 0) + getEntry(v2, i, 0));
    }
    return sum;
}

// Returns the dot product of two Vectors
double vectorDotProduct(Vector* v1, Vector* v2) {
    if (!v1 || !v2) return NAN;
    if (v1->numRows != v2->numRows || v1->numCols != v2->numCols || v1->numCols != 1) return NAN;

    double dot = 0;
    int dim = v1->numRows;
    for (int i = 0; i < dim; i++) {
        dot += getEntry(v1, i, 0) * getEntry(v2, i, 0);
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
    setEntry(crossProd, 0, 0, getEntry(v1, 1, 0) * getEntry(v2, 2, 0) - getEntry(v1, 2, 0) * getEntry(v2, 1, 0));
    setEntry(crossProd, 1, 0, getEntry(v1, 2, 0) * getEntry(v2, 0, 0) - getEntry(v1, 0, 0) * getEntry(v2, 2, 0));
    setEntry(crossProd, 2, 0, getEntry(v1, 0, 0) * getEntry(v2, 1, 0) - getEntry(v1, 1, 0) * getEntry(v2, 0, 0));

    return crossProd;
}

// Returns the L2 norm of a Vector
double l2Norm(Vector* vect) {
    if (!vect) return NAN;

    int dim = vect->numRows;
    double norm = 0;
    for (int i = 0; i < dim; i++) {
        norm += getEntry(vect, i, 0) * getEntry(vect, i, 0);
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
        setEntry(neg, i, 0, - getEntry(vect, i, 0));
    }

    return neg;
}

// Scale a Vector by a constant k
Vector* scaleVector(Vector* vect, double k) {
    if (!vect || isnan(k)) return NULL;

    int dim = vect->numRows;
    Vector* scaled = constructVector(dim);
    if (!scaled) return NULL;
    for (int i = 0; i < dim; i++) {
        setEntry(scaled, i, 0, getEntry(vect, i, 0) * k);
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
		setEntry(diff, i, 0, getEntry(v1, i, 0) - getEntry(v2, i, 0));
	}
	return diff;
}

// Normalize a Vector v
Vector* normalizeVector(Vector* vect) {
	if (!vect) return NULL;

	double norm = l2Norm(vect);
	if (norm == 0) return NULL;

	return scaleVector(vect, 1.0 / norm);
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

// Compute the angle between two Vectors in radians
double vectorAngle(Vector* v1, Vector* v2) {
	if (!v1 || !v2) return NAN;
	if (v1->numRows != v2->numRows || v1->numCols != v2->numCols || v1->numCols != 1) return NAN;

	double dot = vectorDotProduct(v1, v2);
	double normV1 = l2Norm(v1);
	double normV2 = l2Norm(v2);
	if (normV1 == 0 || normV2 == 0) return NAN;

	double cosTheta = dot / (normV1 * normV2);
	if (cosTheta > 1) cosTheta = 1;
	if (cosTheta < -1) cosTheta = -1;

	return acos(cosTheta);
}

// Compute the projection of v1 onto v2
Vector* vectorProjectOnto(Vector* v1, Vector* v2) {
	if (!v1 || !v2) return NULL;
	if (v1->numRows != v2->numRows || v1->numCols != v2->numCols || v1->numCols != 1) return NULL;

	double dot = vectorDotProduct(v1, v2);
	double normV2 = l2Norm(v2);
	if (normV2 == 0) return NULL;

	return scaleVector(v2, dot / (normV2 * normV2));
}

/* ---------- Matrix invariants ---------- */

// Returns true if the Matrix is symmetric, else false
bool isSymmetric(Matrix* matrix) {
    if (!matrix) return false;
    if (!isSquare(matrix)) return false;

    Matrix* trans = transpose(matrix);
    if (!trans) return false;
    bool sym = matrixComp(matrix, trans, 1e-12);

    freeMatrix(trans);
    return sym;
}

// Returns true if the Matrix is antisymmetric, else false
bool isAntisymmetric(Matrix* matrix) {
    if (!matrix) return false;
    if (!isSquare(matrix)) return false;

    Matrix* trans = transpose(matrix);
    if (!trans) return false;
    Matrix* negTrans = multByConstant(trans, -1);
    if (!negTrans) {
        freeMatrix(trans);
        return false;
    }
    bool antisym = matrixComp(matrix, negTrans, 1e-12);

    freeMatrix(trans);
    freeMatrix(negTrans);
    return antisym;
}

// Returns true if the Matrix is orthogonal, else false
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

// Return the rank of a Matrix
int rank(Matrix* matrix) {
    if (!matrix) return -1;

    int m = matrix->numRows;
    int n = matrix->numCols;

    double* a = malloc(m * n * sizeof(double));
    if (!a) return -1;
    for (int k = 0; k < m * n; k++) a[k] = matrix->data[k];

    int rnk = 0;
    int row = 0;
    for (int col = 0; col < n && row < m; col++) {
        int pivot = -1;
        double maxVal = 1e-12;
        for (int i = row; i < m; i++) {
            double v = fabs(a[i * n + col]);
            if (v > maxVal) { maxVal = v; pivot = i; }
        }
        if (pivot == -1) continue;

        if (pivot != row) {
            for (int j = 0; j < n; j++) {
                double tmp = a[row * n + j];
                a[row * n + j] = a[pivot * n + j];
                a[pivot * n + j] = tmp;
            }
        }

        for (int i = row + 1; i < m; i++) {
            double factor = a[i * n + col] / a[row * n + col];
            for (int j = col; j < n; j++) {
                a[i * n + j] -= factor * a[row * n + j];
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
double trace(Matrix* matrix) {
    if (!matrix) return NAN;
    if (!isSquare(matrix)) return NAN;

    double trc = 0.0;
    for (int i = 0; i < matrix->numCols; i++) {
        trc += getEntry(matrix, i, i);
    }

    return trc;
}

// Return the Frobenius norm of a matrix
double frobeniusNorm(Matrix* matrix) {
    if (!matrix) return NAN;

    double sum = 0.0;
    for (int i = 0; i < matrix->numRows * matrix->numCols; i++) {
        sum += matrix->data[i] * matrix->data[i];
    }
    return sqrt(sum);
}

// Compute the determinant of a Matrix
double determinant(Matrix* matrix) {
    if (!isSquare(matrix)) return NAN;
    int n = matrix->numRows;
    
    // Hardcoded fast paths for tiny matrices to avoid LU overhead
    if (n == 1) return matrix->data[0];
    if (n == 2) return matrix->data[0] * matrix->data[3] - matrix->data[1] * matrix->data[2];

    cacheLU(matrix);
    if (!matrix->cachedLU) return 0;
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

// Get the eigenvalues of a 2 x 2 matrix
double* eigenvalues2x2(Matrix* matrix) {
    if (!matrix) return NULL;
    if (matrix->numRows != 2 || matrix->numCols != 2) return NULL;

    double a = getEntry(matrix, 0, 0);
    double b = getEntry(matrix, 0, 1);
    double c = getEntry(matrix, 1, 0);
    double d = getEntry(matrix, 1, 1);

    double trace = a + d;
    double det = a*d - b*c;
    double discriminant = trace*trace - 4*det;

    double* eigs = malloc(2 * sizeof(double));
    if (!eigs) return NULL;

    if (discriminant >= 0) {
        double sqrtDisc = sqrt(discriminant);
        eigs[0] = (trace + sqrtDisc) / 2;
        eigs[1] = (trace - sqrtDisc) / 2;
    } else {
        eigs[0] = NAN;
        eigs[1] = NAN;
    }

    return eigs;
}

// Get the eigenvalues of a 3x3 matrix using the characteristic polynomial
double* eigenvalues3x3(Matrix* matrix) {
    if (!matrix) return NULL;
    if (matrix->numRows != 3 || matrix->numCols != 3) return NULL;

    double a = getEntry(matrix, 0, 0);
    double b = getEntry(matrix, 0, 1);
    double c = getEntry(matrix, 0, 2);
    double d = getEntry(matrix, 1, 0);
    double e = getEntry(matrix, 1, 1);
    double f = getEntry(matrix, 1, 2);
    double g = getEntry(matrix, 2, 0);
    double h = getEntry(matrix, 2, 1);
    double i = getEntry(matrix, 2, 2);

    // Characteristic polynomial: lambda^2 + p·lambda^2 + q·lambda + r = 0
    double p = -(a + e + i);
    double q = a*e + a*i + e*i - b*d - c*g - f*h;
    double r = -(a*e*i + b*f*g + c*d*h - c*e*g - b*d*i - a*f*h);

    // Depress the cubic: substitute lambda = t - p/3 to get t^3 + P·t + Q = 0
    double P = q - p*p/3.0;
    double Q = 2.0*p*p*p/27.0 - p*q/3.0 + r;

    // Discriminant of the depressed cubic
    double disc = -4.0*P*P*P - 27.0*Q*Q;

    double* eigs = malloc(3 * sizeof(double));
    if (!eigs) return NULL;

    if (disc >= 0) {
        double shift = -p / 3.0;
        if (fabs(P) < 1e-12) {
            // Triple root: t^3 + Q = 0 with Q = 0 (since disc = -27Q^2 >= 0 => Q = 0)
            eigs[0] = shift;
            eigs[1] = shift;
            eigs[2] = shift;
        } else {
            // Use the trigonometric method for three real roots
            double m = 2.0 * sqrt(-P / 3.0);
            double cosArg = 3.0*Q / (P*m);
            if (cosArg >  1.0) cosArg =  1.0;
            if (cosArg < -1.0) cosArg = -1.0;
            double theta = acos(cosArg) / 3.0;

            eigs[0] = m * cos(theta) + shift;
            eigs[1] = m * cos(theta - 2.0*M_PI/3.0) + shift;
            eigs[2] = m * cos(theta - 4.0*M_PI/3.0) + shift;
        }
    } else {
        // One real root, two complex: use Cardano
        double sqrtTerm = sqrt(-disc / 108.0);
        double u = cbrt(-Q/2.0 + sqrtTerm);
        double v = cbrt(-Q/2.0 - sqrtTerm);

        eigs[0] = u + v - p/3.0;
        eigs[1] = NAN;
        eigs[2] = NAN;
    }

    return eigs;
}

// Return the eigenvectors of a 2x2 Matrix
Matrix** eigenvectors2x2(Matrix* matrix) {
    if (!matrix) return NULL;
    if (!isSquare(matrix)) return NULL;
    if (!(matrix->numRows == 2)) return NULL;

    // Get eigenvales and allocate memory
    double* eigs = eigenvalues2x2(matrix);
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
        double lambda = eigs[i];

        if (isnan(lambda)) {
            freeMatrix(evects[i]);
            evects[i] = NULL;
            continue;
        }

        double a = getEntry(matrix, 0, 0) - lambda;
        double b = getEntry(matrix, 0, 1);
        double c = getEntry(matrix, 1, 0);
        double d = getEntry(matrix, 1, 1) - lambda;

        if (fabs(b) > 1e-12 || fabs(a) > 1e-12) {
            setEntry(evects[i], 0, 0, b);
            setEntry(evects[i], 1, 0, -a);
        } else if (fabs(c) > 1e-12 || fabs(d) > 1e-12) {
            setEntry(evects[i], 0, 0, d);
            setEntry(evects[i], 1, 0, -c);
        } else {
            setEntry(evects[i], 0, 0, 1);
            setEntry(evects[i], 1, 0, 0);
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
    double* eigs = eigenvalues3x3(matrix);
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
        double lambda = eigs[i];

        if (isnan(lambda)) {
            freeMatrix(evects[i]);
            evects[i] = NULL;
            continue;
        }

        // Rows of (A - eig*I)
        double r00 = getEntry(matrix, 0, 0) - lambda;
        double r01 = getEntry(matrix, 0, 1);
        double r02 = getEntry(matrix, 0, 2);
        double r10 = getEntry(matrix, 1, 0);
        double r11 = getEntry(matrix, 1, 1) - lambda;
        double r12 = getEntry(matrix, 1, 2);
        double r20 = getEntry(matrix, 2, 0);
        double r21 = getEntry(matrix, 2, 1);
        double r22 = getEntry(matrix, 2, 2) - lambda;

        // Find null space vector via cross products of row pairs
        double v0, v1, v2, norm;

        v0 = r01*r12 - r02*r11;
        v1 = r02*r10 - r00*r12;
        v2 = r00*r11 - r01*r10;
        norm = sqrt(v0*v0 + v1*v1 + v2*v2);

        if (norm <= 1e-12) {
            v0 = r01*r22 - r02*r21;
            v1 = r02*r20 - r00*r22;
            v2 = r00*r21 - r01*r20;
            norm = sqrt(v0*v0 + v1*v1 + v2*v2);
        }

        if (norm <= 1e-12) {
            v0 = r11*r22 - r12*r21;
            v1 = r12*r20 - r10*r22;
            v2 = r10*r21 - r11*r20;
            norm = sqrt(v0*v0 + v1*v1 + v2*v2);
        }

        if (norm <= 1e-12) {
            v0 = 1; v1 = 0; v2 = 0;
        }

        setEntry(evects[i], 0, 0, v0);
        setEntry(evects[i], 1, 0, v1);
        setEntry(evects[i], 2, 0, v2);
    }

    free(eigs);
    return evects;
}
