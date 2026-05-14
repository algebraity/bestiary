#ifndef QUATERNIONIC_H
#define QUATERNIONIC_H

#include <stdbool.h>
#include <stddef.h>
#include "hebi.h"
#include "sokko.h"

/* ---------- Definitions of structs ---------- */

typedef struct CDAlgebra CDAlgebra;
typedef struct CDElement CDElement;
typedef struct CDIdeal CDIdeal;
typedef struct CDSubalgebra CDSubalgebra;
typedef struct CDElement Quaternion;
typedef struct CDAlgebra QuaternionAlgebra;
typedef struct QuaternionMatrixRep QuaternionMatrixRep;
typedef struct CDElement Octonion;
typedef struct CDAlgebra OctonionAlgebra;

typedef enum {
    LEFT_IDEAL,
    RIGHT_IDEAL,
    TWO_SIDED_IDEAL
} IdealType;

struct CDAlgebra {
    Field* field;
    size_t degree; // 0 = base field, 1 = quadratic, 2 = quaternion, etc
    size_t dimension; // dimension as a vector space over the base field
    FieldElement* params; // squares of the new units introduced at each doubling
};

struct CDElement {
    CDAlgebra* algebra;
    size_t dimension;
    FieldElement* coeffs; // coefficients in recursive Cayley-Dickson order
};

struct CDIdeal {
    CDAlgebra* algebra;
    CDElement* basis; // linearly independent spanning set of ideal over base field
    size_t count;
    IdealType type;
};

struct CDSubalgebra {
    CDAlgebra* cdAlgebra;
    CDElement* basis;
    size_t count;
};

struct QuaternionMatrixRep {
    QuaternionAlgebra* algebra;
    Field* extField;
    bool ownsExtField; // false if extField == algebra->field
    FieldElement sqrt_a;
};

/* ----------- Construct methods ----------- */
CDAlgebra constructCDAlgebra(Field* field, size_t degree, FieldElement* params);
CDElement constructCDElement(CDAlgebra* algebra, FieldElement* coeffs);
CDAlgebra copyCDAlgebra(CDAlgebra* algebra);
CDElement copyCDElement(CDElement* x);
CDIdeal copyCDIdeal(CDIdeal* ideal);
CDSubalgebra copyCDSubalgebra(CDSubalgebra* subalgebra);

/* ----------- Free methods ----------- */
void freeCDAlgebra(CDAlgebra* algebra);
void freeCDElement(CDElement* x);
void freeCDIdeal(CDIdeal* ideal);
void freeCDSubalgebra(CDSubalgebra* subalgebra);

/* ----------- Print methods ----------- */
char* cdAlgebraToString(CDAlgebra* algebra);
char* cdElementToString(CDElement* x);
char* cdIdealToString(CDIdeal* ideal);
char* cdSubalgebraToString(CDSubalgebra* subalgebra);

/* ----------- Bool methods ----------- */
bool cdAlgebraIsValid(CDAlgebra* algebra);
bool cdElementIsValid(CDElement* x);
bool cdIdealIsValid(CDIdeal* ideal);
bool cdSubalgebraIsValid(CDSubalgebra* subalgebra);
bool cdAlgebraEq(CDAlgebra* x, CDAlgebra* y);
bool sameCDAlgebra(CDElement* x, CDElement* y);
bool cdEq(CDElement* x, CDElement* y);

/* ---------- CDElement arithmetic ---------- */
CDElement cdAdd(CDElement* x, CDElement* y);
CDElement cdSubtract(CDElement* x, CDElement* y);
CDElement cdMult(CDElement* x, CDElement* y);
CDElement cdCommutator(CDElement* x, CDElement* y);
CDElement cdAssociator(CDElement* x, CDElement* y, CDElement* z);
CDElement cdLeftDivide(CDElement* x, CDElement* y);
CDElement cdRightDivide(CDElement* x, CDElement* y);
CDElement cdInverse(CDElement* x);
CDElement cdConjugate(CDElement* x);
FieldElement cdNorm(CDElement* x);
CDElement cdScalarMult(CDElement* x, FieldElement scalar);

/* ---------- Linear algebra ---------- */
CDElement cdBasisElement(CDAlgebra* algebra, size_t index);
CDElement* cdStandardBasis(CDAlgebra* algebra);
Vector* cdToVector(CDElement* x);
CDElement cdFromVector(CDAlgebra* algebra, Vector* vector);
Matrix* cdLeftMultMatrix(CDElement* x);
Matrix* cdRightMultMatrix(CDElement* x);
Matrix* cdCommutatorMatrix(CDElement* x);
Matrix* cdAssociatorMatrix(CDElement* x, CDElement* y);
CDElement* cdSpanBasis(CDElement* elements, size_t count, size_t* basisCount);
bool cdElementInSpan(CDElement* x, CDElement* basis, size_t count);
bool cdIsLeftZeroDivisor(CDElement* x);
bool cdIsRightZeroDivisor(CDElement* x);
CDElement* cdLeftAnnihilator(CDElement* x, size_t* basisCount);
CDElement* cdRightAnnihilator(CDElement* x, size_t* basisCount);
CDElement* cdCenter(CDAlgebra* algebra, size_t* basisCount);
CDElement* cdNucleus(CDAlgebra* algebra, size_t* basisCount);

/* ---------- Ideals and Subalgebras ---------- */
CDIdeal cdConstructLeftIdeal(CDElement* generators, size_t count);
CDIdeal cdConstructRightIdeal(CDElement* generators, size_t count);
CDIdeal cdConstructTwoSidedIdeal(CDElement* generators, size_t count);
CDSubalgebra cdConstructSubalgebra(CDElement* generators, size_t count);
CDIdeal cdIdealFromBasis(CDElement* basis, size_t count, IdealType type);
CDSubalgebra cdSubalgebraFromBasis(CDElement* basis, size_t count);
bool cdIdealContains(CDIdeal* ideal, CDElement* x);
bool cdSubalgebraContains(CDSubalgebra* subalgebra, CDElement* x);
bool cdIdealEq(CDIdeal* x, CDIdeal* y);
bool cdSubalgebraEq(CDSubalgebra* x, CDSubalgebra* y);
CDIdeal cdIntersectIdeals(CDIdeal* x, CDIdeal* y);
CDSubalgebra cdIntersectSubalgebras(CDSubalgebra* x, CDSubalgebra* y);
CDIdeal cdIdealAdd(CDIdeal* x, CDIdeal* y);
CDIdeal cdIdealSubtract(CDIdeal* x, CDIdeal* y);
CDIdeal cdIdealScalarMult(CDIdeal* ideal, FieldElement scalar);
CDIdeal cdIdealLeftMult(CDIdeal* ideal, CDElement* x);
CDIdeal cdIdealRightMult(CDIdeal* ideal, CDElement* x);

/* ---------- Quaternion methods ---------- */
QuaternionAlgebra constructQuaternionAlgebra(Field* field, FieldElement a, FieldElement b);
Quaternion constructQuaternion(QuaternionAlgebra* algebra, FieldElement c0, FieldElement c1,
        FieldElement c2, FieldElement c3);
void freeQuaternionAlgebra(QuaternionAlgebra* algebra);
void freeQuaternion(Quaternion* x);
bool quaternionAlgebraIsValid(QuaternionAlgebra* algebra);
bool quaternionIsValid(Quaternion* x);
QuaternionMatrixRep constructQuaternionMatrixRep(QuaternionAlgebra* algebra);
void freeQuaternionMatrixRep(QuaternionMatrixRep* rep);
Matrix* quaternionToMatrix(Quaternion* q, QuaternionMatrixRep* rep);

/* ---------- Octonion methods ---------- */
OctonionAlgebra constructOctonionAlgebra(Field* field, FieldElement a,
        FieldElement b, FieldElement c);
Octonion constructOctonion(OctonionAlgebra* algebra, FieldElement c0, FieldElement c1,
        FieldElement c2, FieldElement c3, FieldElement c4, FieldElement c5,
        FieldElement c6, FieldElement c7);
void freeOctonionAlgebra(OctonionAlgebra* algebra);
void freeOctonion(Octonion* x);
bool octonionAlgebraIsValid(OctonionAlgebra* algebra);
bool octonionIsValid(Octonion* x);

#endif
