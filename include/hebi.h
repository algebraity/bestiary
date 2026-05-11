#ifndef HEBI_H
#define HEBI_H

#include <stdbool.h>

/* --------- Definitions of structs ---------- */

typedef enum {
    NUMBER_INT,
    NUMBER_FRACTION,
    NUMBER_REAL,
    NUMBER_COMPLEX,
    NUMBER_NAN
} NumberType;

typedef struct Fraction {
    long long num;
    long long denom;
} Fraction;

typedef struct ComplexNumber {
    long double real;
    long double imag;
} ComplexNumber;

typedef struct Number {
    NumberType type;
    union {
        long long i;
        Fraction frac;
        long double x;
        ComplexNumber z;
    } as;
} Number;

/* ---------- Helper methods ---------- */
int comp(const void* a, const void* b);
bool isValidStatsNumber(Number x);

/* ---------- Fractions ---------- */
Fraction constructFraction(long long num, long long denom);
Fraction addFractions(Fraction p, Fraction q);
Fraction subtractFractions(Fraction p, Fraction q);
Fraction multiplyFractions(Fraction p, Fraction q);
Fraction divideFractions(Fraction p, Fraction q);
int compFractions(Fraction p, Fraction q);
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

/* ---------- Transcendental constants ---------- */
long double pi(void);
long double e(void);
long double phi(void);

#endif
