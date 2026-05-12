#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kuma.h"
#include "neko.h"

static int testsRun = 0;
static int testsPassed = 0;

#define CHECK(cond, name) do { \
    testsRun++; \
    if (cond) { testsPassed++; printf("  [PASS] %s\n", name); } \
    else { printf("  [FAIL] %s (line %d)\n", name, __LINE__); } \
} while (0)

#define SECTION(name) printf("\n=== %s ===\n", name)

static bool closeReal(long double a, long double b) {
    return fabsl(a - b) <= 1e-9L;
}

static Number I(long long x) {
    return constructNumberFromInt(x);
}

static Number F(long long num, long long denom) {
    return constructNumberFromFraction(constructFraction(num, denom));
}

static Number R(long double x) {
    return constructNumberFromDouble(x);
}

static void expectInt(const char* name, Number actual, long long expected) {
    CHECK(actual.type == NUMBER_INT && actual.as.i == expected, name);
}

static void expectFraction(const char* name, Number actual, long long num, long long denom) {
    Fraction expected = constructFraction(num, denom);
    CHECK(actual.type == NUMBER_FRACTION
        && actual.as.frac.num == expected.num
        && actual.as.frac.denom == expected.denom, name);
}

static void expectReal(const char* name, Number actual, long double expected) {
    CHECK(actual.type == NUMBER_REAL && closeReal(actual.as.x, expected), name);
}

static void expectNan(const char* name, Number actual) {
    CHECK(actual.type == NUMBER_NAN, name);
}

static void testIsValidStatsNumber(void) {
    SECTION("isValidStatsNumber");

    CHECK(isValidStatsNumber(constructNumberFromInt(5)), "accept int");
    CHECK(isValidStatsNumber(constructNumberFromFraction(constructFraction(2, 3))), "accept valid fraction");
    CHECK(isValidStatsNumber(constructNumberFromDouble(1.25L)), "accept finite real");
    CHECK(!isValidStatsNumber((Number){ .type = NUMBER_FRACTION, .as.frac = {1, 0} }), "reject invalid fraction");
    CHECK(!isValidStatsNumber(constructNumberFromDouble(INFINITY)), "reject infinite real");
    CHECK(!isValidStatsNumber(constructNumberFromComplex((ComplexNumber){1.0L, 2.0L})), "reject complex");
    CHECK(!isValidStatsNumber((Number){ .type = NUMBER_NAN, .as.x = NAN }), "reject Number NAN");
}

static void testIsValidStatsArray(void) {
    SECTION("isValidStatsArray");

    Number valid[] = {
        constructNumberFromInt(1),
        constructNumberFromFraction(constructFraction(1, 2)),
        constructNumberFromDouble(3.0L)
    };
    Number invalid[] = {
        constructNumberFromInt(1),
        constructNumberFromComplex((ComplexNumber){1.0L, 1.0L})
    };

    CHECK(isValidStatsArray(valid, 3), "accept valid array");
    CHECK(!isValidStatsArray(invalid, 2), "reject array containing complex");
    CHECK(!isValidStatsArray(NULL, 3), "reject null array");
    CHECK(!isValidStatsArray(valid, 0), "reject empty array");
}

static void testContainsStatsTypes(void) {
    SECTION("containsFraction / containsReal");

    Number ints[] = {I(1), I(2)};
    Number fractions[] = {I(1), F(1, 2)};
    Number reals[] = {I(1), R(2.5L)};
    Number invalid[] = {I(1), constructNumberFromComplex((ComplexNumber){1.0L, 1.0L})};

    CHECK(!containsFraction(ints, 2), "integer array contains no fraction");
    CHECK(containsFraction(fractions, 2), "fraction array contains fraction");
    CHECK(!containsReal(fractions, 2), "fraction array contains no real");
    CHECK(containsReal(reals, 2), "real array contains real");
    CHECK(!containsFraction(invalid, 2), "invalid array contains no usable fraction");
    CHECK(!containsReal(NULL, 2), "null array contains no real");
}

static void testFactorial(void) {
    SECTION("factorial");

    CHECK(factorial(0) == 1, "zero factorial");
    CHECK(factorial(1) == 1, "one factorial");
    CHECK(factorial(5) == 120, "small factorial");
    CHECK(factorial(20) == 2432902008176640000LL, "largest fitting factorial");
    CHECK(factorial(-1) == 0, "negative factorial returns zero");
    CHECK(factorial(21) == 0, "overflow factorial returns zero");
}

static void testNcr(void) {
    SECTION("ncr");

    CHECK(ncr(5, 2) == 10, "small binomial coefficient");
    CHECK(ncr(52, 5) == 2598960, "card hand binomial coefficient");
    CHECK(ncr(60, 57) == 34220, "symmetric binomial coefficient");
    CHECK(ncr(10, 0) == 1, "zero-subset binomial coefficient");
    CHECK(ncr(5, 6) == 0, "invalid binomial coefficient returns zero");
    CHECK(ncr(67, 33) == 0, "overflow binomial coefficient returns zero");
}

static void testNpr(void) {
    SECTION("npr");

    CHECK(npr(5, 2) == 20, "small permutation count");
    CHECK(npr(10, 0) == 1, "zero-length permutation count");
    CHECK(npr(10, 3) == 720, "larger permutation count");
    CHECK(npr(5, 6) == 0, "invalid permutation count returns zero");
    CHECK(npr(-1, 1) == 0, "negative permutation count returns zero");
    CHECK(npr(21, 21) == 0, "overflow permutation count returns zero");
}

static void testMultinomial(void) {
    SECTION("multinomial");

    long long twoParts[] = {2, 3};
    long long threeParts[] = {1, 2, 3};
    long long withZero[] = {0, 2, 3};
    long long mismatch[] = {2, 2};
    long long negative[] = {1, -1, 3};
    long long overflow[] = {33, 34};

    CHECK(multinomial(5, twoParts, 2) == 10, "two-part multinomial coefficient");
    CHECK(multinomial(6, threeParts, 3) == 60, "three-part multinomial coefficient");
    CHECK(multinomial(5, withZero, 3) == 10, "multinomial coefficient with zero part");
    CHECK(multinomial(0, NULL, 0) == 1, "empty multinomial partition of zero");
    CHECK(multinomial(5, mismatch, 2) == 0, "mismatched parts return zero");
    CHECK(multinomial(3, negative, 3) == 0, "negative part returns zero");
    CHECK(multinomial(67, overflow, 2) == 0, "overflow multinomial coefficient returns zero");
}

static void testSum(void) {
    SECTION("sum");

    Number ia[] = {I(1), I(2), I(3)};
    Number ib[] = {I(-2), I(5)};
    Number fa[] = {F(1, 2), F(1, 3)};
    Number fb[] = {F(2, 3), F(4, 3)};
    Number ra[] = {R(1.5L), R(2.25L)};
    Number rb[] = {R(-1.0L), R(4.0L)};

    expectInt("sum int case 1", sum(ia, 3), 6);
    expectInt("sum int case 2", sum(ib, 2), 3);
    expectFraction("sum fraction case 1", sum(fa, 2), 5, 6);
    expectFraction("sum fraction case 2", sum(fb, 2), 2, 1);
    expectReal("sum real case 1", sum(ra, 2), 3.75L);
    expectReal("sum real case 2", sum(rb, 2), 3.0L);
}

static void testProduct(void) {
    SECTION("product");

    Number ia[] = {I(2), I(3), I(4)};
    Number ib[] = {I(-2), I(5)};
    Number fa[] = {F(1, 2), F(2, 3)};
    Number fb[] = {F(-3, 4), F(2, 5)};
    Number ra[] = {R(1.5L), R(2.0L)};
    Number rb[] = {R(-1.25L), R(4.0L)};

    expectInt("product int case 1", product(ia, 3), 24);
    expectInt("product int case 2", product(ib, 2), -10);
    expectFraction("product fraction case 1", product(fa, 2), 1, 3);
    expectFraction("product fraction case 2", product(fb, 2), -3, 10);
    expectReal("product real case 1", product(ra, 2), 3.0L);
    expectReal("product real case 2", product(rb, 2), -5.0L);
}

static void testMean(void) {
    SECTION("mean");

    Number ia[] = {I(1), I(2), I(3)};
    Number ib[] = {I(1), I(2)};
    Number fa[] = {F(1, 2), F(3, 2)};
    Number fb[] = {F(1, 3), F(2, 3), F(4, 3)};
    Number ra[] = {R(1.0L), R(2.0L)};
    Number rb[] = {R(2.0L), R(4.0L), R(9.0L)};

    expectInt("mean int case 1", mean(ia, 3), 2);
    expectFraction("mean int case 2", mean(ib, 2), 3, 2);
    expectFraction("mean fraction case 1", mean(fa, 2), 1, 1);
    expectFraction("mean fraction case 2", mean(fb, 3), 7, 9);
    expectReal("mean real case 1", mean(ra, 2), 1.5L);
    expectReal("mean real case 2", mean(rb, 3), 5.0L);
}

static void testMedian(void) {
    SECTION("median");

    Number ia[] = {I(3), I(1), I(2)};
    Number ib[] = {I(4), I(1), I(2), I(3)};
    Number fa[] = {F(1, 2), F(3, 2), F(1, 1)};
    Number fb[] = {F(1, 2), F(3, 2)};
    Number ra[] = {R(3.5L), R(1.5L), R(2.5L)};
    Number rb[] = {R(1.0L), R(4.0L)};

    expectInt("median int case 1", median(ia, 3), 2);
    expectFraction("median int case 2", median(ib, 4), 5, 2);
    expectFraction("median fraction case 1", median(fa, 3), 1, 1);
    expectFraction("median fraction case 2", median(fb, 2), 1, 1);
    expectReal("median real case 1", median(ra, 3), 2.5L);
    expectReal("median real case 2", median(rb, 2), 2.5L);
}

static void testMode(void) {
    SECTION("mode");

    Number ia[] = {I(1), I(2), I(2), I(3)};
    Number ib[] = {I(4), I(4), I(5), I(5), I(5)};
    Number fa[] = {F(1, 2), F(2, 3), F(1, 2)};
    Number fb[] = {F(3, 4), F(1, 4), F(3, 4)};
    Number ra[] = {R(1.5L), R(2.5L), R(1.5L)};
    Number rb[] = {R(4.0L), R(4.0L), R(5.0L)};

    expectInt("mode int case 1", mode(ia, 4), 2);
    expectInt("mode int case 2", mode(ib, 5), 5);
    expectFraction("mode fraction case 1", mode(fa, 3), 1, 2);
    expectFraction("mode fraction case 2", mode(fb, 3), 3, 4);
    expectReal("mode real case 1", mode(ra, 3), 1.5L);
    expectReal("mode real case 2", mode(rb, 3), 4.0L);
}

static void testMin(void) {
    SECTION("min");

    Number ia[] = {I(3), I(-1), I(2)};
    Number ib[] = {I(10), I(7)};
    Number fa[] = {F(1, 2), F(-1, 3), F(2, 3)};
    Number fb[] = {F(5, 4), F(3, 4)};
    Number ra[] = {R(2.5L), R(-3.0L), R(1.0L)};
    Number rb[] = {R(8.0L), R(6.5L)};

    expectInt("min int case 1", min(ia, 3), -1);
    expectInt("min int case 2", min(ib, 2), 7);
    expectFraction("min fraction case 1", min(fa, 3), -1, 3);
    expectFraction("min fraction case 2", min(fb, 2), 3, 4);
    expectReal("min real case 1", min(ra, 3), -3.0L);
    expectReal("min real case 2", min(rb, 2), 6.5L);
}

static void testMax(void) {
    SECTION("max");

    Number ia[] = {I(3), I(-1), I(2)};
    Number ib[] = {I(10), I(7)};
    Number fa[] = {F(1, 2), F(-1, 3), F(2, 3)};
    Number fb[] = {F(5, 4), F(3, 4)};
    Number ra[] = {R(2.5L), R(-3.0L), R(1.0L)};
    Number rb[] = {R(8.0L), R(6.5L)};

    expectInt("max int case 1", max(ia, 3), 3);
    expectInt("max int case 2", max(ib, 2), 10);
    expectFraction("max fraction case 1", max(fa, 3), 2, 3);
    expectFraction("max fraction case 2", max(fb, 2), 5, 4);
    expectReal("max real case 1", max(ra, 3), 2.5L);
    expectReal("max real case 2", max(rb, 2), 8.0L);
}

static void testRange(void) {
    SECTION("range");

    Number ia[] = {I(3), I(-1), I(2)};
    Number ib[] = {I(10), I(7)};
    Number fa[] = {F(1, 2), F(-1, 3), F(2, 3)};
    Number fb[] = {F(5, 4), F(3, 4)};
    Number ra[] = {R(2.5L), R(-3.0L), R(1.0L)};
    Number rb[] = {R(8.0L), R(6.5L)};

    expectInt("range int case 1", range(ia, 3), 4);
    expectInt("range int case 2", range(ib, 2), 3);
    expectFraction("range fraction case 1", range(fa, 3), 1, 1);
    expectFraction("range fraction case 2", range(fb, 2), 1, 2);
    expectReal("range real case 1", range(ra, 3), 5.5L);
    expectReal("range real case 2", range(rb, 2), 1.5L);
}

static void testVariance(void) {
    SECTION("variance");

    Number ia[] = {I(1), I(2), I(3)};
    Number ib[] = {I(2), I(4), I(4), I(4), I(5), I(5), I(7), I(9)};
    Number fa[] = {F(1, 2), F(3, 2)};
    Number fb[] = {F(1, 3), F(2, 3), F(1, 1)};
    Number ra[] = {R(1.0L), R(2.0L), R(3.0L)};
    Number rb[] = {R(2.0L), R(4.0L), R(4.0L), R(4.0L), R(5.0L), R(5.0L), R(7.0L), R(9.0L)};

    expectFraction("variance int case 1", variance(ia, 3), 2, 3);
    expectInt("variance int case 2", variance(ib, 8), 4);
    expectFraction("variance fraction case 1", variance(fa, 2), 1, 4);
    expectFraction("variance fraction case 2", variance(fb, 3), 2, 27);
    expectReal("variance real case 1", variance(ra, 3), 2.0L / 3.0L);
    expectReal("variance real case 2", variance(rb, 8), 4.0L);
}

static void testSampleVariance(void) {
    SECTION("sampleVariance");

    Number ia[] = {I(1), I(2), I(3)};
    Number ib[] = {I(1), I(2), I(4)};
    Number fa[] = {F(1, 2), F(3, 2)};
    Number fb[] = {F(1, 3), F(2, 3), F(1, 1)};
    Number ra[] = {R(1.0L), R(2.0L), R(3.0L)};
    Number rb[] = {R(2.0L), R(4.0L), R(6.0L)};

    expectInt("sample variance int case 1", sampleVariance(ia, 3), 1);
    expectFraction("sample variance int case 2", sampleVariance(ib, 3), 7, 3);
    expectFraction("sample variance fraction case 1", sampleVariance(fa, 2), 1, 2);
    expectFraction("sample variance fraction case 2", sampleVariance(fb, 3), 1, 9);
    expectReal("sample variance real case 1", sampleVariance(ra, 3), 1.0L);
    expectReal("sample variance real case 2", sampleVariance(rb, 3), 4.0L);
}

static void testStddev(void) {
    SECTION("stddev");

    Number ia[] = {I(2), I(2), I(2)};
    Number ib[] = {I(1), I(2), I(3)};
    Number fa[] = {F(1, 2), F(3, 2)};
    Number fb[] = {F(1, 3), F(2, 3), F(1, 1)};
    Number ra[] = {R(2.0L), R(2.0L), R(2.0L)};
    Number rb[] = {R(1.0L), R(2.0L), R(3.0L)};

    expectReal("stddev int case 1", stddev(ia, 3), 0.0L);
    expectReal("stddev int case 2", stddev(ib, 3), sqrtl(2.0L / 3.0L));
    expectReal("stddev fraction case 1", stddev(fa, 2), 0.5L);
    expectReal("stddev fraction case 2", stddev(fb, 3), sqrtl(2.0L / 27.0L));
    expectReal("stddev real case 1", stddev(ra, 3), 0.0L);
    expectReal("stddev real case 2", stddev(rb, 3), sqrtl(2.0L / 3.0L));
}

static void testSampleStddev(void) {
    SECTION("sampleStddev");

    Number ia[] = {I(1), I(2), I(3)};
    Number ib[] = {I(2), I(4), I(6)};
    Number fa[] = {F(1, 2), F(3, 2)};
    Number fb[] = {F(1, 3), F(2, 3), F(1, 1)};
    Number ra[] = {R(1.0L), R(2.0L), R(3.0L)};
    Number rb[] = {R(2.0L), R(4.0L), R(6.0L)};

    expectReal("sample stddev int case 1", sampleStddev(ia, 3), 1.0L);
    expectReal("sample stddev int case 2", sampleStddev(ib, 3), 2.0L);
    expectReal("sample stddev fraction case 1", sampleStddev(fa, 2), sqrtl(0.5L));
    expectReal("sample stddev fraction case 2", sampleStddev(fb, 3), 1.0L / 3.0L);
    expectReal("sample stddev real case 1", sampleStddev(ra, 3), 1.0L);
    expectReal("sample stddev real case 2", sampleStddev(rb, 3), 2.0L);
}

static void testMeanAbsDev(void) {
    SECTION("meanAbsDev");

    Number ia[] = {I(1), I(2), I(3)};
    Number ib[] = {I(1), I(2)};
    Number fa[] = {F(1, 2), F(3, 2)};
    Number fb[] = {F(1, 3), F(2, 3), F(1, 1)};
    Number ra[] = {R(1.0L), R(2.0L), R(3.0L)};
    Number rb[] = {R(1.0L), R(2.0L)};

    expectFraction("mean abs dev int case 1", meanAbsDev(ia, 3), 2, 3);
    expectFraction("mean abs dev int case 2", meanAbsDev(ib, 2), 1, 2);
    expectFraction("mean abs dev fraction case 1", meanAbsDev(fa, 2), 1, 2);
    expectFraction("mean abs dev fraction case 2", meanAbsDev(fb, 3), 2, 9);
    expectReal("mean abs dev real case 1", meanAbsDev(ra, 3), 2.0L / 3.0L);
    expectReal("mean abs dev real case 2", meanAbsDev(rb, 2), 0.5L);
}

static void testMedianAbsDev(void) {
    SECTION("medianAbsDev");

    Number ia[] = {I(1), I(2), I(3)};
    Number ib[] = {I(1), I(2), I(100)};
    Number fa[] = {F(1, 2), F(3, 2)};
    Number fb[] = {F(1, 3), F(2, 3), F(1, 1)};
    Number ra[] = {R(1.0L), R(2.0L), R(3.0L)};
    Number rb[] = {R(1.0L), R(2.0L), R(100.0L)};

    expectInt("median abs dev int case 1", medianAbsDev(ia, 3), 1);
    expectInt("median abs dev int case 2", medianAbsDev(ib, 3), 1);
    expectFraction("median abs dev fraction case 1", medianAbsDev(fa, 2), 1, 2);
    expectFraction("median abs dev fraction case 2", medianAbsDev(fb, 3), 1, 3);
    expectReal("median abs dev real case 1", medianAbsDev(ra, 3), 1.0L);
    expectReal("median abs dev real case 2", medianAbsDev(rb, 3), 1.0L);
}

static void testPercentile(void) {
    SECTION("percentile");

    Number ia[] = {I(0), I(10)};
    Number ib[] = {I(10), I(20), I(30)};
    Number fa[] = {F(0, 1), F(1, 1)};
    Number fb[] = {F(1, 2), F(3, 2)};
    Number ra[] = {R(0.0L), R(10.0L)};
    Number rb[] = {R(10.0L), R(20.0L), R(30.0L)};

    expectFraction("percentile int case 1", percentile(ia, 2, I(25)), 5, 2);
    expectInt("percentile int case 2", percentile(ib, 3, I(50)), 20);
    expectFraction("percentile fraction case 1", percentile(fa, 2, I(25)), 1, 4);
    expectFraction("percentile fraction case 2", percentile(fb, 2, I(50)), 1, 1);
    expectReal("percentile real case 1", percentile(ra, 2, I(25)), 2.5L);
    expectReal("percentile real case 2", percentile(rb, 3, I(75)), 25.0L);
}

static void testQuartile(void) {
    SECTION("quartile");

    Number ia[] = {I(0), I(10)};
    Number ib[] = {I(0), I(10), I(20), I(30)};
    Number fa[] = {F(0, 1), F(1, 1)};
    Number fb[] = {F(1, 2), F(3, 2)};
    Number ra[] = {R(0.0L), R(10.0L)};
    Number rb[] = {R(0.0L), R(10.0L), R(20.0L), R(30.0L)};

    expectFraction("quartile int case 1", quartile(ia, 2, 1), 5, 2);
    expectFraction("quartile int case 2", quartile(ib, 4, 3), 45, 2);
    expectFraction("quartile fraction case 1", quartile(fa, 2, 1), 1, 4);
    expectFraction("quartile fraction case 2", quartile(fb, 2, 3), 5, 4);
    expectReal("quartile real case 1", quartile(ra, 2, 1), 2.5L);
    expectReal("quartile real case 2", quartile(rb, 4, 3), 22.5L);
}

static void testIqr(void) {
    SECTION("iqr");

    Number ia[] = {I(0), I(10)};
    Number ib[] = {I(0), I(10), I(20), I(30)};
    Number fa[] = {F(0, 1), F(1, 1)};
    Number fb[] = {F(1, 2), F(3, 2)};
    Number ra[] = {R(0.0L), R(10.0L)};
    Number rb[] = {R(0.0L), R(10.0L), R(20.0L), R(30.0L)};

    expectFraction("iqr int case 1", iqr(ia, 2), 5, 1);
    expectFraction("iqr int case 2", iqr(ib, 4), 15, 1);
    expectFraction("iqr fraction case 1", iqr(fa, 2), 1, 2);
    expectFraction("iqr fraction case 2", iqr(fb, 2), 1, 2);
    expectReal("iqr real case 1", iqr(ra, 2), 5.0L);
    expectReal("iqr real case 2", iqr(rb, 4), 15.0L);
}

static void testGeometricMean(void) {
    SECTION("geometricMean");

    Number ia[] = {I(2), I(8)};
    Number ib[] = {I(1), I(4), I(16)};
    Number fa[] = {F(1, 2), F(2, 1)};
    Number fb[] = {F(1, 4), F(4, 1)};
    Number ra[] = {R(2.0L), R(8.0L)};
    Number rb[] = {R(1.0L), R(9.0L)};

    expectReal("geometric mean int case 1", geometricMean(ia, 2), 4.0L);
    expectReal("geometric mean int case 2", geometricMean(ib, 3), 4.0L);
    expectReal("geometric mean fraction case 1", geometricMean(fa, 2), 1.0L);
    expectReal("geometric mean fraction case 2", geometricMean(fb, 2), 1.0L);
    expectReal("geometric mean real case 1", geometricMean(ra, 2), 4.0L);
    expectReal("geometric mean real case 2", geometricMean(rb, 2), 3.0L);
}

static void testHarmonicMean(void) {
    SECTION("harmonicMean");

    Number ia[] = {I(1), I(2)};
    Number ib[] = {I(2), I(6)};
    Number fa[] = {F(1, 2), F(1, 1)};
    Number fb[] = {F(1, 3), F(1, 1)};
    Number ra[] = {R(1.0L), R(2.0L)};
    Number rb[] = {R(2.0L), R(6.0L)};

    expectFraction("harmonic mean int case 1", harmonicMean(ia, 2), 4, 3);
    expectFraction("harmonic mean int case 2", harmonicMean(ib, 2), 3, 1);
    expectFraction("harmonic mean fraction case 1", harmonicMean(fa, 2), 2, 3);
    expectFraction("harmonic mean fraction case 2", harmonicMean(fb, 2), 1, 2);
    expectReal("harmonic mean real case 1", harmonicMean(ra, 2), 4.0L / 3.0L);
    expectReal("harmonic mean real case 2", harmonicMean(rb, 2), 3.0L);
}

static void testFrequencyFunctions(void) {
    SECTION("frequency functions");

    Number data[] = {I(2), I(1), I(2), I(3), I(1)};
    Number sorted[] = {I(1), I(1), I(2), I(3), I(3), I(3)};
    Number modal[] = {I(1), I(2), I(2), I(3), I(3)};
    Number invalid[] = {I(1), constructNumberFromComplex((ComplexNumber){1.0L, 1.0L})};

    CHECK(numFreq(data, I(2), 5) == 2, "numFreq counts integer matches");
    CHECK(numFreq(data, F(2, 1), 5) == 2, "numFreq counts numerically equal fraction matches");
    CHECK(contains(data, I(3), 5), "contains finds present value");
    CHECK(!contains(data, I(4), 5), "contains rejects absent value");
    CHECK(countDistinct(data, 5) == 3, "countDistinct counts unsorted values");
    CHECK(countDistinct(invalid, 2) == 0, "countDistinct rejects invalid data");

    size_t freqCount = 0;
    Frequency* freqs = frequencies(data, 5, &freqCount);
    CHECK(freqs && freqCount == 3, "frequencies returns three values");
    CHECK(freqs && eqNumbers(freqs[0].value, I(1)) && freqs[0].count == 2, "frequencies first run");
    CHECK(freqs && eqNumbers(freqs[1].value, I(2)) && freqs[1].count == 2, "frequencies second run");
    CHECK(freqs && eqNumbers(freqs[2].value, I(3)) && freqs[2].count == 1, "frequencies third run");
    freeFrequencies(freqs);

    freqCount = 0;
    freqs = frequenciesSorted(sorted, 6, &freqCount);
    CHECK(freqs && freqCount == 3, "frequenciesSorted returns three values");
    CHECK(freqs && eqNumbers(freqs[0].value, I(1)) && freqs[0].count == 2, "frequenciesSorted first run");
    CHECK(freqs && eqNumbers(freqs[1].value, I(2)) && freqs[1].count == 1, "frequenciesSorted second run");
    CHECK(freqs && eqNumbers(freqs[2].value, I(3)) && freqs[2].count == 3, "frequenciesSorted third run");
    freeFrequencies(freqs);

    size_t modeCount = 0;
    Number* modeVals = modes(modal, 5, &modeCount);
    CHECK(modeVals && modeCount == 2, "modes returns tied modes");
    CHECK(modeVals && eqNumbers(modeVals[0], I(2)), "modes first tied value");
    CHECK(modeVals && eqNumbers(modeVals[1], I(3)), "modes second tied value");
    freeModes(modeVals);

    freqCount = 42;
    freqs = frequencies(invalid, 2, &freqCount);
    CHECK(!freqs && freqCount == 0, "frequencies rejects invalid data");
    freeFrequencies(NULL);
    freeModes(NULL);
    CHECK(true, "free frequency helpers accept NULL");
}

static void testCovariance(void) {
    SECTION("covariance");

    Number xi[] = {I(1), I(2), I(3)};
    Number yi[] = {I(2), I(4), I(6)};
    Number xf[] = {F(1, 2), F(3, 2)};
    Number yf[] = {F(1, 1), F(2, 1)};
    Number xr[] = {R(1.0L), R(2.0L), R(3.0L)};
    Number yr[] = {R(3.0L), R(5.0L), R(7.0L)};

    expectFraction("covariance int case", covariance(xi, yi, 3), 4, 3);
    expectFraction("covariance fraction case", covariance(xf, yf, 2), 1, 4);
    expectReal("covariance real case", covariance(xr, yr, 3), 4.0L / 3.0L);
}

static void testSampleCovariance(void) {
    SECTION("sampleCovariance");

    Number xi[] = {I(1), I(2), I(3)};
    Number yi[] = {I(2), I(4), I(6)};
    Number xf[] = {F(1, 2), F(3, 2)};
    Number yf[] = {F(1, 1), F(2, 1)};
    Number xr[] = {R(1.0L), R(2.0L), R(3.0L)};
    Number yr[] = {R(3.0L), R(5.0L), R(7.0L)};

    expectInt("sample covariance int case", sampleCovariance(xi, yi, 3), 2);
    expectFraction("sample covariance fraction case", sampleCovariance(xf, yf, 2), 1, 2);
    expectReal("sample covariance real case", sampleCovariance(xr, yr, 3), 2.0L);
    expectNan("sample covariance rejects singleton", sampleCovariance(xi, yi, 1));
}

static void testCorrelation(void) {
    SECTION("correlation");

    Number xi[] = {I(1), I(2), I(3)};
    Number yi[] = {I(2), I(4), I(6)};
    Number xf[] = {F(1, 2), F(3, 2)};
    Number yf[] = {F(1, 1), F(2, 1)};
    Number xr[] = {R(1.0L), R(2.0L), R(3.0L)};
    Number yr[] = {R(3.0L), R(5.0L), R(7.0L)};
    Number flat[] = {I(1), I(1), I(1)};

    expectReal("correlation int case", correlation(xi, yi, 3), 1.0L);
    expectReal("correlation fraction case", correlation(xf, yf, 2), 1.0L);
    expectReal("correlation real case", correlation(xr, yr, 3), 1.0L);
    expectNan("correlation rejects zero variance", correlation(flat, yi, 3));
}

static void testLinearRegression(void) {
    SECTION("linear regression");

    Number xi[] = {I(1), I(2), I(3)};
    Number yi[] = {I(2), I(4), I(6)};
    Number xf[] = {F(1, 2), F(3, 2)};
    Number yf[] = {F(1, 1), F(2, 1)};
    Number xr[] = {R(1.0L), R(2.0L), R(3.0L)};
    Number yr[] = {R(3.0L), R(5.0L), R(7.0L)};
    Number flat[] = {I(1), I(1), I(1)};

    expectFraction("regression slope int case", linearRegressionSlope(xi, yi, 3), 2, 1);
    expectFraction("regression intercept int case", linearRegressionIntercept(xi, yi, 3), 0, 1);
    expectInt("regression predict int case", linearRegressionPredict(I(2), I(0), I(4)), 8);

    expectFraction("regression slope fraction case", linearRegressionSlope(xf, yf, 2), 1, 1);
    expectFraction("regression intercept fraction case", linearRegressionIntercept(xf, yf, 2), 1, 2);
    expectFraction("regression predict fraction case", linearRegressionPredict(I(1), F(1, 2), I(2)), 5, 2);

    expectReal("regression slope real case", linearRegressionSlope(xr, yr, 3), 2.0L);
    expectReal("regression intercept real case", linearRegressionIntercept(xr, yr, 3), 1.0L);
    expectReal("regression predict real case", linearRegressionPredict(R(2.0L), R(1.0L), R(4.0L)), 9.0L);

    expectNan("regression slope rejects zero x variance", linearRegressionSlope(flat, yi, 3));
    expectNan("regression predict rejects complex input", linearRegressionPredict(I(1), I(0), constructNumberFromComplex((ComplexNumber){1.0L, 1.0L})));
}

static void testBernoulliDistributionFunctions(void) {
    SECTION("Bernoulli distribution functions");

    BernoulliParams fracParams = { .p = F(1, 4) };
    ProbabilityDistribution fracDist = {
        .type = KUMA_DIST_DISCRETE,
        .kind = KUMA_DIST_BERNOULLI,
        .params = &fracParams,
        .ownsParams = false
    };
    BernoulliParams realParams = { .p = R(0.25L) };
    ProbabilityDistribution realDist = {
        .type = KUMA_DIST_DISCRETE,
        .kind = KUMA_DIST_BERNOULLI,
        .params = &realParams,
        .ownsParams = false
    };
    BernoulliParams zeroParams = { .p = I(0) };
    ProbabilityDistribution zeroDist = {
        .type = KUMA_DIST_DISCRETE,
        .kind = KUMA_DIST_BERNOULLI,
        .params = &zeroParams,
        .ownsParams = false
    };
    BernoulliParams oneParams = { .p = I(1) };
    ProbabilityDistribution oneDist = {
        .type = KUMA_DIST_DISCRETE,
        .kind = KUMA_DIST_BERNOULLI,
        .params = &oneParams,
        .ownsParams = false
    };
    BernoulliParams invalidParams = { .p = I(2) };
    ProbabilityDistribution invalidDist = {
        .type = KUMA_DIST_DISCRETE,
        .kind = KUMA_DIST_BERNOULLI,
        .params = &invalidParams,
        .ownsParams = false
    };

    expectFraction("Bernoulli PMF at zero", bernoulliPMF(&fracDist, I(0)), 3, 4);
    expectFraction("Bernoulli PMF at one", bernoulliPMF(&fracDist, I(1)), 1, 4);
    expectInt("Bernoulli PMF outside support", bernoulliPMF(&fracDist, F(1, 2)), 0);
    expectInt("Bernoulli PMF complex input outside support", bernoulliPMF(&fracDist, constructNumberFromComplex((ComplexNumber){1.0L, 1.0L})), 0);

    expectInt("Bernoulli CDF below support", bernoulliCDF(&fracDist, I(-1)), 0);
    expectFraction("Bernoulli CDF between support points", bernoulliCDF(&fracDist, F(1, 2)), 3, 4);
    expectInt("Bernoulli CDF above support", bernoulliCDF(&fracDist, I(1)), 1);
    expectNan("Bernoulli CDF rejects complex input", bernoulliCDF(&fracDist, constructNumberFromComplex((ComplexNumber){1.0L, 1.0L})));

    expectFraction("Bernoulli mean exact", bernoulliMean(&fracDist), 1, 4);
    expectReal("Bernoulli mean real", bernoulliMean(&realDist), 0.25L);
    expectFraction("Bernoulli variance exact", bernoulliVariance(&fracDist), 3, 16);
    expectReal("Bernoulli variance real", bernoulliVariance(&realDist), 0.1875L);
    expectReal("Bernoulli standard deviation exact", bernoulliStddev(&fracDist), sqrtl(3.0L / 16.0L));
    expectReal("Bernoulli standard deviation real", bernoulliStddev(&realDist), sqrtl(0.1875L));

    expectInt("Bernoulli sample with p = 0", bernoulliSample(&zeroDist), 0);
    expectInt("Bernoulli sample with p = 1", bernoulliSample(&oneDist), 1);
    expectNan("Bernoulli rejects invalid probability", bernoulliMean(&invalidDist));
    expectNan("Bernoulli rejects null distribution", bernoulliVariance(NULL));
}

static Number customPDFCallback(ProbabilityDistribution* dist, Number x) {
    (void)dist;
    return x;
}

static Number customCDFCallback(ProbabilityDistribution* dist, Number x) {
    (void)dist;
    return multNumbers(x, x);
}

static Number customMeanCallback(ProbabilityDistribution* dist) {
    (void)dist;
    return F(2, 3);
}

static Number customVarianceCallback(ProbabilityDistribution* dist) {
    (void)dist;
    return F(1, 18);
}

static Number customSampleCallback(ProbabilityDistribution* dist) {
    (void)dist;
    return F(1, 2);
}

static void testRemainingDiscreteDistributionFunctions(void) {
    SECTION("remaining discrete distribution functions");

    BinomialParams binParams = { .n = 3, .p = F(1, 2) };
    ProbabilityDistribution binDist = { .type = KUMA_DIST_DISCRETE, .kind = KUMA_DIST_BINOMIAL, .params = &binParams };
    BinomialParams binOneParams = { .n = 3, .p = I(1) };
    ProbabilityDistribution binOneDist = { .type = KUMA_DIST_DISCRETE, .kind = KUMA_DIST_BINOMIAL, .params = &binOneParams };
    GeometricParams geoParams = { .p = F(1, 2) };
    ProbabilityDistribution geoDist = { .type = KUMA_DIST_DISCRETE, .kind = KUMA_DIST_GEOMETRIC, .params = &geoParams };
    GeometricParams geoOneParams = { .p = I(1) };
    ProbabilityDistribution geoOneDist = { .type = KUMA_DIST_DISCRETE, .kind = KUMA_DIST_GEOMETRIC, .params = &geoOneParams };
    PoissonParams poisParams = { .lambda = I(2) };
    ProbabilityDistribution poisDist = { .type = KUMA_DIST_DISCRETE, .kind = KUMA_DIST_POISSON, .params = &poisParams };
    DiscreteUniformParams duParams = { .a = 2, .b = 4 };
    ProbabilityDistribution duDist = { .type = KUMA_DIST_DISCRETE, .kind = KUMA_DIST_DISCRETE_UNIFORM, .params = &duParams };

    expectFraction("binomial PMF", binomialPMF(&binDist, I(1)), 3, 8);
    expectFraction("binomial CDF", binomialCDF(&binDist, I(1)), 1, 2);
    expectFraction("binomial mean", binomialMean(&binDist), 3, 2);
    expectFraction("binomial variance", binomialVariance(&binDist), 3, 4);
    expectReal("binomial standard deviation", binomialStddev(&binDist), sqrtl(3.0L / 4.0L));
    expectInt("binomial sample with p = 1", binomialSample(&binOneDist), 3);

    expectFraction("geometric PMF", geometricPMF(&geoDist, I(3)), 1, 8);
    expectFraction("geometric CDF", geometricCDF(&geoDist, I(2)), 3, 4);
    expectFraction("geometric mean", geometricMeanDist(&geoDist), 2, 1);
    expectFraction("geometric variance", geometricVariance(&geoDist), 2, 1);
    expectReal("geometric standard deviation", geometricStddev(&geoDist), sqrtl(2.0L));
    expectInt("geometric sample with p = 1", geometricSample(&geoOneDist), 1);

    expectReal("Poisson PMF", poissonPMF(&poisDist, I(2)), 2.0L * expl(-2.0L));
    expectReal("Poisson CDF", poissonCDF(&poisDist, I(2)), 5.0L * expl(-2.0L));
    expectInt("Poisson mean", poissonMean(&poisDist), 2);
    expectInt("Poisson variance", poissonVariance(&poisDist), 2);
    expectReal("Poisson standard deviation", poissonStddev(&poisDist), sqrtl(2.0L));

    expectFraction("discrete uniform PMF", discreteUniformPMF(&duDist, I(3)), 1, 3);
    expectFraction("discrete uniform CDF", discreteUniformCDF(&duDist, I(3)), 2, 3);
    expectInt("discrete uniform mean", discreteUniformMean(&duDist), 3);
    expectFraction("discrete uniform variance", discreteUniformVariance(&duDist), 2, 3);
    expectReal("discrete uniform standard deviation", discreteUniformStddev(&duDist), sqrtl(2.0L / 3.0L));

    seedPRG(12ULL);
    Number sample = discreteUniformSample(&duDist);
    CHECK(sample.type == NUMBER_INT && sample.as.i >= 2 && sample.as.i <= 4, "discrete uniform sample lies in support");
}

static void testContinuousDistributionFunctions(void) {
    SECTION("continuous distribution functions");

    ContinuousUniformParams cuParams = { .a = I(2), .b = I(6) };
    ProbabilityDistribution cuDist = { .type = KUMA_DIST_CONTINUOUS, .kind = KUMA_DIST_CONTINUOUS_UNIFORM, .params = &cuParams };
    NormalParams normalParams = { .mu = I(0), .sigma = I(1) };
    ProbabilityDistribution normalDist = { .type = KUMA_DIST_CONTINUOUS, .kind = KUMA_DIST_NORMAL, .params = &normalParams };
    ExponentialParams expParams = { .lambda = I(2) };
    ProbabilityDistribution expDist = { .type = KUMA_DIST_CONTINUOUS, .kind = KUMA_DIST_EXPONENTIAL, .params = &expParams };

    expectReal("continuous uniform PDF", continuousUniformPDF(&cuDist, I(3)), 0.25L);
    expectInt("continuous uniform PDF outside support", continuousUniformPDF(&cuDist, I(6)), 0);
    expectReal("continuous uniform CDF", continuousUniformCDF(&cuDist, I(4)), 0.5L);
    expectReal("continuous uniform mean", continuousUniformMean(&cuDist), 4.0L);
    expectReal("continuous uniform variance", continuousUniformVariance(&cuDist), 16.0L / 12.0L);
    expectReal("continuous uniform standard deviation", continuousUniformStddev(&cuDist), sqrtl(16.0L / 12.0L));

    NekoExpr* cuPDF = continuousUniformPDFfunc(&cuDist);
    NekoExpr* cuCDF = continuousUniformCDFfunc(&cuDist);
    CHECK(cuPDF && closeReal(nekoEvalExpr(cuPDF, "x", 3.0L), 0.25L), "continuous uniform PDF func evaluates inside support");
    CHECK(cuPDF && closeReal(nekoEvalExpr(cuPDF, "x", 6.0L), 0.0L), "continuous uniform PDF func evaluates outside support");
    CHECK(cuCDF && closeReal(nekoEvalExpr(cuCDF, "x", 4.0L), 0.5L), "continuous uniform CDF func evaluates inside support");
    nekoFreeExpr(cuPDF);
    nekoFreeExpr(cuCDF);

    expectReal("normal PDF", normalPDF(&normalDist, I(0)), 1.0L / sqrtl(2.0L * M_PI));
    expectReal("normal CDF", normalCDF(&normalDist, I(0)), 0.5L);
    expectInt("normal mean", normalMean(&normalDist), 0);
    expectInt("normal variance", normalVariance(&normalDist), 1);
    expectInt("normal standard deviation", normalStddev(&normalDist), 1);

    NekoExpr* normalPDFExpr = normalPDFfunc(&normalDist);
    NekoExpr* normalCDFExpr = normalCDFfunc(&normalDist);
    CHECK(normalPDFExpr && closeReal(nekoEvalExpr(normalPDFExpr, "x", 0.0L), 1.0L / sqrtl(2.0L * M_PI)), "normal PDF func evaluates at mean");
    CHECK(normalCDFExpr && closeReal(nekoEvalExpr(normalCDFExpr, "x", 0.0L), 0.5L), "normal CDF func evaluates at mean");
    nekoFreeExpr(normalPDFExpr);
    nekoFreeExpr(normalCDFExpr);

    expectReal("exponential PDF", exponentialPDF(&expDist, I(1)), 2.0L * expl(-2.0L));
    expectReal("exponential CDF", exponentialCDF(&expDist, I(1)), 1.0L - expl(-2.0L));
    expectFraction("exponential mean", exponentialMean(&expDist), 1, 2);
    expectFraction("exponential variance", exponentialVariance(&expDist), 1, 4);
    expectFraction("exponential standard deviation", exponentialStddev(&expDist), 1, 2);

    NekoExpr* expPDF = exponentialPDFfunc(&expDist);
    NekoExpr* expCDF = exponentialCDFfunc(&expDist);
    CHECK(expPDF && closeReal(nekoEvalExpr(expPDF, "x", 1.0L), 2.0L * expl(-2.0L)), "exponential PDF func evaluates inside support");
    CHECK(expPDF && closeReal(nekoEvalExpr(expPDF, "x", -1.0L), 0.0L), "exponential PDF func evaluates below support");
    CHECK(expCDF && closeReal(nekoEvalExpr(expCDF, "x", 1.0L), 1.0L - expl(-2.0L)), "exponential CDF func evaluates inside support");
    nekoFreeExpr(expPDF);
    nekoFreeExpr(expCDF);
}

static void testCustomDistributionFunctions(void) {
    SECTION("custom distribution functions");

    Number values[] = {I(1), I(2)};
    Number probabilities[] = {F(1, 4), F(3, 4)};
    CustomDiscreteParams cdParams = { .values = values, .probabilities = probabilities, .size = 2 };
    ProbabilityDistribution cdDist = { .type = KUMA_DIST_DISCRETE, .kind = KUMA_DIST_CUSTOM, .params = &cdParams };
    CustomContinuousParams ccParams = { .lower = I(0), .upper = I(1) };
    ProbabilityDistribution ccDist = {
        .type = KUMA_DIST_CONTINUOUS,
        .kind = KUMA_DIST_CUSTOM,
        .params = &ccParams,
        .pdf = customPDFCallback,
        .cdf = customCDFCallback,
        .mean = customMeanCallback,
        .variance = customVarianceCallback,
        .sample = customSampleCallback
    };

    expectFraction("custom discrete PMF", customDiscretePMF(&cdDist, I(2)), 3, 4);
    expectInt("custom discrete PMF outside support", customDiscretePMF(&cdDist, I(3)), 0);
    expectInt("custom discrete CDF below support", customDiscreteCDF(&cdDist, I(0)), 0);
    expectFraction("custom discrete CDF", customDiscreteCDF(&cdDist, I(1)), 1, 4);
    expectFraction("custom discrete mean", customDiscreteMean(&cdDist), 7, 4);
    expectFraction("custom discrete variance", customDiscreteVariance(&cdDist), 3, 16);
    expectReal("custom discrete standard deviation", customDiscreteStddev(&cdDist), sqrtl(3.0L / 16.0L));

    expectFraction("custom continuous PDF callback", customContinuousPDF(&ccDist, F(1, 2)), 1, 2);
    expectFraction("custom continuous CDF callback", customContinuousCDF(&ccDist, F(1, 2)), 1, 4);
    expectFraction("custom continuous mean callback", customContinuousMean(&ccDist), 2, 3);
    expectFraction("custom continuous variance callback", customContinuousVariance(&ccDist), 1, 18);
    expectReal("custom continuous standard deviation fallback", customContinuousStddev(&ccDist), sqrtl(1.0L / 18.0L));
    expectFraction("custom continuous sample callback", customContinuousSample(&ccDist), 1, 2);
}

static void testProbabilityDistributionConstructors(void) {
    SECTION("ProbabilityDistribution constructors and accessors");

    ProbabilityDistribution* bern = constructBernoulliDistribution(F(1, 4));
    CHECK(bern && bern->type == KUMA_DIST_DISCRETE && bern->kind == KUMA_DIST_BERNOULLI, "construct Bernoulli distribution");
    expectFraction("Bernoulli accessor PMF", probabilityPMF(bern, I(1)), 1, 4);
    expectFraction("Bernoulli accessor CDF", probabilityCDF(bern, I(0)), 3, 4);
    expectFraction("Bernoulli accessor mean", probabilityMean(bern), 1, 4);
    expectFraction("Bernoulli accessor variance", probabilityVariance(bern), 3, 16);
    expectReal("Bernoulli accessor standard deviation", probabilityStddev(bern), sqrtl(3.0L / 16.0L));
    expectNan("Bernoulli PDF accessor is unavailable", probabilityPDF(bern, I(0)));
    freeProbabilityDistribution(bern);

    ProbabilityDistribution* bin = constructBinomialDistribution(3, F(1, 2));
    CHECK(bin && bin->pmf == binomialPMF && bin->cdf == binomialCDF, "construct Binomial distribution");
    expectFraction("Binomial accessor PMF", probabilityPMF(bin, I(2)), 3, 8);
    expectFraction("Binomial accessor mean", probabilityMean(bin), 3, 2);
    freeProbabilityDistribution(bin);

    ProbabilityDistribution* geo = constructGeometricDistribution(F(1, 2));
    CHECK(geo && geo->pmf == geometricPMF, "construct Geometric distribution");
    expectFraction("Geometric accessor CDF", probabilityCDF(geo, I(2)), 3, 4);
    expectFraction("Geometric accessor mean", probabilityMean(geo), 2, 1);
    freeProbabilityDistribution(geo);

    ProbabilityDistribution* pois = constructPoissonDistribution(I(2));
    CHECK(pois && pois->pmf == poissonPMF, "construct Poisson distribution");
    expectReal("Poisson accessor PMF", probabilityPMF(pois, I(2)), 2.0L * expl(-2.0L));
    expectInt("Poisson accessor variance", probabilityVariance(pois), 2);
    freeProbabilityDistribution(pois);

    ProbabilityDistribution* du = constructDiscreteUniformDistribution(2, 4);
    CHECK(du && du->pmf == discreteUniformPMF, "construct discrete uniform distribution");
    expectFraction("discrete uniform accessor PMF", probabilityPMF(du, I(2)), 1, 3);
    expectInt("discrete uniform accessor mean", probabilityMean(du), 3);
    freeProbabilityDistribution(du);

    ProbabilityDistribution* cu = constructContinuousUniformDistribution(I(2), I(6));
    CHECK(cu && cu->pdf == continuousUniformPDF, "construct continuous uniform distribution");
    expectReal("continuous uniform accessor PDF", probabilityPDF(cu, I(3)), 0.25L);
    expectReal("continuous uniform accessor CDF", probabilityCDF(cu, I(4)), 0.5L);
    expectNan("continuous uniform PMF accessor is unavailable", probabilityPMF(cu, I(3)));
    freeProbabilityDistribution(cu);

    ProbabilityDistribution* normal = constructNormalDistribution(I(0), I(1));
    CHECK(normal && normal->pdf == normalPDF, "construct normal distribution");
    expectReal("normal accessor PDF", probabilityPDF(normal, I(0)), 1.0L / sqrtl(2.0L * M_PI));
    expectReal("normal accessor CDF", probabilityCDF(normal, I(0)), 0.5L);
    freeProbabilityDistribution(normal);

    ProbabilityDistribution* expDist = constructExponentialDistribution(I(2));
    CHECK(expDist && expDist->pdf == exponentialPDF, "construct exponential distribution");
    expectReal("exponential accessor PDF", probabilityPDF(expDist, I(1)), 2.0L * expl(-2.0L));
    expectFraction("exponential accessor mean", probabilityMean(expDist), 1, 2);
    freeProbabilityDistribution(expDist);

    Number values[] = {I(1), I(2)};
    Number probabilities[] = {F(1, 4), F(3, 4)};
    ProbabilityDistribution* copied = constructCustomDiscreteDistribution(values, probabilities, 2, true);
    CHECK(copied && copied->pmf == customDiscretePMF, "construct copied custom discrete distribution");
    values[1] = I(10);
    expectFraction("copied custom discrete keeps source-independent values", probabilityPMF(copied, I(2)), 3, 4);
    freeProbabilityDistribution(copied);

    Number borrowedValues[] = {I(1), I(2)};
    Number borrowedProbabilities[] = {F(1, 4), F(3, 4)};
    ProbabilityDistribution* borrowed = constructCustomDiscreteDistribution(borrowedValues, borrowedProbabilities, 2, false);
    CHECK(borrowed && borrowed->pmf == customDiscretePMF, "construct borrowed custom discrete distribution");
    borrowedValues[1] = I(10);
    expectFraction("borrowed custom discrete sees source values", probabilityPMF(borrowed, I(10)), 3, 4);
    freeProbabilityDistribution(borrowed);

    ProbabilityDistribution* customCont = constructCustomContinuousDistribution(
        I(0), I(1),
        customPDFCallback,
        customCDFCallback,
        customMeanCallback,
        customVarianceCallback,
        customSampleCallback
    );
    CHECK(customCont && customCont->pdf == customPDFCallback, "construct custom continuous distribution");
    expectFraction("custom continuous accessor PDF", probabilityPDF(customCont, F(1, 2)), 1, 2);
    expectFraction("custom continuous accessor CDF", probabilityCDF(customCont, F(1, 2)), 1, 4);
    expectFraction("custom continuous accessor mean", probabilityMean(customCont), 2, 3);
    expectReal("custom continuous accessor stddev fallback", probabilityStddev(customCont), sqrtl(1.0L / 18.0L));
    expectFraction("custom continuous accessor sample", probabilitySample(customCont), 1, 2);
    freeProbabilityDistribution(customCont);

    CHECK(!constructBernoulliDistribution(I(2)), "invalid Bernoulli constructor rejected");
    CHECK(!constructGeometricDistribution(I(0)), "invalid Geometric constructor rejected");
    CHECK(!constructPoissonDistribution(I(0)), "invalid Poisson constructor rejected");
    CHECK(!constructDiscreteUniformDistribution(4, 2), "invalid discrete uniform constructor rejected");
    CHECK(!constructContinuousUniformDistribution(I(2), I(2)), "invalid continuous uniform constructor rejected");
    CHECK(!constructNormalDistribution(I(0), I(0)), "invalid normal constructor rejected");
    CHECK(!constructExponentialDistribution(I(0)), "invalid exponential constructor rejected");
    CHECK(!constructCustomDiscreteDistribution(values, probabilities, 0, true), "invalid custom discrete constructor rejected");
    CHECK(!constructCustomContinuousDistribution(I(0), I(1), NULL, customCDFCallback, customMeanCallback, customVarianceCallback, customSampleCallback), "invalid custom continuous constructor rejected");

    freeProbabilityDistribution(NULL);
    CHECK(true, "freeProbabilityDistribution accepts NULL");
}

static void testRandomVariableAccessors(void) {
    SECTION("RandomVariable construction and accessors");

    ProbabilityDistribution* bern = constructBernoulliDistribution(F(1, 3));
    RandomVariable* x = constructRandomVariable("X", bern, false);
    CHECK(x && x->distribution == bern, "construct RandomVariable");
    CHECK(x && !x->ownsDistribution, "RandomVariable can borrow distribution");
    CHECK(x && strcmp(x->name, "X") == 0, "RandomVariable stores copied name");

    char mutableName[] = "Y";
    RandomVariable* y = constructRandomVariable(mutableName, bern, false);
    mutableName[0] = 'Z';
    CHECK(y && strcmp(y->name, "Y") == 0, "RandomVariable name is source-independent");

    expectFraction("RandomVariable PMF accessor", rvPMF(x, I(1)), 1, 3);
    expectFraction("RandomVariable CDF accessor", rvCDF(x, I(0)), 2, 3);
    expectFraction("RandomVariable expected value accessor", rvExpectedValue(x), 1, 3);
    expectFraction("RandomVariable variance accessor", rvVariance(x), 2, 9);
    expectReal("RandomVariable standard deviation accessor", rvStddev(x), sqrtl(2.0L / 9.0L));
    expectNan("RandomVariable PDF accessor unavailable for discrete distribution", rvPDF(x, I(0)));

    ProbabilityDistribution* normalDist = constructNormalDistribution(I(0), I(1));
    RandomVariable* normalRv = constructRandomVariable("N", normalDist, false);
    CHECK(normalRv && normalRv->distribution, "construct continuous RandomVariable");
    expectReal("RandomVariable PDF accessor", rvPDF(normalRv, I(0)), 1.0L / sqrtl(2.0L * M_PI));
    expectReal("RandomVariable CDF accessor for continuous distribution", rvCDF(normalRv, I(0)), 0.5L);
    expectNan("RandomVariable PMF accessor unavailable for continuous distribution", rvPMF(normalRv, I(0)));

    expectNan("null RandomVariable PMF returns NAN", rvPMF(NULL, I(0)));
    expectNan("null RandomVariable PDF returns NAN", rvPDF(NULL, I(0)));
    expectNan("null RandomVariable CDF returns NAN", rvCDF(NULL, I(0)));
    expectNan("null RandomVariable expected value returns NAN", rvExpectedValue(NULL));
    expectNan("null RandomVariable variance returns NAN", rvVariance(NULL));
    expectNan("null RandomVariable standard deviation returns NAN", rvStddev(NULL));
    expectNan("null RandomVariable sample returns NAN", rvSample(NULL));

    CHECK(!constructRandomVariable(NULL, bern, false), "RandomVariable constructor rejects null name");
    CHECK(!constructRandomVariable("bad", NULL, false), "RandomVariable constructor rejects null distribution");

    freeProbabilityDistribution(normalDist);
    freeRandomVariable(normalRv);
    freeRandomVariable(y);
    freeRandomVariable(x);
    expectFraction("borrowed distribution survives RandomVariable free", probabilityMean(bern), 1, 3);
    freeProbabilityDistribution(bern);

    freeRandomVariable(NULL);
    CHECK(true, "freeRandomVariable accepts NULL");
}

static void testProbabilityDistributionSchemes(void) {
    SECTION("ProbabilityDistribution schemes");

    Number sourceValues[] = {I(1), I(2)};
    Number sourceProbabilities[] = {F(1, 4), F(3, 4)};
    ProbabilityDistribution* borrowed = constructCustomDiscreteDistribution(sourceValues, sourceProbabilities, 2, false);
    ProbabilityDistribution* copied = copyProbabilityDistribution(borrowed);
    sourceValues[1] = I(10);
    CHECK(copied && copied->kind == KUMA_DIST_CUSTOM, "copy custom discrete distribution");
    expectFraction("copied distribution is source-independent", probabilityPMF(copied, I(2)), 3, 4);
    freeProbabilityDistribution(copied);
    freeProbabilityDistribution(borrowed);

    ProbabilityDistribution* bern = constructBernoulliDistribution(F(1, 2));
    ProbabilityDistribution* affine = affineDistribution(bern, I(2), I(3));
    freeProbabilityDistribution(bern);
    CHECK(affine && affine->kind == KUMA_DIST_AFFINE, "construct affine distribution");
    expectFraction("affine distribution PMF at lower image", probabilityPMF(affine, I(3)), 1, 2);
    expectFraction("affine distribution PMF at upper image", probabilityPMF(affine, I(5)), 1, 2);
    expectFraction("affine distribution CDF between images", probabilityCDF(affine, I(4)), 1, 2);
    expectFraction("affine distribution mean", probabilityMean(affine), 4, 1);
    freeProbabilityDistribution(affine);

    ProbabilityDistribution* bernA = constructBernoulliDistribution(F(1, 2));
    ProbabilityDistribution* bernB = constructBernoulliDistribution(F(1, 2));
    ProbabilityDistribution* sumDist = sumIndependentDistributions(bernA, bernB);
    ProbabilityDistribution* productDist = productIndependentDistributions(bernA, bernB);
    freeProbabilityDistribution(bernA);
    freeProbabilityDistribution(bernB);
    CHECK(sumDist && sumDist->kind == KUMA_DIST_SUM_INDEPENDENT, "construct finite discrete sum distribution");
    expectFraction("sum distribution PMF at one", probabilityPMF(sumDist, I(1)), 1, 2);
    expectFraction("sum distribution CDF at one", probabilityCDF(sumDist, I(1)), 3, 4);
    CHECK(productDist && productDist->kind == KUMA_DIST_PRODUCT_INDEPENDENT, "construct finite discrete product distribution");
    expectFraction("product distribution PMF at zero", probabilityPMF(productDist, I(0)), 3, 4);
    expectFraction("product distribution CDF at zero", probabilityCDF(productDist, I(0)), 3, 4);
    freeProbabilityDistribution(sumDist);
    freeProbabilityDistribution(productDist);

    ProbabilityDistribution* expDist = constructExponentialDistribution(I(2));
    ProbabilityDistribution* shiftedExp = affineDistribution(expDist, I(1), I(1));
    freeProbabilityDistribution(expDist);
    CHECK(shiftedExp && shiftedExp->kind == KUMA_DIST_AFFINE, "construct shifted continuous affine distribution");
    expectReal("shifted exponential PDF at support start", probabilityPDF(shiftedExp, I(1)), 2.0L);
    expectReal("shifted exponential CDF at support start", probabilityCDF(shiftedExp, I(1)), 0.0L);
    freeProbabilityDistribution(shiftedExp);

    ProbabilityDistribution* uniformA = constructContinuousUniformDistribution(I(0), I(1));
    ProbabilityDistribution* uniformB = constructContinuousUniformDistribution(I(0), I(1));
    ProbabilityDistribution* uniformSum = sumIndependentDistributions(uniformA, uniformB);
    freeProbabilityDistribution(uniformA);
    freeProbabilityDistribution(uniformB);
    CHECK(uniformSum && uniformSum->kind == KUMA_DIST_SUM_INDEPENDENT, "construct bounded continuous sum distribution");
    expectReal("public uniform sum PDF below midpoint", probabilityPDF(uniformSum, F(1, 2)), 0.5L);
    expectReal("public uniform sum CDF below midpoint", probabilityCDF(uniformSum, F(1, 2)), 0.125L);
    freeProbabilityDistribution(uniformSum);

    ProbabilityDistribution* uniformC = constructContinuousUniformDistribution(I(1), I(2));
    ProbabilityDistribution* uniformD = constructContinuousUniformDistribution(I(1), I(2));
    ProbabilityDistribution* uniformProduct = productIndependentDistributions(uniformC, uniformD);
    freeProbabilityDistribution(uniformC);
    freeProbabilityDistribution(uniformD);
    CHECK(uniformProduct && uniformProduct->kind == KUMA_DIST_PRODUCT_INDEPENDENT, "construct bounded continuous product distribution");
    Number productPdf = uniformProduct ? probabilityPDF(uniformProduct, I(2)) : constructNumberFromDouble(NAN);
    Number productCdf = uniformProduct ? probabilityCDF(uniformProduct, I(2)) : constructNumberFromDouble(NAN);
    CHECK(productPdf.type == NUMBER_REAL && fabsl(productPdf.as.x - logl(2.0L)) < 1e-6L, "public product PDF by NEKO integration");
    CHECK(productCdf.type == NUMBER_REAL && fabsl(productCdf.as.x - (2.0L * logl(2.0L) - 1.0L)) < 1e-6L, "public product CDF by NEKO integration");
    freeProbabilityDistribution(uniformProduct);

    ProbabilityDistribution* poisson = constructPoissonDistribution(I(2));
    ProbabilityDistribution* unsupported = affineDistribution(poisson, I(1), I(1));
    CHECK(!unsupported, "unsupported infinite discrete affine distribution returns NULL");
    freeProbabilityDistribution(poisson);
}

static void testRandomVariableTransformations(void) {
    SECTION("RandomVariable transformations");

    ProbabilityDistribution* bernDist = constructBernoulliDistribution(F(1, 2));
    RandomVariable* x = constructRandomVariable("X", bernDist, false);
    RandomVariable* scaled = rvScale(x, I(2));
    CHECK(scaled && scaled->ownsDistribution, "scaled RandomVariable owns generated distribution");
    expectFraction("scaled Bernoulli PMF at zero", rvPMF(scaled, I(0)), 1, 2);
    expectFraction("scaled Bernoulli PMF at two", rvPMF(scaled, I(2)), 1, 2);
    expectInt("scaled Bernoulli PMF off support", rvPMF(scaled, I(1)), 0);

    RandomVariable* shifted = rvShift(x, I(3));
    CHECK(shifted && shifted->ownsDistribution, "shifted RandomVariable owns generated distribution");
    expectFraction("shifted Bernoulli PMF at three", rvPMF(shifted, I(3)), 1, 2);
    expectFraction("shifted Bernoulli PMF at four", rvPMF(shifted, I(4)), 1, 2);

    RandomVariable* sumRv = rvSumIndependent(x, x);
    CHECK(sumRv && sumRv->ownsDistribution, "finite discrete sum is supported");
    expectFraction("Bernoulli sum PMF at zero", rvPMF(sumRv, I(0)), 1, 4);
    expectFraction("Bernoulli sum PMF at one", rvPMF(sumRv, I(1)), 1, 2);
    expectFraction("Bernoulli sum PMF at two", rvPMF(sumRv, I(2)), 1, 4);

    RandomVariable* productRv = rvProductIndependent(x, x);
    CHECK(productRv && productRv->ownsDistribution, "finite discrete product is supported");
    expectFraction("Bernoulli product PMF at zero", rvPMF(productRv, I(0)), 3, 4);
    expectFraction("Bernoulli product PMF at one", rvPMF(productRv, I(1)), 1, 4);

    ProbabilityDistribution* normalDist = constructNormalDistribution(I(1), I(2));
    RandomVariable* n = constructRandomVariable("N", normalDist, false);
    RandomVariable* affineNormal = rvAffine(n, I(-3), I(5));
    CHECK(affineNormal && affineNormal->ownsDistribution, "normal affine transform is supported");
    expectInt("normal affine mean", rvExpectedValue(affineNormal), 2);
    expectReal("normal affine standard deviation", rvStddev(affineNormal), 6.0L);

    ProbabilityDistribution* poisA = constructPoissonDistribution(I(2));
    ProbabilityDistribution* poisB = constructPoissonDistribution(I(3));
    RandomVariable* p = constructRandomVariable("P", poisA, false);
    RandomVariable* q = constructRandomVariable("Q", poisB, false);
    RandomVariable* poisSum = rvSumIndependent(p, q);
    CHECK(poisSum && poisSum->ownsDistribution, "Poisson sum is supported");
    expectInt("Poisson sum mean", rvExpectedValue(poisSum), 5);

    ProbabilityDistribution* uniformA = constructContinuousUniformDistribution(I(0), I(1));
    ProbabilityDistribution* uniformB = constructContinuousUniformDistribution(I(0), I(1));
    RandomVariable* u = constructRandomVariable("U", uniformA, false);
    RandomVariable* v = constructRandomVariable("V", uniformB, false);
    RandomVariable* uniformSum = rvSumIndependent(u, v);
    CHECK(uniformSum && uniformSum->ownsDistribution, "bounded continuous sum is supported");
    expectReal("uniform sum PDF below midpoint", rvPDF(uniformSum, F(1, 2)), 0.5L);
    expectReal("uniform sum CDF below midpoint", rvCDF(uniformSum, F(1, 2)), 0.125L);
    expectReal("uniform sum mean", rvExpectedValue(uniformSum), 1.0L);

    ProbabilityDistribution* uniformC = constructContinuousUniformDistribution(I(1), I(2));
    ProbabilityDistribution* uniformD = constructContinuousUniformDistribution(I(1), I(2));
    RandomVariable* r = constructRandomVariable("R", uniformC, false);
    RandomVariable* s = constructRandomVariable("S", uniformD, false);
    RandomVariable* uniformProduct = rvProductIndependent(r, s);
    CHECK(uniformProduct && uniformProduct->ownsDistribution, "bounded continuous product away from zero is supported");
    Number productPdf = uniformProduct ? rvPDF(uniformProduct, I(2)) : constructNumberFromDouble(NAN);
    Number productCdf = uniformProduct ? rvCDF(uniformProduct, I(2)) : constructNumberFromDouble(NAN);
    CHECK(productPdf.type == NUMBER_REAL && fabsl(productPdf.as.x - logl(2.0L)) < 1e-6L, "uniform product PDF by NEKO integration");
    CHECK(productCdf.type == NUMBER_REAL && fabsl(productCdf.as.x - (2.0L * logl(2.0L) - 1.0L)) < 1e-6L, "uniform product CDF by NEKO integration");

    RandomVariable* unsupported = rvShift(p, I(1));
    CHECK(!unsupported, "unsupported infinite discrete affine transform returns NULL");

    RandomVariable* owned = constructRandomVariable("owned", constructBernoulliDistribution(F(1, 4)), true);
    CHECK(owned && owned->ownsDistribution, "RandomVariable can own distribution");

    freeRandomVariable(owned);
    freeRandomVariable(uniformProduct);
    freeRandomVariable(r);
    freeRandomVariable(s);
    freeProbabilityDistribution(uniformC);
    freeProbabilityDistribution(uniformD);
    freeRandomVariable(uniformSum);
    freeRandomVariable(u);
    freeRandomVariable(v);
    freeProbabilityDistribution(uniformA);
    freeProbabilityDistribution(uniformB);
    freeRandomVariable(poisSum);
    freeRandomVariable(p);
    freeRandomVariable(q);
    freeProbabilityDistribution(poisA);
    freeProbabilityDistribution(poisB);
    freeRandomVariable(affineNormal);
    freeRandomVariable(n);
    freeProbabilityDistribution(normalDist);
    freeRandomVariable(productRv);
    freeRandomVariable(sumRv);
    freeRandomVariable(shifted);
    freeRandomVariable(scaled);
    freeRandomVariable(x);
    freeProbabilityDistribution(bernDist);
}

int main(void) {
    testIsValidStatsNumber();
    testIsValidStatsArray();
    testContainsStatsTypes();
    testFactorial();
    testNcr();
    testNpr();
    testMultinomial();
    testSum();
    testProduct();
    testMean();
    testMedian();
    testMode();
    testMin();
    testMax();
    testRange();
    testVariance();
    testSampleVariance();
    testStddev();
    testSampleStddev();
    testMeanAbsDev();
    testMedianAbsDev();
    testPercentile();
    testQuartile();
    testIqr();
    testGeometricMean();
    testHarmonicMean();
    testFrequencyFunctions();
    testCovariance();
    testSampleCovariance();
    testCorrelation();
    testLinearRegression();
    testBernoulliDistributionFunctions();
    testRemainingDiscreteDistributionFunctions();
    testContinuousDistributionFunctions();
    testCustomDistributionFunctions();
    testProbabilityDistributionConstructors();
    testRandomVariableAccessors();
    testProbabilityDistributionSchemes();
    testRandomVariableTransformations();

    printf("\n========================================\n");
    printf("Results: %d / %d tests passed\n", testsPassed, testsRun);
    printf("========================================\n");
    return testsPassed == testsRun ? 0 : 1;
}
