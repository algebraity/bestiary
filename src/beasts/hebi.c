#include<stdlib.h>
#include<stdio.h>
#include<math.h>
#include<complex.h>
#include "hebi.h"

/* ---------- Helper methods ---------- */

// comp method for qsort
int comp(const void* a, const void* b) {
    return (*(long long*)a > *(long long*)b) - (*(long long*)a < *(long long*)b);
}

/* ---------- Fractions ---------- */

// Construct a Fraction with a given num and denom
Fraction constructFraction(long long num, long long denom) {
    if (denom == 0) return (Fraction){0, 0}; // Invalid fraction
    Fraction frac;

    long long g = llabs(num), b = llabs(denom);
    while (b) {
	long long t = b;
	b = g % b;
	g = t;
    }
    if (g == 0) g = 1;

    frac.num = num / g;
    frac.denom = denom / g;
    if (frac.denom < 0) {
	frac.num = -frac.num;
	frac.denom = -frac.denom;
    }
    return frac;
}

// Add two Fractions
Fraction addFractions(Fraction p, Fraction q) {
    return constructFraction(p.num * q.denom + q.num * p.denom, p.denom * q.denom);
}

// Subtract two Fractions
Fraction subtractFractions(Fraction p, Fraction q) {
    return constructFraction(p.num * q.denom - q.num * p.denom, p.denom * q.denom);
}

// Multiply two Fractions
Fraction multiplyFractions(Fraction p, Fraction q) {
    return constructFraction(p.num * q.num, p.denom * q.denom);
}

// Divide two Fractions
Fraction divideFractions(Fraction p, Fraction q) {
    return constructFraction(p.num * q.denom, p.denom * q.num);
}

// Compare Fractions
int compFractions(Fraction p, Fraction q) {
    long long n1 = p.num;
    long long d1 = p.denom;
    long long n2 = q.num;
    long long d2 = q.denom;

    if (n1 == n2 && d1 == d2) return 0;
    if (n1 != n2 && d1 == d2) return (n1 > n2) - (n1 < n2);

    return ((long double)n1/d1 > (long double)n2/d2) - ((long double)n1/d1 < (long double)n2/d2);
}

// Print a Fraction
void printFraction(Fraction frac) {
    if (frac.denom == 1) {
        printf("%lld", frac.num);
        return;
    }

    printf("%lld/%lld", frac.num, frac.denom);
}

/* ---------- Complex arithmetic ---------- */

// Add two ComplexNumbers
ComplexNumber complexAdd(ComplexNumber a, ComplexNumber b) {
    ComplexNumber c;
    c.real = a.real + b.real;
    c.imag = a.imag + b.imag;
    return c;
}

// Subtract two ComplexNumbers
ComplexNumber complexSub(ComplexNumber a, ComplexNumber b) {
    ComplexNumber c;
    c.real = a.real - b.real;
    c.imag = a.imag - b.imag;
    return c;
}

// Multiply two ComplexNumbers
ComplexNumber complexMul(ComplexNumber a, ComplexNumber b) {
    ComplexNumber c;
    c.real = a.real * b.real - a.imag * b.imag;
    c.imag = a.real * b.imag + a.imag * b.real;
    return c;
}

// Divide two ComplexNumbers
ComplexNumber complexDiv(ComplexNumber a, ComplexNumber b) {
    ComplexNumber c;
    double denom = b.real * b.real + b.imag * b.imag;
    c.real = (a.real * b.real + a.imag * b.imag) / denom;
    c.imag = (a.imag * b.real - a.real * b.imag) / denom;
    return c;
}

// Negate a ComplexNumber
ComplexNumber complexNeg(ComplexNumber a) {
    ComplexNumber c;
    c.real = -a.real;
    c.imag = -a.imag;
    return c;
}

// Return the complex conjugate
ComplexNumber complexConj(ComplexNumber a) {
    ComplexNumber c;
    c.real = a.real;
    c.imag = -a.imag;
    return c;
}

// Return the modulus |a|
double complexAbs(ComplexNumber a) {
    return sqrt(a.real * a.real + a.imag * a.imag);
}

// Return the argument arg(a) in (-pi, pi]
double complexArg(ComplexNumber a) {
    return atan2(a.imag, a.real);
}

static double complex toC99Complex(ComplexNumber a) {
    return a.real + a.imag * I;
}

static ComplexNumber fromC99Complex(double complex z) {
    ComplexNumber a;
    a.real = creal(z);
    a.imag = cimag(z);
    return a;
}

// Return the complex exponential of a
ComplexNumber complexExp(ComplexNumber a) {
    return fromC99Complex(cexp(toC99Complex(a)));
}

// Return the principal complex logarithm of a
ComplexNumber complexLog(ComplexNumber a) {
    return fromC99Complex(clog(toC99Complex(a)));
}

// Return the complex sine of a
ComplexNumber complexSin(ComplexNumber a) {
    return fromC99Complex(csin(toC99Complex(a)));
}

// Return the complex cosine of a
ComplexNumber complexCos(ComplexNumber a) {
    return fromC99Complex(ccos(toC99Complex(a)));
}

// Return the complex tangent of a
ComplexNumber complexTan(ComplexNumber a) {
    return fromC99Complex(ctan(toC99Complex(a)));
}

// Return the principal complex inverse sine of a
ComplexNumber complexAsin(ComplexNumber a) {
    return fromC99Complex(casin(toC99Complex(a)));
}

// Return the principal complex inverse cosine of a
ComplexNumber complexAcos(ComplexNumber a) {
    return fromC99Complex(cacos(toC99Complex(a)));
}

// Return the principal complex inverse tangent of a
ComplexNumber complexAtan(ComplexNumber a) {
    return fromC99Complex(catan(toC99Complex(a)));
}

// Return the principal square root of a ComplexNumber
ComplexNumber complexSqrt(ComplexNumber a) {
    ComplexNumber c;
    // Real fast path: sqrt is either real or pure imaginary, no trig noise
    if (a.imag == 0.0) {
        if (a.real >= 0.0) { c.real = sqrt(a.real); c.imag = 0.0; }
        else { c.real = 0.0; c.imag = sqrt(-a.real); }
        return c;
    }
    double r = complexAbs(a);
    double theta = complexArg(a);
    double sr = sqrt(r);
    c.real = sr * cos(theta / 2.0);
    c.imag = sr * sin(theta / 2.0);
    return c;
}

// Return the principal cube root of a ComplexNumber
ComplexNumber complexCbrt(ComplexNumber a) {
    ComplexNumber c;
    // Real fast path: cbrt of a real is real
    if (a.imag == 0.0) {
        c.real = cbrt(a.real);
        c.imag = 0.0;
        return c;
    }
    double r = complexAbs(a);
    double theta = complexArg(a);
    double cr = cbrt(r);
    c.real = cr * cos(theta / 3.0);
    c.imag = cr * sin(theta / 3.0);
    return c;
}

// Tell if two ComplexNumbers are equal up to some tolerance
bool complexEq(ComplexNumber a, ComplexNumber b, double tol) {
    return fabs(a.real - b.real) <= tol && fabs(a.imag - b.imag) <= tol;
}

/* ---------- Transcendental constants ---------- */

// Get a floating point approximation for pi
long double pi(void) {
    long double x = 3.141592653589793;
    return x;
}

// Get a floating point approximation for e
long double e(void) {
    long double x = 2.718281828459045;
    return x;
}

// Get a floating point approximation for phi, the "Golden Ratio"
long double phi(void) {
    long double x = 1.618033988749895;
    return x;
}

