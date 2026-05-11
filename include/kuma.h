#ifndef KUMA_H
#define KUMA_H

#include <stddef.h>
#include <stdbool.h>
#include "hebi.h"

/* ---------- Definitions of structs ---------- */

typedef struct ProbabilityDistribution ProbabilityDistribution;
typedef struct RandomVariable RandomVariable;
typedef struct Frequency Frequency;

typedef enum {
    KUMA_DIST_DISCRETE,
    KUMA_DIST_CONTINUOUS
} KumaDistributionType;

typedef enum {
    KUMA_DIST_BERNOULLI,
    KUMA_DIST_BINOMIAL,
    KUMA_DIST_GEOMETRIC,
    KUMA_DIST_POISSON,
    KUMA_DIST_DISCRETE_UNIFORM,

    KUMA_DIST_CONTINUOUS_UNIFORM,
    KUMA_DIST_NORMAL,
    KUMA_DIST_EXPONENTIAL,

    KUMA_DIST_CUSTOM
} KumaDistributionKind;

typedef struct {
    Number p;
} BernoulliParams;

typedef struct {
    long long n;
    Number p;
} BinomialParams;

typedef struct {
    Number p;
} GeometricParams;

typedef struct {
    Number lambda;
} PoissonParams;

typedef struct {
    long long a;
    long long b;
} DiscreteUniformParams;

typedef struct {
    Number a;
    Number b;
} ContinuousUniformParams;

typedef struct {
    Number mu;
    Number sigma;
} NormalParams;

typedef struct {
    Number lambda;
} ExponentialParams;

/*
 * A finite custom discrete distribution.
 *
 * values[i] has probability probabilities[i].
 * The arrays should have length size.
 */
typedef struct {
    Number* values;
    Number* probabilities;
    size_t size;
} CustomDiscreteParams;

/*
 * A custom continuous distribution.
 *
 * For now this stores only the support interval.
 * The actual pdf/cdf/etc. are supplied through function pointers
 * in ProbabilityDistribution.
 */
typedef struct {
    Number lower;
    Number upper;
} CustomContinuousParams;

struct Frequency {
    Number value;
    size_t count;
};


struct ProbabilityDistribution {
    KumaDistributionType type;
    KumaDistributionKind kind;

    // Distribution-specific parameter structs
    void* params;
    bool ownsParams;

    // function pointers to distribution-specific pointers
    Number (*pmf)(ProbabilityDistribution* dist, Number x);
    Number (*pdf)(ProbabilityDistribution* dist, Number x);
    Number (*cdf)(ProbabilityDistribution* dist, Number x);

    Number (*mean)(ProbabilityDistribution* dist);
    Number (*variance)(ProbabilityDistribution* dist);
    Number (*stddev)(ProbabilityDistribution* dist);

    Number (*sample)(ProbabilityDistribution* dist);
};

/* ---------- Random variable struct ---------- */

struct RandomVariable {
    char* name;
    ProbabilityDistribution* distribution; // not owned by the RV
};

/* ---------- Helper methods ---------- */
bool isValidStatsNumber(Number x);
bool isValidStatsArray(Number* data, size_t size);

/* ---------- Basic combinatorics ---------- */
long long factorial(long long n);
long long ncr(long long n, long long r);
long long npr(long long n, long long r);
long long multinomial(long long n, long long* parts, size_t k);

/* ---------- Basic descriptive statistics ---------- */
Number sum(Number* data, size_t size);
Number product(Number* data, size_t size);
Number mean(Number* data, size_t size);
Number median(Number* data, size_t size);
Number mode(Number* data, size_t size);
Number min(Number* data, size_t size);
Number max(Number* data, size_t size);
Number range(Number* data, size_t size);
Number variance(Number* data, size_t size);
Number sampleVariance(Number* data, size_t size);
Number stddev(Number* data, size_t size);
Number sampleStddev(Number* data, size_t size);
Number meanAbsDev(Number* data, size_t size);
Number medianAbsDev(Number* data, size_t size);
Number percentile(Number* data, size_t size, Number p);
Number quartile(Number* data, size_t size, int q);
Number iqr(Number* data, size_t size);
Number geometricMean(Number* data, size_t size);
Number harmonicMean(Number* data, size_t size);

/* ---------- Sorted-data variants ---------- */
Number medianSorted(Number* sortedData, size_t size);
Number percentileSorted(Number* sortedData, size_t size, Number p);
Number quartileSorted(Number* sortedData, size_t size, int q);

/* ---------- Frequency functions ---------- */
size_t numFreq(Number* data, Number x, size_t size);
size_t countDistinct(Number* data, size_t size);
bool contains(Number* data, Number x, size_t size);
Frequency* frequencies(Number* data, size_t size, size_t* outCount);
Frequency* frequenciesSorted(Number* sortedData, size_t size, size_t* outCount);
Number* modes(Number* data, size_t size, size_t* outCount);
void freeFrequencies(Frequency* freqs);
void freeModes(Number* modes);

/* ---------- Two-variable statistics ---------- */
Number covariance(Number* x, Number* y, size_t size);
Number sampleCovariance(Number* x, Number* y, size_t size);
Number correlation(Number* x, Number* y, size_t size);
Number linearRegressionSlope(Number* x, Number* y, size_t size);
Number linearRegressionIntercept(Number* x, Number* y, size_t size);
Number linearRegressionPredict(Number slope, Number intercept, Number x);

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
);
ProbabilityDistribution* constructBernoulliDistribution(Number p);
ProbabilityDistribution* constructBinomialDistribution(long long n, Number p);
ProbabilityDistribution* constructGeometricDistribution(Number p);
ProbabilityDistribution* constructPoissonDistribution(Number lambda);
ProbabilityDistribution* constructDiscreteUniformDistribution(long long a, long long b);
ProbabilityDistribution* constructContinuousUniformDistribution(Number a, Number b);
ProbabilityDistribution* constructNormalDistribution(Number mu, Number sigma);
ProbabilityDistribution* constructExponentialDistribution(Number lambda);
ProbabilityDistribution* constructCustomDiscreteDistribution(
    Number* values,
    Number* probabilities,
    size_t size,
    bool copyArrays
);
ProbabilityDistribution* constructCustomContinuousDistribution(
    Number lower,
    Number upper,
    Number (*pdf)(ProbabilityDistribution* dist, Number x),
    Number (*cdf)(ProbabilityDistribution* dist, Number x),
    Number (*mean)(ProbabilityDistribution* dist),
    Number (*variance)(ProbabilityDistribution* dist),
    Number (*sample)(ProbabilityDistribution* dist)
);
void freeProbabilityDistribution(ProbabilityDistribution* dist);

/* ---------- ProbabilityDistribution accessors ---------- */
Number probabilityPMF(ProbabilityDistribution* dist, Number x);
Number probabilityPDF(ProbabilityDistribution* dist, Number x);
Number probabilityCDF(ProbabilityDistribution* dist, Number x);
Number probabilityMean(ProbabilityDistribution* dist);
Number probabilityVariance(ProbabilityDistribution* dist);
Number probabilityStddev(ProbabilityDistribution* dist);
Number probabilitySample(ProbabilityDistribution* dist);

/* ---------- Named distribution functions ---------- */

// Bernoulli
Number bernoulliPMF(ProbabilityDistribution* dist, Number x);
Number bernoulliCDF(ProbabilityDistribution* dist, Number x);
Number bernoulliMean(ProbabilityDistribution* dist);
Number bernoulliVariance(ProbabilityDistribution* dist);
Number bernoulliStddev(ProbabilityDistribution* dist);
Number bernoulliSample(ProbabilityDistribution* dist);

// Binomial
Number binomialPMF(ProbabilityDistribution* dist, Number x);
Number binomialCDF(ProbabilityDistribution* dist, Number x);
Number binomialMean(ProbabilityDistribution* dist);
Number binomialVariance(ProbabilityDistribution* dist);
Number binomialStddev(ProbabilityDistribution* dist);
Number binomialSample(ProbabilityDistribution* dist);

// Geometric
Number geometricPMF(ProbabilityDistribution* dist, Number x);
Number geometricCDF(ProbabilityDistribution* dist, Number x);
Number geometricMeanDist(ProbabilityDistribution* dist);
Number geometricVariance(ProbabilityDistribution* dist);
Number geometricStddev(ProbabilityDistribution* dist);
Number geometricSample(ProbabilityDistribution* dist);

// Poisson
Number poissonPMF(ProbabilityDistribution* dist, Number x);
Number poissonCDF(ProbabilityDistribution* dist, Number x);
Number poissonMean(ProbabilityDistribution* dist);
Number poissonVariance(ProbabilityDistribution* dist);
Number poissonStddev(ProbabilityDistribution* dist);
Number poissonSample(ProbabilityDistribution* dist);

// Discrete uniform
Number discreteUniformPMF(ProbabilityDistribution* dist, Number x);
Number discreteUniformCDF(ProbabilityDistribution* dist, Number x);
Number discreteUniformMean(ProbabilityDistribution* dist);
Number discreteUniformVariance(ProbabilityDistribution* dist);
Number discreteUniformStddev(ProbabilityDistribution* dist);
Number discreteUniformSample(ProbabilityDistribution* dist);

// Continuous uniform
Number continuousUniformPDF(ProbabilityDistribution* dist, Number x);
Number continuousUniformCDF(ProbabilityDistribution* dist, Number x);
Number continuousUniformMean(ProbabilityDistribution* dist);
Number continuousUniformVariance(ProbabilityDistribution* dist);
Number continuousUniformStddev(ProbabilityDistribution* dist);
Number continuousUniformSample(ProbabilityDistribution* dist);

// Normal
Number normalPDF(ProbabilityDistribution* dist, Number x);
Number normalCDF(ProbabilityDistribution* dist, Number x);
Number normalMean(ProbabilityDistribution* dist);
Number normalVariance(ProbabilityDistribution* dist);
Number normalStddev(ProbabilityDistribution* dist);
Number normalSample(ProbabilityDistribution* dist);

// Exponential
Number exponentialPDF(ProbabilityDistribution* dist, Number x);
Number exponentialCDF(ProbabilityDistribution* dist, Number x);
Number exponentialMean(ProbabilityDistribution* dist);
Number exponentialVariance(ProbabilityDistribution* dist);
Number exponentialStddev(ProbabilityDistribution* dist);
Number exponentialSample(ProbabilityDistribution* dist);

// Custom discrete
Number customDiscretePMF(ProbabilityDistribution* dist, Number x);
Number customDiscreteCDF(ProbabilityDistribution* dist, Number x);
Number customDiscreteMean(ProbabilityDistribution* dist);
Number customDiscreteVariance(ProbabilityDistribution* dist);
Number customDiscreteStddev(ProbabilityDistribution* dist);
Number customDiscreteSample(ProbabilityDistribution* dist);

// Custom continuous
Number customContinuousPDF(ProbabilityDistribution* dist, Number x);
Number customContinuousCDF(ProbabilityDistribution* dist, Number x);
Number customContinuousMean(ProbabilityDistribution* dist);
Number customContinuousVariance(ProbabilityDistribution* dist);
Number customContinuousStddev(ProbabilityDistribution* dist);
Number customContinuousSample(ProbabilityDistribution* dist);

/* ---------- RandomVariable construction ---------- */
RandomVariable* constructRandomVariable(
    const char* name,
    ProbabilityDistribution* distribution
);
void freeRandomVariable(RandomVariable* rv);

/* ---------- RandomVariable accessors ---------- */
Number rvPMF(RandomVariable* rv, Number x);
Number rvPDF(RandomVariable* rv, Number x);
Number rvCDF(RandomVariable* rv, Number x);
Number rvExpectedValue(RandomVariable* rv);
Number rvVariance(RandomVariable* rv);
Number rvStddev(RandomVariable* rv);
Number rvSample(RandomVariable* rv);

/* ---------- RandomVariable transformations ---------- */
RandomVariable* rvScale(RandomVariable* rv, Number scalar);
RandomVariable* rvShift(RandomVariable* rv, Number shift);
RandomVariable* rvAffine(RandomVariable* rv, Number scalar, Number shift);
RandomVariable* rvSumIndependent(RandomVariable* x, RandomVariable* y);
RandomVariable* rvProductIndependent(RandomVariable* x, RandomVariable* y);

/* ---------- Utility validation functions ---------- */
bool isValidProbability(Number p);
bool isValidProbabilityArray(Number* probabilities, size_t size);
bool isDiscreteDistribution(ProbabilityDistribution* dist);
bool isContinuousDistribution(ProbabilityDistribution* dist);
bool distributionHasPMF(ProbabilityDistribution* dist);
bool distributionHasPDF(ProbabilityDistribution* dist);
bool distributionHasCDF(ProbabilityDistribution* dist);
bool distributionHasSampler(ProbabilityDistribution* dist);

#endif