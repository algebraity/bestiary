#include <math.h>
#include <stdbool.h>
#include <limits.h>
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

static bool closeComplex(ComplexNumber a, long double real, long double imag) {
    return fabsl(a.real - real) <= 1e-8L && fabsl(a.imag - imag) <= 1e-8L;
}

static bool hasComplexRoot(ComplexNumber* roots, size_t count, long double real, long double imag) {
    for (size_t i = 0; i < count; i++) {
        if (closeComplex(roots[i], real, imag)) return true;
    }
    return false;
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

static bool stringEquals(const char* a, const char* b) {
    return a != NULL && b != NULL && strcmp(a, b) == 0;
}

static void checkFractionString(Fraction f, const char* expected, const char* msg) {
    char* string = fractionToString(f);
    CHECK(stringEquals(string, expected), msg);
    free(string);
}

static void checkFieldElementString(FieldElement x, const char* expected, const char* msg) {
    char* string = fieldElementToString(x);
    CHECK(stringEquals(string, expected), msg);
    free(string);
}

static Field* testQQField(void) {
    static Field Q;
    static bool initialized = false;
    if (!initialized) {
        Q = constructQQField();
        initialized = true;
    }
    return &Q;
}

static Field constructSqrt2Field(void) {
    Field* Q = testQQField();
    FieldElement two = fieldElementFromInt(Q, 2);
    Field K = constructQuadraticExtensionField(Q, two, "a");
    freeFieldElement(&two);
    K.repr = "Q(a)";
    return K;
}

static Field constructSqrt3Field(void) {
    Field* Q = testQQField();
    FieldElement three = fieldElementFromInt(Q, 3);
    Field K = constructQuadraticExtensionField(Q, three, "b");
    freeFieldElement(&three);
    K.repr = "Q(b)";
    return K;
}

static FieldElement constructNFElement2(Field* field, Fraction c0, Fraction c1) {
    FieldElement coeffs[2];
    FieldElement x;
    coeffs[0] = fieldElementFromFraction(field->data.nf.baseField, c0);
    coeffs[1] = fieldElementFromFraction(field->data.nf.baseField, c1);
    if (!fieldElementIsValid(&coeffs[0]) || !fieldElementIsValid(&coeffs[1])) {
        freeFieldElement(&coeffs[0]);
        freeFieldElement(&coeffs[1]);
        return (FieldElement){0};
    }
    x = constructNFElement(field, coeffs, 2);
    freeFieldElement(&coeffs[0]);
    freeFieldElement(&coeffs[1]);
    return x;
}

static void freeMany(FieldElement* xs, size_t n) {
    for (size_t i = 0; i < n; i++) {
        freeFieldElement(&xs[i]);
    }
}

static void testFractionHelpers(void) {
    SECTION("fraction helpers");

    Fraction zero = zeroFraction();
    Fraction one = oneFraction();
    Fraction half = constructFraction(1, 2);
    Fraction negHalf = negateFraction(half);
    Fraction invalid = constructFraction(1, 0);

    CHECK(zero.num == 0 && zero.denom == 1, "zeroFraction constructs 0");
    CHECK(one.num == 1 && one.denom == 1, "oneFraction constructs 1");
    CHECK(negHalf.num == -1 && negHalf.denom == 2, "negateFraction negates a valid fraction");
    CHECK(negateFraction(invalid).denom == 0, "negateFraction rejects invalid fraction");

    CHECK(isZeroFraction(zero), "isZeroFraction accepts zero");
    CHECK(!isZeroFraction(one), "isZeroFraction rejects nonzero");
    CHECK(!isZeroFraction(invalid), "isZeroFraction rejects invalid fraction");

    CHECK(eqFraction(constructFraction(2, 4), half), "eqFraction compares normalized values");
    CHECK(!eqFraction(half, one), "eqFraction rejects unequal values");
    CHECK(!eqFraction(invalid, invalid), "eqFraction rejects invalid values");

    Fraction largeA = constructFraction(LLONG_MAX, LLONG_MAX - 2);
    Fraction largeB = constructFraction(LLONG_MAX - 1, LLONG_MAX - 3);
    CHECK(compFractions(largeA, largeB) < 0, "compFractions orders large fractions exactly");

    char* halfString = fractionToString(half);
    char* integerString = fractionToString(one);
    char* invalidString = fractionToString(invalid);
    CHECK(halfString && strcmp(halfString, "1/2") == 0, "fractionToString formats noninteger fraction");
    CHECK(integerString && strcmp(integerString, "1") == 0, "fractionToString formats integer fraction");
    CHECK(invalidString && strcmp(invalidString, "<invalid>") == 0, "fractionToString formats invalid fraction");
    free(halfString);
    free(integerString);
    free(invalidString);
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

static void testComplexHelpers(void) {
    SECTION("complex helpers");

    ComplexNumber quotient = complexDiv((ComplexNumber){1.0L, 2.0L}, (ComplexNumber){0.0L, 0.0L});
    CHECK(isnan(quotient.real) && isnan(quotient.imag), "complexDiv returns NAN on division by zero");

    quotient = complexDiv((ComplexNumber){1.0L, 2.0L}, (ComplexNumber){1.0L, -1.0L});
    CHECK(closeReal(quotient.real, -0.5L) && closeReal(quotient.imag, 1.5L), "complexDiv still divides nonzero values");
}

static void testFieldFractionApi(void) {
    SECTION("field fraction API");

    Fraction z = zeroFraction();
    Fraction o = oneFraction();
    Fraction half = constructFraction(2, 4);
    Fraction neg = constructFraction(-6, 8);
    Fraction third = constructFraction(1, 3);
    Fraction sixth = constructFraction(1, 6);

    CHECK(z.num == 0 && z.denom == 1, "zeroFraction returns 0");
    CHECK(o.num == 1 && o.denom == 1, "oneFraction returns 1");
    CHECK(half.num == 1 && half.denom == 2, "constructFraction reduces 2/4");
    CHECK(neg.num == -3 && neg.denom == 4, "constructFraction normalizes -6/8");
    CHECK(constructFraction(1, 0).denom == 0, "constructFraction rejects zero denominator");
    CHECK(constructFraction(4, -6).num == -2 && constructFraction(4, -6).denom == 3,
          "constructFraction moves denominator sign");

    CHECK(eqFraction(half, constructFraction(1, 2)), "eqFraction sees 1/2 equals 2/4");
    CHECK(!eqFraction(half, third), "eqFraction rejects distinct fractions");
    CHECK(isZeroFraction(z), "isZeroFraction accepts zero");
    CHECK(!isZeroFraction(o), "isZeroFraction rejects one");
    CHECK(negateFraction(third).num == -1 && negateFraction(third).denom == 3,
          "negateFraction negates positive fraction");
    CHECK(negateFraction(neg).num == 3 && negateFraction(neg).denom == 4,
          "negateFraction negates negative fraction");

    CHECK(eqFraction(addFractions(third, sixth), half), "addFractions computes 1/3 + 1/6");
    CHECK(eqFraction(addFractions(half, half), o), "addFractions computes 1/2 + 1/2");
    CHECK(eqFraction(subtractFractions(half, sixth), third), "subtractFractions computes 1/2 - 1/6");
    CHECK(eqFraction(subtractFractions(third, half), negateFraction(sixth)),
          "subtractFractions computes 1/3 - 1/2");
    CHECK(eqFraction(multiplyFractions(half, third), sixth), "multiplyFractions computes 1/2 * 1/3");
    CHECK(eqFraction(multiplyFractions(neg, third), constructFraction(-1, 4)),
          "multiplyFractions computes -3/4 * 1/3");
    CHECK(eqFraction(divideFractions(half, third), constructFraction(3, 2)),
          "divideFractions computes 1/2 / 1/3");
    CHECK(divideFractions(half, z).denom == 0, "divideFractions rejects division by zero");
    CHECK(compFractions(third, half) < 0, "compFractions orders 1/3 < 1/2");
    CHECK(compFractions(half, constructFraction(1, 2)) == 0, "compFractions sees equal fractions");

    checkFractionString(half, "1/2", "fractionToString formats 1/2");
    checkFractionString(o, "1", "fractionToString formats integer fraction");

    printf("  printFraction smoke: ");
    printFraction(half);
    printf("\n");
    CHECK(true, "printFraction prints 1/2 without crashing");
    printf("  printFraction smoke: ");
    printFraction(o);
    printf("\n");
    CHECK(true, "printFraction prints 1 without crashing");
}

static void testFieldConstructors(void) {
    SECTION("field constructors and conversions");

    Field Q = constructQQField();
    Field R = constructRRField();
    Field C = constructCCField();
    long long gf5Mod[] = {0, 1};
    long long gf4Mod[] = {1, 1, 1};
    Field F5 = constructFFField(5, 1, gf5Mod);
    Field F4 = constructFFField(2, 2, gf4Mod);

    CHECK(Q.type == QQ && Q.chr == 0 && stringEquals(Q.repr, "Q"), "constructQQField builds Q");
    CHECK(constructQQField().type == QQ, "constructQQField is repeatable");
    CHECK(R.type == RR && R.chr == 0 && stringEquals(R.repr, "R"), "constructRRField builds R");
    CHECK(constructRRField().type == RR, "constructRRField is repeatable");
    CHECK(C.type == CC && C.chr == 0 && stringEquals(C.repr, "C"), "constructCCField builds C");
    CHECK(constructCCField().type == CC, "constructCCField is repeatable");
    CHECK(F5.type == FF && F5.chr == 5 && F5.data.ff.degree == 1, "constructFFField builds GF(5)");
    CHECK(F4.type == FF && F4.chr == 2 && F4.data.ff.modulus[2] == 1, "constructFFField copies GF(4) modulus");

    FieldElement q1 = constructQQElement(&Q, constructFraction(2, 4));
    FieldElement q2 = constructQQElement(&Q, constructFraction(-6, 9));
    FieldElement r1 = constructRRElement(&R, 2.5L);
    FieldElement r2 = constructRRElement(&R, -1.25L);
    ComplexNumber z1 = {3.0L, -4.0L};
    ComplexNumber z2 = {0.0L, 2.0L};
    FieldElement c1 = constructCCElement(&C, z1);
    FieldElement c2 = constructCCElement(&C, z2);
    long long coeff5a[] = {7};
    long long coeff5b[] = {-1};
    FieldElement f1 = constructFFElement(&F5, coeff5a);
    FieldElement f2 = constructFFElement(&F5, coeff5b);

    checkFieldElementString(q1, "1/2", "constructQQElement normalizes fraction");
    checkFieldElementString(q2, "-2/3", "constructQQElement handles negative rational");
    CHECK(r1.value.real == 2.5L, "constructRRElement stores positive real");
    CHECK(r2.value.real == -1.25L, "constructRRElement stores negative real");
    CHECK(c1.value.z.real == 3.0L && c1.value.z.imag == -4.0L,
          "constructCCElement stores complex value");
    CHECK(c2.value.z.real == 0.0L && c2.value.z.imag == 2.0L,
          "constructCCElement stores pure imaginary value");
    checkFieldElementString(f1, "2", "constructFFElement reduces positive coefficient");
    checkFieldElementString(f2, "4", "constructFFElement reduces negative coefficient");

    FieldElement iQ = fieldElementFromInt(&Q, 8);
    FieldElement iR = fieldElementFromInt(&R, -3);
    FieldElement iC = fieldElementFromInt(&C, 4);
    FieldElement iF = fieldElementFromInt(&F5, 12);
    checkFieldElementString(iQ, "8", "fieldElementFromInt builds Q integer");
    CHECK(iR.value.real == -3.0L, "fieldElementFromInt builds R integer");
    checkFieldElementString(iC, "4", "fieldElementFromInt builds C integer");
    checkFieldElementString(iF, "2", "fieldElementFromInt builds FF integer");

    FieldElement fracQ = fieldElementFromFraction(&Q, constructFraction(3, 4));
    FieldElement fracR = fieldElementFromFraction(&R, constructFraction(3, 2));
    FieldElement fracC = fieldElementFromFraction(&C, constructFraction(-5, 2));
    FieldElement fracF = fieldElementFromFraction(&F5, constructFraction(1, 2));
    checkFieldElementString(fracQ, "3/4", "fieldElementFromFraction builds Q fraction");
    CHECK(fracR.value.real == 1.5L, "fieldElementFromFraction builds R fraction");
    checkFieldElementString(fracC, "-2.5", "fieldElementFromFraction builds C real fraction");
    checkFieldElementString(fracF, "3", "fieldElementFromFraction maps 1/2 to 3 in GF(5)");

    FieldElement dR = fieldElementFromDouble(&R, 6.25L);
    FieldElement dC = fieldElementFromDouble(&C, -2.0L);
    FieldElement dQ = fieldElementFromDouble(&Q, 1.0L);
    CHECK(dR.value.real == 6.25L, "fieldElementFromDouble builds R element");
    checkFieldElementString(dC, "-2", "fieldElementFromDouble builds C real element");
    CHECK(!fieldElementIsValid(&dQ), "fieldElementFromDouble rejects Q");

    ComplexNumber zr = {9.0L, 0.0L};
    ComplexNumber zi = {0.0L, 3.0L};
    FieldElement fromComplexR = fieldElementFromComplex(&R, zr);
    FieldElement fromComplexC = fieldElementFromComplex(&C, zi);
    FieldElement badComplexR = fieldElementFromComplex(&R, zi);
    CHECK(fromComplexR.value.real == 9.0L, "fieldElementFromComplex maps real complex to R");
    checkFieldElementString(fromComplexC, "3i", "fieldElementFromComplex maps complex to C");
    CHECK(!fieldElementIsValid(&badComplexR), "fieldElementFromComplex rejects non-real into R");

    FieldElement twoQ = fieldElementFromInt(&Q, 2);
    FieldElement fourQ = fieldElementFromInt(&Q, 4);
    FieldElement twoR = fieldElementFromInt(&R, 2);
    FieldElement minusOneR = fieldElementFromInt(&R, -1);
    Field sqrt2 = constructQuadraticExtensionField(&Q, twoQ, "a");
    Field sqrt4 = constructQuadraticExtensionField(&Q, fourQ, "b");
    Field rSqrt2 = constructQuadraticExtensionField(&R, twoR, "c");
    Field rSqrtMinusOne = constructQuadraticExtensionField(&R, minusOneR, "d");
    FieldElement oneHalf = fieldElementFromFraction(&Q, constructFraction(1, 2));
    FieldElement threeHalves = fieldElementFromFraction(&Q, constructFraction(3, 2));
    FieldElement nfEltCoeffs[] = {oneHalf, threeHalves};
    FieldElement nfElt = constructNFElement(&sqrt2, nfEltCoeffs, 2);
    FieldElement embeddedQ = embedFieldElement(&sqrt2, fracQ);
    FieldElement embeddedR = embedFieldElement(&C, fracR);
    FieldElement gen = quadraticExtensionGenerator(&sqrt2);
    FieldElement genSq = multiplyFieldElements(gen, gen);
    FieldElement eightInSqrt2 = constructNFElement2(&sqrt2, constructFraction(8, 1),
            constructFraction(0, 1));
    FieldElement threeInSqrt2 = constructNFElement2(&sqrt2, constructFraction(3, 1),
            constructFraction(0, 1));
    Field sqrt8InSqrt2 = constructQuadraticExtensionField(&sqrt2, eightInSqrt2, "e");
    Field sqrt3OverSqrt2 = constructQuadraticExtensionField(&sqrt2, threeInSqrt2, "f");
    FieldElement gen2 = quadraticExtensionGenerator(&sqrt3OverSqrt2);
    FieldElement gen2Sq = multiplyFieldElements(gen2, gen2);
    FieldElement twoF5 = fieldElementFromInt(&F5, 2);
    FieldElement fourF5 = fieldElementFromInt(&F5, 4);
    FieldElement threeF5 = fieldElementFromInt(&F5, 3);
    Field sqrtTwoF5 = constructQuadraticExtensionField(&F5, twoF5, "s");
    Field sqrtFourF5 = constructQuadraticExtensionField(&F5, fourF5, "t");
    FieldElement sqrtFourElt = fieldElementSquareRoot(fourF5);
    FieldElement ffGen = quadraticExtensionGenerator(&sqrtTwoF5);
    FieldElement ffGenSq = multiplyFieldElements(ffGen, ffGen);
    FieldElement embeddedF5 = embedFieldElement(&sqrtTwoF5, threeF5);

    CHECK(sqrt2.type == NF && sqrt2.data.nf.baseField == &Q
            && sqrt2.data.nf.gen.degree == 2, "constructQuadraticExtensionField builds degree-2 NF");
    checkFieldElementString(sqrt2.data.nf.gen.minPolyCoeffs[0], "-2",
            "constructQuadraticExtensionField stores x^2-a constant term");
    CHECK(sqrt4.type == QQ, "constructQuadraticExtensionField keeps Q when sqrt(a) is rational");
    CHECK(rSqrt2.type == RR, "constructQuadraticExtensionField keeps R when a is nonnegative");
    CHECK(rSqrtMinusOne.type == CC, "constructQuadraticExtensionField returns C when a is negative in R");
    checkFieldElementString(nfElt, "1/2 + 3/2*a", "constructNFElement builds degree-2 NF element");
    checkFieldElementString(embeddedQ, "3/4", "embedFieldElement embeds QQ into quadratic extension");
    checkFieldElementString(embeddedR, "1.5", "embedFieldElement embeds RR into C");
    checkFieldElementString(gen, "a", "quadraticExtensionGenerator returns the extension generator");
    checkFieldElementString(genSq, "2", "quadraticExtensionGenerator squares to the radicand");
    CHECK(fieldEq(&sqrt8InSqrt2, &sqrt2),
            "constructQuadraticExtensionField keeps NF when the square root is already present");
    CHECK(sqrt3OverSqrt2.type == NF && sqrt3OverSqrt2.data.nf.baseField == &sqrt2
            && sqrt3OverSqrt2.data.nf.gen.degree == 2,
            "constructQuadraticExtensionField adjoins a generator when NF lacks the square root");
    checkFieldElementString(gen2Sq, "3",
            "quadraticExtensionGenerator squares to the radicand over an NF base");
    CHECK(sqrtTwoF5.type == FF_QEXT && sqrtTwoF5.data.ffqext.baseField == &F5,
            "constructQuadraticExtensionField uses FF_QEXT for a nonsquare in a finite field");
    CHECK(sqrtFourF5.type == FF,
            "constructQuadraticExtensionField keeps the finite field when the radicand is a square");
    checkFieldElementString(sqrtFourElt, "2",
            "fieldElementSquareRoot finds a square root already in a finite field");
    checkFieldElementString(ffGen, "s",
            "quadraticExtensionGenerator returns the finite quadratic generator");
    checkFieldElementString(ffGenSq, "2",
            "quadraticExtensionGenerator squares to the finite-field radicand");
    checkFieldElementString(embeddedF5, "3",
            "embedFieldElement embeds finite-field constants into FF_QEXT");

    FieldElement all[] = {q1, q2, r1, r2, c1, c2, f1, f2, iQ, iR, iC, iF,
        fracQ, fracR, fracC, fracF, dR, dC, dQ, fromComplexR, fromComplexC,
        badComplexR, twoQ, fourQ, twoR, minusOneR, oneHalf, threeHalves, nfElt,
        embeddedQ, embeddedR, gen, genSq, eightInSqrt2, threeInSqrt2, gen2, gen2Sq,
        twoF5, fourF5, threeF5, sqrtFourElt, ffGen, ffGenSq, embeddedF5};
    freeMany(all, sizeof(all) / sizeof(all[0]));
    freeField(&sqrtFourF5);
    freeField(&sqrtTwoF5);
    freeField(&sqrt3OverSqrt2);
    freeField(&sqrt8InSqrt2);
    freeField(&rSqrtMinusOne);
    freeField(&rSqrt2);
    freeField(&sqrt4);
    freeField(&sqrt2);
    freeField(&F5);
    freeField(&F4);
}

static void testFieldPrintAndBoolApi(void) {
    SECTION("printing, validity, and field equality");

    Field Q = constructQQField();
    Field Q2 = constructQQField();
    Field R = constructRRField();
    long long gf5Mod[] = {0, 1};
    long long gf7Mod[] = {0, 1};
    long long gf4Mod[] = {1, 1, 1};
    Field F5 = constructFFField(5, 1, gf5Mod);
    Field F7 = constructFFField(7, 1, gf7Mod);
    Field F4 = constructFFField(2, 2, gf4Mod);
    Field K2 = constructSqrt2Field();
    Field K2b = constructSqrt2Field();
    Field K3 = constructSqrt3Field();

    CHECK(fieldEq(&Q, &Q2), "fieldEq accepts two Q fields");
    CHECK(!fieldEq(&Q, &R), "fieldEq rejects Q and R");
    CHECK(fieldEq(&F5, &F5), "fieldEq accepts same finite field");
    CHECK(!fieldEq(&F5, &F7), "fieldEq rejects different prime finite fields");
    CHECK(fieldEq(&K2, &K2b), "fieldEq accepts matching number fields");
    CHECK(!fieldEq(&K2, &K3), "fieldEq rejects different number fields");

    FieldElement zeroQ = zeroFieldElement(&Q);
    FieldElement oneQ = oneFieldElement(&Q);
    FieldElement twoQ = fieldElementFromInt(&Q, 2);
    FieldElement zeroF = zeroFieldElement(&F5);
    FieldElement oneF = oneFieldElement(&F5);
    FieldElement a = constructNFElement2(&K2, constructFraction(0, 1), constructFraction(1, 1));
    FieldElement zeroK = zeroFieldElement(&K2);
    FieldElement oneK = oneFieldElement(&K2);

    CHECK(fieldElementIsValid(&zeroQ), "fieldElementIsValid accepts Q zero");
    CHECK(fieldElementIsValid(&oneF), "fieldElementIsValid accepts FF one");
    CHECK(!fieldElementIsValid(NULL), "fieldElementIsValid rejects NULL");
    CHECK(fieldElementIsZero(zeroQ), "fieldElementIsZero accepts Q zero");
    CHECK(!fieldElementIsZero(oneQ), "fieldElementIsZero rejects Q one");
    CHECK(fieldElementIsZero(zeroF), "fieldElementIsZero accepts FF zero");
    CHECK(!fieldElementIsZero(oneF), "fieldElementIsZero rejects FF one");
    CHECK(fieldElementIsZero(zeroK), "fieldElementIsZero accepts NF zero");
    CHECK(!fieldElementIsZero(a), "fieldElementIsZero rejects NF generator");
    CHECK(fieldElementIsOne(oneQ), "fieldElementIsOne accepts Q one");
    CHECK(!fieldElementIsOne(twoQ), "fieldElementIsOne rejects Q two");
    CHECK(fieldElementIsOne(oneF), "fieldElementIsOne accepts FF one");
    CHECK(!fieldElementIsOne(zeroF), "fieldElementIsOne rejects FF zero");
    CHECK(fieldElementIsOne(oneK), "fieldElementIsOne accepts NF one");
    CHECK(!fieldElementIsOne(a), "fieldElementIsOne rejects NF generator");
    CHECK(fieldElementIsUnit(oneQ), "fieldElementIsUnit accepts Q one");
    CHECK(!fieldElementIsUnit(zeroQ), "fieldElementIsUnit rejects Q zero");
    CHECK(fieldElementIsUnit(oneF), "fieldElementIsUnit accepts FF one");
    CHECK(!fieldElementIsUnit(zeroF), "fieldElementIsUnit rejects FF zero");
    CHECK(fieldElementIsUnit(a), "fieldElementIsUnit accepts nonzero NF element");
    CHECK(sameField(&oneQ, &twoQ), "sameField accepts two Q elements");
    CHECK(!sameField(&oneQ, &oneF), "sameField rejects Q and FF elements");

    checkFieldElementString(zeroQ, "0", "fieldElementToString formats Q zero");
    checkFieldElementString(twoQ, "2", "fieldElementToString formats Q two");
    checkFieldElementString(oneF, "1", "fieldElementToString formats FF one");
    checkFieldElementString(a, "a", "fieldElementToString formats NF generator");
    checkFieldElementString(oneK, "1", "fieldElementToString formats NF one");
    checkFieldElementString((FieldElement){0}, "<invalid>", "fieldElementToString formats invalid element");

    printf("  printFieldElement smoke: ");
    printFieldElement(twoQ);
    printf("\n");
    CHECK(true, "printFieldElement prints Q element without crashing");
    printf("  printFieldElement smoke: ");
    printFieldElement(a);
    printf("\n");
    CHECK(true, "printFieldElement prints NF element without crashing");

    freeFieldElement(&oneF);
    CHECK(oneF.field == NULL, "freeFieldElement clears FF element field");
    freeFieldElement(&a);
    CHECK(a.field == NULL, "freeFieldElement clears NF element field");
    freeField(&F4);
    CHECK(F4.data.ff.modulus == NULL, "freeField clears FF modulus");
    freeField(&K3);
    CHECK(K3.data.nf.gen.minPolyCoeffs == NULL, "freeField clears NF generator polynomial");

    FieldElement remaining[] = {zeroQ, oneQ, twoQ, zeroF, zeroK, oneK};
    freeMany(remaining, sizeof(remaining) / sizeof(remaining[0]));
    freeField(&F5);
    freeField(&F7);
    freeField(&K2);
    freeField(&K2b);
}

static void testClassicalFieldArithmetic(void) {
    SECTION("QQ, RR, and CC arithmetic");

    Field Q = constructQQField();
    Field R = constructRRField();
    Field C = constructCCField();
    FieldElement qHalf = constructQQElement(&Q, constructFraction(1, 2));
    FieldElement qThird = constructQQElement(&Q, constructFraction(1, 3));
    FieldElement qSum = addFieldElements(qHalf, qThird);
    FieldElement qDiff = subtractFieldElements(qHalf, qThird);
    FieldElement qNeg = negateFieldElement(qThird);
    FieldElement qProd = multiplyFieldElements(qHalf, qThird);
    FieldElement qInv = invertFieldElement(qHalf);
    FieldElement qQuot = divideFieldElements(qHalf, qThird);
    FieldElement qCopy = copyFieldElement(qHalf);
    FieldElement qNorm = normalizeFieldElement(qThird);
    FieldElement qZero = zeroFieldElement(&Q);
    FieldElement qOne = oneFieldElement(&Q);

    checkFieldElementString(qSum, "5/6", "addFieldElements computes Q sum");
    checkFieldElementString(qDiff, "1/6", "subtractFieldElements computes Q difference");
    checkFieldElementString(qNeg, "-1/3", "negateFieldElement computes Q negation");
    checkFieldElementString(qProd, "1/6", "multiplyFieldElements computes Q product");
    checkFieldElementString(qInv, "2", "invertFieldElement computes Q inverse");
    checkFieldElementString(qQuot, "3/2", "divideFieldElements computes Q quotient");
    checkFieldElementString(qCopy, "1/2", "copyFieldElement copies Q element");
    checkFieldElementString(qNorm, "1/3", "normalizeFieldElement normalizes Q element");
    CHECK(fieldElementIsZero(qZero), "zeroFieldElement builds Q zero");
    CHECK(fieldElementIsOne(qOne), "oneFieldElement builds Q one");

    FieldElement r2 = constructRRElement(&R, 2.0L);
    FieldElement r3 = constructRRElement(&R, 3.0L);
    FieldElement rAdd = addFieldElements(r2, r3);
    FieldElement rSub = subtractFieldElements(r3, r2);
    FieldElement rNeg = negateFieldElement(r3);
    FieldElement rMul = multiplyFieldElements(r2, r3);
    FieldElement rInv = invertFieldElement(r2);
    FieldElement rDiv = divideFieldElements(r3, r2);
    FieldElement rCopy = copyFieldElement(r3);
    FieldElement rNorm = normalizeFieldElement(r2);
    FieldElement rZero = zeroFieldElement(&R);
    FieldElement rOne = oneFieldElement(&R);

    CHECK(rAdd.value.real == 5.0L, "addFieldElements computes R sum");
    CHECK(rSub.value.real == 1.0L, "subtractFieldElements computes R difference");
    CHECK(rNeg.value.real == -3.0L, "negateFieldElement computes R negation");
    CHECK(rMul.value.real == 6.0L, "multiplyFieldElements computes R product");
    CHECK(rInv.value.real == 0.5L, "invertFieldElement computes R inverse");
    CHECK(rDiv.value.real == 1.5L, "divideFieldElements computes R quotient");
    CHECK(rCopy.value.real == 3.0L, "copyFieldElement copies R element");
    CHECK(rNorm.value.real == 2.0L, "normalizeFieldElement copies R element");
    CHECK(fieldElementIsZero(rZero), "zeroFieldElement builds R zero");
    CHECK(fieldElementIsOne(rOne), "oneFieldElement builds R one");

    ComplexNumber ca = {1.0L, 2.0L};
    ComplexNumber cb = {3.0L, -1.0L};
    FieldElement cA = constructCCElement(&C, ca);
    FieldElement cB = constructCCElement(&C, cb);
    FieldElement cAdd = addFieldElements(cA, cB);
    FieldElement cSub = subtractFieldElements(cA, cB);
    FieldElement cNeg = negateFieldElement(cA);
    FieldElement cMul = multiplyFieldElements(cA, cB);
    FieldElement cInv = invertFieldElement(cA);
    FieldElement cDiv = divideFieldElements(cA, cB);
    FieldElement cCopy = copyFieldElement(cB);
    FieldElement cNorm = normalizeFieldElement(cA);
    FieldElement cZero = zeroFieldElement(&C);
    FieldElement cOne = oneFieldElement(&C);

    checkFieldElementString(cAdd, "4 + 1i", "addFieldElements computes C sum");
    checkFieldElementString(cSub, "-2 + 3i", "subtractFieldElements computes C difference");
    checkFieldElementString(cNeg, "-1 - 2i", "negateFieldElement computes C negation");
    checkFieldElementString(cMul, "5 + 5i", "multiplyFieldElements computes C product");
    CHECK(fabsl(cInv.value.z.real - 0.2L) < 1e-18L, "invertFieldElement computes C inverse real part");
    CHECK(fabsl(cInv.value.z.imag + 0.4L) < 1e-18L, "invertFieldElement computes C inverse imag part");
    CHECK(fabsl(cDiv.value.z.real - 0.1L) < 1e-18L, "divideFieldElements computes C quotient real part");
    CHECK(fabsl(cDiv.value.z.imag - 0.7L) < 1e-18L, "divideFieldElements computes C quotient imag part");
    checkFieldElementString(cCopy, "3 - 1i", "copyFieldElement copies C element");
    checkFieldElementString(cNorm, "1 + 2i", "normalizeFieldElement copies C element");
    CHECK(fieldElementIsZero(cZero), "zeroFieldElement builds C zero");
    CHECK(fieldElementIsOne(cOne), "oneFieldElement builds C one");

    FieldElement all[] = {qHalf, qThird, qSum, qDiff, qNeg, qProd, qInv, qQuot, qCopy,
        qNorm, qZero, qOne, r2, r3, rAdd, rSub, rNeg, rMul, rInv, rDiv, rCopy,
        rNorm, rZero, rOne, cA, cB, cAdd, cSub, cNeg, cMul, cInv, cDiv, cCopy,
        cNorm, cZero, cOne};
    freeMany(all, sizeof(all) / sizeof(all[0]));
}

static void testFiniteFieldArithmetic(void) {
    SECTION("finite field arithmetic");

    long long gf5Mod[] = {0, 1};
    long long gf4Mod[] = {1, 1, 1};
    Field F5 = constructFFField(5, 1, gf5Mod);
    Field F4 = constructFFField(2, 2, gf4Mod);
    FieldElement f2 = fieldElementFromInt(&F5, 2);
    FieldElement f3 = fieldElementFromInt(&F5, 3);
    FieldElement f4 = fieldElementFromInt(&F5, 4);
    FieldElement fAdd = addFieldElements(f2, f3);
    FieldElement fSub = subtractFieldElements(f2, f3);
    FieldElement fNeg = negateFieldElement(f2);
    FieldElement fMul = multiplyFieldElements(f2, f3);
    FieldElement fInv = invertFieldElement(f2);
    FieldElement fDiv = divideFieldElements(f3, f2);
    FieldElement fCopy = copyFieldElement(f4);
    FieldElement fNorm = normalizeFieldElement(f3);
    FieldElement fZero = zeroFieldElement(&F5);
    FieldElement fOne = oneFieldElement(&F5);

    checkFieldElementString(fAdd, "0", "addFieldElements computes GF(5) sum");
    checkFieldElementString(fSub, "4", "subtractFieldElements computes GF(5) difference");
    checkFieldElementString(fNeg, "3", "negateFieldElement computes GF(5) negation");
    checkFieldElementString(fMul, "1", "multiplyFieldElements computes GF(5) product");
    checkFieldElementString(fInv, "3", "invertFieldElement computes GF(5) inverse");
    checkFieldElementString(fDiv, "4", "divideFieldElements computes GF(5) quotient");
    checkFieldElementString(fCopy, "4", "copyFieldElement copies GF(5) element");
    checkFieldElementString(fNorm, "3", "normalizeFieldElement copies GF(5) element");
    CHECK(fieldElementIsZero(fZero), "zeroFieldElement builds GF(5) zero");
    CHECK(fieldElementIsOne(fOne), "oneFieldElement builds GF(5) one");

    long long xCoeffs[] = {0, 1};
    long long onePlusXCoeffs[] = {1, 1};
    FieldElement x = constructFFElement(&F4, xCoeffs);
    FieldElement onePlusX = constructFFElement(&F4, onePlusXCoeffs);
    FieldElement xPlusX = addFieldElements(x, x);
    FieldElement xSq = multiplyFieldElements(x, x);
    FieldElement xInv = invertFieldElement(x);
    FieldElement xQuot = divideFieldElements(onePlusX, x);
    FieldElement xSub = subtractFieldElements(x, onePlusX);
    FieldElement xNeg = negateFieldElement(x);
    FieldElement two = fieldElementFromInt(&F5, 2);
    Field qext = constructQuadraticExtensionField(&F5, two, "s");
    FieldElement qextOne = oneFieldElement(&qext);
    FieldElement s = quadraticExtensionGenerator(&qext);
    FieldElement onePlusS = addFieldElements(qextOne, s);
    FieldElement onePlusSSq = multiplyFieldElements(onePlusS, onePlusS);
    FieldElement sInv = invertFieldElement(s);
    FieldElement sTimesSInv = multiplyFieldElements(s, sInv);

    checkFieldElementString(x, "x", "constructFFElement builds GF(4) generator");
    checkFieldElementString(onePlusX, "1 + x", "constructFFElement builds GF(4) element");
    checkFieldElementString(xPlusX, "0", "addFieldElements computes x+x=0 in GF(4)");
    checkFieldElementString(xSq, "1 + x", "multiplyFieldElements reduces x^2 in GF(4)");
    checkFieldElementString(xInv, "1 + x", "invertFieldElement computes x inverse in GF(4)");
    checkFieldElementString(xQuot, "x", "divideFieldElements computes (1+x)/x in GF(4)");
    checkFieldElementString(xSub, "1", "subtractFieldElements computes x-(1+x) in GF(4)");
    checkFieldElementString(xNeg, "x", "negateFieldElement is identity in characteristic 2");
    CHECK(qext.type == FF_QEXT,
            "constructQuadraticExtensionField produces FF_QEXT in finite-field arithmetic tests");
    checkFieldElementString(onePlusS, "1 + s", "addFieldElements builds a mixed FF_QEXT element");
    checkFieldElementString(onePlusSSq, "3 + 2*s", "multiplyFieldElements computes (1+s)^2 in FF_QEXT");
    checkFieldElementString(sInv, "3*s", "invertFieldElement computes the inverse of the FF_QEXT generator");
    CHECK(fieldElementIsOne(sTimesSInv), "invertFieldElement gives a multiplicative inverse in FF_QEXT");

    FieldElement all[] = {f2, f3, f4, fAdd, fSub, fNeg, fMul, fInv, fDiv, fCopy,
        fNorm, fZero, fOne, x, onePlusX, xPlusX, xSq, xInv, xQuot, xSub, xNeg,
        two, qextOne, s, onePlusS, onePlusSSq, sInv, sTimesSInv};
    freeMany(all, sizeof(all) / sizeof(all[0]));
    freeField(&qext);
    freeField(&F5);
    freeField(&F4);
}

static void testNumberFieldArithmetic(void) {
    SECTION("number field arithmetic");

    Field K = constructSqrt2Field();
    FieldElement a = constructNFElement2(&K, constructFraction(0, 1), constructFraction(1, 1));
    FieldElement one = oneFieldElement(&K);
    FieldElement two = constructNFElement2(&K, constructFraction(2, 1), constructFraction(0, 1));
    FieldElement onePlusA = constructNFElement2(&K, constructFraction(1, 1), constructFraction(1, 1));
    FieldElement aPlusOne = addFieldElements(a, one);
    FieldElement aMinusOne = subtractFieldElements(a, one);
    FieldElement negA = negateFieldElement(a);
    FieldElement aSq = multiplyFieldElements(a, a);
    FieldElement invA = invertFieldElement(a);
    FieldElement divTwoA = divideFieldElements(two, a);
    FieldElement copyA = copyFieldElement(a);
    FieldElement normOnePlusA = normalizeFieldElement(onePlusA);
    FieldElement zero = zeroFieldElement(&K);

    checkFieldElementString(a, "a", "constructNFElement2 builds generator");
    checkFieldElementString(onePlusA, "1 + a", "constructNFElement2 builds mixed NF element");
    checkFieldElementString(aPlusOne, "1 + a", "addFieldElements computes a+1");
    checkFieldElementString(aMinusOne, "-1 + a", "subtractFieldElements computes a-1");
    checkFieldElementString(negA, "-a", "negateFieldElement computes -a");
    checkFieldElementString(aSq, "2", "multiplyFieldElements reduces a^2 to 2");
    checkFieldElementString(invA, "1/2*a", "invertFieldElement computes 1/a = a/2");
    checkFieldElementString(divTwoA, "a", "divideFieldElements computes 2/a = a");
    checkFieldElementString(copyA, "a", "copyFieldElement copies NF element");
    checkFieldElementString(normOnePlusA, "1 + a", "normalizeFieldElement copies NF element");
    CHECK(fieldElementIsZero(zero), "zeroFieldElement builds NF zero");
    CHECK(fieldElementIsOne(one), "oneFieldElement builds NF one");

    FieldElement all[] = {a, one, two, onePlusA, aPlusOne, aMinusOne, negA, aSq,
        invA, divTwoA, copyA, normOnePlusA, zero};
    freeMany(all, sizeof(all) / sizeof(all[0]));
    freeField(&K);
}

static void testNumberFieldTowerArithmetic(void) {
    SECTION("number field tower arithmetic");

    Field K = constructSqrt2Field();
    FieldElement a = constructNFElement2(&K, constructFraction(0, 1), constructFraction(1, 1));
    Field L = {0};
    L.repr = "Q(a)(b)";
    L.chr = 0;
    L.type = NF;
    L.data.nf.baseField = &K;
    L.data.nf.gen.repr = "b";
    L.data.nf.gen.degree = 2;
    L.data.nf.gen.minPolyCoeffs = malloc(3 * sizeof(FieldElement));
    if (L.data.nf.gen.minPolyCoeffs == NULL) {
        CHECK(false, "allocated tower NF minimal polynomial");
        freeFieldElement(&a);
        freeField(&K);
        return;
    }
    L.data.nf.gen.minPolyCoeffs[0] = negateFieldElement(a);
    L.data.nf.gen.minPolyCoeffs[1] = zeroFieldElement(&K);
    L.data.nf.gen.minPolyCoeffs[2] = oneFieldElement(&K);

    FieldElement b = constructNFElement2(&L, constructFraction(0, 1), constructFraction(1, 1));
    FieldElement bSq = multiplyFieldElements(b, b);
    FieldElement bFourth = multiplyFieldElements(bSq, bSq);
    FieldElement oneL = oneFieldElement(&L);
    FieldElement twoL = fieldElementFromInt(&L, 2);

    CHECK(fieldElementIsValid(&b), "constructNFElement2 builds generator over NF base");
    checkFieldElementString(b, "b", "fieldElementToString formats tower generator");
    checkFieldElementString(bSq, "a", "multiplyFieldElements reduces b^2 to base generator");
    checkFieldElementString(bFourth, "2", "multiplyFieldElements reduces b^4 to rational constant");
    CHECK(fieldElementIsOne(oneL), "oneFieldElement builds tower NF one");
    checkFieldElementString(twoL, "2", "fieldElementFromInt embeds constants into tower NF");

    FieldElement all[] = {a, b, bSq, bFourth, oneL, twoL};
    freeMany(all, sizeof(all) / sizeof(all[0]));
    freeField(&L);
    freeField(&K);
}

static void testInvalidAndCrossFieldBehavior(void) {
    SECTION("invalid and cross-field behavior");

    Field Q = constructQQField();
    Field R = constructRRField();
    long long gf5Mod[] = {0, 1};
    Field F5 = constructFFField(5, 1, gf5Mod);
    FieldElement qOne = oneFieldElement(&Q);
    FieldElement rOne = oneFieldElement(&R);
    FieldElement fZero = zeroFieldElement(&F5);
    FieldElement invalid = {0};

    CHECK(!sameField(&qOne, &rOne), "sameField rejects Q and R");
    CHECK(!fieldElementIsValid(&invalid), "fieldElementIsValid rejects zeroed struct");
    CHECK(!fieldElementIsZero(invalid), "fieldElementIsZero rejects invalid element");
    CHECK(!fieldElementIsOne(invalid), "fieldElementIsOne rejects invalid element");
    CHECK(!fieldElementIsUnit(invalid), "fieldElementIsUnit rejects invalid element");
    CHECK(!fieldElementIsUnit(fZero), "fieldElementIsUnit rejects finite field zero");
    CHECK(!fieldElementIsValid(&(FieldElement){0}), "fieldElementIsValid rejects temporary invalid");
    CHECK(!fieldEq(NULL, &Q), "fieldEq rejects NULL lhs");
    CHECK(!fieldEq(&Q, NULL), "fieldEq rejects NULL rhs");
    CHECK(!fieldEq(NULL, NULL), "fieldEq rejects two NULLs");

    FieldElement badAdd = addFieldElements(qOne, rOne);
    FieldElement badSub = subtractFieldElements(qOne, rOne);
    FieldElement badMul = multiplyFieldElements(qOne, rOne);
    FieldElement badDiv = divideFieldElements(qOne, rOne);
    FieldElement badInv = invertFieldElement(fZero);

    CHECK(!fieldElementIsValid(&badAdd), "addFieldElements rejects mismatched fields");
    CHECK(!fieldElementIsValid(&badSub), "subtractFieldElements rejects mismatched fields");
    CHECK(!fieldElementIsValid(&badMul), "multiplyFieldElements rejects mismatched fields");
    CHECK(!fieldElementIsValid(&badDiv), "divideFieldElements rejects mismatched fields");
    CHECK(!fieldElementIsValid(&badInv), "invertFieldElement rejects zero");

    freeFieldElement(&qOne);
    freeFieldElement(&rOne);
    freeFieldElement(&fZero);
    freeField(&F5);
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
    bool inRange = first >= -2.0L && first <= 5.0L;
    for (int i = 0; i < 100; i++) {
        long double x = randomReal(-2.0L, 5.0L);
        inRange = inRange && x >= -2.0L && x <= 5.0L;
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

static void testRandomComplexComp(void) {
    SECTION("randomComplexComp");
    seedPRG(4444ULL);
    ComplexNumber first = randomComplexComp(-1.0L, 1.0L, 2.0L, 4.0L);
    bool inRange = first.real >= -1.0L && first.real <= 1.0L
        && first.imag >= 2.0L && first.imag <= 4.0L;
    for (int i = 0; i < 100; i++) {
        ComplexNumber z = randomComplexComp(-1.0L, 1.0L, 2.0L, 4.0L);
        inRange = inRange
            && z.real >= -1.0L && z.real <= 1.0L
            && z.imag >= 2.0L && z.imag <= 4.0L;
    }
    CHECK(inRange, "complex values stay in intervals");

    seedPRG(4444ULL);
    ComplexNumber repeat = randomComplexComp(-1.0L, 1.0L, 2.0L, 4.0L);
    CHECK(closeReal(first.real, repeat.real) && closeReal(first.imag, repeat.imag), "complex generation is deterministic");
    CHECK(isnan(randomComplexComp(2.0L, 1.0L, 0.0L, 1.0L).real), "invalid real interval gives NAN real part");
    CHECK(isnan(randomComplexComp(0.0L, 1.0L, 2.0L, 1.0L).imag), "invalid imaginary interval gives NAN imaginary part");
}

static void testRandomComplexMod(void) {
    SECTION("randomComplexMod");
    seedPRG(5555ULL);
    ComplexNumber first = randomComplexMod(2.0L, 5.0L);
    long double firstMod = complexAbs(first);
    bool inRange = firstMod >= 2.0L && firstMod <= 5.0L;
    for (int i = 0; i < 100; i++) {
        ComplexNumber z = randomComplexMod(2.0L, 5.0L);
        long double mod = complexAbs(z);
        inRange = inRange && mod >= 2.0L && mod <= 5.0L;
    }
    CHECK(inRange, "complex modulus stays in interval");

    seedPRG(5555ULL);
    ComplexNumber repeat = randomComplexMod(2.0L, 5.0L);
    CHECK(closeReal(first.real, repeat.real) && closeReal(first.imag, repeat.imag), "modulus complex generation is deterministic");
    CHECK(isnan(randomComplexMod(-1.0L, 1.0L).real), "negative modulus range is invalid");
    CHECK(isnan(randomComplexMod(2.0L, 1.0L).imag), "reversed modulus range is invalid");
}

static void testSolveQuadratic(void) {
    SECTION("solveQuadratic");
    ComplexNumber roots[2] = {0};

    size_t n = solveQuadratic((ComplexNumber){1.0L, 0.0L}, (ComplexNumber){-3.0L, 0.0L}, (ComplexNumber){2.0L, 0.0L}, roots);
    CHECK(n == 2 && hasComplexRoot(roots, n, 1.0L, 0.0L) && hasComplexRoot(roots, n, 2.0L, 0.0L), "real quadratic roots");

    n = solveQuadratic((ComplexNumber){1.0L, 0.0L}, (ComplexNumber){0.0L, 0.0L}, (ComplexNumber){1.0L, 0.0L}, roots);
    CHECK(n == 2 && hasComplexRoot(roots, n, 0.0L, -1.0L) && hasComplexRoot(roots, n, 0.0L, 1.0L), "complex quadratic roots");

    n = solveQuadratic((ComplexNumber){0.0L, 0.0L}, (ComplexNumber){2.0L, 0.0L}, (ComplexNumber){-4.0L, 0.0L}, roots);
    CHECK(n == 1 && hasComplexRoot(roots, n, 2.0L, 0.0L), "degenerate quadratic becomes linear");
}

static void testSolveCubic(void) {
    SECTION("solveCubic");
    ComplexNumber roots[3] = {0};

    size_t n = solveCubic((ComplexNumber){1.0L, 0.0L}, (ComplexNumber){-6.0L, 0.0L}, (ComplexNumber){11.0L, 0.0L}, (ComplexNumber){-6.0L, 0.0L}, roots);
    CHECK(n == 3 && hasComplexRoot(roots, n, 1.0L, 0.0L) && hasComplexRoot(roots, n, 2.0L, 0.0L) && hasComplexRoot(roots, n, 3.0L, 0.0L), "real cubic roots");

    n = solveCubic((ComplexNumber){1.0L, 0.0L}, (ComplexNumber){0.0L, 0.0L}, (ComplexNumber){0.0L, 0.0L}, (ComplexNumber){1.0L, 0.0L}, roots);
    CHECK(n == 3 && hasComplexRoot(roots, n, -1.0L, 0.0L) && hasComplexRoot(roots, n, 0.5L, -0.86602540378443864676L) && hasComplexRoot(roots, n, 0.5L, 0.86602540378443864676L), "complex cubic roots");

    n = solveCubic((ComplexNumber){0.0L, 0.0L}, (ComplexNumber){1.0L, 0.0L}, (ComplexNumber){-3.0L, 0.0L}, (ComplexNumber){2.0L, 0.0L}, roots);
    CHECK(n == 2 && hasComplexRoot(roots, n, 1.0L, 0.0L) && hasComplexRoot(roots, n, 2.0L, 0.0L), "degenerate cubic becomes quadratic");
}

static void testSolveQuartic(void) {
    SECTION("solveQuartic");
    ComplexNumber roots[4] = {0};

    size_t n = solveQuartic((ComplexNumber){1.0L, 0.0L}, (ComplexNumber){0.0L, 0.0L}, (ComplexNumber){-5.0L, 0.0L}, (ComplexNumber){0.0L, 0.0L}, (ComplexNumber){4.0L, 0.0L}, roots);
    CHECK(n == 4 && hasComplexRoot(roots, n, -2.0L, 0.0L) && hasComplexRoot(roots, n, -1.0L, 0.0L) && hasComplexRoot(roots, n, 1.0L, 0.0L) && hasComplexRoot(roots, n, 2.0L, 0.0L), "real quartic roots");

    n = solveQuartic((ComplexNumber){1.0L, 0.0L}, (ComplexNumber){0.0L, 0.0L}, (ComplexNumber){0.0L, 0.0L}, (ComplexNumber){0.0L, 0.0L}, (ComplexNumber){1.0L, 0.0L}, roots);
    CHECK(n == 4
          && hasComplexRoot(roots, n, -0.70710678118654752440L, -0.70710678118654752440L)
          && hasComplexRoot(roots, n, -0.70710678118654752440L, 0.70710678118654752440L)
          && hasComplexRoot(roots, n, 0.70710678118654752440L, -0.70710678118654752440L)
          && hasComplexRoot(roots, n, 0.70710678118654752440L, 0.70710678118654752440L),
          "complex quartic roots");

    n = solveQuartic((ComplexNumber){0.0L, 0.0L}, (ComplexNumber){1.0L, 0.0L}, (ComplexNumber){-6.0L, 0.0L}, (ComplexNumber){11.0L, 0.0L}, (ComplexNumber){-6.0L, 0.0L}, roots);
    CHECK(n == 3 && hasComplexRoot(roots, n, 1.0L, 0.0L) && hasComplexRoot(roots, n, 2.0L, 0.0L) && hasComplexRoot(roots, n, 3.0L, 0.0L), "degenerate quartic becomes cubic");
}

int main(void) {
    testFractionHelpers();
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
    testComplexHelpers();
    testFieldFractionApi();
    testFieldConstructors();
    testFieldPrintAndBoolApi();
    testClassicalFieldArithmetic();
    testFiniteFieldArithmetic();
    testNumberFieldArithmetic();
    testNumberFieldTowerArithmetic();
    testInvalidAndCrossFieldBehavior();
    testPRG();
    testRandomInt();
    testRandomReal();
    testRandomFraction();
    testRandomComplexComp();
    testRandomComplexMod();
    testSolveQuadratic();
    testSolveCubic();
    testSolveQuartic();

    printf("\n========================================\n");
    printf("Results: %d / %d tests passed\n", testsPassed, testsRun);
    printf("========================================\n");
    return testsPassed == testsRun ? 0 : 1;
}
