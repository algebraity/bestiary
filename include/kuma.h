#ifndef KUMA_H
#define KUMA_H

#include <stddef.h>
#include <stdbool.h>
#include "hebi.h"

/* ---------- Definitions of structs ---------- */

typedef struct ProbabilityDistribution ProbabilityDistribution;
typedef struct RandomVariable RandomVariable;
typedef struct Frequency Frequency;
typedef struct NekoExpr NekoExpr;
typedef struct KumaBoxPlotData KumaBoxPlotData;
typedef struct KumaHistogramData KumaHistogramData;
typedef struct KumaFrequencyPlotData KumaFrequencyPlotData;
typedef struct KumaBarGraphData KumaBarGraphData;
typedef struct KumaDensityPlotData KumaDensityPlotData;
typedef struct KumaDotPlotData KumaDotPlotData;
typedef struct KumaECDFPlotData KumaECDFPlotData;
typedef struct KumaQQPlotData KumaQQPlotData;
typedef struct KumaScatterPlotData KumaScatterPlotData;
typedef struct KumaRegressionPlotData KumaRegressionPlotData;

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

    KUMA_DIST_CUSTOM,
    KUMA_DIST_AFFINE,
    KUMA_DIST_SUM_INDEPENDENT,
    KUMA_DIST_PRODUCT_INDEPENDENT
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
    bool ownsArrays;
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

typedef struct {
    CustomContinuousParams support;
    ProbabilityDistribution* base;
    Number scalar;
    Number shift;
    long double scalarValue;
    long double shiftValue;
} AffineDistributionParams;

typedef struct {
    CustomContinuousParams support;
    ProbabilityDistribution* x;
    ProbabilityDistribution* y;
    long double xLower;
    long double xUpper;
    long double yLower;
    long double yUpper;
} BinaryDistributionParams;

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
    ProbabilityDistribution* distribution;
    bool ownsDistribution;
};

/* ---------- Plot data structs ---------- */

typedef struct {
    long double x;
    long double y;
} KumaPlotPoint;

typedef struct {
    Number value;
    size_t count;
    long double position;
    long double proportion;
} KumaFrequencyPlotItem;

typedef struct {
    Number label;
    Number value;
    long double height;
} KumaBarGraphItem;

typedef struct {
    long double lower;
    long double upper;
    long double midpoint;
    size_t count;
    long double proportion;
    long double density;
} KumaHistogramBin;

struct KumaBoxPlotData {
    long double minimum;
    long double q1;
    long double median;
    long double q3;
    long double maximum;
    long double lowerFence;
    long double upperFence;
    long double lowerWhisker;
    long double upperWhisker;
    long double* outliers;
    size_t outlierCount;
};

struct KumaHistogramData {
    KumaHistogramBin* bins;
    size_t binCount;
    size_t sampleSize;
    long double minimum;
    long double maximum;
    long double binWidth;
};

struct KumaFrequencyPlotData {
    KumaFrequencyPlotItem* items;
    size_t count;
    size_t sampleSize;
};

struct KumaBarGraphData {
    KumaBarGraphItem* bars;
    size_t count;
};

struct KumaDensityPlotData {
    KumaPlotPoint* points;
    size_t count;
    size_t sampleSize;
    long double bandwidth;
};

struct KumaDotPlotData {
    KumaFrequencyPlotItem* dots;
    size_t count;
    size_t sampleSize;
};

struct KumaECDFPlotData {
    KumaPlotPoint* points;
    size_t count;
    size_t sampleSize;
};

struct KumaQQPlotData {
    KumaPlotPoint* points;
    size_t count;
};

struct KumaScatterPlotData {
    KumaPlotPoint* points;
    size_t count;
};

struct KumaRegressionPlotData {
    KumaPlotPoint* points;
    size_t count;
    KumaPlotPoint lineStart;
    KumaPlotPoint lineEnd;
    Number slope;
    Number intercept;
};

/* ---------- Helper methods ---------- */
bool isValidStatsNumber(Number x);
bool isValidStatsArray(Number* data, size_t size);
bool containsFraction(Number* data, size_t size);
bool containsReal(Number* data, size_t size);

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

/* ---------- Plot data functions ---------- */
KumaBoxPlotData* boxplot(Number* data, size_t size);
KumaHistogramData* histogram(Number* data, size_t size, size_t binCount);
KumaFrequencyPlotData* frequencyPlot(Number* data, size_t size);
KumaBarGraphData* barGraph(Number* labels, Number* values, size_t size);
KumaDensityPlotData* densityPlot(Number* data, size_t size, size_t pointCount, Number bandwidth);
KumaDotPlotData* dotPlot(Number* data, size_t size);
KumaECDFPlotData* ecdf(Number* data, size_t size);
KumaQQPlotData* qqPlot(Number* data, size_t size, ProbabilityDistribution* dist);
KumaScatterPlotData* scatterPlot(Number* x, Number* y, size_t size);
KumaRegressionPlotData* regressionPlot(Number* x, Number* y, size_t size);
void freeBoxPlotData(KumaBoxPlotData* plot);
void freeHistogramData(KumaHistogramData* plot);
void freeFrequencyPlotData(KumaFrequencyPlotData* plot);
void freeBarGraphData(KumaBarGraphData* plot);
void freeDensityPlotData(KumaDensityPlotData* plot);
void freeDotPlotData(KumaDotPlotData* plot);
void freeECDFPlotData(KumaECDFPlotData* plot);
void freeQQPlotData(KumaQQPlotData* plot);
void freeScatterPlotData(KumaScatterPlotData* plot);
void freeRegressionPlotData(KumaRegressionPlotData* plot);

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
ProbabilityDistribution* copyProbabilityDistribution(ProbabilityDistribution* dist);
ProbabilityDistribution* affineDistribution(ProbabilityDistribution* dist, Number scalar, Number shift);
ProbabilityDistribution* sumIndependentDistributions(ProbabilityDistribution* x, ProbabilityDistribution* y);
ProbabilityDistribution* productIndependentDistributions(ProbabilityDistribution* x, ProbabilityDistribution* y);

/* ---------- ProbabilityDistribution accessors ---------- */
Number probabilityPMF(ProbabilityDistribution* dist, Number x);
Number probabilityPDF(ProbabilityDistribution* dist, Number x);
Number probabilityCDF(ProbabilityDistribution* dist, Number x);
Number probabilityMean(ProbabilityDistribution* dist);
Number probabilityVariance(ProbabilityDistribution* dist);
Number probabilityStddev(ProbabilityDistribution* dist);
Number probabilitySample(ProbabilityDistribution* dist);
NekoExpr* probabilityPDFfunc(ProbabilityDistribution* dist);
NekoExpr* probabilityCDFfunc(ProbabilityDistribution* dist);

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
Number bernoulliPMF(ProbabilityDistribution* dist, Number x);
Number bernoulliCDF(ProbabilityDistribution* dist, Number x);
Number bernoulliMean(ProbabilityDistribution* dist);
Number bernoulliVariance(ProbabilityDistribution* dist);
Number bernoulliStddev(ProbabilityDistribution* dist);
Number bernoulliSample(ProbabilityDistribution* dist);

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
Number binomialPMF(ProbabilityDistribution* dist, Number x);
Number binomialCDF(ProbabilityDistribution* dist, Number x);
Number binomialMean(ProbabilityDistribution* dist);
Number binomialVariance(ProbabilityDistribution* dist);
Number binomialStddev(ProbabilityDistribution* dist);
Number binomialSample(ProbabilityDistribution* dist);

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
Number geometricPMF(ProbabilityDistribution* dist, Number x);
Number geometricCDF(ProbabilityDistribution* dist, Number x);
Number geometricMeanDist(ProbabilityDistribution* dist);
Number geometricVariance(ProbabilityDistribution* dist);
Number geometricStddev(ProbabilityDistribution* dist);
Number geometricSample(ProbabilityDistribution* dist);

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
Number poissonPMF(ProbabilityDistribution* dist, Number x);
Number poissonCDF(ProbabilityDistribution* dist, Number x);
Number poissonMean(ProbabilityDistribution* dist);
Number poissonVariance(ProbabilityDistribution* dist);
Number poissonStddev(ProbabilityDistribution* dist);
Number poissonSample(ProbabilityDistribution* dist);

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
Number discreteUniformPMF(ProbabilityDistribution* dist, Number x);
Number discreteUniformCDF(ProbabilityDistribution* dist, Number x);
Number discreteUniformMean(ProbabilityDistribution* dist);
Number discreteUniformVariance(ProbabilityDistribution* dist);
Number discreteUniformStddev(ProbabilityDistribution* dist);
Number discreteUniformSample(ProbabilityDistribution* dist);

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
Number continuousUniformPDF(ProbabilityDistribution* dist, Number x);
Number continuousUniformCDF(ProbabilityDistribution* dist, Number x);
NekoExpr* continuousUniformPDFfunc(ProbabilityDistribution* dist);
NekoExpr* continuousUniformCDFfunc(ProbabilityDistribution* dist);
Number continuousUniformMean(ProbabilityDistribution* dist);
Number continuousUniformVariance(ProbabilityDistribution* dist);
Number continuousUniformStddev(ProbabilityDistribution* dist);
Number continuousUniformSample(ProbabilityDistribution* dist);

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
Number normalPDF(ProbabilityDistribution* dist, Number x);
Number normalCDF(ProbabilityDistribution* dist, Number x);
NekoExpr* normalPDFfunc(ProbabilityDistribution* dist);
NekoExpr* normalCDFfunc(ProbabilityDistribution* dist);
Number normalMean(ProbabilityDistribution* dist);
Number normalVariance(ProbabilityDistribution* dist);
Number normalStddev(ProbabilityDistribution* dist);
Number normalSample(ProbabilityDistribution* dist);

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
Number exponentialPDF(ProbabilityDistribution* dist, Number x);
Number exponentialCDF(ProbabilityDistribution* dist, Number x);
NekoExpr* exponentialPDFfunc(ProbabilityDistribution* dist);
NekoExpr* exponentialCDFfunc(ProbabilityDistribution* dist);
Number exponentialMean(ProbabilityDistribution* dist);
Number exponentialVariance(ProbabilityDistribution* dist);
Number exponentialStddev(ProbabilityDistribution* dist);
Number exponentialSample(ProbabilityDistribution* dist);

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
Number customDiscretePMF(ProbabilityDistribution* dist, Number x);
Number customDiscreteCDF(ProbabilityDistribution* dist, Number x);
Number customDiscreteMean(ProbabilityDistribution* dist);
Number customDiscreteVariance(ProbabilityDistribution* dist);
Number customDiscreteStddev(ProbabilityDistribution* dist);
Number customDiscreteSample(ProbabilityDistribution* dist);

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
Number customContinuousPDF(ProbabilityDistribution* dist, Number x);
Number customContinuousCDF(ProbabilityDistribution* dist, Number x);
Number customContinuousMean(ProbabilityDistribution* dist);
Number customContinuousVariance(ProbabilityDistribution* dist);
Number customContinuousStddev(ProbabilityDistribution* dist);
Number customContinuousSample(ProbabilityDistribution* dist);

/* ---------- Distribution schemes ---------- */
Number affineDistributionPMF(ProbabilityDistribution* dist, Number x);
Number affineDistributionPDF(ProbabilityDistribution* dist, Number x);
Number affineDistributionCDF(ProbabilityDistribution* dist, Number x);
Number affineDistributionMean(ProbabilityDistribution* dist);
Number affineDistributionVariance(ProbabilityDistribution* dist);
Number affineDistributionStddev(ProbabilityDistribution* dist);
Number affineDistributionSample(ProbabilityDistribution* dist);
Number sumIndependentDistributionPMF(ProbabilityDistribution* dist, Number x);
Number sumIndependentDistributionPDF(ProbabilityDistribution* dist, Number x);
Number sumIndependentDistributionCDF(ProbabilityDistribution* dist, Number x);
Number sumIndependentDistributionMean(ProbabilityDistribution* dist);
Number sumIndependentDistributionVariance(ProbabilityDistribution* dist);
Number sumIndependentDistributionStddev(ProbabilityDistribution* dist);
Number sumIndependentDistributionSample(ProbabilityDistribution* dist);
Number productIndependentDistributionPMF(ProbabilityDistribution* dist, Number x);
Number productIndependentDistributionPDF(ProbabilityDistribution* dist, Number x);
Number productIndependentDistributionCDF(ProbabilityDistribution* dist, Number x);
Number productIndependentDistributionMean(ProbabilityDistribution* dist);
Number productIndependentDistributionVariance(ProbabilityDistribution* dist);
Number productIndependentDistributionStddev(ProbabilityDistribution* dist);
Number productIndependentDistributionSample(ProbabilityDistribution* dist);

/* ---------- RandomVariable construction ---------- */
RandomVariable* constructRandomVariable(
    const char* name,
    ProbabilityDistribution* distribution,
    bool ownsDistribution
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
NekoExpr* rvPDFfunc(RandomVariable* rv);
NekoExpr* rvCDFfunc(RandomVariable* rv);

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
