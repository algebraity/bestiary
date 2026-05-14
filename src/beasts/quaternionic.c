#include<limits.h>
#include<stdarg.h>
#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#include<string.h>
#include "hebi.h"
#include "sokko.h"
#include "quaternionic.h"

/* ---------- Helper methods ---------- */

// Compute the dimension of a Cayley-Dickson algebra from its degree
static size_t dimensionFromDegree(size_t degree) {
    if (degree >= sizeof(size_t) * CHAR_BIT) return 0;
    return (size_t)1 << degree;
}

// Multiply two size_t values when the product is representable
static bool checkedSizeMul(size_t a, size_t b, size_t* out) {
    if (out == NULL || (a != 0 && b > SIZE_MAX / a)) return false;
    *out = a * b;
    return true;
}

// Add two size_t values when the sum is representable
static bool checkedSizeAdd(size_t a, size_t b, size_t* out) {
    if (out == NULL || a > SIZE_MAX - b) return false;
    *out = a + b;
    return true;
}

// Append text to a dynamically allocated string buffer
static bool appendText(char** string, size_t* length, const char* suffix) {
    size_t suffixLength;
    char* grown;
    if (string == NULL || length == NULL || suffix == NULL) return false;

    // Grow the buffer and append the requested text
    suffixLength = strlen(suffix);
    grown = realloc(*string, *length + suffixLength + 1);
    if (grown == NULL) return false;
    *string = grown;
    memcpy(*string + *length, suffix, suffixLength + 1);
    *length += suffixLength;
    return true;
}

// Append formatted text to a dynamically allocated string buffer
static bool appendFormatted(char** string, size_t* length, const char* format, ...) {
    va_list args;
    va_list copy;
    int suffixLength;
    char* grown;
    if (string == NULL || length == NULL || format == NULL) return false;

    // Measure and append the formatted string in one buffer growth
    va_start(args, format);
    va_copy(copy, args);
    suffixLength = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if (suffixLength < 0) {
        va_end(args);
        return false;
    }
    grown = realloc(*string, *length + (size_t)suffixLength + 1);
    if (grown == NULL) {
        va_end(args);
        return false;
    }
    *string = grown;
    vsnprintf(*string + *length, (size_t)suffixLength + 1, format, args);
    va_end(args);
    *length += (size_t)suffixLength;
    return true;
}

// Check whether a Field can be used as a Cayley-Dickson base field
static bool fieldSupportsCD(Field* field) {
    FieldElement zero;
    bool valid;
    if (field == NULL || field->chr == 2) return false;

    // Probe the field by constructing its zero element
    zero = zeroFieldElement(field);
    valid = fieldElementIsValid(&zero);
    freeFieldElement(&zero);
    return valid;
}

// Allocate a zeroed array of FieldElements
static FieldElement* allocateFieldElementArray(size_t n) {
    if (n == 0) return NULL;
    return calloc(n, sizeof(FieldElement));
}

// Free an owned array of Cayley-Dickson elements
static void freeCDElementArray(CDElement* elements, size_t count);

// Free an array of FieldElements
static void freeFieldElementArray(FieldElement* xs, size_t n) {
    if (xs == NULL) return;

    // Free each owned coefficient before releasing the array
    for (size_t i = 0; i < n; i++) {
        freeFieldElement(&xs[i]);
    }
    free(xs);
}

// Check whether two CDAlgebra structs define the same algebra
static bool cdAlgebraDataEq(CDAlgebra* x, CDAlgebra* y) {
    if (!cdAlgebraIsValid(x) || !cdAlgebraIsValid(y)) return false;
    if (!fieldEq(x->field, y->field) || x->degree != y->degree
            || x->dimension != y->dimension) {
        return false;
    }

    // Compare each Cayley-Dickson parameter over the base field
    for (size_t i = 0; i < x->degree; i++) {
        if (!eqFieldElements(x->params[i], y->params[i])) return false;
    }
    return true;
}

// Allocate an uninitialized CDElement over a valid CDAlgebra
static CDElement allocateCDElement(CDAlgebra* algebra) {
    CDElement x = {0};
    if (!cdAlgebraIsValid(algebra)) return x;

    // Attach the algebra metadata and allocate coefficient storage
    x.algebra = algebra;
    x.dimension = algebra->dimension;
    x.coeffs = allocateFieldElementArray(x.dimension);
    if (x.coeffs == NULL) {
        x.algebra = NULL;
        x.dimension = 0;
    }
    return x;
}

// Add two arrays of FieldElements
static bool addCoeffArrays(FieldElement* x, FieldElement* y, FieldElement* out, size_t n) {
    for (size_t i = 0; i < n; i++) {
        out[i] = addFieldElements(x[i], y[i]);
        if (!fieldElementIsValid(&out[i])) return false;
    }
    return true;
}

// Subtract two arrays of FieldElements
static bool subtractCoeffArrays(FieldElement* x, FieldElement* y, FieldElement* out, size_t n) {
    for (size_t i = 0; i < n; i++) {
        out[i] = subtractFieldElements(x[i], y[i]);
        if (!fieldElementIsValid(&out[i])) return false;
    }
    return true;
}

// Add x plus scalar times y coefficientwise
static bool addScaledCoeffArrays(FieldElement* x, FieldElement scalar,
        FieldElement* y, FieldElement* out, size_t n) {
    FieldElement scaled;
    for (size_t i = 0; i < n; i++) {
        scaled = multiplyFieldElements(scalar, y[i]);
        if (!fieldElementIsValid(&scaled)) return false;
        out[i] = addFieldElements(x[i], scaled);
        freeFieldElement(&scaled);
        if (!fieldElementIsValid(&out[i])) return false;
    }
    return true;
}

// Conjugate a raw Cayley-Dickson coefficient array
static bool conjugateCoeffArray(size_t degree, FieldElement* x, FieldElement* out) {
    size_t dimension = dimensionFromDegree(degree);
    if (dimension == 0) return false;

    // Copy the scalar part and negate the non-scalar coordinates
    out[0] = copyFieldElement(x[0]);
    if (!fieldElementIsValid(&out[0])) return false;
    for (size_t i = 1; i < dimension; i++) {
        out[i] = negateFieldElement(x[i]);
        if (!fieldElementIsValid(&out[i])) return false;
    }
    return true;
}

// Compute the Cayley-Dickson norm of a raw coefficient array
static bool normCoeffArray(CDAlgebra* algebra, size_t degree,
        FieldElement* x, FieldElement* out) {
    size_t half;
    FieldElement lowerNorm = {0};
    FieldElement upperNorm = {0};
    FieldElement scaledUpperNorm = {0};
    bool ok = false;

    // Handle the base-field norm before recursing into doubled coordinates
    if (degree == 0) {
        *out = multiplyFieldElements(x[0], x[0]);
        return fieldElementIsValid(out);
    }

    // Compute the half dimension used to split the coefficient array
    half = dimensionFromDegree(degree - 1);
    if (half == 0) return false;

    // Split x into lower and upper halves and recurse on the norm formula
    if (normCoeffArray(algebra, degree - 1, x, &lowerNorm)
            && normCoeffArray(algebra, degree - 1, x + half, &upperNorm)) {
        scaledUpperNorm = multiplyFieldElements(algebra->params[degree - 1], upperNorm);
        if (fieldElementIsValid(&scaledUpperNorm)) {
            *out = subtractFieldElements(lowerNorm, scaledUpperNorm);
            ok = fieldElementIsValid(out);
        }
    }

    // Free recursive norm temporaries before returning whether the norm was built
    freeFieldElement(&lowerNorm);
    freeFieldElement(&upperNorm);
    freeFieldElement(&scaledUpperNorm);
    return ok;
}

// Multiply two raw Cayley-Dickson coefficient arrays
static bool multiplyCoeffArrays(CDAlgebra* algebra, size_t degree,
        FieldElement* x, FieldElement* y, FieldElement* out) {
    size_t half;
    FieldElement* x0y0 = NULL;
    FieldElement* y1Conj = NULL;
    FieldElement* y1ConjX1 = NULL;
    FieldElement* y1X0 = NULL;
    FieldElement* y0Conj = NULL;
    FieldElement* x1Y0Conj = NULL;
    bool ok = false;

    // Handle base-field multiplication before recursing into doubled coordinates
    if (degree == 0) {
        out[0] = multiplyFieldElements(x[0], y[0]);
        return fieldElementIsValid(&out[0]);
    }

    // Compute the half dimension used to split both operands
    half = dimensionFromDegree(degree - 1);
    if (half == 0) return false;

    // Allocate temporary half-sized arrays for the recursive product pieces
    x0y0 = allocateFieldElementArray(half);
    y1Conj = allocateFieldElementArray(half);
    y1ConjX1 = allocateFieldElementArray(half);
    y1X0 = allocateFieldElementArray(half);
    y0Conj = allocateFieldElementArray(half);
    x1Y0Conj = allocateFieldElementArray(half);

    // Combine the recursive product pieces when every temporary was allocated
    if (x0y0 != NULL && y1Conj != NULL && y1ConjX1 != NULL
            && y1X0 != NULL && y0Conj != NULL && x1Y0Conj != NULL) {
        // Compute the recursive Cayley-Dickson product component by component
        ok = multiplyCoeffArrays(algebra, degree - 1, x, y, x0y0)
            && conjugateCoeffArray(degree - 1, y + half, y1Conj)
            && multiplyCoeffArrays(algebra, degree - 1, y1Conj, x + half, y1ConjX1)
            && addScaledCoeffArrays(x0y0, algebra->params[degree - 1],
                    y1ConjX1, out, half)
            && multiplyCoeffArrays(algebra, degree - 1, y + half, x, y1X0)
            && conjugateCoeffArray(degree - 1, y, y0Conj)
            && multiplyCoeffArrays(algebra, degree - 1, x + half, y0Conj, x1Y0Conj)
            && addCoeffArrays(y1X0, x1Y0Conj, out + half, half);
    }

    // Release all recursive temporaries before reporting success
    freeFieldElementArray(x0y0, half);
    freeFieldElementArray(y1Conj, half);
    freeFieldElementArray(y1ConjX1, half);
    freeFieldElementArray(y1X0, half);
    freeFieldElementArray(y0Conj, half);
    freeFieldElementArray(x1Y0Conj, half);
    return ok;
}

/* ----------- Construct methods ----------- */

// Construct a Cayley-Dickson algebra
CDAlgebra constructCDAlgebra(Field* field, size_t degree, FieldElement* params) {
    CDAlgebra algebra = {0};
    size_t dimension = dimensionFromDegree(degree);
    if (dimension == 0 || !fieldSupportsCD(field)
            || (degree > 0 && params == NULL)) {
        return algebra;
    }

    // Store the shared algebra metadata before copying the parameters
    algebra.field = field;
    algebra.degree = degree;
    algebra.dimension = dimension;
    if (degree > 0) {
        algebra.params = allocateFieldElementArray(degree);
        if (algebra.params == NULL) {
            algebra.field = NULL;
            algebra.degree = 0;
            algebra.dimension = 0;
            return algebra;
        }
    }

    // Copy and validate each doubling parameter
    for (size_t i = 0; i < degree; i++) {
        if (!fieldElementIsValid(&params[i]) || !fieldEq(field, params[i].field)) {
            freeCDAlgebra(&algebra);
            return (CDAlgebra){0};
        }
        algebra.params[i] = copyFieldElementToField(field, params[i]);
        if (!fieldElementIsValid(&algebra.params[i])) {
            freeCDAlgebra(&algebra);
            return (CDAlgebra){0};
        }
    }

    // Re-run structural validation on the fully constructed algebra
    if (!cdAlgebraIsValid(&algebra)) {
        freeCDAlgebra(&algebra);
        return (CDAlgebra){0};
    }
    return algebra;
}

// Construct a Cayley-Dickson element
CDElement constructCDElement(CDAlgebra* algebra, FieldElement* coeffs) {
    CDElement x = {0};
    if (!cdAlgebraIsValid(algebra) || coeffs == NULL) return x;

    // Allocate the coordinate array for this algebra
    x = allocateCDElement(algebra);
    if (x.coeffs == NULL) return (CDElement){0};

    // Copy coefficients only after checking they belong to the base field
    for (size_t i = 0; i < x.dimension; i++) {
        if (!fieldElementIsValid(&coeffs[i]) || !fieldEq(algebra->field, coeffs[i].field)) {
            freeCDElement(&x);
            return (CDElement){0};
        }
        x.coeffs[i] = copyFieldElementToField(algebra->field, coeffs[i]);
        if (!fieldElementIsValid(&x.coeffs[i])) {
            freeCDElement(&x);
            return (CDElement){0};
        }
    }
    return x;
}

// Copy a Cayley-Dickson algebra
CDAlgebra copyCDAlgebra(CDAlgebra* algebra) {
    if (!cdAlgebraIsValid(algebra)) return (CDAlgebra){0};
    return constructCDAlgebra(algebra->field, algebra->degree, algebra->params);
}

// Copy a Cayley-Dickson element
CDElement copyCDElement(CDElement* x) {
    if (!cdElementIsValid(x)) return (CDElement){0};
    return constructCDElement(x->algebra, x->coeffs);
}

/* ----------- Free methods ----------- */

// Free a Cayley-Dickson algebra
void freeCDAlgebra(CDAlgebra* algebra) {
    if (algebra == NULL) return;

    // Release the copied parameters and reset the public struct
    freeFieldElementArray(algebra->params, algebra->degree);
    algebra->params = NULL;
    algebra->field = NULL;
    algebra->degree = 0;
    algebra->dimension = 0;
}

// Free a Cayley-Dickson element
void freeCDElement(CDElement* x) {
    if (x == NULL) return;

    // Release all coordinates and detach the element from its algebra
    freeFieldElementArray(x->coeffs, x->dimension);
    x->coeffs = NULL;
    x->algebra = NULL;
    x->dimension = 0;
}

// Free a Cayley-Dickson ideal
void freeCDIdeal(CDIdeal* ideal) {
    if (ideal == NULL) return;

    // Release the owned basis and clear the wrapper metadata
    freeCDElementArray(ideal->basis, ideal->count);
    ideal->basis = NULL;
    ideal->algebra = NULL;
    ideal->count = 0;
    ideal->type = LEFT_IDEAL;
}

// Free a Cayley-Dickson subalgebra
void freeCDSubalgebra(CDSubalgebra* subalgebra) {
    if (subalgebra == NULL) return;

    // Release the owned basis and clear the wrapper metadata
    freeCDElementArray(subalgebra->basis, subalgebra->count);
    subalgebra->basis = NULL;
    subalgebra->cdAlgebra = NULL;
    subalgebra->count = 0;
}

/* ----------- Print methods ----------- */

// Convert a Cayley-Dickson algebra to a string
char* cdAlgebraToString(CDAlgebra* algebra) {
    char* fieldString;
    char* string = NULL;
    size_t length = 0;
    if (!cdAlgebraIsValid(algebra)) return strdup("<invalid CD algebra>");

    // Begin with the degree, dimension, and base field
    fieldString = fieldToString(algebra->field);
    if (fieldString == NULL) return NULL;
    if (!appendFormatted(&string, &length, "CD(degree=%zu, dim=%zu, field=%s, params=[",
                algebra->degree, algebra->dimension, fieldString)) {
        free(fieldString);
        return NULL;
    }
    free(fieldString);

    // Append each Cayley-Dickson doubling parameter
    for (size_t i = 0; i < algebra->degree; i++) {
        char* paramString = fieldElementToString(algebra->params[i]);
        if (paramString == NULL
                || (i > 0 && !appendText(&string, &length, ", "))
                || !appendText(&string, &length, paramString)) {
            free(paramString);
            free(string);
            return NULL;
        }
        free(paramString);
    }
    if (!appendText(&string, &length, "])")) {
        free(string);
        return NULL;
    }
    return string;
}

// Convert a Cayley-Dickson element to a string
char* cdElementToString(CDElement* x) {
    char* string = NULL;
    size_t length = 0;
    bool first = true;
    if (!cdElementIsValid(x)) return strdup("<invalid CD element>");

    // Append each nonzero coordinate in the standard basis
    for (size_t i = 0; i < x->dimension; i++) {
        char* coeffString;
        if (fieldElementIsZero(x->coeffs[i])) continue;
        coeffString = fieldElementToString(x->coeffs[i]);
        if (coeffString == NULL) {
            free(string);
            return NULL;
        }
        if (!first && !appendText(&string, &length, " + ")) {
            free(coeffString);
            free(string);
            return NULL;
        }
        if (i == 0) {
            if (!appendText(&string, &length, coeffString)) {
                free(coeffString);
                free(string);
                return NULL;
            }
        }
        else if (fieldElementIsOne(x->coeffs[i])) {
            if (!appendFormatted(&string, &length, "e%zu", i)) {
                free(coeffString);
                free(string);
                return NULL;
            }
        }
        else if (!appendFormatted(&string, &length, "%s*e%zu", coeffString, i)) {
            free(coeffString);
            free(string);
            return NULL;
        }
        free(coeffString);
        first = false;
    }

    // Return zero when every coordinate is zero
    if (first && !appendText(&string, &length, "0")) {
        free(string);
        return NULL;
    }
    return string;
}

// Convert a Cayley-Dickson ideal to a string
char* cdIdealToString(CDIdeal* ideal) {
    char* string = NULL;
    size_t length = 0;
    const char* type = "two-sided";
    if (!cdIdealIsValid(ideal)) return strdup("<invalid CD ideal>");
    if (ideal->type == LEFT_IDEAL) type = "left";
    else if (ideal->type == RIGHT_IDEAL) type = "right";

    // Begin with ideal metadata before listing the spanning basis
    if (!appendFormatted(&string, &length, "<%s CD ideal; basis=[", type)) return NULL;
    for (size_t i = 0; i < ideal->count; i++) {
        char* basisString = cdElementToString(&ideal->basis[i]);
        if (basisString == NULL
                || (i > 0 && !appendText(&string, &length, ", "))
                || !appendText(&string, &length, basisString)) {
            free(basisString);
            free(string);
            return NULL;
        }
        free(basisString);
    }
    if (!appendText(&string, &length, "]>")) {
        free(string);
        return NULL;
    }
    return string;
}

// Convert a Cayley-Dickson subalgebra to a string
char* cdSubalgebraToString(CDSubalgebra* subalgebra) {
    char* string = NULL;
    size_t length = 0;
    if (!cdSubalgebraIsValid(subalgebra)) return strdup("<invalid CD subalgebra>");

    // Begin with subalgebra metadata before listing the spanning basis
    if (!appendText(&string, &length, "<CD subalgebra; basis=[")) return NULL;
    for (size_t i = 0; i < subalgebra->count; i++) {
        char* basisString = cdElementToString(&subalgebra->basis[i]);
        if (basisString == NULL
                || (i > 0 && !appendText(&string, &length, ", "))
                || !appendText(&string, &length, basisString)) {
            free(basisString);
            free(string);
            return NULL;
        }
        free(basisString);
    }
    if (!appendText(&string, &length, "]>")) {
        free(string);
        return NULL;
    }
    return string;
}

/* ----------- Bool methods ----------- */

// Check whether a Cayley-Dickson algebra is valid
bool cdAlgebraIsValid(CDAlgebra* algebra) {
    if (algebra == NULL || !fieldSupportsCD(algebra->field)
            || dimensionFromDegree(algebra->degree) != algebra->dimension) {
        return false;
    }
    if (algebra->degree > 0 && algebra->params == NULL) return false;

    // Check every doubling parameter against the base field
    for (size_t i = 0; i < algebra->degree; i++) {
        if (!fieldElementIsValid(&algebra->params[i])
                || !fieldEq(algebra->field, algebra->params[i].field)) {
            return false;
        }
    }
    return true;
}

// Check whether a Cayley-Dickson element is valid
bool cdElementIsValid(CDElement* x) {
    if (x == NULL || !cdAlgebraIsValid(x->algebra)
            || x->dimension != x->algebra->dimension || x->coeffs == NULL) {
        return false;
    }

    // Check every coordinate against the element's base field
    for (size_t i = 0; i < x->dimension; i++) {
        if (!fieldElementIsValid(&x->coeffs[i])
                || !fieldEq(x->algebra->field, x->coeffs[i].field)) {
            return false;
        }
    }
    return true;
}

// Check whether a Cayley-Dickson ideal is valid
bool cdIdealIsValid(CDIdeal* ideal) {
    if (ideal == NULL || !cdAlgebraIsValid(ideal->algebra)
            || ideal->type < LEFT_IDEAL || ideal->type > TWO_SIDED_IDEAL) {
        return false;
    }
    if (ideal->count == 0) return ideal->basis == NULL;
    if (ideal->basis == NULL) return false;

    // Check that every basis element belongs to the wrapped algebra
    for (size_t i = 0; i < ideal->count; i++) {
        if (!cdElementIsValid(&ideal->basis[i])
                || !cdAlgebraDataEq(ideal->algebra, ideal->basis[i].algebra)) {
            return false;
        }
    }
    return true;
}

// Check whether a Cayley-Dickson subalgebra is valid
bool cdSubalgebraIsValid(CDSubalgebra* subalgebra) {
    if (subalgebra == NULL || !cdAlgebraIsValid(subalgebra->cdAlgebra)
            || subalgebra->count == 0 || subalgebra->basis == NULL) {
        return false;
    }

    // Check that every basis element belongs to the ambient algebra
    for (size_t i = 0; i < subalgebra->count; i++) {
        if (!cdElementIsValid(&subalgebra->basis[i])
                || !cdAlgebraDataEq(subalgebra->cdAlgebra, subalgebra->basis[i].algebra)) {
            return false;
        }
    }
    return true;
}

// Check whether two Cayley-Dickson algebras define the same algebra
bool cdAlgebraEq(CDAlgebra* x, CDAlgebra* y) {
    return cdAlgebraDataEq(x, y);
}

// Check whether two Cayley-Dickson elements belong to the same algebra
bool sameCDAlgebra(CDElement* x, CDElement* y) {
    if (!cdElementIsValid(x) || !cdElementIsValid(y)) return false;
    return cdAlgebraDataEq(x->algebra, y->algebra);
}

// Check whether two Cayley-Dickson elements are equal
bool cdEq(CDElement* x, CDElement* y) {
    if (!sameCDAlgebra(x, y)) return false;

    // Compare coordinates once both elements are known to share an algebra
    for (size_t i = 0; i < x->dimension; i++) {
        if (!eqFieldElements(x->coeffs[i], y->coeffs[i])) return false;
    }
    return true;
}

/* ---------- CDElement arithmetic ---------- */

// Add two Cayley-Dickson elements
CDElement cdAdd(CDElement* x, CDElement* y) {
    CDElement sum = {0};
    if (!sameCDAlgebra(x, y)) return sum;

    // Allocate the result and add coordinates componentwise
    sum = allocateCDElement(x->algebra);
    if (sum.coeffs == NULL) return (CDElement){0};
    if (!addCoeffArrays(x->coeffs, y->coeffs, sum.coeffs, sum.dimension)) {
        freeCDElement(&sum);
        return (CDElement){0};
    }
    return sum;
}

// Subtract two Cayley-Dickson elements
CDElement cdSubtract(CDElement* x, CDElement* y) {
    CDElement diff = {0};
    if (!sameCDAlgebra(x, y)) return diff;

    // Allocate the result and subtract coordinates componentwise
    diff = allocateCDElement(x->algebra);
    if (diff.coeffs == NULL) return (CDElement){0};
    if (!subtractCoeffArrays(x->coeffs, y->coeffs, diff.coeffs, diff.dimension)) {
        freeCDElement(&diff);
        return (CDElement){0};
    }
    return diff;
}

// Multiply two Cayley-Dickson elements
CDElement cdMult(CDElement* x, CDElement* y) {
    CDElement product = {0};
    if (!sameCDAlgebra(x, y)) return product;

    // Allocate the result and fill it using recursive CD multiplication
    product = allocateCDElement(x->algebra);
    if (product.coeffs == NULL) return (CDElement){0};
    if (!multiplyCoeffArrays(x->algebra, x->algebra->degree, x->coeffs, y->coeffs,
                product.coeffs)) {
        freeCDElement(&product);
        return (CDElement){0};
    }
    return product;
}

// Return the commutator xy-yx of two Cayley-Dickson elements
CDElement cdCommutator(CDElement* x, CDElement* y) {
    CDElement commutator = {0};
    CDElement xy;
    CDElement yx;
    if (!sameCDAlgebra(x, y)) return commutator;

    // Compute both products and subtract them when both succeed
    xy = cdMult(x, y);
    yx = cdMult(y, x);
    if (cdElementIsValid(&xy) && cdElementIsValid(&yx)) {
        commutator = cdSubtract(&xy, &yx);
    }
    freeCDElement(&xy);
    freeCDElement(&yx);
    return commutator;
}

// Return the associator (xy)z-x(yz) of three Cayley-Dickson elements
CDElement cdAssociator(CDElement* x, CDElement* y, CDElement* z) {
    CDElement associator = {0};
    CDElement xy;
    CDElement yz;
    CDElement xyZ;
    CDElement xYz;
    if (!sameCDAlgebra(x, y) || !sameCDAlgebra(x, z)) return associator;

    // Compute the two parenthesizations before subtracting
    xy = cdMult(x, y);
    yz = cdMult(y, z);
    if (cdElementIsValid(&xy) && cdElementIsValid(&yz)) {
        xyZ = cdMult(&xy, z);
        xYz = cdMult(x, &yz);
        if (cdElementIsValid(&xyZ) && cdElementIsValid(&xYz)) {
            associator = cdSubtract(&xyZ, &xYz);
        }
        freeCDElement(&xyZ);
        freeCDElement(&xYz);
    }
    freeCDElement(&xy);
    freeCDElement(&yz);
    return associator;
}

// Left divide two Cayley-Dickson elements
CDElement cdLeftDivide(CDElement* x, CDElement* y) {
    CDElement quotient = {0};
    CDElement inverse;
    if (!sameCDAlgebra(x, y)) return quotient;

    // Form y^{-1} before multiplying on the left
    inverse = cdInverse(y);
    if (!cdElementIsValid(&inverse)) return quotient;

    // Multiply y^{-1}x for left division
    quotient = cdMult(&inverse, x);
    freeCDElement(&inverse);
    return quotient;
}

// Right divide two Cayley-Dickson elements
CDElement cdRightDivide(CDElement* x, CDElement* y) {
    CDElement quotient = {0};
    CDElement inverse;
    if (!sameCDAlgebra(x, y)) return quotient;

    // Form y^{-1} before multiplying on the right
    inverse = cdInverse(y);
    if (!cdElementIsValid(&inverse)) return quotient;

    // Multiply xy^{-1} for right division
    quotient = cdMult(x, &inverse);
    freeCDElement(&inverse);
    return quotient;
}

// Invert a Cayley-Dickson element by the conjugate formula
CDElement cdInverse(CDElement* x) {
    CDElement inverse = {0};
    CDElement conjugate;
    FieldElement norm;
    FieldElement normInverse;

    // Apply x^{-1} = conjugate(x) / norm(x)
    conjugate = cdConjugate(x);
    norm = cdNorm(x);
    normInverse = invertFieldElement(norm);
    inverse = cdScalarMult(&conjugate, normInverse);
    freeCDElement(&conjugate);
    freeFieldElement(&norm);
    freeFieldElement(&normInverse);
    return inverse;
}

// Return the conjugate of a Cayley-Dickson element
CDElement cdConjugate(CDElement* x) {
    CDElement conjugate = {0};
    if (!cdElementIsValid(x)) return conjugate;

    // Allocate the result and fill it with the raw conjugate formula
    conjugate = allocateCDElement(x->algebra);
    if (conjugate.coeffs == NULL) return (CDElement){0};
    if (!conjugateCoeffArray(x->algebra->degree, x->coeffs, conjugate.coeffs)) {
        freeCDElement(&conjugate);
        return (CDElement){0};
    }
    return conjugate;
}

// Return the Cayley-Dickson norm of an element
FieldElement cdNorm(CDElement* x) {
    FieldElement norm = {0};
    if (!cdElementIsValid(x)) return norm;

    // Delegate to the recursive raw coefficient norm routine
    if (!normCoeffArray(x->algebra, x->algebra->degree, x->coeffs, &norm)) {
        freeFieldElement(&norm);
        return (FieldElement){0};
    }
    return norm;
}

// Multiply a Cayley-Dickson element by a scalar
CDElement cdScalarMult(CDElement* x, FieldElement scalar) {
    CDElement product = {0};
    if (!cdElementIsValid(x) || !fieldElementIsValid(&scalar)
            || !fieldEq(x->algebra->field, scalar.field)) {
            return product;
    }

    // Allocate and fill the scalar multiple coordinate by coordinate
    product = allocateCDElement(x->algebra);
    if (product.coeffs == NULL) return (CDElement){0};
    for (size_t i = 0; i < product.dimension; i++) {
        product.coeffs[i] = multiplyFieldElements(scalar, x->coeffs[i]);
        if (!fieldElementIsValid(&product.coeffs[i])) {
            freeCDElement(&product);
            return (CDElement){0};
        }
    }
    return product;
}

/* ---------- Linear algebra ---------- */

// Set a Matrix column from a Cayley-Dickson element's coordinates
static bool setMatrixColumnFromCDElement(Matrix* matrix, size_t column, CDElement* x) {
    Vector* vector;
    bool success;
    if (!cdElementIsValid(x)) return false;

    // Convert the element to coordinates and set the target matrix column
    vector = cdToVector(x);
    if (vector == NULL) return false;
    success = setMatrixColumn(matrix, column, vector);
    freeVector(vector);
    return success;
}

// Check whether a Cayley-Dickson element is zero
static bool cdElementIsZero(CDElement* x) {
    if (!cdElementIsValid(x)) return false;

    // Test each coordinate for zero in the base field
    for (size_t i = 0; i < x->dimension; i++) {
        if (!fieldElementIsZero(x->coeffs[i])) return false;
    }
    return true;
}

// Return a matrix whose columns are Cayley-Dickson elements
static Matrix* matrixFromCDElements(CDElement* elements, size_t count) {
    Matrix* matrix;
    Vector** columns;
    if (elements == NULL || count == 0 || !cdElementIsValid(&elements[0])) return NULL;

    // Convert the Cayley-Dickson elements into column vectors and assemble the matrix
    columns = calloc(count, sizeof(Vector*));
    if (columns == NULL) return NULL;
    for (size_t j = 0; j < count; j++) {
        if (!sameCDAlgebra(&elements[0], &elements[j])) {
            freeVectorBasis(columns, j);
            return NULL;
        }
        columns[j] = cdToVector(&elements[j]);
        if (columns[j] == NULL) {
            freeVectorBasis(columns, j);
            return NULL;
        }
    }

    // Assemble the basis matrix after converting each element into coordinates
    matrix = matrixFromColumns(columns, count);
    freeVectorBasis(columns, count);
    return matrix;
}

// Return a matrix whose columns are basis plus x
static Matrix* matrixFromCDElementsWithExtra(CDElement* basis, size_t count, CDElement* x) {
    Matrix* matrix;
    Vector** columns;
    if (!cdElementIsValid(x)) return NULL;

    // Convert the Cayley-Dickson elements into column vectors and assemble the matrix
    columns = calloc(count + 1, sizeof(Vector*));
    if (columns == NULL) return NULL;
    for (size_t j = 0; j < count; j++) {
        if (!sameCDAlgebra(x, &basis[j])) {
            freeVectorBasis(columns, j);
            return NULL;
        }
        columns[j] = cdToVector(&basis[j]);
        if (columns[j] == NULL) {
            freeVectorBasis(columns, j);
            return NULL;
        }
    }
    columns[count] = cdToVector(x);
    if (columns[count] == NULL) {
        freeVectorBasis(columns, count);
        return NULL;
    }

    // Assemble the augmented basis matrix with x as the final column
    matrix = matrixFromColumns(columns, count + 1);
    freeVectorBasis(columns, count + 1);
    return matrix;
}

// Convert a Vector basis to a Cayley-Dickson basis
static CDElement* cdBasisFromVectorBasis(CDAlgebra* algebra, Vector** vectors,
        size_t count, size_t* basisCount) {
    CDElement* basis;
    if (basisCount != NULL) *basisCount = 0;
    if (!cdAlgebraIsValid(algebra) || vectors == NULL || count == 0 || basisCount == NULL) {
        return NULL;
    }

    // Allocate the CD basis and convert each vector into an element
    basis = calloc(count, sizeof(CDElement));
    if (basis == NULL) return NULL;
    for (size_t i = 0; i < count; i++) {
        basis[i] = cdFromVector(algebra, vectors[i]);
        if (!cdElementIsValid(&basis[i])) {
            for (size_t j = 0; j <= i; j++) freeCDElement(&basis[j]);
            free(basis);
            return NULL;
        }
    }

    *basisCount = count;
    return basis;
}

// Return a matrix whose columns are associators in a fixed slot
static Matrix* cdAssociatorSlotMatrix(CDElement* x, CDElement* y, int slot) {
    Matrix* matrix;
    CDElement* basis;
    CDElement associator;
    if (!sameCDAlgebra(x, y) || slot < 0 || slot > 2) return NULL;

    // Allocate the matrix before filling its columns with basis images
    matrix = constructMatrixOverField(x->algebra->field, x->dimension, x->dimension);
    if (matrix == NULL) return NULL;
    basis = cdStandardBasis(x->algebra);
    if (basis == NULL) {
        freeMatrix(matrix);
        return NULL;
    }

    // Fill each column with the associator map evaluated on the matching basis vector
    for (size_t j = 0; j < x->dimension; j++) {
        if (slot == 0) {
            associator = cdAssociator(&basis[j], x, y);
        }
        else if (slot == 1) {
            associator = cdAssociator(x, &basis[j], y);
        }
        else {
            associator = cdAssociator(x, y, &basis[j]);
        }

        // Abort the associator matrix build if a column cannot be written
        if (!setMatrixColumnFromCDElement(matrix, j, &associator)) {
            freeCDElement(&associator);
            for (size_t k = 0; k < x->dimension; k++) freeCDElement(&basis[k]);
            free(basis);
            freeMatrix(matrix);
            return NULL;
        }
        freeCDElement(&associator);
    }

    // Free the standard basis before returning the completed associator matrix
    for (size_t k = 0; k < x->dimension; k++) freeCDElement(&basis[k]);
    free(basis);
    return matrix;
}

// Return the standard basis element e_index of a Cayley-Dickson algebra
CDElement cdBasisElement(CDAlgebra* algebra, size_t index) {
    CDElement x = {0};
    if (!cdAlgebraIsValid(algebra) || index >= algebra->dimension) return x;

    // Allocate the element and fill the requested standard basis coordinate
    x = allocateCDElement(algebra);
    if (x.coeffs == NULL) return (CDElement){0};
    for (size_t i = 0; i < x.dimension; i++) {
        x.coeffs[i] = i == index ? oneFieldElement(algebra->field)
                                 : zeroFieldElement(algebra->field);
        if (!fieldElementIsValid(&x.coeffs[i])) {
            freeCDElement(&x);
            return (CDElement){0};
        }
    }
    return x;
}

// Return the standard basis of a Cayley-Dickson algebra
CDElement* cdStandardBasis(CDAlgebra* algebra) {
    CDElement* basis;
    if (!cdAlgebraIsValid(algebra)) return NULL;

    // Allocate and fill every standard basis element of the algebra
    basis = calloc(algebra->dimension, sizeof(CDElement));
    if (basis == NULL) return NULL;
    for (size_t i = 0; i < algebra->dimension; i++) {
        basis[i] = cdBasisElement(algebra, i);
        if (!cdElementIsValid(&basis[i])) {
            for (size_t j = 0; j <= i; j++) {
                freeCDElement(&basis[j]);
            }
            free(basis);
            return NULL;
        }
    }
    return basis;
}

// Return the coordinate vector of a Cayley-Dickson element
Vector* cdToVector(CDElement* x) {
    if (!cdElementIsValid(x)) return NULL;
    return constructVectorFromArrayOverField(x->algebra->field, x->dimension,
            x->coeffs, x->dimension);
}

// Construct a Cayley-Dickson element from a coordinate vector
CDElement cdFromVector(CDAlgebra* algebra, Vector* vector) {
    if (!cdAlgebraIsValid(algebra) || vector == NULL || vector->data == NULL
            || vector->numRows != algebra->dimension || vector->numCols != 1
            || !fieldEq(algebra->field, vector->field)) {
        return (CDElement){0};
    }
    return constructCDElement(algebra, vector->data);
}

// Return the matrix of left multiplication by a Cayley-Dickson element
Matrix* cdLeftMultMatrix(CDElement* x) {
    Matrix* matrix;
    CDElement* basis;
    CDElement product;
    if (!cdElementIsValid(x)) return NULL;

    // Allocate the matrix before filling its columns with basis images
    matrix = constructMatrixOverField(x->algebra->field, x->dimension, x->dimension);
    if (matrix == NULL) return NULL;
    basis = cdStandardBasis(x->algebra);
    if (basis == NULL) {
        freeMatrix(matrix);
        return NULL;
    }

    // Fill each column with left multiplication by the matching basis element
    for (size_t j = 0; j < x->dimension; j++) {
        product = cdMult(x, &basis[j]);
        if (!setMatrixColumnFromCDElement(matrix, j, &product)) {
            freeCDElement(&product);
            for (size_t k = 0; k < x->dimension; k++) freeCDElement(&basis[k]);
            free(basis);
            freeMatrix(matrix);
            return NULL;
        }
        freeCDElement(&product);
    }

    // Free the standard basis before returning the left multiplication matrix
    for (size_t k = 0; k < x->dimension; k++) freeCDElement(&basis[k]);
    free(basis);
    return matrix;
}

// Return the matrix of right multiplication by a Cayley-Dickson element
Matrix* cdRightMultMatrix(CDElement* x) {
    Matrix* matrix;
    CDElement* basis;
    CDElement product;
    if (!cdElementIsValid(x)) return NULL;

    // Allocate the matrix before filling its columns with basis images
    matrix = constructMatrixOverField(x->algebra->field, x->dimension, x->dimension);
    if (matrix == NULL) return NULL;
    basis = cdStandardBasis(x->algebra);
    if (basis == NULL) {
        freeMatrix(matrix);
        return NULL;
    }

    // Fill each column with right multiplication by the matching basis element
    for (size_t j = 0; j < x->dimension; j++) {
        product = cdMult(&basis[j], x);
        if (!setMatrixColumnFromCDElement(matrix, j, &product)) {
            freeCDElement(&product);
            for (size_t k = 0; k < x->dimension; k++) freeCDElement(&basis[k]);
            free(basis);
            freeMatrix(matrix);
            return NULL;
        }
        freeCDElement(&product);
    }

    // Free the standard basis before returning the right multiplication matrix
    for (size_t k = 0; k < x->dimension; k++) freeCDElement(&basis[k]);
    free(basis);
    return matrix;
}

// Return the matrix of the commutator map y -> xy-yx
Matrix* cdCommutatorMatrix(CDElement* x) {
    Matrix* left;
    Matrix* right;
    Matrix* commutator;
    if (!cdElementIsValid(x)) return NULL;

    // Build the left and right multiplication matrices used by the commutator map
    left = cdLeftMultMatrix(x);
    right = cdRightMultMatrix(x);
    if (left == NULL || right == NULL) {
        freeMatrix(left);
        freeMatrix(right);
        return NULL;
    }

    // Subtract right multiplication from left multiplication and release both inputs
    commutator = subtractMatrices(left, right);
    freeMatrix(left);
    freeMatrix(right);
    return commutator;
}

// Return the matrix of the associator map z -> (xy)z-x(yz)
Matrix* cdAssociatorMatrix(CDElement* x, CDElement* y) {
    Matrix* matrix;
    CDElement* basis;
    CDElement associator;
    if (!sameCDAlgebra(x, y)) return NULL;

    // Allocate the matrix before filling its columns with basis images
    matrix = constructMatrixOverField(x->algebra->field, x->dimension, x->dimension);
    if (matrix == NULL) return NULL;
    basis = cdStandardBasis(x->algebra);
    if (basis == NULL) {
        freeMatrix(matrix);
        return NULL;
    }

    // Fill each column with the associator evaluated on the matching basis element
    for (size_t j = 0; j < x->dimension; j++) {
        associator = cdAssociator(x, y, &basis[j]);
        if (!setMatrixColumnFromCDElement(matrix, j, &associator)) {
            freeCDElement(&associator);
            for (size_t k = 0; k < x->dimension; k++) freeCDElement(&basis[k]);
            free(basis);
            freeMatrix(matrix);
            return NULL;
        }
        freeCDElement(&associator);
    }

    // Free the standard basis before returning the associator matrix
    for (size_t k = 0; k < x->dimension; k++) freeCDElement(&basis[k]);
    free(basis);
    return matrix;
}

// Return a linearly independent basis for the span of Cayley-Dickson elements
CDElement* cdSpanBasis(CDElement* elements, size_t count, size_t* basisCount) {
    Matrix* matrix;
    Vector** vectorBasis;
    CDElement* basis;
    size_t vectorCount = 0;
    if (basisCount != NULL) *basisCount = 0;
    if (basisCount == NULL || elements == NULL || count == 0 || !cdElementIsValid(&elements[0])) {
        return NULL;
    }

    // Convert the input elements to a coordinate matrix before taking its column space
    matrix = matrixFromCDElements(elements, count);
    if (matrix == NULL) return NULL;
    vectorBasis = columnSpace(matrix, &vectorCount);
    freeMatrix(matrix);
    if (vectorBasis == NULL || vectorCount == 0) return NULL;

    // Convert the independent vector-space columns back into CD basis elements
    basis = cdBasisFromVectorBasis(elements[0].algebra, vectorBasis, vectorCount, basisCount);
    freeVectorBasis(vectorBasis, vectorCount);
    return basis;
}

// Check whether a Cayley-Dickson element lies in the span of a basis
bool cdElementInSpan(CDElement* x, CDElement* basis, size_t count) {
    Matrix* basisMatrix;
    Matrix* augmentedMatrix;
    size_t basisRank;
    size_t augmentedRank;
    if (!cdElementIsValid(x)) return false;
    if (count == 0) return cdElementIsZero(x);
    if (basis == NULL) return false;

    // Compare the rank before and after adjoining x to test span membership
    basisMatrix = matrixFromCDElements(basis, count);
    augmentedMatrix = matrixFromCDElementsWithExtra(basis, count, x);
    if (basisMatrix == NULL || augmentedMatrix == NULL) {
        freeMatrix(basisMatrix);
        freeMatrix(augmentedMatrix);
        return false;
    }

    // Compute the rank needed for the membership test
    basisRank = rank(basisMatrix);
    augmentedRank = rank(augmentedMatrix);
    freeMatrix(basisMatrix);
    freeMatrix(augmentedMatrix);
    return basisRank != (size_t)-1 && basisRank == augmentedRank;
}

// Check whether left multiplication by x has nonzero kernel
bool cdIsLeftZeroDivisor(CDElement* x) {
    Matrix* left;
    size_t nty;
    if (!cdElementIsValid(x)) return false;

    // Build the left multiplication matrix and test whether its kernel is nontrivial
    left = cdLeftMultMatrix(x);
    if (left == NULL) return false;
    nty = nullity(left);
    freeMatrix(left);
    return nty != (size_t)-1 && nty > 0;
}

// Check whether right multiplication by x has nonzero kernel
bool cdIsRightZeroDivisor(CDElement* x) {
    Matrix* right;
    size_t nty;
    if (!cdElementIsValid(x)) return false;

    // Build the right multiplication matrix and test whether its kernel is nontrivial
    right = cdRightMultMatrix(x);
    if (right == NULL) return false;
    nty = nullity(right);
    freeMatrix(right);
    return nty != (size_t)-1 && nty > 0;
}

// Return a basis for the left annihilator { y : yx = 0 }
CDElement* cdLeftAnnihilator(CDElement* x, size_t* basisCount) {
    Matrix* right;
    Vector** nullBasis;
    CDElement* annihilator;
    size_t nullCount = 0;
    if (basisCount != NULL) *basisCount = 0;
    if (!cdElementIsValid(x) || basisCount == NULL) return NULL;

    // Compute the nullspace that represents the left annihilator basis
    right = cdRightMultMatrix(x);
    if (right == NULL) return NULL;
    nullBasis = nullSpace(right, &nullCount);
    freeMatrix(right);
    annihilator = cdBasisFromVectorBasis(x->algebra, nullBasis, nullCount, basisCount);
    freeVectorBasis(nullBasis, nullCount);
    return annihilator;
}

// Return a basis for the right annihilator { y : xy = 0 }
CDElement* cdRightAnnihilator(CDElement* x, size_t* basisCount) {
    Matrix* left;
    Vector** nullBasis;
    CDElement* annihilator;
    size_t nullCount = 0;
    if (basisCount != NULL) *basisCount = 0;
    if (!cdElementIsValid(x) || basisCount == NULL) return NULL;

    // Compute the nullspace that represents the right annihilator basis
    left = cdLeftMultMatrix(x);
    if (left == NULL) return NULL;
    nullBasis = nullSpace(left, &nullCount);
    freeMatrix(left);
    annihilator = cdBasisFromVectorBasis(x->algebra, nullBasis, nullCount, basisCount);
    freeVectorBasis(nullBasis, nullCount);
    return annihilator;
}

// Return a basis for the center of a Cayley-Dickson algebra
CDElement* cdCenter(CDAlgebra* algebra, size_t* basisCount) {
    Matrix* equations;
    CDElement* basis;
    Vector** nullBasis;
    CDElement* center;
    size_t nullCount = 0;
    size_t equationRows;
    if (basisCount != NULL) *basisCount = 0;
    if (!cdAlgebraIsValid(algebra) || basisCount == NULL) return NULL;
    if (!checkedSizeMul(algebra->dimension, algebra->dimension, &equationRows)) return NULL;

    // Allocate the linear system whose kernel gives the requested invariant subspace
    equations = constructMatrixOverField(algebra->field, equationRows, algebra->dimension);
    if (equations == NULL) return NULL;
    basis = cdStandardBasis(algebra);
    if (basis == NULL) {
        freeMatrix(equations);
        return NULL;
    }

    // Stack the commutator maps for all basis elements into one linear system
    for (size_t i = 0; i < algebra->dimension; i++) {
        Matrix* commutator = cdCommutatorMatrix(&basis[i]);
        if (commutator == NULL
                || !setMatrixRowBlock(equations, i * algebra->dimension, commutator)) {
            freeMatrix(commutator);
            for (size_t j = 0; j < algebra->dimension; j++) freeCDElement(&basis[j]);
            free(basis);
            freeMatrix(equations);
            return NULL;
        }
        freeMatrix(commutator);
    }

    // Solve the commutator system and convert the kernel to center elements
    nullBasis = nullSpace(equations, &nullCount);
    center = cdBasisFromVectorBasis(algebra, nullBasis, nullCount, basisCount);
    freeVectorBasis(nullBasis, nullCount);
    for (size_t j = 0; j < algebra->dimension; j++) freeCDElement(&basis[j]);
    free(basis);
    freeMatrix(equations);
    return center;
}

// Return a basis for the full nucleus of a Cayley-Dickson algebra
CDElement* cdNucleus(CDAlgebra* algebra, size_t* basisCount) {
    Matrix* equations;
    CDElement* basis;
    Vector** nullBasis;
    CDElement* nucleus;
    size_t nullCount = 0;
    size_t blockRow = 0;
    size_t squareDim;
    size_t cubeDim;
    size_t equationRows;
    if (basisCount != NULL) *basisCount = 0;
    if (!cdAlgebraIsValid(algebra) || basisCount == NULL) return NULL;
    if (!checkedSizeMul(algebra->dimension, algebra->dimension, &squareDim)
            || !checkedSizeMul(squareDim, algebra->dimension, &cubeDim)
            || !checkedSizeMul(3, cubeDim, &equationRows)) {
        return NULL;
    }

    // Allocate the linear system whose kernel gives the requested invariant subspace
    equations = constructMatrixOverField(algebra->field, equationRows, algebra->dimension);
    if (equations == NULL) return NULL;
    basis = cdStandardBasis(algebra);
    if (basis == NULL) {
        freeMatrix(equations);
        return NULL;
    }

    // Stack all three associator-slot constraints for every pair of basis elements
    for (size_t i = 0; i < algebra->dimension; i++) {
        for (size_t j = 0; j < algebra->dimension; j++) {
            for (int slot = 0; slot < 3; slot++) {
                Matrix* associator = cdAssociatorSlotMatrix(&basis[i], &basis[j], slot);
                if (associator == NULL
                        || !setMatrixRowBlock(equations, blockRow, associator)) {
                    freeMatrix(associator);
                    for (size_t k = 0; k < algebra->dimension; k++) freeCDElement(&basis[k]);
                    free(basis);
                    freeMatrix(equations);
                    return NULL;
                }
                blockRow += algebra->dimension;
                freeMatrix(associator);
            }
        }
    }

    // Solve the associator system and convert the kernel to nucleus elements
    nullBasis = nullSpace(equations, &nullCount);
    nucleus = cdBasisFromVectorBasis(algebra, nullBasis, nullCount, basisCount);
    freeVectorBasis(nullBasis, nullCount);
    for (size_t k = 0; k < algebra->dimension; k++) freeCDElement(&basis[k]);
    free(basis);
    freeMatrix(equations);
    return nucleus;
}

// Free an owned array of Cayley-Dickson elements
static void freeCDElementArray(CDElement* elements, size_t count) {
    if (elements == NULL) return;

    // Free every owned Cayley-Dickson element before releasing the array
    for (size_t i = 0; i < count; i++) {
        freeCDElement(&elements[i]);
    }
    free(elements);
}

// Return the common algebra of a generator list
static CDAlgebra* cdGeneratorAlgebra(CDElement* generators, size_t count) {
    if (generators == NULL || count == 0 || !cdElementIsValid(&generators[0])) return NULL;

    // Ensure all generators share one algebra and return that common algebra
    for (size_t i = 1; i < count; i++) {
        if (!sameCDAlgebra(&generators[0], &generators[i])) return NULL;
    }
    return generators[0].algebra;
}

// Reduce an owned array to a basis and free the input
static CDElement* cdReduceOwnedSpan(CDElement* elements, size_t count, size_t* basisCount) {
    CDElement* basis;
    basis = cdSpanBasis(elements, count, basisCount);
    freeCDElementArray(elements, count);
    return basis;
}

// Return an owned copy of an array of Cayley-Dickson elements
static CDElement* copyCDElementArray(CDElement* elements, size_t count) {
    CDElement* copies;
    if (count == 0) return NULL;
    if (elements == NULL) return NULL;

    // Allocate and fill an owned copy of the Cayley-Dickson element array
    copies = calloc(count, sizeof(CDElement));
    if (copies == NULL) return NULL;
    for (size_t i = 0; i < count; i++) {
        if (!cdElementIsValid(&elements[i])) {
            freeCDElementArray(copies, i);
            return NULL;
        }
        copies[i] = constructCDElement(elements[i].algebra, elements[i].coeffs);
        if (!cdElementIsValid(&copies[i])) {
            freeCDElementArray(copies, i + 1);
            return NULL;
        }
    }
    return copies;
}

// Check whether a basis is closed under left or right multiplication by the whole algebra
static bool cdBasisHasIdealClosure(CDElement* basis, size_t count, bool left) {
    CDElement* algebraBasis;
    if (count == 0) return true;
    if (basis == NULL || !cdElementIsValid(&basis[0])) return false;

    // Build the full algebra basis used to test one-sided closure
    algebraBasis = cdStandardBasis(basis[0].algebra);
    if (algebraBasis == NULL) return false;
    for (size_t i = 0; i < basis[0].algebra->dimension; i++) {
        for (size_t j = 0; j < count; j++) {
            CDElement product = left ? cdMult(&algebraBasis[i], &basis[j])
                                     : cdMult(&basis[j], &algebraBasis[i]);
            bool inSpan = cdElementIsValid(&product) && cdElementInSpan(&product, basis, count);
            freeCDElement(&product);
            if (!inSpan) {
                freeCDElementArray(algebraBasis, basis[0].algebra->dimension);
                return false;
            }
        }
    }
    freeCDElementArray(algebraBasis, basis[0].algebra->dimension);
    return true;
}

// Infer the strongest ideal type satisfied by a basis
static bool inferIdealTypeFromBasis(CDElement* basis, size_t count, IdealType* type) {
    bool leftClosed;
    bool rightClosed;
    if (type == NULL) return false;
    if (count == 0) {
        *type = TWO_SIDED_IDEAL;
        return true;
    }
    if (basis == NULL) return false;

    // Check left and right closure to choose the strongest ideal type
    leftClosed = cdBasisHasIdealClosure(basis, count, true);
    rightClosed = cdBasisHasIdealClosure(basis, count, false);
    if (leftClosed && rightClosed) {
        *type = TWO_SIDED_IDEAL;
        return true;
    }
    if (leftClosed) {
        *type = LEFT_IDEAL;
        return true;
    }
    if (rightClosed) {
        *type = RIGHT_IDEAL;
        return true;
    }
    return false;
}

// Return a basis for the intersection of two spans
static CDElement* cdIntersectSpans(CDElement* xBasis, size_t xCount, CDElement* yBasis,
        size_t yCount, size_t* basisCount, bool* success) {
    Matrix* xMatrix;
    Matrix* equations;
    Vector** columns;
    Vector** nullBasis;
    CDElement* candidates;
    CDElement* intersection;
    size_t nullCount = 0;
    size_t candidateCount = 0;
    size_t columnCount;
    if (basisCount != NULL) *basisCount = 0;
    if (success != NULL) *success = false;
    if (basisCount == NULL || success == NULL) return NULL;
    if (xCount == 0 || yCount == 0) {
        *success = true;
        return NULL;
    }
    if (xBasis == NULL || yBasis == NULL || !sameCDAlgebra(&xBasis[0], &yBasis[0])) return NULL;
    if (!checkedSizeAdd(xCount, yCount, &columnCount)) return NULL;

    // Build the equation Xc - Yd = 0 whose solutions identify common span elements
    xMatrix = matrixFromCDElements(xBasis, xCount);
    columns = calloc(columnCount, sizeof(Vector*));
    if (xMatrix == NULL || columns == NULL) {
        freeMatrix(xMatrix);
        free(columns);
        return NULL;
    }

    // Convert the x-basis and the negated y-basis into columns of one system
    for (size_t i = 0; i < xCount; i++) {
        columns[i] = cdToVector(&xBasis[i]);
        if (columns[i] == NULL) {
            freeMatrix(xMatrix);
            freeVectorBasis(columns, i);
            return NULL;
        }
    }
    for (size_t i = 0; i < yCount; i++) {
        Vector* column = cdToVector(&yBasis[i]);
        if (column == NULL) {
            freeMatrix(xMatrix);
            freeVectorBasis(columns, xCount + i);
            return NULL;
        }
        columns[xCount + i] = negativeVector(column);
        freeVector(column);
        if (columns[xCount + i] == NULL) {
            freeMatrix(xMatrix);
            freeVectorBasis(columns, xCount + i + 1);
            return NULL;
        }
    }

    // Assemble the block matrix whose kernel encodes equal span combinations
    equations = matrixFromColumns(columns, columnCount);
    freeVectorBasis(columns, columnCount);
    if (equations == NULL) {
        freeMatrix(xMatrix);
        return NULL;
    }

    // The nullspace stores coefficient pairs for equal linear combinations
    nullBasis = nullSpace(equations, &nullCount);
    freeMatrix(equations);
    if (nullBasis == NULL || nullCount == 0) {
        freeVectorBasis(nullBasis, nullCount);
        freeMatrix(xMatrix);
        *success = true;
        return NULL;
    }

    // Allocate the intersection candidates produced by nullspace relations
    candidates = calloc(nullCount, sizeof(CDElement));
    if (candidates == NULL) {
        freeVectorBasis(nullBasis, nullCount);
        freeMatrix(xMatrix);
        return NULL;
    }
    // Convert the x-side coefficients of each null vector back to CD elements
    for (size_t i = 0; i < nullCount; i++) {
        Vector* coeffs = constructVectorFromArrayOverField(xBasis[0].algebra->field,
                xCount, nullBasis[i]->data, xCount);
        Vector* image;
        if (coeffs == NULL) {
            freeVectorBasis(nullBasis, nullCount);
            freeMatrix(xMatrix);
            freeCDElementArray(candidates, i);
            return NULL;
        }
        image = applyMatrix(xMatrix, coeffs);
        freeVector(coeffs);
        if (image == NULL) {
            freeVectorBasis(nullBasis, nullCount);
            freeMatrix(xMatrix);
            freeCDElementArray(candidates, i);
            return NULL;
        }
        candidates[i] = cdFromVector(xBasis[0].algebra, image);
        freeVector(image);
        if (!cdElementIsValid(&candidates[i])) {
            freeVectorBasis(nullBasis, nullCount);
            freeMatrix(xMatrix);
            freeCDElementArray(candidates, i + 1);
            return NULL;
        }
        candidateCount++;
    }

    // Reduce the candidate elements to a basis for the intersection
    freeVectorBasis(nullBasis, nullCount);
    freeMatrix(xMatrix);
    intersection = cdReduceOwnedSpan(candidates, candidateCount, basisCount);
    if (intersection != NULL) *success = true;
    return intersection;
}

// Close a basis under Cayley-Dickson multiplication
static CDElement* cdCloseSubalgebraBasis(CDElement* basis, size_t* basisCount) {
    size_t count;
    if (basisCount == NULL) return NULL;
    count = *basisCount;

    // Repeat the closure step until the basis stabilizes
    while (basis != NULL && count > 0 && count < basis[0].algebra->dimension) {
        size_t productCount;
        size_t candidateCount;
        CDElement* candidates;
        CDElement* nextBasis;
        size_t nextCount = 0;
        size_t index = count;
        if (!checkedSizeMul(count, count, &productCount)
                || !checkedSizeAdd(count, productCount, &candidateCount)) {
            freeCDElementArray(basis, count);
            *basisCount = 0;
            return NULL;
        }
        candidates = calloc(candidateCount, sizeof(CDElement));
        if (candidates == NULL) {
            freeCDElementArray(basis, count);
            *basisCount = 0;
            return NULL;
        }

        // Keep the current basis and add all pairwise products
        for (size_t i = 0; i < count; i++) {
            candidates[i] = basis[i];
        }
        for (size_t i = 0; i < count; i++) {
            for (size_t j = 0; j < count; j++) {
                candidates[index] = cdMult(&basis[i], &basis[j]);
                if (!cdElementIsValid(&candidates[index])) {
                    for (size_t k = count; k <= index; k++) freeCDElement(&candidates[k]);
                    free(candidates);
                    freeCDElementArray(basis, count);
                    *basisCount = 0;
                    return NULL;
                }
                index++;
            }
        }

        // Reduce the candidate products to the next independent basis
        nextBasis = cdSpanBasis(candidates, candidateCount, &nextCount);
        for (size_t i = count; i < candidateCount; i++) freeCDElement(&candidates[i]);
        free(candidates);
        if (nextBasis == NULL || nextCount == 0) {
            freeCDElementArray(basis, count);
            *basisCount = 0;
            return NULL;
        }

        // Replace the current basis with the enlarged independent basis
        freeCDElementArray(basis, count);
        basis = nextBasis;
        // Stop once the closure step adds no new independent elements
        if (nextCount == count) {
            *basisCount = nextCount;
            return basis;
        }
        count = nextCount;
    }

    *basisCount = count;
    return basis;
}

// Close a basis under left or right multiplication by the whole algebra
static CDElement* cdCloseIdealBasis(CDElement* basis, size_t* basisCount, bool left) {
    CDElement* algebraBasis;
    CDAlgebra* algebra;
    size_t count;
    if (basisCount == NULL) return NULL;
    count = *basisCount;
    if (basis == NULL || count == 0) return NULL;

    // Build the full algebra basis used to close this ideal
    algebra = basis[0].algebra;
    algebraBasis = cdStandardBasis(algebra);
    if (algebraBasis == NULL) {
        freeCDElementArray(basis, count);
        *basisCount = 0;
        return NULL;
    }

    // Repeat the closure step until the basis stabilizes
    while (count < algebra->dimension) {
        size_t productCount;
        size_t candidateCount;
        CDElement* candidates;
        CDElement* nextBasis;
        size_t nextCount = 0;
        size_t index = count;
        if (!checkedSizeMul(algebra->dimension, count, &productCount)
                || !checkedSizeAdd(count, productCount, &candidateCount)) {
            freeCDElementArray(basis, count);
            freeCDElementArray(algebraBasis, algebra->dimension);
            *basisCount = 0;
            return NULL;
        }
        candidates = calloc(candidateCount, sizeof(CDElement));
        if (candidates == NULL) {
            freeCDElementArray(basis, count);
            freeCDElementArray(algebraBasis, algebra->dimension);
            *basisCount = 0;
            return NULL;
        }

        // Keep the current basis and add its products by the full algebra basis
        for (size_t i = 0; i < count; i++) {
            candidates[i] = basis[i];
        }
        for (size_t i = 0; i < algebra->dimension; i++) {
            for (size_t j = 0; j < count; j++) {
                candidates[index] = left ? cdMult(&algebraBasis[i], &basis[j])
                                         : cdMult(&basis[j], &algebraBasis[i]);
                if (!cdElementIsValid(&candidates[index])) {
                    for (size_t k = count; k <= index; k++) freeCDElement(&candidates[k]);
                    free(candidates);
                    freeCDElementArray(basis, count);
                    freeCDElementArray(algebraBasis, algebra->dimension);
                    *basisCount = 0;
                    return NULL;
                }
                index++;
            }
        }

        // Reduce the candidate products to the next independent basis
        nextBasis = cdSpanBasis(candidates, candidateCount, &nextCount);
        for (size_t i = count; i < candidateCount; i++) freeCDElement(&candidates[i]);
        free(candidates);
        if (nextBasis == NULL || nextCount == 0) {
            freeCDElementArray(basis, count);
            freeCDElementArray(algebraBasis, algebra->dimension);
            *basisCount = 0;
            return NULL;
        }

        // Replace the current basis with the enlarged independent basis
        freeCDElementArray(basis, count);
        basis = nextBasis;
        // Stop once the ideal closure adds no new independent elements
        if (nextCount == count) {
            freeCDElementArray(algebraBasis, algebra->dimension);
            *basisCount = nextCount;
            return basis;
        }
        count = nextCount;
    }

    // Release the algebra basis before returning the stabilized basis
    freeCDElementArray(algebraBasis, algebra->dimension);
    *basisCount = count;
    return basis;
}

/* ---------- Ideals and Subalgebras ---------- */

// Return a basis for the unital subalgebra generated by the given elements
static CDElement* cdGeneratedSubalgebra(CDElement* generators, size_t count, size_t* basisCount) {
    CDAlgebra* algebra;
    CDElement* candidates;
    CDElement* basis;
    size_t initialCount = 0;
    if (basisCount != NULL) *basisCount = 0;
    if (basisCount == NULL) return NULL;

    // Find the common algebra and add the unit to the generator list
    algebra = cdGeneratorAlgebra(generators, count);
    if (algebra == NULL) return NULL;
    if (count == SIZE_MAX) return NULL;
    candidates = calloc(count + 1, sizeof(CDElement));
    if (candidates == NULL) return NULL;
    candidates[0] = cdBasisElement(algebra, 0);
    if (!cdElementIsValid(&candidates[0])) {
        free(candidates);
        return NULL;
    }
    for (size_t i = 0; i < count; i++) {
        candidates[i + 1] = constructCDElement(algebra, generators[i].coeffs);
        if (!cdElementIsValid(&candidates[i + 1])) {
            freeCDElementArray(candidates, i + 2);
            return NULL;
        }
    }

    // Add the algebra unit to the generators and reduce to an initial basis
    basis = cdReduceOwnedSpan(candidates, count + 1, &initialCount);
    if (basis == NULL || initialCount == 0) return NULL;
    *basisCount = initialCount;
    return cdCloseSubalgebraBasis(basis, basisCount);
}

// Return a basis for the left ideal generated by the given elements
static CDElement* cdGeneratedLeftIdeal(CDElement* generators, size_t count, size_t* basisCount) {
    CDElement* basis;
    size_t initialCount = 0;
    if (basisCount != NULL) *basisCount = 0;
    if (basisCount == NULL || cdGeneratorAlgebra(generators, count) == NULL) return NULL;

    // Reduce the generators to an initial left-ideal basis before closing it
    basis = cdSpanBasis(generators, count, &initialCount);
    if (basis == NULL || initialCount == 0) return NULL;
    *basisCount = initialCount;
    return cdCloseIdealBasis(basis, basisCount, true);
}

// Return a basis for the right ideal generated by the given elements
static CDElement* cdGeneratedRightIdeal(CDElement* generators, size_t count, size_t* basisCount) {
    CDElement* basis;
    size_t initialCount = 0;
    if (basisCount != NULL) *basisCount = 0;
    if (basisCount == NULL || cdGeneratorAlgebra(generators, count) == NULL) return NULL;

    // Reduce the generators to an initial right-ideal basis before closing it
    basis = cdSpanBasis(generators, count, &initialCount);
    if (basis == NULL || initialCount == 0) return NULL;
    *basisCount = initialCount;
    return cdCloseIdealBasis(basis, basisCount, false);
}

// Close a basis under both left and right multiplication by the whole algebra
static CDElement* cdCloseTwoSidedIdealBasis(CDElement* basis, size_t* basisCount) {
    CDElement* algebraBasis;
    CDAlgebra* algebra;
    size_t count;
    if (basisCount == NULL) return NULL;
    count = *basisCount;
    if (basis == NULL || count == 0) return NULL;

    // Build the full algebra basis used to close the two-sided ideal
    algebra = basis[0].algebra;
    algebraBasis = cdStandardBasis(algebra);
    if (algebraBasis == NULL) {
        freeCDElementArray(basis, count);
        *basisCount = 0;
        return NULL;
    }

    // Repeat the closure step until the basis stabilizes
    while (count < algebra->dimension) {
        size_t oneSidedCount;
        size_t twoSidedCount;
        size_t candidateCount;
        CDElement* candidates;
        CDElement* nextBasis;
        size_t nextCount = 0;
        size_t index = count;
        if (!checkedSizeMul(algebra->dimension, count, &oneSidedCount)
                || !checkedSizeMul(2, oneSidedCount, &twoSidedCount)
                || !checkedSizeAdd(count, twoSidedCount, &candidateCount)) {
            freeCDElementArray(basis, count);
            freeCDElementArray(algebraBasis, algebra->dimension);
            *basisCount = 0;
            return NULL;
        }
        candidates = calloc(candidateCount, sizeof(CDElement));
        if (candidates == NULL) {
            freeCDElementArray(basis, count);
            freeCDElementArray(algebraBasis, algebra->dimension);
            *basisCount = 0;
            return NULL;
        }

        // Keep the current basis and add both left and right products by the algebra basis
        for (size_t i = 0; i < count; i++) {
            candidates[i] = basis[i];
        }
        for (size_t i = 0; i < algebra->dimension; i++) {
            for (size_t j = 0; j < count; j++) {
                candidates[index] = cdMult(&algebraBasis[i], &basis[j]);
                if (!cdElementIsValid(&candidates[index])) {
                    for (size_t k = count; k <= index; k++) freeCDElement(&candidates[k]);
                    free(candidates);
                    freeCDElementArray(basis, count);
                    freeCDElementArray(algebraBasis, algebra->dimension);
                    *basisCount = 0;
                    return NULL;
                }
                index++;
                candidates[index] = cdMult(&basis[j], &algebraBasis[i]);
                if (!cdElementIsValid(&candidates[index])) {
                    for (size_t k = count; k <= index; k++) freeCDElement(&candidates[k]);
                    free(candidates);
                    freeCDElementArray(basis, count);
                    freeCDElementArray(algebraBasis, algebra->dimension);
                    *basisCount = 0;
                    return NULL;
                }
                index++;
            }
        }

        // Reduce the candidate products to the next independent basis
        nextBasis = cdSpanBasis(candidates, candidateCount, &nextCount);
        for (size_t i = count; i < candidateCount; i++) freeCDElement(&candidates[i]);
        free(candidates);
        if (nextBasis == NULL || nextCount == 0) {
            freeCDElementArray(basis, count);
            freeCDElementArray(algebraBasis, algebra->dimension);
            *basisCount = 0;
            return NULL;
        }

        // Replace the current basis with the enlarged independent basis
        freeCDElementArray(basis, count);
        basis = nextBasis;
        // Stop once two-sided closure adds no new independent elements
        if (nextCount == count) {
            freeCDElementArray(algebraBasis, algebra->dimension);
            *basisCount = nextCount;
            return basis;
        }
        count = nextCount;
    }

    // Release the algebra basis before returning the stabilized basis
    freeCDElementArray(algebraBasis, algebra->dimension);
    *basisCount = count;
    return basis;
}

// Return a basis for the two-sided ideal generated by the given elements
static CDElement* cdGeneratedTwoSidedIdeal(CDElement* generators, size_t count, size_t* basisCount) {
    CDElement* basis;
    size_t initialCount = 0;
    if (basisCount != NULL) *basisCount = 0;
    if (basisCount == NULL || cdGeneratorAlgebra(generators, count) == NULL) return NULL;

    // Reduce the generators to an initial two-sided ideal basis before closing it
    basis = cdSpanBasis(generators, count, &initialCount);
    if (basis == NULL || initialCount == 0) return NULL;
    *basisCount = initialCount;
    return cdCloseTwoSidedIdealBasis(basis, basisCount);
}

// Return the ideal type of the sum/difference of two ideal types when defined
static bool combinedIdealType(IdealType x, IdealType y, IdealType* out) {
    if (out == NULL) return false;
    if (x == y) {
        *out = x;
        return true;
    }
    if ((x == LEFT_IDEAL && y == TWO_SIDED_IDEAL)
            || (x == TWO_SIDED_IDEAL && y == LEFT_IDEAL)) {
        *out = LEFT_IDEAL;
        return true;
    }
    if ((x == RIGHT_IDEAL && y == TWO_SIDED_IDEAL)
            || (x == TWO_SIDED_IDEAL && y == RIGHT_IDEAL)) {
        *out = RIGHT_IDEAL;
        return true;
    }
    return false;
}

// Wrap an owned ideal basis in a CDIdeal struct
static CDIdeal cdIdealFromOwnedBasis(CDAlgebra* algebra, CDElement* basis, size_t count,
        IdealType type) {
    CDIdeal ideal = {0};
    ideal.algebra = algebra;
    ideal.basis = basis;
    ideal.count = count;
    ideal.type = type;
    if (!cdIdealIsValid(&ideal)) {
        freeCDIdeal(&ideal);
        return (CDIdeal){0};
    }
    return ideal;
}

// Wrap an owned subalgebra basis in a CDSubalgebra struct
static CDSubalgebra cdSubalgebraFromOwnedBasis(CDAlgebra* algebra, CDElement* basis, size_t count) {
    CDSubalgebra subalgebra = {0};
    subalgebra.cdAlgebra = algebra;
    subalgebra.basis = basis;
    subalgebra.count = count;
    if (!cdSubalgebraIsValid(&subalgebra)) {
        freeCDSubalgebra(&subalgebra);
        return (CDSubalgebra){0};
    }
    return subalgebra;
}

// Construct an ideal of a given type from generators
static CDIdeal cdConstructIdealWithType(CDElement* generators, size_t count, IdealType type) {
    CDIdeal ideal = {0};
    CDAlgebra* algebra = cdGeneratorAlgebra(generators, count);
    CDElement* basis = NULL;
    size_t basisCount = 0;
    if (algebra == NULL) return ideal;

    // Generate the requested one-sided or two-sided ideal basis before wrapping it
    if (type == LEFT_IDEAL) {
        basis = cdGeneratedLeftIdeal(generators, count, &basisCount);
    }
    else if (type == RIGHT_IDEAL) {
        basis = cdGeneratedRightIdeal(generators, count, &basisCount);
    }
    else if (type == TWO_SIDED_IDEAL) {
        basis = cdGeneratedTwoSidedIdeal(generators, count, &basisCount);
    }
    else {
        return ideal;
    }
    return cdIdealFromOwnedBasis(algebra, basis, basisCount, type);
}

// Construct the smallest ideal of a given type containing an owned generator array
static CDIdeal cdIdealFromOwnedGenerators(CDElement* generators, size_t count, IdealType type) {
    CDIdeal ideal = cdConstructIdealWithType(generators, count, type);
    freeCDElementArray(generators, count);
    return ideal;
}

// Construct the left ideal generated by the given elements
CDIdeal cdConstructLeftIdeal(CDElement* generators, size_t count) {
    return cdConstructIdealWithType(generators, count, LEFT_IDEAL);
}

// Construct the right ideal generated by the given elements
CDIdeal cdConstructRightIdeal(CDElement* generators, size_t count) {
    return cdConstructIdealWithType(generators, count, RIGHT_IDEAL);
}

// Construct the two-sided ideal generated by the given elements
CDIdeal cdConstructTwoSidedIdeal(CDElement* generators, size_t count) {
    return cdConstructIdealWithType(generators, count, TWO_SIDED_IDEAL);
}

// Construct the unital subalgebra generated by the given elements
CDSubalgebra cdConstructSubalgebra(CDElement* generators, size_t count) {
    CDSubalgebra subalgebra = {0};
    CDAlgebra* algebra = cdGeneratorAlgebra(generators, count);
    CDElement* basis;
    size_t basisCount = 0;
    if (algebra == NULL) return subalgebra;

    // Generate the subalgebra basis before wrapping it
    basis = cdGeneratedSubalgebra(generators, count, &basisCount);
    return cdSubalgebraFromOwnedBasis(algebra, basis, basisCount);
}

// Construct an ideal from a spanning basis
CDIdeal cdIdealFromBasis(CDElement* basis, size_t count, IdealType type) {
    return cdConstructIdealWithType(basis, count, type);
}

// Construct a subalgebra from a spanning basis
CDSubalgebra cdSubalgebraFromBasis(CDElement* basis, size_t count) {
    return cdConstructSubalgebra(basis, count);
}

// Copy a Cayley-Dickson ideal
CDIdeal copyCDIdeal(CDIdeal* ideal) {
    CDIdeal copy = {0};
    if (!cdIdealIsValid(ideal)) return copy;
    copy.algebra = ideal->algebra;
    copy.count = ideal->count;
    copy.type = ideal->type;
    if (ideal->count == 0) return copy;
    copy.basis = copyCDElementArray(ideal->basis, ideal->count);
    if (!copy.basis) return (CDIdeal){0};
    if (!cdIdealIsValid(&copy)) {
        freeCDIdeal(&copy);
        return (CDIdeal){0};
    }
    return copy;
}

// Copy a Cayley-Dickson subalgebra
CDSubalgebra copyCDSubalgebra(CDSubalgebra* subalgebra) {
    CDSubalgebra copy = {0};
    if (!cdSubalgebraIsValid(subalgebra)) return copy;
    copy.cdAlgebra = subalgebra->cdAlgebra;
    copy.count = subalgebra->count;
    copy.basis = copyCDElementArray(subalgebra->basis, subalgebra->count);
    if (!copy.basis) return (CDSubalgebra){0};
    if (!cdSubalgebraIsValid(&copy)) {
        freeCDSubalgebra(&copy);
        return (CDSubalgebra){0};
    }
    return copy;
}

// Check whether a Cayley-Dickson element lies in an ideal
bool cdIdealContains(CDIdeal* ideal, CDElement* x) {
    if (!cdIdealIsValid(ideal) || !cdElementIsValid(x)
            || !cdAlgebraDataEq(ideal->algebra, x->algebra)) {
        return false;
    }
    return cdElementInSpan(x, ideal->basis, ideal->count);
}

// Check whether a Cayley-Dickson element lies in a subalgebra
bool cdSubalgebraContains(CDSubalgebra* subalgebra, CDElement* x) {
    if (!cdSubalgebraIsValid(subalgebra) || !cdElementIsValid(x)
            || !cdAlgebraDataEq(subalgebra->cdAlgebra, x->algebra)) {
        return false;
    }
    return cdElementInSpan(x, subalgebra->basis, subalgebra->count);
}

// Check whether two Cayley-Dickson ideals are equal as subspaces
bool cdIdealEq(CDIdeal* x, CDIdeal* y) {
    if (!cdIdealIsValid(x) || !cdIdealIsValid(y) || !cdAlgebraDataEq(x->algebra, y->algebra)) {
        return false;
    }

    // Check mutual containment of the two ideal bases
    for (size_t i = 0; i < x->count; i++) {
        if (!cdIdealContains(y, &x->basis[i])) return false;
    }
    for (size_t i = 0; i < y->count; i++) {
        if (!cdIdealContains(x, &y->basis[i])) return false;
    }
    return true;
}

// Check whether two Cayley-Dickson subalgebras are equal
bool cdSubalgebraEq(CDSubalgebra* x, CDSubalgebra* y) {
    if (!cdSubalgebraIsValid(x) || !cdSubalgebraIsValid(y)
            || !cdAlgebraDataEq(x->cdAlgebra, y->cdAlgebra)) {
        return false;
    }

    // Check mutual containment of the two subalgebra bases
    for (size_t i = 0; i < x->count; i++) {
        if (!cdSubalgebraContains(y, &x->basis[i])) return false;
    }
    for (size_t i = 0; i < y->count; i++) {
        if (!cdSubalgebraContains(x, &y->basis[i])) return false;
    }
    return true;
}

// Intersect two Cayley-Dickson ideals
CDIdeal cdIntersectIdeals(CDIdeal* x, CDIdeal* y) {
    CDIdeal intersection = {0};
    CDElement* basis;
    size_t basisCount = 0;
    IdealType type;
    bool success = false;
    if (!cdIdealIsValid(x) || !cdIdealIsValid(y) || !cdAlgebraDataEq(x->algebra, y->algebra)) {
        return intersection;
    }
    if (x->count == 0 || y->count == 0) {
        intersection.algebra = x->algebra;
        intersection.type = TWO_SIDED_IDEAL;
        return intersection;
    }

    // Intersect the two spans and wrap the resulting basis
    basis = cdIntersectSpans(x->basis, x->count, y->basis, y->count, &basisCount, &success);
    if (!success) return intersection;
    if (basisCount == 0) {
        intersection.algebra = x->algebra;
        intersection.type = TWO_SIDED_IDEAL;
        return intersection;
    }
    if (basis == NULL || !inferIdealTypeFromBasis(basis, basisCount, &type)) {
        freeCDElementArray(basis, basisCount);
        return (CDIdeal){0};
    }
    return cdIdealFromOwnedBasis(x->algebra, basis, basisCount, type);
}

// Intersect two Cayley-Dickson subalgebras
CDSubalgebra cdIntersectSubalgebras(CDSubalgebra* x, CDSubalgebra* y) {
    CDSubalgebra intersection = {0};
    CDElement* basis;
    size_t basisCount = 0;
    bool success = false;
    if (!cdSubalgebraIsValid(x) || !cdSubalgebraIsValid(y)
            || !cdAlgebraDataEq(x->cdAlgebra, y->cdAlgebra)) {
        return intersection;
    }

    // Intersect the two spans and wrap the resulting basis
    basis = cdIntersectSpans(x->basis, x->count, y->basis, y->count, &basisCount, &success);
    if (!success || basis == NULL || basisCount == 0) {
        freeCDElementArray(basis, basisCount);
        return (CDSubalgebra){0};
    }
    return cdSubalgebraFromOwnedBasis(x->cdAlgebra, basis, basisCount);
}

// Add two compatible Cayley-Dickson ideals
CDIdeal cdIdealAdd(CDIdeal* x, CDIdeal* y) {
    CDElement* generators;
    CDIdeal sum = {0};
    IdealType type;
    if (!cdIdealIsValid(x) || !cdIdealIsValid(y) || !cdAlgebraDataEq(x->algebra, y->algebra)
            || !combinedIdealType(x->type, y->type, &type)) {
        return sum;
    }

    // Count the generators needed to span the sum
    size_t generatorCount;
    if (!checkedSizeAdd(x->count, y->count, &generatorCount)) return sum;
    if (generatorCount == 0) {
        sum.algebra = x->algebra;
        sum.type = type;
        return sum;
    }

    // Allocate generator storage and copy the first ideal basis into it
    generators = calloc(generatorCount, sizeof(CDElement));
    if (generators == NULL && generatorCount > 0) return sum;
    if (x->count > 0) {
        CDElement* xCopies = copyCDElementArray(x->basis, x->count);
        if (xCopies == NULL) {
            free(generators);
            return (CDIdeal){0};
        }
        for (size_t i = 0; i < x->count; i++) generators[i] = xCopies[i];
        free(xCopies);
    }

    // Append copies of the second ideal basis before generating the summed ideal
    for (size_t i = 0; i < y->count; i++) {
        generators[x->count + i] = constructCDElement(y->algebra, y->basis[i].coeffs);
        if (!cdElementIsValid(&generators[x->count + i])) {
            freeCDElementArray(generators, x->count + i + 1);
            return (CDIdeal){0};
        }
    }
    return cdIdealFromOwnedGenerators(generators, generatorCount, type);
}

// Subtract two compatible Cayley-Dickson ideals
CDIdeal cdIdealSubtract(CDIdeal* x, CDIdeal* y) {
    CDElement* generators;
    CDIdeal diff = {0};
    IdealType type;
    if (!cdIdealIsValid(x) || !cdIdealIsValid(y) || !cdAlgebraDataEq(x->algebra, y->algebra)
            || !combinedIdealType(x->type, y->type, &type)) {
        return diff;
    }

    // Count the generators needed to span the difference
    size_t generatorCount;
    if (!checkedSizeAdd(x->count, y->count, &generatorCount)) return diff;
    if (generatorCount == 0) {
        diff.algebra = x->algebra;
        diff.type = type;
        return diff;
    }

    // Allocate generator storage and copy the first ideal basis into it
    generators = calloc(generatorCount, sizeof(CDElement));
    if (generators == NULL && generatorCount > 0) return diff;
    if (x->count > 0) {
        CDElement* xCopies = copyCDElementArray(x->basis, x->count);
        if (xCopies == NULL) {
            free(generators);
            return (CDIdeal){0};
        }
        for (size_t i = 0; i < x->count; i++) generators[i] = xCopies[i];
        free(xCopies);
    }

    // Append negated copies of the second ideal basis before generating the difference
    for (size_t i = 0; i < y->count; i++) {
        FieldElement minusOne = fieldElementFromInt(y->algebra->field, -1);
        CDElement neg = cdScalarMult(&y->basis[i], minusOne);
        freeFieldElement(&minusOne);
        if (!cdElementIsValid(&neg)) {
            freeCDElementArray(generators, x->count + i);
            return (CDIdeal){0};
        }
        generators[x->count + i] = neg;
    }
    return cdIdealFromOwnedGenerators(generators, generatorCount, type);
}

// Multiply an ideal by a scalar
CDIdeal cdIdealScalarMult(CDIdeal* ideal, FieldElement scalar) {
    CDElement* generators;
    CDIdeal product = {0};
    if (!cdIdealIsValid(ideal) || !fieldElementIsValid(&scalar)
            || !fieldEq(ideal->algebra->field, scalar.field)) {
        return product;
    }
    if (ideal->count == 0) {
        product.algebra = ideal->algebra;
        product.type = ideal->type;
        return product;
    }

    // Allocate generators and fill them by scaling each ideal basis element
    generators = calloc(ideal->count, sizeof(CDElement));
    if (generators == NULL) return product;
    for (size_t i = 0; i < ideal->count; i++) {
        generators[i] = cdScalarMult(&ideal->basis[i], scalar);
        if (!cdElementIsValid(&generators[i])) {
            freeCDElementArray(generators, i + 1);
            return (CDIdeal){0};
        }
    }
    return cdIdealFromOwnedGenerators(generators, ideal->count, ideal->type);
}

// Construct the ideal generated by left-multiplying an ideal by an element
CDIdeal cdIdealLeftMult(CDIdeal* ideal, CDElement* x) {
    CDElement* generators;
    CDIdeal product = {0};
    if (!cdIdealIsValid(ideal) || !cdElementIsValid(x)
            || !cdAlgebraDataEq(ideal->algebra, x->algebra)) {
        return product;
    }
    if (ideal->count == 0) {
        product.algebra = ideal->algebra;
        product.type = ideal->type;
        return product;
    }

    // Allocate generators and fill them by left-multiplying each ideal basis element
    generators = calloc(ideal->count, sizeof(CDElement));
    if (generators == NULL) return product;
    for (size_t i = 0; i < ideal->count; i++) {
        generators[i] = cdMult(x, &ideal->basis[i]);
        if (!cdElementIsValid(&generators[i])) {
            freeCDElementArray(generators, i + 1);
            return (CDIdeal){0};
        }
    }
    return cdIdealFromOwnedGenerators(generators, ideal->count, ideal->type);
}

// Construct the ideal generated by right-multiplying an ideal by an element
CDIdeal cdIdealRightMult(CDIdeal* ideal, CDElement* x) {
    CDElement* generators;
    CDIdeal product = {0};
    if (!cdIdealIsValid(ideal) || !cdElementIsValid(x)
            || !cdAlgebraDataEq(ideal->algebra, x->algebra)) {
        return product;
    }
    if (ideal->count == 0) {
        product.algebra = ideal->algebra;
        product.type = ideal->type;
        return product;
    }

    // Allocate generators and fill them by right-multiplying each ideal basis element
    generators = calloc(ideal->count, sizeof(CDElement));
    if (generators == NULL) return product;
    for (size_t i = 0; i < ideal->count; i++) {
        generators[i] = cdMult(&ideal->basis[i], x);
        if (!cdElementIsValid(&generators[i])) {
            freeCDElementArray(generators, i + 1);
            return (CDIdeal){0};
        }
    }
    return cdIdealFromOwnedGenerators(generators, ideal->count, ideal->type);
}

// Check whether a field is a quadratic extension of a given base field
static bool isQuadraticExtensionOfField(Field* field, Field* baseField) {
    return field != NULL && baseField != NULL
        && ((field->type == NF && field->data.nf.baseField == baseField
                && field->data.nf.gen.degree == 2)
            || (field->type == FF_QEXT && field->data.ffqext.baseField == baseField));
}

// Check whether two quaternion algebras have the same defining data
static bool sameQuaternionAlgebraData(QuaternionAlgebra* x, QuaternionAlgebra* y) {
    return quaternionAlgebraIsValid(x) && quaternionAlgebraIsValid(y)
        && fieldEq(x->field, y->field)
        && eqFieldElements(x->params[0], y->params[0])
        && eqFieldElements(x->params[1], y->params[1]);
}

/* ----------- Quaternion methods ----------- */

// Construct a QuaternionAlgebra
QuaternionAlgebra constructQuaternionAlgebra(Field* field, FieldElement a, FieldElement b) {
    FieldElement params[2] = {a, b};
    QuaternionAlgebra algebra = constructCDAlgebra(field, 2, params);

    // Reject any Cayley-Dickson result that is not exactly quaternionic
    if (!quaternionAlgebraIsValid(&algebra)) {
        freeQuaternionAlgebra(&algebra);
        return (QuaternionAlgebra){0};
    }
    return algebra;
}

// Construct a Quaternion
Quaternion constructQuaternion(QuaternionAlgebra* algebra, FieldElement c0, FieldElement c1,
        FieldElement c2, FieldElement c3) {
    FieldElement coeffs[4] = {c0, c1, c2, c3};
    Quaternion q = {0};

    // Build the Cayley-Dickson element only after validating the algebra
    if (!quaternionAlgebraIsValid(algebra)) return q;
    q = constructCDElement(algebra, coeffs);
    if (!quaternionIsValid(&q)) {
        freeQuaternion(&q);
        return (Quaternion){0};
    }
    return q;
}

// Free a QuaternionAlgebra
void freeQuaternionAlgebra(QuaternionAlgebra* algebra) {
    freeCDAlgebra(algebra);
}

// Free a Quaternion
void freeQuaternion(Quaternion* x) {
    freeCDElement(x);
}

// Check whether a QuaternionAlgebra is valid
bool quaternionAlgebraIsValid(QuaternionAlgebra* algebra) {
    return cdAlgebraIsValid(algebra) && algebra->degree == 2 && algebra->dimension == 4;
}

// Check whether a Quaternion is valid
bool quaternionIsValid(Quaternion* x) {
    return cdElementIsValid(x) && x->algebra->degree == 2 && x->dimension == 4;
}

// Construct the scalar-field data for the standard 2x2 matrix representation
QuaternionMatrixRep constructQuaternionMatrixRep(QuaternionAlgebra* algebra) {
    QuaternionMatrixRep rep = {0};
    Field extension = {0};
    FieldElement embeddedA = {0};
    if (!quaternionAlgebraIsValid(algebra)) return rep;

    // Construct or borrow a field containing a square root of the first parameter
    rep.algebra = algebra;
    extension = constructQuadraticExtensionField(algebra->field, algebra->params[0], "a");
    if (!fieldEq(&extension, algebra->field)) {
        rep.extField = malloc(sizeof(Field));
        if (rep.extField == NULL) {
            freeField(&extension);
            return (QuaternionMatrixRep){0};
        }
        *rep.extField = extension;
        rep.ownsExtField = true;
    }
    else {
        freeField(&extension);
        rep.extField = algebra->field;
        rep.ownsExtField = false;
    }

    // Store the actual square root used in the matrix representation
    if (isQuadraticExtensionOfField(rep.extField, algebra->field)) {
        rep.sqrt_a = quadraticExtensionGenerator(rep.extField);
    }
    else {
        embeddedA = embedFieldElement(rep.extField, algebra->params[0]);
        if (!fieldElementIsValid(&embeddedA)) {
            freeQuaternionMatrixRep(&rep);
            return (QuaternionMatrixRep){0};
        }
        rep.sqrt_a = fieldElementSquareRoot(embeddedA);
        freeFieldElement(&embeddedA);
    }

    // Validate the representation data before returning it
    if (rep.extField == NULL || !fieldElementIsValid(&rep.sqrt_a)
            || !fieldEq(rep.extField, rep.sqrt_a.field)) {
        freeQuaternionMatrixRep(&rep);
        return (QuaternionMatrixRep){0};
    }
    return rep;
}

// Free a QuaternionMatrixRep
void freeQuaternionMatrixRep(QuaternionMatrixRep* rep) {
    if (rep == NULL) return;

    // Release the optional extension field and clear the wrapper
    freeFieldElement(&rep->sqrt_a);
    if (rep->ownsExtField) {
        freeField(rep->extField);
        free(rep->extField);
    }
    rep->algebra = NULL;
    rep->extField = NULL;
    rep->ownsExtField = false;
}

// Convert a quaternion to its standard 2x2 matrix representation
Matrix* quaternionToMatrix(Quaternion* q, QuaternionMatrixRep* rep) {
    Matrix* matrix;
    FieldElement entries[4] = {0};
    FieldElement zSqrtA = {0};
    FieldElement yPlusZSqrtA = {0};
    FieldElement xSqrtA = {0};
    FieldElement tPlusXSqrtA = {0};
    FieldElement tMinusXSqrtA = {0};
    FieldElement yMinusZSqrtA = {0};
    FieldElement embeddedB = {0};
    FieldElement bTimesYPlusZSqrtA = {0};
    if (!quaternionIsValid(q) || rep == NULL || rep->extField == NULL
            || !fieldElementIsValid(&rep->sqrt_a)
            || !sameQuaternionAlgebraData(q->algebra, rep->algebra)) {
        return NULL;
    }

    // Embed the quaternion coordinates and second parameter into the representation field
    entries[0] = embedFieldElement(rep->extField, q->coeffs[0]);
    entries[1] = embedFieldElement(rep->extField, q->coeffs[1]);
    entries[2] = embedFieldElement(rep->extField, q->coeffs[2]);
    entries[3] = embedFieldElement(rep->extField, q->coeffs[3]);
    embeddedB = embedFieldElement(rep->extField, q->algebra->params[1]);
    if (!fieldElementIsValid(&entries[0]) || !fieldElementIsValid(&entries[1])
            || !fieldElementIsValid(&entries[2]) || !fieldElementIsValid(&entries[3])
            || !fieldElementIsValid(&embeddedB)) {
        freeFieldElement(&entries[0]);
        freeFieldElement(&entries[1]);
        freeFieldElement(&entries[2]);
        freeFieldElement(&entries[3]);
        freeFieldElement(&embeddedB);
        return NULL;
    }

    // Compute the four matrix entries from the quaternion coordinates
    zSqrtA = multiplyFieldElements(entries[3], rep->sqrt_a);
    yPlusZSqrtA = addFieldElements(entries[2], zSqrtA);
    xSqrtA = multiplyFieldElements(entries[1], rep->sqrt_a);
    tPlusXSqrtA = addFieldElements(entries[0], xSqrtA);
    tMinusXSqrtA = subtractFieldElements(entries[0], xSqrtA);
    yMinusZSqrtA = subtractFieldElements(entries[2], zSqrtA);
    bTimesYPlusZSqrtA = multiplyFieldElements(embeddedB, yPlusZSqrtA);
    if (!fieldElementIsValid(&zSqrtA) || !fieldElementIsValid(&yPlusZSqrtA)
            || !fieldElementIsValid(&xSqrtA) || !fieldElementIsValid(&tPlusXSqrtA)
            || !fieldElementIsValid(&tMinusXSqrtA) || !fieldElementIsValid(&yMinusZSqrtA)
            || !fieldElementIsValid(&bTimesYPlusZSqrtA)) {
        freeFieldElement(&entries[0]);
        freeFieldElement(&entries[1]);
        freeFieldElement(&entries[2]);
        freeFieldElement(&entries[3]);
        freeFieldElement(&embeddedB);
        freeFieldElement(&zSqrtA);
        freeFieldElement(&yPlusZSqrtA);
        freeFieldElement(&xSqrtA);
        freeFieldElement(&tPlusXSqrtA);
        freeFieldElement(&tMinusXSqrtA);
        freeFieldElement(&yMinusZSqrtA);
        freeFieldElement(&bTimesYPlusZSqrtA);
        return NULL;
    }

    // Release the embedded source coordinates before reusing the entry array
    freeFieldElement(&entries[0]);
    freeFieldElement(&entries[1]);
    freeFieldElement(&entries[2]);
    freeFieldElement(&entries[3]);
    freeFieldElement(&embeddedB);

    // Transfer the computed entries into a 2x2 matrix and free temporaries
    entries[0] = tPlusXSqrtA;
    entries[1] = bTimesYPlusZSqrtA;
    entries[2] = yMinusZSqrtA;
    entries[3] = tMinusXSqrtA;
    matrix = constructMatrixFromArrayOverField(rep->extField, 2, 2, entries, 4);
    freeFieldElement(&entries[0]);
    freeFieldElement(&entries[1]);
    freeFieldElement(&entries[2]);
    freeFieldElement(&entries[3]);
    freeFieldElement(&zSqrtA);
    freeFieldElement(&yPlusZSqrtA);
    freeFieldElement(&xSqrtA);
    return matrix;
}

/* ----------- Octonion methods ----------- */

// Construct an OctonionAlgebra
OctonionAlgebra constructOctonionAlgebra(Field* field, FieldElement a,
        FieldElement b, FieldElement c) {
    FieldElement params[3] = {a, b, c};
    OctonionAlgebra algebra = constructCDAlgebra(field, 3, params);

    // Reject any Cayley-Dickson result that is not exactly octonionic
    if (!octonionAlgebraIsValid(&algebra)) {
        freeOctonionAlgebra(&algebra);
        return (OctonionAlgebra){0};
    }
    return algebra;
}

// Construct an Octonion
Octonion constructOctonion(OctonionAlgebra* algebra, FieldElement c0, FieldElement c1,
        FieldElement c2, FieldElement c3, FieldElement c4, FieldElement c5,
        FieldElement c6, FieldElement c7) {
    FieldElement coeffs[8] = {c0, c1, c2, c3, c4, c5, c6, c7};
    Octonion x = {0};

    // Build the Cayley-Dickson element only after validating the algebra
    if (!octonionAlgebraIsValid(algebra)) return x;
    x = constructCDElement(algebra, coeffs);
    if (!octonionIsValid(&x)) {
        freeOctonion(&x);
        return (Octonion){0};
    }
    return x;
}

// Free an OctonionAlgebra
void freeOctonionAlgebra(OctonionAlgebra* algebra) {
    freeCDAlgebra(algebra);
}

// Free an Octonion
void freeOctonion(Octonion* x) {
    freeCDElement(x);
}

// Check whether an OctonionAlgebra is valid
bool octonionAlgebraIsValid(OctonionAlgebra* algebra) {
    return cdAlgebraIsValid(algebra) && algebra->degree == 3 && algebra->dimension == 8;
}

// Check whether an Octonion is valid
bool octonionIsValid(Octonion* x) {
    return cdElementIsValid(x) && x->algebra->degree == 3 && x->dimension == 8;
}
