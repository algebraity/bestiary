#include<stdlib.h>
#include<stdio.h>
#include<math.h>
#include<complex.h>
#include<limits.h>
#include "hebi.h"

/* ---------- Helper methods ---------- */

// comp method for qsort
int comp(const void* a, const void* b) {
    return (*(long long*)a > *(long long*)b) - (*(long long*)a < *(long long*)b);
}

static unsigned long long llMagnitude(long long x) {
    if (x >= 0) return (unsigned long long)x;
    return (unsigned long long)(-(x + 1)) + 1ULL;
}

static unsigned long long gcdUnsignedLongLong(unsigned long long a, unsigned long long b) {
    while (b) {
        unsigned long long t = b;
        b = a % b;
        a = t;
    }
    return a ? a : 1ULL;
}

static int divideByUnsignedFactor(long long value, unsigned long long factor, long long* out) {
    if (!out || factor == 0) return 0;
    if (factor > (unsigned long long)LLONG_MAX) {
        if (value == 0) {
            *out = 0;
            return 1;
        }
        if (factor == (1ULL << 63) && value == LLONG_MIN) {
            *out = -1;
            return 1;
        }
        return 0;
    }
    *out = value / (long long)factor;
    return 1;
}

static int checkedAddLongLong(long long a, long long b, long long* out) {
#if defined(__GNUC__) || defined(__clang__)
    return !__builtin_add_overflow(a, b, out);
#else
    if ((b > 0 && a > LLONG_MAX - b) || (b < 0 && a < LLONG_MIN - b)) return 0;
    *out = a + b;
    return 1;
#endif
}

static int checkedSubLongLong(long long a, long long b, long long* out) {
#if defined(__GNUC__) || defined(__clang__)
    return !__builtin_sub_overflow(a, b, out);
#else
    if ((b < 0 && a > LLONG_MAX + b) || (b > 0 && a < LLONG_MIN + b)) return 0;
    *out = a - b;
    return 1;
#endif
}

static int checkedMulLongLong(long long a, long long b, long long* out) {
#if defined(__GNUC__) || defined(__clang__)
    return !__builtin_mul_overflow(a, b, out);
#else
    if (a == 0 || b == 0) {
        *out = 0;
        return 1;
    }
    if (a == -1 && b == LLONG_MIN) return 0;
    if (b == -1 && a == LLONG_MIN) return 0;
    long long r = a * b;
    if (r / b != a) return 0;
    *out = r;
    return 1;
#endif
}

/* ---------- Fractions ---------- */

// Construct a Fraction with a given num and denom
Fraction constructFraction(long long num, long long denom) {
    if (denom == 0) return (Fraction){0, 0}; // Invalid fraction

    unsigned long long g = gcdUnsignedLongLong(llMagnitude(num), llMagnitude(denom));
    Fraction frac;
    if (!divideByUnsignedFactor(num, g, &frac.num)
            || !divideByUnsignedFactor(denom, g, &frac.denom)) {
        return (Fraction){0, 0};
    }
    if (frac.denom < 0) {
        if (frac.num == LLONG_MIN || frac.denom == LLONG_MIN) return (Fraction){0, 0};
	frac.num = -frac.num;
	frac.denom = -frac.denom;
    }
    return frac;
}

// Add two Fractions
Fraction addFractions(Fraction p, Fraction q) {
    if (p.denom == 0 || q.denom == 0) return (Fraction){0, 0};
    unsigned long long g = gcdUnsignedLongLong(llMagnitude(p.denom), llMagnitude(q.denom));
    long long pg = p.denom / (long long)g;
    long long qg = q.denom / (long long)g;
    long long left, right, num, denom;
    if (!checkedMulLongLong(p.num, qg, &left)
            || !checkedMulLongLong(q.num, pg, &right)
            || !checkedAddLongLong(left, right, &num)
            || !checkedMulLongLong(pg, q.denom, &denom)) {
        return (Fraction){0, 0};
    }
    return constructFraction(num, denom);
}

// Subtract two Fractions
Fraction subtractFractions(Fraction p, Fraction q) {
    if (p.denom == 0 || q.denom == 0) return (Fraction){0, 0};
    unsigned long long g = gcdUnsignedLongLong(llMagnitude(p.denom), llMagnitude(q.denom));
    long long pg = p.denom / (long long)g;
    long long qg = q.denom / (long long)g;
    long long left, right, num, denom;
    if (!checkedMulLongLong(p.num, qg, &left)
            || !checkedMulLongLong(q.num, pg, &right)
            || !checkedSubLongLong(left, right, &num)
            || !checkedMulLongLong(pg, q.denom, &denom)) {
        return (Fraction){0, 0};
    }
    return constructFraction(num, denom);
}

// Multiply two Fractions
Fraction multiplyFractions(Fraction p, Fraction q) {
    if (p.denom == 0 || q.denom == 0) return (Fraction){0, 0};
    unsigned long long g1 = gcdUnsignedLongLong(llMagnitude(p.num), llMagnitude(q.denom));
    unsigned long long g2 = gcdUnsignedLongLong(llMagnitude(q.num), llMagnitude(p.denom));
    long long pnum, qnum, pden, qden, num, denom;
    if (!divideByUnsignedFactor(p.num, g1, &pnum)
            || !divideByUnsignedFactor(q.denom, g1, &qden)
            || !divideByUnsignedFactor(q.num, g2, &qnum)
            || !divideByUnsignedFactor(p.denom, g2, &pden)
            || !checkedMulLongLong(pnum, qnum, &num)
            || !checkedMulLongLong(pden, qden, &denom)) {
        return (Fraction){0, 0};
    }
    return constructFraction(num, denom);
}

// Divide two Fractions
Fraction divideFractions(Fraction p, Fraction q) {
    if (p.denom == 0 || q.denom == 0 || q.num == 0) return (Fraction){0, 0};
    Fraction reciprocal = constructFraction(q.denom, q.num);
    if (reciprocal.denom == 0) return (Fraction){0, 0};
    return multiplyFractions(p, reciprocal);
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
    long double denom = b.real * b.real + b.imag * b.imag;
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
long double complexAbs(ComplexNumber a) {
    return sqrtl(a.real * a.real + a.imag * a.imag);
}

// Return the argument arg(a) in (-pi, pi]
long double complexArg(ComplexNumber a) {
    return atan2l(a.imag, a.real);
}

static long double complex toC99Complex(ComplexNumber a) {
    return a.real + a.imag * I;
}

static ComplexNumber fromC99Complex(long double complex z) {
    ComplexNumber a;
    a.real = creall(z);
    a.imag = cimagl(z);
    return a;
}

// Return the complex exponential of a
ComplexNumber complexExp(ComplexNumber a) {
    return fromC99Complex(cexpl(toC99Complex(a)));
}

// Return the principal complex logarithm of a
ComplexNumber complexLog(ComplexNumber a) {
    return fromC99Complex(clogl(toC99Complex(a)));
}

// Return the complex sine of a
ComplexNumber complexSin(ComplexNumber a) {
    return fromC99Complex(csinl(toC99Complex(a)));
}

// Return the complex cosine of a
ComplexNumber complexCos(ComplexNumber a) {
    return fromC99Complex(ccosl(toC99Complex(a)));
}

// Return the complex tangent of a
ComplexNumber complexTan(ComplexNumber a) {
    return fromC99Complex(ctanl(toC99Complex(a)));
}

// Return the principal complex inverse sine of a
ComplexNumber complexAsin(ComplexNumber a) {
    return fromC99Complex(casinl(toC99Complex(a)));
}

// Return the principal complex inverse cosine of a
ComplexNumber complexAcos(ComplexNumber a) {
    return fromC99Complex(cacosl(toC99Complex(a)));
}

// Return the principal complex inverse tangent of a
ComplexNumber complexAtan(ComplexNumber a) {
    return fromC99Complex(catanl(toC99Complex(a)));
}

// Return the principal square root of a ComplexNumber
ComplexNumber complexSqrt(ComplexNumber a) {
    ComplexNumber c;
    // Real fast path: sqrt is either real or pure imaginary, no trig noise
    if (a.imag == 0.0) {
        if (a.real >= 0.0) { c.real = sqrtl(a.real); c.imag = 0.0; }
        else { c.real = 0.0; c.imag = sqrtl(-a.real); }
        return c;
    }
    long double r = complexAbs(a);
    long double theta = complexArg(a);
    long double sr = sqrtl(r);
    c.real = sr * cosl(theta / 2.0);
    c.imag = sr * sinl(theta / 2.0);
    return c;
}

// Return the principal cube root of a ComplexNumber
ComplexNumber complexCbrt(ComplexNumber a) {
    ComplexNumber c;
    // Real fast path: cbrt of a real is real
    if (a.imag == 0.0) {
        c.real = cbrtl(a.real);
        c.imag = 0.0;
        return c;
    }
    long double r = complexAbs(a);
    long double theta = complexArg(a);
    long double cr = cbrtl(r);
    c.real = cr * cosl(theta / 3.0);
    c.imag = cr * sinl(theta / 3.0);
    return c;
}

// Tell if two ComplexNumbers are equal up to some tolerance
bool complexEq(ComplexNumber a, ComplexNumber b, long double tol) {
    return fabsl(a.real - b.real) <= tol && fabsl(a.imag - b.imag) <= tol;
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
