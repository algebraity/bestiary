#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "hebi.h"

static int testsRun = 0;
static int testsPassed = 0;

#define CHECK(cond, name) do { \
    testsRun++; \
    if (cond) { testsPassed++; printf("  [PASS] %s\n", name); } \
    else { printf("  [FAIL] %s (line %d)\n", name, __LINE__); } \
} while (0)

#define SECTION(name) printf("\n=== %s ===\n", name)

static bool closeReal(long double a, long double b) {
    return fabsl(a - b) <= 1e-12;
}

static bool isInt(Number n, long long expected) {
    return n.type == NUMBER_INT && n.as.i == expected;
}

static bool isReal(Number n, long double expected) {
    return n.type == NUMBER_REAL && closeReal(n.as.x, expected);
}

static bool isFraction(Number n, long long num, long long denom) {
    return n.type == NUMBER_FRACTION && n.as.frac.num == num && n.as.frac.denom == denom;
}

static bool isComplex(Number n, long double real, long double imag) {
    return n.type == NUMBER_COMPLEX
        && closeReal(n.as.z.real, real)
        && closeReal(n.as.z.imag, imag);
}

static bool printToBuffer(Number n, char* buf, size_t bufSize) {
    if (!buf || bufSize == 0) return false;
    buf[0] = '\0';

    FILE* tmp = tmpfile();
    if (!tmp) return false;

    fflush(stdout);
    int saved = dup(STDOUT_FILENO);
    if (saved < 0) {
        fclose(tmp);
        return false;
    }

    if (dup2(fileno(tmp), STDOUT_FILENO) < 0) {
        close(saved);
        fclose(tmp);
        return false;
    }

    printNumber(n);
    fflush(stdout);

    if (dup2(saved, STDOUT_FILENO) < 0) {
        close(saved);
        fclose(tmp);
        return false;
    }
    close(saved);

    fseek(tmp, 0, SEEK_SET);
    size_t nread = fread(buf, 1, bufSize - 1, tmp);
    buf[nread] = '\0';
    fclose(tmp);
    return true;
}

static void testConstructNumberFromInt(void) {
    SECTION("constructNumberFromInt");
    CHECK(isInt(constructNumberFromInt(0), 0), "construct zero int");
    CHECK(isInt(constructNumberFromInt(42), 42), "construct positive int");
    CHECK(isInt(constructNumberFromInt(-17), -17), "construct negative int");
}

static void testConstructNumberFromDouble(void) {
    SECTION("constructNumberFromDouble");
    CHECK(isReal(constructNumberFromDouble(2.5), 2.5), "construct positive real");
    CHECK(isReal(constructNumberFromDouble(-0.25), -0.25), "construct negative real");
    CHECK(constructNumberFromDouble(NAN).type == NUMBER_NAN, "construct NAN real");
}

static void testConstructNumberFromFraction(void) {
    SECTION("constructNumberFromFraction");
    CHECK(isFraction(constructNumberFromFraction(constructFraction(1, 2)), 1, 2), "construct fraction");
    CHECK(isFraction(constructNumberFromFraction(constructFraction(-2, 4)), -1, 2), "construct normalized fraction");
    CHECK(constructNumberFromFraction((Fraction){1, 0}).type == NUMBER_NAN, "reject invalid fraction");
}

static void testConstructNumberFromComplex(void) {
    SECTION("constructNumberFromComplex");
    CHECK(isComplex(constructNumberFromComplex((ComplexNumber){1.0, 2.0}), 1.0, 2.0), "construct complex");
    CHECK(isComplex(constructNumberFromComplex((ComplexNumber){3.0, 0.0}), 3.0, 0.0), "construct real-valued complex");
    CHECK(constructNumberFromComplex((ComplexNumber){NAN, 1.0}).type == NUMBER_NAN, "reject complex NAN");
}

static void testAddNumbers(void) {
    SECTION("addNumbers");
    CHECK(isInt(addNumbers(constructNumberFromInt(2), constructNumberFromInt(3)), 5), "int + int");
    CHECK(isFraction(addNumbers(constructNumberFromFraction(constructFraction(1, 2)), constructNumberFromInt(1)), 3, 2), "fraction + int");
    CHECK(isComplex(addNumbers(constructNumberFromComplex((ComplexNumber){1.0, 2.0}), constructNumberFromDouble(0.5)), 1.5, 2.0), "complex + real");
}

static void testSubNumbers(void) {
    SECTION("subNumbers");
    CHECK(isInt(subNumbers(constructNumberFromInt(7), constructNumberFromInt(3)), 4), "int - int");
    CHECK(isFraction(subNumbers(constructNumberFromInt(1), constructNumberFromFraction(constructFraction(3, 4))), 1, 4), "int - fraction");
    CHECK(isComplex(subNumbers(constructNumberFromComplex((ComplexNumber){1.0, 2.0}), constructNumberFromComplex((ComplexNumber){3.0, -4.0})), -2.0, 6.0), "complex - complex");
}

static void testMultNumbers(void) {
    SECTION("multNumbers");
    CHECK(isInt(multNumbers(constructNumberFromInt(6), constructNumberFromInt(7)), 42), "int * int");
    CHECK(isFraction(multNumbers(constructNumberFromFraction(constructFraction(2, 3)), constructNumberFromFraction(constructFraction(9, 4))), 3, 2), "fraction * fraction");
    CHECK(isComplex(multNumbers(constructNumberFromComplex((ComplexNumber){1.0, 2.0}), constructNumberFromComplex((ComplexNumber){3.0, 4.0})), -5.0, 10.0), "complex * complex");
}

static void testDivNumbers(void) {
    SECTION("divNumbers");
    CHECK(isInt(divNumbers(constructNumberFromInt(8), constructNumberFromInt(2)), 4), "exact int / int");
    CHECK(isFraction(divNumbers(constructNumberFromInt(3), constructNumberFromInt(2)), 3, 2), "nonexact int / int");
    CHECK(divNumbers(constructNumberFromInt(1), constructNumberFromInt(0)).type == NUMBER_NAN, "division by zero");
}

static void testCompNumbers(void) {
    SECTION("compNumbers");
    CHECK(compNumbers(constructNumberFromInt(1), constructNumberFromInt(2)) < 0, "compare int less");
    CHECK(compNumbers(constructNumberFromFraction(constructFraction(3, 2)), constructNumberFromDouble(1.25)) > 0, "compare fraction and real");
    CHECK(compNumbers(constructNumberFromComplex((ComplexNumber){1.0, 2.0}), constructNumberFromComplex((ComplexNumber){1.0, 3.0})) < 0, "compare complex lexicographically");
}

static void testEqNumbers(void) {
    SECTION("eqNumbers");
    CHECK(eqNumbers(constructNumberFromInt(2), constructNumberFromFraction(constructFraction(4, 2))), "int equals fraction");
    CHECK(eqNumbers(constructNumberFromDouble(2.5), constructNumberFromFraction(constructFraction(5, 2))), "real equals fraction");
    CHECK(!eqNumbers(constructNumberFromComplex((ComplexNumber){1.0, 1.0}), constructNumberFromComplex((ComplexNumber){1.0, 2.0})), "different complex numbers not equal");
}

static void testFreeNumber(void) {
    SECTION("freeNumber");
    freeNumber(constructNumberFromInt(1));
    CHECK(true, "free int number");
    freeNumber(constructNumberFromFraction(constructFraction(1, 2)));
    CHECK(true, "free fraction number");
    freeNumber(constructNumberFromComplex((ComplexNumber){1.0, 2.0}));
    CHECK(true, "free complex number");
}

static void testPrintNumber(void) {
    SECTION("printNumber");
    char buf[128];

    CHECK(printToBuffer(constructNumberFromInt(12), buf, sizeof(buf)) && strcmp(buf, "12") == 0, "print int");
    CHECK(printToBuffer(constructNumberFromFraction(constructFraction(3, 4)), buf, sizeof(buf)) && strcmp(buf, "3/4") == 0, "print fraction");
    CHECK(printToBuffer(constructNumberFromDouble(2.5), buf, sizeof(buf)) && strcmp(buf, "2.5") == 0, "print real");
}

static void testSpecialFunctions(void) {
    SECTION("special functions");

    ComplexNumber erfI = complexErf((ComplexNumber){0.0L, 1.0L});
    ComplexNumber eiOne = complexEi((ComplexNumber){1.0L, 0.0L});
    ComplexNumber eiI = complexEi((ComplexNumber){0.0L, 1.0L});

    CHECK(closeReal(realErf(0.0L), 0.0L), "real erf at zero");
    CHECK(closeReal(realErf(1.0L), 0.84270079294971486934L), "real erf at one");
    CHECK(closeReal(realEi(1.0L), 1.89511781635593675547L), "real Ei at one");
    CHECK(closeReal(realEi(-1.0L), -0.21938393439552027368L), "real Ei at negative one");

    CHECK(closeReal(erfI.real, 0.0L) && closeReal(erfI.imag, 1.6504257587975428760L), "complex erf at i");
    CHECK(closeReal(eiOne.real, realEi(1.0L)) && closeReal(eiOne.imag, 0.0L), "complex Ei agrees on positive real input");
    CHECK(closeReal(eiI.real, 0.33740392290096813466L) && closeReal(eiI.imag, 2.5168793971620796342L), "complex Ei at i");
}

static void testPRG(void) {
    SECTION("PRG");
    seedPRG(12345ULL);
    unsigned long long first = stepPRG();
    unsigned long long second = stepPRG();
    seedPRG(12345ULL);
    CHECK(stepPRG() == first, "seed repeats first value");
    CHECK(stepPRG() == second, "seed repeats second value");
    CHECK(first != second, "PRG advances state");

    resetPRG();
    first = stepPRG();
    resetPRG();
    CHECK(stepPRG() == first, "reset repeats default sequence");
}

static void testRandomInt(void) {
    SECTION("randomInt");
    seedPRG(9876ULL);
    long long first = randomInt(-3, 3);
    bool inRange = first >= -3 && first <= 3;
    for (int i = 0; i < 100; i++) {
        long long x = randomInt(-3, 3);
        inRange = inRange && x >= -3 && x <= 3;
    }
    CHECK(inRange, "integer values stay in range");

    seedPRG(9876ULL);
    CHECK(randomInt(-3, 3) == first, "integer generation is deterministic");
    CHECK(randomInt(5, 5) == 5, "single integer range returns bound");
    CHECK(randomInt(6, 5) == 0, "invalid integer range returns zero");
}

static void testRandomReal(void) {
    SECTION("randomReal");
    seedPRG(2222ULL);
    long double first = randomReal(-2.0L, 5.0L);
    bool inRange = first >= -2.0L && first < 5.0L;
    for (int i = 0; i < 100; i++) {
        long double x = randomReal(-2.0L, 5.0L);
        inRange = inRange && x >= -2.0L && x < 5.0L;
    }
    CHECK(inRange, "real values stay in interval");

    seedPRG(2222ULL);
    CHECK(closeReal(randomReal(-2.0L, 5.0L), first), "real generation is deterministic");
    CHECK(closeReal(randomReal(1.25L, 1.25L), 1.25L), "single real interval returns bound");
    CHECK(isnan(randomReal(2.0L, 1.0L)), "invalid real interval returns NAN");
}

static void testRandomFraction(void) {
    SECTION("randomFraction");
    seedPRG(3333ULL);
    Fraction first = randomFraction(-4, 4, -3, 3);
    bool inRange = first.denom != 0
        && first.num >= -4 && first.num <= 4
        && first.denom >= -3 && first.denom <= 3;
    for (int i = 0; i < 100; i++) {
        Fraction x = randomFraction(-4, 4, -3, 3);
        inRange = inRange
            && x.denom != 0
            && x.num >= -4 && x.num <= 4
            && x.denom >= -3 && x.denom <= 3;
    }
    CHECK(inRange, "fraction values stay in ranges");

    seedPRG(3333ULL);
    Fraction repeat = randomFraction(-4, 4, -3, 3);
    CHECK(first.num == repeat.num && first.denom == repeat.denom, "fraction generation is deterministic");
    CHECK(randomFraction(1, 1, 0, 0).denom == 0, "zero-only denominator range is invalid");
    CHECK(randomFraction(2, 1, 1, 2).denom == 0, "invalid numerator range is invalid");
}

static void testRandomComplex(void) {
    SECTION("randomComplex");
    seedPRG(4444ULL);
    ComplexNumber first = randomComplex(-1.0L, 1.0L, 2.0L, 4.0L);
    bool inRange = first.real >= -1.0L && first.real < 1.0L
        && first.imag >= 2.0L && first.imag < 4.0L;
    for (int i = 0; i < 100; i++) {
        ComplexNumber z = randomComplex(-1.0L, 1.0L, 2.0L, 4.0L);
        inRange = inRange
            && z.real >= -1.0L && z.real < 1.0L
            && z.imag >= 2.0L && z.imag < 4.0L;
    }
    CHECK(inRange, "complex values stay in intervals");

    seedPRG(4444ULL);
    ComplexNumber repeat = randomComplex(-1.0L, 1.0L, 2.0L, 4.0L);
    CHECK(closeReal(first.real, repeat.real) && closeReal(first.imag, repeat.imag), "complex generation is deterministic");
    CHECK(isnan(randomComplex(2.0L, 1.0L, 0.0L, 1.0L).real), "invalid real interval gives NAN real part");
    CHECK(isnan(randomComplex(0.0L, 1.0L, 2.0L, 1.0L).imag), "invalid imaginary interval gives NAN imaginary part");
}

int main(void) {
    testConstructNumberFromInt();
    testConstructNumberFromDouble();
    testConstructNumberFromFraction();
    testConstructNumberFromComplex();
    testAddNumbers();
    testSubNumbers();
    testMultNumbers();
    testDivNumbers();
    testCompNumbers();
    testEqNumbers();
    testFreeNumber();
    testPrintNumber();
    testSpecialFunctions();
    testPRG();
    testRandomInt();
    testRandomReal();
    testRandomFraction();
    testRandomComplex();

    printf("\n========================================\n");
    printf("Results: %d / %d tests passed\n", testsPassed, testsRun);
    printf("========================================\n");
    return testsPassed == testsRun ? 0 : 1;
}
