#include<limits.h>
#include<math.h>
#include<stdio.h>
#include<stdarg.h>
#include<stdlib.h>
#include<string.h>
#include "kuma.h"
#include "neko.h"

/* ---------- Helper methods ---------- */

// Duplicate a string for KUMA-owned names
static char* dupstr(const char* s) {
    // Reject missing input before measuring the string
    if (!s) return NULL;

    // Allocate exactly enough space for the string and terminator
    size_t n = strlen(s);
    char* out = malloc(n + 1);
    if (!out) return NULL;

    // Copy the full byte sequence including the terminator
    memcpy(out, s, n + 1);
    return out;
}

// Limit exact finite support enumeration to a practical table size
#define KUMA_MAX_ENUM_SUPPORT 100000

typedef struct {
    ProbabilityDistribution* x;
    ProbabilityDistribution* y;
    long double target;
    int mode;
} BinaryIntegrandData;

// Multiply two long long values if the result fits
static bool checkedLongLongMul(long long a, long long b, long long* out) {
    // Reject invalid output storage
    if (!out) return false;

    // Use compiler overflow checks when they are available
#if defined(__GNUC__) || defined(__clang__)
    return !__builtin_mul_overflow(a, b, out);
#else
    // Handle zero before division-based overflow checks
    if (a == 0 || b == 0) {
        *out = 0;
        return true;
    }

    // Reject the only negation cases that overflow
    if (a == -1 && b == LLONG_MIN) return false;
    if (b == -1 && a == LLONG_MIN) return false;

    // Check each sign pattern before multiplying
    if (a > 0) {
        if (b > 0 && a > LLONG_MAX / b) return false;
        if (b < 0 && b < LLONG_MIN / a) return false;
    } else {
        if (b > 0 && a < LLONG_MIN / b) return false;
        if (b < 0 && a < LLONG_MAX / b) return false;
    }
    *out = a * b;
    return true;
#endif
}

// Add two long long values if the result fits
static bool checkedLongLongAdd(long long a, long long b, long long* out) {
    // Reject invalid output storage
    if (!out) return false;

    // Use compiler overflow checks when they are available
#if defined(__GNUC__) || defined(__clang__)
    return !__builtin_add_overflow(a, b, out);
#else
    // Check both overflow directions before adding
    if ((b > 0 && a > LLONG_MAX - b) || (b < 0 && a < LLONG_MIN - b)) return false;
    *out = a + b;
    return true;
#endif
}

// Compute the greatest common divisor of two unsigned long long values
static unsigned long long gcdUnsignedLongLong(unsigned long long a, unsigned long long b) {
    // Apply the Euclidean algorithm until the remainder vanishes
    while (b) {
        unsigned long long t = b;
        b = a % b;
        a = t;
    }
    return a ? a : 1ULL;
}

// Return the shared KUMA invalid numeric result
static Number kumaNan(void) {
    // Use the HEBI constructor so the sentinel stays consistent
    return constructNumberFromDouble(NAN);
}

// Convert a valid KUMA Number to a finite long double
static bool numberToLongDouble(Number x, long double* out) {
    // Reject invalid output storage and invalid KUMA numbers
    if (!out || !isValidStatsNumber(x)) return false;

    // Convert according to the active Number variant
    switch (x.type) {
        case NUMBER_INT:
            *out = (long double)x.as.i;
            return true;
        case NUMBER_FRACTION:
            *out = (long double)x.as.frac.num / (long double)x.as.frac.denom;
            return isfinite(*out);
        case NUMBER_REAL:
            *out = x.as.x;
            return isfinite(*out);
        case NUMBER_COMPLEX:
        case NUMBER_NAN:
            return false;
    }
    return false;
}

// Return true if a Number is a probability value
static bool isProbabilityNumber(Number p) {
    // Validate the Number before comparing against the probability interval
    if (!isValidStatsNumber(p)) return false;
    return compNumbers(p, constructNumberFromInt(0)) >= 0
        && compNumbers(p, constructNumberFromInt(1)) <= 0;
}

// Return true if a Number is strictly positive
static bool isPositiveNumber(Number x) {
    // Validate the Number before comparing against zero
    if (!isValidStatsNumber(x)) return false;
    return compNumbers(x, constructNumberFromInt(0)) > 0;
}

// Decode an integer-valued Number as a long long
static bool numberToLongLongExact(Number x, long long* out) {
    // Reject invalid output storage and invalid KUMA numbers
    if (!out || !isValidStatsNumber(x)) return false;

    // Accept exact integer storage directly
    if (x.type == NUMBER_INT) {
        *out = x.as.i;
        return true;
    }

    // Accept fractions whose denominator divides the numerator
    if (x.type == NUMBER_FRACTION) {
        if (x.as.frac.denom == 0 || x.as.frac.num % x.as.frac.denom != 0) return false;
        *out = x.as.frac.num / x.as.frac.denom;
        return true;
    }

    // Accept finite real values that are exactly integral at KUMA tolerance
    if (x.type == NUMBER_REAL) {
        long double rounded = roundl(x.as.x);
        if (fabsl(x.as.x - rounded) > 1e-12L) return false;
        if (rounded < (long double)LLONG_MIN || rounded > (long double)LLONG_MAX) return false;
        *out = (long long)rounded;
        return true;
    }

    return false;
}

// Compute a nonnegative integer power of a Number
static Number powNumberNonnegative(Number base, long long exp) {
    // Reject invalid bases and exponents
    if (!isValidStatsNumber(base) || exp < 0) return kumaNan();

    // Multiply exp copies of the base using HEBI arithmetic
    Number result = constructNumberFromInt(1);
    for (long long i = 0; i < exp; i++) {
        result = multNumbers(result, base);
        if (result.type == NUMBER_NAN) return result;
    }
    return result;
}

// Convert a nonnegative integral Number to a discrete support index
static bool supportIndex(Number x, long long* out) {
    // Decode an exact integer and require nonnegative support
    if (!numberToLongLongExact(x, out)) return false;
    return *out >= 0;
}

// Compute the size of an inclusive long long interval
static bool discreteUniformWidth(long long a, long long b, long long* out) {
    // Reject invalid output storage and empty intervals
    if (!out || a > b) return false;

    // Compute the unsigned span and reject spans that do not fit in long long
    unsigned long long span = (unsigned long long)b - (unsigned long long)a + 1ULL;
    if (span == 0ULL || span > (unsigned long long)LLONG_MAX) return false;
    *out = (long long)span;
    return true;
}

// Validate a continuous uniform distribution and unpack real endpoints
static bool getContinuousUniformParams(ProbabilityDistribution* dist, long double* a, long double* b) {
    // Reject distributions with the wrong shape
    if (!dist || dist->type != KUMA_DIST_CONTINUOUS || dist->kind != KUMA_DIST_CONTINUOUS_UNIFORM || !dist->params) return false;

    // Convert endpoints and require a nonempty interval
    ContinuousUniformParams* params = (ContinuousUniformParams*)dist->params;
    if (!numberToLongDouble(params->a, a) || !numberToLongDouble(params->b, b)) return false;
    return *a < *b;
}

// Validate a normal distribution and unpack real parameters
static bool getNormalParams(ProbabilityDistribution* dist, long double* mu, long double* sigma) {
    // Reject distributions with the wrong shape
    if (!dist || dist->type != KUMA_DIST_CONTINUOUS || dist->kind != KUMA_DIST_NORMAL || !dist->params) return false;

    // Convert parameters and require positive standard deviation
    NormalParams* params = (NormalParams*)dist->params;
    if (!numberToLongDouble(params->mu, mu) || !numberToLongDouble(params->sigma, sigma)) return false;
    return *sigma > 0.0L;
}

// Validate an exponential distribution and unpack its rate
static bool getExponentialParams(ProbabilityDistribution* dist, long double* lambda) {
    // Reject distributions with the wrong shape
    if (!dist || dist->type != KUMA_DIST_CONTINUOUS || dist->kind != KUMA_DIST_EXPONENTIAL || !dist->params) return false;

    // Convert the rate and require positivity
    ExponentialParams* params = (ExponentialParams*)dist->params;
    if (!numberToLongDouble(params->lambda, lambda)) return false;
    return *lambda > 0.0L;
}

// Format an owned random-variable name
static char* formatName(const char* fmt, ...) {
    // Measure the formatted output
    va_list args;
    va_start(args, fmt);
    int needed = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    if (needed < 0) return NULL;

    // Allocate and write the formatted name
    char* out = malloc((size_t)needed + 1);
    if (!out) return NULL;
    va_start(args, fmt);
    vsnprintf(out, (size_t)needed + 1, fmt, args);
    va_end(args);
    return out;
}

// Return the name of a random variable or a fallback
static const char* rvName(RandomVariable* rv, const char* fallback) {
    // Prefer a nonempty stored name
    if (rv && rv->name && rv->name[0]) return rv->name;
    return fallback;
}

// Return true if a Number is exactly zero
static bool isZeroNumber(Number x) {
    // Compare valid KUMA numbers against zero
    return isValidStatsNumber(x) && eqNumbers(x, constructNumberFromInt(0));
}

// Return true if a Number is exactly one
static bool isOneNumber(Number x) {
    // Compare valid KUMA numbers against one
    return isValidStatsNumber(x) && eqNumbers(x, constructNumberFromInt(1));
}

// Free paired Number arrays
static void freeNumberArrays(Number* values, Number* probabilities) {
    // Release both optional buffers
    free(values);
    free(probabilities);
}

// Build a custom discrete distribution that takes ownership of its arrays
static ProbabilityDistribution* constructOwnedCustomDiscrete(Number* values, Number* probabilities, size_t size) {
    // Reject invalid arrays before taking ownership
    if (!isValidStatsArray(values, size) || !isValidProbabilityArray(probabilities, size)) {
        freeNumberArrays(values, probabilities);
        return NULL;
    }

    // Allocate custom discrete parameters around the owned arrays
    CustomDiscreteParams* params = calloc(1, sizeof(CustomDiscreteParams));
    if (!params) {
        freeNumberArrays(values, probabilities);
        return NULL;
    }
    params->values = values;
    params->probabilities = probabilities;
    params->size = size;
    params->ownsArrays = true;

    // Attach the custom discrete callback table
    return constructProbabilityDistribution(KUMA_DIST_DISCRETE, KUMA_DIST_CUSTOM, params, true,
                                            customDiscretePMF, NULL, customDiscreteCDF,
                                            customDiscreteMean, customDiscreteVariance, customDiscreteStddev,
                                            customDiscreteSample);
}

// Add a probability mass to a finite support table
static bool addMergedMass(Number* values, Number* probabilities, size_t* count, Number value, Number probability) {
    // Merge into an existing support value when possible
    if (!values || !probabilities || !count) return false;
    for (size_t i = 0; i < *count; i++) {
        if (eqNumbers(values[i], value)) {
            probabilities[i] = addNumbers(probabilities[i], probability);
            return probabilities[i].type != NUMBER_NAN;
        }
    }

    // Append a new support value
    values[*count] = value;
    probabilities[*count] = probability;
    (*count)++;
    return true;
}

// Extract the finite support table of a discrete distribution
static bool finiteDiscreteSupport(ProbabilityDistribution* dist, Number** outValues, Number** outProbabilities, size_t* outSize) {
    // Reject unsupported storage and non-discrete distributions
    if (!dist || !outValues || !outProbabilities || !outSize || dist->type != KUMA_DIST_DISCRETE) return false;
    *outValues = NULL;
    *outProbabilities = NULL;
    *outSize = 0;

    // Determine the support size for each finite named distribution
    size_t size = 0;
    long long start = 0;
    if (dist->kind == KUMA_DIST_BERNOULLI) {
        size = 2;
        start = 0;
    } else if (dist->kind == KUMA_DIST_BINOMIAL) {
        if (!dist->params) return false;
        BinomialParams* params = (BinomialParams*)dist->params;
        if (params->n < 0 || (unsigned long long)params->n + 1ULL > KUMA_MAX_ENUM_SUPPORT) return false;
        size = (size_t)params->n + 1;
        start = 0;
    } else if (dist->kind == KUMA_DIST_DISCRETE_UNIFORM) {
        if (!dist->params) return false;
        DiscreteUniformParams* params = (DiscreteUniformParams*)dist->params;
        long long width = 0;
        if (!discreteUniformWidth(params->a, params->b, &width) || (unsigned long long)width > KUMA_MAX_ENUM_SUPPORT) return false;
        size = (size_t)width;
        start = params->a;
    } else if (dist->kind == KUMA_DIST_CUSTOM) {
        if (!dist->params) return false;
        CustomDiscreteParams* params = (CustomDiscreteParams*)dist->params;
        if (!isValidStatsArray(params->values, params->size) || !isValidProbabilityArray(params->probabilities, params->size)) return false;
        if (params->size > KUMA_MAX_ENUM_SUPPORT) return false;
        size = params->size;
    } else if (dist->kind == KUMA_DIST_AFFINE) {
        // Transform and merge finite support from the copied base distribution
        AffineDistributionParams* params = (AffineDistributionParams*)dist->params;
        if (!params || !params->base) return false;
        Number* baseValues = NULL;
        Number* baseProbabilities = NULL;
        size_t baseSize = 0;
        if (!finiteDiscreteSupport(params->base, &baseValues, &baseProbabilities, &baseSize)) return false;
        Number* values = calloc(baseSize, sizeof(Number));
        Number* probabilities = calloc(baseSize, sizeof(Number));
        if (!values || !probabilities) {
            freeNumberArrays(baseValues, baseProbabilities);
            freeNumberArrays(values, probabilities);
            return false;
        }
        size_t count = 0;
        for (size_t i = 0; i < baseSize; i++) {
            Number value = addNumbers(multNumbers(params->scalar, baseValues[i]), params->shift);
            if (value.type == NUMBER_NAN || !addMergedMass(values, probabilities, &count, value, baseProbabilities[i])) {
                freeNumberArrays(baseValues, baseProbabilities);
                freeNumberArrays(values, probabilities);
                return false;
            }
        }
        freeNumberArrays(baseValues, baseProbabilities);
        *outValues = values;
        *outProbabilities = probabilities;
        *outSize = count;
        return true;
    } else if (dist->kind == KUMA_DIST_SUM_INDEPENDENT || dist->kind == KUMA_DIST_PRODUCT_INDEPENDENT) {
        // Enumerate and merge finite support from both copied source distributions
        BinaryDistributionParams* params = (BinaryDistributionParams*)dist->params;
        if (!params || !params->x || !params->y) return false;
        Number* xValues = NULL;
        Number* xProbabilities = NULL;
        Number* yValues = NULL;
        Number* yProbabilities = NULL;
        size_t xSize = 0;
        size_t ySize = 0;
        if (!finiteDiscreteSupport(params->x, &xValues, &xProbabilities, &xSize)) return false;
        if (!finiteDiscreteSupport(params->y, &yValues, &yProbabilities, &ySize)) {
            freeNumberArrays(xValues, xProbabilities);
            return false;
        }
        if (xSize != 0 && ySize > KUMA_MAX_ENUM_SUPPORT / xSize) {
            freeNumberArrays(xValues, xProbabilities);
            freeNumberArrays(yValues, yProbabilities);
            return false;
        }
        size_t maxSize = xSize * ySize;
        Number* values = calloc(maxSize, sizeof(Number));
        Number* probabilities = calloc(maxSize, sizeof(Number));
        if (!values || !probabilities) {
            freeNumberArrays(xValues, xProbabilities);
            freeNumberArrays(yValues, yProbabilities);
            freeNumberArrays(values, probabilities);
            return false;
        }
        size_t count = 0;
        for (size_t i = 0; i < xSize; i++) {
            for (size_t j = 0; j < ySize; j++) {
                Number value = dist->kind == KUMA_DIST_PRODUCT_INDEPENDENT
                    ? multNumbers(xValues[i], yValues[j])
                    : addNumbers(xValues[i], yValues[j]);
                Number probability = multNumbers(xProbabilities[i], yProbabilities[j]);
                if (value.type == NUMBER_NAN || probability.type == NUMBER_NAN
                    || !addMergedMass(values, probabilities, &count, value, probability)) {
                    freeNumberArrays(xValues, xProbabilities);
                    freeNumberArrays(yValues, yProbabilities);
                    freeNumberArrays(values, probabilities);
                    return false;
                }
            }
        }
        freeNumberArrays(xValues, xProbabilities);
        freeNumberArrays(yValues, yProbabilities);
        *outValues = values;
        *outProbabilities = probabilities;
        *outSize = count;
        return true;
    } else {
        return false;
    }

    // Allocate copied support and mass arrays
    Number* values = calloc(size, sizeof(Number));
    Number* probabilities = calloc(size, sizeof(Number));
    if (!values || !probabilities) {
        freeNumberArrays(values, probabilities);
        return false;
    }

    // Copy custom support directly
    if (dist->kind == KUMA_DIST_CUSTOM) {
        CustomDiscreteParams* params = (CustomDiscreteParams*)dist->params;
        for (size_t i = 0; i < size; i++) {
            values[i] = params->values[i];
            probabilities[i] = params->probabilities[i];
        }
    } else {
        for (size_t i = 0; i < size; i++) {
            values[i] = constructNumberFromInt(start + (long long)i);
            probabilities[i] = probabilityPMF(dist, values[i]);
            if (probabilities[i].type == NUMBER_NAN) {
                freeNumberArrays(values, probabilities);
                return false;
            }
        }
    }

    // Return the copied support table
    *outValues = values;
    *outProbabilities = probabilities;
    *outSize = size;
    return true;
}

// Return finite support bounds for a continuous distribution
static bool continuousSupport(ProbabilityDistribution* dist, long double* lower, long double* upper) {
    // Reject unsupported storage and non-continuous distributions
    if (!dist || !lower || !upper || dist->type != KUMA_DIST_CONTINUOUS || !dist->params) return false;

    // Use the named continuous uniform support
    if (dist->kind == KUMA_DIST_CONTINUOUS_UNIFORM) {
        return getContinuousUniformParams(dist, lower, upper);
    }

    // Custom continuous params begin with lower and upper support endpoints
    if (dist->kind == KUMA_DIST_CUSTOM) {
        CustomContinuousParams* params = (CustomContinuousParams*)dist->params;
        if (!numberToLongDouble(params->lower, lower) || !numberToLongDouble(params->upper, upper)) return false;
        return *lower < *upper && isfinite(*lower) && isfinite(*upper);
    }

    // Distribution scheme params begin with lower and upper support endpoints
    if (dist->kind == KUMA_DIST_AFFINE || dist->kind == KUMA_DIST_SUM_INDEPENDENT || dist->kind == KUMA_DIST_PRODUCT_INDEPENDENT) {
        CustomContinuousParams* params = (CustomContinuousParams*)dist->params;
        if (!numberToLongDouble(params->lower, lower) || !numberToLongDouble(params->upper, upper)) return false;
        return *lower < *upper && isfinite(*lower) && isfinite(*upper);
    }

    return false;
}

// Construct a one-point discrete distribution
static ProbabilityDistribution* constructDegenerateDistribution(Number value) {
    // Allocate the single support value and mass
    Number* values = calloc(1, sizeof(Number));
    Number* probabilities = calloc(1, sizeof(Number));
    if (!values || !probabilities) {
        freeNumberArrays(values, probabilities);
        return NULL;
    }

    // Store a point mass at the requested value
    values[0] = value;
    probabilities[0] = constructNumberFromInt(1);
    return constructOwnedCustomDiscrete(values, probabilities, 1);
}

// Compare two Numbers for sorted descriptive statistics
static int compareStatsNumbers(const void* a, const void* b) {
    // Delegate numeric ordering to HEBI and squash to qsort's int convention
    long long cmp = compNumbers(*(const Number*)a, *(const Number*)b);
    return (cmp > 0) - (cmp < 0);
}

// Copy and sort a valid stats array
static Number* sortedNumberCopy(Number* data, size_t size) {
    // Reject invalid arrays before allocating storage
    if (!isValidStatsArray(data, size)) return NULL;

    // Copy input values so callers are never mutated
    Number* sorted = malloc(size * sizeof(Number));
    if (!sorted) return NULL;
    for (size_t i = 0; i < size; i++) sorted[i] = data[i];
    qsort(sorted, size, sizeof(Number), compareStatsNumbers);
    return sorted;
}

// Convert a valid stats array to long double coordinates
static long double* numberArrayToLongDouble(Number* data, size_t size) {
    // Reject invalid arrays before allocating storage
    if (!isValidStatsArray(data, size)) return NULL;

    // Convert each value through the shared Number conversion helper
    long double* values = malloc(size * sizeof(long double));
    if (!values) return NULL;
    for (size_t i = 0; i < size; i++) {
        if (!numberToLongDouble(data[i], &values[i])) {
            free(values);
            return NULL;
        }
    }
    return values;
}

// Sort a long double array in ascending order
static int compareLongDoubleValues(const void* a, const void* b) {
    // Compare finite plotting coordinates directly
    long double x = *(const long double*)a;
    long double y = *(const long double*)b;
    return (x > y) - (x < y);
}

// Compute a p-quantile of sorted long double data by linear interpolation
static long double sortedLongDoubleQuantile(long double* sorted, size_t size, long double p) {
    // Clamp percentile positions to the available sample range
    if (!sorted || size == 0) return NAN;
    if (p <= 0.0L) return sorted[0];
    if (p >= 1.0L) return sorted[size - 1];

    // Interpolate between adjacent order statistics
    long double pos = p * (long double)(size - 1);
    size_t lower = (size_t)floorl(pos);
    if (lower >= size - 1) return sorted[size - 1];
    long double weight = pos - (long double)lower;
    return sorted[lower] + weight * (sorted[lower + 1] - sorted[lower]);
}

// Return the sample standard deviation of long double data
static long double longDoubleSampleStddev(long double* values, size_t size) {
    // Reject missing or singleton samples
    if (!values || size < 2) return NAN;

    // Compute the sample mean
    long double total = 0.0L;
    for (size_t i = 0; i < size; i++) total += values[i];
    long double avg = total / (long double)size;

    // Compute the unbiased sample variance
    long double ss = 0.0L;
    for (size_t i = 0; i < size; i++) {
        long double diff = values[i] - avg;
        ss += diff * diff;
    }
    return sqrtl(ss / (long double)(size - 1));
}

// Evaluate a distribution CDF as a long double
static bool distributionCDFLongDouble(ProbabilityDistribution* dist, long double x, long double* out) {
    // Reject invalid storage and unavailable distribution CDFs
    if (!dist || !out || !distributionHasCDF(dist)) return false;

    // Call the distribution accessor and convert the result
    Number result = probabilityCDF(dist, constructNumberFromDouble(x));
    if (!numberToLongDouble(result, out)) return false;
    return isfinite(*out);
}

// Compute a discrete distribution quantile from finite support
static bool discreteDistributionQuantile(ProbabilityDistribution* dist, long double p, long double* out) {
    // Reject invalid output storage and unsupported probabilities
    if (!dist || !out || p < 0.0L || p > 1.0L) return false;

    // Enumerate finite support values when KUMA can do so exactly
    Number* values = NULL;
    Number* probabilities = NULL;
    size_t size = 0;
    if (!finiteDiscreteSupport(dist, &values, &probabilities, &size)) return false;

    // Search for the smallest support value whose CDF reaches p
    bool found = false;
    long double best = 0.0L;
    for (size_t i = 0; i < size; i++) {
        long double x = 0.0L;
        long double cdf = 0.0L;
        if (!numberToLongDouble(values[i], &x) || !distributionCDFLongDouble(dist, x, &cdf)) {
            freeNumberArrays(values, probabilities);
            return false;
        }
        if (cdf + 1e-15L >= p && (!found || x < best)) {
            best = x;
            found = true;
        }
    }

    freeNumberArrays(values, probabilities);
    if (!found) return false;
    *out = best;
    return true;
}

// Find a finite bracket for a continuous distribution quantile
static bool continuousQuantileBracket(ProbabilityDistribution* dist, long double p, long double* lower, long double* upper) {
    // Prefer finite known support when the distribution has it
    if (continuousSupport(dist, lower, upper)) return true;

    // Use named infinite-support distributions when available
    long double mu = 0.0L;
    long double sigma = 0.0L;
    if (getNormalParams(dist, &mu, &sigma)) {
        *lower = mu - 10.0L * sigma;
        *upper = mu + 10.0L * sigma;
        return true;
    }

    long double lambda = 0.0L;
    if (getExponentialParams(dist, &lambda)) {
        *lower = 0.0L;
        *upper = p >= 1.0L ? 64.0L / lambda : -logl(fmaxl(1e-18L, 1.0L - p)) / lambda;
        if (*upper <= *lower) *upper = 1.0L / lambda;
        return true;
    }

    // Fall back to mean and standard deviation accessors
    Number meanValue = probabilityMean(dist);
    Number sdValue = probabilityStddev(dist);
    if (!numberToLongDouble(meanValue, &mu) || !numberToLongDouble(sdValue, &sigma) || sigma <= 0.0L) return false;
    *lower = mu - 10.0L * sigma;
    *upper = mu + 10.0L * sigma;
    return isfinite(*lower) && isfinite(*upper) && *lower < *upper;
}

// Compute a distribution quantile for QQ plot data
static bool distributionQuantile(ProbabilityDistribution* dist, long double p, long double* out) {
    // Reject invalid inputs and unsupported distributions
    if (!dist || !out || p <= 0.0L || p >= 1.0L || !distributionHasCDF(dist)) return false;

    // Use finite discrete support directly
    if (dist->type == KUMA_DIST_DISCRETE) return discreteDistributionQuantile(dist, p, out);

    // Build a bracket for bisection
    long double lower = 0.0L;
    long double upper = 0.0L;
    if (!continuousQuantileBracket(dist, p, &lower, &upper)) return false;

    // Expand the bracket until it contains the target probability
    long double cLower = 0.0L;
    long double cUpper = 0.0L;
    if (!distributionCDFLongDouble(dist, lower, &cLower) || !distributionCDFLongDouble(dist, upper, &cUpper)) return false;
    long double span = upper - lower;
    for (int i = 0; i < 64 && cLower > p; i++) {
        upper = lower;
        lower -= span;
        span *= 2.0L;
        if (!distributionCDFLongDouble(dist, lower, &cLower)) return false;
    }
    span = upper - lower;
    for (int i = 0; i < 64 && cUpper < p; i++) {
        lower = upper;
        upper += span;
        span *= 2.0L;
        if (!distributionCDFLongDouble(dist, upper, &cUpper)) return false;
    }
    if (cLower > p || cUpper < p) return false;

    // Bisect to a stable plotting quantile
    for (int i = 0; i < 100; i++) {
        long double mid = (lower + upper) / 2.0L;
        long double cMid = 0.0L;
        if (!distributionCDFLongDouble(dist, mid, &cMid)) return false;
        if (cMid < p) lower = mid;
        else upper = mid;
    }
    *out = (lower + upper) / 2.0L;
    return isfinite(*out);
}

// Returns true if a Number can be used for KUMA methods, else false
bool isValidStatsNumber(Number x) {
    // Check validity using the active Number variant
    switch (x.type) {
        case NUMBER_INT:
            return true;
        case NUMBER_FRACTION:
            return x.as.frac.denom != 0;
        case NUMBER_REAL:
            return isfinite(x.as.x);
        case NUMBER_COMPLEX:
        case NUMBER_NAN:
            return false;
    }
    return false;
}

// Returns true if an array of Numbers can be used for KUMA methods, else false
bool isValidStatsArray(Number* data, size_t size) {
    // Reject missing or empty arrays
    if (!data || size < 1) return false;

    // Reject the first invalid array element
    for (size_t i = 0; i < size; i++) {
        if (!isValidStatsNumber(data[i])) return false;
    }

    return true;
}

// Returns true if a valid stats array contains a Fraction value
bool containsFraction(Number* data, size_t size) {
    // Reject missing, empty, or invalid arrays
    if (!isValidStatsArray(data, size)) return false;

    // Search for the first Fraction value
    for (size_t i = 0; i < size; i++) {
        switch (data[i].type) {
            case NUMBER_INT:
            case NUMBER_REAL:
                break;
            case NUMBER_FRACTION:
                return true;
            case NUMBER_COMPLEX:
            case NUMBER_NAN:
                return false;
        }
    }

    return false;
}

// Returns true if a valid stats array contains a real value
bool containsReal(Number* data, size_t size) {
    // Reject missing, empty, or invalid arrays
    if (!isValidStatsArray(data, size)) return false;

    // Search for the first real value
    for (size_t i = 0; i < size; i++) {
        switch (data[i].type) {
            case NUMBER_INT:
            case NUMBER_FRACTION:
                break;
            case NUMBER_REAL:
                return true;
            case NUMBER_COMPLEX:
            case NUMBER_NAN:
                return false;
        }
    }

    return false;
}

/* ---------- Basic combinatorics ---------- */

// Compute n! for nonnegative n
long long factorial(long long n) {
    // Reject values outside the combinatorial domain
    if (n < 0) return 0;

    // Accumulate the product with overflow checks
    long long result = 1;
    for (long long i = 2; i <= n; i++) {
        if (!checkedLongLongMul(result, i, &result)) return 0;
    }

    return result;
}

// Compute the binomial coefficient n choose r
long long ncr(long long n, long long r) {
    // Reject values outside the combinatorial domain
    if (n < 0 || r < 0 || r > n) return 0;

    // Use symmetry to minimize the number of multiplication steps
    if (r > n - r) r = n - r;

    // Build the reduced product one exact factor at a time
    long long result = 1;
    for (long long i = 1; i <= r; i++) {
        long long numerator = n - r + i;
        long long denominator = i;

        // Cancel the new numerator against the new denominator
        unsigned long long g = gcdUnsignedLongLong((unsigned long long)numerator, (unsigned long long)denominator);
        numerator /= (long long)g;
        denominator /= (long long)g;

        // Cancel any remaining denominator against the accumulated result
        g = gcdUnsignedLongLong((unsigned long long)result, (unsigned long long)denominator);
        result /= (long long)g;
        denominator /= (long long)g;

        // Multiply only after the exact division has been discharged
        if (denominator != 1) return 0;
        if (!checkedLongLongMul(result, numerator, &result)) return 0;
    }

    return result;
}

// Compute the number of ordered r-permutations of n objects
long long npr(long long n, long long r) {
    // Reject values outside the combinatorial domain
    if (n < 0 || r < 0 || r > n) return 0;

    // Multiply the r descending factors of n!/(n-r)!
    long long result = 1;
    for (long long i = n - r + 1; i <= n; i++) {
        if (!checkedLongLongMul(result, i, &result)) return 0;
    }

    return result;
}

// Compute the multinomial coefficient for a partition of n
long long multinomial(long long n, long long* parts, size_t k) {
    // Reject invalid partitions before reading part data
    if (n < 0 || (!parts && k > 0)) return 0;

    // Treat the empty partition as valid only for n = 0
    if (k == 0) return n == 0 ? 1 : 0;

    // Multiply successive binomial choices from the remaining pool
    long long remaining = n;
    long long result = 1;
    for (size_t i = 0; i < k; i++) {
        // Reject negative parts and parts larger than the remaining total
        if (parts[i] < 0 || parts[i] > remaining) return 0;

        // Choose the current part and update the running product
        long long coefficient = ncr(remaining, parts[i]);
        if (coefficient == 0) return 0;
        if (!checkedLongLongMul(result, coefficient, &result)) return 0;
        remaining -= parts[i];
    }

    return remaining == 0 ? result : 0;
}

/* ---------- Basic descriptive statistics ---------- */

// Compute the sum of a valid stats array
Number sum(Number* data, size_t size) {
    // Reject arrays containing invalid KUMA stats values
    if (!isValidStatsArray(data, size)) return kumaNan();

    // Detect whether the result must be promoted beyond integer arithmetic
    bool hasFraction = containsFraction(data, size);
    bool hasReal = containsReal(data, size);

    // Sum real-valued data using long double arithmetic
    if (hasReal) {
        long double total = 0.0L;
        for (size_t i = 0; i < size; i++) {
            switch (data[i].type) {
                case NUMBER_INT:
                    total += (long double)data[i].as.i;
                    break;
                case NUMBER_FRACTION:
                    total += (long double)data[i].as.frac.num / (long double)data[i].as.frac.denom;
                    break;
                case NUMBER_REAL:
                    total += data[i].as.x;
                    break;
                case NUMBER_COMPLEX:
                case NUMBER_NAN:
                    return kumaNan();
            }
        }
        return isfinite(total) ? constructNumberFromDouble(total) : kumaNan();
    }

    // Sum exact fractional data using HEBI fraction arithmetic
    if (hasFraction) {
        Number total = constructNumberFromInt(0);
        for (size_t i = 0; i < size; i++) {
            switch (data[i].type) {
                case NUMBER_INT:
                case NUMBER_FRACTION:
                    total = addNumbers(total, data[i]);
                    break;
                case NUMBER_REAL:
                case NUMBER_COMPLEX:
                case NUMBER_NAN:
                    return kumaNan();
            }
            if (total.type == NUMBER_NAN) return total;
        }
        return total;
    }

    // Sum integer data with overflow checks
    long long total = 0;
    for (size_t i = 0; i < size; i++) {
        switch (data[i].type) {
            case NUMBER_INT:
                if (!checkedLongLongAdd(total, data[i].as.i, &total)) return kumaNan();
                break;
            case NUMBER_FRACTION:
            case NUMBER_REAL:
            case NUMBER_COMPLEX:
            case NUMBER_NAN:
                return kumaNan();
        }
    }
    return constructNumberFromInt(total);
}

// Compute the product of a valid stats array
Number product(Number* data, size_t size) {
    // Reject arrays containing invalid KUMA stats values
    if (!isValidStatsArray(data, size)) return kumaNan();

    // Detect whether the result must be promoted beyond integer arithmetic
    bool hasFraction = containsFraction(data, size);
    bool hasReal = containsReal(data, size);

    // Multiply real-valued data using long double arithmetic
    if (hasReal) {
        long double total = 1.0L;
        for (size_t i = 0; i < size; i++) {
            switch (data[i].type) {
                case NUMBER_INT:
                    total *= (long double)data[i].as.i;
                    break;
                case NUMBER_FRACTION:
                    total *= (long double)data[i].as.frac.num / (long double)data[i].as.frac.denom;
                    break;
                case NUMBER_REAL:
                    total *= data[i].as.x;
                    break;
                case NUMBER_COMPLEX:
                case NUMBER_NAN:
                    return kumaNan();
            }
        }
        return isfinite(total) ? constructNumberFromDouble(total) : kumaNan();
    }

    // Multiply exact fractional data using HEBI fraction arithmetic
    if (hasFraction) {
        Number total = constructNumberFromInt(1);
        for (size_t i = 0; i < size; i++) {
            switch (data[i].type) {
                case NUMBER_INT:
                case NUMBER_FRACTION:
                    total = multNumbers(total, data[i]);
                    break;
                case NUMBER_REAL:
                case NUMBER_COMPLEX:
                case NUMBER_NAN:
                    return kumaNan();
            }
            if (total.type == NUMBER_NAN) return total;
        }
        return total;
    }

    // Multiply integer data with overflow checks
    long long total = 1;
    for (size_t i = 0; i < size; i++) {
        switch (data[i].type) {
            case NUMBER_INT:
                if (!checkedLongLongMul(total, data[i].as.i, &total)) return kumaNan();
                break;
            case NUMBER_FRACTION:
            case NUMBER_REAL:
            case NUMBER_COMPLEX:
            case NUMBER_NAN:
                return kumaNan();
        }
    }
    return constructNumberFromInt(total);
}

// Compute the arithmetic mean of a valid stats array
Number mean(Number* data, size_t size) {
    // Reject arrays containing invalid KUMA stats values
    if (!isValidStatsArray(data, size)) return kumaNan();
    if (size > (size_t)LLONG_MAX) return kumaNan();

    // Detect whether the result must be promoted beyond exact integer division
    bool hasFraction = containsFraction(data, size);
    bool hasReal = containsReal(data, size);

    // Average real-valued data using long double arithmetic
    if (hasReal) {
        long double total = 0.0L;
        for (size_t i = 0; i < size; i++) {
            switch (data[i].type) {
                case NUMBER_INT:
                    total += (long double)data[i].as.i;
                    break;
                case NUMBER_FRACTION:
                    total += (long double)data[i].as.frac.num / (long double)data[i].as.frac.denom;
                    break;
                case NUMBER_REAL:
                    total += data[i].as.x;
                    break;
                case NUMBER_COMPLEX:
                case NUMBER_NAN:
                    return kumaNan();
            }
        }
        long double result = total / (long double)size;
        return isfinite(result) ? constructNumberFromDouble(result) : kumaNan();
    }

    // Average exact fractional data using exact Number division
    if (hasFraction) {
        Number total = constructNumberFromInt(0);
        for (size_t i = 0; i < size; i++) {
            switch (data[i].type) {
                case NUMBER_INT:
                case NUMBER_FRACTION:
                    total = addNumbers(total, data[i]);
                    break;
                case NUMBER_REAL:
                case NUMBER_COMPLEX:
                case NUMBER_NAN:
                    return kumaNan();
            }
            if (total.type == NUMBER_NAN) return total;
        }
        return divNumbers(total, constructNumberFromInt((long long)size));
    }

    // Average integer data exactly whenever possible
    long long total = 0;
    for (size_t i = 0; i < size; i++) {
        switch (data[i].type) {
            case NUMBER_INT:
                if (!checkedLongLongAdd(total, data[i].as.i, &total)) return kumaNan();
                break;
            case NUMBER_FRACTION:
            case NUMBER_REAL:
            case NUMBER_COMPLEX:
            case NUMBER_NAN:
                return kumaNan();
        }
    }
    return divNumbers(constructNumberFromInt(total), constructNumberFromInt((long long)size));
}

// Compute the median of a valid stats array
Number median(Number* data, size_t size) {
    // Reject arrays containing invalid KUMA stats values
    if (!isValidStatsArray(data, size)) return kumaNan();

    // Copy and sort the data so the input array is not mutated
    Number* sorted = malloc(size * sizeof(Number));
    if (!sorted) return kumaNan();
    for (size_t i = 0; i < size; i++) sorted[i] = data[i];
    qsort(sorted, size, sizeof(Number), compareStatsNumbers);

    // Detect whether real promotion is needed for averaging middle elements
    bool hasReal = containsReal(data, size);

    // Return the middle value directly for odd-length arrays
    if (size % 2) {
        Number result = sorted[size / 2];
        if (hasReal && result.type != NUMBER_REAL) {
            switch (result.type) {
                case NUMBER_INT:
                    result = constructNumberFromDouble((long double)result.as.i);
                    break;
                case NUMBER_FRACTION:
                    result = constructNumberFromDouble((long double)result.as.frac.num / (long double)result.as.frac.denom);
                    break;
                case NUMBER_REAL:
                case NUMBER_COMPLEX:
                case NUMBER_NAN:
                    break;
            }
        }
        free(sorted);
        return result;
    }

    // Average the two middle values for even-length real arrays
    if (hasReal) {
        long double a = 0.0L;
        long double b = 0.0L;
        switch (sorted[size / 2 - 1].type) {
            case NUMBER_INT:
                a = (long double)sorted[size / 2 - 1].as.i;
                break;
            case NUMBER_FRACTION:
                a = (long double)sorted[size / 2 - 1].as.frac.num / (long double)sorted[size / 2 - 1].as.frac.denom;
                break;
            case NUMBER_REAL:
                a = sorted[size / 2 - 1].as.x;
                break;
            case NUMBER_COMPLEX:
            case NUMBER_NAN:
                free(sorted);
                return kumaNan();
        }
        switch (sorted[size / 2].type) {
            case NUMBER_INT:
                b = (long double)sorted[size / 2].as.i;
                break;
            case NUMBER_FRACTION:
                b = (long double)sorted[size / 2].as.frac.num / (long double)sorted[size / 2].as.frac.denom;
                break;
            case NUMBER_REAL:
                b = sorted[size / 2].as.x;
                break;
            case NUMBER_COMPLEX:
            case NUMBER_NAN:
                free(sorted);
                return kumaNan();
        }
        free(sorted);
        return constructNumberFromDouble((a + b) / 2.0L);
    }

    // Average the two middle values exactly for integer and fractional arrays
    Number total = addNumbers(sorted[size / 2 - 1], sorted[size / 2]);
    free(sorted);
    if (total.type == NUMBER_NAN) return total;
    return divNumbers(total, constructNumberFromInt(2));
}

// Compute the smallest mode of a valid stats array
Number mode(Number* data, size_t size) {
    // Reject arrays containing invalid KUMA stats values
    if (!isValidStatsArray(data, size)) return kumaNan();

    // Copy and sort the data so equal values become adjacent
    Number* sorted = malloc(size * sizeof(Number));
    if (!sorted) return kumaNan();
    for (size_t i = 0; i < size; i++) sorted[i] = data[i];
    qsort(sorted, size, sizeof(Number), compareStatsNumbers);

    // Detect whether real promotion is needed for the result
    bool hasReal = containsReal(data, size);

    // Scan adjacent runs and keep the first value with maximal frequency
    Number best = sorted[0];
    size_t bestCount = 1;
    size_t currentCount = 1;
    for (size_t i = 0; i < size; i++) {
        if (i == 0) continue;
        if (eqNumbers(sorted[i], sorted[i - 1])) {
            currentCount++;
        } else {
            currentCount = 1;
        }
        if (currentCount > bestCount) {
            bestCount = currentCount;
            best = sorted[i];
        }
    }

    // Promote an exact mode to real when the input data contains reals
    if (hasReal && best.type != NUMBER_REAL) {
        switch (best.type) {
            case NUMBER_INT:
                best = constructNumberFromDouble((long double)best.as.i);
                break;
            case NUMBER_FRACTION:
                best = constructNumberFromDouble((long double)best.as.frac.num / (long double)best.as.frac.denom);
                break;
            case NUMBER_REAL:
            case NUMBER_COMPLEX:
            case NUMBER_NAN:
                break;
        }
    }

    free(sorted);
    return best;
}

// Compute the minimum of a valid stats array
Number min(Number* data, size_t size) {
    // Reject arrays containing invalid KUMA stats values
    if (!isValidStatsArray(data, size)) return kumaNan();

    // Detect whether real promotion is needed for the result
    bool hasReal = containsReal(data, size);

    // Scan the data for the smallest value
    Number result = data[0];
    for (size_t i = 0; i < size; i++) {
        if (compNumbers(data[i], result) < 0) result = data[i];
    }

    // Promote an exact minimum to real when the input data contains reals
    if (hasReal && result.type != NUMBER_REAL) {
        switch (result.type) {
            case NUMBER_INT:
                return constructNumberFromDouble((long double)result.as.i);
            case NUMBER_FRACTION:
                return constructNumberFromDouble((long double)result.as.frac.num / (long double)result.as.frac.denom);
            case NUMBER_REAL:
            case NUMBER_COMPLEX:
            case NUMBER_NAN:
                break;
        }
    }
    return result;
}

// Compute the maximum of a valid stats array
Number max(Number* data, size_t size) {
    // Reject arrays containing invalid KUMA stats values
    if (!isValidStatsArray(data, size)) return kumaNan();

    // Detect whether real promotion is needed for the result
    bool hasReal = containsReal(data, size);

    // Scan the data for the largest value
    Number result = data[0];
    for (size_t i = 0; i < size; i++) {
        if (compNumbers(data[i], result) > 0) result = data[i];
    }

    // Promote an exact maximum to real when the input data contains reals
    if (hasReal && result.type != NUMBER_REAL) {
        switch (result.type) {
            case NUMBER_INT:
                return constructNumberFromDouble((long double)result.as.i);
            case NUMBER_FRACTION:
                return constructNumberFromDouble((long double)result.as.frac.num / (long double)result.as.frac.denom);
            case NUMBER_REAL:
            case NUMBER_COMPLEX:
            case NUMBER_NAN:
                break;
        }
    }
    return result;
}

// Compute the range of a valid stats array
Number range(Number* data, size_t size) {
    // Reject arrays containing invalid KUMA stats values
    if (!isValidStatsArray(data, size)) return kumaNan();

    // Compute endpoints through the existing minimum and maximum logic
    Number lo = min(data, size);
    Number hi = max(data, size);
    if (lo.type == NUMBER_NAN || hi.type == NUMBER_NAN) return kumaNan();

    // Subtract endpoints using real arithmetic when either endpoint is real
    if (lo.type == NUMBER_REAL || hi.type == NUMBER_REAL) {
        long double a = 0.0L;
        long double b = 0.0L;
        switch (lo.type) {
            case NUMBER_INT:
                a = (long double)lo.as.i;
                break;
            case NUMBER_FRACTION:
                a = (long double)lo.as.frac.num / (long double)lo.as.frac.denom;
                break;
            case NUMBER_REAL:
                a = lo.as.x;
                break;
            case NUMBER_COMPLEX:
            case NUMBER_NAN:
                return kumaNan();
        }
        switch (hi.type) {
            case NUMBER_INT:
                b = (long double)hi.as.i;
                break;
            case NUMBER_FRACTION:
                b = (long double)hi.as.frac.num / (long double)hi.as.frac.denom;
                break;
            case NUMBER_REAL:
                b = hi.as.x;
                break;
            case NUMBER_COMPLEX:
            case NUMBER_NAN:
                return kumaNan();
        }
        return constructNumberFromDouble(b - a);
    }

    // Subtract endpoints exactly for integer and fractional arrays
    return subNumbers(hi, lo);
}

// Compute the population variance of a valid stats array
Number variance(Number* data, size_t size) {
    // Reject arrays containing invalid KUMA stats values
    if (!isValidStatsArray(data, size)) return kumaNan();
    if (size > (size_t)LLONG_MAX) return kumaNan();

    // Detect whether the variance must be computed with real arithmetic
    bool hasReal = containsReal(data, size);

    // Compute real population variance by averaging squared deviations
    if (hasReal) {
        long double avg = 0.0L;
        for (size_t i = 0; i < size; i++) {
            switch (data[i].type) {
                case NUMBER_INT:
                    avg += (long double)data[i].as.i;
                    break;
                case NUMBER_FRACTION:
                    avg += (long double)data[i].as.frac.num / (long double)data[i].as.frac.denom;
                    break;
                case NUMBER_REAL:
                    avg += data[i].as.x;
                    break;
                case NUMBER_COMPLEX:
                case NUMBER_NAN:
                    return kumaNan();
            }
        }
        avg /= (long double)size;

        long double total = 0.0L;
        for (size_t i = 0; i < size; i++) {
            long double x = 0.0L;
            switch (data[i].type) {
                case NUMBER_INT:
                    x = (long double)data[i].as.i;
                    break;
                case NUMBER_FRACTION:
                    x = (long double)data[i].as.frac.num / (long double)data[i].as.frac.denom;
                    break;
                case NUMBER_REAL:
                    x = data[i].as.x;
                    break;
                case NUMBER_COMPLEX:
                case NUMBER_NAN:
                    return kumaNan();
            }
            long double diff = x - avg;
            total += diff * diff;
        }
        long double result = total / (long double)size;
        return isfinite(result) ? constructNumberFromDouble(result) : kumaNan();
    }

    // Compute exact population variance with HEBI Number arithmetic
    Number avg = mean(data, size);
    if (avg.type == NUMBER_NAN) return avg;
    Number total = constructNumberFromInt(0);
    for (size_t i = 0; i < size; i++) {
        Number diff;
        switch (data[i].type) {
            case NUMBER_INT:
            case NUMBER_FRACTION:
                diff = subNumbers(data[i], avg);
                break;
            case NUMBER_REAL:
            case NUMBER_COMPLEX:
            case NUMBER_NAN:
                return kumaNan();
        }
        Number squared = multNumbers(diff, diff);
        total = addNumbers(total, squared);
        if (total.type == NUMBER_NAN) return total;
    }
    return divNumbers(total, constructNumberFromInt((long long)size));
}

// Compute the sample variance of a valid stats array
Number sampleVariance(Number* data, size_t size) {
    // Reject arrays that cannot support a sample denominator
    if (!isValidStatsArray(data, size) || size < 2) return kumaNan();
    if (size > (size_t)LLONG_MAX) return kumaNan();

    // Detect whether the variance must be computed with real arithmetic
    bool hasReal = containsReal(data, size);

    // Compute real sample variance by averaging squared deviations over n - 1
    if (hasReal) {
        long double avg = 0.0L;
        for (size_t i = 0; i < size; i++) {
            switch (data[i].type) {
                case NUMBER_INT:
                    avg += (long double)data[i].as.i;
                    break;
                case NUMBER_FRACTION:
                    avg += (long double)data[i].as.frac.num / (long double)data[i].as.frac.denom;
                    break;
                case NUMBER_REAL:
                    avg += data[i].as.x;
                    break;
                case NUMBER_COMPLEX:
                case NUMBER_NAN:
                    return kumaNan();
            }
        }
        avg /= (long double)size;

        long double total = 0.0L;
        for (size_t i = 0; i < size; i++) {
            long double x = 0.0L;
            switch (data[i].type) {
                case NUMBER_INT:
                    x = (long double)data[i].as.i;
                    break;
                case NUMBER_FRACTION:
                    x = (long double)data[i].as.frac.num / (long double)data[i].as.frac.denom;
                    break;
                case NUMBER_REAL:
                    x = data[i].as.x;
                    break;
                case NUMBER_COMPLEX:
                case NUMBER_NAN:
                    return kumaNan();
            }
            long double diff = x - avg;
            total += diff * diff;
        }
        long double result = total / (long double)(size - 1);
        return isfinite(result) ? constructNumberFromDouble(result) : kumaNan();
    }

    // Compute exact sample variance with HEBI Number arithmetic
    Number avg = mean(data, size);
    if (avg.type == NUMBER_NAN) return avg;
    Number total = constructNumberFromInt(0);
    for (size_t i = 0; i < size; i++) {
        Number diff;
        switch (data[i].type) {
            case NUMBER_INT:
            case NUMBER_FRACTION:
                diff = subNumbers(data[i], avg);
                break;
            case NUMBER_REAL:
            case NUMBER_COMPLEX:
            case NUMBER_NAN:
                return kumaNan();
        }
        Number squared = multNumbers(diff, diff);
        total = addNumbers(total, squared);
        if (total.type == NUMBER_NAN) return total;
    }
    return divNumbers(total, constructNumberFromInt((long long)size - 1));
}

// Compute the population standard deviation of a valid stats array
Number stddev(Number* data, size_t size) {
    // Reuse population variance and validate the result variant
    Number v = variance(data, size);
    if (v.type == NUMBER_NAN) return v;

    // Convert the variance to a real radicand
    long double x = 0.0L;
    switch (v.type) {
        case NUMBER_INT:
            x = (long double)v.as.i;
            break;
        case NUMBER_FRACTION:
            x = (long double)v.as.frac.num / (long double)v.as.frac.denom;
            break;
        case NUMBER_REAL:
            x = v.as.x;
            break;
        case NUMBER_COMPLEX:
        case NUMBER_NAN:
            return kumaNan();
    }

    // Return the nonnegative real square root
    if (x < 0.0L) return kumaNan();
    return constructNumberFromDouble(sqrtl(x));
}

// Compute the sample standard deviation of a valid stats array
Number sampleStddev(Number* data, size_t size) {
    // Reuse sample variance and validate the result variant
    Number v = sampleVariance(data, size);
    if (v.type == NUMBER_NAN) return v;

    // Convert the variance to a real radicand
    long double x = 0.0L;
    switch (v.type) {
        case NUMBER_INT:
            x = (long double)v.as.i;
            break;
        case NUMBER_FRACTION:
            x = (long double)v.as.frac.num / (long double)v.as.frac.denom;
            break;
        case NUMBER_REAL:
            x = v.as.x;
            break;
        case NUMBER_COMPLEX:
        case NUMBER_NAN:
            return kumaNan();
    }

    // Return the nonnegative real square root
    if (x < 0.0L) return kumaNan();
    return constructNumberFromDouble(sqrtl(x));
}

// Compute the mean absolute deviation from the mean
Number meanAbsDev(Number* data, size_t size) {
    // Reject arrays containing invalid KUMA stats values
    if (!isValidStatsArray(data, size)) return kumaNan();
    if (size > (size_t)LLONG_MAX) return kumaNan();

    // Detect whether real arithmetic is needed
    bool hasReal = containsReal(data, size);

    // Compute real absolute deviations from the real mean
    if (hasReal) {
        Number avgNumber = mean(data, size);
        if (avgNumber.type != NUMBER_REAL) return kumaNan();
        long double total = 0.0L;
        for (size_t i = 0; i < size; i++) {
            long double x = 0.0L;
            switch (data[i].type) {
                case NUMBER_INT:
                    x = (long double)data[i].as.i;
                    break;
                case NUMBER_FRACTION:
                    x = (long double)data[i].as.frac.num / (long double)data[i].as.frac.denom;
                    break;
                case NUMBER_REAL:
                    x = data[i].as.x;
                    break;
                case NUMBER_COMPLEX:
                case NUMBER_NAN:
                    return kumaNan();
            }
            total += fabsl(x - avgNumber.as.x);
        }
        return constructNumberFromDouble(total / (long double)size);
    }

    // Compute exact absolute deviations from the exact mean
    Number avg = mean(data, size);
    if (avg.type == NUMBER_NAN) return avg;
    Number total = constructNumberFromInt(0);
    for (size_t i = 0; i < size; i++) {
        Number diff;
        switch (data[i].type) {
            case NUMBER_INT:
            case NUMBER_FRACTION:
                diff = subNumbers(data[i], avg);
                break;
            case NUMBER_REAL:
            case NUMBER_COMPLEX:
            case NUMBER_NAN:
                return kumaNan();
        }
        switch (diff.type) {
            case NUMBER_INT:
                if (diff.as.i == LLONG_MIN) return kumaNan();
                if (diff.as.i < 0) diff.as.i = -diff.as.i;
                break;
            case NUMBER_FRACTION:
                if (diff.as.frac.num == LLONG_MIN) return kumaNan();
                if (diff.as.frac.num < 0) diff.as.frac.num = -diff.as.frac.num;
                break;
            case NUMBER_REAL:
            case NUMBER_COMPLEX:
            case NUMBER_NAN:
                return kumaNan();
        }
        total = addNumbers(total, diff);
        if (total.type == NUMBER_NAN) return total;
    }
    return divNumbers(total, constructNumberFromInt((long long)size));
}

// Compute the median absolute deviation from the median
Number medianAbsDev(Number* data, size_t size) {
    // Reject arrays containing invalid KUMA stats values
    if (!isValidStatsArray(data, size)) return kumaNan();

    // Compute the central value before building deviations
    Number med = median(data, size);
    if (med.type == NUMBER_NAN) return med;

    // Detect whether real arithmetic is needed
    bool hasReal = med.type == NUMBER_REAL || containsReal(data, size);

    // Allocate a deviation array that can be passed through median
    Number* devs = malloc(size * sizeof(Number));
    if (!devs) return kumaNan();

    // Build real absolute deviations when real arithmetic is needed
    if (hasReal) {
        long double center = 0.0L;
        switch (med.type) {
            case NUMBER_INT:
                center = (long double)med.as.i;
                break;
            case NUMBER_FRACTION:
                center = (long double)med.as.frac.num / (long double)med.as.frac.denom;
                break;
            case NUMBER_REAL:
                center = med.as.x;
                break;
            case NUMBER_COMPLEX:
            case NUMBER_NAN:
                free(devs);
                return kumaNan();
        }
        for (size_t i = 0; i < size; i++) {
            long double x = 0.0L;
            switch (data[i].type) {
                case NUMBER_INT:
                    x = (long double)data[i].as.i;
                    break;
                case NUMBER_FRACTION:
                    x = (long double)data[i].as.frac.num / (long double)data[i].as.frac.denom;
                    break;
                case NUMBER_REAL:
                    x = data[i].as.x;
                    break;
                case NUMBER_COMPLEX:
                case NUMBER_NAN:
                    free(devs);
                    return kumaNan();
            }
            devs[i] = constructNumberFromDouble(fabsl(x - center));
        }
        Number result = median(devs, size);
        free(devs);
        return result;
    }

    // Build exact absolute deviations when all values are exact
    for (size_t i = 0; i < size; i++) {
        Number diff;
        switch (data[i].type) {
            case NUMBER_INT:
            case NUMBER_FRACTION:
                diff = subNumbers(data[i], med);
                break;
            case NUMBER_REAL:
            case NUMBER_COMPLEX:
            case NUMBER_NAN:
                free(devs);
                return kumaNan();
        }
        switch (diff.type) {
            case NUMBER_INT:
                if (diff.as.i == LLONG_MIN) {
                    free(devs);
                    return kumaNan();
                }
                if (diff.as.i < 0) diff.as.i = -diff.as.i;
                break;
            case NUMBER_FRACTION:
                if (diff.as.frac.num == LLONG_MIN) {
                    free(devs);
                    return kumaNan();
                }
                if (diff.as.frac.num < 0) diff.as.frac.num = -diff.as.frac.num;
                break;
            case NUMBER_REAL:
            case NUMBER_COMPLEX:
            case NUMBER_NAN:
                free(devs);
                return kumaNan();
        }
        devs[i] = diff;
    }
    Number result = median(devs, size);
    free(devs);
    return result;
}

// Compute a percentile using linear interpolation between sorted values
Number percentile(Number* data, size_t size, Number p) {
    // Reject invalid data arrays and invalid percentile values
    if (!isValidStatsArray(data, size) || !isValidStatsNumber(p)) return kumaNan();
    if (size > (size_t)LLONG_MAX) return kumaNan();

    // Convert the percentile argument to a real value for range validation
    long double pReal = 0.0L;
    switch (p.type) {
        case NUMBER_INT:
            pReal = (long double)p.as.i;
            break;
        case NUMBER_FRACTION:
            pReal = (long double)p.as.frac.num / (long double)p.as.frac.denom;
            break;
        case NUMBER_REAL:
            pReal = p.as.x;
            break;
        case NUMBER_COMPLEX:
        case NUMBER_NAN:
            return kumaNan();
    }
    if (pReal < 0.0L || pReal > 100.0L) return kumaNan();

    // Copy and sort the data so interpolation can use adjacent order statistics
    Number* sorted = malloc(size * sizeof(Number));
    if (!sorted) return kumaNan();
    for (size_t i = 0; i < size; i++) sorted[i] = data[i];
    qsort(sorted, size, sizeof(Number), compareStatsNumbers);

    // Detect whether interpolation should be carried out in real arithmetic
    bool hasReal = p.type == NUMBER_REAL || containsReal(data, size);

    // Return the only value immediately for singleton arrays
    if (size == 1) {
        Number result = sorted[0];
        if (hasReal && result.type != NUMBER_REAL) {
            switch (result.type) {
                case NUMBER_INT:
                    result = constructNumberFromDouble((long double)result.as.i);
                    break;
                case NUMBER_FRACTION:
                    result = constructNumberFromDouble((long double)result.as.frac.num / (long double)result.as.frac.denom);
                    break;
                case NUMBER_REAL:
                case NUMBER_COMPLEX:
                case NUMBER_NAN:
                    break;
            }
        }
        free(sorted);
        return result;
    }

    // Interpolate real data at position p/100 * (n - 1)
    if (hasReal) {
        long double pos = (pReal / 100.0L) * (long double)(size - 1);
        size_t lower = (size_t)floorl(pos);
        if (lower >= size - 1) {
            Number result = sorted[size - 1];
            switch (result.type) {
                case NUMBER_INT:
                    result = constructNumberFromDouble((long double)result.as.i);
                    break;
                case NUMBER_FRACTION:
                    result = constructNumberFromDouble((long double)result.as.frac.num / (long double)result.as.frac.denom);
                    break;
                case NUMBER_REAL:
                    break;
                case NUMBER_COMPLEX:
                case NUMBER_NAN:
                    free(sorted);
                    return kumaNan();
            }
            free(sorted);
            return result;
        }
        size_t upper = lower + 1;
        long double weight = pos - (long double)lower;
        long double a = 0.0L;
        long double b = 0.0L;
        switch (sorted[lower].type) {
            case NUMBER_INT:
                a = (long double)sorted[lower].as.i;
                break;
            case NUMBER_FRACTION:
                a = (long double)sorted[lower].as.frac.num / (long double)sorted[lower].as.frac.denom;
                break;
            case NUMBER_REAL:
                a = sorted[lower].as.x;
                break;
            case NUMBER_COMPLEX:
            case NUMBER_NAN:
                free(sorted);
                return kumaNan();
        }
        switch (sorted[upper].type) {
            case NUMBER_INT:
                b = (long double)sorted[upper].as.i;
                break;
            case NUMBER_FRACTION:
                b = (long double)sorted[upper].as.frac.num / (long double)sorted[upper].as.frac.denom;
                break;
            case NUMBER_REAL:
                b = sorted[upper].as.x;
                break;
            case NUMBER_COMPLEX:
            case NUMBER_NAN:
                free(sorted);
                return kumaNan();
        }
        free(sorted);
        return constructNumberFromDouble(a + weight * (b - a));
    }

    // Compute the exact interpolation position for integer and fractional data
    Fraction pFrac;
    switch (p.type) {
        case NUMBER_INT:
            pFrac = constructFraction(p.as.i, 1);
            break;
        case NUMBER_FRACTION:
            pFrac = p.as.frac;
            break;
        case NUMBER_REAL:
        case NUMBER_COMPLEX:
        case NUMBER_NAN:
            free(sorted);
            return kumaNan();
    }
    Fraction pos = multiplyFractions(pFrac, constructFraction((long long)size - 1, 100));
    if (pos.denom == 0) {
        free(sorted);
        return kumaNan();
    }

    // Use exact fraction arithmetic to interpolate between adjacent values
    long long lowerLong = pos.num / pos.denom;
    if (lowerLong >= (long long)size - 1) {
        Number result = sorted[size - 1];
        free(sorted);
        return result;
    }
    size_t lower = (size_t)lowerLong;
    size_t upper = lower + 1;
    Fraction weightFrac = subtractFractions(pos, constructFraction(lowerLong, 1));
    if (weightFrac.denom == 0) {
        free(sorted);
        return kumaNan();
    }
    if (weightFrac.num == 0) {
        Number result = sorted[lower];
        free(sorted);
        return result;
    }
    Number weight = constructNumberFromFraction(weightFrac);
    Number diff = subNumbers(sorted[upper], sorted[lower]);
    Number scaled = multNumbers(weight, diff);
    Number result = addNumbers(sorted[lower], scaled);
    free(sorted);
    return result;
}

// Compute quartile q using percentiles 0, 25, 50, 75, and 100
Number quartile(Number* data, size_t size, int q) {
    // Reject quartile indices outside the standard five-number positions
    if (q < 0 || q > 4) return kumaNan();

    // Delegate interpolation to the percentile routine
    return percentile(data, size, constructNumberFromInt((long long)q * 25));
}

// Compute the interquartile range of a valid stats array
Number iqr(Number* data, size_t size) {
    // Compute the first and third quartiles through the quartile routine
    Number q1 = quartile(data, size, 1);
    Number q3 = quartile(data, size, 3);
    if (q1.type == NUMBER_NAN || q3.type == NUMBER_NAN) return kumaNan();

    // Subtract quartiles using the active Number arithmetic
    return subNumbers(q3, q1);
}

// Compute the geometric mean of a valid positive stats array
Number geometricMean(Number* data, size_t size) {
    // Reject arrays containing invalid KUMA stats values
    if (!isValidStatsArray(data, size)) return kumaNan();

    // Sum logarithms so large products do not overflow
    long double logTotal = 0.0L;
    for (size_t i = 0; i < size; i++) {
        long double x = 0.0L;
        switch (data[i].type) {
            case NUMBER_INT:
                x = (long double)data[i].as.i;
                break;
            case NUMBER_FRACTION:
                x = (long double)data[i].as.frac.num / (long double)data[i].as.frac.denom;
                break;
            case NUMBER_REAL:
                x = data[i].as.x;
                break;
            case NUMBER_COMPLEX:
            case NUMBER_NAN:
                return kumaNan();
        }
        if (x <= 0.0L) return kumaNan();
        logTotal += logl(x);
    }

    // Exponentiate the average logarithm to obtain the mean
    long double result = expl(logTotal / (long double)size);
    return isfinite(result) ? constructNumberFromDouble(result) : kumaNan();
}

// Compute the harmonic mean of a valid stats array
Number harmonicMean(Number* data, size_t size) {
    // Reject arrays containing invalid KUMA stats values
    if (!isValidStatsArray(data, size)) return kumaNan();
    if (size > (size_t)LLONG_MAX) return kumaNan();

    // Detect whether the reciprocal sum must be computed in real arithmetic
    bool hasReal = containsReal(data, size);

    // Compute the reciprocal sum in long double arithmetic for real data
    if (hasReal) {
        long double denom = 0.0L;
        for (size_t i = 0; i < size; i++) {
            long double x = 0.0L;
            switch (data[i].type) {
                case NUMBER_INT:
                    x = (long double)data[i].as.i;
                    break;
                case NUMBER_FRACTION:
                    x = (long double)data[i].as.frac.num / (long double)data[i].as.frac.denom;
                    break;
                case NUMBER_REAL:
                    x = data[i].as.x;
                    break;
                case NUMBER_COMPLEX:
                case NUMBER_NAN:
                    return kumaNan();
            }
            if (x == 0.0L) return kumaNan();
            denom += 1.0L / x;
        }
        if (denom == 0.0L) return kumaNan();
        return constructNumberFromDouble((long double)size / denom);
    }

    // Compute the reciprocal sum exactly for integer and fractional data
    Number denom = constructNumberFromInt(0);
    for (size_t i = 0; i < size; i++) {
        Number reciprocal;
        switch (data[i].type) {
            case NUMBER_INT:
                if (data[i].as.i == 0) return kumaNan();
                reciprocal = constructNumberFromFraction(constructFraction(1, data[i].as.i));
                break;
            case NUMBER_FRACTION:
                if (data[i].as.frac.num == 0) return kumaNan();
                reciprocal = constructNumberFromFraction(constructFraction(data[i].as.frac.denom, data[i].as.frac.num));
                break;
            case NUMBER_REAL:
            case NUMBER_COMPLEX:
            case NUMBER_NAN:
                return kumaNan();
        }
        denom = addNumbers(denom, reciprocal);
        if (denom.type == NUMBER_NAN) return denom;
    }

    // Divide the sample size by the exact reciprocal sum
    switch (denom.type) {
        case NUMBER_INT:
            if (denom.as.i == 0) return kumaNan();
            break;
        case NUMBER_FRACTION:
            if (denom.as.frac.num == 0) return kumaNan();
            break;
        case NUMBER_REAL:
        case NUMBER_COMPLEX:
        case NUMBER_NAN:
            return kumaNan();
    }
    return divNumbers(constructNumberFromInt((long long)size), denom);
}

/* ---------- Frequency functions ---------- */

// Count how many times x occurs in a valid stats array
size_t numFreq(Number* data, Number x, size_t size) {
    // Reject invalid data arrays or invalid search values
    if (!isValidStatsArray(data, size) || !isValidStatsNumber(x)) return 0;

    // Count entries equal to x using HEBI Number equality
    size_t count = 0;
    for (size_t i = 0; i < size; i++) {
        if (eqNumbers(data[i], x)) count++;
    }

    return count;
}

// Count the number of distinct values in a valid stats array
size_t countDistinct(Number* data, size_t size) {
    // Reject arrays containing invalid KUMA stats values
    if (!isValidStatsArray(data, size)) return 0;

    // Copy and sort the data so equal values become adjacent
    Number* sorted = malloc(size * sizeof(Number));
    if (!sorted) return 0;
    for (size_t i = 0; i < size; i++) sorted[i] = data[i];
    qsort(sorted, size, sizeof(Number), compareStatsNumbers);

    // Count the first value and each subsequent new run
    size_t count = 1;
    for (size_t i = 1; i < size; i++) {
        if (!eqNumbers(sorted[i], sorted[i - 1])) count++;
    }

    free(sorted);
    return count;
}

// Return true if a valid stats array contains x
bool contains(Number* data, Number x, size_t size) {
    // Delegate membership to the frequency counter
    return numFreq(data, x, size) > 0;
}

// Compute frequencies for a valid unsorted stats array
Frequency* frequencies(Number* data, size_t size, size_t* outCount) {
    // Initialize the output count before any early return
    if (outCount) *outCount = 0;

    // Reject arrays containing invalid KUMA stats values
    if (!isValidStatsArray(data, size)) return NULL;

    // Copy and sort the data so the sorted frequency routine can count runs
    Number* sorted = malloc(size * sizeof(Number));
    if (!sorted) return NULL;
    for (size_t i = 0; i < size; i++) sorted[i] = data[i];
    qsort(sorted, size, sizeof(Number), compareStatsNumbers);

    // Compute adjacent-run frequencies from the sorted copy
    Frequency* result = frequenciesSorted(sorted, size, outCount);
    free(sorted);
    return result;
}

// Compute frequencies for a valid sorted stats array
Frequency* frequenciesSorted(Number* sortedData, size_t size, size_t* outCount) {
    // Initialize the output count before any early return
    if (outCount) *outCount = 0;

    // Reject arrays containing invalid KUMA stats values
    if (!isValidStatsArray(sortedData, size)) return NULL;

    // Count adjacent runs in the sorted array
    size_t distinct = 1;
    for (size_t i = 1; i < size; i++) {
        if (!eqNumbers(sortedData[i], sortedData[i - 1])) distinct++;
    }

    // Allocate one Frequency entry for each distinct value
    Frequency* freqs = malloc(distinct * sizeof(Frequency));
    if (!freqs) return NULL;

    // Fill each Frequency entry with its run value and run length
    size_t index = 0;
    freqs[index].value = sortedData[0];
    freqs[index].count = 1;
    for (size_t i = 1; i < size; i++) {
        if (eqNumbers(sortedData[i], sortedData[i - 1])) {
            freqs[index].count++;
        } else {
            index++;
            freqs[index].value = sortedData[i];
            freqs[index].count = 1;
        }
    }

    if (outCount) *outCount = distinct;
    return freqs;
}

// Return every modal value in a valid stats array
Number* modes(Number* data, size_t size, size_t* outCount) {
    // Initialize the output count before any early return
    if (outCount) *outCount = 0;

    // Compute frequencies so ties can be identified cleanly
    size_t freqCount = 0;
    Frequency* freqs = frequencies(data, size, &freqCount);
    if (!freqs || freqCount == 0) return NULL;

    // Find the largest observed frequency
    size_t maxCount = freqs[0].count;
    for (size_t i = 1; i < freqCount; i++) {
        if (freqs[i].count > maxCount) maxCount = freqs[i].count;
    }

    // Count how many values attain the largest frequency
    size_t modeCount = 0;
    for (size_t i = 0; i < freqCount; i++) {
        if (freqs[i].count == maxCount) modeCount++;
    }

    // Copy the modal values into a compact result array
    Number* result = malloc(modeCount * sizeof(Number));
    if (!result) {
        freeFrequencies(freqs);
        return NULL;
    }
    size_t index = 0;
    for (size_t i = 0; i < freqCount; i++) {
        if (freqs[i].count == maxCount) {
            result[index] = freqs[i].value;
            index++;
        }
    }

    freeFrequencies(freqs);
    if (outCount) *outCount = modeCount;
    return result;
}

// Free a Frequency array
void freeFrequencies(Frequency* freqs) {
    // Freeing NULL is allowed by the C standard
    free(freqs);
}

// Free a modes array
void freeModes(Number* modes) {
    // Freeing NULL is allowed by the C standard
    free(modes);
}

/* ---------- Two-variable statistics ---------- */

// Compute the population covariance of two valid stats arrays
Number covariance(Number* x, Number* y, size_t size) {
    // Reject invalid arrays and incompatible lengths
    if (!isValidStatsArray(x, size) || !isValidStatsArray(y, size)) return kumaNan();
    if (size > (size_t)LLONG_MAX) return kumaNan();

    // Detect whether covariance must be computed with real arithmetic
    bool hasReal = containsReal(x, size) || containsReal(y, size);

    // Compute real covariance by averaging products of deviations
    if (hasReal) {
        Number meanX = mean(x, size);
        Number meanY = mean(y, size);
        if (meanX.type != NUMBER_REAL || meanY.type != NUMBER_REAL) return kumaNan();

        long double total = 0.0L;
        for (size_t i = 0; i < size; i++) {
            long double xi = 0.0L;
            long double yi = 0.0L;
            switch (x[i].type) {
                case NUMBER_INT:
                    xi = (long double)x[i].as.i;
                    break;
                case NUMBER_FRACTION:
                    xi = (long double)x[i].as.frac.num / (long double)x[i].as.frac.denom;
                    break;
                case NUMBER_REAL:
                    xi = x[i].as.x;
                    break;
                case NUMBER_COMPLEX:
                case NUMBER_NAN:
                    return kumaNan();
            }
            switch (y[i].type) {
                case NUMBER_INT:
                    yi = (long double)y[i].as.i;
                    break;
                case NUMBER_FRACTION:
                    yi = (long double)y[i].as.frac.num / (long double)y[i].as.frac.denom;
                    break;
                case NUMBER_REAL:
                    yi = y[i].as.x;
                    break;
                case NUMBER_COMPLEX:
                case NUMBER_NAN:
                    return kumaNan();
            }
            total += (xi - meanX.as.x) * (yi - meanY.as.x);
        }

        long double result = total / (long double)size;
        return isfinite(result) ? constructNumberFromDouble(result) : kumaNan();
    }

    // Compute exact covariance with HEBI Number arithmetic
    Number meanX = mean(x, size);
    Number meanY = mean(y, size);
    if (meanX.type == NUMBER_NAN || meanY.type == NUMBER_NAN) return kumaNan();

    Number total = constructNumberFromInt(0);
    for (size_t i = 0; i < size; i++) {
        Number dx = subNumbers(x[i], meanX);
        Number dy = subNumbers(y[i], meanY);
        Number prod = multNumbers(dx, dy);
        total = addNumbers(total, prod);
        if (total.type == NUMBER_NAN) return total;
    }

    return divNumbers(total, constructNumberFromInt((long long)size));
}

// Compute the sample covariance of two valid stats arrays
Number sampleCovariance(Number* x, Number* y, size_t size) {
    // Reject invalid arrays and sample sizes without a sample denominator
    if (!isValidStatsArray(x, size) || !isValidStatsArray(y, size) || size < 2) return kumaNan();
    if (size > (size_t)LLONG_MAX) return kumaNan();

    // Detect whether covariance must be computed with real arithmetic
    bool hasReal = containsReal(x, size) || containsReal(y, size);

    // Compute real sample covariance by averaging products over n - 1
    if (hasReal) {
        Number meanX = mean(x, size);
        Number meanY = mean(y, size);
        if (meanX.type != NUMBER_REAL || meanY.type != NUMBER_REAL) return kumaNan();

        long double total = 0.0L;
        for (size_t i = 0; i < size; i++) {
            long double xi = 0.0L;
            long double yi = 0.0L;
            switch (x[i].type) {
                case NUMBER_INT:
                    xi = (long double)x[i].as.i;
                    break;
                case NUMBER_FRACTION:
                    xi = (long double)x[i].as.frac.num / (long double)x[i].as.frac.denom;
                    break;
                case NUMBER_REAL:
                    xi = x[i].as.x;
                    break;
                case NUMBER_COMPLEX:
                case NUMBER_NAN:
                    return kumaNan();
            }
            switch (y[i].type) {
                case NUMBER_INT:
                    yi = (long double)y[i].as.i;
                    break;
                case NUMBER_FRACTION:
                    yi = (long double)y[i].as.frac.num / (long double)y[i].as.frac.denom;
                    break;
                case NUMBER_REAL:
                    yi = y[i].as.x;
                    break;
                case NUMBER_COMPLEX:
                case NUMBER_NAN:
                    return kumaNan();
            }
            total += (xi - meanX.as.x) * (yi - meanY.as.x);
        }

        long double result = total / (long double)(size - 1);
        return isfinite(result) ? constructNumberFromDouble(result) : kumaNan();
    }

    // Compute exact sample covariance with HEBI Number arithmetic
    Number meanX = mean(x, size);
    Number meanY = mean(y, size);
    if (meanX.type == NUMBER_NAN || meanY.type == NUMBER_NAN) return kumaNan();

    Number total = constructNumberFromInt(0);
    for (size_t i = 0; i < size; i++) {
        Number dx = subNumbers(x[i], meanX);
        Number dy = subNumbers(y[i], meanY);
        Number prod = multNumbers(dx, dy);
        total = addNumbers(total, prod);
        if (total.type == NUMBER_NAN) return total;
    }

    return divNumbers(total, constructNumberFromInt((long long)size - 1));
}

// Compute the Pearson correlation of two valid stats arrays
Number correlation(Number* x, Number* y, size_t size) {
    // Compute covariance and both standard deviations
    Number cov = covariance(x, y, size);
    Number sx = stddev(x, size);
    Number sy = stddev(y, size);
    if (cov.type == NUMBER_NAN || sx.type == NUMBER_NAN || sy.type == NUMBER_NAN) return kumaNan();

    // Divide covariance by the product of standard deviations
    Number denom = multNumbers(sx, sy);
    switch (denom.type) {
        case NUMBER_INT:
            if (denom.as.i == 0) return kumaNan();
            break;
        case NUMBER_FRACTION:
            if (denom.as.frac.num == 0) return kumaNan();
            break;
        case NUMBER_REAL:
            if (denom.as.x == 0.0L) return kumaNan();
            break;
        case NUMBER_COMPLEX:
        case NUMBER_NAN:
            return kumaNan();
    }

    return divNumbers(cov, denom);
}

// Compute the least-squares regression slope for y on x
Number linearRegressionSlope(Number* x, Number* y, size_t size) {
    // Compute the numerator and denominator of the slope
    Number cov = covariance(x, y, size);
    Number varX = variance(x, size);
    if (cov.type == NUMBER_NAN || varX.type == NUMBER_NAN) return kumaNan();

    // Reject data with no variation in x
    switch (varX.type) {
        case NUMBER_INT:
            if (varX.as.i == 0) return kumaNan();
            break;
        case NUMBER_FRACTION:
            if (varX.as.frac.num == 0) return kumaNan();
            break;
        case NUMBER_REAL:
            if (varX.as.x == 0.0L) return kumaNan();
            break;
        case NUMBER_COMPLEX:
        case NUMBER_NAN:
            return kumaNan();
    }

    return divNumbers(cov, varX);
}

// Compute the least-squares regression intercept for y on x
Number linearRegressionIntercept(Number* x, Number* y, size_t size) {
    // Compute the slope and the two sample means
    Number slope = linearRegressionSlope(x, y, size);
    Number meanX = mean(x, size);
    Number meanY = mean(y, size);
    if (slope.type == NUMBER_NAN || meanX.type == NUMBER_NAN || meanY.type == NUMBER_NAN) return kumaNan();

    // Apply b = ybar - m*xbar
    Number slopeMean = multNumbers(slope, meanX);
    if (slopeMean.type == NUMBER_NAN) return slopeMean;
    return subNumbers(meanY, slopeMean);
}

// Predict y from a regression slope, intercept, and x value
Number linearRegressionPredict(Number slope, Number intercept, Number x) {
    // Reject invalid scalar inputs
    if (!isValidStatsNumber(slope) || !isValidStatsNumber(intercept) || !isValidStatsNumber(x)) return kumaNan();

    // Apply y = mx + b
    Number product = multNumbers(slope, x);
    if (product.type == NUMBER_NAN) return product;
    return addNumbers(product, intercept);
}

/* ---------- Plot data functions ---------- */

// Compute five-number and outlier data for a boxplot
KumaBoxPlotData* boxplot(Number* data, size_t size) {
    // Convert and sort data for quantile and whisker calculations
    long double* values = numberArrayToLongDouble(data, size);
    if (!values) return NULL;
    qsort(values, size, sizeof(long double), compareLongDoubleValues);

    // Allocate the owned result before filling fields
    KumaBoxPlotData* plot = calloc(1, sizeof(KumaBoxPlotData));
    if (!plot) {
        free(values);
        return NULL;
    }

    // Compute the five-number summary and Tukey fences
    plot->minimum = values[0];
    plot->q1 = sortedLongDoubleQuantile(values, size, 0.25L);
    plot->median = sortedLongDoubleQuantile(values, size, 0.5L);
    plot->q3 = sortedLongDoubleQuantile(values, size, 0.75L);
    plot->maximum = values[size - 1];
    long double iqrValue = plot->q3 - plot->q1;
    plot->lowerFence = plot->q1 - 1.5L * iqrValue;
    plot->upperFence = plot->q3 + 1.5L * iqrValue;
    plot->lowerWhisker = plot->minimum;
    plot->upperWhisker = plot->maximum;

    // Count outliers across the full sample
    for (size_t i = 0; i < size; i++) {
        if (values[i] < plot->lowerFence || values[i] > plot->upperFence) plot->outlierCount++;
    }

    // Locate the lower whisker at the first non-outlier
    for (size_t i = 0; i < size; i++) {
        if (values[i] >= plot->lowerFence && values[i] <= plot->upperFence) {
            plot->lowerWhisker = values[i];
            break;
        }
    }

    // Locate the upper whisker at the last non-outlier
    for (size_t i = size; i > 0; i--) {
        size_t index = i - 1;
        if (values[index] >= plot->lowerFence && values[index] <= plot->upperFence) {
            plot->upperWhisker = values[index];
            break;
        }
    }

    // Copy outlier coordinates into owned storage
    if (plot->outlierCount > 0) {
        plot->outliers = malloc(plot->outlierCount * sizeof(long double));
        if (!plot->outliers) {
            free(values);
            freeBoxPlotData(plot);
            return NULL;
        }
        size_t outIndex = 0;
        for (size_t i = 0; i < size; i++) {
            if (values[i] < plot->lowerFence || values[i] > plot->upperFence)
                plot->outliers[outIndex++] = values[i];
        }
    }

    free(values);
    return plot;
}

// Compute equal-width histogram bins
KumaHistogramData* histogram(Number* data, size_t size, size_t binCount) {
    // Convert data to plotting coordinates
    long double* values = numberArrayToLongDouble(data, size);
    if (!values) return NULL;

    // Choose a default bin count when none is supplied
    if (binCount == 0) binCount = (size_t)ceill(sqrtl((long double)size));
    if (binCount == 0) {
        free(values);
        return NULL;
    }

    // Find the sample endpoints
    long double lo = values[0];
    long double hi = values[0];
    for (size_t i = 1; i < size; i++) {
        if (values[i] < lo) lo = values[i];
        if (values[i] > hi) hi = values[i];
    }

    // Widen degenerate data so the histogram has visible bins
    if (lo == hi) {
        lo -= 0.5L;
        hi += 0.5L;
    }
    long double width = (hi - lo) / (long double)binCount;
    if (!isfinite(width) || width <= 0.0L) {
        free(values);
        return NULL;
    }

    // Allocate and initialize bins
    KumaHistogramData* plot = calloc(1, sizeof(KumaHistogramData));
    if (!plot) {
        free(values);
        return NULL;
    }
    plot->bins = calloc(binCount, sizeof(KumaHistogramBin));
    if (!plot->bins) {
        free(values);
        freeHistogramData(plot);
        return NULL;
    }
    plot->binCount = binCount;
    plot->sampleSize = size;
    plot->minimum = lo;
    plot->maximum = hi;
    plot->binWidth = width;
    for (size_t i = 0; i < binCount; i++) {
        plot->bins[i].lower = lo + (long double)i * width;
        plot->bins[i].upper = i == binCount - 1 ? hi : lo + (long double)(i + 1) * width;
        plot->bins[i].midpoint = (plot->bins[i].lower + plot->bins[i].upper) / 2.0L;
    }

    // Count each value into the appropriate bin
    for (size_t i = 0; i < size; i++) {
        size_t index = (size_t)floorl((values[i] - lo) / width);
        if (index >= binCount) index = binCount - 1;
        plot->bins[index].count++;
    }

    // Compute plot heights as proportions and probability densities
    for (size_t i = 0; i < binCount; i++) {
        plot->bins[i].proportion = (long double)plot->bins[i].count / (long double)size;
        plot->bins[i].density = plot->bins[i].proportion / width;
    }

    free(values);
    return plot;
}

// Compute value-count data for a frequency plot
KumaFrequencyPlotData* frequencyPlot(Number* data, size_t size) {
    // Delegate run counting to the existing frequency helper
    size_t count = 0;
    Frequency* freqs = frequencies(data, size, &count);
    if (!freqs || count == 0) return NULL;

    // Allocate the plot data wrapper
    KumaFrequencyPlotData* plot = calloc(1, sizeof(KumaFrequencyPlotData));
    if (!plot) {
        freeFrequencies(freqs);
        return NULL;
    }
    plot->items = calloc(count, sizeof(KumaFrequencyPlotItem));
    if (!plot->items) {
        freeFrequencies(freqs);
        freeFrequencyPlotData(plot);
        return NULL;
    }

    // Copy values, counts, numeric positions, and proportions
    plot->count = count;
    plot->sampleSize = size;
    for (size_t i = 0; i < count; i++) {
        long double position = 0.0L;
        if (!numberToLongDouble(freqs[i].value, &position)) {
            freeFrequencies(freqs);
            freeFrequencyPlotData(plot);
            return NULL;
        }
        plot->items[i].value = freqs[i].value;
        plot->items[i].count = freqs[i].count;
        plot->items[i].position = position;
        plot->items[i].proportion = (long double)freqs[i].count / (long double)size;
    }

    freeFrequencies(freqs);
    return plot;
}

// Compute explicit bar-graph data from labels and values
KumaBarGraphData* barGraph(Number* labels, Number* values, size_t size) {
    // Reject invalid labels, invalid values, and negative bar heights
    if (!isValidStatsArray(labels, size) || !isValidStatsArray(values, size)) return NULL;

    // Allocate one bar per input pair
    KumaBarGraphData* plot = calloc(1, sizeof(KumaBarGraphData));
    if (!plot) return NULL;
    plot->bars = calloc(size, sizeof(KumaBarGraphItem));
    if (!plot->bars) {
        freeBarGraphData(plot);
        return NULL;
    }
    plot->count = size;

    // Copy labels and decode heights
    for (size_t i = 0; i < size; i++) {
        long double height = 0.0L;
        if (!numberToLongDouble(values[i], &height) || height < 0.0L) {
            freeBarGraphData(plot);
            return NULL;
        }
        plot->bars[i].label = labels[i];
        plot->bars[i].value = values[i];
        plot->bars[i].height = height;
    }
    return plot;
}

// Compute a Gaussian kernel density estimate
KumaDensityPlotData* densityPlot(Number* data, size_t size, size_t pointCount, Number bandwidth) {
    // Convert data to plotting coordinates
    long double* values = numberArrayToLongDouble(data, size);
    if (!values) return NULL;
    qsort(values, size, sizeof(long double), compareLongDoubleValues);

    // Choose a default sample count and bandwidth when needed
    if (pointCount == 0) pointCount = 128;
    if (pointCount < 2) {
        free(values);
        return NULL;
    }
    long double h = 0.0L;
    if (!numberToLongDouble(bandwidth, &h) || h <= 0.0L) {
        long double sd = longDoubleSampleStddev(values, size);
        h = 1.06L * (isfinite(sd) && sd > 0.0L ? sd : 1.0L) * powl((long double)size, -0.2L);
    }
    if (!isfinite(h) || h <= 0.0L) {
        free(values);
        return NULL;
    }

    // Allocate output sample points
    KumaDensityPlotData* plot = calloc(1, sizeof(KumaDensityPlotData));
    if (!plot) {
        free(values);
        return NULL;
    }
    plot->points = calloc(pointCount, sizeof(KumaPlotPoint));
    if (!plot->points) {
        free(values);
        freeDensityPlotData(plot);
        return NULL;
    }
    plot->count = pointCount;
    plot->sampleSize = size;
    plot->bandwidth = h;

    // Sample the KDE over a padded data interval
    long double lo = values[0] - 3.0L * h;
    long double hi = values[size - 1] + 3.0L * h;
    long double step = (hi - lo) / (long double)(pointCount - 1);
    long double norm = 1.0L / ((long double)size * h * sqrtl(2.0L * M_PI));
    for (size_t i = 0; i < pointCount; i++) {
        long double x = lo + (long double)i * step;
        long double total = 0.0L;
        for (size_t j = 0; j < size; j++) {
            long double z = (x - values[j]) / h;
            total += expl(-0.5L * z * z);
        }
        plot->points[i].x = x;
        plot->points[i].y = norm * total;
    }

    free(values);
    return plot;
}

// Compute stacked dot-plot data from value frequencies
KumaDotPlotData* dotPlot(Number* data, size_t size) {
    // Reuse frequency plot data because a dot plot is value-count data
    KumaFrequencyPlotData* freqs = frequencyPlot(data, size);
    if (!freqs) return NULL;

    // Transfer the item array into the dot-plot wrapper
    KumaDotPlotData* plot = calloc(1, sizeof(KumaDotPlotData));
    if (!plot) {
        freeFrequencyPlotData(freqs);
        return NULL;
    }
    plot->dots = freqs->items;
    plot->count = freqs->count;
    plot->sampleSize = freqs->sampleSize;
    freqs->items = NULL;
    freeFrequencyPlotData(freqs);
    return plot;
}

// Compute empirical CDF jump points
KumaECDFPlotData* ecdf(Number* data, size_t size) {
    // Sort data and count distinct runs
    Number* sorted = sortedNumberCopy(data, size);
    if (!sorted) return NULL;
    size_t distinct = countDistinct(sorted, size);
    if (distinct == 0) {
        free(sorted);
        return NULL;
    }

    // Allocate one ECDF point per distinct sample value
    KumaECDFPlotData* plot = calloc(1, sizeof(KumaECDFPlotData));
    if (!plot) {
        free(sorted);
        return NULL;
    }
    plot->points = calloc(distinct, sizeof(KumaPlotPoint));
    if (!plot->points) {
        free(sorted);
        freeECDFPlotData(plot);
        return NULL;
    }
    plot->count = distinct;
    plot->sampleSize = size;

    // Fill jump coordinates using cumulative proportions
    size_t point = 0;
    size_t cumulative = 0;
    for (size_t i = 0; i < size; ) {
        size_t j = i + 1;
        while (j < size && eqNumbers(sorted[j], sorted[i])) j++;
        cumulative = j;
        long double x = 0.0L;
        if (!numberToLongDouble(sorted[i], &x)) {
            free(sorted);
            freeECDFPlotData(plot);
            return NULL;
        }
        plot->points[point].x = x;
        plot->points[point].y = (long double)cumulative / (long double)size;
        point++;
        i = j;
    }

    free(sorted);
    return plot;
}

// Compute QQ plot coordinates against a probability distribution
KumaQQPlotData* qqPlot(Number* data, size_t size, ProbabilityDistribution* dist) {
    // Convert and sort sample data
    if (!dist || !distributionHasCDF(dist)) return NULL;
    long double* values = numberArrayToLongDouble(data, size);
    if (!values) return NULL;
    qsort(values, size, sizeof(long double), compareLongDoubleValues);

    // Allocate one point per sample
    KumaQQPlotData* plot = calloc(1, sizeof(KumaQQPlotData));
    if (!plot) {
        free(values);
        return NULL;
    }
    plot->points = calloc(size, sizeof(KumaPlotPoint));
    if (!plot->points) {
        free(values);
        freeQQPlotData(plot);
        return NULL;
    }
    plot->count = size;

    // Pair theoretical quantiles with sorted sample quantiles
    for (size_t i = 0; i < size; i++) {
        long double p = ((long double)i + 0.5L) / (long double)size;
        long double q = 0.0L;
        if (!distributionQuantile(dist, p, &q)) {
            free(values);
            freeQQPlotData(plot);
            return NULL;
        }
        plot->points[i].x = q;
        plot->points[i].y = values[i];
    }

    free(values);
    return plot;
}

// Compute paired coordinates for a scatter plot
KumaScatterPlotData* scatterPlot(Number* x, Number* y, size_t size) {
    // Convert both arrays into coordinate storage
    long double* xs = numberArrayToLongDouble(x, size);
    long double* ys = numberArrayToLongDouble(y, size);
    if (!xs || !ys) {
        free(xs);
        free(ys);
        return NULL;
    }

    // Allocate one point per pair
    KumaScatterPlotData* plot = calloc(1, sizeof(KumaScatterPlotData));
    if (!plot) {
        free(xs);
        free(ys);
        return NULL;
    }
    plot->points = calloc(size, sizeof(KumaPlotPoint));
    if (!plot->points) {
        free(xs);
        free(ys);
        freeScatterPlotData(plot);
        return NULL;
    }
    plot->count = size;

    // Copy coordinate pairs into the plot data
    for (size_t i = 0; i < size; i++) {
        plot->points[i].x = xs[i];
        plot->points[i].y = ys[i];
    }

    free(xs);
    free(ys);
    return plot;
}

// Compute scatter data plus least-squares regression line endpoints
KumaRegressionPlotData* regressionPlot(Number* x, Number* y, size_t size) {
    // Build the scatter point data first
    KumaScatterPlotData* scatter = scatterPlot(x, y, size);
    if (!scatter) return NULL;

    // Compute regression coefficients and convert them to real coordinates
    Number slope = linearRegressionSlope(x, y, size);
    Number intercept = linearRegressionIntercept(x, y, size);
    long double m = 0.0L;
    long double b = 0.0L;
    if (slope.type == NUMBER_NAN || intercept.type == NUMBER_NAN
            || !numberToLongDouble(slope, &m) || !numberToLongDouble(intercept, &b)) {
        freeScatterPlotData(scatter);
        return NULL;
    }

    // Find the plotted x-range for the regression segment
    long double xMin = scatter->points[0].x;
    long double xMax = scatter->points[0].x;
    for (size_t i = 1; i < scatter->count; i++) {
        if (scatter->points[i].x < xMin) xMin = scatter->points[i].x;
        if (scatter->points[i].x > xMax) xMax = scatter->points[i].x;
    }

    // Transfer scatter storage into the regression plot wrapper
    KumaRegressionPlotData* plot = calloc(1, sizeof(KumaRegressionPlotData));
    if (!plot) {
        freeScatterPlotData(scatter);
        return NULL;
    }
    plot->points = scatter->points;
    plot->count = scatter->count;
    scatter->points = NULL;
    freeScatterPlotData(scatter);
    plot->slope = slope;
    plot->intercept = intercept;
    plot->lineStart = (KumaPlotPoint){ .x = xMin, .y = m * xMin + b };
    plot->lineEnd = (KumaPlotPoint){ .x = xMax, .y = m * xMax + b };
    return plot;
}

// Free boxplot data
void freeBoxPlotData(KumaBoxPlotData* plot) {
    // Free nested arrays before freeing the wrapper
    if (!plot) return;
    free(plot->outliers);
    free(plot);
}

// Free histogram data
void freeHistogramData(KumaHistogramData* plot) {
    // Free nested arrays before freeing the wrapper
    if (!plot) return;
    free(plot->bins);
    free(plot);
}

// Free frequency plot data
void freeFrequencyPlotData(KumaFrequencyPlotData* plot) {
    // Free nested arrays before freeing the wrapper
    if (!plot) return;
    free(plot->items);
    free(plot);
}

// Free bar graph data
void freeBarGraphData(KumaBarGraphData* plot) {
    // Free nested arrays before freeing the wrapper
    if (!plot) return;
    free(plot->bars);
    free(plot);
}

// Free density plot data
void freeDensityPlotData(KumaDensityPlotData* plot) {
    // Free nested arrays before freeing the wrapper
    if (!plot) return;
    free(plot->points);
    free(plot);
}

// Free dot plot data
void freeDotPlotData(KumaDotPlotData* plot) {
    // Free nested arrays before freeing the wrapper
    if (!plot) return;
    free(plot->dots);
    free(plot);
}

// Free ECDF plot data
void freeECDFPlotData(KumaECDFPlotData* plot) {
    // Free nested arrays before freeing the wrapper
    if (!plot) return;
    free(plot->points);
    free(plot);
}

// Free QQ plot data
void freeQQPlotData(KumaQQPlotData* plot) {
    // Free nested arrays before freeing the wrapper
    if (!plot) return;
    free(plot->points);
    free(plot);
}

// Free scatter plot data
void freeScatterPlotData(KumaScatterPlotData* plot) {
    // Free nested arrays before freeing the wrapper
    if (!plot) return;
    free(plot->points);
    free(plot);
}

// Free regression plot data
void freeRegressionPlotData(KumaRegressionPlotData* plot) {
    // Free nested arrays before freeing the wrapper
    if (!plot) return;
    free(plot->points);
    free(plot);
}

/* ---------- ProbabilityDistribution construction ---------- */
ProbabilityDistribution* constructProbabilityDistribution(
    KumaDistributionType type,
    KumaDistributionKind kind,
    void* params,
    bool ownsParams,
    Number (*pmf)(ProbabilityDistribution* dist, Number x),
    Number (*pdf)(ProbabilityDistribution* dist, Number x),
    Number (*cdf)(ProbabilityDistribution* dist, Number x),
    Number (*mean)(ProbabilityDistribution* dist),
    Number (*variance)(ProbabilityDistribution* dist),
    Number (*stddev)(ProbabilityDistribution* dist),
    Number (*sample)(ProbabilityDistribution* dist)
) {
    // Allocate the distribution wrapper
    ProbabilityDistribution* dist = calloc(1, sizeof(ProbabilityDistribution));
    if (!dist) return NULL;

    // Store the metadata, parameter storage, and callback table
    dist->type = type;
    dist->kind = kind;
    dist->params = params;
    dist->ownsParams = ownsParams;
    dist->pmf = pmf;
    dist->pdf = pdf;
    dist->cdf = cdf;
    dist->mean = mean;
    dist->variance = variance;
    dist->stddev = stddev;
    dist->sample = sample;
    return dist;
}

// Construct a Bernoulli distribution
ProbabilityDistribution* constructBernoulliDistribution(Number p) {
    // Reject invalid Bernoulli probabilities
    if (!isProbabilityNumber(p)) return NULL;

    // Allocate and initialize owned Bernoulli parameters
    BernoulliParams* params = malloc(sizeof(BernoulliParams));
    if (!params) return NULL;
    params->p = p;

    // Attach the Bernoulli callback table
    return constructProbabilityDistribution(KUMA_DIST_DISCRETE, KUMA_DIST_BERNOULLI, params, true,
                                            bernoulliPMF, NULL, bernoulliCDF,
                                            bernoulliMean, bernoulliVariance, bernoulliStddev,
                                            bernoulliSample);
}

// Construct a binomial distribution
ProbabilityDistribution* constructBinomialDistribution(long long n, Number p) {
    // Reject invalid trial counts and probabilities
    if (n < 0 || !isProbabilityNumber(p)) return NULL;

    // Allocate and initialize owned binomial parameters
    BinomialParams* params = malloc(sizeof(BinomialParams));
    if (!params) return NULL;
    params->n = n;
    params->p = p;

    // Attach the binomial callback table
    return constructProbabilityDistribution(KUMA_DIST_DISCRETE, KUMA_DIST_BINOMIAL, params, true,
                                            binomialPMF, NULL, binomialCDF,
                                            binomialMean, binomialVariance, binomialStddev,
                                            binomialSample);
}

// Construct a geometric distribution
ProbabilityDistribution* constructGeometricDistribution(Number p) {
    // Reject invalid geometric probabilities
    if (!isProbabilityNumber(p) || compNumbers(p, constructNumberFromInt(0)) <= 0) return NULL;

    // Allocate and initialize owned geometric parameters
    GeometricParams* params = malloc(sizeof(GeometricParams));
    if (!params) return NULL;
    params->p = p;

    // Attach the geometric callback table
    return constructProbabilityDistribution(KUMA_DIST_DISCRETE, KUMA_DIST_GEOMETRIC, params, true,
                                            geometricPMF, NULL, geometricCDF,
                                            geometricMeanDist, geometricVariance, geometricStddev,
                                            geometricSample);
}

// Construct a Poisson distribution
ProbabilityDistribution* constructPoissonDistribution(Number lambda) {
    // Reject invalid Poisson rates
    if (!isPositiveNumber(lambda)) return NULL;

    // Allocate and initialize owned Poisson parameters
    PoissonParams* params = malloc(sizeof(PoissonParams));
    if (!params) return NULL;
    params->lambda = lambda;

    // Attach the Poisson callback table
    return constructProbabilityDistribution(KUMA_DIST_DISCRETE, KUMA_DIST_POISSON, params, true,
                                            poissonPMF, NULL, poissonCDF,
                                            poissonMean, poissonVariance, poissonStddev,
                                            poissonSample);
}

// Construct a discrete uniform distribution
ProbabilityDistribution* constructDiscreteUniformDistribution(long long a, long long b) {
    // Reject invalid discrete intervals
    long long width = 0;
    if (!discreteUniformWidth(a, b, &width)) return NULL;

    // Allocate and initialize owned discrete uniform parameters
    DiscreteUniformParams* params = malloc(sizeof(DiscreteUniformParams));
    if (!params) return NULL;
    params->a = a;
    params->b = b;

    // Attach the discrete uniform callback table
    return constructProbabilityDistribution(KUMA_DIST_DISCRETE, KUMA_DIST_DISCRETE_UNIFORM, params, true,
                                            discreteUniformPMF, NULL, discreteUniformCDF,
                                            discreteUniformMean, discreteUniformVariance, discreteUniformStddev,
                                            discreteUniformSample);
}

// Construct a continuous uniform distribution
ProbabilityDistribution* constructContinuousUniformDistribution(Number a, Number b) {
    // Reject invalid continuous intervals
    long double lower = 0.0L;
    long double upper = 0.0L;
    if (!numberToLongDouble(a, &lower) || !numberToLongDouble(b, &upper) || lower >= upper) return NULL;

    // Allocate and initialize owned continuous uniform parameters
    ContinuousUniformParams* params = malloc(sizeof(ContinuousUniformParams));
    if (!params) return NULL;
    params->a = a;
    params->b = b;

    // Attach the continuous uniform callback table
    return constructProbabilityDistribution(KUMA_DIST_CONTINUOUS, KUMA_DIST_CONTINUOUS_UNIFORM, params, true,
                                            NULL, continuousUniformPDF, continuousUniformCDF,
                                            continuousUniformMean, continuousUniformVariance, continuousUniformStddev,
                                            continuousUniformSample);
}

// Construct a normal distribution
ProbabilityDistribution* constructNormalDistribution(Number mu, Number sigma) {
    // Reject invalid normal parameters
    long double muValue = 0.0L;
    long double sigmaValue = 0.0L;
    if (!numberToLongDouble(mu, &muValue) || !numberToLongDouble(sigma, &sigmaValue) || sigmaValue <= 0.0L) return NULL;

    // Allocate and initialize owned normal parameters
    NormalParams* params = malloc(sizeof(NormalParams));
    if (!params) return NULL;
    params->mu = mu;
    params->sigma = sigma;

    // Attach the normal callback table
    return constructProbabilityDistribution(KUMA_DIST_CONTINUOUS, KUMA_DIST_NORMAL, params, true,
                                            NULL, normalPDF, normalCDF,
                                            normalMean, normalVariance, normalStddev,
                                            normalSample);
}

// Construct an exponential distribution
ProbabilityDistribution* constructExponentialDistribution(Number lambda) {
    // Reject invalid exponential rates
    if (!isPositiveNumber(lambda)) return NULL;

    // Allocate and initialize owned exponential parameters
    ExponentialParams* params = malloc(sizeof(ExponentialParams));
    if (!params) return NULL;
    params->lambda = lambda;

    // Attach the exponential callback table
    return constructProbabilityDistribution(KUMA_DIST_CONTINUOUS, KUMA_DIST_EXPONENTIAL, params, true,
                                            NULL, exponentialPDF, exponentialCDF,
                                            exponentialMean, exponentialVariance, exponentialStddev,
                                            exponentialSample);
}

// Construct a custom discrete distribution
ProbabilityDistribution* constructCustomDiscreteDistribution(
    Number* values,
    Number* probabilities,
    size_t size,
    bool copyArrays
) {
    // Reject invalid support and probability arrays
    if (!isValidStatsArray(values, size) || !isValidProbabilityArray(probabilities, size)) return NULL;

    // Allocate the parameter struct
    CustomDiscreteParams* params = calloc(1, sizeof(CustomDiscreteParams));
    if (!params) return NULL;
    params->size = size;
    params->ownsArrays = copyArrays;

    // Either copy the arrays or store the caller-supplied views
    if (copyArrays) {
        params->values = calloc(size, sizeof(Number));
        params->probabilities = calloc(size, sizeof(Number));
        if (!params->values || !params->probabilities) {
            free(params->values);
            free(params->probabilities);
            free(params);
            return NULL;
        }
        for (size_t i = 0; i < size; i++) {
            params->values[i] = values[i];
            params->probabilities[i] = probabilities[i];
        }
    } else {
        params->values = values;
        params->probabilities = probabilities;
    }

    // Attach the custom discrete callback table
    return constructProbabilityDistribution(KUMA_DIST_DISCRETE, KUMA_DIST_CUSTOM, params, true,
                                            customDiscretePMF, NULL, customDiscreteCDF,
                                            customDiscreteMean, customDiscreteVariance, customDiscreteStddev,
                                            customDiscreteSample);
}

// Construct a custom continuous distribution
ProbabilityDistribution* constructCustomContinuousDistribution(
    Number lower,
    Number upper,
    Number (*pdf)(ProbabilityDistribution* dist, Number x),
    Number (*cdf)(ProbabilityDistribution* dist, Number x),
    Number (*mean)(ProbabilityDistribution* dist),
    Number (*variance)(ProbabilityDistribution* dist),
    Number (*sample)(ProbabilityDistribution* dist)
) {
    // Reject invalid support endpoints and missing distribution callbacks
    long double lowerValue = 0.0L;
    long double upperValue = 0.0L;
    if (!numberToLongDouble(lower, &lowerValue) || !numberToLongDouble(upper, &upperValue) || lowerValue >= upperValue) return NULL;
    if (!pdf || !cdf || !mean || !variance) return NULL;

    // Allocate and initialize owned custom continuous parameters
    CustomContinuousParams* params = malloc(sizeof(CustomContinuousParams));
    if (!params) return NULL;
    params->lower = lower;
    params->upper = upper;

    // Attach the caller-supplied callback table
    return constructProbabilityDistribution(KUMA_DIST_CONTINUOUS, KUMA_DIST_CUSTOM, params, true,
                                            NULL, pdf, cdf,
                                            mean, variance, NULL,
                                            sample);
}

// Free a ProbabilityDistribution and any owned parameter storage
void freeProbabilityDistribution(ProbabilityDistribution* dist) {
    // Treat NULL as already freed
    if (!dist) return;

    // Free owned custom discrete arrays before freeing the parameter struct
    if (dist->ownsParams && dist->params && dist->kind == KUMA_DIST_CUSTOM && dist->type == KUMA_DIST_DISCRETE) {
        CustomDiscreteParams* params = (CustomDiscreteParams*)dist->params;
        if (params->ownsArrays) {
            free(params->values);
            free(params->probabilities);
        }
    }

    // Free owned source distributions inside distribution-scheme parameters
    if (dist->ownsParams && dist->params && dist->kind == KUMA_DIST_AFFINE) {
        AffineDistributionParams* params = (AffineDistributionParams*)dist->params;
        freeProbabilityDistribution(params->base);
    }
    if (dist->ownsParams && dist->params
        && (dist->kind == KUMA_DIST_SUM_INDEPENDENT || dist->kind == KUMA_DIST_PRODUCT_INDEPENDENT)) {
        BinaryDistributionParams* params = (BinaryDistributionParams*)dist->params;
        freeProbabilityDistribution(params->x);
        freeProbabilityDistribution(params->y);
    }

    // Free owned parameter storage and the wrapper itself
    if (dist->ownsParams) free(dist->params);
    free(dist);
}

// Deep copy a probability distribution
ProbabilityDistribution* copyProbabilityDistribution(ProbabilityDistribution* dist) {
    // Reject missing source distributions
    if (!dist) return NULL;

    // Copy named distributions through their public constructors
    if (dist->kind == KUMA_DIST_BERNOULLI && dist->params) {
        BernoulliParams* params = (BernoulliParams*)dist->params;
        return constructBernoulliDistribution(params->p);
    }
    if (dist->kind == KUMA_DIST_BINOMIAL && dist->params) {
        BinomialParams* params = (BinomialParams*)dist->params;
        return constructBinomialDistribution(params->n, params->p);
    }
    if (dist->kind == KUMA_DIST_GEOMETRIC && dist->params) {
        GeometricParams* params = (GeometricParams*)dist->params;
        return constructGeometricDistribution(params->p);
    }
    if (dist->kind == KUMA_DIST_POISSON && dist->params) {
        PoissonParams* params = (PoissonParams*)dist->params;
        return constructPoissonDistribution(params->lambda);
    }
    if (dist->kind == KUMA_DIST_DISCRETE_UNIFORM && dist->params) {
        DiscreteUniformParams* params = (DiscreteUniformParams*)dist->params;
        return constructDiscreteUniformDistribution(params->a, params->b);
    }
    if (dist->kind == KUMA_DIST_CONTINUOUS_UNIFORM && dist->params) {
        ContinuousUniformParams* params = (ContinuousUniformParams*)dist->params;
        return constructContinuousUniformDistribution(params->a, params->b);
    }
    if (dist->kind == KUMA_DIST_NORMAL && dist->params) {
        NormalParams* params = (NormalParams*)dist->params;
        return constructNormalDistribution(params->mu, params->sigma);
    }
    if (dist->kind == KUMA_DIST_EXPONENTIAL && dist->params) {
        ExponentialParams* params = (ExponentialParams*)dist->params;
        return constructExponentialDistribution(params->lambda);
    }

    // Copy custom distributions through their public constructors
    if (dist->kind == KUMA_DIST_CUSTOM && dist->params && dist->type == KUMA_DIST_DISCRETE) {
        CustomDiscreteParams* params = (CustomDiscreteParams*)dist->params;
        return constructCustomDiscreteDistribution(params->values, params->probabilities, params->size, true);
    }
    if (dist->kind == KUMA_DIST_CUSTOM && dist->params && dist->type == KUMA_DIST_CONTINUOUS) {
        CustomContinuousParams* params = (CustomContinuousParams*)dist->params;
        return constructCustomContinuousDistribution(params->lower, params->upper,
                                                     dist->pdf, dist->cdf, dist->mean, dist->variance,
                                                     dist->sample);
    }

    // Copy distribution schemes through their public constructors
    if (dist->kind == KUMA_DIST_AFFINE && dist->params) {
        AffineDistributionParams* params = (AffineDistributionParams*)dist->params;
        return affineDistribution(params->base, params->scalar, params->shift);
    }
    if (dist->kind == KUMA_DIST_SUM_INDEPENDENT && dist->params) {
        BinaryDistributionParams* params = (BinaryDistributionParams*)dist->params;
        return sumIndependentDistributions(params->x, params->y);
    }
    if (dist->kind == KUMA_DIST_PRODUCT_INDEPENDENT && dist->params) {
        BinaryDistributionParams* params = (BinaryDistributionParams*)dist->params;
        return productIndependentDistributions(params->x, params->y);
    }

    return NULL;
}

// Construct an affine distribution from a copied source distribution
ProbabilityDistribution* affineDistribution(ProbabilityDistribution* dist, Number scalar, Number shift) {
    // Reject invalid sources and affine parameters
    if (!dist || !isValidStatsNumber(scalar) || !isValidStatsNumber(shift)) return NULL;

    // Collapse zero-scale transformations to a copied point mass
    long double scalarValue = 0.0L;
    long double shiftValue = 0.0L;
    if (!numberToLongDouble(scalar, &scalarValue) || !numberToLongDouble(shift, &shiftValue)) return NULL;
    if (scalarValue == 0.0L) return constructDegenerateDistribution(shift);

    // Require finite support for discrete affine distributions
    if (dist->type == KUMA_DIST_DISCRETE) {
        Number* values = NULL;
        Number* probabilities = NULL;
        size_t size = 0;
        if (!finiteDiscreteSupport(dist, &values, &probabilities, &size)) return NULL;
        freeNumberArrays(values, probabilities);
    }

    // Deep copy the source distribution into the scheme parameters
    ProbabilityDistribution* base = copyProbabilityDistribution(dist);
    if (!base) return NULL;
    AffineDistributionParams* params = calloc(1, sizeof(AffineDistributionParams));
    if (!params) {
        freeProbabilityDistribution(base);
        return NULL;
    }
    params->base = base;
    params->scalar = scalar;
    params->shift = shift;
    params->scalarValue = scalarValue;
    params->shiftValue = shiftValue;

    // Store transformed finite support when the base support is finite
    long double lower = 0.0L;
    long double upper = 0.0L;
    if (dist->type == KUMA_DIST_CONTINUOUS && continuousSupport(dist, &lower, &upper)) {
        long double a = scalarValue * lower + shiftValue;
        long double b = scalarValue * upper + shiftValue;
        params->support.lower = constructNumberFromDouble(fminl(a, b));
        params->support.upper = constructNumberFromDouble(fmaxl(a, b));
    } else {
        params->support.lower = kumaNan();
        params->support.upper = kumaNan();
    }

    // Attach callbacks for the affine distribution scheme
    return constructProbabilityDistribution(dist->type, KUMA_DIST_AFFINE, params, true,
                                            dist->type == KUMA_DIST_DISCRETE ? affineDistributionPMF : NULL,
                                            dist->type == KUMA_DIST_CONTINUOUS ? affineDistributionPDF : NULL,
                                            affineDistributionCDF,
                                            affineDistributionMean, affineDistributionVariance, affineDistributionStddev,
                                            affineDistributionSample);
}

// Construct a sum of independent distributions from copied source distributions
ProbabilityDistribution* sumIndependentDistributions(ProbabilityDistribution* x, ProbabilityDistribution* y) {
    // Reject missing sources
    if (!x || !y) return NULL;

    // Use exact named closure for independent Poisson sums
    if (x->kind == KUMA_DIST_POISSON && y->kind == KUMA_DIST_POISSON && x->params && y->params) {
        PoissonParams* px = (PoissonParams*)x->params;
        PoissonParams* py = (PoissonParams*)y->params;
        return constructPoissonDistribution(addNumbers(px->lambda, py->lambda));
    }

    // Use exact named closure for independent normal sums
    if (x->kind == KUMA_DIST_NORMAL && y->kind == KUMA_DIST_NORMAL && x->params && y->params) {
        NormalParams* px = (NormalParams*)x->params;
        NormalParams* py = (NormalParams*)y->params;
        Number mu = addNumbers(px->mu, py->mu);
        Number variance = addNumbers(multNumbers(px->sigma, px->sigma), multNumbers(py->sigma, py->sigma));
        long double varianceValue = 0.0L;
        if (!numberToLongDouble(variance, &varianceValue) || varianceValue <= 0.0L) return NULL;
        return constructNormalDistribution(mu, constructNumberFromDouble(sqrtl(varianceValue)));
    }

    // Support exact finite discrete sums and bounded continuous sums
    KumaDistributionType type = x->type;
    if (x->type != y->type) return NULL;
    long double xLower = 0.0L;
    long double xUpper = 0.0L;
    long double yLower = 0.0L;
    long double yUpper = 0.0L;
    if (type == KUMA_DIST_DISCRETE) {
        Number* values = NULL;
        Number* probabilities = NULL;
        size_t size = 0;
        if (!finiteDiscreteSupport(x, &values, &probabilities, &size)) return NULL;
        freeNumberArrays(values, probabilities);
        if (!finiteDiscreteSupport(y, &values, &probabilities, &size)) return NULL;
        freeNumberArrays(values, probabilities);
    } else if (!continuousSupport(x, &xLower, &xUpper) || !continuousSupport(y, &yLower, &yUpper)) {
        return NULL;
    }

    // Deep copy both sources into the scheme parameters
    ProbabilityDistribution* xCopy = copyProbabilityDistribution(x);
    ProbabilityDistribution* yCopy = copyProbabilityDistribution(y);
    if (!xCopy || !yCopy) {
        freeProbabilityDistribution(xCopy);
        freeProbabilityDistribution(yCopy);
        return NULL;
    }
    BinaryDistributionParams* params = calloc(1, sizeof(BinaryDistributionParams));
    if (!params) {
        freeProbabilityDistribution(xCopy);
        freeProbabilityDistribution(yCopy);
        return NULL;
    }
    params->x = xCopy;
    params->y = yCopy;
    params->xLower = xLower;
    params->xUpper = xUpper;
    params->yLower = yLower;
    params->yUpper = yUpper;
    params->support.lower = type == KUMA_DIST_CONTINUOUS ? constructNumberFromDouble(xLower + yLower) : kumaNan();
    params->support.upper = type == KUMA_DIST_CONTINUOUS ? constructNumberFromDouble(xUpper + yUpper) : kumaNan();

    // Attach callbacks for the independent-sum distribution scheme
    return constructProbabilityDistribution(type, KUMA_DIST_SUM_INDEPENDENT, params, true,
                                            type == KUMA_DIST_DISCRETE ? sumIndependentDistributionPMF : NULL,
                                            type == KUMA_DIST_CONTINUOUS ? sumIndependentDistributionPDF : NULL,
                                            sumIndependentDistributionCDF,
                                            sumIndependentDistributionMean, sumIndependentDistributionVariance,
                                            sumIndependentDistributionStddev, sumIndependentDistributionSample);
}

// Construct a product of independent distributions from copied source distributions
ProbabilityDistribution* productIndependentDistributions(ProbabilityDistribution* x, ProbabilityDistribution* y) {
    // Reject missing sources and mixed distribution types
    if (!x || !y || x->type != y->type) return NULL;
    KumaDistributionType type = x->type;
    long double xLower = 0.0L;
    long double xUpper = 0.0L;
    long double yLower = 0.0L;
    long double yUpper = 0.0L;

    // Support exact finite discrete products and bounded continuous products
    if (type == KUMA_DIST_DISCRETE) {
        Number* values = NULL;
        Number* probabilities = NULL;
        size_t size = 0;
        if (!finiteDiscreteSupport(x, &values, &probabilities, &size)) return NULL;
        freeNumberArrays(values, probabilities);
        if (!finiteDiscreteSupport(y, &values, &probabilities, &size)) return NULL;
        freeNumberArrays(values, probabilities);
    } else if (!continuousSupport(x, &xLower, &xUpper) || !continuousSupport(y, &yLower, &yUpper)) {
        return NULL;
    }

    // Choose an integration variable whose support avoids zero for continuous products
    bool swapped = false;
    if (type == KUMA_DIST_CONTINUOUS && xLower <= 0.0L && xUpper >= 0.0L) {
        if (yLower <= 0.0L && yUpper >= 0.0L) return NULL;
        swapped = true;
    }

    // Deep copy both sources into the scheme parameters
    ProbabilityDistribution* xCopy = copyProbabilityDistribution(swapped ? y : x);
    ProbabilityDistribution* yCopy = copyProbabilityDistribution(swapped ? x : y);
    if (!xCopy || !yCopy) {
        freeProbabilityDistribution(xCopy);
        freeProbabilityDistribution(yCopy);
        return NULL;
    }
    BinaryDistributionParams* params = calloc(1, sizeof(BinaryDistributionParams));
    if (!params) {
        freeProbabilityDistribution(xCopy);
        freeProbabilityDistribution(yCopy);
        return NULL;
    }
    params->x = xCopy;
    params->y = yCopy;
    params->xLower = swapped ? yLower : xLower;
    params->xUpper = swapped ? yUpper : xUpper;
    params->yLower = swapped ? xLower : yLower;
    params->yUpper = swapped ? xUpper : yUpper;

    // Store finite product support when continuous support is finite
    if (type == KUMA_DIST_CONTINUOUS) {
        long double p1 = xLower * yLower;
        long double p2 = xLower * yUpper;
        long double p3 = xUpper * yLower;
        long double p4 = xUpper * yUpper;
        params->support.lower = constructNumberFromDouble(fminl(fminl(p1, p2), fminl(p3, p4)));
        params->support.upper = constructNumberFromDouble(fmaxl(fmaxl(p1, p2), fmaxl(p3, p4)));
    } else {
        params->support.lower = kumaNan();
        params->support.upper = kumaNan();
    }

    // Attach callbacks for the independent-product distribution scheme
    return constructProbabilityDistribution(type, KUMA_DIST_PRODUCT_INDEPENDENT, params, true,
                                            type == KUMA_DIST_DISCRETE ? productIndependentDistributionPMF : NULL,
                                            type == KUMA_DIST_CONTINUOUS ? productIndependentDistributionPDF : NULL,
                                            productIndependentDistributionCDF,
                                            productIndependentDistributionMean, productIndependentDistributionVariance,
                                            productIndependentDistributionStddev, productIndependentDistributionSample);
}

/* ---------- ProbabilityDistribution accessors ---------- */

// Evaluate a distribution PMF
Number probabilityPMF(ProbabilityDistribution* dist, Number x) {
    // Reject distributions without a mass callback
    if (!dist || !dist->pmf) return kumaNan();
    return dist->pmf(dist, x);
}

// Evaluate a distribution PDF
Number probabilityPDF(ProbabilityDistribution* dist, Number x) {
    // Reject distributions without a density callback
    if (!dist || !dist->pdf) return kumaNan();
    return dist->pdf(dist, x);
}

// Evaluate a distribution CDF
Number probabilityCDF(ProbabilityDistribution* dist, Number x) {
    // Reject distributions without a cumulative callback
    if (!dist || !dist->cdf) return kumaNan();
    return dist->cdf(dist, x);
}

// Evaluate a distribution mean
Number probabilityMean(ProbabilityDistribution* dist) {
    // Reject distributions without a mean callback
    if (!dist || !dist->mean) return kumaNan();
    return dist->mean(dist);
}

// Evaluate a distribution variance
Number probabilityVariance(ProbabilityDistribution* dist) {
    // Reject distributions without a variance callback
    if (!dist || !dist->variance) return kumaNan();
    return dist->variance(dist);
}

// Evaluate a distribution standard deviation
Number probabilityStddev(ProbabilityDistribution* dist) {
    // Use a distribution-specific callback when present
    if (!dist) return kumaNan();
    if (dist->stddev) return dist->stddev(dist);

    // Fall back to sqrt(variance) when only variance is available
    Number v = probabilityVariance(dist);
    long double x = 0.0L;
    if (!numberToLongDouble(v, &x) || x < 0.0L) return kumaNan();
    return constructNumberFromDouble(sqrtl(x));
}

// Sample from a distribution
Number probabilitySample(ProbabilityDistribution* dist) {
    // Reject distributions without a sampler callback
    if (!dist || !dist->sample) return kumaNan();
    return dist->sample(dist);
}

// Build a NEKO expression for a supported distribution PDF
NekoExpr* probabilityPDFfunc(ProbabilityDistribution* dist) {
    // Reject missing distributions before dispatching by kind
    if (!dist) return NULL;

    // Delegate to named continuous distributions with symbolic densities
    switch (dist->kind) {
        case KUMA_DIST_CONTINUOUS_UNIFORM:
            return continuousUniformPDFfunc(dist);
        case KUMA_DIST_NORMAL:
            return normalPDFfunc(dist);
        case KUMA_DIST_EXPONENTIAL:
            return exponentialPDFfunc(dist);
        case KUMA_DIST_BERNOULLI:
        case KUMA_DIST_BINOMIAL:
        case KUMA_DIST_GEOMETRIC:
        case KUMA_DIST_POISSON:
        case KUMA_DIST_DISCRETE_UNIFORM:
        case KUMA_DIST_CUSTOM:
        case KUMA_DIST_AFFINE:
        case KUMA_DIST_SUM_INDEPENDENT:
        case KUMA_DIST_PRODUCT_INDEPENDENT:
            return NULL;
    }

    return NULL;
}

// Build a NEKO expression for a supported distribution CDF
NekoExpr* probabilityCDFfunc(ProbabilityDistribution* dist) {
    // Reject missing distributions before dispatching by kind
    if (!dist) return NULL;

    // Delegate to named continuous distributions with symbolic cumulative functions
    switch (dist->kind) {
        case KUMA_DIST_CONTINUOUS_UNIFORM:
            return continuousUniformCDFfunc(dist);
        case KUMA_DIST_NORMAL:
            return normalCDFfunc(dist);
        case KUMA_DIST_EXPONENTIAL:
            return exponentialCDFfunc(dist);
        case KUMA_DIST_BERNOULLI:
        case KUMA_DIST_BINOMIAL:
        case KUMA_DIST_GEOMETRIC:
        case KUMA_DIST_POISSON:
        case KUMA_DIST_DISCRETE_UNIFORM:
        case KUMA_DIST_CUSTOM:
        case KUMA_DIST_AFFINE:
        case KUMA_DIST_SUM_INDEPENDENT:
        case KUMA_DIST_PRODUCT_INDEPENDENT:
            return NULL;
    }

    return NULL;
}

/* ---------- Named distribution functions ---------- */

/*
 * Bernoulli distribution
 *
 * Parameters:
 * p = probability of success, with 0 <= p <= 1
 *
 * PMF:
 * P(X = 0) = 1 - p
 * P(X = 1) = p
 *
 * CDF:
 * F(x) = 0 for x < 0
 * F(x) = 1 - p for 0 <= x < 1
 * F(x) = 1 for x >= 1
 *
 * Mean:
 * E[X] = p
 *
 * Variance:
 * Var(X) = p(1 - p)
 */

// Compute the Bernoulli probability mass function
Number bernoulliPMF(ProbabilityDistribution* dist, Number x) {
    // Reject invalid Bernoulli distributions and invalid probability values
    if (!dist || dist->type != KUMA_DIST_DISCRETE || dist->kind != KUMA_DIST_BERNOULLI || !dist->params) return kumaNan();
    BernoulliParams* params = (BernoulliParams*)dist->params;
    Number p = params->p;
    if (!isValidStatsNumber(p)) return kumaNan();
    if (compNumbers(p, constructNumberFromInt(0)) < 0 || compNumbers(p, constructNumberFromInt(1)) > 0) return kumaNan();

    // Return zero for any value outside the Bernoulli support
    if (!isValidStatsNumber(x)) return constructNumberFromInt(0);
    if (eqNumbers(x, constructNumberFromInt(1))) return p;
    if (!eqNumbers(x, constructNumberFromInt(0))) return constructNumberFromInt(0);

    // Return 1 - p at x = 0
    return subNumbers(constructNumberFromInt(1), p);
}

// Compute the Bernoulli cumulative distribution function
Number bernoulliCDF(ProbabilityDistribution* dist, Number x) {
    // Reject invalid Bernoulli distributions and invalid probability values
    if (!dist || dist->type != KUMA_DIST_DISCRETE || dist->kind != KUMA_DIST_BERNOULLI || !dist->params) return kumaNan();
    BernoulliParams* params = (BernoulliParams*)dist->params;
    Number p = params->p;
    if (!isValidStatsNumber(p) || !isValidStatsNumber(x)) return kumaNan();
    if (compNumbers(p, constructNumberFromInt(0)) < 0 || compNumbers(p, constructNumberFromInt(1)) > 0) return kumaNan();

    // Return the lower tail values by comparing x to the support points
    if (compNumbers(x, constructNumberFromInt(0)) < 0) return constructNumberFromInt(0);
    if (compNumbers(x, constructNumberFromInt(1)) < 0) return subNumbers(constructNumberFromInt(1), p);
    return constructNumberFromInt(1);
}

// Compute the Bernoulli mean
Number bernoulliMean(ProbabilityDistribution* dist) {
    // Reject invalid Bernoulli distributions and invalid probability values
    if (!dist || dist->type != KUMA_DIST_DISCRETE || dist->kind != KUMA_DIST_BERNOULLI || !dist->params) return kumaNan();
    BernoulliParams* params = (BernoulliParams*)dist->params;
    Number p = params->p;
    if (!isValidStatsNumber(p)) return kumaNan();
    if (compNumbers(p, constructNumberFromInt(0)) < 0 || compNumbers(p, constructNumberFromInt(1)) > 0) return kumaNan();

    // The mean of a Bernoulli random variable is p
    return p;
}

// Compute the Bernoulli variance
Number bernoulliVariance(ProbabilityDistribution* dist) {
    // Reject invalid Bernoulli distributions and invalid probability values
    if (!dist || dist->type != KUMA_DIST_DISCRETE || dist->kind != KUMA_DIST_BERNOULLI || !dist->params) return kumaNan();
    BernoulliParams* params = (BernoulliParams*)dist->params;
    Number p = params->p;
    if (!isValidStatsNumber(p)) return kumaNan();
    if (compNumbers(p, constructNumberFromInt(0)) < 0 || compNumbers(p, constructNumberFromInt(1)) > 0) return kumaNan();

    // Apply Var(X) = p(1 - p)
    Number q = subNumbers(constructNumberFromInt(1), p);
    if (q.type == NUMBER_NAN) return q;
    return multNumbers(p, q);
}

// Compute the Bernoulli standard deviation
Number bernoulliStddev(ProbabilityDistribution* dist) {
    // Compute the variance before converting to a real square root
    Number v = bernoulliVariance(dist);
    if (v.type == NUMBER_NAN) return v;

    // Convert the variance to a real radicand
    long double x = 0.0L;
    switch (v.type) {
        case NUMBER_INT:
            x = (long double)v.as.i;
            break;
        case NUMBER_FRACTION:
            x = (long double)v.as.frac.num / (long double)v.as.frac.denom;
            break;
        case NUMBER_REAL:
            x = v.as.x;
            break;
        case NUMBER_COMPLEX:
        case NUMBER_NAN:
            return kumaNan();
    }

    // Return sqrt(p(1 - p)) as a real value
    if (x < 0.0L) return kumaNan();
    return constructNumberFromDouble(sqrtl(x));
}

// Sample a Bernoulli random variable using the HEBI PRG
Number bernoulliSample(ProbabilityDistribution* dist) {
    // Reject invalid Bernoulli distributions and invalid probability values
    if (!dist || dist->type != KUMA_DIST_DISCRETE || dist->kind != KUMA_DIST_BERNOULLI || !dist->params) return kumaNan();
    BernoulliParams* params = (BernoulliParams*)dist->params;
    Number p = params->p;
    if (!isValidStatsNumber(p)) return kumaNan();
    if (compNumbers(p, constructNumberFromInt(0)) < 0 || compNumbers(p, constructNumberFromInt(1)) > 0) return kumaNan();

    // Convert p to a long double threshold
    long double threshold = 0.0L;
    switch (p.type) {
        case NUMBER_INT:
            threshold = (long double)p.as.i;
            break;
        case NUMBER_FRACTION:
            threshold = (long double)p.as.frac.num / (long double)p.as.frac.denom;
            break;
        case NUMBER_REAL:
            threshold = p.as.x;
            break;
        case NUMBER_COMPLEX:
        case NUMBER_NAN:
            return kumaNan();
    }

    // Compare a uniform random draw against p
    return randomReal(0.0L, 1.0L) < threshold ? constructNumberFromInt(1) : constructNumberFromInt(0);
}

/*
 * Binomial distribution
 *
 * Parameters:
 * n = number of independent Bernoulli trials, with n >= 0
 * p = probability of success on each trial, with 0 <= p <= 1
 *
 * PMF:
 * P(X = k) = C(n,k)p^k(1 - p)^(n-k) for k = 0, 1, ..., n
 *
 * CDF:
 * F(x) = sum_{k=0}^{floor(x)} C(n,k)p^k(1 - p)^(n-k)
 *
 * Mean:
 * E[X] = np
 *
 * Variance:
 * Var(X) = np(1 - p)
 */

// Compute the binomial probability mass function
Number binomialPMF(ProbabilityDistribution* dist, Number x) {
    // Reject invalid binomial distributions and invalid probabilities
    if (!dist || dist->type != KUMA_DIST_DISCRETE || dist->kind != KUMA_DIST_BINOMIAL || !dist->params) return kumaNan();
    BinomialParams* params = (BinomialParams*)dist->params;
    if (params->n < 0 || !isProbabilityNumber(params->p)) return kumaNan();

    // Return zero outside the integer support
    long long k = 0;
    if (!supportIndex(x, &k) || k > params->n) return constructNumberFromInt(0);

    // Build C(n,k)p^k(1-p)^(n-k)
    long long coeff = ncr(params->n, k);
    if (coeff == 0 && k >= 0 && k <= params->n) return kumaNan();
    Number left = powNumberNonnegative(params->p, k);
    Number q = subNumbers(constructNumberFromInt(1), params->p);
    Number right = powNumberNonnegative(q, params->n - k);
    Number prob = multNumbers(left, right);
    if (prob.type == NUMBER_NAN) return prob;
    return multNumbers(constructNumberFromInt(coeff), prob);
}

// Compute the binomial cumulative distribution function
Number binomialCDF(ProbabilityDistribution* dist, Number x) {
    // Reject invalid binomial distributions and invalid inputs
    if (!dist || dist->type != KUMA_DIST_DISCRETE || dist->kind != KUMA_DIST_BINOMIAL || !dist->params || !isValidStatsNumber(x)) return kumaNan();
    BinomialParams* params = (BinomialParams*)dist->params;
    if (params->n < 0 || !isProbabilityNumber(params->p)) return kumaNan();

    // Return the trivial lower and upper tail values
    long double xv = 0.0L;
    if (!numberToLongDouble(x, &xv)) return kumaNan();
    if (xv < 0.0L) return constructNumberFromInt(0);
    if (xv >= (long double)params->n) return constructNumberFromInt(1);

    // Sum the PMF through floor(x)
    long long last = (long long)floorl(xv);
    Number total = constructNumberFromInt(0);
    for (long long k = 0; k <= last; k++) {
        Number term = binomialPMF(dist, constructNumberFromInt(k));
        total = addNumbers(total, term);
        if (total.type == NUMBER_NAN) return total;
    }
    return total;
}

// Compute the binomial mean
Number binomialMean(ProbabilityDistribution* dist) {
    // Reject invalid binomial distributions and invalid probabilities
    if (!dist || dist->type != KUMA_DIST_DISCRETE || dist->kind != KUMA_DIST_BINOMIAL || !dist->params) return kumaNan();
    BinomialParams* params = (BinomialParams*)dist->params;
    if (params->n < 0 || !isProbabilityNumber(params->p)) return kumaNan();

    // Apply E[X] = np
    return multNumbers(constructNumberFromInt(params->n), params->p);
}

// Compute the binomial variance
Number binomialVariance(ProbabilityDistribution* dist) {
    // Reject invalid binomial distributions and invalid probabilities
    if (!dist || dist->type != KUMA_DIST_DISCRETE || dist->kind != KUMA_DIST_BINOMIAL || !dist->params) return kumaNan();
    BinomialParams* params = (BinomialParams*)dist->params;
    if (params->n < 0 || !isProbabilityNumber(params->p)) return kumaNan();

    // Apply Var(X) = np(1 - p)
    Number meanValue = binomialMean(dist);
    Number q = subNumbers(constructNumberFromInt(1), params->p);
    return multNumbers(meanValue, q);
}

// Compute the binomial standard deviation
Number binomialStddev(ProbabilityDistribution* dist) {
    // Compute the variance before converting to a real square root
    Number v = binomialVariance(dist);
    long double x = 0.0L;
    if (!numberToLongDouble(v, &x) || x < 0.0L) return kumaNan();
    return constructNumberFromDouble(sqrtl(x));
}

// Sample a binomial random variable using the HEBI PRG
Number binomialSample(ProbabilityDistribution* dist) {
    // Reject invalid binomial distributions and invalid probabilities
    if (!dist || dist->type != KUMA_DIST_DISCRETE || dist->kind != KUMA_DIST_BINOMIAL || !dist->params) return kumaNan();
    BinomialParams* params = (BinomialParams*)dist->params;
    long double p = 0.0L;
    if (params->n < 0 || !isProbabilityNumber(params->p) || !numberToLongDouble(params->p, &p)) return kumaNan();

    // Count successes across n independent uniform trials
    long long successes = 0;
    for (long long i = 0; i < params->n; i++) {
        if (randomReal(0.0L, 1.0L) < p) successes++;
    }
    return constructNumberFromInt(successes);
}

/*
 * Geometric distribution
 *
 * Parameters:
 * p = probability of success on each trial, with 0 < p <= 1
 *
 * PMF:
 * P(X = k) = (1 - p)^(k-1)p for k = 1, 2, ...
 *
 * CDF:
 * F(x) = 0 for x < 1
 * F(x) = 1 - (1 - p)^floor(x) for x >= 1
 *
 * Mean:
 * E[X] = 1/p
 *
 * Variance:
 * Var(X) = (1 - p)/p^2
 */

// Compute the geometric probability mass function
Number geometricPMF(ProbabilityDistribution* dist, Number x) {
    // Reject invalid geometric distributions and invalid probabilities
    if (!dist || dist->type != KUMA_DIST_DISCRETE || dist->kind != KUMA_DIST_GEOMETRIC || !dist->params) return kumaNan();
    GeometricParams* params = (GeometricParams*)dist->params;
    if (!isProbabilityNumber(params->p) || compNumbers(params->p, constructNumberFromInt(0)) <= 0) return kumaNan();

    // Return zero outside the positive integer support
    long long k = 0;
    if (!numberToLongLongExact(x, &k) || k < 1) return constructNumberFromInt(0);

    // Build (1-p)^(k-1)p
    Number q = subNumbers(constructNumberFromInt(1), params->p);
    Number tail = powNumberNonnegative(q, k - 1);
    return multNumbers(tail, params->p);
}

// Compute the geometric cumulative distribution function
Number geometricCDF(ProbabilityDistribution* dist, Number x) {
    // Reject invalid geometric distributions and invalid inputs
    if (!dist || dist->type != KUMA_DIST_DISCRETE || dist->kind != KUMA_DIST_GEOMETRIC || !dist->params || !isValidStatsNumber(x)) return kumaNan();
    GeometricParams* params = (GeometricParams*)dist->params;
    if (!isProbabilityNumber(params->p) || compNumbers(params->p, constructNumberFromInt(0)) <= 0) return kumaNan();

    // Return zero below the support
    long double xv = 0.0L;
    if (!numberToLongDouble(x, &xv)) return kumaNan();
    if (xv < 1.0L) return constructNumberFromInt(0);

    // Apply F(x) = 1 - (1-p)^floor(x)
    long long k = (long long)floorl(xv);
    Number q = subNumbers(constructNumberFromInt(1), params->p);
    Number tail = powNumberNonnegative(q, k);
    return subNumbers(constructNumberFromInt(1), tail);
}

// Compute the geometric mean
Number geometricMeanDist(ProbabilityDistribution* dist) {
    // Reject invalid geometric distributions and invalid probabilities
    if (!dist || dist->type != KUMA_DIST_DISCRETE || dist->kind != KUMA_DIST_GEOMETRIC || !dist->params) return kumaNan();
    GeometricParams* params = (GeometricParams*)dist->params;
    if (!isProbabilityNumber(params->p) || compNumbers(params->p, constructNumberFromInt(0)) <= 0) return kumaNan();

    // Apply E[X] = 1/p
    return divNumbers(constructNumberFromInt(1), params->p);
}

// Compute the geometric variance
Number geometricVariance(ProbabilityDistribution* dist) {
    // Reject invalid geometric distributions and invalid probabilities
    if (!dist || dist->type != KUMA_DIST_DISCRETE || dist->kind != KUMA_DIST_GEOMETRIC || !dist->params) return kumaNan();
    GeometricParams* params = (GeometricParams*)dist->params;
    if (!isProbabilityNumber(params->p) || compNumbers(params->p, constructNumberFromInt(0)) <= 0) return kumaNan();

    // Apply Var(X) = (1-p)/p^2
    Number q = subNumbers(constructNumberFromInt(1), params->p);
    Number p2 = multNumbers(params->p, params->p);
    return divNumbers(q, p2);
}

// Compute the geometric standard deviation
Number geometricStddev(ProbabilityDistribution* dist) {
    // Compute the variance before converting to a real square root
    Number v = geometricVariance(dist);
    long double x = 0.0L;
    if (!numberToLongDouble(v, &x) || x < 0.0L) return kumaNan();
    return constructNumberFromDouble(sqrtl(x));
}

// Sample a geometric random variable using the HEBI PRG
Number geometricSample(ProbabilityDistribution* dist) {
    // Reject invalid geometric distributions and invalid probabilities
    if (!dist || dist->type != KUMA_DIST_DISCRETE || dist->kind != KUMA_DIST_GEOMETRIC || !dist->params) return kumaNan();
    GeometricParams* params = (GeometricParams*)dist->params;
    long double p = 0.0L;
    if (!isProbabilityNumber(params->p) || compNumbers(params->p, constructNumberFromInt(0)) <= 0 || !numberToLongDouble(params->p, &p)) return kumaNan();

    // Count trials until the first success
    long long trials = 1;
    while (randomReal(0.0L, 1.0L) >= p) trials++;
    return constructNumberFromInt(trials);
}

/*
 * Poisson distribution
 *
 * Parameters:
 * lambda = positive event rate, with lambda > 0
 *
 * PMF:
 * P(X = k) = exp(-lambda)lambda^k/k! for k = 0, 1, ...
 *
 * CDF:
 * F(x) = sum_{k=0}^{floor(x)} exp(-lambda)lambda^k/k!
 *
 * Mean:
 * E[X] = lambda
 *
 * Variance:
 * Var(X) = lambda
 */

// Compute the Poisson probability mass function
Number poissonPMF(ProbabilityDistribution* dist, Number x) {
    // Reject invalid Poisson distributions and invalid rates
    if (!dist || dist->type != KUMA_DIST_DISCRETE || dist->kind != KUMA_DIST_POISSON || !dist->params) return kumaNan();
    PoissonParams* params = (PoissonParams*)dist->params;
    long double lambda = 0.0L;
    if (!isPositiveNumber(params->lambda) || !numberToLongDouble(params->lambda, &lambda)) return kumaNan();

    // Return zero outside the nonnegative integer support
    long long k = 0;
    if (!supportIndex(x, &k)) return constructNumberFromInt(0);

    // Build exp(-lambda)lambda^k/k! by recurrence
    long double p = expl(-lambda);
    for (long long i = 1; i <= k; i++) p *= lambda / (long double)i;
    return isfinite(p) ? constructNumberFromDouble(p) : kumaNan();
}

// Compute the Poisson cumulative distribution function
Number poissonCDF(ProbabilityDistribution* dist, Number x) {
    // Reject invalid Poisson distributions and invalid inputs
    if (!dist || dist->type != KUMA_DIST_DISCRETE || dist->kind != KUMA_DIST_POISSON || !dist->params || !isValidStatsNumber(x)) return kumaNan();
    PoissonParams* params = (PoissonParams*)dist->params;
    long double lambda = 0.0L;
    long double xv = 0.0L;
    if (!isPositiveNumber(params->lambda) || !numberToLongDouble(params->lambda, &lambda) || !numberToLongDouble(x, &xv)) return kumaNan();

    // Return zero below the support
    if (xv < 0.0L) return constructNumberFromInt(0);

    // Sum the recurrent PMF through floor(x)
    long long last = (long long)floorl(xv);
    long double term = expl(-lambda);
    long double total = term;
    for (long long k = 1; k <= last; k++) {
        term *= lambda / (long double)k;
        total += term;
    }
    return isfinite(total) ? constructNumberFromDouble(total > 1.0L ? 1.0L : total) : kumaNan();
}

// Compute the Poisson mean
Number poissonMean(ProbabilityDistribution* dist) {
    // Reject invalid Poisson distributions and invalid rates
    if (!dist || dist->type != KUMA_DIST_DISCRETE || dist->kind != KUMA_DIST_POISSON || !dist->params) return kumaNan();
    PoissonParams* params = (PoissonParams*)dist->params;
    if (!isPositiveNumber(params->lambda)) return kumaNan();
    return params->lambda;
}

// Compute the Poisson variance
Number poissonVariance(ProbabilityDistribution* dist) {
    // The variance of a Poisson random variable is lambda
    return poissonMean(dist);
}

// Compute the Poisson standard deviation
Number poissonStddev(ProbabilityDistribution* dist) {
    // Compute the variance before converting to a real square root
    Number v = poissonVariance(dist);
    long double x = 0.0L;
    if (!numberToLongDouble(v, &x) || x < 0.0L) return kumaNan();
    return constructNumberFromDouble(sqrtl(x));
}

// Sample a Poisson random variable using the HEBI PRG
Number poissonSample(ProbabilityDistribution* dist) {
    // Reject invalid Poisson distributions and invalid rates
    if (!dist || dist->type != KUMA_DIST_DISCRETE || dist->kind != KUMA_DIST_POISSON || !dist->params) return kumaNan();
    PoissonParams* params = (PoissonParams*)dist->params;
    long double lambda = 0.0L;
    if (!isPositiveNumber(params->lambda) || !numberToLongDouble(params->lambda, &lambda)) return kumaNan();

    // Use inverse-transform sampling from the recurrent CDF
    long double u = randomReal(0.0L, 1.0L);
    long double term = expl(-lambda);
    long double total = term;
    long long k = 0;
    while (u > total && k < 1000000) {
        k++;
        term *= lambda / (long double)k;
        total += term;
    }
    return k < 1000000 ? constructNumberFromInt(k) : kumaNan();
}

/*
 * Discrete uniform distribution
 *
 * Parameters:
 * a = lower integer endpoint
 * b = upper integer endpoint, with a <= b
 *
 * PMF:
 * P(X = k) = 1/(b - a + 1) for k = a, a+1, ..., b
 *
 * CDF:
 * F(x) = 0 for x < a
 * F(x) = (floor(x) - a + 1)/(b - a + 1) for a <= x < b
 * F(x) = 1 for x >= b
 *
 * Mean:
 * E[X] = (a + b)/2
 *
 * Variance:
 * Var(X) = ((b - a + 1)^2 - 1)/12
 */

// Compute the discrete uniform probability mass function
Number discreteUniformPMF(ProbabilityDistribution* dist, Number x) {
    // Reject invalid discrete uniform distributions
    if (!dist || dist->type != KUMA_DIST_DISCRETE || dist->kind != KUMA_DIST_DISCRETE_UNIFORM || !dist->params) return kumaNan();
    DiscreteUniformParams* params = (DiscreteUniformParams*)dist->params;
    long long width = 0;
    if (!discreteUniformWidth(params->a, params->b, &width)) return kumaNan();

    // Return zero outside the integer support
    long long k = 0;
    if (!numberToLongLongExact(x, &k) || k < params->a || k > params->b) return constructNumberFromInt(0);

    // Return the reciprocal of the support size
    return divNumbers(constructNumberFromInt(1), constructNumberFromInt(width));
}

// Compute the discrete uniform cumulative distribution function
Number discreteUniformCDF(ProbabilityDistribution* dist, Number x) {
    // Reject invalid discrete uniform distributions and invalid inputs
    if (!dist || dist->type != KUMA_DIST_DISCRETE || dist->kind != KUMA_DIST_DISCRETE_UNIFORM || !dist->params || !isValidStatsNumber(x)) return kumaNan();
    DiscreteUniformParams* params = (DiscreteUniformParams*)dist->params;
    long double xv = 0.0L;
    long long width = 0;
    if (!discreteUniformWidth(params->a, params->b, &width) || !numberToLongDouble(x, &xv)) return kumaNan();

    // Return the lower and upper tail values
    if (xv < (long double)params->a) return constructNumberFromInt(0);
    if (xv >= (long double)params->b) return constructNumberFromInt(1);

    // Count support points at or below floor(x)
    long long k = (long long)floorl(xv);
    return divNumbers(constructNumberFromInt(k - params->a + 1), constructNumberFromInt(width));
}

// Compute the discrete uniform mean
Number discreteUniformMean(ProbabilityDistribution* dist) {
    // Reject invalid discrete uniform distributions
    if (!dist || dist->type != KUMA_DIST_DISCRETE || dist->kind != KUMA_DIST_DISCRETE_UNIFORM || !dist->params) return kumaNan();
    DiscreteUniformParams* params = (DiscreteUniformParams*)dist->params;
    long long width = 0;
    if (!discreteUniformWidth(params->a, params->b, &width)) return kumaNan();

    // Apply E[X] = (a+b)/2
    return divNumbers(addNumbers(constructNumberFromInt(params->a), constructNumberFromInt(params->b)), constructNumberFromInt(2));
}

// Compute the discrete uniform variance
Number discreteUniformVariance(ProbabilityDistribution* dist) {
    // Reject invalid discrete uniform distributions
    if (!dist || dist->type != KUMA_DIST_DISCRETE || dist->kind != KUMA_DIST_DISCRETE_UNIFORM || !dist->params) return kumaNan();
    DiscreteUniformParams* params = (DiscreteUniformParams*)dist->params;
    long long n = 0;
    if (!discreteUniformWidth(params->a, params->b, &n)) return kumaNan();

    // Apply Var(X) = (n^2 - 1)/12
    Number n2 = multNumbers(constructNumberFromInt(n), constructNumberFromInt(n));
    return divNumbers(subNumbers(n2, constructNumberFromInt(1)), constructNumberFromInt(12));
}

// Compute the discrete uniform standard deviation
Number discreteUniformStddev(ProbabilityDistribution* dist) {
    // Compute the variance before converting to a real square root
    Number v = discreteUniformVariance(dist);
    long double x = 0.0L;
    if (!numberToLongDouble(v, &x) || x < 0.0L) return kumaNan();
    return constructNumberFromDouble(sqrtl(x));
}

// Sample a discrete uniform random variable using the HEBI PRG
Number discreteUniformSample(ProbabilityDistribution* dist) {
    // Reject invalid discrete uniform distributions
    if (!dist || dist->type != KUMA_DIST_DISCRETE || dist->kind != KUMA_DIST_DISCRETE_UNIFORM || !dist->params) return kumaNan();
    DiscreteUniformParams* params = (DiscreteUniformParams*)dist->params;
    if (params->a > params->b) return kumaNan();

    // Draw uniformly from the inclusive integer interval
    return constructNumberFromInt(randomInt(params->a, params->b));
}

/*
 * Continuous uniform distribution
 *
 * Parameters:
 * a = lower real endpoint
 * b = upper real endpoint, with a < b
 *
 * PDF:
 * f(x) = 1/(b - a) for a <= x < b
 * f(x) = 0 otherwise
 *
 * CDF:
 * F(x) = 0 for x < a
 * F(x) = (x - a)/(b - a) for a <= x < b
 * F(x) = 1 for x >= b
 *
 * Mean:
 * E[X] = (a + b)/2
 *
 * Variance:
 * Var(X) = (b - a)^2/12
 */

// Compute the continuous uniform probability density function
Number continuousUniformPDF(ProbabilityDistribution* dist, Number x) {
    // Reject invalid continuous uniform distributions and invalid inputs
    long double a = 0.0L;
    long double b = 0.0L;
    long double xv = 0.0L;
    if (!getContinuousUniformParams(dist, &a, &b) || !numberToLongDouble(x, &xv)) return kumaNan();

    // Return the constant density on the half-open support representative
    if (xv < a || xv >= b) return constructNumberFromInt(0);
    return constructNumberFromDouble(1.0L / (b - a));
}

// Compute the continuous uniform cumulative distribution function
Number continuousUniformCDF(ProbabilityDistribution* dist, Number x) {
    // Reject invalid continuous uniform distributions and invalid inputs
    long double a = 0.0L;
    long double b = 0.0L;
    long double xv = 0.0L;
    if (!getContinuousUniformParams(dist, &a, &b) || !numberToLongDouble(x, &xv)) return kumaNan();

    // Return the piecewise cumulative probability
    if (xv < a) return constructNumberFromInt(0);
    if (xv >= b) return constructNumberFromInt(1);
    return constructNumberFromDouble((xv - a) / (b - a));
}

// Build a NEKO expression for the continuous uniform density
NekoExpr* continuousUniformPDFfunc(ProbabilityDistribution* dist) {
    // Reject invalid continuous uniform distributions
    long double a = 0.0L;
    long double b = 0.0L;
    if (!getContinuousUniformParams(dist, &a, &b)) return NULL;

    // Build (step(x-a)-step(x-b))/(b-a)
    return nekoSimplify(nekoDiv(
        nekoSub(nekoStep(nekoSub(nekoVar("x"), nekoConst(a))),
                nekoStep(nekoSub(nekoVar("x"), nekoConst(b)))),
        nekoConst(b - a)
    ));
}

// Build a NEKO expression for the continuous uniform cumulative distribution
NekoExpr* continuousUniformCDFfunc(ProbabilityDistribution* dist) {
    // Reject invalid continuous uniform distributions
    long double a = 0.0L;
    long double b = 0.0L;
    if (!getContinuousUniformParams(dist, &a, &b)) return NULL;

    // Build ((x-a)/(b-a))*(step(x-a)-step(x-b)) + step(x-b)
    NekoExpr* scaled = nekoDiv(nekoSub(nekoVar("x"), nekoConst(a)), nekoConst(b - a));
    NekoExpr* window = nekoSub(nekoStep(nekoSub(nekoVar("x"), nekoConst(a))),
                               nekoStep(nekoSub(nekoVar("x"), nekoConst(b))));
    NekoExpr* upper = nekoStep(nekoSub(nekoVar("x"), nekoConst(b)));
    return nekoSimplify(nekoAdd(nekoMul(scaled, window), upper));
}

// Compute the continuous uniform mean
Number continuousUniformMean(ProbabilityDistribution* dist) {
    // Reject invalid continuous uniform distributions
    long double a = 0.0L;
    long double b = 0.0L;
    if (!getContinuousUniformParams(dist, &a, &b)) return kumaNan();

    // Apply E[X] = (a+b)/2
    return constructNumberFromDouble((a + b) / 2.0L);
}

// Compute the continuous uniform variance
Number continuousUniformVariance(ProbabilityDistribution* dist) {
    // Reject invalid continuous uniform distributions
    long double a = 0.0L;
    long double b = 0.0L;
    if (!getContinuousUniformParams(dist, &a, &b)) return kumaNan();

    // Apply Var(X) = (b-a)^2/12
    long double width = b - a;
    return constructNumberFromDouble(width * width / 12.0L);
}

// Compute the continuous uniform standard deviation
Number continuousUniformStddev(ProbabilityDistribution* dist) {
    // Compute the variance before converting to a real square root
    Number v = continuousUniformVariance(dist);
    long double x = 0.0L;
    if (!numberToLongDouble(v, &x) || x < 0.0L) return kumaNan();
    return constructNumberFromDouble(sqrtl(x));
}

// Sample a continuous uniform random variable using the HEBI PRG
Number continuousUniformSample(ProbabilityDistribution* dist) {
    // Reject invalid continuous uniform distributions
    long double a = 0.0L;
    long double b = 0.0L;
    if (!getContinuousUniformParams(dist, &a, &b)) return kumaNan();

    // Draw uniformly from the real interval
    return constructNumberFromDouble(randomReal(a, b));
}

/*
 * Normal distribution
 *
 * Parameters:
 * mu = real mean
 * sigma = positive real standard deviation, with sigma > 0
 *
 * PDF:
 * f(x) = exp(-((x - mu)^2)/(2sigma^2))/(sigma sqrt(2pi))
 *
 * CDF:
 * F(x) = (1 + erf((x - mu)/(sigma sqrt(2))))/2
 *
 * Mean:
 * E[X] = mu
 *
 * Variance:
 * Var(X) = sigma^2
 */

// Compute the normal probability density function
Number normalPDF(ProbabilityDistribution* dist, Number x) {
    // Reject invalid normal distributions and invalid inputs
    long double mu = 0.0L;
    long double sigma = 0.0L;
    long double xv = 0.0L;
    if (!getNormalParams(dist, &mu, &sigma) || !numberToLongDouble(x, &xv)) return kumaNan();

    // Apply the normal density formula
    long double z = (xv - mu) / sigma;
    long double density = expl(-0.5L * z * z) / (sigma * sqrtl(2.0L * M_PI));
    return isfinite(density) ? constructNumberFromDouble(density) : kumaNan();
}

// Compute the normal cumulative distribution function
Number normalCDF(ProbabilityDistribution* dist, Number x) {
    // Reject invalid normal distributions and invalid inputs
    long double mu = 0.0L;
    long double sigma = 0.0L;
    long double xv = 0.0L;
    if (!getNormalParams(dist, &mu, &sigma) || !numberToLongDouble(x, &xv)) return kumaNan();

    // Apply the error-function form of the normal CDF
    long double z = (xv - mu) / (sigma * sqrtl(2.0L));
    return constructNumberFromDouble((1.0L + realErf(z)) / 2.0L);
}

// Build a NEKO expression for the normal density
NekoExpr* normalPDFfunc(ProbabilityDistribution* dist) {
    // Reject invalid normal distributions
    long double mu = 0.0L;
    long double sigma = 0.0L;
    if (!getNormalParams(dist, &mu, &sigma)) return NULL;

    // Build exp(-((x-mu)^2)/(2sigma^2))/(sigma sqrt(2pi))
    NekoExpr* diff = nekoSub(nekoVar("x"), nekoConst(mu));
    NekoExpr* exponent = nekoNeg(nekoDiv(nekoPow(diff, nekoConst(2.0L)), nekoConst(2.0L * sigma * sigma)));
    return nekoSimplify(nekoMul(nekoConst(1.0L / (sigma * sqrtl(2.0L * M_PI))), nekoExp(exponent)));
}

// Build a NEKO expression for the normal cumulative distribution
NekoExpr* normalCDFfunc(ProbabilityDistribution* dist) {
    // Reject invalid normal distributions
    long double mu = 0.0L;
    long double sigma = 0.0L;
    if (!getNormalParams(dist, &mu, &sigma)) return NULL;

    // Build (1 + erf((x-mu)/(sigma sqrt(2))))/2
    NekoExpr* z = nekoDiv(nekoSub(nekoVar("x"), nekoConst(mu)), nekoConst(sigma * sqrtl(2.0L)));
    return nekoSimplify(nekoDiv(nekoAdd(nekoConst(1.0L), nekoErf(z)), nekoConst(2.0L)));
}

// Compute the normal mean
Number normalMean(ProbabilityDistribution* dist) {
    // Reject invalid normal distributions
    if (!dist || dist->type != KUMA_DIST_CONTINUOUS || dist->kind != KUMA_DIST_NORMAL || !dist->params) return kumaNan();
    NormalParams* params = (NormalParams*)dist->params;
    long double mu = 0.0L;
    long double sigma = 0.0L;
    if (!numberToLongDouble(params->mu, &mu) || !numberToLongDouble(params->sigma, &sigma) || sigma <= 0.0L) return kumaNan();
    return params->mu;
}

// Compute the normal variance
Number normalVariance(ProbabilityDistribution* dist) {
    // Reject invalid normal distributions
    if (!dist || dist->type != KUMA_DIST_CONTINUOUS || dist->kind != KUMA_DIST_NORMAL || !dist->params) return kumaNan();
    NormalParams* params = (NormalParams*)dist->params;
    long double mu = 0.0L;
    long double sigma = 0.0L;
    if (!numberToLongDouble(params->mu, &mu) || !numberToLongDouble(params->sigma, &sigma) || sigma <= 0.0L) return kumaNan();

    // Apply Var(X) = sigma^2 with HEBI arithmetic
    return multNumbers(params->sigma, params->sigma);
}

// Compute the normal standard deviation
Number normalStddev(ProbabilityDistribution* dist) {
    // The standard deviation parameter is sigma itself
    if (!dist || dist->type != KUMA_DIST_CONTINUOUS || dist->kind != KUMA_DIST_NORMAL || !dist->params) return kumaNan();
    NormalParams* params = (NormalParams*)dist->params;
    long double mu = 0.0L;
    long double sigma = 0.0L;
    if (!numberToLongDouble(params->mu, &mu) || !numberToLongDouble(params->sigma, &sigma) || sigma <= 0.0L) return kumaNan();
    return params->sigma;
}

// Sample a normal random variable using the HEBI PRG
Number normalSample(ProbabilityDistribution* dist) {
    // Reject invalid normal distributions
    long double mu = 0.0L;
    long double sigma = 0.0L;
    if (!getNormalParams(dist, &mu, &sigma)) return kumaNan();

    // Apply the Box-Muller transform
    long double u1 = randomReal(0.0L, 1.0L);
    long double u2 = randomReal(0.0L, 1.0L);
    if (u1 <= 0.0L) u1 = 1e-18L;
    long double z = sqrtl(-2.0L * logl(u1)) * cosl(2.0L * M_PI * u2);
    return constructNumberFromDouble(mu + sigma * z);
}

/*
 * Exponential distribution
 *
 * Parameters:
 * lambda = positive real rate, with lambda > 0
 *
 * PDF:
 * f(x) = lambda exp(-lambda x) for x >= 0
 * f(x) = 0 for x < 0
 *
 * CDF:
 * F(x) = 1 - exp(-lambda x) for x >= 0
 * F(x) = 0 for x < 0
 *
 * Mean:
 * E[X] = 1/lambda
 *
 * Variance:
 * Var(X) = 1/lambda^2
 */

// Compute the exponential probability density function
Number exponentialPDF(ProbabilityDistribution* dist, Number x) {
    // Reject invalid exponential distributions and invalid inputs
    long double lambda = 0.0L;
    long double xv = 0.0L;
    if (!getExponentialParams(dist, &lambda) || !numberToLongDouble(x, &xv)) return kumaNan();

    // Apply the one-sided exponential density
    if (xv < 0.0L) return constructNumberFromInt(0);
    return constructNumberFromDouble(lambda * expl(-lambda * xv));
}

// Compute the exponential cumulative distribution function
Number exponentialCDF(ProbabilityDistribution* dist, Number x) {
    // Reject invalid exponential distributions and invalid inputs
    long double lambda = 0.0L;
    long double xv = 0.0L;
    if (!getExponentialParams(dist, &lambda) || !numberToLongDouble(x, &xv)) return kumaNan();

    // Apply the one-sided exponential CDF
    if (xv < 0.0L) return constructNumberFromInt(0);
    return constructNumberFromDouble(1.0L - expl(-lambda * xv));
}

// Build a NEKO expression for the exponential density
NekoExpr* exponentialPDFfunc(ProbabilityDistribution* dist) {
    // Reject invalid exponential distributions
    long double lambda = 0.0L;
    if (!getExponentialParams(dist, &lambda)) return NULL;

    // Build lambda*exp(-lambda*x)*step(x)
    NekoExpr* core = nekoMul(nekoConst(lambda), nekoExp(nekoMul(nekoConst(-lambda), nekoVar("x"))));
    return nekoSimplify(nekoMul(core, nekoStep(nekoVar("x"))));
}

// Build a NEKO expression for the exponential cumulative distribution
NekoExpr* exponentialCDFfunc(ProbabilityDistribution* dist) {
    // Reject invalid exponential distributions
    long double lambda = 0.0L;
    if (!getExponentialParams(dist, &lambda)) return NULL;

    // Build (1-exp(-lambda*x))*step(x)
    NekoExpr* core = nekoSub(nekoConst(1.0L), nekoExp(nekoMul(nekoConst(-lambda), nekoVar("x"))));
    return nekoSimplify(nekoMul(core, nekoStep(nekoVar("x"))));
}

// Compute the exponential mean
Number exponentialMean(ProbabilityDistribution* dist) {
    // Reject invalid exponential distributions
    if (!dist || dist->type != KUMA_DIST_CONTINUOUS || dist->kind != KUMA_DIST_EXPONENTIAL || !dist->params) return kumaNan();
    ExponentialParams* params = (ExponentialParams*)dist->params;
    if (!isPositiveNumber(params->lambda)) return kumaNan();

    // Apply E[X] = 1/lambda
    return divNumbers(constructNumberFromInt(1), params->lambda);
}

// Compute the exponential variance
Number exponentialVariance(ProbabilityDistribution* dist) {
    // Reject invalid exponential distributions
    if (!dist || dist->type != KUMA_DIST_CONTINUOUS || dist->kind != KUMA_DIST_EXPONENTIAL || !dist->params) return kumaNan();
    ExponentialParams* params = (ExponentialParams*)dist->params;
    if (!isPositiveNumber(params->lambda)) return kumaNan();

    // Apply Var(X) = 1/lambda^2
    Number lambda2 = multNumbers(params->lambda, params->lambda);
    return divNumbers(constructNumberFromInt(1), lambda2);
}

// Compute the exponential standard deviation
Number exponentialStddev(ProbabilityDistribution* dist) {
    // The standard deviation of an exponential random variable is 1/lambda
    return exponentialMean(dist);
}

// Sample an exponential random variable using the HEBI PRG
Number exponentialSample(ProbabilityDistribution* dist) {
    // Reject invalid exponential distributions
    long double lambda = 0.0L;
    if (!getExponentialParams(dist, &lambda)) return kumaNan();

    // Apply inverse-transform sampling
    long double u = randomReal(0.0L, 1.0L);
    if (u >= 1.0L) u = 1.0L - 1e-18L;
    return constructNumberFromDouble(-logl(1.0L - u) / lambda);
}

/*
 * Custom discrete distribution
 *
 * Parameters:
 * values = finite support values
 * probabilities = matching probability masses
 * size = number of support values
 *
 * PMF:
 * P(X = x) = sum of probabilities attached to support values equal to x
 *
 * CDF:
 * F(x) = sum of probabilities attached to support values <= x
 *
 * Mean:
 * E[X] = sum values[i] probabilities[i]
 *
 * Variance:
 * Var(X) = sum (values[i] - E[X])^2 probabilities[i]
 */

// Compute the custom discrete probability mass function
Number customDiscretePMF(ProbabilityDistribution* dist, Number x) {
    // Reject invalid custom discrete distributions and invalid inputs
    if (!dist || dist->type != KUMA_DIST_DISCRETE || dist->kind != KUMA_DIST_CUSTOM || !dist->params || !isValidStatsNumber(x)) return kumaNan();
    CustomDiscreteParams* params = (CustomDiscreteParams*)dist->params;
    if (!isValidStatsArray(params->values, params->size) || !isValidProbabilityArray(params->probabilities, params->size)) return kumaNan();

    // Sum all masses attached to the requested value
    Number total = constructNumberFromInt(0);
    for (size_t i = 0; i < params->size; i++) {
        if (eqNumbers(params->values[i], x)) total = addNumbers(total, params->probabilities[i]);
    }
    return total;
}

// Compute the custom discrete cumulative distribution function
Number customDiscreteCDF(ProbabilityDistribution* dist, Number x) {
    // Reject invalid custom discrete distributions and invalid inputs
    if (!dist || dist->type != KUMA_DIST_DISCRETE || dist->kind != KUMA_DIST_CUSTOM || !dist->params || !isValidStatsNumber(x)) return kumaNan();
    CustomDiscreteParams* params = (CustomDiscreteParams*)dist->params;
    if (!isValidStatsArray(params->values, params->size) || !isValidProbabilityArray(params->probabilities, params->size)) return kumaNan();

    // Sum all masses attached to support values at or below x
    Number total = constructNumberFromInt(0);
    for (size_t i = 0; i < params->size; i++) {
        if (compNumbers(params->values[i], x) <= 0) total = addNumbers(total, params->probabilities[i]);
    }
    return total;
}

// Compute the custom discrete mean
Number customDiscreteMean(ProbabilityDistribution* dist) {
    // Reject invalid custom discrete distributions
    if (!dist || dist->type != KUMA_DIST_DISCRETE || dist->kind != KUMA_DIST_CUSTOM || !dist->params) return kumaNan();
    CustomDiscreteParams* params = (CustomDiscreteParams*)dist->params;
    if (!isValidStatsArray(params->values, params->size) || !isValidProbabilityArray(params->probabilities, params->size)) return kumaNan();

    // Sum value-probability products
    Number total = constructNumberFromInt(0);
    for (size_t i = 0; i < params->size; i++) {
        total = addNumbers(total, multNumbers(params->values[i], params->probabilities[i]));
    }
    return total;
}

// Compute the custom discrete variance
Number customDiscreteVariance(ProbabilityDistribution* dist) {
    // Reject invalid custom discrete distributions
    if (!dist || dist->type != KUMA_DIST_DISCRETE || dist->kind != KUMA_DIST_CUSTOM || !dist->params) return kumaNan();
    CustomDiscreteParams* params = (CustomDiscreteParams*)dist->params;
    if (!isValidStatsArray(params->values, params->size) || !isValidProbabilityArray(params->probabilities, params->size)) return kumaNan();

    // Sum squared deviations weighted by probability
    Number mu = customDiscreteMean(dist);
    Number total = constructNumberFromInt(0);
    for (size_t i = 0; i < params->size; i++) {
        Number diff = subNumbers(params->values[i], mu);
        Number sq = multNumbers(diff, diff);
        total = addNumbers(total, multNumbers(sq, params->probabilities[i]));
    }
    return total;
}

// Compute the custom discrete standard deviation
Number customDiscreteStddev(ProbabilityDistribution* dist) {
    // Compute the variance before converting to a real square root
    Number v = customDiscreteVariance(dist);
    long double x = 0.0L;
    if (!numberToLongDouble(v, &x) || x < 0.0L) return kumaNan();
    return constructNumberFromDouble(sqrtl(x));
}

// Sample a custom discrete random variable using the HEBI PRG
Number customDiscreteSample(ProbabilityDistribution* dist) {
    // Reject invalid custom discrete distributions
    if (!dist || dist->type != KUMA_DIST_DISCRETE || dist->kind != KUMA_DIST_CUSTOM || !dist->params) return kumaNan();
    CustomDiscreteParams* params = (CustomDiscreteParams*)dist->params;
    if (!isValidStatsArray(params->values, params->size) || !isValidProbabilityArray(params->probabilities, params->size)) return kumaNan();

    // Walk the cumulative masses until the random draw is reached
    long double u = randomReal(0.0L, 1.0L);
    long double total = 0.0L;
    for (size_t i = 0; i < params->size; i++) {
        long double p = 0.0L;
        if (!numberToLongDouble(params->probabilities[i], &p)) return kumaNan();
        total += p;
        if (u <= total) return params->values[i];
    }
    return params->values[params->size - 1];
}

/*
 * Custom continuous distribution
 *
 * Parameters:
 * lower = lower support endpoint
 * upper = upper support endpoint
 * pdf, cdf, mean, variance, and sample are supplied by function pointers
 *
 * PDF:
 * f(x) is delegated to the supplied density function
 *
 * CDF:
 * F(x) is delegated to the supplied cumulative distribution function
 *
 * Mean:
 * E[X] is delegated to the supplied mean function
 *
 * Variance:
 * Var(X) is delegated to the supplied variance function
 */

// Compute the custom continuous probability density function
Number customContinuousPDF(ProbabilityDistribution* dist, Number x) {
    // Delegate to the supplied density callback when present
    if (!dist || dist->type != KUMA_DIST_CONTINUOUS || dist->kind != KUMA_DIST_CUSTOM || !dist->pdf || dist->pdf == customContinuousPDF) return kumaNan();
    return dist->pdf(dist, x);
}

// Compute the custom continuous cumulative distribution function
Number customContinuousCDF(ProbabilityDistribution* dist, Number x) {
    // Delegate to the supplied cumulative callback when present
    if (!dist || dist->type != KUMA_DIST_CONTINUOUS || dist->kind != KUMA_DIST_CUSTOM || !dist->cdf || dist->cdf == customContinuousCDF) return kumaNan();
    return dist->cdf(dist, x);
}

// Compute the custom continuous mean
Number customContinuousMean(ProbabilityDistribution* dist) {
    // Delegate to the supplied mean callback when present
    if (!dist || dist->type != KUMA_DIST_CONTINUOUS || dist->kind != KUMA_DIST_CUSTOM || !dist->mean || dist->mean == customContinuousMean) return kumaNan();
    return dist->mean(dist);
}

// Compute the custom continuous variance
Number customContinuousVariance(ProbabilityDistribution* dist) {
    // Delegate to the supplied variance callback when present
    if (!dist || dist->type != KUMA_DIST_CONTINUOUS || dist->kind != KUMA_DIST_CUSTOM || !dist->variance || dist->variance == customContinuousVariance) return kumaNan();
    return dist->variance(dist);
}

// Compute the custom continuous standard deviation
Number customContinuousStddev(ProbabilityDistribution* dist) {
    // Prefer a supplied standard deviation callback when present
    if (!dist || dist->type != KUMA_DIST_CONTINUOUS || dist->kind != KUMA_DIST_CUSTOM) return kumaNan();
    if (dist->stddev && dist->stddev != customContinuousStddev) return dist->stddev(dist);

    // Otherwise compute sqrt(variance) from the supplied variance callback
    Number v = customContinuousVariance(dist);
    long double x = 0.0L;
    if (!numberToLongDouble(v, &x) || x < 0.0L) return kumaNan();
    return constructNumberFromDouble(sqrtl(x));
}

// Sample a custom continuous random variable
Number customContinuousSample(ProbabilityDistribution* dist) {
    // Delegate to the supplied sampler callback when present
    if (!dist || dist->type != KUMA_DIST_CONTINUOUS || dist->kind != KUMA_DIST_CUSTOM || !dist->sample || dist->sample == customContinuousSample) return kumaNan();
    return dist->sample(dist);
}

// Evaluate a distribution PDF as a long double
static long double pdfValue(ProbabilityDistribution* dist, long double x) {
    // Convert the callback result to a finite real value
    long double value = 0.0L;
    Number result = probabilityPDF(dist, constructNumberFromDouble(x));
    return numberToLongDouble(result, &value) ? value : NAN;
}

// Evaluate a distribution CDF as a long double
static long double cdfValue(ProbabilityDistribution* dist, long double x) {
    // Convert the callback result to a finite real value
    long double value = 0.0L;
    Number result = probabilityCDF(dist, constructNumberFromDouble(x));
    return numberToLongDouble(result, &value) ? value : NAN;
}

// Evaluate an integrand for continuous binary random-variable operations
static long double binaryContinuousIntegrand(long double t, void* userdata) {
    // Decode the operation being integrated
    BinaryIntegrandData* data = (BinaryIntegrandData*)userdata;
    if (!data || !data->x || !data->y) return NAN;

    // Compute the sum density integrand
    if (data->mode == 0) {
        long double fx = pdfValue(data->x, t);
        long double fy = pdfValue(data->y, data->target - t);
        return isfinite(fx) && isfinite(fy) ? fx * fy : NAN;
    }

    // Compute the sum CDF integrand
    if (data->mode == 1) {
        long double fx = pdfValue(data->x, t);
        long double fy = cdfValue(data->y, data->target - t);
        return isfinite(fx) && isfinite(fy) ? fx * fy : NAN;
    }

    // Compute the product density integrand
    if (data->mode == 2) {
        if (t == 0.0L) return NAN;
        long double fx = pdfValue(data->x, t);
        long double fy = pdfValue(data->y, data->target / t);
        return isfinite(fx) && isfinite(fy) ? fx * fy / fabsl(t) : NAN;
    }

    // Compute the product CDF integrand
    if (data->mode == 3) {
        if (t == 0.0L) return NAN;
        long double fx = pdfValue(data->x, t);
        long double fy = cdfValue(data->y, data->target / t);
        if (!isfinite(fx) || !isfinite(fy)) return NAN;
        return t > 0.0L ? fx * fy : fx * (1.0L - fy);
    }

    return NAN;
}

// Numerically integrate a binary continuous operation with NEKO
static bool integrateBinaryContinuous(BinaryDistributionParams* params, long double lower, long double upper,
                                      long double target, int mode, long double* out) {
    // Reject invalid storage and empty integration intervals
    if (!params || !out || lower > upper) return false;
    if (lower == upper) {
        *out = 0.0L;
        return true;
    }

    // Wrap the KUMA integrand as a NEKO callback
    BinaryIntegrandData data = { .x = params->x, .y = params->y, .target = target, .mode = mode };
    NekoFunc* func = nekoFuncFromCallback(binaryContinuousIntegrand, &data);
    if (!func) return false;

    // Integrate adaptively over the requested finite interval
    NekoNumericResult result = nekoIntegrateNumeric(func, lower, upper, NEKO_INTEGRATE_ADAPTIVE_SIMPSON, 0, 1e-8L);
    nekoFreeFunc(func);
    if (result.status != NEKO_OK || !isfinite(result.value)) return false;
    *out = result.value;
    return true;
}

// Compute the mass function of an affine distribution
Number affineDistributionPMF(ProbabilityDistribution* dist, Number x) {
    // Reject invalid affine distributions and inputs
    if (!dist || dist->kind != KUMA_DIST_AFFINE || !dist->params || !isValidStatsNumber(x)) return kumaNan();
    AffineDistributionParams* params = (AffineDistributionParams*)dist->params;
    if (!params->base || params->base->type != KUMA_DIST_DISCRETE) return kumaNan();

    // Handle degenerate zero-scale transformations
    if (params->scalarValue == 0.0L) return eqNumbers(x, params->shift) ? constructNumberFromInt(1) : constructNumberFromInt(0);

    // Pull the query point back through the affine map
    Number shifted = subNumbers(x, params->shift);
    Number baseX = divNumbers(shifted, params->scalar);
    if (baseX.type == NUMBER_NAN) return baseX;
    return probabilityPMF(params->base, baseX);
}

// Compute the density of an affine distribution
Number affineDistributionPDF(ProbabilityDistribution* dist, Number x) {
    // Reject invalid affine distributions and inputs
    if (!dist || dist->kind != KUMA_DIST_AFFINE || !dist->params || !isValidStatsNumber(x)) return kumaNan();
    AffineDistributionParams* params = (AffineDistributionParams*)dist->params;
    if (!params->base || params->scalarValue == 0.0L) return kumaNan();

    // Apply f_Y(y) = f_X((y-b)/a)/|a|
    long double y = 0.0L;
    if (!numberToLongDouble(x, &y)) return kumaNan();
    long double density = pdfValue(params->base, (y - params->shiftValue) / params->scalarValue);
    if (!isfinite(density)) return kumaNan();
    return constructNumberFromDouble(density / fabsl(params->scalarValue));
}

// Compute the CDF of an affine distribution
Number affineDistributionCDF(ProbabilityDistribution* dist, Number x) {
    // Reject invalid affine distributions and inputs
    if (!dist || dist->kind != KUMA_DIST_AFFINE || !dist->params || !isValidStatsNumber(x)) return kumaNan();
    AffineDistributionParams* params = (AffineDistributionParams*)dist->params;
    if (!params->base || params->scalarValue == 0.0L) return kumaNan();

    // Enumerate finite discrete support to handle decreasing affine maps exactly
    if (params->base->type == KUMA_DIST_DISCRETE) {
        Number* values = NULL;
        Number* probabilities = NULL;
        size_t size = 0;
        if (!finiteDiscreteSupport(params->base, &values, &probabilities, &size)) return kumaNan();
        Number total = constructNumberFromInt(0);
        for (size_t i = 0; i < size; i++) {
            Number transformed = addNumbers(multNumbers(params->scalar, values[i]), params->shift);
            if (transformed.type == NUMBER_NAN) {
                freeNumberArrays(values, probabilities);
                return kumaNan();
            }
            if (compNumbers(transformed, x) <= 0) total = addNumbers(total, probabilities[i]);
        }
        freeNumberArrays(values, probabilities);
        return total;
    }

    // Map the query point back through the affine transform
    long double y = 0.0L;
    if (!numberToLongDouble(x, &y)) return kumaNan();
    Number baseX = constructNumberFromDouble((y - params->shiftValue) / params->scalarValue);
    Number baseCDF = probabilityCDF(params->base, baseX);
    if (baseCDF.type == NUMBER_NAN) return baseCDF;

    // Reverse the tail when the affine map is decreasing
    if (params->scalarValue > 0.0L) return baseCDF;
    return subNumbers(constructNumberFromInt(1), baseCDF);
}

// Compute the mean of an affine distribution
Number affineDistributionMean(ProbabilityDistribution* dist) {
    // Apply E[aX+b] = aE[X] + b
    if (!dist || dist->kind != KUMA_DIST_AFFINE || !dist->params) return kumaNan();
    AffineDistributionParams* params = (AffineDistributionParams*)dist->params;
    Number scaled = multNumbers(params->scalar, probabilityMean(params->base));
    if (scaled.type == NUMBER_NAN) return scaled;
    return addNumbers(scaled, params->shift);
}

// Compute the variance of an affine distribution
Number affineDistributionVariance(ProbabilityDistribution* dist) {
    // Apply Var(aX+b) = a^2 Var(X)
    if (!dist || dist->kind != KUMA_DIST_AFFINE || !dist->params) return kumaNan();
    AffineDistributionParams* params = (AffineDistributionParams*)dist->params;
    Number scalar2 = multNumbers(params->scalar, params->scalar);
    if (scalar2.type == NUMBER_NAN) return scalar2;
    return multNumbers(scalar2, probabilityVariance(params->base));
}

// Compute the standard deviation of an affine distribution
Number affineDistributionStddev(ProbabilityDistribution* dist) {
    // Apply sd(aX+b) = |a| sd(X)
    if (!dist || dist->kind != KUMA_DIST_AFFINE || !dist->params) return kumaNan();
    AffineDistributionParams* params = (AffineDistributionParams*)dist->params;
    Number sd = probabilityStddev(params->base);
    long double sdValue = 0.0L;
    if (!numberToLongDouble(sd, &sdValue)) return kumaNan();
    return constructNumberFromDouble(fabsl(params->scalarValue) * sdValue);
}

// Sample an affine distribution
Number affineDistributionSample(ProbabilityDistribution* dist) {
    // Apply the affine map to a base sample
    if (!dist || dist->kind != KUMA_DIST_AFFINE || !dist->params) return kumaNan();
    AffineDistributionParams* params = (AffineDistributionParams*)dist->params;
    Number scaled = multNumbers(params->scalar, probabilitySample(params->base));
    if (scaled.type == NUMBER_NAN) return scaled;
    return addNumbers(scaled, params->shift);
}

// Compute the mass function of a sum of independent distributions
Number sumIndependentDistributionPMF(ProbabilityDistribution* dist, Number x) {
    // Reject invalid sum distributions and inputs
    if (!dist || dist->kind != KUMA_DIST_SUM_INDEPENDENT || !dist->params || !isValidStatsNumber(x)) return kumaNan();
    BinaryDistributionParams* params = (BinaryDistributionParams*)dist->params;
    Number* xValues = NULL;
    Number* xProbabilities = NULL;
    Number* yValues = NULL;
    Number* yProbabilities = NULL;
    size_t xSize = 0;
    size_t ySize = 0;
    if (!finiteDiscreteSupport(params->x, &xValues, &xProbabilities, &xSize)) return kumaNan();
    if (!finiteDiscreteSupport(params->y, &yValues, &yProbabilities, &ySize)) {
        freeNumberArrays(xValues, xProbabilities);
        return kumaNan();
    }

    // Sum probabilities over all pairs with the requested sum
    Number total = constructNumberFromInt(0);
    for (size_t i = 0; i < xSize; i++) {
        for (size_t j = 0; j < ySize; j++) {
            Number value = addNumbers(xValues[i], yValues[j]);
            if (eqNumbers(value, x)) total = addNumbers(total, multNumbers(xProbabilities[i], yProbabilities[j]));
        }
    }
    freeNumberArrays(xValues, xProbabilities);
    freeNumberArrays(yValues, yProbabilities);
    return total;
}

// Compute the density of a sum of independent distributions
Number sumIndependentDistributionPDF(ProbabilityDistribution* dist, Number x) {
    // Reject invalid sum distributions and inputs
    if (!dist || dist->kind != KUMA_DIST_SUM_INDEPENDENT || !dist->params || !isValidStatsNumber(x)) return kumaNan();
    BinaryDistributionParams* params = (BinaryDistributionParams*)dist->params;
    long double z = 0.0L;
    if (!numberToLongDouble(x, &z)) return kumaNan();

    // Integrate over the support intersection where both densities can be nonzero
    long double lower = fmaxl(params->xLower, z - params->yUpper);
    long double upper = fminl(params->xUpper, z - params->yLower);
    if (lower >= upper) return constructNumberFromInt(0);
    long double value = 0.0L;
    if (!integrateBinaryContinuous(params, lower, upper, z, 0, &value)) return kumaNan();
    return constructNumberFromDouble(value);
}

// Compute the CDF of a sum of independent distributions
Number sumIndependentDistributionCDF(ProbabilityDistribution* dist, Number x) {
    // Reject invalid sum distributions and inputs
    if (!dist || dist->kind != KUMA_DIST_SUM_INDEPENDENT || !dist->params || !isValidStatsNumber(x)) return kumaNan();
    BinaryDistributionParams* params = (BinaryDistributionParams*)dist->params;

    // Enumerate finite discrete support exactly
    if (dist->type == KUMA_DIST_DISCRETE) {
        Number* xValues = NULL;
        Number* xProbabilities = NULL;
        Number* yValues = NULL;
        Number* yProbabilities = NULL;
        size_t xSize = 0;
        size_t ySize = 0;
        if (!finiteDiscreteSupport(params->x, &xValues, &xProbabilities, &xSize)) return kumaNan();
        if (!finiteDiscreteSupport(params->y, &yValues, &yProbabilities, &ySize)) {
            freeNumberArrays(xValues, xProbabilities);
            return kumaNan();
        }
        Number total = constructNumberFromInt(0);
        for (size_t i = 0; i < xSize; i++) {
            for (size_t j = 0; j < ySize; j++) {
                Number value = addNumbers(xValues[i], yValues[j]);
                if (compNumbers(value, x) <= 0) total = addNumbers(total, multNumbers(xProbabilities[i], yProbabilities[j]));
            }
        }
        freeNumberArrays(xValues, xProbabilities);
        freeNumberArrays(yValues, yProbabilities);
        return total;
    }

    long double z = 0.0L;
    if (!numberToLongDouble(x, &z)) return kumaNan();

    // Integrate f_X(t)F_Y(z-t) over the finite support of X
    long double value = 0.0L;
    if (!integrateBinaryContinuous(params, params->xLower, params->xUpper, z, 1, &value)) return kumaNan();
    if (value < 0.0L && value > -1e-10L) value = 0.0L;
    if (value > 1.0L && value < 1.0L + 1e-10L) value = 1.0L;
    return constructNumberFromDouble(value);
}

// Compute the mass function of a product of independent distributions
Number productIndependentDistributionPMF(ProbabilityDistribution* dist, Number x) {
    // Reject invalid product distributions and inputs
    if (!dist || dist->kind != KUMA_DIST_PRODUCT_INDEPENDENT || !dist->params || !isValidStatsNumber(x)) return kumaNan();
    BinaryDistributionParams* params = (BinaryDistributionParams*)dist->params;
    Number* xValues = NULL;
    Number* xProbabilities = NULL;
    Number* yValues = NULL;
    Number* yProbabilities = NULL;
    size_t xSize = 0;
    size_t ySize = 0;
    if (!finiteDiscreteSupport(params->x, &xValues, &xProbabilities, &xSize)) return kumaNan();
    if (!finiteDiscreteSupport(params->y, &yValues, &yProbabilities, &ySize)) {
        freeNumberArrays(xValues, xProbabilities);
        return kumaNan();
    }

    // Sum probabilities over all pairs with the requested product
    Number total = constructNumberFromInt(0);
    for (size_t i = 0; i < xSize; i++) {
        for (size_t j = 0; j < ySize; j++) {
            Number value = multNumbers(xValues[i], yValues[j]);
            if (eqNumbers(value, x)) total = addNumbers(total, multNumbers(xProbabilities[i], yProbabilities[j]));
        }
    }
    freeNumberArrays(xValues, xProbabilities);
    freeNumberArrays(yValues, yProbabilities);
    return total;
}

// Compute the density of a product of independent distributions
Number productIndependentDistributionPDF(ProbabilityDistribution* dist, Number x) {
    // Reject invalid product distributions and inputs
    if (!dist || dist->kind != KUMA_DIST_PRODUCT_INDEPENDENT || !dist->params || !isValidStatsNumber(x)) return kumaNan();
    BinaryDistributionParams* params = (BinaryDistributionParams*)dist->params;
    long double z = 0.0L;
    if (!numberToLongDouble(x, &z)) return kumaNan();

    // Integrate f_X(t)f_Y(z/t)/|t| over the finite support of X
    long double value = 0.0L;
    if (!integrateBinaryContinuous(params, params->xLower, params->xUpper, z, 2, &value)) return kumaNan();
    return constructNumberFromDouble(value);
}

// Compute the CDF of a product of independent distributions
Number productIndependentDistributionCDF(ProbabilityDistribution* dist, Number x) {
    // Reject invalid product distributions and inputs
    if (!dist || dist->kind != KUMA_DIST_PRODUCT_INDEPENDENT || !dist->params || !isValidStatsNumber(x)) return kumaNan();
    BinaryDistributionParams* params = (BinaryDistributionParams*)dist->params;

    // Enumerate finite discrete support exactly
    if (dist->type == KUMA_DIST_DISCRETE) {
        Number* xValues = NULL;
        Number* xProbabilities = NULL;
        Number* yValues = NULL;
        Number* yProbabilities = NULL;
        size_t xSize = 0;
        size_t ySize = 0;
        if (!finiteDiscreteSupport(params->x, &xValues, &xProbabilities, &xSize)) return kumaNan();
        if (!finiteDiscreteSupport(params->y, &yValues, &yProbabilities, &ySize)) {
            freeNumberArrays(xValues, xProbabilities);
            return kumaNan();
        }
        Number total = constructNumberFromInt(0);
        for (size_t i = 0; i < xSize; i++) {
            for (size_t j = 0; j < ySize; j++) {
                Number value = multNumbers(xValues[i], yValues[j]);
                if (compNumbers(value, x) <= 0) total = addNumbers(total, multNumbers(xProbabilities[i], yProbabilities[j]));
            }
        }
        freeNumberArrays(xValues, xProbabilities);
        freeNumberArrays(yValues, yProbabilities);
        return total;
    }

    long double z = 0.0L;
    if (!numberToLongDouble(x, &z)) return kumaNan();

    // Integrate the conditional product event over the finite support of X
    long double value = 0.0L;
    if (!integrateBinaryContinuous(params, params->xLower, params->xUpper, z, 3, &value)) return kumaNan();
    if (value < 0.0L && value > -1e-10L) value = 0.0L;
    if (value > 1.0L && value < 1.0L + 1e-10L) value = 1.0L;
    return constructNumberFromDouble(value);
}

// Compute the mean of a sum of independent distributions
Number sumIndependentDistributionMean(ProbabilityDistribution* dist) {
    // Apply E[X+Y] = E[X] + E[Y]
    if (!dist || dist->kind != KUMA_DIST_SUM_INDEPENDENT || !dist->params) return kumaNan();
    BinaryDistributionParams* params = (BinaryDistributionParams*)dist->params;
    return addNumbers(probabilityMean(params->x), probabilityMean(params->y));
}

// Compute the variance of a sum of independent distributions
Number sumIndependentDistributionVariance(ProbabilityDistribution* dist) {
    // Apply Var(X+Y) = Var(X) + Var(Y)
    if (!dist || dist->kind != KUMA_DIST_SUM_INDEPENDENT || !dist->params) return kumaNan();
    BinaryDistributionParams* params = (BinaryDistributionParams*)dist->params;
    return addNumbers(probabilityVariance(params->x), probabilityVariance(params->y));
}

// Compute the standard deviation of a sum of independent distributions
Number sumIndependentDistributionStddev(ProbabilityDistribution* dist) {
    // Take the square root of the variance
    Number v = sumIndependentDistributionVariance(dist);
    long double x = 0.0L;
    if (!numberToLongDouble(v, &x) || x < 0.0L) return kumaNan();
    return constructNumberFromDouble(sqrtl(x));
}

// Sample a sum of independent distributions
Number sumIndependentDistributionSample(ProbabilityDistribution* dist) {
    // Add independent samples from both source distributions
    if (!dist || dist->kind != KUMA_DIST_SUM_INDEPENDENT || !dist->params) return kumaNan();
    BinaryDistributionParams* params = (BinaryDistributionParams*)dist->params;
    return addNumbers(probabilitySample(params->x), probabilitySample(params->y));
}

// Compute the mean of a product of independent distributions
Number productIndependentDistributionMean(ProbabilityDistribution* dist) {
    // Apply E[XY] = E[X]E[Y]
    if (!dist || dist->kind != KUMA_DIST_PRODUCT_INDEPENDENT || !dist->params) return kumaNan();
    BinaryDistributionParams* params = (BinaryDistributionParams*)dist->params;
    return multNumbers(probabilityMean(params->x), probabilityMean(params->y));
}

// Compute the variance of a product of independent distributions
Number productIndependentDistributionVariance(ProbabilityDistribution* dist) {
    // Apply Var(XY) = E[X^2]E[Y^2] - E[X]^2E[Y]^2
    if (!dist || dist->kind != KUMA_DIST_PRODUCT_INDEPENDENT || !dist->params) return kumaNan();
    BinaryDistributionParams* params = (BinaryDistributionParams*)dist->params;
    Number mux = probabilityMean(params->x);
    Number muy = probabilityMean(params->y);
    Number vx = probabilityVariance(params->x);
    Number vy = probabilityVariance(params->y);
    Number mux2 = multNumbers(mux, mux);
    Number muy2 = multNumbers(muy, muy);
    Number ex2 = addNumbers(vx, mux2);
    Number ey2 = addNumbers(vy, muy2);
    Number first = multNumbers(ex2, ey2);
    Number second = multNumbers(mux2, muy2);
    return subNumbers(first, second);
}

// Compute the standard deviation of a product of independent distributions
Number productIndependentDistributionStddev(ProbabilityDistribution* dist) {
    // Take the square root of the variance
    Number v = productIndependentDistributionVariance(dist);
    long double x = 0.0L;
    if (!numberToLongDouble(v, &x) || x < 0.0L) return kumaNan();
    return constructNumberFromDouble(sqrtl(x));
}

// Sample a product of independent distributions
Number productIndependentDistributionSample(ProbabilityDistribution* dist) {
    // Multiply independent samples from both source distributions
    if (!dist || dist->kind != KUMA_DIST_PRODUCT_INDEPENDENT || !dist->params) return kumaNan();
    BinaryDistributionParams* params = (BinaryDistributionParams*)dist->params;
    return multNumbers(probabilitySample(params->x), probabilitySample(params->y));
}

/* ---------- RandomVariable construction ---------- */

// Construct a random variable wrapper around a distribution
RandomVariable* constructRandomVariable(
    const char* name,
    ProbabilityDistribution* distribution,
    bool ownsDistribution
) {
    // Reject missing names and missing distributions
    if (!name || !distribution) return NULL;

    // Allocate the random variable wrapper
    RandomVariable* rv = calloc(1, sizeof(RandomVariable));
    if (!rv) return NULL;

    // Copy the name and store the distribution ownership flag
    rv->name = dupstr(name);
    if (!rv->name) {
        free(rv);
        return NULL;
    }
    rv->distribution = distribution;
    rv->ownsDistribution = ownsDistribution;
    return rv;
}

// Free a random variable wrapper and any owned distribution
void freeRandomVariable(RandomVariable* rv) {
    // Treat NULL as already freed
    if (!rv) return;

    // Release any owned distribution before releasing the wrapper storage
    if (rv->ownsDistribution) freeProbabilityDistribution(rv->distribution);
    free(rv->name);
    free(rv);
}

/* ---------- RandomVariable accessors ---------- */

// Evaluate a random variable PMF
Number rvPMF(RandomVariable* rv, Number x) {
    // Delegate to the underlying distribution accessor
    if (!rv || !rv->distribution) return kumaNan();
    return probabilityPMF(rv->distribution, x);
}

// Evaluate a random variable PDF
Number rvPDF(RandomVariable* rv, Number x) {
    // Delegate to the underlying distribution accessor
    if (!rv || !rv->distribution) return kumaNan();
    return probabilityPDF(rv->distribution, x);
}

// Evaluate a random variable CDF
Number rvCDF(RandomVariable* rv, Number x) {
    // Delegate to the underlying distribution accessor
    if (!rv || !rv->distribution) return kumaNan();
    return probabilityCDF(rv->distribution, x);
}

// Evaluate a random variable expected value
Number rvExpectedValue(RandomVariable* rv) {
    // Delegate to the underlying distribution accessor
    if (!rv || !rv->distribution) return kumaNan();
    return probabilityMean(rv->distribution);
}

// Evaluate a random variable variance
Number rvVariance(RandomVariable* rv) {
    // Delegate to the underlying distribution accessor
    if (!rv || !rv->distribution) return kumaNan();
    return probabilityVariance(rv->distribution);
}

// Evaluate a random variable standard deviation
Number rvStddev(RandomVariable* rv) {
    // Delegate to the underlying distribution accessor
    if (!rv || !rv->distribution) return kumaNan();
    return probabilityStddev(rv->distribution);
}

// Sample a random variable
Number rvSample(RandomVariable* rv) {
    // Delegate to the underlying distribution accessor
    if (!rv || !rv->distribution) return kumaNan();
    return probabilitySample(rv->distribution);
}

// Build a NEKO expression for a supported random variable PDF
NekoExpr* rvPDFfunc(RandomVariable* rv) {
    // Delegate to the underlying distribution function builder
    if (!rv || !rv->distribution) return NULL;
    return probabilityPDFfunc(rv->distribution);
}

// Build a NEKO expression for a supported random variable CDF
NekoExpr* rvCDFfunc(RandomVariable* rv) {
    // Delegate to the underlying distribution function builder
    if (!rv || !rv->distribution) return NULL;
    return probabilityCDFfunc(rv->distribution);
}

/* ---------- RandomVariable transformations ---------- */

// Construct an owned transformed random variable
static RandomVariable* constructOwnedTransformedRV(char* name, ProbabilityDistribution* distribution) {
    // Reject missing transformation products
    if (!name || !distribution) {
        free(name);
        freeProbabilityDistribution(distribution);
        return NULL;
    }

    // Transfer distribution ownership into the random variable wrapper
    RandomVariable* rv = constructRandomVariable(name, distribution, true);
    free(name);
    if (!rv) freeProbabilityDistribution(distribution);
    return rv;
}

// Scale a random variable
RandomVariable* rvScale(RandomVariable* rv, Number scalar) {
    // Delegate to the affine transformation with zero shift
    return rvAffine(rv, scalar, constructNumberFromInt(0));
}

// Shift a random variable
RandomVariable* rvShift(RandomVariable* rv, Number shift) {
    // Delegate to the affine transformation with unit scale
    return rvAffine(rv, constructNumberFromInt(1), shift);
}

// Apply an affine transformation to a random variable
RandomVariable* rvAffine(RandomVariable* rv, Number scalar, Number shift) {
    // Reject invalid random variables and affine parameters
    if (!rv || !rv->distribution || !isValidStatsNumber(scalar) || !isValidStatsNumber(shift)) return NULL;

    // Build the transformed variable name
    char* name = NULL;
    if (isZeroNumber(shift)) {
        name = formatName("scale(%s)", rvName(rv, "X"));
    } else if (isOneNumber(scalar)) {
        name = formatName("shift(%s)", rvName(rv, "X"));
    } else {
        name = formatName("affine(%s)", rvName(rv, "X"));
    }

    // Build the public distribution-level affine transform
    return constructOwnedTransformedRV(name, affineDistribution(rv->distribution, scalar, shift));
}

// Sum independent random variables
RandomVariable* rvSumIndependent(RandomVariable* x, RandomVariable* y) {
    // Reject invalid random variables
    if (!x || !x->distribution || !y || !y->distribution) return NULL;
    char* name = formatName("sum(%s,%s)", rvName(x, "X"), rvName(y, "Y"));
    return constructOwnedTransformedRV(name, sumIndependentDistributions(x->distribution, y->distribution));
}

// Multiply independent random variables
RandomVariable* rvProductIndependent(RandomVariable* x, RandomVariable* y) {
    // Reject invalid random variables
    if (!x || !x->distribution || !y || !y->distribution) return NULL;
    char* name = formatName("product(%s,%s)", rvName(x, "X"), rvName(y, "Y"));
    return constructOwnedTransformedRV(name, productIndependentDistributions(x->distribution, y->distribution));
}

/* ---------- Utility validation functions ---------- */
// Return true if a Number is a probability
bool isValidProbability(Number p) {
    // Delegate to the shared probability validator
    return isProbabilityNumber(p);
}

// Return true if an array is a probability mass array
bool isValidProbabilityArray(Number* probabilities, size_t size) {
    // Reject missing, empty, or invalid arrays
    if (!probabilities || size < 1) return false;

    // Verify every probability and accumulate the total
    Number total = constructNumberFromInt(0);
    for (size_t i = 0; i < size; i++) {
        if (!isProbabilityNumber(probabilities[i])) return false;
        total = addNumbers(total, probabilities[i]);
        if (total.type == NUMBER_NAN) return false;
    }

    // Accept exact totals or real totals within a small tolerance
    long double totalReal = 0.0L;
    if (!numberToLongDouble(total, &totalReal)) return false;
    return fabsl(totalReal - 1.0L) <= 1e-12L;
}

// Return true if a distribution is discrete
bool isDiscreteDistribution(ProbabilityDistribution* dist) {
    // Check the distribution type tag
    return dist && dist->type == KUMA_DIST_DISCRETE;
}

// Return true if a distribution is continuous
bool isContinuousDistribution(ProbabilityDistribution* dist) {
    // Check the distribution type tag
    return dist && dist->type == KUMA_DIST_CONTINUOUS;
}

// Return true if a distribution has a PMF callback
bool distributionHasPMF(ProbabilityDistribution* dist) {
    // Discrete distributions expose probability mass functions through pmf
    return dist && dist->pmf != NULL;
}

// Return true if a distribution has a PDF callback
bool distributionHasPDF(ProbabilityDistribution* dist) {
    // Continuous distributions expose densities through pdf
    return dist && dist->pdf != NULL;
}

// Return true if a distribution has a CDF callback
bool distributionHasCDF(ProbabilityDistribution* dist) {
    // Any distribution may expose a cumulative distribution through cdf
    return dist && dist->cdf != NULL;
}

// Return true if a distribution has a sampler callback
bool distributionHasSampler(ProbabilityDistribution* dist) {
    // Any distribution may expose sampling through sample
    return dist && dist->sample != NULL;
}
