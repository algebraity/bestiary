#ifndef HEBI_H
#define HEBI_H

#include <stdbool.h>
#include <stddef.h>

/* --------- Definitions of structs ---------- */

typedef struct Fraction Fraction;
typedef struct ComplexNumber ComplexNumber;
typedef struct Number Number;
typedef struct Field Field;
typedef struct FieldElement FieldElement;
typedef struct NFGen NFGen;

typedef enum {
    NUMBER_INT,
    NUMBER_FRACTION,
    NUMBER_REAL,
    NUMBER_COMPLEX,
    NUMBER_NAN
} NumberType;

typedef enum {
    QQ,
    RR,
    CC,
    NF,     // characteristic-0 algebraic extension by one generator
    FF,     // finite field
    FF_QEXT // quadratic ext of a finite field
} FieldType;

struct Fraction {
    long long num;
    long long denom;
};

struct ComplexNumber {
    long double real;
    long double imag;
};

struct Number {
    NumberType type;
    union {
        long long i;
        Fraction frac;
        long double x;
        ComplexNumber z;
    } as;
};

struct NFGen {
    char* repr;
    size_t degree;
    FieldElement* minPolyCoeffs;
};

struct Field {
    char* repr;
    long long chr;
    FieldType type;
    union {
        // data of a number field
        struct {
            Field* baseField;
            NFGen gen; // one gen over baseField
        } nf;

        // data of a finite field
        struct {
            long long p;
            size_t degree;
            long long* modulus;
        } ff;

        // data of a quadratic ff extension
        struct {
            Field* baseField;
            FieldElement* radicand;
            char* genRepr;
        } ffqext;
    } data;
};

struct FieldElement {
    Field* field;
    union {
        Fraction frac;
        long double real;
        ComplexNumber z;

        // finite field case
        struct {
            long long* coeffs;
        } ffData;

        // number field case
        struct {
            FieldElement* coeffs; // length field->data.nf.gen.degree, coefficients in baseField
        } nfData;

        // quadratic finite field extension case
        struct {
            FieldElement* coeffs; // length 2, coefficients in baseField
        } ffqextData;
    } value;
};

/* ---------- Helper methods ---------- */
int comp(const void* a, const void* b);

/* ---------- Fractions ---------- */
Fraction zeroFraction(void);
Fraction oneFraction(void);
Fraction constructFraction(long long num, long long denom);
Fraction negateFraction(Fraction x);
Fraction addFractions(Fraction p, Fraction q);
Fraction subtractFractions(Fraction p, Fraction q);
Fraction multiplyFractions(Fraction p, Fraction q);
Fraction divideFractions(Fraction p, Fraction q);
bool isZeroFraction(Fraction x);
bool eqFraction(Fraction p, Fraction q);
int compFractions(Fraction p, Fraction q);
char* fractionToString(Fraction frac);
void printFraction(Fraction frac);

/* ---------- Complex arithmetic ---------- */
ComplexNumber complexAdd(ComplexNumber a, ComplexNumber b);
ComplexNumber complexSub(ComplexNumber a, ComplexNumber b);
ComplexNumber complexMul(ComplexNumber a, ComplexNumber b);
ComplexNumber complexDiv(ComplexNumber a, ComplexNumber b);
ComplexNumber complexNeg(ComplexNumber a);
ComplexNumber complexConj(ComplexNumber a);
long double complexAbs(ComplexNumber a);
long double complexArg(ComplexNumber a);
ComplexNumber complexExp(ComplexNumber a);
ComplexNumber complexLog(ComplexNumber a);
ComplexNumber complexSin(ComplexNumber a);
ComplexNumber complexCos(ComplexNumber a);
ComplexNumber complexTan(ComplexNumber a);
ComplexNumber complexAsin(ComplexNumber a);
ComplexNumber complexAcos(ComplexNumber a);
ComplexNumber complexAtan(ComplexNumber a);
ComplexNumber complexSqrt(ComplexNumber a);
ComplexNumber complexCbrt(ComplexNumber a);
bool complexEq(ComplexNumber a, ComplexNumber b, long double tol);

/* ---------- Special functions ---------- */
long double realErf(long double x);
long double realEi(long double x);
ComplexNumber complexErf(ComplexNumber z);
ComplexNumber complexEi(ComplexNumber z);

/* ---------- Number arithmetic ---------- */
Number constructNumberFromInt(long long i);
Number constructNumberFromDouble(long double x);
Number constructNumberFromFraction(Fraction frac);
Number constructNumberFromComplex(ComplexNumber z);
Number addNumbers(Number a, Number b);
Number subNumbers(Number a, Number b);
Number multNumbers(Number a, Number b);
Number divNumbers(Number a, Number b);
long long compNumbers(Number a, Number b);
bool eqNumbers(Number a, Number b);
void freeNumber(Number x);
void printNumber(Number x);

/* ---------- Randomness ---------- */
void seedPRG(unsigned long long seed);
void resetPRG(void);
unsigned long long stepPRG(void);
long long randomInt(long long min, long long max);
long double randomReal(long double min, long double max);
Fraction randomFraction(long long minNum, long long maxNum, long long minDenom, long long maxDenom);
ComplexNumber randomComplexComp(long double minReal, long double maxReal, long double minImag, long double maxImag);
ComplexNumber randomComplexMod(long double minMod, long double maxMod);
ComplexNumber randomComplex(long double minReal, long double maxReal, long double minImag, long double maxImag);

/* ---------- Polynomial solvers ---------- */
size_t solveQuadratic(ComplexNumber a, ComplexNumber b, ComplexNumber c, ComplexNumber roots[2]);
size_t solveCubic(ComplexNumber a, ComplexNumber b, ComplexNumber c, ComplexNumber d, ComplexNumber roots[3]);
size_t solveQuartic(ComplexNumber a, ComplexNumber b, ComplexNumber c, ComplexNumber d, ComplexNumber e, ComplexNumber roots[4]);

/* ---------- Transcendental constants ---------- */
long double pi(void);
long double e(void);
long double phi(void);

/* ---------- Field construct methods ---------- */
Field constructQQField(void);
Field constructRRField(void);
Field constructCCField(void);
Field constructFFField(long long p, size_t degree, long long* modulus);
Field constructQuadraticExtensionField(Field* baseField, FieldElement radicand,
        const char* genRepr);
FieldElement constructQQElement(Field* field, Fraction value);
FieldElement constructRRElement(Field* field, long double value);
FieldElement constructCCElement(Field* field, ComplexNumber value);
FieldElement constructFFElement(Field* field, long long* coeffs);
FieldElement constructNFElement(Field* field, FieldElement* coeffs, size_t coeffLen);
FieldElement fieldElementFromInt(Field* field, long long n);
FieldElement fieldElementFromFraction(Field* field, Fraction q);
FieldElement fieldElementFromDouble(Field* field, long double x);
FieldElement fieldElementFromComplex(Field* field, ComplexNumber z);

/* ---------- Field free methods ---------- */
void freeField(Field* field);
void freeFieldElement(FieldElement* element);

/* ----------- Field print methods ---------- */
char* fieldToString(Field* field);
void printFieldElement(FieldElement x);
char* fieldElementToString(FieldElement x);

/* ----------- Field bool methods ---------- */
bool fieldEq(Field* a, Field* b);
bool eqFieldElements(FieldElement x, FieldElement y);
bool fieldElementIsValid(FieldElement* x);
bool sameField(FieldElement* x, FieldElement* y);
bool fieldElementIsZero(FieldElement x);
bool fieldElementIsOne(FieldElement x);
bool fieldElementIsUnit(FieldElement x);

/* ----------- Field helpers ----------- */
Field copyField(Field* field);
FieldElement copyFieldElement(FieldElement x);
FieldElement copyFieldElementToField(Field* field, FieldElement x);
FieldElement normalizeFieldElement(FieldElement x);
FieldElement zeroFieldElement(Field* field);
FieldElement oneFieldElement(Field* field);
FieldElement fieldElementSquareRoot(FieldElement x);
FieldElement embedFieldElement(Field* targetField, FieldElement x);
FieldElement quadraticExtensionGenerator(Field* field);

/* ----------- Field arithmetic ---------- */
FieldElement addFieldElements(FieldElement x, FieldElement y);
FieldElement subtractFieldElements(FieldElement x, FieldElement y);
FieldElement negateFieldElement(FieldElement x);
FieldElement multiplyFieldElements(FieldElement x, FieldElement y);
FieldElement invertFieldElement(FieldElement x);
FieldElement divideFieldElements(FieldElement x, FieldElement y);
#endif
