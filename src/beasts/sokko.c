#include<stdlib.h>
#include<stdio.h>
#include<math.h>
#include<string.h>
#include<stdint.h>
#include "hebi.h"
#include "sokko.h"

/* ---------- Helper methods ---------- */

// Return the shared real field used by default SOKKO constructors
Field* sokkoRealField(void) {
    static Field field;
    static bool initialized = false;
    if (!initialized) {
        field = constructRRField();
        initialized = true;
    }
    return &field;
}

// Return the shared complex field used by SOKKO complex helpers
Field* sokkoComplexField(void) {
    static Field field;
    static bool initialized = false;
    if (!initialized) {
        field = constructCCField();
        initialized = true;
    }
    return &field;
}

// Check whether a Field can be used for matrix arithmetic
static bool fieldSupportsMatrices(Field* field) {
    FieldElement zero;
    bool valid;
    if (field == NULL) return false;

    // Probe the field by constructing and validating its zero element
    zero = zeroFieldElement(field);
    valid = fieldElementIsValid(&zero);
    freeFieldElement(&zero);
    return valid;
}

// Check whether a field has a faithful numeric projection into C
static bool fieldSupportsNumericProjection(Field* field) {
    return field != NULL && (field->type == QQ || field->type == RR || field->type == CC);
}

static void freeMatrixField(Field* field);

// Copy a field ownership chain for Matrix storage
static Field* copyMatrixField(Field* field) {
    Field* copy;
    if (!fieldSupportsMatrices(field)) return NULL;

    // Allocate the top-level field and dispatch by representation
    copy = calloc(1, sizeof(Field));
    if (copy == NULL) return NULL;
    switch (field->type) {
        case QQ:
            *copy = constructQQField();
            return copy;
        case RR:
            *copy = constructRRField();
            return copy;
        case CC:
            *copy = constructCCField();
            return copy;
        case FF:
            *copy = constructFFField(field->data.ff.p, field->data.ff.degree,
                    field->data.ff.modulus);
            return fieldSupportsMatrices(copy) ? copy : (free(copy), NULL);
        case FF_QEXT:
            copy->repr = field->repr;
            copy->chr = field->chr;
            copy->type = FF_QEXT;
            copy->data.ffqext.baseField = copyMatrixField(field->data.ffqext.baseField);
            copy->data.ffqext.genRepr = field->data.ffqext.genRepr;
            copy->data.ffqext.radicand = calloc(1, sizeof(FieldElement));
            if (copy->data.ffqext.baseField == NULL || copy->data.ffqext.radicand == NULL) {
                freeMatrixField(copy);
                return NULL;
            }
            *copy->data.ffqext.radicand = copyFieldElementToField(
                    copy->data.ffqext.baseField, *field->data.ffqext.radicand);
            if (!fieldSupportsMatrices(copy)) {
                freeMatrixField(copy);
                return NULL;
            }
            return copy;
        case NF:
            copy->repr = field->repr;
            copy->chr = field->chr;
            copy->type = NF;
            copy->data.nf.baseField = copyMatrixField(field->data.nf.baseField);
            copy->data.nf.gen.repr = field->data.nf.gen.repr;
            copy->data.nf.gen.degree = field->data.nf.gen.degree;
            copy->data.nf.gen.minPolyCoeffs =
                calloc(field->data.nf.gen.degree + 1, sizeof(FieldElement));
            if (copy->data.nf.baseField == NULL || copy->data.nf.gen.minPolyCoeffs == NULL) {
                freeMatrixField(copy);
                return NULL;
            }
            for (size_t i = 0; i <= field->data.nf.gen.degree; i++) {
                copy->data.nf.gen.minPolyCoeffs[i] = copyFieldElementToField(
                        copy->data.nf.baseField, field->data.nf.gen.minPolyCoeffs[i]);
                if (!fieldElementIsValid(&copy->data.nf.gen.minPolyCoeffs[i])) {
                    freeMatrixField(copy);
                    return NULL;
                }
            }
            return fieldSupportsMatrices(copy) ? copy : (freeMatrixField(copy), NULL);
    }
    free(copy);
    return NULL;
}

// Free a Matrix-owned field ownership chain
static void freeMatrixField(Field* field) {
    Field* base = NULL;
    if (field == NULL) return;

    // Save the non-owning base pointer before freeField clears the top-level metadata
    if (field->type == FF_QEXT) base = field->data.ffqext.baseField;
    else if (field->type == NF) base = field->data.nf.baseField;
    freeField(field);
    freeMatrixField(base);
    free(field);
}

// Check whether one field can be embedded into another
static bool fieldEmbedsInto(Field* source, Field* target) {
    if (!fieldSupportsMatrices(source) || !fieldSupportsMatrices(target)) return false;
    if (fieldEq(source, target)) return true;

    // Test the public embedding path with the multiplicative identity
    FieldElement one = oneFieldElement(source);
    FieldElement embedded = embedFieldElement(target, one);
    bool ok = fieldElementIsValid(&embedded) && fieldEq(target, embedded.field);
    freeFieldElement(&one);
    freeFieldElement(&embedded);
    return ok;
}

// Return the flattened array index of a Matrix entry
static size_t matrixIndex(const Matrix* matrix, size_t i, size_t j) {
    return i * matrix->numCols + j;
}

// Return true when a FieldElement has a nonzero imaginary component
bool elemIsComplex(FieldElement x) {
    return fieldElementIsValid(&x) && x.field != NULL && x.field->type == CC
        && x.value.z.imag != 0.0L;
}

// Check whether matrix dimensions have a representable entry count
static bool matrixEntryCount(size_t rows, size_t cols, size_t* count) {
    if (count == NULL || rows == 0 || cols == 0 || rows > SIZE_MAX / cols) return false;
    *count = rows * cols;
    return true;
}

// Check whether matrix indices are valid
static bool matrixIndicesAreValid(const Matrix* matrix, size_t i, size_t j) {
    return matrix != NULL && i < matrix->numRows && j < matrix->numCols;
}

// Check whether a Matrix has valid structural data
static bool matrixIsValid(Matrix* matrix) {
    size_t count;
    if (matrix == NULL || !fieldSupportsMatrices(matrix->field)
            || matrix->numRows == 0 || matrix->numCols == 0 || matrix->data == NULL) {
        return false;
    }
    if (!matrixEntryCount(matrix->numRows, matrix->numCols, &count)) return false;

    // Check that every matrix entry is valid over the matrix field
    for (size_t i = 0; i < count; i++) {
        if (!fieldElementIsValid(&matrix->data[i])
                || !fieldEq(matrix->field, matrix->data[i].field)) {
            return false;
        }
    }
    return true;
}

// Return whether a Matrix row is zero up to matrix tolerance
static bool matrixRowIsZero(Matrix* matrix, size_t row) {
    if (!matrixIsValid(matrix) || row >= matrix->numRows) return true;

    // Scan the row for any nonzero entry
    for (size_t j = 0; j < matrix->numCols; j++) {
        if (!elemIsZero(matrix->data[matrixIndex(matrix, row, j)], 1e-12)) return false;
    }
    return true;
}

// Return the promoted field for two scalar fields
static Field* promotedField(Field* a, Field* b) {
    if (!fieldSupportsMatrices(a) || !fieldSupportsMatrices(b)) return NULL;
    if (fieldEq(a, b)) return a;
    if ((a->type == CC && (b->type == RR || b->type == QQ))
            || (b->type == CC && (a->type == RR || a->type == QQ))) {
        return sokkoComplexField();
    }
    if ((a->type == RR && b->type == QQ) || (b->type == RR && a->type == QQ)) {
        return sokkoRealField();
    }
    if (fieldEmbedsInto(b, a)) return a;
    if (fieldEmbedsInto(a, b)) return b;
    return NULL;
}

// Infer a common field for a flattened data array
static Field* inferFieldFromArray(FieldElement* data, size_t lenData) {
    if (data == NULL || lenData == 0) return NULL;
    if (!fieldElementIsValid(&data[0])) return NULL;
    Field* field = data[0].field;

    // Promote the candidate field as each entry is inspected
    for (size_t i = 1; i < lenData; i++) {
        if (!fieldElementIsValid(&data[i])) return NULL;
        field = promotedField(field, data[i].field);
        if (field == NULL) return NULL;
    }
    return field;
}

// Check whether every matrix entry can be projected numerically
static bool matrixSupportsNumericProjection(Matrix* matrix) {
    return matrixIsValid(matrix) && fieldSupportsNumericProjection(matrix->field);
}

// Promote every entry of a Matrix to a compatible target field
static bool promoteMatrixToField(Matrix* matrix, Field* targetField) {
    size_t count;
    FieldElement* promoted;
    Field* targetCopy;
    Field* oldField;
    bool oldOwnsField;
    if (!matrixIsValid(matrix) || !fieldSupportsMatrices(targetField)) return false;
    if (fieldEq(matrix->field, targetField)) return true;
    if (!matrixEntryCount(matrix->numRows, matrix->numCols, &count)) return false;

    // Copy the target field so promoted entries remain valid after the caller returns
    targetCopy = copyMatrixField(targetField);
    if (targetCopy == NULL) return false;

    // Embed all entries before mutating the matrix field
    promoted = calloc(count, sizeof(FieldElement));
    if (promoted == NULL) {
        freeMatrixField(targetCopy);
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        promoted[i] = embedFieldElement(targetCopy, matrix->data[i]);
        if (!fieldElementIsValid(&promoted[i])) {
            for (size_t j = 0; j <= i; j++) freeFieldElement(&promoted[j]);
            free(promoted);
            freeMatrixField(targetCopy);
            return false;
        }
    }

    // Replace the old entries with their promoted copies
    oldField = matrix->field;
    oldOwnsField = matrix->ownsField;
    for (size_t i = 0; i < count; i++) freeFieldElement(&matrix->data[i]);
    free(matrix->data);
    if (oldOwnsField && oldField != NULL) freeMatrixField(oldField);
    matrix->field = targetCopy;
    matrix->ownsField = true;
    matrix->data = promoted;
    resetMatrixCache(matrix);
    return true;
}

// Return the promoted field for two Matrices
static Field* promotedMatrixField(Matrix* A, Matrix* B) {
    if (!matrixIsValid(A) || !matrixIsValid(B)) return NULL;
    return promotedField(A->field, B->field);
}

// Return zero in the requested Field
static FieldElement elemZeroForField(Field* field) {
    if (!fieldSupportsMatrices(field)) return (FieldElement){0};
    return zeroFieldElement(field);
}

// Return one in the requested Field
static FieldElement elemOneForField(Field* field) {
    if (!fieldSupportsMatrices(field)) return (FieldElement){0};
    return oneFieldElement(field);
}

// Set an owned Matrix entry
static bool setOwnedEntry(Matrix* matrix, size_t index, FieldElement value) {
    if (!matrixIsValid(matrix) || !fieldElementIsValid(&value)
            || !fieldEq(matrix->field, value.field)) {
        freeFieldElement(&value);
        return false;
    }

    // Rebase structurally equal entries onto the matrix's owned field
    if (value.field != matrix->field) {
        FieldElement rebased = copyFieldElementToField(matrix->field, value);
        freeFieldElement(&value);
        value = rebased;
        if (!fieldElementIsValid(&value)) return false;
    }

    // Replace the existing entry with the owned value
    freeFieldElement(&matrix->data[index]);
    matrix->data[index] = value;
    return true;
}

// Return true if the Matrix is square, false otherwise
bool isSquare(Matrix* matrix) {
    return matrixIsValid(matrix) && matrix->numRows == matrix->numCols;
}

/* ---------- Free methods ---------- */

// Free an LU struct
void freeLU(LU* lu) {
    size_t count = 0;
    if (!lu) return;
    if (lu->data != NULL && matrixEntryCount(lu->n, lu->n, &count)) {
        for (size_t i = 0; i < count; i++) {
            freeFieldElement(&lu->data[i]);
        }
    }
    free(lu->data);
    free(lu->perm);
    free(lu);
}

// Reset the cache for a Matrix struct
void resetMatrixCache(Matrix* matrix) {
    if (matrix == NULL) return;
    freeLU(matrix->cachedLU);
    matrix->cachedLU = NULL;
}

// Free the memory controlled by the Matrix object
void freeMatrix(Matrix* matrix) {
    size_t count = 0;
    if (!matrix) return;
    resetMatrixCache(matrix);
    if (matrix->data != NULL && matrixEntryCount(matrix->numRows, matrix->numCols, &count)) {
        for (size_t i = 0; i < count; i++) {
            freeFieldElement(&matrix->data[i]);
        }
    }
    free(matrix->data);
    if (matrix->ownsField && matrix->field != NULL) freeMatrixField(matrix->field);
    free(matrix);
}

/* ---------- Accessors ---------- */

// Get the values at position (i, j) of the matrix, 0-indexed
FieldElement getEntry(const Matrix* matrix, size_t i, size_t j) {
    Matrix* mutableMatrix = (Matrix*)matrix;
    if (!matrixIndicesAreValid(matrix, i, j) || !matrixIsValid(mutableMatrix)) {
        return (FieldElement){0};
    }
    return copyFieldElement(matrix->data[i * matrix->numCols + j]);
}

// Set the value at position (i, j) of the matrix, 0-indexed
void setEntry(Matrix* matrix, size_t i, size_t j, FieldElement x) {
    FieldElement copy;
    Field* field;
    if (!matrixIndicesAreValid(matrix, i, j) || !matrixIsValid(matrix)
            || !fieldElementIsValid(&x)) {
        return;
    }
    field = promotedField(matrix->field, x.field);
    if (field != NULL && !promoteMatrixToField(matrix, field)) return;

    // Embed the incoming value and clear any cached decomposition after replacement
    copy = embedFieldElement(matrix->field, x);
    if (!fieldElementIsValid(&copy)) return;
    if (setOwnedEntry(matrix, matrixIndex(matrix, i, j), copy)) {
        resetMatrixCache(matrix);
    }
}

/* ---------- FieldElement operations ---------- */

// Wrap a long double in a FieldElement
FieldElement elemFromReal(long double x) {
    return constructRRElement(sokkoRealField(), x);
}

// Wrap a ComplexNumber in a FieldElement; collapses to real when imag == 0
FieldElement elemFromComplex(ComplexNumber c) {
    if (c.imag == 0.0L) return elemFromReal(c.real);
    return constructCCElement(sokkoComplexField(), c);
}

// Promote a FieldElement to a ComplexNumber
ComplexNumber elemToComplex(FieldElement a) {
    if (!fieldElementIsValid(&a) || a.field == NULL) return (ComplexNumber){NAN, NAN};
    if (a.field->type == CC) return a.value.z;
    if (a.field->type == RR) return (ComplexNumber){a.value.real, 0.0L};
    if (a.field->type == QQ) {
        return (ComplexNumber){
            (long double)a.value.frac.num / (long double)a.value.frac.denom, 0.0L
        };
    }
    return (ComplexNumber){NAN, NAN};
}

// Add two FieldElements
FieldElement elemAdd(FieldElement a, FieldElement b) {
    Field* field = promotedField(a.field, b.field);
    if (field == NULL) return (FieldElement){0};
    FieldElement aa = embedFieldElement(field, a);
    FieldElement bb = embedFieldElement(field, b);
    FieldElement sum = addFieldElements(aa, bb);
    freeFieldElement(&aa);
    freeFieldElement(&bb);
    return sum;
}

// Subtract two FieldElements
FieldElement elemSub(FieldElement a, FieldElement b) {
    Field* field = promotedField(a.field, b.field);
    if (field == NULL) return (FieldElement){0};
    FieldElement aa = embedFieldElement(field, a);
    FieldElement bb = embedFieldElement(field, b);
    FieldElement diff = subtractFieldElements(aa, bb);
    freeFieldElement(&aa);
    freeFieldElement(&bb);
    return diff;
}

// Multiply two FieldElements
FieldElement elemMul(FieldElement a, FieldElement b) {
    Field* field = promotedField(a.field, b.field);
    if (field == NULL) return (FieldElement){0};
    FieldElement aa = embedFieldElement(field, a);
    FieldElement bb = embedFieldElement(field, b);
    FieldElement product = multiplyFieldElements(aa, bb);
    freeFieldElement(&aa);
    freeFieldElement(&bb);
    return product;
}

// Divide two FieldElements
FieldElement elemDiv(FieldElement a, FieldElement b) {
    Field* field = promotedField(a.field, b.field);
    if (field == NULL) return (FieldElement){0};
    FieldElement aa = embedFieldElement(field, a);
    FieldElement bb = embedFieldElement(field, b);
    FieldElement quotient = divideFieldElements(aa, bb);
    freeFieldElement(&aa);
    freeFieldElement(&bb);
    return quotient;
}

// Negate a FieldElement
FieldElement elemNeg(FieldElement a) {
    return negateFieldElement(a);
}

// Return the complex conjugate (or the value itself for real)
FieldElement elemConj(FieldElement a) {
    if (!fieldElementIsValid(&a) || a.field == NULL) return (FieldElement){0};
    if (a.field->type == CC) return elemFromComplex(complexConj(a.value.z));
    return copyFieldElement(a);
}

// Return |a|, always a real number
long double elemAbs(FieldElement a) {
    ComplexNumber z = elemToComplex(a);
    if (isnan(z.real) || isnan(z.imag)) return NAN;
    return complexAbs(z);
}

// Tell if a FieldElement is within tol of zero
bool elemIsZero(FieldElement a, long double tol) {
    if (!fieldElementIsValid(&a)) return false;
    if (tol > 0.0L && fieldSupportsNumericProjection(a.field)) return elemAbs(a) <= tol;
    return fieldElementIsZero(a);
}

// Tell if a FieldElement carries NaN
bool elemIsNan(FieldElement a) {
    if (!fieldElementIsValid(&a)) return true;
    if (!fieldSupportsNumericProjection(a.field)) return false;
    ComplexNumber z = elemToComplex(a);
    return isnan(z.real) || isnan(z.imag);
}

// Tell if two FieldElements are equal up to some tolerance
bool elemEq(FieldElement a, FieldElement b, long double tol) {
    if (!fieldElementIsValid(&a) || !fieldElementIsValid(&b)) return false;
    if (tol <= 0.0L || !fieldSupportsNumericProjection(a.field)
            || !fieldSupportsNumericProjection(b.field)) {
        Field* field = promotedField(a.field, b.field);
        if (field == NULL) return false;
        FieldElement aa = embedFieldElement(field, a);
        FieldElement bb = embedFieldElement(field, b);
        bool equal = eqFieldElements(aa, bb);
        freeFieldElement(&aa);
        freeFieldElement(&bb);
        return equal;
    }
    FieldElement diff = elemSub(a, b);
    bool equal = elemAbs(diff) <= tol;
    freeFieldElement(&diff);
    return equal;
}

static void formatElem(FieldElement e, char* out, size_t outSize) {
    char* string = fieldElementToString(e);
    snprintf(out, outSize, "%s", string != NULL ? string : "<invalid>");
    free(string);
}

/* ---------- LU methods ----------- */

// Compute the determinant of an LU
FieldElement luDet(LU* lu) {
    FieldElement det;
    if (lu == NULL || !fieldSupportsMatrices(lu->field)) return elemFromReal(NAN);

    // Accumulate the diagonal product in the LU field
    det = fieldElementFromInt(lu->field, lu->sign);
    for (size_t i = 0; i < lu->n; i++) {
        FieldElement next = elemMul(det, lu->data[i*lu->n + i]);
        freeFieldElement(&det);
        det = next;
    }
    return det;
}

// Decompose a Matrix into the product of L and U, where
// L is lower triangular and U is upper triangular
LU* luDecompose(Matrix* matrix) {
    if (!isSquare(matrix)) return NULL;
    size_t n = matrix->numRows;

    LU* lu = calloc(1, sizeof(LU));
    if (!lu) return NULL;
    lu->field = matrix->field;
    lu->n = n;
    lu->sign = 1;
    lu->data = calloc(n * n, sizeof(FieldElement));
    lu->perm = malloc(n * sizeof(size_t));
    if (!lu->data || !lu->perm) { freeLU(lu); return NULL; }
    for (size_t i = 0; i < n * n; i++) {
        lu->data[i] = copyFieldElement(matrix->data[i]);
        if (!fieldElementIsValid(&lu->data[i])) { freeLU(lu); return NULL; }
    }

    // Initialize permutation: row i is originally at position i
    for (size_t i = 0; i < n; i++) lu->perm[i] = i;

    FieldElement* a = lu->data;  // shorthand for clarity

    for (size_t k = 0; k < n; k++) {
        // Choose a pivot using numeric magnitude when available, otherwise exact nonzero
        size_t pivot = k;
        bool foundPivot = false;
        if (fieldSupportsNumericProjection(matrix->field)) {
            long double maxVal = 1e-12;
            for (size_t i = k; i < n; i++) {
                long double v = elemAbs(a[i * n + k]);
                if (v > maxVal) {
                    maxVal = v;
                    pivot = i;
                    foundPivot = true;
                }
            }
        } else {
            for (size_t i = k; i < n; i++) {
                if (!fieldElementIsZero(a[i * n + k])) {
                    pivot = i;
                    foundPivot = true;
                    break;
                }
            }
        }

        // Singular (or close enough): no valid LU exists
        if (!foundPivot) { freeLU(lu); return NULL; }

        // Swap row k with pivot row, if needed
        if (pivot != k) {
            for (size_t j = 0; j < n; j++) {
                FieldElement tmp = a[k * n + j];
                a[k * n + j] = a[pivot * n + j];
                a[pivot * n + j] = tmp;
            }
            // Track the swap in the permutation array
            size_t tmpIdx = lu->perm[k];
            lu->perm[k] = lu->perm[pivot];
            lu->perm[pivot] = tmpIdx;
            lu->sign = -lu->sign;
        }

        // Eliminate below the pivot, storing the multipliers in L's position
        for (size_t i = k + 1; i < n; i++) {
            FieldElement factor = elemDiv(a[i * n + k], a[k * n + k]);
            if (!fieldElementIsValid(&factor)) { freeLU(lu); return NULL; }
            freeFieldElement(&a[i * n + k]);
            a[i * n + k] = factor;
            for (size_t j = k + 1; j < n; j++) {
                FieldElement product = elemMul(factor, a[k * n + j]);
                FieldElement diff = elemSub(a[i * n + j], product);
                freeFieldElement(&product);
                if (!fieldElementIsValid(&diff)) { freeLU(lu); return NULL; }
                freeFieldElement(&a[i * n + j]);
                a[i * n + j] = diff;
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
    if (!matrixIsValid(b)) return NULL;

    size_t n = lu->n;
    Matrix* x = constructMatrixOverField(lu->field, n, 1);
    if (!x) return NULL;

    // Apply the permutation to b, storing result in x
    for (size_t i = 0; i < n; i++) {
	setEntry(x, i, 0, b->data[lu->perm[i]]);
    }

    // Use forward-substitution for Ly = Pb
    for (size_t i = 0; i < n; i++) {
        FieldElement sum = getEntry(x, i, 0);
        for (size_t j = 0; j < i; j++) {
            FieldElement product = elemMul(lu->data[i*n + j], x->data[j]);
            FieldElement diff = elemSub(sum, product);
            freeFieldElement(&product);
            freeFieldElement(&sum);
            sum = diff;
        }
        setOwnedEntry(x, i, sum);
    }

    // Use backward-substitution for Ux = y
    for (size_t i = n; i-- > 0;) {
	FieldElement sum = getEntry(x, i, 0);
	for (size_t j = i+1; j < n; j++) {
	    FieldElement product = elemMul(lu->data[i*n + j], x->data[j]);
            FieldElement diff = elemSub(sum, product);
            freeFieldElement(&product);
            freeFieldElement(&sum);
            sum = diff;
	}
        FieldElement quotient = elemDiv(sum, lu->data[i*n+i]);
        freeFieldElement(&sum);
	setOwnedEntry(x, i, quotient);
    }

    return x;
}

// Compute the inverse of a Matrix with LU
Matrix* luInverse(LU* lu) {
    if (!lu) return NULL;
    size_t n = lu->n;

    // Get the column vectors of the identity matrix
    Matrix** cols = malloc(n * sizeof(Matrix*));
    if (!cols) return NULL;
    for (size_t i = 0; i < n; i++) {
	    cols[i] = constructMatrixOverField(lu->field, n, 1);
	    if (!cols[i]) {
	        for (size_t j = 0; j < i; j ++) freeMatrix(cols[j]);
	        free(cols);
	        return NULL;
	    }
            FieldElement one = elemOneForField(lu->field);
	    setEntry(cols[i], i, 0, one);
            freeFieldElement(&one);
    }

    // Construct a matrix of the inverse column vectors
    Matrix** soln = malloc(n * sizeof(Matrix*));
    if (!soln) {
	for (size_t i = 0; i < n; i++) freeMatrix(cols[i]);
	free(cols);
	return NULL;
    }
    for (size_t i = 0; i < lu->n; i++) {
	    soln[i] = luSolve(lu, cols[i]);
	    if (!soln[i]) {
	        for (size_t j = 0; j < i; j++) freeMatrix(soln[j]);
	        for (size_t j = 0; j < n; j++) freeMatrix(cols[j]);
	        free(soln);
	        free(cols);
	        return NULL;
	    }
    }

    // Construct the inverse matrix
    Matrix* inverted = constructMatrixOverField(lu->field, n, n);
    if (!inverted) {
	for (size_t i = 0; i < n; i++) {
	    freeMatrix(cols[i]);
	    freeMatrix(soln[i]);
    	}
        free(cols);
        free(soln);
	return NULL;
    }
    for (size_t i = 0; i < n; i++) {
	for (size_t j = 0; j < n; j++) {
            FieldElement entry = getEntry(soln[j], i, 0);
	    setEntry(inverted, i, j, entry);
            freeFieldElement(&entry);
	}
    }

    // Free memory
    for (size_t i = 0; i < n; i++) {
	freeMatrix(cols[i]);
	freeMatrix(soln[i]);
    }
    free(cols);
    free(soln);

    return inverted;
}

/* ----------- Main methods ---------- */

// Construct a Matrix object over a given Field
Matrix* constructMatrixOverField(Field* field, size_t numRows, size_t numCols) {
    size_t count;
    if (!fieldSupportsMatrices(field) || !matrixEntryCount(numRows, numCols, &count)) return NULL;

    Matrix* matrix = calloc(1, sizeof(Matrix));
    if (!matrix) return NULL;
    matrix->field = copyMatrixField(field);
    if (!matrix->field) {
        free(matrix);
        return NULL;
    }
    matrix->ownsField = true;
    matrix->numRows = numRows;
    matrix->numCols = numCols;

    matrix->data = calloc(count, sizeof(FieldElement));
    if (!matrix->data) {
	free(matrix);
	return NULL;
    }
    for (size_t i = 0; i < count; i++) {
        matrix->data[i] = elemZeroForField(matrix->field);
        if (!fieldElementIsValid(&matrix->data[i])) {
            freeMatrix(matrix);
            return NULL;
        }
    }
    matrix->cachedLU = NULL;

    return matrix;
}

// Construct a Matrix object over the default real field
Matrix* constructMatrix(size_t numRows, size_t numCols) {
    return constructMatrixOverField(sokkoRealField(), numRows, numCols);
}

// Construct a matrix over a given Field from a 2D data matrix
Matrix* constructMatrixFromMatrixOverField(Field* field, size_t numRows, size_t numCols, FieldElement** data, size_t lenData, size_t colLenData) {
    if (!fieldSupportsMatrices(field) || data == NULL || lenData != numRows || colLenData != numCols) return NULL;

    Matrix* matrix = constructMatrixOverField(field, numRows, numCols);
    if (!matrix) return NULL;
    for (size_t i = 0; i < numRows; i++) {
        if (data[i] == NULL) {
            freeMatrix(matrix);
            return NULL;
        }
	for (size_t j = 0; j < numCols; j++) {
	    setEntry(matrix, i, j, data[i][j]);
            if (!fieldElementIsValid(&matrix->data[matrixIndex(matrix, i, j)])) {
                freeMatrix(matrix);
                return NULL;
            }
	}
    }

    return matrix;
}

// Construct a matrix from a 2D data matrix
Matrix* constructMatrixFromMatrix(size_t numRows, size_t numCols, FieldElement** data, size_t lenData, size_t colLenData) {
    Field* field = NULL;
    if (data == NULL || lenData != numRows || colLenData != numCols) return NULL;

    // Infer the result field by scanning the rows
    for (size_t i = 0; i < lenData; i++) {
        Field* rowField;
        if (data[i] == NULL) return NULL;
        rowField = inferFieldFromArray(data[i], colLenData);
        if (rowField == NULL) return NULL;
        field = field == NULL ? rowField : promotedField(field, rowField);
        if (field == NULL) return NULL;
    }
    return constructMatrixFromMatrixOverField(field, numRows, numCols, data, lenData, colLenData);
}

// Construct a matrix over a given Field from a flattened data matrix
Matrix* constructMatrixFromArrayOverField(Field* field, size_t numRows, size_t numCols, FieldElement* data, size_t lenData) {
    size_t count;
    if (!fieldSupportsMatrices(field) || data == NULL
            || !matrixEntryCount(numRows, numCols, &count) || lenData != count) {
        return NULL;
    }

    Matrix* matrix = constructMatrixOverField(field, numRows, numCols);
    if (!matrix) return NULL;
    for (size_t i = 0; i < numRows; i++) {
	for (size_t j = 0; j < numCols; j++) {
	    setEntry(matrix, i, j, data[i*numCols + j]);
            if (!fieldElementIsValid(&matrix->data[matrixIndex(matrix, i, j)])) {
                freeMatrix(matrix);
                return NULL;
            }
	}
    }

    return matrix;
}

// Construct a matrix from a flattened data matrix
Matrix* constructMatrixFromArray(size_t numRows, size_t numCols, FieldElement* data, size_t lenData) {
    size_t count;
    Field* field;
    if (data == NULL || !matrixEntryCount(numRows, numCols, &count) || lenData != count) return NULL;

    field = inferFieldFromArray(data, lenData);
    if (field == NULL) return NULL;
    return constructMatrixFromArrayOverField(field, numRows, numCols, data, lenData);
}

// Print the Matrix
void printMatrix(Matrix* matrix) {
    if (!matrix) {
        printf("(null matrix)\n");
        return;
    }
    size_t cols = matrix->numCols;
    size_t rows = matrix->numRows;
    int* widths = calloc(cols, sizeof(int));
    if (!widths) {
        printf("(failed to allocate print widths)\n");
        return;
    }

    for (size_t j = 0; j < cols; j++) {
        int width = 1;
        for (size_t i = 0; i < rows; i++) {
            char buf[64];
            formatElem(matrix->data[i * cols + j], buf, sizeof(buf));
            int w = (int)strlen(buf);
            if (w > width) width = w;
        }
        widths[j] = width;
    }

    for (size_t i = 0; i < matrix->numRows; i++) {
	printf("| ");
	for (size_t j = 0; j < matrix->numCols; j++) {
	    char buf[64];
	    formatElem(matrix->data[i * matrix->numCols + j], buf, sizeof(buf));
	    printf("%*s", widths[j], buf);
	    if (j + 1 < matrix->numCols) printf(" , ");
	}
	printf(" |\n");
    }
    free(widths);
}

// Return a deep copy of a Matrix
Matrix* copyMatrix(Matrix* matrix) {
    if (!matrixIsValid(matrix)) return NULL;
    Matrix* newMatrix = constructMatrixOverField(matrix->field, matrix->numRows, matrix->numCols);
    if (!newMatrix) return NULL;

    for (size_t i = 0; i < matrix->numRows; i++) {
	for (size_t j = 0; j < matrix->numCols; j++) {
            FieldElement entry = getEntry(matrix, i, j);
	    setEntry(newMatrix, i, j, entry);
            freeFieldElement(&entry);
	}
    }

    newMatrix->cachedLU = NULL;
    return newMatrix;
}

// Tell if two matrices are equal up to some tolerance
bool matrixComp(Matrix* A, Matrix* B, long double tol) {
    if (!matrixIsValid(A) || !matrixIsValid(B)) return false;
    if (A->numRows != B->numRows || A->numCols != B->numCols) return false;

    for (size_t i = 0; i < A->numRows; i++) {
	for (size_t j = 0; j < A->numCols; j++) {
            FieldElement a = getEntry(A, i, j);
            FieldElement b = getEntry(B, i, j);
            bool equal = elemEq(a, b, tol);
            freeFieldElement(&a);
            freeFieldElement(&b);
	    if (!equal) return false;
	}
    }

    return true;
}

// Get the n x n identity matrix
Matrix* idMatrixOverField(Field* field, size_t n) {
    Matrix* id = constructMatrixOverField(field, n, n);
    if (!id) return NULL;

    FieldElement one = elemOneForField(field);
    for (size_t i = 0; i < n; i++) setEntry(id, i, i, one);
    freeFieldElement(&one);

    return id;
}

// Get the n x n identity matrix over the default real field
Matrix* idMatrix(size_t n) {
    return idMatrixOverField(sokkoRealField(), n);
}

// Free an array of Vectors
void freeVectorBasis(Vector** basis, size_t count) {
    if (basis == NULL) return;

    // Free each vector before releasing the basis array
    for (size_t i = 0; i < count; i++) freeVector(basis[i]);
    free(basis);
}

/* ---------- Basic operations ---------- */

// Compute the dot product of two arrays
FieldElement simpleDotProduct(FieldElement* v, FieldElement* w, size_t vlen, size_t wlen) {
    if (vlen != wlen) return elemFromReal(NAN);
    Field* field = inferFieldFromArray(v, vlen);
    if (field == NULL) return elemFromReal(NAN);

    for (size_t i = 0; i < wlen; i++) {
        field = promotedField(field, w[i].field);
        if (field == NULL) return elemFromReal(NAN);
    }

    FieldElement dp = elemZeroForField(field);
    for (size_t i = 0; i < vlen; i++) {
        FieldElement product = elemMul(v[i], w[i]);
        FieldElement sum = elemAdd(dp, product);
        freeFieldElement(&product);
        freeFieldElement(&dp);
        dp = sum;
    }
    return dp;
}

// Compute the dot product of two n x 1 matrices
FieldElement dotProduct(Matrix* v, Matrix* w) {
    Field* field;
    if (!matrixIsValid(v) || !matrixIsValid(w)) return elemFromReal(NAN);
    if (v->numCols != 1 || w->numCols != 1) return elemFromReal(NAN);
    if (v->numRows != w->numRows) return elemFromReal(NAN);
    field = promotedMatrixField(v, w);
    if (field == NULL) return elemFromReal(NAN);

    FieldElement dp = elemZeroForField(field);
    for (size_t i = 0; i < v->numRows; i++) {
        FieldElement lhs = getEntry(v, i, 0);
        FieldElement rhs = getEntry(w, i, 0);
        FieldElement product = elemMul(lhs, rhs);
        FieldElement sum = elemAdd(dp, product);
        freeFieldElement(&lhs);
        freeFieldElement(&rhs);
        freeFieldElement(&product);
        freeFieldElement(&dp);
        dp = sum;
    }
    return dp;
}

// Compute Av for a matrix A and a vector v
Matrix* applyMatrix(Matrix* A, Matrix* v) {
    Field* field;
    if (!matrixIsValid(A) || !matrixIsValid(v)) return NULL;
    if (A->numCols != v->numRows) return NULL;
    if (v->numCols != 1) return NULL;
    field = promotedMatrixField(A, v);
    if (field == NULL) return NULL;

    Matrix* toReturn = constructMatrixOverField(field, A->numRows, 1);
    if (!toReturn) return NULL;

    for (size_t i = 0; i < A->numRows; i++) {
        FieldElement entry = elemZeroForField(field);
        for (size_t j = 0; j < A->numCols; j++) {
            FieldElement a = getEntry(A, i, j);
            FieldElement b = getEntry(v, j, 0);
            FieldElement product = elemMul(a, b);
            FieldElement sum = elemAdd(entry, product);
            freeFieldElement(&a);
            freeFieldElement(&b);
            freeFieldElement(&product);
            freeFieldElement(&entry);
            entry = sum;
        }
        setEntry(toReturn, i, 0, entry);
        freeFieldElement(&entry);
    }

    return toReturn;
}

// Multiply a Matrix by a constant c
Matrix* multByConstant(Matrix* matrix, FieldElement c) {
    Field* field;
    if (!matrixIsValid(matrix) || !fieldElementIsValid(&c)) return NULL;
    field = promotedField(matrix->field, c.field);
    if (field == NULL) return NULL;
    Matrix* toReturn = constructMatrixOverField(field, matrix->numRows, matrix->numCols);
    if (!toReturn) return NULL;
    for (size_t i = 0; i < matrix->numRows * matrix->numCols; i++) {
        FieldElement product = elemMul(matrix->data[i], c);
	setOwnedEntry(toReturn, i, product);
    }

    return toReturn;
}

// Return matrix^n
Matrix* matrixPow(Matrix* matrix, int n) {
    if (!matrix) return NULL;
    if (!isSquare(matrix)) return NULL;

    if (n == 0) return idMatrixOverField(matrix->field, matrix->numRows);

    Matrix* base;
    if (n > 0) {
        base = copyMatrix(matrix);
    } else {
        base = invertMatrix(matrix);
        n = -n;
    }
    if (!base) return NULL;

    Matrix* result = idMatrixOverField(matrix->field, matrix->numRows);
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
    Field* field;
    if (!matrixIsValid(A) || !matrixIsValid(B)) return NULL;
    if (!(A->numRows == B->numRows && A->numCols == B->numCols)) return NULL;
    field = promotedMatrixField(A, B);
    if (field == NULL) return NULL;
    Matrix* C = constructMatrixOverField(field, A->numRows, A->numCols);
    if (!C) return NULL;

    for (size_t i = 0; i < A->numRows * A->numCols; i++) {
        FieldElement sum = elemAdd(A->data[i], B->data[i]);
	setOwnedEntry(C, i, embedFieldElement(field, sum));
        freeFieldElement(&sum);
    }

    return C;
}

// Subtract two Matrices
Matrix* subtractMatrices(Matrix* A, Matrix* B) {
    Field* field;
    if (!matrixIsValid(A) || !matrixIsValid(B)) return NULL;
    if (!(A->numRows == B->numRows && A->numCols == B->numCols)) return NULL;
    field = promotedMatrixField(A, B);
    if (field == NULL) return NULL;
    Matrix* C = constructMatrixOverField(field, A->numRows, A->numCols);
    if (!C) return NULL;

    for (size_t i = 0; i < A->numRows * A->numCols; i++) {
        FieldElement diff = elemSub(A->data[i], B->data[i]);
	setOwnedEntry(C, i, embedFieldElement(field, diff));
        freeFieldElement(&diff);
    }

    return C;
}

// Get the product of two matrices if they can be multiplied
Matrix* multiplyMatrices(Matrix* A, Matrix* B) {
    Field* field;
    if (!matrixIsValid(A) || !matrixIsValid(B)) return NULL;
    if (A->numCols != B->numRows) return NULL;
    field = promotedMatrixField(A, B);
    if (field == NULL) return NULL;
    Matrix* prod = constructMatrixOverField(field, A->numRows, B->numCols);
    if (!prod) return NULL;

    for (size_t i = 0; i < A->numRows; i++) {
	    for (size_t j = 0; j < B->numCols; j++) {
	        FieldElement s = elemZeroForField(field);
	        for (size_t k = 0; k < A->numCols; k++) {
                    FieldElement a = getEntry(A, i, k);
                    FieldElement b = getEntry(B, k, j);
                    FieldElement product = elemMul(a, b);
                    FieldElement sum = elemAdd(s, product);
                    freeFieldElement(&a);
                    freeFieldElement(&b);
                    freeFieldElement(&product);
                    freeFieldElement(&s);
		    s = sum;
	        }
	        setEntry(prod, i, j, s);
                freeFieldElement(&s);
	    }
    }

    return prod;
}

// Get the tensor product of two matrices
Matrix* tensorMatrices(Matrix* A, Matrix* B) {
    Field* field;
    if (!matrixIsValid(A) || !matrixIsValid(B)) return NULL;
    size_t m = A->numRows;
    size_t n = A->numCols;
    size_t p = B->numRows;
    size_t q = B->numCols;
    field = promotedMatrixField(A, B);
    if (field == NULL) return NULL;

    Matrix* C = constructMatrixOverField(field, m*p, n*q);
    if (!C) return NULL;

    for (size_t i = 0; i < m; i++) {
	for (size_t j = 0; j < n; j++) {
	    FieldElement aij = getEntry(A, i, j);
	    for (size_t r = 0; r < p; r++) {
		for (size_t s = 0; s < q; s++) {
		    FieldElement brs = getEntry(B, r, s);
                    FieldElement product = elemMul(aij, brs);
		    size_t I = i*p+r;
		    size_t J = j*q+s;
		    setEntry(C, I, J, product);
                    freeFieldElement(&product);
                    freeFieldElement(&brs);
		}
	    }
            freeFieldElement(&aij);
	}
    }

    return C;
}

// Get the transpose of a Matrix
Matrix* transpose(Matrix* matrix) {
    if (!matrixIsValid(matrix)) return NULL;
    Matrix* trans = constructMatrixOverField(matrix->field, matrix->numCols, matrix->numRows);
    if (!trans) return NULL;

    for (size_t i = 0; i < matrix->numRows; i++) {
	    for (size_t j = 0; j < matrix->numCols; j++) {
                FieldElement entry = getEntry(matrix, i, j);
	        setEntry(trans, j, i, entry);
                freeFieldElement(&entry);
	    }
    }

    return trans;
}

// Get the conjugate transpose of a Matrix
Matrix* adjoint(Matrix* matrix) {
    if (!matrixIsValid(matrix)) return NULL;

    Matrix* adj = constructMatrixOverField(matrix->field, matrix->numCols, matrix->numRows);
    if (!adj) return NULL;

    for (size_t i = 0; i < matrix->numRows; i++) {
        for (size_t j = 0; j < matrix->numCols; j++) {
            FieldElement entry = getEntry(matrix, i, j);
            FieldElement conj = elemConj(entry);
            setEntry(adj, j, i, conj);
            freeFieldElement(&entry);
            freeFieldElement(&conj);
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
Vector* constructVector(size_t dim) {
    return constructVectorOverField(sokkoRealField(), dim);
}

// Construct a Vector over a given Field
Vector* constructVectorOverField(Field* field, size_t dim) {
    if (dim < 1) return NULL;
    return constructMatrixOverField(field, dim, 1);
}

// Construct a Vector of positive dim from an array
Vector* constructVectorFromArray(size_t dim, FieldElement *data, size_t dataLen) {
    if (dim < 1) return NULL;
    if (!data) return NULL;
    if (dim != dataLen) return NULL;

    Vector* vec = constructMatrixFromArray(dim, 1, data, dataLen);
    if (!vec) return NULL;
    return vec;
}

// Construct a Vector over a given Field from an array
Vector* constructVectorFromArrayOverField(Field* field, size_t dim, FieldElement* data, size_t dataLen) {
    if (dim < 1) return NULL;
    if (!data) return NULL;
    if (dim != dataLen) return NULL;
    return constructMatrixFromArrayOverField(field, dim, 1, data, dataLen);
}

// Construct a Vector of dim 2 given two inputs
Vector* constructVector2(FieldElement x, FieldElement y) {
    if (elemIsNan(x) || elemIsNan(y)) return NULL;
    FieldElement data[] = {x, y};
    return constructVectorFromArray(2, data, 2);
}

// Construct a Vector of dim 3 given three inputs
Vector* constructVector3(FieldElement x, FieldElement y, FieldElement z) {
    if (elemIsNan(x) || elemIsNan(y) || elemIsNan(z)) return NULL;
    FieldElement data[] = {x, y, z};
    return constructVectorFromArray(3, data, 3);
}

// Adds two Vectors of the same dimension
Vector* addVectors(Vector* v1, Vector* v2) {
    Field* field;
    if (!matrixIsValid(v1) || !matrixIsValid(v2)) return NULL;
    if (v1->numRows != v2->numRows || v1->numCols != v2->numCols || v1->numCols != 1) return NULL;
    field = promotedMatrixField(v1, v2);
    if (field == NULL) return NULL;

    size_t dim = v1->numRows;
    Vector* sum = constructVectorOverField(field, dim);
    if (!sum) return NULL;

    for (size_t i = 0; i < dim; i++) {
        FieldElement a = getEntry(v1, i, 0);
        FieldElement b = getEntry(v2, i, 0);
        FieldElement value = elemAdd(a, b);
        setEntry(sum, i, 0, value);
        freeFieldElement(&a);
        freeFieldElement(&b);
        freeFieldElement(&value);
    }
    return sum;
}

// Returns the dot product of two Vectors
FieldElement vectorDotProduct(Vector* v1, Vector* v2) {
    Field* field;
    if (!matrixIsValid(v1) || !matrixIsValid(v2)) return elemFromReal(NAN);
    if (v1->numRows != v2->numRows || v1->numCols != v2->numCols || v1->numCols != 1) return elemFromReal(NAN);
    field = promotedMatrixField(v1, v2);
    if (field == NULL) return elemFromReal(NAN);

    FieldElement dot = elemZeroForField(field);
    size_t dim = v1->numRows;
    for (size_t i = 0; i < dim; i++) {
        FieldElement a = getEntry(v1, i, 0);
        FieldElement b = getEntry(v2, i, 0);
        FieldElement product = elemMul(a, b);
        FieldElement sum = elemAdd(dot, product);
        freeFieldElement(&a);
        freeFieldElement(&b);
        freeFieldElement(&product);
        freeFieldElement(&dot);
        dot = sum;
    }

    return dot;
}

// Returns the Hermitian dot product <v1, v2> = sum conj(v1_i) * v2_i
FieldElement hermitianDotProduct(Vector* v1, Vector* v2) {
    Field* field;
    if (!matrixIsValid(v1) || !matrixIsValid(v2)) return elemFromReal(NAN);
    if (v1->numRows != v2->numRows || v1->numCols != v2->numCols || v1->numCols != 1) return elemFromReal(NAN);
    field = promotedMatrixField(v1, v2);
    if (field == NULL) return elemFromReal(NAN);

    FieldElement dot = elemZeroForField(field);
    size_t dim = v1->numRows;
    for (size_t i = 0; i < dim; i++) {
        FieldElement entry = getEntry(v1, i, 0);
        FieldElement lhs = elemConj(entry);
        FieldElement rhs = getEntry(v2, i, 0);
        FieldElement product = elemMul(lhs, rhs);
        FieldElement sum = elemAdd(dot, product);
        freeFieldElement(&entry);
        freeFieldElement(&lhs);
        freeFieldElement(&rhs);
        freeFieldElement(&product);
        freeFieldElement(&dot);
        dot = sum;
    }

    return dot;
}

// Returns the cross product of two 3D Vectors
Vector* crossProduct(Vector* v1, Vector* v2) {
    Field* field;
    if (!matrixIsValid(v1) || !matrixIsValid(v2)) return NULL;
    if (v1->numRows != v2->numRows || v1->numCols != v2->numCols || v1->numCols != 1) return NULL;
    if (v1->numRows != 3) return NULL;
    field = promotedMatrixField(v1, v2);
    if (field == NULL) return NULL;

    Vector* crossProd = constructVectorOverField(field, 3);
    if (!crossProd) return NULL;

    FieldElement v10 = getEntry(v1, 0, 0), v11 = getEntry(v1, 1, 0), v12 = getEntry(v1, 2, 0);
    FieldElement v20 = getEntry(v2, 0, 0), v21 = getEntry(v2, 1, 0), v22 = getEntry(v2, 2, 0);

    setEntry(crossProd, 0, 0, elemSub(elemMul(v11, v22), elemMul(v12, v21)));
    setEntry(crossProd, 1, 0, elemSub(elemMul(v12, v20), elemMul(v10, v22)));
    setEntry(crossProd, 2, 0, elemSub(elemMul(v10, v21), elemMul(v11, v20)));

    return crossProd;
}

// Returns the L2 norm of a Vector (sqrt of sum of |v_i|^2, always real)
long double l2Norm(Vector* vect) {
    if (!matrixSupportsNumericProjection(vect) || vect->numCols != 1) return NAN;

    long double norm = 0;
    for (size_t i = 0; i < vect->numRows; i++) {
        FieldElement entry = getEntry(vect, i, 0);
        long double m = elemAbs(entry);
        freeFieldElement(&entry);
        norm += m * m;
    }

    return sqrtl(norm);
}

// Return the opposite of a vector
Vector* negativeVector(Vector* vect) {
    if (!matrixIsValid(vect)) return NULL;

    size_t dim = vect->numRows;
    Vector* neg = constructVectorOverField(vect->field, dim);
    if (!neg) return NULL;
    for (size_t i = 0; i < dim; i++) {
        FieldElement entry = getEntry(vect, i, 0);
        FieldElement value = elemNeg(entry);
        setEntry(neg, i, 0, value);
        freeFieldElement(&entry);
        freeFieldElement(&value);
    }

    return neg;
}

// Scale a Vector by a constant k
Vector* scaleVector(Vector* vect, FieldElement k) {
    Field* field;
    if (!matrixIsValid(vect) || elemIsNan(k)) return NULL;
    field = promotedField(vect->field, k.field);
    if (field == NULL) return NULL;

    size_t dim = vect->numRows;
    Vector* scaled = constructVectorOverField(field, dim);
    if (!scaled) return NULL;
    for (size_t i = 0; i < dim; i++) {
        FieldElement entry = getEntry(vect, i, 0);
        FieldElement value = elemMul(entry, k);
        setEntry(scaled, i, 0, value);
        freeFieldElement(&entry);
        freeFieldElement(&value);
    }

    return scaled;
}

// Compute v1 - v2
Vector* subtractVectors(Vector* v1, Vector* v2) {
    Field* field;
    if (!matrixIsValid(v1) || !matrixIsValid(v2)) return NULL;
	if (v1->numRows != v2->numRows || v1->numCols != v2->numCols || v1->numCols != 1) return NULL;
    field = promotedMatrixField(v1, v2);
    if (field == NULL) return NULL;

	size_t dim = v1->numRows;
	Vector* diff = constructVectorOverField(field, dim);
	if (!diff) return NULL;

	for (size_t i = 0; i < dim; i++) {
        FieldElement a = getEntry(v1, i, 0);
        FieldElement b = getEntry(v2, i, 0);
        FieldElement value = elemSub(a, b);
		setEntry(diff, i, 0, value);
        freeFieldElement(&a);
        freeFieldElement(&b);
        freeFieldElement(&value);
	}
	return diff;
}

// Normalize a Vector v
Vector* normalizeVector(Vector* vect) {
	if (!vect) return NULL;

	long double norm = l2Norm(vect);
	if (!isfinite(norm) || norm == 0) return NULL;

    FieldElement scale = elemFromReal(1.0 / norm);
    Vector* normalized = scaleVector(vect, scale);
    freeFieldElement(&scale);
	return normalized;
}

// Compute the distance between two Vectors
long double vectorDistance(Vector* v1, Vector* v2) {
	if (!matrixSupportsNumericProjection(v1) || !matrixSupportsNumericProjection(v2)) return NAN;
	if (v1->numRows != v2->numRows || v1->numCols != v2->numCols || v1->numCols != 1) return NAN;

	Vector* diff = subtractVectors(v1, v2);
	if (!diff) return NAN;

	long double dist = l2Norm(diff);
	freeMatrix(diff);
	return dist;
}

// Compute the angle between two Vectors in radians.
// Uses Re(<v1, v2>) / (||v1|| ||v2||); returns NAN if either vector has
// a strictly complex dot product component that prevents a real angle.
long double vectorAngle(Vector* v1, Vector* v2) {
	if (!matrixSupportsNumericProjection(v1) || !matrixSupportsNumericProjection(v2)) return NAN;
	if (v1->numRows != v2->numRows || v1->numCols != v2->numCols || v1->numCols != 1) return NAN;

	FieldElement dot = vectorDotProduct(v1, v2);
	if (elemIsNan(dot)) return NAN;
	long double normV1 = l2Norm(v1);
	long double normV2 = l2Norm(v2);
	if (normV1 == 0 || normV2 == 0) {
        freeFieldElement(&dot);
        return NAN;
    }

    ComplexNumber dotValue = elemToComplex(dot);
	long double reDot = dotValue.real;
	long double cosTheta = reDot / (normV1 * normV2);
	if (cosTheta > 1) cosTheta = 1;
	if (cosTheta < -1) cosTheta = -1;
    freeFieldElement(&dot);

	return acosl(cosTheta);
}

// Compute the projection of v1 onto v2
Vector* vectorProjectOnto(Vector* v1, Vector* v2) {
	if (!matrixSupportsNumericProjection(v1) || !matrixSupportsNumericProjection(v2)) return NULL;
	if (v1->numRows != v2->numRows || v1->numCols != v2->numCols || v1->numCols != 1) return NULL;

	FieldElement dot = vectorDotProduct(v1, v2);
	long double normV2 = l2Norm(v2);
	if (normV2 == 0) {
        freeFieldElement(&dot);
        return NULL;
    }

    FieldElement denom = elemFromReal(normV2 * normV2);
	FieldElement scale = elemDiv(dot, denom);
	Vector* projection = scaleVector(v2, scale);
    freeFieldElement(&dot);
    freeFieldElement(&denom);
    freeFieldElement(&scale);
	return projection;
}

// Set a Matrix column from a Vector
bool setMatrixColumn(Matrix* matrix, size_t column, Vector* vector) {
    if (!matrixIsValid(matrix) || !matrixIsValid(vector) || vector->numCols != 1
            || matrix->numRows != vector->numRows || column >= matrix->numCols) {
        return false;
    }

    // Copy each vector entry into the requested matrix column
    for (size_t i = 0; i < matrix->numRows; i++) {
        FieldElement entry = getEntry(vector, i, 0);
        setEntry(matrix, i, column, entry);
        freeFieldElement(&entry);
    }
    resetMatrixCache(matrix);
    return true;
}

// Set a Matrix row block from another Matrix
bool setMatrixRowBlock(Matrix* target, size_t startRow, Matrix* block) {
    if (!matrixIsValid(target) || !matrixIsValid(block)
            || startRow > target->numRows || block->numRows > target->numRows - startRow
            || target->numCols != block->numCols) {
        return false;
    }

    // Copy the block into consecutive rows of the target matrix
    for (size_t i = 0; i < block->numRows; i++) {
        for (size_t j = 0; j < block->numCols; j++) {
            FieldElement entry = getEntry(block, i, j);
            setEntry(target, startRow + i, j, entry);
            freeFieldElement(&entry);
        }
    }
    resetMatrixCache(target);
    return true;
}

// Return a Matrix whose columns are the given Vectors
Matrix* matrixFromColumns(Vector** columns, size_t count) {
    if (columns == NULL || count == 0 || !matrixIsValid(columns[0])
            || columns[0]->numCols != 1) {
        return NULL;
    }

    // Infer a compatible field across every supplied vector
    Field* field = columns[0]->field;
    for (size_t j = 1; j < count; j++) {
        if (!matrixIsValid(columns[j]) || columns[j]->numCols != 1
                || columns[j]->numRows != columns[0]->numRows) {
            return NULL;
        }
        field = promotedField(field, columns[j]->field);
        if (field == NULL) return NULL;
    }

    // Allocate the matrix and copy each vector into its column
    Matrix* matrix = constructMatrixOverField(field, columns[0]->numRows, count);
    if (matrix == NULL) return NULL;
    for (size_t j = 0; j < count; j++) {
        if (!setMatrixColumn(matrix, j, columns[j])) {
            freeMatrix(matrix);
            return NULL;
        }
    }
    return matrix;
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
    Matrix* id = idMatrixOverField(matrix->field, matrix->numRows);
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
    Matrix* id = idMatrixOverField(matrix->field, matrix->numRows);
    if (!id) {
        freeMatrix(adj);
        freeMatrix(prod);
        return false;
    }

    long double tol = 1e-12 * fmaxl(1.0, frobeniusNorm(matrix));
    bool unitary = matrixComp(prod, id, tol);

    freeMatrix(adj);
    freeMatrix(prod);
    freeMatrix(id);
    return unitary;
}

// Return the rank of a Matrix
size_t rank(Matrix* matrix) {
    size_t rnk = 0;
    Matrix* reduced;
    if (!matrixIsValid(matrix)) return (size_t)-1;

    // Count the nonzero rows in the reduced row echelon form
    reduced = reduceRows(matrix);
    if (reduced == NULL) return (size_t)-1;
    for (size_t i = 0; i < reduced->numRows; i++) {
        if (!matrixRowIsZero(reduced, i)) rnk++;
    }
    freeMatrix(reduced);
    return rnk;
}

// Return the nullity of a Matrix
size_t nullity(Matrix* matrix) {
    size_t rnk;
    if (!matrixIsValid(matrix)) return (size_t)-1;

    // Compute nullity from the rank-nullity formula
    rnk = rank(matrix);
    if (rnk == (size_t)-1) return (size_t)-1;
    return matrix->numCols - rnk;
}

// Return the trace of a Matrix
FieldElement trace(Matrix* matrix) {
    if (!matrixIsValid(matrix)) return elemFromReal(NAN);
    if (!isSquare(matrix)) return elemFromReal(NAN);

    FieldElement trc = elemZeroForField(matrix->field);
    for (size_t i = 0; i < matrix->numCols; i++) {
        FieldElement entry = getEntry(matrix, i, i);
        FieldElement sum = elemAdd(trc, entry);
        freeFieldElement(&entry);
        freeFieldElement(&trc);
        trc = sum;
    }

    return trc;
}

// Return the Frobenius norm of a matrix (always real)
long double frobeniusNorm(Matrix* matrix) {
    if (!matrixSupportsNumericProjection(matrix)) return NAN;

    long double sum = 0.0;
    for (size_t i = 0; i < matrix->numRows * matrix->numCols; i++) {
        long double m = elemAbs(matrix->data[i]);
        sum += m * m;
    }
    return sqrtl(sum);
}

// Compute the determinant of a Matrix
FieldElement determinant(Matrix* matrix) {
    if (!isSquare(matrix)) return elemFromReal(NAN);
    size_t n = matrix->numRows;

    // Hardcoded fast paths for tiny matrices to avoid LU overhead
    if (n == 1) return copyFieldElement(matrix->data[0]);
    if (n == 2) {
        FieldElement ad = elemMul(matrix->data[0], matrix->data[3]);
        FieldElement bc = elemMul(matrix->data[1], matrix->data[2]);
        FieldElement det = elemSub(ad, bc);
        freeFieldElement(&ad);
        freeFieldElement(&bc);
        return det;
    }

    cacheLU(matrix);
    if (!matrix->cachedLU) return elemZeroForField(matrix->field);
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
    if (!matrixIsValid(matrix)) return NULL;
    size_t m = matrix->numRows;
    size_t n = matrix->numCols;
    Matrix* R = copyMatrix(matrix);
    if (!R) return NULL;

    size_t row = 0;
    for (size_t col = 0; col < n && row < m; col++) {
        bool foundPivot = false;
        size_t pivot = row;
        if (fieldSupportsNumericProjection(R->field)) {
            long double maxVal = 1e-12;

            // Partial pivot numerically when the field has a numeric projection
            for (size_t i = row; i < m; i++) {
                FieldElement entry = getEntry(R, i, col);
                long double v = elemAbs(entry);
                freeFieldElement(&entry);
                if (v > maxVal) {
                    maxVal = v;
                    pivot = i;
                    foundPivot = true;
                }
            }
        } else {
            // Pivot by exact nonzero over finite fields and symbolic extensions
            for (size_t i = row; i < m; i++) {
                FieldElement entry = getEntry(R, i, col);
                bool nonzero = !fieldElementIsZero(entry);
                freeFieldElement(&entry);
                if (nonzero) {
                    pivot = i;
                    foundPivot = true;
                    break;
                }
            }
        }
        if (!foundPivot) continue;

        // Swap the pivot row into place
        if (pivot != row) {
            for (size_t j = 0; j < n; j++) {
                FieldElement upper = getEntry(R, row, j);
                FieldElement lower = getEntry(R, pivot, j);
                setEntry(R, row, j, lower);
                setEntry(R, pivot, j, upper);
                freeFieldElement(&upper);
                freeFieldElement(&lower);
            }
        }

        // Scale pivot row so the pivot is 1
        FieldElement pv = getEntry(R, row, col);
        for (size_t j = col; j < n; j++) {
            FieldElement entry = getEntry(R, row, j);
            FieldElement quotient = elemDiv(entry, pv);
            setEntry(R, row, j, quotient);
            freeFieldElement(&entry);
            freeFieldElement(&quotient);
        }

        // Force exact 1 to avoid rounding drift on the pivot
        FieldElement one = elemOneForField(R->field);
        setEntry(R, row, col, one);
        freeFieldElement(&one);

        // Eliminate every other row's entry in this column
        for (size_t i = 0; i < m; i++) {
            if (i == row) continue;
            FieldElement factor = getEntry(R, i, col);
            if (elemIsZero(factor, 1e-12)) {
                freeFieldElement(&factor);
                continue;
            }
            for (size_t j = col; j < n; j++) {
                FieldElement target = getEntry(R, i, j);
                FieldElement source = getEntry(R, row, j);
                FieldElement product = elemMul(factor, source);
                FieldElement value = elemSub(target, product);
                setEntry(R, i, j, value);
                freeFieldElement(&target);
                freeFieldElement(&source);
                freeFieldElement(&product);
                freeFieldElement(&value);
            }
            FieldElement zero = elemZeroForField(R->field);
            setEntry(R, i, col, zero);
            freeFieldElement(&zero);
            freeFieldElement(&factor);
        }

        freeFieldElement(&pv);
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

// Return a basis for the nullspace of a Matrix
Vector** nullSpace(Matrix* matrix, size_t* count) {
    Matrix* reduced;
    bool* pivotCols;
    size_t* pivotForRow;
    size_t pivotRows = 0;
    size_t freeCount = 0;
    Vector** basis;
    size_t basisIndex = 0;
    if (count != NULL) *count = 0;
    if (!matrixIsValid(matrix) || count == NULL) return NULL;

    // Reduce the matrix and allocate pivot bookkeeping for the kernel basis
    reduced = reduceRows(matrix);
    if (reduced == NULL) return NULL;
    pivotCols = calloc(reduced->numCols, sizeof(bool));
    pivotForRow = malloc(reduced->numRows * sizeof(size_t));
    if (pivotCols == NULL || pivotForRow == NULL) {
        free(pivotCols);
        free(pivotForRow);
        freeMatrix(reduced);
        return NULL;
    }

    // Record the pivot column, if any, for each nonzero row
    for (size_t i = 0; i < reduced->numRows; i++) {
        pivotForRow[i] = reduced->numCols;
        for (size_t j = 0; j < reduced->numCols; j++) {
            if (!elemIsZero(reduced->data[matrixIndex(reduced, i, j)], 1e-12)) {
                pivotForRow[pivotRows++] = j;
                pivotCols[j] = true;
                break;
            }
        }
    }

    // Count the free variables, one basis vector per free column
    for (size_t j = 0; j < reduced->numCols; j++) {
        if (!pivotCols[j]) freeCount++;
    }
    if (freeCount == 0) {
        free(pivotCols);
        free(pivotForRow);
        freeMatrix(reduced);
        return NULL;
    }

    // Allocate one nullspace basis vector for each free column
    basis = calloc(freeCount, sizeof(Vector*));
    if (basis == NULL) {
        free(pivotCols);
        free(pivotForRow);
        freeMatrix(reduced);
        return NULL;
    }

    // Build each nullspace vector from one chosen free variable
    for (size_t freeCol = 0; freeCol < reduced->numCols; freeCol++) {
        if (pivotCols[freeCol]) continue;

        // Allocate the current basis vector and set its free coordinate
        basis[basisIndex] = constructVectorOverField(reduced->field, reduced->numCols);
        if (basis[basisIndex] == NULL) {
            freeVectorBasis(basis, basisIndex);
            free(pivotCols);
            free(pivotForRow);
            freeMatrix(reduced);
            return NULL;
        }

        // Set the chosen free variable to one
        FieldElement one = elemOneForField(reduced->field);
        setEntry(basis[basisIndex], freeCol, 0, one);
        freeFieldElement(&one);

        // Solve each pivot variable in terms of the selected free variable
        for (size_t row = 0; row < pivotRows; row++) {
            FieldElement coeff = getEntry(reduced, row, freeCol);
            FieldElement negCoeff = elemNeg(coeff);
            setEntry(basis[basisIndex], pivotForRow[row], 0, negCoeff);
            freeFieldElement(&coeff);
            freeFieldElement(&negCoeff);
        }
        basisIndex++;
    }

    // Release elimination scratch data and return the completed basis
    free(pivotCols);
    free(pivotForRow);
    freeMatrix(reduced);
    *count = freeCount;
    return basis;
}

// Return a basis of the column space of the matrix as a heap-allocated array of column
// Vectors. Sets *count to the number of basis vectors (the rank). Caller frees each
// Vector via freeVector and the array via free.
Vector** columnSpace(Matrix* matrix, size_t* count) {
    if (!matrixIsValid(matrix) || !count) return NULL;
    size_t m = matrix->numRows;
    size_t n = matrix->numCols;
    *count = 0;

    Matrix* R = reduceRows(matrix);
    if (!R) return NULL;

    const long double tol = 1e-12;

    // Identify pivot columns of R: row r's pivot is the leftmost column with a 1 in that row.
    size_t* pivotCols = malloc(n * sizeof(size_t));
    if (!pivotCols) { freeMatrix(R); return NULL; }
    size_t numPivots = 0;
    size_t row = 0;
    for (size_t col = 0; col < n && row < m; col++) {
        FieldElement v = getEntry(R, row, col);
        bool nonzero = !elemIsZero(v, tol);
        freeFieldElement(&v);
        if (nonzero) {
            pivotCols[numPivots++] = col;
            row++;
        }
    }
    freeMatrix(R);

    if (numPivots == 0) { free(pivotCols); return NULL; }

    Vector** basis = malloc(numPivots * sizeof(Vector*));
    if (!basis) { free(pivotCols); return NULL; }

    for (size_t k = 0; k < numPivots; k++) {
        basis[k] = constructVectorOverField(matrix->field, m);
        if (!basis[k]) {
            for (size_t j = 0; j < k; j++) freeVector(basis[j]);
            free(basis);
            free(pivotCols);
            return NULL;
        }
        size_t c = pivotCols[k];
        for (size_t i = 0; i < m; i++) {
            FieldElement entry = getEntry(matrix, i, c);
            setEntry(basis[k], i, 0, entry);
            freeFieldElement(&entry);
        }
    }
    free(pivotCols);

    *count = numPivots;
    return basis;
}

// Return a basis of the row space of the matrix as column Vectors of length numCols.
// Sets *count to the number of basis vectors (the rank). The vectors are the non-zero
// rows of the RREF, transposed into column form. Caller frees each Vector and the array.
Vector** rowSpace(Matrix* matrix, size_t* count) {
    if (!matrixIsValid(matrix) || !count) return NULL;
    size_t m = matrix->numRows;
    size_t n = matrix->numCols;
    *count = 0;

    Matrix* R = reduceRows(matrix);
    if (!R) return NULL;

    const long double tol = 1e-12;
    size_t numPivots = 0;
    bool* nonzero = calloc(m, sizeof(bool));
    if (!nonzero) { freeMatrix(R); return NULL; }
    for (size_t i = 0; i < m; i++) {
        for (size_t j = 0; j < n; j++) {
            FieldElement entry = getEntry(R, i, j);
            bool hasEntry = !elemIsZero(entry, tol);
            freeFieldElement(&entry);
            if (hasEntry) {
                nonzero[i] = true;
                numPivots++;
                break;
            }
        }
    }

    if (numPivots == 0) { freeMatrix(R); free(nonzero); return NULL; }

    Vector** basis = malloc(numPivots * sizeof(Vector*));
    if (!basis) { freeMatrix(R); free(nonzero); return NULL; }

    size_t k = 0;
    for (size_t i = 0; i < m; i++) {
        if (!nonzero[i]) continue;
        basis[k] = constructVectorOverField(R->field, n);
        if (!basis[k]) {
            for (size_t j = 0; j < k; j++) freeVector(basis[j]);
            free(basis);
            freeMatrix(R);
            free(nonzero);
            return NULL;
        }
        for (size_t j = 0; j < n; j++) {
            FieldElement entry = getEntry(R, i, j);
            setEntry(basis[k], j, 0, entry);
            freeFieldElement(&entry);
        }
        k++;
    }
    free(nonzero);
    freeMatrix(R);

    *count = numPivots;
    return basis;
}

// Get the eigenvalues of a 2 x 2 matrix.
// Returns complex eigenvalues when the discriminant is negative or when
// input entries are complex; pure-real eigenvalues collapse to real FieldElements.
FieldElement* eigenvalues2x2(Matrix* matrix) {
    if (!matrixSupportsNumericProjection(matrix)) return NULL;
    if (matrix->numRows != 2 || matrix->numCols != 2) return NULL;

    FieldElement a = getEntry(matrix, 0, 0);
    FieldElement b = getEntry(matrix, 0, 1);
    FieldElement c = getEntry(matrix, 1, 0);
    FieldElement d = getEntry(matrix, 1, 1);

    FieldElement trc = elemAdd(a, d);
    FieldElement det = elemSub(elemMul(a, d), elemMul(b, c));
    FieldElement four = elemFromReal(4.0);
    FieldElement discriminant = elemSub(elemMul(trc, trc), elemMul(four, det));

    // Take the principal square root in the complex plane so negative
    // discriminants produce purely imaginary sqrt terms.
    ComplexNumber sqrtDisc = complexSqrt(elemToComplex(discriminant));
    FieldElement sqrtDiscElem = elemFromComplex(sqrtDisc);

    FieldElement* eigs = malloc(2 * sizeof(FieldElement));
    if (!eigs) return NULL;

    FieldElement two = elemFromReal(2.0);
    eigs[0] = elemDiv(elemAdd(trc, sqrtDiscElem), two);
    eigs[1] = elemDiv(elemSub(trc, sqrtDiscElem), two);

    return eigs;
}

// Get the eigenvalues of a 3x3 matrix using the characteristic polynomial.
// All three roots (including complex conjugate pairs) are returned as FieldElements.
FieldElement* eigenvalues3x3(Matrix* matrix) {
    if (!matrixSupportsNumericProjection(matrix)) return NULL;
    if (matrix->numRows != 3 || matrix->numCols != 3) return NULL;

    FieldElement a = getEntry(matrix, 0, 0);
    FieldElement b = getEntry(matrix, 0, 1);
    FieldElement c = getEntry(matrix, 0, 2);
    FieldElement d = getEntry(matrix, 1, 0);
    FieldElement e = getEntry(matrix, 1, 1);
    FieldElement f = getEntry(matrix, 1, 2);
    FieldElement g = getEntry(matrix, 2, 0);
    FieldElement h = getEntry(matrix, 2, 1);
    FieldElement i = getEntry(matrix, 2, 2);

    // Characteristic polynomial: lambda^3 + p*lambda^2 + q*lambda + r = 0
    FieldElement p = elemNeg(elemAdd(elemAdd(a, e), i));
    // q = ae + ai + ei - bd - cg - fh
    FieldElement q = elemSub(
        elemSub(
            elemSub(
                elemAdd(elemAdd(elemMul(a, e), elemMul(a, i)), elemMul(e, i)),
                elemMul(b, d)),
            elemMul(c, g)),
        elemMul(f, h));
    // r = -(aei + bfg + cdh - ceg - bdi - afh)
    FieldElement r = elemNeg(
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

    FieldElement* eigs = malloc(3 * sizeof(FieldElement));
    if (!eigs) return NULL;

    // Cardano in complex arithmetic:
    // u^3 = -Q/2 + sqrtl(Q^2/4 + P^3/27), v = -P/(3u) when u != 0.
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
        // -Q/2 + sqrtl(Q^2/4 + P^3/27) = 0 implies the root is cbrtl(-Q).
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
    ComplexNumber omega = { -0.5,  sqrtl(3.0) / 2.0 };
    ComplexNumber omega2 = { -0.5, -sqrtl(3.0) / 2.0 };

    ComplexNumber t1 = complexAdd(u, v);
    ComplexNumber t2 = complexAdd(complexMul(omega, u), complexMul(omega2, v));
    ComplexNumber t3 = complexAdd(complexMul(omega2, u), complexMul(omega, v));

    ComplexNumber pOver3 = complexDiv(elemToComplex(p), three);
    ComplexNumber lam1 = complexSub(t1, pOver3);
    ComplexNumber lam2 = complexSub(t2, pOver3);
    ComplexNumber lam3 = complexSub(t3, pOver3);

    // Cardano introduces tiny imaginary residuals when all roots of a real
    // polynomial are real; collapse them so real inputs yield real outputs.
    bool inputsReal = !elemIsComplex(a) && !elemIsComplex(b) && !elemIsComplex(c) &&
                      !elemIsComplex(d) && !elemIsComplex(e) && !elemIsComplex(f) &&
                      !elemIsComplex(g) && !elemIsComplex(h) && !elemIsComplex(i);
    if (inputsReal) {
        long double scale = fmaxl(fmaxl(complexAbs(lam1), complexAbs(lam2)), complexAbs(lam3));
        long double tol = 1e-9 * scale + 1e-12;
        if (fabsl(lam1.imag) < tol) lam1.imag = 0.0;
        if (fabsl(lam2.imag) < tol) lam2.imag = 0.0;
        if (fabsl(lam3.imag) < tol) lam3.imag = 0.0;
    }

    eigs[0] = elemFromComplex(lam1);
    eigs[1] = elemFromComplex(lam2);
    eigs[2] = elemFromComplex(lam3);

    return eigs;
}

// Return the eigenvectors of a 2x2 Matrix
Matrix** eigenvectors2x2(Matrix* matrix) {
    if (!matrixSupportsNumericProjection(matrix)) return NULL;
    if (!isSquare(matrix)) return NULL;
    if (!(matrix->numRows == 2)) return NULL;

    // Get eigenvales and allocate memory
    FieldElement* eigs = eigenvalues2x2(matrix);
    if (!eigs) return NULL;
    Matrix** evects = malloc(2 * sizeof(Matrix*));
    if (!evects) {
        free(eigs);
        return NULL;
    }
    for (size_t i = 0; i < 2; i++) {
        evects[i] = constructMatrix(2, 1);
        if (!evects[i]) {
            for (size_t j = 0; j < i; j++) freeMatrix(evects[j]);
            free(evects);
            free(eigs);
            return NULL;
        }
    }

    // Get the eigenvectors
    for (size_t i = 0; i < 2; i++) {
        FieldElement lambda = eigs[i];

        if (elemIsNan(lambda)) {
            freeMatrix(evects[i]);
            evects[i] = NULL;
            continue;
        }

        // Rows of A - lambda*I
        FieldElement a = elemSub(getEntry(matrix, 0, 0), lambda);
        FieldElement b = getEntry(matrix, 0, 1);
        FieldElement c = getEntry(matrix, 1, 0);
        FieldElement d = elemSub(getEntry(matrix, 1, 1), lambda);

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
    if (!matrixSupportsNumericProjection(matrix)) return NULL;
    if (!isSquare(matrix)) return NULL;
    if (!(matrix->numRows == 3)) return NULL;

    // Get eigenvales and allocate memory
    FieldElement* eigs = eigenvalues3x3(matrix);
    if (!eigs) return NULL;
    Matrix** evects = malloc(3 * sizeof(Matrix*));
    if (!evects) {
        free(eigs);
        return NULL;
    }
    for (size_t i = 0; i < 3; i++) {
        evects[i] = constructMatrix(3, 1);
        if (!evects[i]) {
            for (size_t j = 0; j < i; j++) freeMatrix(evects[j]);
            free(evects);
            free(eigs);
            return NULL;
        }
    }

    // Get the eigenvectors
    for (size_t i = 0; i < 3; i++) {
        FieldElement lambda = eigs[i];

        if (elemIsNan(lambda)) {
            freeMatrix(evects[i]);
            evects[i] = NULL;
            continue;
        }

        // Rows of (A - eig*I)
        FieldElement r00 = elemSub(getEntry(matrix, 0, 0), lambda);
        FieldElement r01 = getEntry(matrix, 0, 1);
        FieldElement r02 = getEntry(matrix, 0, 2);
        FieldElement r10 = getEntry(matrix, 1, 0);
        FieldElement r11 = elemSub(getEntry(matrix, 1, 1), lambda);
        FieldElement r12 = getEntry(matrix, 1, 2);
        FieldElement r20 = getEntry(matrix, 2, 0);
        FieldElement r21 = getEntry(matrix, 2, 1);
        FieldElement r22 = elemSub(getEntry(matrix, 2, 2), lambda);

        // Find null space vector via cross products of row pairs
        FieldElement v0, v1, v2;
        long double normSq;

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

static const long double EIGEN_TOL = 1e-10;
static const long double EIGEN_REAL_SNAP = 1e-7;

// Complex Givens rotation that maps (x, y) -> (r, 0) for r >= 0 real.
// Uses c = conj(x)/r, s = conj(y)/r so G = [[c, s], [-conj(s), conj(c)]].
static void cGivens(ComplexNumber x, ComplexNumber y, ComplexNumber* c, ComplexNumber* s) {
    long double ax = complexAbs(x);
    long double ay = complexAbs(y);
    long double r = sqrtl(ax*ax + ay*ay);
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
static void applyGivensLeft(ComplexNumber* H, size_t n, size_t p, size_t q,
                            ComplexNumber c, ComplexNumber s, size_t j0, size_t j1) {
    ComplexNumber cs = complexConj(s);
    ComplexNumber cc = complexConj(c);
    for (size_t j = j0; j < j1; j++) {
        ComplexNumber a = H[p*n + j];
        ComplexNumber b = H[q*n + j];
        H[p*n + j] = complexAdd(complexMul(c, a), complexMul(s, b));
        H[q*n + j] = complexAdd(complexNeg(complexMul(cs, a)), complexMul(cc, b));
    }
}

// Apply G^* (right) to cols p, q across rows [i0, i1).
//   col_p   <-  conj(c)*col_p + conj(s)*col_q
//   col_q   <- -s*col_p + c*col_q
static void applyGivensRight(ComplexNumber* H, size_t n, size_t p, size_t q,
                             ComplexNumber c, ComplexNumber s, size_t i0, size_t i1) {
    ComplexNumber cs = complexConj(s);
    ComplexNumber cc = complexConj(c);
    for (size_t i = i0; i < i1; i++) {
        ComplexNumber a = H[i*n + p];
        ComplexNumber b = H[i*n + q];
        H[i*n + p] = complexAdd(complexMul(cc, a), complexMul(cs, b));
        H[i*n + q] = complexAdd(complexNeg(complexMul(s, a)), complexMul(c, b));
    }
}

// Reduce H (n x n) to upper Hessenberg form by similarity (in place).
static void hessenbergReduce(ComplexNumber* H, size_t n) {
    if (n < 3) return;
    for (size_t k = 0; k + 2 < n; k++) {
        for (size_t i = k + 2; i < n; i++) {
            ComplexNumber c, s;
            cGivens(H[(k+1)*n + k], H[i*n + k], &c, &s);
            applyGivensLeft(H, n, k + 1, i, c, s, k, n);
            applyGivensRight(H, n, k + 1, i, c, s, 0, n);
        }
    }
}

// Run shifted QR iteration on Hessenberg H (n x n); fill eigs[0..n-1].
static void qrIterate(ComplexNumber* H, size_t n, ComplexNumber* eigs) {
    if (n == 0) return;
    if (n == 1) { eigs[0] = H[0]; return; }

    size_t p = n;
    size_t totalIter = 0;
    size_t sinceDeflate = 0;
    size_t maxIter = 5000 * n + 5000;

    while (p > 1 && totalIter < maxIter) {
        bool hasSplit = false;
        bool deflatedTail = false;
        size_t split = 0;

        // Try to deflate any small subdiagonal in [1, p-1]
        for (size_t i = p - 1; i > 0; i--) {
            ComplexNumber sub = H[i*n + (i-1)];
            ComplexNumber d1 = H[i*n + i];
            ComplexNumber d2 = H[(i-1)*n + (i-1)];
            long double tol = EIGEN_TOL * (complexAbs(d1) + complexAbs(d2)) + 1e-300;
            if (complexAbs(sub) < tol) {
                H[i*n + (i-1)] = (ComplexNumber){0.0, 0.0};
                if (i == p - 1) {
                    eigs[p - 1] = d1;
                    p--;
                    deflatedTail = true;
                    break;
                }
                split = i;
                hasSplit = true;
                break;
            }
        }
        if (deflatedTail) { sinceDeflate = 0; continue; }

        if (p <= 1) break;

        // Operate on the active block [start, p) where start is split or 0
        size_t start = hasSplit ? split : 0;

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
            long double bump = complexAbs(H[(p-1)*n + (p-2)]);
            if (sinceDeflate % 20 == 0) {
                mu.real += 1.5 * bump;
                mu.imag += 0.7 * bump;
            } else {
                mu.real += 0.75 * bump;
            }
        }

        // H[start:p, start:p] -= mu*I
        for (size_t i = start; i < p; i++) H[i*n + i] = complexSub(H[i*n + i], mu);

        // QR step: zero subdiagonals via Givens, accumulating rotations.
        size_t rotCount = p - 1 - start;
        ComplexNumber* cs = malloc((rotCount > 0 ? rotCount : 1) * sizeof(ComplexNumber));
        ComplexNumber* ss = malloc((rotCount > 0 ? rotCount : 1) * sizeof(ComplexNumber));
        if (!cs || !ss) {
            free(cs); free(ss);
            for (size_t i = start; i < p; i++) H[i*n + i] = complexAdd(H[i*n + i], mu);
            for (size_t i = 0; i < p; i++) eigs[i] = H[i*n + i];
            return;
        }
        for (size_t i = start; i < p - 1; i++) {
            size_t idx = i - start;
            cGivens(H[i*n + i], H[(i+1)*n + i], &cs[idx], &ss[idx]);
            applyGivensLeft(H, n, i, i + 1, cs[idx], ss[idx], i, n);
        }
        // Apply Q on the right (R*Q): apply each G_i^* to columns i, i+1
        for (size_t i = start; i < p - 1; i++) {
            size_t idx = i - start;
            applyGivensRight(H, n, i, i + 1, cs[idx], ss[idx], 0, p);
        }
        free(cs); free(ss);

        // H[start:p, start:p] += mu*I
        for (size_t i = start; i < p; i++) H[i*n + i] = complexAdd(H[i*n + i], mu);

        totalIter++;
        sinceDeflate++;
    }

    // Anything left along the diagonal of the unconverged block is best estimate
    for (size_t i = 0; i < p; i++) eigs[i] = H[i*n + i];
}

// Build a complex copy of A (n x n) packed row-major.
static ComplexNumber* matrixToComplex(Matrix* A) {
    size_t n = A->numRows;
    ComplexNumber* H = malloc(n * n * sizeof(ComplexNumber));
    if (!H) return NULL;
    for (size_t i = 0; i < n; i++)
        for (size_t j = 0; j < n; j++)
            H[i*n + j] = elemToComplex(getEntry(A, i, j));
    return H;
}

// Find a single null-space vector of the n x n complex matrix M (modified in place).
// Writes a normalized vector to v. If M is full-rank within tolerance, returns 0;
// otherwise returns 1 on success.
static int complexNullVector(ComplexNumber* M, size_t n, ComplexNumber* v) {
    size_t* pivotCol = malloc(n * sizeof(size_t));
    if (!pivotCol) return 0;
    for (size_t i = 0; i < n; i++) pivotCol[i] = SIZE_MAX;

    long double scale = 0.0;
    for (size_t i = 0; i < n * n; i++) {
        long double a = complexAbs(M[i]);
        if (a > scale) scale = a;
    }
    long double pivotTol = EIGEN_TOL * fmaxl(1.0, scale) + 1e-12;

    size_t row = 0;
    for (size_t col = 0; col < n && row < n; col++) {
        bool foundPivot = false;
        size_t piv = row;

        // Partial pivot: largest |M[r, col]| for r in [row, n)
        long double best = 0.0;
        for (size_t r = row; r < n; r++) {
            long double a = complexAbs(M[r*n + col]);
            if (a > best) {
                best = a;
                piv = r;
                foundPivot = true;
            }
        }
        if (!foundPivot || best < pivotTol) continue;
        if (piv != row) {
            for (size_t c = 0; c < n; c++) {
                ComplexNumber t = M[row*n + c];
                M[row*n + c] = M[piv*n + c];
                M[piv*n + c] = t;
            }
        }
        // Eliminate above and below
        ComplexNumber pv = M[row*n + col];
        for (size_t r = 0; r < n; r++) {
            if (r == row) continue;
            ComplexNumber f = complexDiv(M[r*n + col], pv);
            if (complexAbs(f) < 1e-300) continue;
            for (size_t c = col; c < n; c++) {
                M[r*n + c] = complexSub(M[r*n + c], complexMul(f, M[row*n + c]));
            }
        }
        // Normalize pivot row to 1
        for (size_t c = col; c < n; c++) M[row*n + c] = complexDiv(M[row*n + c], pv);
        pivotCol[row] = col;
        row++;
    }

    // Find a free column
    size_t freeCol = SIZE_MAX;
    for (size_t c = 0; c < n; c++) {
        bool found = false;
        for (size_t r = 0; r < n; r++) if (pivotCol[r] == c) { found = true; break; }
        if (!found) { freeCol = c; break; }
    }
    if (freeCol == SIZE_MAX) { free(pivotCol); return 0; }

    // Build null vector: v[freeCol] = 1, v[other free] = 0,
    // v[pivotCol[r]] = -M[r, freeCol]
    for (size_t i = 0; i < n; i++) { v[i].real = 0.0; v[i].imag = 0.0; }
    v[freeCol].real = 1.0;
    for (size_t r = 0; r < n; r++) {
        if (pivotCol[r] != SIZE_MAX) {
            v[pivotCol[r]] = complexNeg(M[r*n + freeCol]);
        }
    }
    free(pivotCol);

    // Normalize
    long double s = 0.0;
    for (size_t i = 0; i < n; i++) {
        long double a = complexAbs(v[i]);
        s += a * a;
    }
    s = sqrtl(s);
    if (s < 1e-300) {
        v[0].real = 1.0; v[0].imag = 0.0;
        return 1;
    }
    for (size_t i = 0; i < n; i++) {
        v[i].real /= s;
        v[i].imag /= s;
    }
    return 1;
}

// Returns true iff every entry of A is a real (non-complex) FieldElement.
static bool matrixIsReal(Matrix* A) {
    size_t n = A->numRows;
    for (size_t i = 0; i < n; i++)
        for (size_t j = 0; j < n; j++) {
            FieldElement entry = getEntry(A, i, j);
            bool complex = elemIsComplex(entry);
            freeFieldElement(&entry);
            if (complex) return false;
        }
    return true;
}

// Compute eigenvalues of an arbitrary square matrix.
// Returns a freshly allocated array of length matrix->numRows on success.
FieldElement* eigenvalues(Matrix* matrix) {
    if (!matrixSupportsNumericProjection(matrix)) return NULL;
    if (!isSquare(matrix)) return NULL;

    size_t n = matrix->numRows;
    if (n == 2) return eigenvalues2x2(matrix);
    if (n == 3) return eigenvalues3x3(matrix);

    FieldElement* out = malloc(n * sizeof(FieldElement));
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
        long double scale = 0.0;
        for (size_t i = 0; i < n; i++) {
            long double a = complexAbs(eigs[i]);
            if (a > scale) scale = a;
        }
        long double tol = EIGEN_REAL_SNAP * scale + 1e-12;
        for (size_t i = 0; i < n; i++)
            if (fabsl(eigs[i].imag) < tol) eigs[i].imag = 0.0;
    }

    for (size_t i = 0; i < n; i++) out[i] = elemFromComplex(eigs[i]);
    free(eigs);
    free(H);
    return out;
}

// Compute eigenvectors of an arbitrary square matrix.
// Returns a freshly allocated array of n column matrices, parallel to eigenvalues().
Matrix** eigenvectors(Matrix* matrix) {
    if (!matrixSupportsNumericProjection(matrix)) return NULL;
    if (!isSquare(matrix)) return NULL;

    size_t n = matrix->numRows;
    if (n == 2) return eigenvectors2x2(matrix);
    if (n == 3) return eigenvectors3x3(matrix);

    FieldElement* eigs = eigenvalues(matrix);
    if (!eigs) return NULL;

    Matrix** evects = malloc(n * sizeof(Matrix*));
    if (!evects) { free(eigs); return NULL; }
    for (size_t i = 0; i < n; i++) {
        evects[i] = constructMatrix(n, 1);
        if (!evects[i]) {
            for (size_t j = 0; j < i; j++) freeMatrix(evects[j]);
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
        for (size_t i = 0; i < n; i++) freeMatrix(evects[i]);
        free(evects); free(eigs);
        return NULL;
    }

    for (size_t k = 0; k < n; k++) {
        if (elemIsNan(eigs[k])) {
            freeMatrix(evects[k]);
            evects[k] = NULL;
            continue;
        }
        ComplexNumber lam = elemToComplex(eigs[k]);

        // Build M = A - lam*I
        for (size_t i = 0; i < n; i++)
            for (size_t j = 0; j < n; j++)
                M[i*n + j] = elemToComplex(getEntry(matrix, i, j));
        for (size_t i = 0; i < n; i++) M[i*n + i] = complexSub(M[i*n + i], lam);

        if (!complexNullVector(M, n, v)) {
            // Fallback: e_0
            for (size_t i = 0; i < n; i++) { v[i].real = 0.0; v[i].imag = 0.0; }
            v[0].real = 1.0;
        }

        // Snap tiny imaginary parts to 0 for real inputs with real eigenvalue
        if (inputReal && fabsl(lam.imag) < 1e-12) {
            long double scale = 0.0;
            for (size_t i = 0; i < n; i++) {
                long double a = complexAbs(v[i]);
                if (a > scale) scale = a;
            }
            long double tol = EIGEN_REAL_SNAP * scale + 1e-12;
            for (size_t i = 0; i < n; i++)
                if (fabsl(v[i].imag) < tol) v[i].imag = 0.0;
        }

        for (size_t i = 0; i < n; i++)
            setEntry(evects[k], i, 0, elemFromComplex(v[i]));
    }

    free(M); free(v); free(eigs);
    return evects;
}
