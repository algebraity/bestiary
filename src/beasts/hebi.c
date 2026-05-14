#include<stdlib.h>
#include<stdio.h>
#include<math.h>
#include<complex.h>
#include<limits.h>
#include<stdarg.h>
#include<string.h>
#include<stdint.h>
#include "hebi.h"

#define HEBI_EULER_GAMMA 0.577215664901532860606512090082402431L
#define HEBI_PI 3.141592653589793238462643383279502884L

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

// Allocate and format a string
static char* formatString(const char* format, ...) {
    va_list args;
    va_list copy;
    va_start(args, format);
    va_copy(copy, args);
    int length = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if (length < 0) {
        va_end(args);
        return NULL;
    }

    char* string = malloc((size_t)length + 1);
    if (!string) {
        va_end(args);
        return NULL;
    }

    vsnprintf(string, (size_t)length + 1, format, args);
    va_end(args);
    return string;
}

/* ---------- Fractions ---------- */

// Construct the zero Fraction
Fraction zeroFraction(void) {
    return constructFraction(0, 1);
}

// Construct the one Fraction
Fraction oneFraction(void) {
    return constructFraction(1, 1);
}

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

// Negate a Fraction
Fraction negateFraction(Fraction x) {
    if (x.denom == 0 || x.num == LLONG_MIN) return (Fraction){0, 0};
    return constructFraction(-x.num, x.denom);
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

// Check whether a Fraction is zero
bool isZeroFraction(Fraction x) {
    return x.denom != 0 && x.num == 0;
}

// Check whether two Fractions are equal
bool eqFraction(Fraction p, Fraction q) {
    if (p.denom == 0 || q.denom == 0) return false;
    return compFractions(p, q) == 0;
}

// Compare Fractions
int compFractions(Fraction p, Fraction q) {
    if (p.denom == 0 || q.denom == 0) return 0;
    if (p.num == q.num && p.denom == q.denom) return 0;
    if (p.denom == q.denom) return (p.num > q.num) - (p.num < q.num);

    // Cross-multiply exactly so large rational values stay correctly ordered
#if defined(__GNUC__) || defined(__clang__)
    __int128 left = (__int128)p.num * q.denom;
    __int128 right = (__int128)q.num * p.denom;
    return (left > right) - (left < right);
#else
    long double left = (long double)p.num / (long double)p.denom;
    long double right = (long double)q.num / (long double)q.denom;
    return (left > right) - (left < right);
#endif
}

// Convert a Fraction to a string
char* fractionToString(Fraction frac) {
    if (frac.denom == 0) return formatString("<invalid>");
    if (frac.denom == 1) return formatString("%lld", frac.num);
    return formatString("%lld/%lld", frac.num, frac.denom);
}

// Print a Fraction
void printFraction(Fraction frac) {
    char* string = fractionToString(frac);
    if (!string) return;
    printf("%s", string);
    free(string);
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
    if (denom == 0.0L) {
        c.real = NAN;
        c.imag = NAN;
        return c;
    }
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

/* ---------- Special functions ---------- */

// Return the real error function
long double realErf(long double x) {
    return erfl(x);
}

// Return the real exponential integral Ei on its real branch
long double realEi(long double x) {
    // Handle the logarithmic singularity at zero
    if (x == 0.0L) return -INFINITY;
    if (isnan(x)) return NAN;
    if (!isfinite(x)) return x > 0.0L ? INFINITY : 0.0L;

    // Use the defining power series for moderate real inputs
    if (fabsl(x) <= 40.0L) {
        long double term = x;
        long double sum = term;
        for (int k = 2; k <= 400; k++) {
            term *= x / (long double)k;
            long double add = term / (long double)k;
            sum += add;
            if (fabsl(add) <= 1e-21L * (1.0L + fabsl(sum))) break;
        }
        return HEBI_EULER_GAMMA + logl(fabsl(x)) + sum;
    }

    // Use the standard asymptotic expansion away from the origin
    long double term = 1.0L;
    long double sum = term;
    long double prev = fabsl(term);
    for (int k = 1; k <= 200; k++) {
        term *= (long double)k / x;
        long double mag = fabsl(term);
        if (mag > prev) break;
        sum += term;
        prev = mag;
        if (mag <= 1e-21L * (1.0L + fabsl(sum))) break;
    }
    return expl(x) * sum / x;
}

// Return the complex error function
ComplexNumber complexErf(ComplexNumber z) {
    // Use the real branch when the input is real
    if (z.imag == 0.0L) return (ComplexNumber){realErf(z.real), 0.0L};

    // Sum the entire power series for erf(z)
    long double complex w = toC99Complex(z);
    long double complex term = w;
    long double complex sum = term;
    for (int n = 1; n <= 300; n++) {
        term *= -w * w / (long double)n;
        long double complex add = term / (long double)(2 * n + 1);
        sum += add;
        if (cabsl(add) <= 1e-18L * (1.0L + cabsl(sum))) break;
    }

    return fromC99Complex((2.0L / sqrtl(HEBI_PI)) * sum);
}

// Return the principal complex exponential integral Ei
ComplexNumber complexEi(ComplexNumber z) {
    // Use the real branch when the input is positive real
    if (z.imag == 0.0L && z.real > 0.0L) return (ComplexNumber){realEi(z.real), 0.0L};

    // Handle the logarithmic singularity at zero
    long double complex w = toC99Complex(z);
    if (cabsl(w) == 0.0L) return (ComplexNumber){-INFINITY, 0.0L};

    // Use the defining principal-branch series for moderate inputs
    if (cabsl(w) <= 40.0L) {
        long double complex term = w;
        long double complex sum = term;
        for (int k = 2; k <= 500; k++) {
            term *= w / (long double)k;
            long double complex add = term / (long double)k;
            sum += add;
            if (cabsl(add) <= 1e-18L * (1.0L + cabsl(sum))) break;
        }
        return fromC99Complex(HEBI_EULER_GAMMA + clogl(w) + sum);
    }

    // Use the standard asymptotic expansion for large inputs
    long double complex term = 1.0L;
    long double complex sum = term;
    long double prev = cabsl(term);
    for (int k = 1; k <= 250; k++) {
        term *= (long double)k / w;
        long double mag = cabsl(term);
        if (mag > prev) break;
        sum += term;
        prev = mag;
        if (mag <= 1e-18L * (1.0L + cabsl(sum))) break;
    }
    return fromC99Complex(cexpl(w) * sum / w);
}

/* ---------- Number arithmetic ---------- */

// Return the shared Number NAN sentinel
static Number numberNan(void) {
    Number n;
    n.type = NUMBER_NAN;
    n.as.x = NAN;
    return n;
}

// Check whether a Fraction has a valid denominator
static bool fractionIsValid(Fraction frac) {
    return frac.denom != 0;
}

// Convert an exact Number to a Fraction
static Fraction numberToFraction(Number n) {
    switch (n.type) {
        case NUMBER_INT:      return constructFraction(n.as.i, 1);
        case NUMBER_FRACTION: return n.as.frac;
        default:              return (Fraction){0, 0};
    }
}

// Convert a Number to a real value
static long double numberToReal(Number n) {
    switch (n.type) {
        case NUMBER_INT:      return (long double)n.as.i;
        case NUMBER_FRACTION: return (long double)n.as.frac.num / (long double)n.as.frac.denom;
        case NUMBER_REAL:     return n.as.x;
        case NUMBER_COMPLEX:  return n.as.z.real;
        case NUMBER_NAN:      return NAN;
    }
    return NAN;
}

// Convert a Number to a ComplexNumber
static ComplexNumber numberToComplex(Number n) {
    switch (n.type) {
        case NUMBER_INT:      return (ComplexNumber){(long double)n.as.i, 0.0};
        case NUMBER_FRACTION: return (ComplexNumber){numberToReal(n), 0.0};
        case NUMBER_REAL:     return (ComplexNumber){n.as.x, 0.0};
        case NUMBER_COMPLEX:  return n.as.z;
        case NUMBER_NAN:      return (ComplexNumber){NAN, NAN};
    }
    return (ComplexNumber){NAN, NAN};
}

// Wrap a Fraction result as a Number
static Number constructNumberFromFractionResult(Fraction frac) {
    return fractionIsValid(frac) ? constructNumberFromFraction(frac) : numberNan();
}

// Construct a Number from an integer
Number constructNumberFromInt(long long i) {
    Number n;
    n.type = NUMBER_INT;
    n.as.i = i;
    return n;
}

// Construct a Number from a real value
Number constructNumberFromDouble(long double x) {
    Number n;
    if (isnan(x)) return numberNan();
    n.type = NUMBER_REAL;
    n.as.x = x;
    return n;
}

// Construct a Number from a Fraction
Number constructNumberFromFraction(Fraction frac) {
    Number n;
    if (!fractionIsValid(frac)) return numberNan();
    n.type = NUMBER_FRACTION;
    n.as.frac = frac;
    return n;
}

// Construct a Number from a ComplexNumber
Number constructNumberFromComplex(ComplexNumber z) {
    Number n;
    if (isnan(z.real) || isnan(z.imag)) return numberNan();
    n.type = NUMBER_COMPLEX;
    n.as.z = z;
    return n;
}

// Add two Numbers
Number addNumbers(Number a, Number b) {
    if (a.type == NUMBER_NAN || b.type == NUMBER_NAN) return numberNan();
    if (a.type == NUMBER_COMPLEX || b.type == NUMBER_COMPLEX)
        return constructNumberFromComplex(complexAdd(numberToComplex(a), numberToComplex(b)));
    if (a.type == NUMBER_REAL || b.type == NUMBER_REAL)
        return constructNumberFromDouble(numberToReal(a) + numberToReal(b));
    if (a.type == NUMBER_FRACTION || b.type == NUMBER_FRACTION)
        return constructNumberFromFractionResult(addFractions(numberToFraction(a), numberToFraction(b)));
    return constructNumberFromInt(a.as.i + b.as.i);
}

// Subtract two Numbers
Number subNumbers(Number a, Number b) {
    if (a.type == NUMBER_NAN || b.type == NUMBER_NAN) return numberNan();
    if (a.type == NUMBER_COMPLEX || b.type == NUMBER_COMPLEX)
        return constructNumberFromComplex(complexSub(numberToComplex(a), numberToComplex(b)));
    if (a.type == NUMBER_REAL || b.type == NUMBER_REAL)
        return constructNumberFromDouble(numberToReal(a) - numberToReal(b));
    if (a.type == NUMBER_FRACTION || b.type == NUMBER_FRACTION)
        return constructNumberFromFractionResult(subtractFractions(numberToFraction(a), numberToFraction(b)));
    return constructNumberFromInt(a.as.i - b.as.i);
}

// Multiply two Numbers
Number multNumbers(Number a, Number b) {
    if (a.type == NUMBER_NAN || b.type == NUMBER_NAN) return numberNan();
    if (a.type == NUMBER_COMPLEX || b.type == NUMBER_COMPLEX)
        return constructNumberFromComplex(complexMul(numberToComplex(a), numberToComplex(b)));
    if (a.type == NUMBER_REAL || b.type == NUMBER_REAL)
        return constructNumberFromDouble(numberToReal(a) * numberToReal(b));
    if (a.type == NUMBER_FRACTION || b.type == NUMBER_FRACTION)
        return constructNumberFromFractionResult(multiplyFractions(numberToFraction(a), numberToFraction(b)));
    return constructNumberFromInt(a.as.i * b.as.i);
}

// Divide two Numbers
Number divNumbers(Number a, Number b) {
    if (a.type == NUMBER_NAN || b.type == NUMBER_NAN) return numberNan();
    if (a.type == NUMBER_COMPLEX || b.type == NUMBER_COMPLEX) {
        ComplexNumber divisor = numberToComplex(b);
        if (divisor.real == 0.0 && divisor.imag == 0.0) return numberNan();
        return constructNumberFromComplex(complexDiv(numberToComplex(a), divisor));
    }
    if (a.type == NUMBER_REAL || b.type == NUMBER_REAL) {
        long double divisor = numberToReal(b);
        if (divisor == 0.0) return numberNan();
        return constructNumberFromDouble(numberToReal(a) / divisor);
    }
    if (a.type == NUMBER_FRACTION || b.type == NUMBER_FRACTION)
        return constructNumberFromFractionResult(divideFractions(numberToFraction(a), numberToFraction(b)));
    if (b.as.i == 0) return numberNan();
    if (a.as.i % b.as.i == 0) return constructNumberFromInt(a.as.i / b.as.i);
    return constructNumberFromFractionResult(constructFraction(a.as.i, b.as.i));
}

// Compare two Numbers
long long compNumbers(Number a, Number b) {
    if (a.type == NUMBER_NAN && b.type == NUMBER_NAN) return 0;
    if (a.type == NUMBER_NAN) return 1;
    if (b.type == NUMBER_NAN) return -1;
    if (a.type == NUMBER_COMPLEX || b.type == NUMBER_COMPLEX) {
        ComplexNumber z = numberToComplex(a);
        ComplexNumber w = numberToComplex(b);
        if (z.real != w.real) return (z.real > w.real) - (z.real < w.real);
        return (z.imag > w.imag) - (z.imag < w.imag);
    }
    if ((a.type == NUMBER_INT || a.type == NUMBER_FRACTION)
            && (b.type == NUMBER_INT || b.type == NUMBER_FRACTION)) {
        return compFractions(numberToFraction(a), numberToFraction(b));
    }
    long double x = numberToReal(a);
    long double y = numberToReal(b);
    return (x > y) - (x < y);
}

// Check whether two Numbers are equal
bool eqNumbers(Number a, Number b) {
    return compNumbers(a, b) == 0;
}

// Free any resources owned by a Number
void freeNumber(Number x) {
    (void)x;
}

// Print a Number
void printNumber(Number x) {
    switch (x.type) {
        case NUMBER_INT:
            printf("%lld", x.as.i);
            break;
        case NUMBER_FRACTION:
            printFraction(x.as.frac);
            break;
        case NUMBER_REAL:
            printf("%Lg", x.as.x);
            break;
        case NUMBER_COMPLEX:
            printf("(%Lg + %Lgi)", x.as.z.real, x.as.z.imag);
            break;
        case NUMBER_NAN:
            printf("nan");
            break;
    }
}

/* ---------- Randomness ---------- */

static const unsigned long long DEFAULT_PRG_SEED = 0xD1B54A32D192ED03ULL;
static unsigned long long prgState = 0xD1B54A32D192ED03ULL;

// Convert an unsigned bit pattern to a long long
static long long unsignedToLongLong(unsigned long long x) {
    if (x <= (unsigned long long)LLONG_MAX) return (long long)x;
    return (long long)(x - ((unsigned long long)LLONG_MAX + 1ULL)) + LLONG_MIN;
}

// Return the unsigned size of an inclusive long long range
static unsigned long long unsignedRangeSize(long long min, long long max) {
    return (unsigned long long)max - (unsigned long long)min + 1ULL;
}

// Return an unbiased random offset below span
static unsigned long long randomOffset(unsigned long long span) {
    if (span == 0) return stepPRG();
    unsigned long long threshold = (0ULL - span) % span;
    unsigned long long x;
    do {
        x = stepPRG();
    } while (x < threshold);
    return x % span;
}

// Set the PRG seed
void seedPRG(unsigned long long seed) {
    prgState = seed;
}

// Reset the PRG to the default seed
void resetPRG(void) {
    seedPRG(DEFAULT_PRG_SEED);
}

// Step the PRG and return a raw 64-bit value
unsigned long long stepPRG(void) {
    unsigned long long z;
    prgState += 0x9E3779B97F4A7C15ULL;
    z = prgState;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

// Return a random integer in an inclusive range
long long randomInt(long long min, long long max) {
    if (min > max) return 0;
    unsigned long long offset = randomOffset(unsignedRangeSize(min, max));
    return unsignedToLongLong((unsigned long long)min + offset);
}

// Return a random real number in an interval
long double randomReal(long double min, long double max) {
    if (!isfinite(min) || !isfinite(max) || min > max) return NAN;
    if (min == max) return min;
    long double u = (long double)stepPRG() / (long double)ULLONG_MAX;
    return min + (max - min) * u;
}

// Return a random Fraction with ranged numerator and denominator
Fraction randomFraction(long long minNum, long long maxNum, long long minDenom, long long maxDenom) {
    if (minNum > maxNum || minDenom > maxDenom) return (Fraction){0, 0};
    if (minDenom == 0 && maxDenom == 0) return (Fraction){0, 0};

    long long denom;
    do {
        denom = randomInt(minDenom, maxDenom);
    } while (denom == 0);

    return constructFraction(randomInt(minNum, maxNum), denom);
}

// Return a random ComplexNumber from real and imaginary intervals
ComplexNumber randomComplexComp(long double minReal, long double maxReal, long double minImag, long double maxImag) {
    ComplexNumber z;
    z.real = randomReal(minReal, maxReal);
    z.imag = randomReal(minImag, maxImag);
    return z;
}

// Return a random ComplexNumber with modulus in an interval
ComplexNumber randomComplexMod(long double minMod, long double maxMod) {
    if (!isfinite(minMod) || !isfinite(maxMod) || minMod < 0.0L || minMod > maxMod) {
        return (ComplexNumber){NAN, NAN};
    }
    long double r = randomReal(minMod, maxMod);
    long double theta = randomReal(0.0L, 2.0L * pi());
    return (ComplexNumber){r * cosl(theta), r * sinl(theta)};
}

// Return a random ComplexNumber from real and imaginary intervals
ComplexNumber randomComplex(long double minReal, long double maxReal, long double minImag, long double maxImag) {
    return randomComplexComp(minReal, maxReal, minImag, maxImag);
}

/* ---------- Polynomial solvers ---------- */

// Return a ComplexNumber from real and imaginary parts
static ComplexNumber complexValue(long double real, long double imag) {
    return (ComplexNumber){real, imag};
}

// Compare roots in a stable real-then-imaginary order
static int compComplexRoots(const void* lhs, const void* rhs) {
    const ComplexNumber* a = (const ComplexNumber*)lhs;
    const ComplexNumber* b = (const ComplexNumber*)rhs;
    long double ar = fabsl(a->real) < 1e-10L ? 0.0L : a->real;
    long double br = fabsl(b->real) < 1e-10L ? 0.0L : b->real;
    long double ai = fabsl(a->imag) < 1e-10L ? 0.0L : a->imag;
    long double bi = fabsl(b->imag) < 1e-10L ? 0.0L : b->imag;
    if (ar < br) return -1;
    if (ar > br) return 1;
    if (ai < bi) return -1;
    if (ai > bi) return 1;
    return 0;
}

// Tell whether a complex value is numerically zero
static bool complexNearZero(ComplexNumber z) {
    return complexAbs(z) < 1e-14L;
}

// Multiply a complex value by a real scalar
static ComplexNumber complexScale(ComplexNumber z, long double x) {
    return (ComplexNumber){z.real * x, z.imag * x};
}

// Snap tiny complex components to exact zero
static ComplexNumber snapComplexRoot(ComplexNumber z) {
    if (fabsl(z.real) < 1e-10L) z.real = 0.0L;
    if (fabsl(z.imag) < 1e-10L) z.imag = 0.0L;
    return z;
}

// Normalize, snap, and sort a root array
static size_t finishRoots(ComplexNumber* roots, size_t count) {
    for (size_t i = 0; i < count; i++) roots[i] = snapComplexRoot(roots[i]);
    qsort(roots, count, sizeof(ComplexNumber), compComplexRoots);
    return count;
}

// Return one quotient of complex values
static ComplexNumber complexQuot(ComplexNumber a, ComplexNumber b) {
    if (complexNearZero(b)) return complexValue(NAN, NAN);
    return complexDiv(a, b);
}

// Solve a linear equation over the complex numbers
static size_t solveLinear(ComplexNumber a, ComplexNumber b, ComplexNumber roots[1]) {
    if (complexNearZero(a)) return 0;
    roots[0] = complexNeg(complexQuot(b, a));
    return finishRoots(roots, 1);
}

// Return one term in the cubic formula
static ComplexNumber cubicRootTerm(ComplexNumber delta0, ComplexNumber c, ComplexNumber omega) {
    ComplexNumber wc = complexMul(omega, c);
    if (complexNearZero(wc)) return complexValue(NAN, NAN);
    return complexAdd(wc, complexQuot(delta0, wc));
}

// Solve a quadratic equation over the complex numbers
size_t solveQuadratic(ComplexNumber a, ComplexNumber b, ComplexNumber c, ComplexNumber roots[2]) {
    if (!roots) return 0;
    if (complexNearZero(a)) return solveLinear(b, c, roots);

    ComplexNumber fourAC = complexScale(complexMul(a, c), 4.0L);
    ComplexNumber discriminant = complexSub(complexMul(b, b), fourAC);
    ComplexNumber radical = complexSqrt(discriminant);
    ComplexNumber denom = complexScale(a, 2.0L);

    roots[0] = complexQuot(complexSub(complexNeg(b), radical), denom);
    roots[1] = complexQuot(complexAdd(complexNeg(b), radical), denom);
    return finishRoots(roots, 2);
}

// Solve a cubic equation over the complex numbers
size_t solveCubic(ComplexNumber a, ComplexNumber b, ComplexNumber c, ComplexNumber d, ComplexNumber roots[3]) {
    if (!roots) return 0;
    if (complexNearZero(a)) return solveQuadratic(b, c, d, roots);

    ComplexNumber a2 = complexMul(a, a);
    ComplexNumber b2 = complexMul(b, b);
    ComplexNumber b3 = complexMul(b2, b);
    ComplexNumber delta0 = complexSub(b2, complexScale(complexMul(a, c), 3.0L));
    ComplexNumber delta1 = complexAdd(
        complexSub(complexScale(b3, 2.0L), complexScale(complexMul(complexMul(a, b), c), 9.0L)),
        complexScale(complexMul(a2, d), 27.0L));
    ComplexNumber radical = complexSqrt(complexSub(complexMul(delta1, delta1),
                                                   complexScale(complexMul(complexMul(delta0, delta0), delta0), 4.0L)));
    ComplexNumber bigC = complexCbrt(complexScale(complexAdd(delta1, radical), 0.5L));
    if (complexNearZero(bigC)) bigC = complexCbrt(complexScale(complexSub(delta1, radical), 0.5L));

    if (complexNearZero(bigC)) {
        roots[0] = roots[1] = roots[2] = complexNeg(complexQuot(b, complexScale(a, 3.0L)));
        return finishRoots(roots, 3);
    }

    ComplexNumber omega = complexValue(-0.5L, sqrtl(3.0L) / 2.0L);
    ComplexNumber omegas[3] = {complexValue(1.0L, 0.0L), omega, complexMul(omega, omega)};
    ComplexNumber denom = complexScale(a, 3.0L);
    for (size_t k = 0; k < 3; k++) {
        ComplexNumber term = cubicRootTerm(delta0, bigC, omegas[k]);
        roots[k] = complexNeg(complexQuot(complexAdd(b, term), denom));
    }
    return finishRoots(roots, 3);
}

// Solve a biquadratic depressed quartic
static size_t solveDepressedBiquadratic(ComplexNumber alpha, ComplexNumber gamma, ComplexNumber shift, ComplexNumber roots[4]) {
    ComplexNumber radical = complexSqrt(complexSub(complexMul(alpha, alpha), complexScale(gamma, 4.0L)));
    ComplexNumber y1 = complexScale(complexAdd(complexNeg(alpha), radical), 0.5L);
    ComplexNumber y2 = complexScale(complexSub(complexNeg(alpha), radical), 0.5L);
    ComplexNumber s1 = complexSqrt(y1);
    ComplexNumber s2 = complexSqrt(y2);

    roots[0] = complexAdd(shift, s1);
    roots[1] = complexSub(shift, s1);
    roots[2] = complexAdd(shift, s2);
    roots[3] = complexSub(shift, s2);
    return finishRoots(roots, 4);
}

// Solve a quartic equation over the complex numbers
size_t solveQuartic(ComplexNumber a, ComplexNumber b, ComplexNumber c, ComplexNumber d, ComplexNumber e, ComplexNumber roots[4]) {
    if (!roots) return 0;
    if (complexNearZero(a)) return solveCubic(b, c, d, e, roots);

    ComplexNumber A = complexQuot(b, a);
    ComplexNumber B = complexQuot(c, a);
    ComplexNumber C = complexQuot(d, a);
    ComplexNumber D = complexQuot(e, a);
    ComplexNumber A2 = complexMul(A, A);
    ComplexNumber A3 = complexMul(A2, A);
    ComplexNumber A4 = complexMul(A2, A2);
    ComplexNumber shift = complexScale(A, -0.25L);

    ComplexNumber alpha = complexAdd(B, complexScale(A2, -3.0L / 8.0L));
    ComplexNumber beta = complexAdd(complexSub(complexScale(A3, 1.0L / 8.0L),
                                               complexScale(complexMul(A, B), 0.5L)), C);
    ComplexNumber gamma = complexAdd(
        complexSub(complexAdd(complexScale(A4, -3.0L / 256.0L),
                              complexScale(complexMul(A2, B), 1.0L / 16.0L)),
                   complexScale(complexMul(A, C), 0.25L)),
        D);

    if (complexNearZero(beta)) return solveDepressedBiquadratic(alpha, gamma, shift, roots);

    ComplexNumber P = complexSub(complexScale(complexMul(alpha, alpha), -1.0L / 12.0L), gamma);
    ComplexNumber Q = complexAdd(
        complexSub(complexScale(complexMul(complexMul(alpha, alpha), alpha), -1.0L / 108.0L),
                   complexScale(complexMul(beta, beta), 1.0L / 8.0L)),
        complexScale(complexMul(alpha, gamma), 1.0L / 3.0L));
    ComplexNumber Rradicand = complexAdd(complexScale(complexMul(Q, Q), 0.25L),
                                         complexScale(complexMul(complexMul(P, P), P), 1.0L / 27.0L));
    ComplexNumber R = complexAdd(complexScale(complexNeg(Q), 0.5L), complexSqrt(Rradicand));
    ComplexNumber U = complexCbrt(R);
    if (complexNearZero(U)) U = complexCbrt(complexSub(complexScale(complexNeg(Q), 0.5L), complexSqrt(Rradicand)));

    ComplexNumber y;
    if (complexNearZero(U)) {
        y = complexSub(complexScale(alpha, -5.0L / 6.0L), complexCbrt(Q));
    } else {
        y = complexAdd(complexScale(alpha, -5.0L / 6.0L),
                       complexSub(U, complexScale(complexQuot(P, U), 1.0L / 3.0L)));
    }

    ComplexNumber W = complexSqrt(complexAdd(alpha, complexScale(y, 2.0L)));
    if (complexNearZero(W)) return solveDepressedBiquadratic(alpha, gamma, shift, roots);

    ComplexNumber threeAlphaTwoY = complexAdd(complexScale(alpha, 3.0L), complexScale(y, 2.0L));
    ComplexNumber betaOverW = complexQuot(beta, W);
    ComplexNumber inner1 = complexNeg(complexAdd(threeAlphaTwoY, complexScale(betaOverW, 2.0L)));
    ComplexNumber inner2 = complexNeg(complexSub(threeAlphaTwoY, complexScale(betaOverW, 2.0L)));
    ComplexNumber sqrt1 = complexSqrt(inner1);
    ComplexNumber sqrt2 = complexSqrt(inner2);

    roots[0] = complexAdd(shift, complexScale(complexAdd(W, sqrt1), 0.5L));
    roots[1] = complexAdd(shift, complexScale(complexSub(W, sqrt1), 0.5L));
    roots[2] = complexAdd(shift, complexScale(complexAdd(complexNeg(W), sqrt2), 0.5L));
    roots[3] = complexAdd(shift, complexScale(complexSub(complexNeg(W), sqrt2), 0.5L));
    return finishRoots(roots, 4);
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

/* ---------- Field helper methods ---------- */

// Reduce an integer modulo p
static long long reduceLongLongMod(long long n, long long p) {
    long long r = n % p;
    if (r < 0) r += p;
    return r;
}

// Multiply two integers modulo p
static long long multiplyLongLongMod(long long a, long long b, long long p) {
#if defined(__GNUC__) || defined(__clang__)
    __int128 product = (__int128)reduceLongLongMod(a, p) * reduceLongLongMod(b, p);
    long long r = (long long)(product % p);
    if (r < 0) r += p;
    return r;
#else
    long long result = 0;
    a = reduceLongLongMod(a, p);
    b = reduceLongLongMod(b, p);
    while (b > 0) {
        if (b % 2 == 1) result = reduceLongLongMod(result + a, p);
        a = reduceLongLongMod(a + a, p);
        b /= 2;
    }
    return result;
#endif
}

// Add two integers modulo p
static long long addLongLongMod(long long a, long long b, long long p) {
#if defined(__GNUC__) || defined(__clang__)
    __int128 sum = (__int128)reduceLongLongMod(a, p) + reduceLongLongMod(b, p);
    long long r = (long long)(sum % p);
    if (r < 0) r += p;
    return r;
#else
    return reduceLongLongMod(reduceLongLongMod(a, p) + reduceLongLongMod(b, p), p);
#endif
}

// Subtract two integers modulo p
static long long subtractLongLongMod(long long a, long long b, long long p) {
#if defined(__GNUC__) || defined(__clang__)
    __int128 diff = (__int128)reduceLongLongMod(a, p) - reduceLongLongMod(b, p);
    long long r = (long long)(diff % p);
    if (r < 0) r += p;
    return r;
#else
    return reduceLongLongMod(reduceLongLongMod(a, p) - reduceLongLongMod(b, p), p);
#endif
}

// Invert an integer modulo p
static int inverseLongLongMod(long long n, long long p, long long* inverse) {
#if defined(__GNUC__) || defined(__clang__)
    __int128 t = 0;
    __int128 newT = 1;
    __int128 r = p;
    __int128 newR = reduceLongLongMod(n, p);

    // Iterate until the computation reaches its stopping condition
    while (newR != 0) {
        __int128 quotient = r / newR;
        __int128 oldT = t;
        __int128 oldR = r;
        t = newT;
        r = newR;
        newT = oldT - quotient * newT;
        newR = oldR - quotient * newR;
    }

    // Accept the inverse only when the Euclidean algorithm ended at gcd one
    if (r != 1) return 0;
    *inverse = reduceLongLongMod((long long)(t % p), p);
    return 1;
#else
    long long t = 0;
    long long newT = 1;
    long long r = p;
    long long newR = reduceLongLongMod(n, p);

    // Iterate until the computation reaches its stopping condition
    while (newR != 0) {
        long long quotient = r / newR;
        long long oldT = t;
        long long oldR = r;
        t = newT;
        r = newR;
        newT = oldT - quotient * newT;
        newR = oldR - quotient * newR;
    }

    // Accept the inverse only when the Euclidean algorithm ended at gcd one
    if (r != 1) return 0;
    *inverse = reduceLongLongMod(t, p);
    return 1;
#endif
}

// Check whether a positive integer is prime
static bool isPrimeLongLong(long long n) {
    if (n < 2) return false;
    if (n == 2) return true;
    if (n % 2 == 0) return false;
    for (long long d = 3; d <= n / d; d += 2) {
        if (n % d == 0) return false;
    }
    return true;
}

// Add two size_t values when the sum is representable
static bool checkedSizeAdd(size_t a, size_t b, size_t* out) {
    if (out == NULL || a > SIZE_MAX - b) return false;
    *out = a + b;
    return true;
}

// Multiply two size_t values when the product is representable
static bool checkedSizeMul(size_t a, size_t b, size_t* out) {
    if (out == NULL || (a != 0 && b > SIZE_MAX / a)) return false;
    *out = a * b;
    return true;
}

// Append a string suffix to a heap-allocated string
static int appendString(char** string, size_t* length, const char* suffix) {
    size_t suffixLength = strlen(suffix);
    size_t newLength;
    size_t allocSize;
    char* grown;
    if (!checkedSizeAdd(*length, suffixLength, &newLength)
            || !checkedSizeAdd(newLength, 1, &allocSize)) {
        return 0;
    }

    // Grow the destination string so the suffix and terminator fit
    grown = realloc(*string, allocSize);
    if (grown == NULL) return 0;

    // Copy the suffix into place and update the tracked string length
    memcpy(grown + *length, suffix, suffixLength + 1);
    *string = grown;
    *length = newLength;
    return 1;
}

// Append a formatted suffix to a heap-allocated string
static int appendFormat(char** string, size_t* length, const char* format, ...) {
    va_list args;
    va_list copy;
    va_start(args, format);
    va_copy(copy, args);
    int suffixLength = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if (suffixLength < 0) {
        va_end(args);
        return 0;
    }

    // Compute the new buffer size required for the formatted suffix
    size_t newLength;
    size_t allocSize;
    if (!checkedSizeAdd(*length, (size_t)suffixLength, &newLength)
            || !checkedSizeAdd(newLength, 1, &allocSize)) {
        va_end(args);
        return 0;
    }

    // Grow the destination string before formatting into the new tail
    char* grown = realloc(*string, allocSize);
    if (grown == NULL) {
        va_end(args);
        return 0;
    }

    // Format the suffix into place and update the tracked string length
    vsnprintf(grown + *length, (size_t)suffixLength + 1, format, args);
    va_end(args);
    *string = grown;
    *length = newLength;
    return 1;
}

// Check whether a Field has valid structural data
static bool fieldHasValidStructure(Field* field) {
    if (field == NULL) return false;

    // Validate the structural invariants required by each field type
    switch (field->type) {
        case QQ:
        case RR:
        case CC:
            return field->repr != NULL && field->chr == 0;
        case FF:
            if (field->repr == NULL || field->chr != field->data.ff.p
                    || !isPrimeLongLong(field->data.ff.p)
                    || field->data.ff.degree == 0
                    || field->data.ff.degree > (SIZE_MAX / sizeof(long long)) - 1) {
                return false;
            }
            if (field->data.ff.degree == 1) return true;
            return field->data.ff.modulus != NULL
                && reduceLongLongMod(field->data.ff.modulus[field->data.ff.degree],
                        field->data.ff.p) != 0;
        case FF_QEXT:
            return field->repr != NULL
                && field->data.ffqext.baseField != NULL
                && fieldHasValidStructure(field->data.ffqext.baseField)
                && field->data.ffqext.baseField->type == FF
                && field->chr == field->data.ffqext.baseField->chr
                && field->data.ffqext.radicand != NULL
                && fieldElementIsValid(field->data.ffqext.radicand)
                && fieldEq(field->data.ffqext.baseField,
                    field->data.ffqext.radicand->field)
                && fieldElementIsUnit(*field->data.ffqext.radicand);
        case NF:
            if (field->repr == NULL || field->chr != 0 || field->data.nf.baseField == NULL
                    || field->data.nf.gen.degree == 0
                    || field->data.nf.gen.degree > (SIZE_MAX / sizeof(FieldElement)) - 1
                    || field->data.nf.gen.minPolyCoeffs == NULL) {
                return false;
            }
            if (!fieldHasValidStructure(field->data.nf.baseField)) return false;
            for (size_t j = 0; j <= field->data.nf.gen.degree; j++) {
                if (!fieldElementIsValid(&field->data.nf.gen.minPolyCoeffs[j])
                        || !fieldEq(field->data.nf.baseField,
                            field->data.nf.gen.minPolyCoeffs[j].field)) {
                    return false;
                }
            }
            return fieldElementIsUnit(
                    field->data.nf.gen.minPolyCoeffs[field->data.nf.gen.degree]);
    }

    // Reject unknown field tags after the known cases are exhausted
    return false;
}

// Check whether two number field generators are equal
static bool nfGenEq(NFGen* a, NFGen* b) {
    if (a == NULL || b == NULL || a->degree != b->degree
            || a->minPolyCoeffs == NULL || b->minPolyCoeffs == NULL) {
        return false;
    }

    // Compare every minimal-polynomial coefficient of the two generators
    for (size_t i = 0; i <= a->degree; i++) {
        if (!eqFieldElements(a->minPolyCoeffs[i], b->minPolyCoeffs[i])) return false;
    }
    return true;
}

// Check whether two finite fields are equal
static bool finiteFieldEq(Field* a, Field* b) {
    if (a->data.ff.p != b->data.ff.p || a->data.ff.degree != b->data.ff.degree) {
        return false;
    }

    // Treat prime fields with omitted moduli as the same field
    if (a->data.ff.degree == 1
            && (a->data.ff.modulus == NULL || b->data.ff.modulus == NULL)) {
        return true;
    }

    // Compare finite-field defining moduli coefficient by coefficient
    if (a->data.ff.modulus == NULL || b->data.ff.modulus == NULL) return false;
    for (size_t i = 0; i <= a->data.ff.degree; i++) {
        if (reduceLongLongMod(a->data.ff.modulus[i], a->data.ff.p)
                != reduceLongLongMod(b->data.ff.modulus[i], b->data.ff.p)) {
            return false;
        }
    }
    return true;
}

// Check whether two number fields are equal
static bool numberFieldEq(Field* a, Field* b) {
    if (!fieldEq(a->data.nf.baseField, b->data.nf.baseField)) return false;
    return nfGenEq(&a->data.nf.gen, &b->data.nf.gen);
}

// Free an array of FieldElements
static void freeFieldElementArray(FieldElement* xs, size_t n) {
    if (xs == NULL) return;

    // Free every owned field element before releasing the array
    for (size_t i = 0; i < n; i++) {
        freeFieldElement(&xs[i]);
    }
    free(xs);
}

// Return the active degree of a tower-style number field
static size_t nfDegree(Field* field) {
    if (!fieldHasValidStructure(field) || field->type != NF) return 0;
    return field->data.nf.gen.degree;
}

// Return the size of a finite field as a search bound
static size_t ffOrder(Field* field) {
    size_t order = 1;
    if (!fieldHasValidStructure(field) || field->type != FF) return 0;

    // Multiply the characteristic through each extension degree with overflow checks
    for (size_t i = 0; i < field->data.ff.degree; i++) {
        if (order > SIZE_MAX / (size_t)field->data.ff.p) return 0;
        order *= (size_t)field->data.ff.p;
    }
    return order;
}

// Build the counter-th finite field element in coefficient form
static long long* ffCoeffsFromIndex(Field* field, size_t index) {
    long long* coeffs;
    if (!fieldHasValidStructure(field) || field->type != FF) return NULL;

    // Allocate and fill base-p coefficients for the requested finite-field element
    coeffs = calloc(field->data.ff.degree, sizeof(long long));
    if (coeffs == NULL) return NULL;
    for (size_t i = 0; i < field->data.ff.degree; i++) {
        coeffs[i] = (long long)(index % (size_t)field->data.ff.p);
        index /= (size_t)field->data.ff.p;
    }
    return coeffs;
}

// Construct an element of a quadratic finite field extension
static FieldElement constructFFQExtElement(Field* field, FieldElement c0, FieldElement c1) {
    FieldElement x = {0};
    if (!fieldHasValidStructure(field) || field->type != FF_QEXT
            || !fieldElementIsValid(&c0) || !fieldElementIsValid(&c1)
            || !fieldEq(field->data.ffqext.baseField, c0.field)
            || !fieldEq(field->data.ffqext.baseField, c1.field)) {
        return x;
    }

    // Attach the field and reduce each coefficient modulo the characteristic
    x.field = field;
    x.value.ffqextData.coeffs = calloc(2, sizeof(FieldElement));
    if (x.value.ffqextData.coeffs == NULL) {
        x.field = NULL;
        return x;
    }

    // Copy the two base-field coefficients into the quadratic extension element
    x.value.ffqextData.coeffs[0] = copyFieldElement(c0);
    x.value.ffqextData.coeffs[1] = copyFieldElement(c1);
    if (!fieldElementIsValid(&x.value.ffqextData.coeffs[0])
            || !fieldElementIsValid(&x.value.ffqextData.coeffs[1])) {
        freeFieldElement(&x);
        return (FieldElement){0};
    }
    return x;
}

// Copy a Field using the repo's non-owning base-field convention
static Field copyFieldStruct(Field* field) {
    Field copy = {0};
    if (!fieldHasValidStructure(field)) return copy;

    // Copy field metadata and duplicate any owned defining data
    copy.repr = field->repr;
    copy.chr = field->chr;
    copy.type = field->type;
    switch (field->type) {
        case QQ:
        case RR:
        case CC:
            return copy;
        case FF:
            copy = constructFFField(field->data.ff.p, field->data.ff.degree,
                    field->data.ff.modulus);
            copy.repr = field->repr;
            return copy;
        case FF_QEXT:
            copy.repr = field->repr;
            copy.chr = field->chr;
            copy.type = FF_QEXT;
            copy.data.ffqext.baseField = field->data.ffqext.baseField;
            copy.data.ffqext.genRepr = field->data.ffqext.genRepr;
            copy.data.ffqext.radicand = malloc(sizeof(FieldElement));
            if (copy.data.ffqext.radicand == NULL) return (Field){0};
            *copy.data.ffqext.radicand = copyFieldElement(*field->data.ffqext.radicand);
            if (!fieldElementIsValid(copy.data.ffqext.radicand)) {
                freeField(&copy);
                return (Field){0};
            }
            return copy;
        case NF:
            copy.data.nf.baseField = field->data.nf.baseField;
            copy.data.nf.gen.repr = field->data.nf.gen.repr;
            copy.data.nf.gen.degree = field->data.nf.gen.degree;
            copy.data.nf.gen.minPolyCoeffs =
                calloc(field->data.nf.gen.degree + 1, sizeof(FieldElement));
            if (copy.data.nf.gen.minPolyCoeffs == NULL) return (Field){0};
            for (size_t i = 0; i <= field->data.nf.gen.degree; i++) {
                copy.data.nf.gen.minPolyCoeffs[i] =
                    copyFieldElement(field->data.nf.gen.minPolyCoeffs[i]);
                if (!fieldElementIsValid(&copy.data.nf.gen.minPolyCoeffs[i])) {
                    freeField(&copy);
                    return (Field){0};
                }
            }
            return copy;
    }

    // Return the copied field after the type-specific ownership work
    return copy;
}

// Check whether a Field can be used as a base for a characteristic-0 quadratic extension
static bool fieldSupportsQuadraticExtension(Field* field) {
    if (!fieldHasValidStructure(field)) return false;
    if (field->type == FF) return true;
    if (field->chr != 0) return false;
    return field->type == QQ || field->type == RR || field->type == CC || field->type == NF;
}

// Check whether a long long is a perfect square and return its root
static bool perfectSquareLongLong(long long n, long long* root) {
    long long r;
    if (root == NULL || n < 0) return false;

    // Round to a candidate square root and adjust around floating-point error
    r = (long long)floorl(sqrtl((long double)n) + 0.5L);
    while (r > 0 && r > n / r) r--;
    while ((r + 1) > 0 && (r + 1) <= n / (r + 1)) r++;
    if ((unsigned long long)r * (unsigned long long)r
            != (unsigned long long)n) {
        return false;
    }
    *root = r;
    return true;
}

// Check whether an NF is of the form baseField(u) with u^2 = radicand
static bool fieldHasSimpleQuadraticGenerator(Field* field, FieldElement* radicand) {
    FieldElement minusConstant;
    if (!fieldHasValidStructure(field) || field->type != NF || nfDegree(field) != 2) {
        return false;
    }
    if (!fieldElementIsZero(field->data.nf.gen.minPolyCoeffs[1])
            || !fieldElementIsOne(field->data.nf.gen.minPolyCoeffs[2])) {
        return false;
    }
    if (radicand == NULL) return true;

    // Extract the radicand from the monic quadratic generator polynomial
    minusConstant = negateFieldElement(field->data.nf.gen.minPolyCoeffs[0]);
    if (!fieldElementIsValid(&minusConstant)) return false;
    *radicand = minusConstant;
    return true;
}

// Check whether two quadratic finite field extensions are equal
static bool finiteQuadraticExtensionEq(Field* a, Field* b) {
    return fieldEq(a->data.ffqext.baseField, b->data.ffqext.baseField)
        && a->data.ffqext.radicand != NULL && b->data.ffqext.radicand != NULL
        && eqFieldElements(*a->data.ffqext.radicand, *b->data.ffqext.radicand);
}

// Add finite field coefficient arrays
static long long* addFFCoeffs(Field* field, long long* a, long long* b) {
    size_t degree = field->data.ff.degree;
    long long p = field->data.ff.p;
    long long* coeffs = malloc(degree * sizeof(long long));
    if (coeffs == NULL) return NULL;

    // Add finite-field coefficients modulo the characteristic
    for (size_t i = 0; i < degree; i++) {
        coeffs[i] = addLongLongMod(a[i], b[i], p);
    }
    return coeffs;
}

// Subtract finite field coefficient arrays
static long long* subtractFFCoeffs(Field* field, long long* a, long long* b) {
    size_t degree = field->data.ff.degree;
    long long p = field->data.ff.p;
    long long* coeffs = malloc(degree * sizeof(long long));
    if (coeffs == NULL) return NULL;

    // Subtract finite-field coefficients modulo the characteristic
    for (size_t i = 0; i < degree; i++) {
        coeffs[i] = subtractLongLongMod(a[i], b[i], p);
    }
    return coeffs;
}

// Negate a finite field coefficient array
static long long* negateFFCoeffs(Field* field, long long* a) {
    size_t degree = field->data.ff.degree;
    long long p = field->data.ff.p;
    long long* coeffs = malloc(degree * sizeof(long long));
    if (coeffs == NULL) return NULL;

    // Negate finite-field coefficients modulo the characteristic
    for (size_t i = 0; i < degree; i++) {
        coeffs[i] = subtractLongLongMod(0, a[i], p);
    }
    return coeffs;
}

// Reduce a finite field polynomial modulo the defining modulus
static int reduceFFPolynomial(Field* field, long long* coeffs, size_t len) {
    size_t degree = field->data.ff.degree;
    long long p = field->data.ff.p;
    if (degree == 0 || len == 0) return 0;

    // Reduce prime-field representatives directly when there is no extension modulus
    if (degree == 1 && field->data.ff.modulus == NULL) {
        coeffs[0] = reduceLongLongMod(coeffs[0], p);
        return 1;
    }
    if (field->data.ff.modulus == NULL) return 0;

    // Normalize by the leading modulus coefficient before polynomial reduction
    long long leadInverse;
    if (!inverseLongLongMod(field->data.ff.modulus[degree], p, &leadInverse)) return 0;

    // Cancel high-degree coefficients against the finite-field defining polynomial
    for (size_t k = len; k-- > degree;) {
        long long coeff = reduceLongLongMod(coeffs[k], p);
        if (coeff == 0) continue;

        // Compute the scaled modulus row used to cancel this high-degree term
        long long factor = multiplyLongLongMod(coeff, leadInverse, p);
        for (size_t i = 0; i <= degree; i++) {
            size_t index = k - degree + i;
            long long term = multiplyLongLongMod(factor, field->data.ff.modulus[i], p);
            coeffs[index] = subtractLongLongMod(coeffs[index], term, p);
        }
    }

    // Reduce the remaining low-degree coefficients modulo the characteristic
    for (size_t i = 0; i < degree; i++) {
        coeffs[i] = reduceLongLongMod(coeffs[i], p);
    }
    return 1;
}

// Multiply finite field coefficient arrays
static long long* multiplyFFCoeffs(Field* field, long long* a, long long* b) {
    size_t degree = field->data.ff.degree;
    long long p = field->data.ff.p;
    size_t twiceDegree;
    size_t len;
    long long* product;
    long long* coeffs = NULL;
    if (!checkedSizeMul(2, degree, &twiceDegree) || twiceDegree == 0) {
        return NULL;
    }
    len = twiceDegree - 1;
    product = calloc(len, sizeof(long long));
    if (product == NULL) return NULL;

    // Convolve the two coefficient arrays before reducing by the modulus
    for (size_t i = 0; i < degree; i++) {
        for (size_t j = 0; j < degree; j++) {
            long long term = multiplyLongLongMod(a[i], b[j], p);
            product[i + j] = addLongLongMod(product[i + j], term, p);
        }
    }

    // Reduce the convolution product to the finite-field basis coordinates
    if (!reduceFFPolynomial(field, product, len)) {
        free(product);
        return NULL;
    }

    // Copy the reduced finite-field product coefficients into the result array
    coeffs = malloc(degree * sizeof(long long));
    if (coeffs != NULL) {
        for (size_t i = 0; i < degree; i++) {
            coeffs[i] = product[i];
        }
    }
    free(product);
    return coeffs;
}

// Invert a finite field coefficient array
static long long* invertFFCoeffs(Field* field, long long* a) {
    size_t degree = field->data.ff.degree;
    long long p = field->data.ff.p;
    size_t width;
    size_t entryCount;
    long long* matrix;
    long long* solution = NULL;
    if (!checkedSizeAdd(degree, 1, &width)
            || !checkedSizeMul(degree, width, &entryCount)) {
        return NULL;
    }

    // Allocate the augmented multiplication system for finite-field inversion
    matrix = calloc(entryCount, sizeof(long long));
    if (matrix == NULL) return NULL;

    // Build the multiplication-by-a matrix with an augmented identity target
    for (size_t col = 0; col < degree; col++) {
        long long* basis = calloc(degree, sizeof(long long));
        long long* product;
        if (basis == NULL) {
            free(matrix);
            return NULL;
        }
        basis[col] = 1;
        product = multiplyFFCoeffs(field, a, basis);
        free(basis);
        if (product == NULL) {
            free(matrix);
            return NULL;
        }
        for (size_t row = 0; row < degree; row++) {
            matrix[row * (degree + 1) + col] = product[row];
        }
        free(product);
    }
    matrix[degree] = 1;

    // Row-reduce the augmented system over the prime field
    for (size_t col = 0; col < degree; col++) {
        size_t pivot = degree;
        long long inverse;
        for (size_t row = col; row < degree; row++) {
            if (matrix[row * (degree + 1) + col] != 0) {
                pivot = row;
                break;
            }
        }
        if (pivot == degree) {
            free(matrix);
            return NULL;
        }

        // Swap the selected pivot row into place before normalization
        if (pivot != col) {
            for (size_t j = col; j <= degree; j++) {
                long long tmp = matrix[col * (degree + 1) + j];
                matrix[col * (degree + 1) + j] = matrix[pivot * (degree + 1) + j];
                matrix[pivot * (degree + 1) + j] = tmp;
            }
        }

        // Normalize the finite-field pivot row by the pivot inverse
        if (!inverseLongLongMod(matrix[col * (degree + 1) + col], p, &inverse)) {
            free(matrix);
            return NULL;
        }
        for (size_t j = col; j <= degree; j++) {
            matrix[col * (degree + 1) + j] =
                multiplyLongLongMod(matrix[col * (degree + 1) + j], inverse, p);
        }

        // Clear the pivot column from every other row of the finite-field system
        for (size_t row = 0; row < degree; row++) {
            long long factor;
            if (row == col) continue;
            factor = matrix[row * (degree + 1) + col];
            if (factor == 0) continue;
            for (size_t j = col; j <= degree; j++) {
                long long term = multiplyLongLongMod(factor,
                        matrix[col * (degree + 1) + j], p);
                matrix[row * (degree + 1) + j] =
                    subtractLongLongMod(matrix[row * (degree + 1) + j], term, p);
            }
        }
    }

    // Read the inverse coefficients from the augmented column
    solution = malloc(degree * sizeof(long long));
    if (solution != NULL) {
        for (size_t i = 0; i < degree; i++) {
            solution[i] = matrix[i * (degree + 1) + degree];
        }
    }
    free(matrix);
    return solution;
}

// Allocate an array of zero elements over a Field
static FieldElement* zeroFieldElementArray(Field* field, size_t n) {
    FieldElement* xs;
    if (!fieldHasValidStructure(field) || n == 0) return NULL;

    // Allocate and fill a zero field-element array
    xs = calloc(n, sizeof(FieldElement));
    if (xs == NULL) return NULL;
    for (size_t i = 0; i < n; i++) {
        xs[i] = zeroFieldElement(field);
        if (!fieldElementIsValid(&xs[i])) {
            freeFieldElementArray(xs, n);
            return NULL;
        }
    }
    return xs;
}

// Copy a FieldElement array
static FieldElement* copyFieldElementArray(FieldElement* xs, size_t n) {
    FieldElement* copy;
    if (xs == NULL || n == 0) return NULL;

    // Allocate and fill a copy field-element array
    copy = calloc(n, sizeof(FieldElement));
    if (copy == NULL) return NULL;
    for (size_t i = 0; i < n; i++) {
        copy[i] = copyFieldElement(xs[i]);
        if (!fieldElementIsValid(&copy[i])) {
            freeFieldElementArray(copy, n);
            return NULL;
        }
    }
    return copy;
}

// Add number field coefficient arrays
static FieldElement* addNFCoeffs(Field* field, FieldElement* a, FieldElement* b) {
    size_t degree = nfDegree(field);
    FieldElement* coeffs = calloc(degree, sizeof(FieldElement));
    if (degree == 0 || coeffs == NULL) return NULL;

    // Add number-field coefficients over the base field
    for (size_t i = 0; i < degree; i++) {
        coeffs[i] = addFieldElements(a[i], b[i]);
        if (!fieldElementIsValid(&coeffs[i])) {
            freeFieldElementArray(coeffs, degree);
            return NULL;
        }
    }
    return coeffs;
}

// Subtract number field coefficient arrays
static FieldElement* subtractNFCoeffs(Field* field, FieldElement* a, FieldElement* b) {
    size_t degree = nfDegree(field);
    FieldElement* coeffs = calloc(degree, sizeof(FieldElement));
    if (degree == 0 || coeffs == NULL) return NULL;

    // Subtract number-field coefficients over the base field
    for (size_t i = 0; i < degree; i++) {
        coeffs[i] = subtractFieldElements(a[i], b[i]);
        if (!fieldElementIsValid(&coeffs[i])) {
            freeFieldElementArray(coeffs, degree);
            return NULL;
        }
    }
    return coeffs;
}

// Negate a number field coefficient array
static FieldElement* negateNFCoeffs(Field* field, FieldElement* a) {
    size_t degree = nfDegree(field);
    FieldElement* coeffs = calloc(degree, sizeof(FieldElement));
    if (degree == 0 || coeffs == NULL) return NULL;

    // Negate number-field coefficients over the base field
    for (size_t i = 0; i < degree; i++) {
        coeffs[i] = negateFieldElement(a[i]);
        if (!fieldElementIsValid(&coeffs[i])) {
            freeFieldElementArray(coeffs, degree);
            return NULL;
        }
    }
    return coeffs;
}

// Reduce a number field polynomial modulo the minimal polynomial
static int reduceNFPolynomial(Field* field, FieldElement* coeffs, size_t len) {
    NFGen* gen;
    size_t degree;
    if (!fieldHasValidStructure(field) || field->type != NF || len == 0) return 0;

    // Load the active generator data used for polynomial reduction
    gen = &field->data.nf.gen;
    degree = gen->degree;
    // Cancel high-degree terms using the defining polynomial
    for (size_t k = len; k-- > degree;) {
        FieldElement factor = {0};
        if (fieldElementIsZero(coeffs[k])) continue;

        // Cancel a high-degree term using the defining polynomial
        factor = divideFieldElements(coeffs[k], gen->minPolyCoeffs[degree]);
        if (!fieldElementIsValid(&factor)) return 0;

        // Subtract the matching defining-polynomial multiple from lower coefficients
        for (size_t i = 0; i <= degree; i++) {
            size_t index = k - degree + i;
            FieldElement term = multiplyFieldElements(factor, gen->minPolyCoeffs[i]);
            FieldElement diff;
            if (!fieldElementIsValid(&term)) {
                freeFieldElement(&factor);
                return 0;
            }
            diff = subtractFieldElements(coeffs[index], term);
            freeFieldElement(&term);
            if (!fieldElementIsValid(&diff)) {
                freeFieldElement(&factor);
                return 0;
            }
            freeFieldElement(&coeffs[index]);
            coeffs[index] = diff;
        }
        freeFieldElement(&factor);
    }
    return 1;
}

// Multiply number field coefficient arrays
static FieldElement* multiplyNFCoeffs(Field* field, FieldElement* a, FieldElement* b) {
    size_t degree = nfDegree(field);
    size_t twiceDegree;
    size_t len;
    Field* baseField;
    FieldElement* product;
    FieldElement* coeffs = NULL;
    if (degree == 0) return NULL;
    if (!checkedSizeMul(2, degree, &twiceDegree) || twiceDegree == 0) return NULL;
    len = twiceDegree - 1;

    // Allocate the base-field polynomial product used for convolution
    baseField = field->data.nf.baseField;
    product = zeroFieldElementArray(baseField, len);
    if (product == NULL) return NULL;

    // Convolve the polynomial representatives before reducing by the minimal polynomial
    for (size_t i = 0; i < degree; i++) {
        for (size_t j = 0; j < degree; j++) {
            FieldElement term = multiplyFieldElements(a[i], b[j]);
            FieldElement sum;
            if (!fieldElementIsValid(&term)) {
                freeFieldElementArray(product, len);
                return NULL;
            }
            sum = addFieldElements(product[i + j], term);
            freeFieldElement(&term);
            if (!fieldElementIsValid(&sum)) {
                freeFieldElementArray(product, len);
                return NULL;
            }
            freeFieldElement(&product[i + j]);
            product[i + j] = sum;
        }
    }

    // Reduce the convolution product by the minimal polynomial
    if (!reduceNFPolynomial(field, product, len)) {
        freeFieldElementArray(product, len);
        return NULL;
    }

    // Copy the reduced product into a degree-sized coefficient array
    coeffs = copyFieldElementArray(product, degree);
    freeFieldElementArray(product, len);
    return coeffs;
}

// Invert a number field coefficient array
static FieldElement* invertNFCoeffs(Field* field, FieldElement* a) {
    size_t degree = nfDegree(field);
    size_t width;
    size_t entryCount;
    Field* baseField;
    FieldElement* matrix;
    FieldElement* solution = NULL;
    if (degree == 0) return NULL;
    if (!checkedSizeAdd(degree, 1, &width)
            || !checkedSizeMul(degree, width, &entryCount)) {
        return NULL;
    }

    // Allocate the augmented base-field system used to solve for the inverse
    baseField = field->data.nf.baseField;
    matrix = zeroFieldElementArray(baseField, entryCount);
    if (matrix == NULL) return NULL;

    // Build the multiplication-by-a matrix with an augmented identity target
    for (size_t col = 0; col < degree; col++) {
        FieldElement* basis = zeroFieldElementArray(baseField, degree);
        FieldElement* product;
        if (basis == NULL) {
            freeFieldElementArray(matrix, entryCount);
            return NULL;
        }
        freeFieldElement(&basis[col]);
        basis[col] = oneFieldElement(baseField);
        if (!fieldElementIsValid(&basis[col])) {
            freeFieldElementArray(basis, degree);
            freeFieldElementArray(matrix, entryCount);
            return NULL;
        }

        // Multiply a by this basis vector to fill one column of the inverse system
        product = multiplyNFCoeffs(field, a, basis);
        freeFieldElementArray(basis, degree);
        if (product == NULL) {
            freeFieldElementArray(matrix, entryCount);
            return NULL;
        }
        for (size_t row = 0; row < degree; row++) {
            freeFieldElement(&matrix[row * width + col]);
            matrix[row * width + col] = product[row];
        }
        free(product);
    }
    freeFieldElement(&matrix[degree]);
    matrix[degree] = oneFieldElement(baseField);
    if (!fieldElementIsValid(&matrix[degree])) {
        freeFieldElementArray(matrix, entryCount);
        return NULL;
    }

    // Row-reduce the augmented system over the base field
    for (size_t col = 0; col < degree; col++) {
        size_t pivot = degree;
        FieldElement pivotInverse;
        for (size_t row = col; row < degree; row++) {
            if (!fieldElementIsZero(matrix[row * width + col])) {
                pivot = row;
                break;
            }
        }
        if (pivot == degree) {
            freeFieldElementArray(matrix, entryCount);
            return NULL;
        }

        // Swap the selected pivot row into place before normalization
        if (pivot != col) {
            for (size_t j = 0; j <= degree; j++) {
                FieldElement tmp = matrix[col * width + j];
                matrix[col * width + j] = matrix[pivot * width + j];
                matrix[pivot * width + j] = tmp;
            }
        }

        // Normalize the number-field pivot row by the pivot inverse
        pivotInverse = invertFieldElement(matrix[col * width + col]);
        if (!fieldElementIsValid(&pivotInverse)) {
            freeFieldElementArray(matrix, entryCount);
            return NULL;
        }
        for (size_t j = col; j <= degree; j++) {
            FieldElement product = multiplyFieldElements(matrix[col * width + j],
                    pivotInverse);
            if (!fieldElementIsValid(&product)) {
                freeFieldElement(&pivotInverse);
                freeFieldElementArray(matrix, entryCount);
                return NULL;
            }
            freeFieldElement(&matrix[col * width + j]);
            matrix[col * width + j] = product;
        }
        freeFieldElement(&pivotInverse);

        // Clear the pivot column from every other row of the number-field system
        for (size_t row = 0; row < degree; row++) {
            FieldElement factor;
            if (row == col || fieldElementIsZero(matrix[row * width + col])) continue;

            // Copy the row factor used to eliminate the pivot-column entry
            factor = copyFieldElement(matrix[row * width + col]);
            if (!fieldElementIsValid(&factor)) {
                freeFieldElementArray(matrix, entryCount);
                return NULL;
            }
            for (size_t j = col; j <= degree; j++) {
                FieldElement term = multiplyFieldElements(factor, matrix[col * width + j]);
                FieldElement diff;
                if (!fieldElementIsValid(&term)) {
                    freeFieldElement(&factor);
                    freeFieldElementArray(matrix, entryCount);
                    return NULL;
                }
                diff = subtractFieldElements(matrix[row * width + j], term);
                freeFieldElement(&term);
                if (!fieldElementIsValid(&diff)) {
                    freeFieldElement(&factor);
                    freeFieldElementArray(matrix, entryCount);
                    return NULL;
                }
                freeFieldElement(&matrix[row * width + j]);
                matrix[row * width + j] = diff;
            }
            freeFieldElement(&factor);
        }
    }

    // Read the inverse coefficients from the augmented column
    solution = calloc(degree, sizeof(FieldElement));
    if (solution != NULL) {
        for (size_t i = 0; i < degree; i++) {
            solution[i] = copyFieldElement(matrix[i * width + degree]);
            if (!fieldElementIsValid(&solution[i])) {
                freeFieldElementArray(solution, degree);
                solution = NULL;
                break;
            }
        }
    }
    freeFieldElementArray(matrix, entryCount);
    return solution;
}

/* ---------- Construct methods ---------- */

// Construct the field of rational numbers Q
Field constructQQField(void) {
    Field Q = {0};
    Q.repr = "Q";
    Q.chr = 0;
    Q.type = QQ;
    return Q;
}

// Construct the field of real numbers R
Field constructRRField(void) {
    Field R = {0};
    R.repr = "R";
    R.chr = 0;
    R.type = RR;
    return R;
}

// Construct the field of complex numbers C
Field constructCCField(void) {
    Field C = {0};
    C.repr = "C";
    C.chr = 0;
    C.type = CC;
    return C;
}

// Construct a finite field of order p^degree
Field constructFFField(long long p, size_t degree, long long* modulus) {
    Field F = {0};
    if (!isPrimeLongLong(p) || degree == 0 || (degree > 1 && modulus == NULL)
            || degree > (SIZE_MAX / sizeof(long long)) - 1) {
        return F;
    }

    // Store finite-field metadata and copy the defining modulus when needed
    F.repr = "FF";
    F.chr = p;
    F.type = FF;
    F.data.ff.p = p;
    F.data.ff.degree = degree;
    if (degree > 0 && modulus != NULL) {
        F.data.ff.modulus = malloc((degree + 1) * sizeof(long long));
        if (F.data.ff.modulus != NULL) {
            for (size_t i = 0; i <= degree; i++) {
                F.data.ff.modulus[i] = reduceLongLongMod(modulus[i], p);
            }
        }
    }
    return F;
}

// Construct a quadratic extension field baseField(u) with u^2 = radicand
Field constructQuadraticExtensionField(Field* baseField, FieldElement radicand,
        const char* genRepr) {
    Field extension = {0};
    FieldElement sqrtInBase = {0};
    FieldElement minusRadicand = {0};
    FieldElement zero = {0};
    FieldElement one = {0};

    // Reject invalid bases or radicands before constructing the extension
    if (!fieldSupportsQuadraticExtension(baseField) || !fieldElementIsValid(&radicand)
            || !fieldEq(baseField, radicand.field)) {
        return extension;
    }

    // Reuse the base field when the square root already lives there
    sqrtInBase = fieldElementSquareRoot(radicand);
    if (fieldElementIsValid(&sqrtInBase)) {
        freeFieldElement(&sqrtInBase);
        return copyFieldStruct(baseField);
    }
    // For finite fields, keep the quadratic extension as the private FF_QEXT field type
    if (baseField->type == FF) {
        extension.repr = "FF_QEXT";
        extension.chr = baseField->chr;
        extension.type = FF_QEXT;
        extension.data.ffqext.baseField = baseField;
        extension.data.ffqext.genRepr = (char*)(genRepr != NULL ? genRepr : "s");
        extension.data.ffqext.radicand = malloc(sizeof(FieldElement));
        if (extension.data.ffqext.radicand == NULL) return (Field){0};
        *extension.data.ffqext.radicand = copyFieldElement(radicand);
        if (!fieldElementIsValid(extension.data.ffqext.radicand)
                || !fieldHasValidStructure(&extension)) {
            freeField(&extension);
            return (Field){0};
        }
        return extension;
    }
    // Over the reals, the only missing square roots are complex
    if (baseField->type == RR) {
        return constructCCField();
    }

    // In characteristic zero, construct baseField[u]/(u^2-radicand)
    minusRadicand = negateFieldElement(radicand);
    zero = zeroFieldElement(baseField);
    one = oneFieldElement(baseField);
    if (!fieldElementIsValid(&minusRadicand) || !fieldElementIsValid(&zero)
            || !fieldElementIsValid(&one)) {
        freeFieldElement(&minusRadicand);
        freeFieldElement(&zero);
        freeFieldElement(&one);
        return (Field){0};
    }

    // Populate the number-field structure for the quadratic extension
    extension.repr = "NF";
    extension.chr = 0;
    extension.type = NF;
    extension.data.nf.baseField = baseField;
    extension.data.nf.gen.repr = (char*)(genRepr != NULL ? genRepr : "a");
    extension.data.nf.gen.degree = 2;
    extension.data.nf.gen.minPolyCoeffs = calloc(3, sizeof(FieldElement));
    if (extension.data.nf.gen.minPolyCoeffs == NULL) {
        freeFieldElement(&minusRadicand);
        freeFieldElement(&zero);
        freeFieldElement(&one);
        return (Field){0};
    }

    // Store the polynomial u^2-radicand and validate the completed extension
    extension.data.nf.gen.minPolyCoeffs[0] = minusRadicand;
    extension.data.nf.gen.minPolyCoeffs[1] = zero;
    extension.data.nf.gen.minPolyCoeffs[2] = one;
    if (!fieldHasValidStructure(&extension)) {
        freeField(&extension);
        return (Field){0};
    }
    return extension;
}

// Construct an element of the field of rational numbers Q
FieldElement constructQQElement(Field* field, Fraction value) {
    FieldElement x = {0};
    if (!fieldHasValidStructure(field) || field->type != QQ) return x;
    x.field = field;
    x.value.frac = constructFraction(value.num, value.denom);
    return x;
}

// Construct an element of the field of real numbers R
FieldElement constructRRElement(Field* field, long double value) {
    FieldElement x = {0};
    if (!fieldHasValidStructure(field) || field->type != RR) return x;
    x.field = field;
    x.value.real = value;
    return x;
}

// Construct an element of the field of complex numbers C
FieldElement constructCCElement(Field* field, ComplexNumber value) {
    FieldElement x = {0};
    if (!fieldHasValidStructure(field) || field->type != CC) return x;
    x.field = field;
    x.value.z = value;
    return x;
}

// Construct an element of a finite field
FieldElement constructFFElement(Field* field, long long* coeffs) {
    FieldElement x = {0};
    if (!fieldHasValidStructure(field) || field->type != FF || coeffs == NULL) {
        return x;
    }

    // Allocate and reduce finite-field coefficients into the new element
    x.value.ffData.coeffs = malloc(field->data.ff.degree * sizeof(long long));
    if (x.value.ffData.coeffs == NULL) return x;

    // Allocate coefficient storage and copy validated base-field coefficients
    x.field = field;
    for (size_t i = 0; i < field->data.ff.degree; i++) {
        x.value.ffData.coeffs[i] = reduceLongLongMod(coeffs[i], field->data.ff.p);
    }
    return x;
}

// Construct an element of a characteristic-0 one-generator extension field
FieldElement constructNFElement(Field* field, FieldElement* coeffs, size_t coeffLen) {
    FieldElement x = {0};
    size_t degree;
    if (!fieldHasValidStructure(field) || field->type != NF || coeffs == NULL) return x;

    // Resolve the active number-field degree before copying coefficients
    degree = nfDegree(field);
    if (degree == 0 || coeffLen != degree) return x;

    // Allocate coefficient storage and copy validated base-field coefficients
    x.field = field;
    x.value.nfData.coeffs = calloc(degree, sizeof(FieldElement));
    if (x.value.nfData.coeffs == NULL) {
        x.field = NULL;
        return x;
    }
    for (size_t i = 0; i < degree; i++) {
        if (!fieldElementIsValid(&coeffs[i])
                || !fieldEq(field->data.nf.baseField, coeffs[i].field)) {
            freeFieldElement(&x);
            return (FieldElement){0};
        }
        x.value.nfData.coeffs[i] = copyFieldElement(coeffs[i]);
        if (!fieldElementIsValid(&x.value.nfData.coeffs[i])) {
            freeFieldElement(&x);
            return (FieldElement){0};
        }
    }
    return x;
}

// Construct a field element from an integer
FieldElement fieldElementFromInt(Field* field, long long n) {
    FieldElement x = {0};
    if (!fieldHasValidStructure(field)) return x;

    // Dispatch the scalar conversion through the target field representation
    if (field->type == QQ) {
        return constructQQElement(field, constructFraction(n, 1));
    }
    if (field->type == RR) {
        return constructRRElement(field, (long double)n);
    }
    if (field->type == CC) {
        ComplexNumber z = {(long double)n, 0.0L};
        return constructCCElement(field, z);
    }
    if (field->type == FF && field->data.ff.p > 0 && field->data.ff.degree > 0) {
        x.value.ffData.coeffs = malloc(field->data.ff.degree * sizeof(long long));
        if (x.value.ffData.coeffs == NULL) return x;
        x.field = field;
        x.value.ffData.coeffs[0] = reduceLongLongMod(n, field->data.ff.p);
        for (size_t i = 1; i < field->data.ff.degree; i++) {
            x.value.ffData.coeffs[i] = 0;
        }
    }
    if (field->type == FF_QEXT && fieldHasValidStructure(field)) {
        FieldElement baseValue = fieldElementFromInt(field->data.ffqext.baseField, n);
        FieldElement zero = zeroFieldElement(field->data.ffqext.baseField);
        if (!fieldElementIsValid(&baseValue) || !fieldElementIsValid(&zero)) {
            freeFieldElement(&baseValue);
            freeFieldElement(&zero);
            return x;
        }
        x = constructFFQExtElement(field, baseValue, zero);
        freeFieldElement(&baseValue);
        freeFieldElement(&zero);
        return x;
    }
    if (field->type == NF && fieldHasValidStructure(field)) {
        FieldElement baseValue = fieldElementFromInt(field->data.nf.baseField, n);
        if (!fieldElementIsValid(&baseValue)) return x;

        // Dispatch the scalar conversion through the target field representation
        x = zeroFieldElement(field);
        if (!fieldElementIsValid(&x)) {
            freeFieldElement(&baseValue);
            return (FieldElement){0};
        }
        freeFieldElement(&x.value.nfData.coeffs[0]);
        x.value.nfData.coeffs[0] = baseValue;
    }
    return x;
}

// Construct a field element from a Fraction
FieldElement fieldElementFromFraction(Field* field, Fraction q) {
    FieldElement x = {0};
    if (!fieldHasValidStructure(field) || q.denom == 0) return x;

    // Dispatch the scalar conversion through the target field representation
    if (field->type == QQ) {
        return constructQQElement(field, q);
    }
    if (field->type == RR) {
        return constructRRElement(field, (long double)q.num / (long double)q.denom);
    }
    if (field->type == CC) {
        ComplexNumber z = {(long double)q.num / (long double)q.denom, 0.0L};
        return constructCCElement(field, z);
    }
    if (field->type == FF && field->data.ff.p > 0 && field->data.ff.degree > 0) {
        long long denomInverse;
        long long num = reduceLongLongMod(q.num, field->data.ff.p);
        long long denom = reduceLongLongMod(q.denom, field->data.ff.p);
        if (!inverseLongLongMod(denom, field->data.ff.p, &denomInverse)) return x;

        // Dispatch the scalar conversion through the target field representation
        x.value.ffData.coeffs = malloc(field->data.ff.degree * sizeof(long long));
        if (x.value.ffData.coeffs == NULL) return x;
        x.field = field;
        x.value.ffData.coeffs[0] = multiplyLongLongMod(num, denomInverse, field->data.ff.p);
        for (size_t i = 1; i < field->data.ff.degree; i++) {
            x.value.ffData.coeffs[i] = 0;
        }
    }
    if (field->type == FF_QEXT && fieldHasValidStructure(field)) {
        FieldElement baseValue = fieldElementFromFraction(field->data.ffqext.baseField, q);
        FieldElement zero = zeroFieldElement(field->data.ffqext.baseField);
        if (!fieldElementIsValid(&baseValue) || !fieldElementIsValid(&zero)) {
            freeFieldElement(&baseValue);
            freeFieldElement(&zero);
            return x;
        }
        x = constructFFQExtElement(field, baseValue, zero);
        freeFieldElement(&baseValue);
        freeFieldElement(&zero);
        return x;
    }
    if (field->type == NF && fieldHasValidStructure(field)) {
        FieldElement baseValue = fieldElementFromFraction(field->data.nf.baseField, q);
        if (!fieldElementIsValid(&baseValue)) return x;

        // Dispatch the scalar conversion through the target field representation
        x = zeroFieldElement(field);
        if (!fieldElementIsValid(&x)) {
            freeFieldElement(&baseValue);
            return (FieldElement){0};
        }
        freeFieldElement(&x.value.nfData.coeffs[0]);
        x.value.nfData.coeffs[0] = baseValue;
    }
    return x;
}

// Construct a field element from a long double
FieldElement fieldElementFromDouble(Field* field, long double x) {
    FieldElement y = {0};
    if (!fieldHasValidStructure(field)) return y;

    // Dispatch the scalar conversion through the target field representation
    if (field->type == RR) {
        return constructRRElement(field, x);
    }
    if (field->type == CC) {
        ComplexNumber z = {x, 0.0L};
        return constructCCElement(field, z);
    }
    return y;
}

// Construct a field element from a ComplexNumber
FieldElement fieldElementFromComplex(Field* field, ComplexNumber z) {
    FieldElement x = {0};
    if (!fieldHasValidStructure(field)) return x;

    // Dispatch the scalar conversion through the target field representation
    if (field->type == CC) {
        return constructCCElement(field, z);
    }
    if (field->type == RR && z.imag == 0.0L) {
        return constructRRElement(field, z.real);
    }
    return x;
}

/* ---------- Free methods ---------- */

// Free a Field struct and the objects it owns
void freeField(Field* field) {
    if (field == NULL) return;

    // Release type-specific owned storage before clearing the public struct
    if (field->type == NF) {
        if (field->data.nf.gen.minPolyCoeffs != NULL) {
            for (size_t i = 0; i <= field->data.nf.gen.degree; i++) {
                freeFieldElement(&field->data.nf.gen.minPolyCoeffs[i]);
            }
        }
        free(field->data.nf.gen.minPolyCoeffs);
        field->data.nf.gen.minPolyCoeffs = NULL;
        field->data.nf.gen.degree = 0;
    }
    else if (field->type == FF_QEXT) {
        if (field->data.ffqext.radicand != NULL) {
            freeFieldElement(field->data.ffqext.radicand);
        }
        free(field->data.ffqext.radicand);
        field->data.ffqext.radicand = NULL;
        field->data.ffqext.baseField = NULL;
        field->data.ffqext.genRepr = NULL;
    }
    else if (field->type == FF) {
        free(field->data.ff.modulus);
        field->data.ff.modulus = NULL;
    }
}

// Free a FieldElement struct and the objects it owns
void freeFieldElement(FieldElement* element) {
    if (element == NULL || element->field == NULL) return;

    // Release type-specific owned storage before clearing the public struct
    if (element->field->type == FF) {
        free(element->value.ffData.coeffs);
        element->value.ffData.coeffs = NULL;
    }
    else if (element->field->type == FF_QEXT) {
        if (element->value.ffqextData.coeffs != NULL) {
            freeFieldElement(&element->value.ffqextData.coeffs[0]);
            freeFieldElement(&element->value.ffqextData.coeffs[1]);
        }
        free(element->value.ffqextData.coeffs);
        element->value.ffqextData.coeffs = NULL;
    }
    else if (element->field->type == NF) {
        size_t degree = element->field->data.nf.gen.degree;
        if (element->value.nfData.coeffs != NULL) {
            for (size_t i = 0; i < degree; i++) {
                freeFieldElement(&element->value.nfData.coeffs[i]);
            }
        }
        free(element->value.nfData.coeffs);
        element->value.nfData.coeffs = NULL;
    }

    // Clear the field pointer after releasing representation-specific storage
    element->field = NULL;
}

/* ----------- Print methods ---------- */

// Convert a Field to a string
char* fieldToString(Field* field) {
    if (!fieldHasValidStructure(field)) return formatString("<invalid field>");

    // Dispatch to the representation-specific case for this field type
    switch (field->type) {
        case QQ:
        case RR:
        case CC:
            return formatString("%s", field->repr ? field->repr : "?");
        case FF:
            if (field->data.ff.degree == 1) {
                return formatString("GF(%lld)", field->data.ff.p);
            }
            return formatString("GF(%lld^%zu)", field->data.ff.p, field->data.ff.degree);
        case FF_QEXT: {
            char* base = fieldToString(field->data.ffqext.baseField);
            const char* gen = field->data.ffqext.genRepr ? field->data.ffqext.genRepr : "u";
            char* out = base ? formatString("%s(%s)", base, gen) : NULL;
            free(base);
            return out;
        }
        case NF: {
            char* base = fieldToString(field->data.nf.baseField);
            const char* gen = field->data.nf.gen.repr ? field->data.nf.gen.repr : "a";
            char* out = base ? formatString("%s(%s)", base, gen) : NULL;
            free(base);
            return out;
        }
    }

    // Return the invalid fallback after unsupported field cases
    return formatString("<invalid field>");
}

// Convert a finite field element to a string
static char* finiteFieldElementToString(FieldElement x) {
    if (x.value.ffData.coeffs == NULL || x.field->data.ff.degree == 0) {
        return formatString("<invalid>");
    }

    // Initialize the output string used to collect finite-field terms
    char* string = formatString("");
    if (string == NULL) return NULL;
    size_t length = 0;
    bool firstTerm = true;

    // Append one term for each nonzero finite-field coefficient
    for (size_t i = 0; i < x.field->data.ff.degree; i++) {
        long long coeff = reduceLongLongMod(x.value.ffData.coeffs[i], x.field->data.ff.p);
        if (coeff == 0) continue;

        // Separate nonzero finite-field terms with plus signs
        if (!firstTerm && !appendString(&string, &length, " + ")) {
            free(string);
            return NULL;
        }

        // Append the current finite-field term with the right power of x
        int ok;
        if (i == 0) {
            ok = appendFormat(&string, &length, "%lld", coeff);
        }
        else if (coeff == 1) {
            ok = appendFormat(&string, &length, i == 1 ? "x" : "x^%zu", i);
        }
        else {
            ok = appendFormat(&string, &length, i == 1 ? "%lld*x" : "%lld*x^%zu", coeff, i);
        }
        if (!ok) {
            free(string);
            return NULL;
        }
        firstTerm = false;
    }

    // Return zero when no finite-field coefficient contributed a term
    if (firstTerm) {
        free(string);
        return formatString("0");
    }
    return string;
}

// Convert a number field element to a string
static char* numberFieldElementToString(FieldElement x) {
    if (x.value.nfData.coeffs == NULL) {
        return formatString("<unsupported NF element>");
    }

    // Load the generator metadata used to format powers
    NFGen gen = x.field->data.nf.gen;
    if (gen.degree == 0) return formatString("<invalid>");
    const char* genRepr = gen.repr != NULL ? gen.repr : "a";

    // Initialize the output string used to collect number-field terms
    char* string = formatString("");
    if (string == NULL) return NULL;
    size_t length = 0;
    bool firstTerm = true;

    // Append one term for each nonzero number-field coefficient
    for (size_t i = 0; i < gen.degree; i++) {
        FieldElement coeff = x.value.nfData.coeffs[i];
        FieldElement displayCoeff = {0};
        bool negative = false;
        bool coeffIsOne;
        char* coeffString;
        int ok;

        // Reject invalid coefficients before adding them to the display string
        if (!fieldElementIsValid(&coeff)) {
            free(string);
            return formatString("<invalid>");
        }
        if (fieldElementIsZero(coeff)) continue;

        // Work with a positive display coefficient while remembering the sign
        if (coeff.field->type == QQ && coeff.value.frac.num < 0) {
            negative = true;
            displayCoeff = constructQQElement(coeff.field, negateFraction(coeff.value.frac));
        }
        else {
            displayCoeff = copyFieldElement(coeff);
        }
        if (!fieldElementIsValid(&displayCoeff)) {
            free(string);
            return formatString("<invalid>");
        }

        // Convert the display coefficient before appending its term
        coeffString = fieldElementToString(displayCoeff);
        if (coeffString == NULL) {
            freeFieldElement(&displayCoeff);
            free(string);
            return NULL;
        }

        // Append the leading sign only when the first term is negative
        if (firstTerm) {
            if (negative && !appendString(&string, &length, "-")) {
                free(coeffString);
                freeFieldElement(&displayCoeff);
                free(string);
                return NULL;
            }
        }
        else {
            if (!appendString(&string, &length, negative ? " - " : " + ")) {
                free(coeffString);
                freeFieldElement(&displayCoeff);
                free(string);
                return NULL;
            }
        }

        // Append either a scalar term or a generator-power term
        coeffIsOne = fieldElementIsOne(displayCoeff);
        if (i == 0) {
            ok = appendString(&string, &length, coeffString);
        }
        else if (coeffIsOne) {
            ok = appendFormat(&string, &length, i == 1 ? "%s" : "%s^%zu", genRepr, i);
        }
        else {
            ok = appendFormat(&string, &length, i == 1 ? "%s*%s" : "%s*%s^%zu",
                    coeffString, genRepr, i);
        }

        // Release temporary display data before handling append failure
        free(coeffString);
        freeFieldElement(&displayCoeff);
        if (!ok) {
            free(string);
            return NULL;
        }
        firstTerm = false;
    }

    // Return zero when no number-field coefficient contributed a term
    if (firstTerm) {
        free(string);
        return formatString("0");
    }
    return string;
}

// Convert a quadratic finite field extension element to a string
static char* finiteQuadraticExtensionElementToString(FieldElement x) {
    const char* genRepr;
    char* c0String;
    char* c1String;
    char* string = NULL;
    if (x.value.ffqextData.coeffs == NULL) return formatString("<invalid>");

    // Handle pure scalar and pure generator cases before formatting a sum
    genRepr = x.field->data.ffqext.genRepr != NULL ? x.field->data.ffqext.genRepr : "s";
    if (fieldElementIsZero(x.value.ffqextData.coeffs[1])) {
        return fieldElementToString(x.value.ffqextData.coeffs[0]);
    }
    if (fieldElementIsZero(x.value.ffqextData.coeffs[0])) {
        if (fieldElementIsOne(x.value.ffqextData.coeffs[1])) {
            return formatString("%s", genRepr);
        }
        c1String = fieldElementToString(x.value.ffqextData.coeffs[1]);
        if (c1String == NULL) return NULL;
        string = formatString("%s*%s", c1String, genRepr);
        free(c1String);
        return string;
    }

    // Convert both quadratic-extension coefficients before combining them
    c0String = fieldElementToString(x.value.ffqextData.coeffs[0]);
    c1String = fieldElementToString(x.value.ffqextData.coeffs[1]);
    if (c0String == NULL || c1String == NULL) {
        free(c0String);
        free(c1String);
        return NULL;
    }
    if (fieldElementIsOne(x.value.ffqextData.coeffs[1])) {
        string = formatString("%s + %s", c0String, genRepr);
    }
    else {
        string = formatString("%s + %s*%s", c0String, c1String, genRepr);
    }
    free(c0String);
    free(c1String);
    return string;
}

// Print a FieldElement
void printFieldElement(FieldElement x) {
    char* string = fieldElementToString(x);
    if (string == NULL) return;
    printf("%s", string);
    free(string);
}

// Convert a FieldElement to a string
char* fieldElementToString(FieldElement x) {
    if (x.field == NULL) return formatString("<invalid>");

    // Dispatch to the representation-specific case for this field type
    switch (x.field->type) {
        case QQ:
            return fractionToString(x.value.frac);
        case RR:
            return formatString("%.21Lg", x.value.real);
        case CC:
            if (x.value.z.imag == 0.0L) {
                return formatString("%.21Lg", x.value.z.real);
            }
            if (x.value.z.real == 0.0L) {
                return formatString("%.21Lgi", x.value.z.imag);
            }
            if (x.value.z.imag < 0.0L) {
                return formatString("%.21Lg - %.21Lgi", x.value.z.real,
                        -x.value.z.imag);
            }
            return formatString("%.21Lg + %.21Lgi", x.value.z.real,
                    x.value.z.imag);
        case FF:
            return finiteFieldElementToString(x);
        case FF_QEXT:
            return finiteQuadraticExtensionElementToString(x);
        case NF:
            return numberFieldElementToString(x);
    }

    // Return the invalid fallback after unsupported field cases
    return formatString("<invalid>");
}

/* ----------- Bool methods ---------- */

// Check whether two Fields are equal
bool fieldEq(Field* a, Field* b) {
    if (!fieldHasValidStructure(a) || !fieldHasValidStructure(b)) return false;
    if (a->type != b->type || a->chr != b->chr) return false;

    // Dispatch to the representation-specific case for this field type
    switch (a->type) {
        case QQ:
        case RR:
        case CC:
            return true;
        case FF:
            return finiteFieldEq(a, b);
        case FF_QEXT:
            return finiteQuadraticExtensionEq(a, b);
        case NF:
            return numberFieldEq(a, b);
    }

    // Return the invalid fallback after unsupported field cases
    return false;
}

// Check whether two FieldElements are equal
bool eqFieldElements(FieldElement x, FieldElement y) {
    FieldElement diff;
    bool equal;
    if (!sameField(&x, &y)) return false;

    // Compare by subtracting and testing whether the difference is zero
    diff = subtractFieldElements(x, y);
    equal = fieldElementIsZero(diff);
    freeFieldElement(&diff);
    return equal;
}

// Check whether a FieldElement is valid
bool fieldElementIsValid(FieldElement* x) {
    if (x == NULL || !fieldHasValidStructure(x->field)) return false;

    // Dispatch to the representation-specific case for this field type
    switch (x->field->type) {
        case QQ:
            return x->value.frac.denom != 0;
        case RR:
            return isfinite(x->value.real);
        case CC:
            return isfinite(x->value.z.real) && isfinite(x->value.z.imag);
        case FF:
            return x->value.ffData.coeffs != NULL;
        case FF_QEXT:
            return x->value.ffqextData.coeffs != NULL
                && fieldElementIsValid(&x->value.ffqextData.coeffs[0])
                && fieldElementIsValid(&x->value.ffqextData.coeffs[1])
                && fieldEq(x->field->data.ffqext.baseField,
                    x->value.ffqextData.coeffs[0].field)
                && fieldEq(x->field->data.ffqext.baseField,
                    x->value.ffqextData.coeffs[1].field);
        case NF:
            if (x->value.nfData.coeffs == NULL) return false;
            for (size_t i = 0; i < x->field->data.nf.gen.degree; i++) {
                if (!fieldElementIsValid(&x->value.nfData.coeffs[i])
                        || !fieldEq(x->field->data.nf.baseField,
                            x->value.nfData.coeffs[i].field)) {
                    return false;
                }
            }
            return true;
    }

    // Return the invalid fallback after unsupported field cases
    return false;
}

// Check whether two FieldElements belong to the same Field
bool sameField(FieldElement* x, FieldElement* y) {
    if (!fieldElementIsValid(x) || !fieldElementIsValid(y)) return false;
    return fieldEq(x->field, y->field);
}

// Check whether a FieldElement is zero
bool fieldElementIsZero(FieldElement x) {
    if (!fieldElementIsValid(&x)) return false;

    // Dispatch to the representation-specific case for this field type
    switch (x.field->type) {
        case QQ:
            return x.value.frac.num == 0;
        case RR:
            return x.value.real == 0.0;
        case CC:
            return x.value.z.real == 0.0 && x.value.z.imag == 0.0;
        case FF:
            for (size_t i = 0; i < x.field->data.ff.degree; i++) {
                if (reduceLongLongMod(x.value.ffData.coeffs[i], x.field->data.ff.p) != 0) {
                    return false;
                }
            }
            return true;
        case FF_QEXT:
            return fieldElementIsZero(x.value.ffqextData.coeffs[0])
                && fieldElementIsZero(x.value.ffqextData.coeffs[1]);
        case NF:
            for (size_t i = 0; i < x.field->data.nf.gen.degree; i++) {
                if (!fieldElementIsZero(x.value.nfData.coeffs[i])) {
                    return false;
                }
            }
            return true;
    }

    // Return the invalid fallback after unsupported field cases
    return false;
}

// Check whether a FieldElement is one
bool fieldElementIsOne(FieldElement x) {
    if (!fieldElementIsValid(&x)) return false;

    // Dispatch to the representation-specific case for this field type
    switch (x.field->type) {
        case QQ:
            return eqFraction(x.value.frac, (Fraction){1, 1});
        case RR:
            return x.value.real == 1.0;
        case CC:
            return x.value.z.real == 1.0 && x.value.z.imag == 0.0;
        case FF:
            if (reduceLongLongMod(x.value.ffData.coeffs[0], x.field->data.ff.p) != 1) {
                return false;
            }
            for (size_t i = 1; i < x.field->data.ff.degree; i++) {
                if (reduceLongLongMod(x.value.ffData.coeffs[i], x.field->data.ff.p) != 0) {
                    return false;
                }
            }
            return true;
        case FF_QEXT:
            return fieldElementIsOne(x.value.ffqextData.coeffs[0])
                && fieldElementIsZero(x.value.ffqextData.coeffs[1]);
        case NF:
            if (!fieldElementIsOne(x.value.nfData.coeffs[0])) return false;
            for (size_t i = 1; i < x.field->data.nf.gen.degree; i++) {
                if (!fieldElementIsZero(x.value.nfData.coeffs[i])) {
                    return false;
                }
            }
            return true;
    }

    // Return the invalid fallback after unsupported field cases
    return false;
}

// Check whether a FieldElement is a unit
bool fieldElementIsUnit(FieldElement x) {
    if (!fieldElementIsValid(&x)) return false;
    return !fieldElementIsZero(x);
}

/* ----------- Field helpers ----------- */

// Copy a Field
Field copyField(Field* field) {
    return copyFieldStruct(field);
}

// Copy a FieldElement
FieldElement copyFieldElement(FieldElement x) {
    FieldElement copy = {0};
    if (!fieldElementIsValid(&x)) return copy;

    // Reconstruct a new element using the representation for this field type
    switch (x.field->type) {
        case QQ:
            return constructQQElement(x.field, x.value.frac);
        case RR:
            return constructRRElement(x.field, x.value.real);
        case CC:
            return constructCCElement(x.field, x.value.z);
        case FF:
            return constructFFElement(x.field, x.value.ffData.coeffs);
        case FF_QEXT:
            return constructFFQExtElement(x.field, x.value.ffqextData.coeffs[0],
                    x.value.ffqextData.coeffs[1]);
        case NF:
            copy.field = x.field;
            copy.value.nfData.coeffs =
                calloc(x.field->data.nf.gen.degree, sizeof(FieldElement));
            if (copy.value.nfData.coeffs == NULL) {
                copy.field = NULL;
                return copy;
            }
            for (size_t i = 0; i < x.field->data.nf.gen.degree; i++) {
                copy.value.nfData.coeffs[i] = copyFieldElement(x.value.nfData.coeffs[i]);
                if (!fieldElementIsValid(&copy.value.nfData.coeffs[i])) {
                    freeFieldElement(&copy);
                    return (FieldElement){0};
                }
            }
            return copy;
    }

    // Return the copied field element after representation-specific ownership work
    return copy;
}

// Copy a FieldElement while rebasing it onto an equal target Field
FieldElement copyFieldElementToField(Field* field, FieldElement x) {
    FieldElement copy = {0};
    if (!fieldHasValidStructure(field) || !fieldElementIsValid(&x)
            || !fieldEq(field, x.field)) {
        return copy;
    }

    // Reconstruct the element using the target field's ownership chain
    switch (field->type) {
        case QQ:
            return constructQQElement(field, x.value.frac);
        case RR:
            return constructRRElement(field, x.value.real);
        case CC:
            return constructCCElement(field, x.value.z);
        case FF:
            return constructFFElement(field, x.value.ffData.coeffs);
        case FF_QEXT: {
            FieldElement c0 = copyFieldElementToField(field->data.ffqext.baseField,
                    x.value.ffqextData.coeffs[0]);
            FieldElement c1 = copyFieldElementToField(field->data.ffqext.baseField,
                    x.value.ffqextData.coeffs[1]);
            if (fieldElementIsValid(&c0) && fieldElementIsValid(&c1)) {
                copy = constructFFQExtElement(field, c0, c1);
            }
            freeFieldElement(&c0);
            freeFieldElement(&c1);
            return copy;
        }
        case NF: {
            size_t degree = field->data.nf.gen.degree;
            FieldElement* coeffs = calloc(degree, sizeof(FieldElement));
            if (coeffs == NULL) return copy;
            for (size_t i = 0; i < degree; i++) {
                coeffs[i] = copyFieldElementToField(field->data.nf.baseField,
                        x.value.nfData.coeffs[i]);
                if (!fieldElementIsValid(&coeffs[i])) {
                    freeFieldElementArray(coeffs, i + 1);
                    return (FieldElement){0};
                }
            }
            copy = constructNFElement(field, coeffs, degree);
            freeFieldElementArray(coeffs, degree);
            return copy;
        }
    }

    // Return the invalid fallback after unsupported field cases
    return copy;
}

// Normalize a FieldElement
FieldElement normalizeFieldElement(FieldElement x) {
    return copyFieldElement(x);
}

// Construct the zero element of a Field
FieldElement zeroFieldElement(Field* field) {
    FieldElement zero = {0};
    if (!fieldHasValidStructure(field)) return zero;

    // Dispatch to the representation-specific case for this field type
    switch (field->type) {
        case QQ:
            return constructQQElement(field, zeroFraction());
        case RR:
            return constructRRElement(field, 0.0L);
        case CC: {
            ComplexNumber z = {0.0L, 0.0L};
            return constructCCElement(field, z);
        }
        case FF: {
            long long* coeffs = calloc((size_t)field->data.ff.degree, sizeof(long long));
            if (coeffs == NULL) return zero;
            zero = constructFFElement(field, coeffs);
            free(coeffs);
            return zero;
        }
        case FF_QEXT: {
            FieldElement c0 = zeroFieldElement(field->data.ffqext.baseField);
            FieldElement c1 = zeroFieldElement(field->data.ffqext.baseField);
            if (!fieldElementIsValid(&c0) || !fieldElementIsValid(&c1)) {
                freeFieldElement(&c0);
                freeFieldElement(&c1);
                return zero;
            }
            zero = constructFFQExtElement(field, c0, c1);
            freeFieldElement(&c0);
            freeFieldElement(&c1);
            return zero;
        }
        case NF:
            zero.field = field;
            zero.value.nfData.coeffs =
                calloc(field->data.nf.gen.degree, sizeof(FieldElement));
            if (zero.value.nfData.coeffs == NULL) {
                zero.field = NULL;
                return zero;
            }
            for (size_t i = 0; i < field->data.nf.gen.degree; i++) {
                zero.value.nfData.coeffs[i] = zeroFieldElement(field->data.nf.baseField);
                if (!fieldElementIsValid(&zero.value.nfData.coeffs[i])) {
                    freeFieldElement(&zero);
                    return (FieldElement){0};
                }
            }
            return zero;
    }

    // Return the invalid fallback after unsupported field cases
    return zero;
}

// Construct the one element of a Field
FieldElement oneFieldElement(Field* field) {
    FieldElement one = {0};
    if (!fieldHasValidStructure(field)) return one;

    // Dispatch to the representation-specific case for this field type
    switch (field->type) {
        case QQ:
            return constructQQElement(field, oneFraction());
        case RR:
            return constructRRElement(field, 1.0L);
        case CC: {
            ComplexNumber z = {1.0L, 0.0L};
            return constructCCElement(field, z);
        }
        case FF: {
            long long* coeffs = calloc((size_t)field->data.ff.degree, sizeof(long long));
            if (coeffs == NULL) return one;
            coeffs[0] = 1;
            one = constructFFElement(field, coeffs);
            free(coeffs);
            return one;
        }
        case FF_QEXT: {
            FieldElement c0 = oneFieldElement(field->data.ffqext.baseField);
            FieldElement c1 = zeroFieldElement(field->data.ffqext.baseField);
            if (!fieldElementIsValid(&c0) || !fieldElementIsValid(&c1)) {
                freeFieldElement(&c0);
                freeFieldElement(&c1);
                return one;
            }
            one = constructFFQExtElement(field, c0, c1);
            freeFieldElement(&c0);
            freeFieldElement(&c1);
            return one;
        }
        case NF:
            one.field = field;
            one.value.nfData.coeffs =
                calloc(field->data.nf.gen.degree, sizeof(FieldElement));
            if (one.value.nfData.coeffs == NULL) {
                one.field = NULL;
                return one;
            }
            one.value.nfData.coeffs[0] = oneFieldElement(field->data.nf.baseField);
            if (!fieldElementIsValid(&one.value.nfData.coeffs[0])) {
                freeFieldElement(&one);
                return (FieldElement){0};
            }
            for (size_t i = 1; i < field->data.nf.gen.degree; i++) {
                one.value.nfData.coeffs[i] = zeroFieldElement(field->data.nf.baseField);
                if (!fieldElementIsValid(&one.value.nfData.coeffs[i])) {
                    freeFieldElement(&one);
                    return (FieldElement){0};
                }
            }
            return one;
    }

    // Return the invalid fallback after unsupported field cases
    return one;
}

// Return a square root of a FieldElement when one is available in the same Field
FieldElement fieldElementSquareRoot(FieldElement x) {
    FieldElement root = {0};
    if (!fieldElementIsValid(&x)) return root;

    // Dispatch square-root extraction by field representation
    switch (x.field->type) {
        case QQ: {
            long long numRoot;
            long long denomRoot;
            // A rational square root exists only when numerator and denominator are squares
            if (x.value.frac.num < 0
                    || !perfectSquareLongLong(x.value.frac.num, &numRoot)
                    || !perfectSquareLongLong(x.value.frac.denom, &denomRoot)) {
                return (FieldElement){0};
            }
            return constructQQElement(x.field, constructFraction(numRoot, denomRoot));
        }
        case RR:
            if (x.value.real < 0.0L) return (FieldElement){0};
            return constructRRElement(x.field, sqrtl(x.value.real));
        case CC:
            return constructCCElement(x.field, complexSqrt(x.value.z));
        case FF: {
            size_t order = ffOrder(x.field);
            // Search the finite field explicitly; this keeps FF square roots simple and exact
            for (size_t i = 0; i < order; i++) {
                long long* coeffs = ffCoeffsFromIndex(x.field, i);
                FieldElement candidate;
                FieldElement square;
                if (coeffs == NULL) return (FieldElement){0};
                candidate = constructFFElement(x.field, coeffs);
                free(coeffs);
                if (!fieldElementIsValid(&candidate)) return (FieldElement){0};
                square = multiplyFieldElements(candidate, candidate);
                if (fieldElementIsValid(&square) && eqFieldElements(square, x)) {
                    freeFieldElement(&square);
                    return candidate;
                }
                freeFieldElement(&square);
                freeFieldElement(&candidate);
            }
            return (FieldElement){0};
        }
        case FF_QEXT:
            return (FieldElement){0};
        case NF: {
            Field* baseField = x.field->data.nf.baseField;
            size_t degree = nfDegree(x.field);
            bool isConstant = true;
            FieldElement constantRoot = {0};
            FieldElement radicand = {0};
            // First detect whether the element is really from the base field
            for (size_t i = 1; i < degree; i++) {
                if (!fieldElementIsZero(x.value.nfData.coeffs[i])) {
                    isConstant = false;
                    break;
                }
            }

            // Try a base-field square root before using quadratic-extension formulas
            if (isConstant) {
                constantRoot = fieldElementSquareRoot(x.value.nfData.coeffs[0]);
                if (fieldElementIsValid(&constantRoot)) {
                    root = embedFieldElement(x.field, constantRoot);
                    freeFieldElement(&constantRoot);
                    return root;
                }
            }
            // Beyond constants, only simple quadratic generators have a closed-form path here
            if (degree != 2 || !fieldHasSimpleQuadraticGenerator(x.field, &radicand)) {
                freeFieldElement(&constantRoot);
                return (FieldElement){0};
            }

            // Pull out the quadratic coefficients before trying extension-specific roots
            FieldElement c0 = x.value.nfData.coeffs[0];
            FieldElement c1 = x.value.nfData.coeffs[1];
            // If the element is a base multiple of the radicand, try a pure generator root
            if (fieldElementIsZero(c1)) {
                FieldElement quotient = divideFieldElements(c0, radicand);
                FieldElement y1 = fieldElementSquareRoot(quotient);
                if (fieldElementIsValid(&y1)) {
                    FieldElement zero = zeroFieldElement(baseField);
                    FieldElement coeffs[2];
                    if (fieldElementIsValid(&zero)) {
                        coeffs[0] = zero;
                        coeffs[1] = y1;
                        root = constructNFElement(x.field, coeffs, 2);
                        if (fieldElementIsValid(&root)) {
                            freeFieldElement(&zero);
                            freeFieldElement(&y1);
                            freeFieldElement(&quotient);
                            freeFieldElement(&radicand);
                            return root;
                        }
                        freeFieldElement(&root);
                    }
                    freeFieldElement(&zero);
                }
                freeFieldElement(&quotient);
                freeFieldElement(&y1);
            }

            // Compute the quadratic formula ingredients for a square root in base coordinates
            FieldElement c0Sq = multiplyFieldElements(c0, c0);
            FieldElement c1Sq = multiplyFieldElements(c1, c1);
            FieldElement dC1Sq = multiplyFieldElements(radicand, c1Sq);
            FieldElement disc = subtractFieldElements(c0Sq, dC1Sq);
            FieldElement sqrtDisc = fieldElementSquareRoot(disc);
            FieldElement half = fieldElementFromFraction(baseField, constructFraction(1, 2));
            FieldElement signTerms[2] = {0};

            // Use the quadratic equations for (y0 + y1*u)^2 = c0 + c1*u
            if (!fieldElementIsValid(&c0Sq) || !fieldElementIsValid(&c1Sq)
                    || !fieldElementIsValid(&dC1Sq) || !fieldElementIsValid(&disc)
                    || !fieldElementIsValid(&sqrtDisc) || !fieldElementIsValid(&half)) {
                freeFieldElement(&radicand);
                freeFieldElement(&c0Sq);
                freeFieldElement(&c1Sq);
                freeFieldElement(&dC1Sq);
                freeFieldElement(&disc);
                freeFieldElement(&sqrtDisc);
                freeFieldElement(&half);
                return (FieldElement){0};
            }

            // Build the two candidate signs for y0^2
            signTerms[0] = addFieldElements(c0, sqrtDisc);
            signTerms[1] = subtractFieldElements(c0, sqrtDisc);
            // Try both choices for y0^2, then verify the square to reject extraneous roots
            for (size_t i = 0; i < 2; i++) {
                FieldElement y0Sq = multiplyFieldElements(half, signTerms[i]);
                FieldElement y0 = fieldElementSquareRoot(y0Sq);
                if (fieldElementIsValid(&y0) && !fieldElementIsZero(y0)) {
                    FieldElement twoY0 = addFieldElements(y0, y0);
                    FieldElement y1 = divideFieldElements(c1, twoY0);
                    FieldElement coeffs[2];
                    FieldElement square;
                    if (fieldElementIsValid(&twoY0) && fieldElementIsValid(&y1)) {
                        coeffs[0] = y0;
                        coeffs[1] = y1;
                        root = constructNFElement(x.field, coeffs, 2);
                        square = multiplyFieldElements(root, root);
                        if (fieldElementIsValid(&root) && fieldElementIsValid(&square)
                                && eqFieldElements(square, x)) {
                            freeFieldElement(&square);
                            freeFieldElement(&twoY0);
                            freeFieldElement(&y0Sq);
                            freeFieldElement(&signTerms[0]);
                            freeFieldElement(&signTerms[1]);
                            freeFieldElement(&radicand);
                            freeFieldElement(&c0Sq);
                            freeFieldElement(&c1Sq);
                            freeFieldElement(&dC1Sq);
                            freeFieldElement(&disc);
                            freeFieldElement(&sqrtDisc);
                            freeFieldElement(&half);
                            return root;
                        }
                        freeFieldElement(&root);
                        freeFieldElement(&square);
                    }
                    freeFieldElement(&twoY0);
                    freeFieldElement(&y1);
                }
                freeFieldElement(&y0);
                freeFieldElement(&y0Sq);
            }

            // Release quadratic square-root temporaries before reporting no root
            freeFieldElement(&signTerms[0]);
            freeFieldElement(&signTerms[1]);
            freeFieldElement(&radicand);
            freeFieldElement(&c0Sq);
            freeFieldElement(&c1Sq);
            freeFieldElement(&dC1Sq);
            freeFieldElement(&disc);
            freeFieldElement(&sqrtDisc);
            freeFieldElement(&half);
            return (FieldElement){0};
        }
    }

    // Return the square root found by the representation-specific branch
    return root;
}

// Embed a FieldElement into another compatible Field as a constant
FieldElement embedFieldElement(Field* targetField, FieldElement x) {
    FieldElement embedded = {0};
    if (!fieldHasValidStructure(targetField) || !fieldElementIsValid(&x)) return embedded;

    // Rebase elements from an equal field onto the requested target field
    if (fieldEq(targetField, x.field)) return copyFieldElementToField(targetField, x);

    // Coerce rational elements into real scalars when requested
    if (targetField->type == RR && x.field->type == QQ) {
        return constructRRElement(targetField,
                (long double)x.value.frac.num / (long double)x.value.frac.denom);
    }
    if (targetField->type == CC) {
        if (x.field->type == QQ) {
            ComplexNumber z = {
                (long double)x.value.frac.num / (long double)x.value.frac.denom, 0.0
            };
            return constructCCElement(targetField, z);
        }
        if (x.field->type == RR) {
            ComplexNumber z = {x.value.real, 0.0L};
            return constructCCElement(targetField, z);
        }
    }
    if (targetField->type == FF_QEXT
            && fieldEq(targetField->data.ffqext.baseField, x.field)) {
        FieldElement zero = zeroFieldElement(targetField->data.ffqext.baseField);
        if (!fieldElementIsValid(&zero)) return embedded;
        embedded = constructFFQExtElement(targetField, x, zero);
        freeFieldElement(&zero);
        return embedded;
    }
    if (targetField->type == NF && fieldEq(targetField->data.nf.baseField, x.field)) {
        size_t degree = nfDegree(targetField);
        FieldElement* coeffs;
        if (degree == 0) return embedded;

        // Build constant-extension coordinates with x in degree zero
        coeffs = calloc(degree, sizeof(FieldElement));
        if (coeffs == NULL) return embedded;
        coeffs[0] = copyFieldElement(x);
        if (!fieldElementIsValid(&coeffs[0])) {
            free(coeffs);
            return (FieldElement){0};
        }
        for (size_t i = 1; i < degree; i++) {
            coeffs[i] = zeroFieldElement(targetField->data.nf.baseField);
            if (!fieldElementIsValid(&coeffs[i])) {
                freeFieldElementArray(coeffs, i + 1);
                return (FieldElement){0};
            }
        }

        // Construct the embedded number-field element and release temporary coordinates
        embedded = constructNFElement(targetField, coeffs, degree);
        freeFieldElementArray(coeffs, degree);
    }
    return embedded;
}

// Return the generator u of a quadratic extension field
FieldElement quadraticExtensionGenerator(Field* field) {
    FieldElement u = {0};
    FieldElement zero = {0};
    FieldElement one = {0};
    FieldElement coeffs[2];
    if (!fieldHasValidStructure(field)) return u;

    // Build the finite quadratic generator from base-field zero and one
    if (field->type == FF_QEXT) {
        zero = zeroFieldElement(field->data.ffqext.baseField);
        one = oneFieldElement(field->data.ffqext.baseField);
        if (!fieldElementIsValid(&zero) || !fieldElementIsValid(&one)) {
            freeFieldElement(&zero);
            freeFieldElement(&one);
            return (FieldElement){0};
        }
        u = constructFFQExtElement(field, zero, one);
        freeFieldElement(&zero);
        freeFieldElement(&one);
        return u;
    }
    if (field->type != NF || nfDegree(field) != 2) return u;

    // Build the number-field generator coordinates over the base field
    zero = zeroFieldElement(field->data.nf.baseField);
    one = oneFieldElement(field->data.nf.baseField);
    if (!fieldElementIsValid(&zero) || !fieldElementIsValid(&one)) {
        freeFieldElement(&zero);
        freeFieldElement(&one);
        return (FieldElement){0};
    }

    // Construct the generator element from coordinates (0, 1)
    coeffs[0] = zero;
    coeffs[1] = one;
    u = constructNFElement(field, coeffs, 2);
    freeFieldElement(&coeffs[0]);
    freeFieldElement(&coeffs[1]);
    return u;
}

/* ----------- Field arithmetic ---------- */

// Add two FieldElements
FieldElement addFieldElements(FieldElement x, FieldElement y) {
    FieldElement sum = {0};
    if (!sameField(&x, &y)) return sum;

    // Dispatch to the representation-specific case for this field type
    switch (x.field->type) {
        case QQ:
            return constructQQElement(x.field, addFractions(x.value.frac, y.value.frac));
        case RR:
            return constructRRElement(x.field, x.value.real + y.value.real);
        case CC:
            return constructCCElement(x.field, complexAdd(x.value.z, y.value.z));
        case FF: {
            long long* coeffs = addFFCoeffs(x.field, x.value.ffData.coeffs,
                    y.value.ffData.coeffs);
            if (coeffs == NULL) return sum;
            sum = constructFFElement(x.field, coeffs);
            free(coeffs);
            return sum;
        }
        case FF_QEXT: {
            FieldElement c0 = addFieldElements(x.value.ffqextData.coeffs[0],
                    y.value.ffqextData.coeffs[0]);
            FieldElement c1 = addFieldElements(x.value.ffqextData.coeffs[1],
                    y.value.ffqextData.coeffs[1]);
            if (!fieldElementIsValid(&c0) || !fieldElementIsValid(&c1)) {
                freeFieldElement(&c0);
                freeFieldElement(&c1);
                return sum;
            }
            sum = constructFFQExtElement(x.field, c0, c1);
            freeFieldElement(&c0);
            freeFieldElement(&c1);
            return sum;
        }
        case NF:
            sum.field = x.field;
            sum.value.nfData.coeffs = addNFCoeffs(x.field, x.value.nfData.coeffs,
                    y.value.nfData.coeffs);
            if (sum.value.nfData.coeffs == NULL) sum.field = NULL;
            return sum;
    }

    // Return the invalid fallback after unsupported field cases
    return sum;
}

// Subtract two FieldElements
FieldElement subtractFieldElements(FieldElement x, FieldElement y) {
    FieldElement diff = {0};
    if (!sameField(&x, &y)) return diff;

    // Dispatch to the representation-specific case for this field type
    switch (x.field->type) {
        case QQ:
            return constructQQElement(x.field, subtractFractions(x.value.frac, y.value.frac));
        case RR:
            return constructRRElement(x.field, x.value.real - y.value.real);
        case CC:
            return constructCCElement(x.field, complexSub(x.value.z, y.value.z));
        case FF: {
            long long* coeffs = subtractFFCoeffs(x.field, x.value.ffData.coeffs,
                    y.value.ffData.coeffs);
            if (coeffs == NULL) return diff;
            diff = constructFFElement(x.field, coeffs);
            free(coeffs);
            return diff;
        }
        case FF_QEXT: {
            FieldElement c0 = subtractFieldElements(x.value.ffqextData.coeffs[0],
                    y.value.ffqextData.coeffs[0]);
            FieldElement c1 = subtractFieldElements(x.value.ffqextData.coeffs[1],
                    y.value.ffqextData.coeffs[1]);
            if (!fieldElementIsValid(&c0) || !fieldElementIsValid(&c1)) {
                freeFieldElement(&c0);
                freeFieldElement(&c1);
                return diff;
            }
            diff = constructFFQExtElement(x.field, c0, c1);
            freeFieldElement(&c0);
            freeFieldElement(&c1);
            return diff;
        }
        case NF:
            diff.field = x.field;
            diff.value.nfData.coeffs = subtractNFCoeffs(x.field, x.value.nfData.coeffs,
                    y.value.nfData.coeffs);
            if (diff.value.nfData.coeffs == NULL) diff.field = NULL;
            return diff;
    }

    // Return the invalid fallback after unsupported field cases
    return diff;
}

// Negate a FieldElement
FieldElement negateFieldElement(FieldElement x) {
    FieldElement neg = {0};
    if (!fieldElementIsValid(&x)) return neg;

    // Dispatch to the representation-specific case for this field type
    switch (x.field->type) {
        case QQ:
            return constructQQElement(x.field, negateFraction(x.value.frac));
        case RR:
            return constructRRElement(x.field, -x.value.real);
        case CC:
            return constructCCElement(x.field, complexNeg(x.value.z));
        case FF: {
            long long* coeffs = negateFFCoeffs(x.field, x.value.ffData.coeffs);
            if (coeffs == NULL) return neg;
            neg = constructFFElement(x.field, coeffs);
            free(coeffs);
            return neg;
        }
        case FF_QEXT: {
            FieldElement c0 = negateFieldElement(x.value.ffqextData.coeffs[0]);
            FieldElement c1 = negateFieldElement(x.value.ffqextData.coeffs[1]);
            if (!fieldElementIsValid(&c0) || !fieldElementIsValid(&c1)) {
                freeFieldElement(&c0);
                freeFieldElement(&c1);
                return neg;
            }
            neg = constructFFQExtElement(x.field, c0, c1);
            freeFieldElement(&c0);
            freeFieldElement(&c1);
            return neg;
        }
        case NF:
            neg.field = x.field;
            neg.value.nfData.coeffs = negateNFCoeffs(x.field, x.value.nfData.coeffs);
            if (neg.value.nfData.coeffs == NULL) neg.field = NULL;
            return neg;
    }

    // Return the invalid fallback after unsupported field cases
    return neg;
}

// Multiply two FieldElements
FieldElement multiplyFieldElements(FieldElement x, FieldElement y) {
    FieldElement product = {0};
    if (!sameField(&x, &y)) return product;

    // Dispatch to the representation-specific case for this field type
    switch (x.field->type) {
        case QQ:
            return constructQQElement(x.field, multiplyFractions(x.value.frac, y.value.frac));
        case RR:
            return constructRRElement(x.field, x.value.real * y.value.real);
        case CC:
            return constructCCElement(x.field, complexMul(x.value.z, y.value.z));
        case FF: {
            long long* coeffs = multiplyFFCoeffs(x.field, x.value.ffData.coeffs,
                    y.value.ffData.coeffs);
            if (coeffs == NULL) return product;
            product = constructFFElement(x.field, coeffs);
            free(coeffs);
            return product;
        }
        case FF_QEXT: {
            FieldElement x0y0 = multiplyFieldElements(x.value.ffqextData.coeffs[0],
                    y.value.ffqextData.coeffs[0]);
            FieldElement x1y1 = multiplyFieldElements(x.value.ffqextData.coeffs[1],
                    y.value.ffqextData.coeffs[1]);
            FieldElement ax1y1 = multiplyFieldElements(*x.field->data.ffqext.radicand, x1y1);
            FieldElement c0 = addFieldElements(x0y0, ax1y1);
            FieldElement x0y1 = multiplyFieldElements(x.value.ffqextData.coeffs[0],
                    y.value.ffqextData.coeffs[1]);
            FieldElement x1y0 = multiplyFieldElements(x.value.ffqextData.coeffs[1],
                    y.value.ffqextData.coeffs[0]);
            FieldElement c1 = addFieldElements(x0y1, x1y0);
            if (!fieldElementIsValid(&x0y0) || !fieldElementIsValid(&x1y1)
                    || !fieldElementIsValid(&ax1y1) || !fieldElementIsValid(&c0)
                    || !fieldElementIsValid(&x0y1) || !fieldElementIsValid(&x1y0)
                    || !fieldElementIsValid(&c1)) {
                freeFieldElement(&x0y0);
                freeFieldElement(&x1y1);
                freeFieldElement(&ax1y1);
                freeFieldElement(&c0);
                freeFieldElement(&x0y1);
                freeFieldElement(&x1y0);
                freeFieldElement(&c1);
                return product;
            }
            product = constructFFQExtElement(x.field, c0, c1);
            freeFieldElement(&x0y0);
            freeFieldElement(&x1y1);
            freeFieldElement(&ax1y1);
            freeFieldElement(&c0);
            freeFieldElement(&x0y1);
            freeFieldElement(&x1y0);
            freeFieldElement(&c1);
            return product;
        }
        case NF:
            product.field = x.field;
            product.value.nfData.coeffs = multiplyNFCoeffs(x.field, x.value.nfData.coeffs,
                    y.value.nfData.coeffs);
            if (product.value.nfData.coeffs == NULL) product.field = NULL;
            return product;
    }

    // Return the product assembled by the representation-specific branch
    return product;
}

// Invert a FieldElement
FieldElement invertFieldElement(FieldElement x) {
    FieldElement inverse = {0};
    if (!fieldElementIsValid(&x) || fieldElementIsZero(x)) return inverse;

    // Dispatch to the representation-specific case for this field type
    switch (x.field->type) {
        case QQ:
            return constructQQElement(x.field, divideFractions(oneFraction(), x.value.frac));
        case RR:
            return constructRRElement(x.field, 1.0 / x.value.real);
        case CC: {
            ComplexNumber one = {1.0L, 0.0L};
            return constructCCElement(x.field, complexDiv(one, x.value.z));
        }
        case FF: {
            long long* coeffs = invertFFCoeffs(x.field, x.value.ffData.coeffs);
            if (coeffs == NULL) return inverse;
            inverse = constructFFElement(x.field, coeffs);
            free(coeffs);
            return inverse;
        }
        case FF_QEXT: {
            FieldElement conj0 = copyFieldElement(x.value.ffqextData.coeffs[0]);
            FieldElement conj1 = negateFieldElement(x.value.ffqextData.coeffs[1]);
            FieldElement x0Sq = multiplyFieldElements(x.value.ffqextData.coeffs[0],
                    x.value.ffqextData.coeffs[0]);
            FieldElement x1Sq = multiplyFieldElements(x.value.ffqextData.coeffs[1],
                    x.value.ffqextData.coeffs[1]);
            FieldElement ax1Sq = multiplyFieldElements(*x.field->data.ffqext.radicand, x1Sq);
            FieldElement norm = subtractFieldElements(x0Sq, ax1Sq);
            FieldElement normInv = invertFieldElement(norm);
            FieldElement c0 = multiplyFieldElements(conj0, normInv);
            FieldElement c1 = multiplyFieldElements(conj1, normInv);
            if (!fieldElementIsValid(&conj0) || !fieldElementIsValid(&conj1)
                    || !fieldElementIsValid(&x0Sq) || !fieldElementIsValid(&x1Sq)
                    || !fieldElementIsValid(&ax1Sq) || !fieldElementIsValid(&norm)
                    || !fieldElementIsValid(&normInv) || !fieldElementIsValid(&c0)
                    || !fieldElementIsValid(&c1)) {
                freeFieldElement(&conj0);
                freeFieldElement(&conj1);
                freeFieldElement(&x0Sq);
                freeFieldElement(&x1Sq);
                freeFieldElement(&ax1Sq);
                freeFieldElement(&norm);
                freeFieldElement(&normInv);
                freeFieldElement(&c0);
                freeFieldElement(&c1);
                return inverse;
            }
            inverse = constructFFQExtElement(x.field, c0, c1);
            freeFieldElement(&conj0);
            freeFieldElement(&conj1);
            freeFieldElement(&x0Sq);
            freeFieldElement(&x1Sq);
            freeFieldElement(&ax1Sq);
            freeFieldElement(&norm);
            freeFieldElement(&normInv);
            freeFieldElement(&c0);
            freeFieldElement(&c1);
            return inverse;
        }
        case NF:
            inverse.field = x.field;
            inverse.value.nfData.coeffs = invertNFCoeffs(x.field, x.value.nfData.coeffs);
            if (inverse.value.nfData.coeffs == NULL) inverse.field = NULL;
            return inverse;
    }

    // Return the inverse assembled by the representation-specific branch
    return inverse;
}

// Divide two FieldElements
FieldElement divideFieldElements(FieldElement x, FieldElement y) {
    FieldElement quotient = {0};
    FieldElement inverse;
    if (!sameField(&x, &y) || fieldElementIsZero(y)) return quotient;

    // Invert the divisor before multiplying on the right
    inverse = invertFieldElement(y);
    if (!fieldElementIsValid(&inverse)) return quotient;

    // Multiply by the inverse and release it before returning
    quotient = multiplyFieldElements(x, inverse);
    freeFieldElement(&inverse);
    return quotient;
}
