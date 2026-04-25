#ifndef HEBI_H
#define HEBI_H

#include <stdbool.h>

/* --------- Definitions of structs ---------- */

typedef struct Fraction {
    long long num;
    long long denom;
} Fraction;

typedef struct ComplexNumber {
    double real;
    double imag;
} ComplexNumber;

/* ---------- Helper methods ---------- */
int comp(const void* a, const void* b);

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
double complexAbs(ComplexNumber a);
double complexArg(ComplexNumber a);
ComplexNumber complexSqrt(ComplexNumber a);
ComplexNumber complexCbrt(ComplexNumber a);
bool complexEq(ComplexNumber a, ComplexNumber b, double tol);

/* ---------- Transcendental constants ---------- */
long double pi(void);
long double e(void);
long double phi(void);

#endif
