#include<stdlib.h>
#include<stdio.h>
#include<math.h>
#include<limits.h>
#include "hebi.h"
#include "ookami.h"

/* ---------- Main CombSet methods ---------- */

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

static int checkedNegLongLong(long long value, long long* out) {
    if (value == LLONG_MIN) return 0;
    *out = -value;
    return 1;
}

static int checkedCardProduct(int a, int b, int* out) {
    if (a < 0 || b < 0 || !out) return 0;
    if (a != 0 && b > INT_MAX / a) return 0;
    *out = a * b;
    return 1;
}

// Sort and deduplicate the set
void normalizeSet(long long* set, int *card) {
    if (*card < 1) return;

    // Sort the set
    qsort(set, *card, sizeof(long long), comp);
    
    // Deduplicate the set
    int j = 0;
    for (int i = 1; i < *card; i++) {
	if (set[i] != set[j]) set[++j] = set[i];
    }

    *card = j+1;
}

// Print the elements of combset
void printSet(CombSet* combset) {
    printf("[");
    for (int i = 0; i < combset->card-1; i++) printf("%lld, ", combset->set[i]);
    printf("%lld]\n", combset->set[combset->card-1]);
}

// Construct a CombSet from a base set, given the cardinality of the set
CombSet* constructCombset(long long* baseSet, int card) {
    if (card < 1) return NULL;

    // Define a new CombSet and set the cardinality
    CombSet* combset = malloc(sizeof(CombSet));
    if (!combset) return NULL;
    combset->set = malloc((size_t)card * sizeof(long long));
    if (!combset->set) {
        free(combset);
        return NULL;
    }
    combset->card = card;

    // Define and normalize the base set
    for (int i = 0; i < combset->card; i++) combset->set[i] = baseSet[i];
    normalizeSet(combset->set, &(combset->card));
    
    // Return the resulting CombSet
    return combset;
}

// Construct the integer range {start, start+step, ..., end}
CombSet* constructRangeSet(long long start, long long end, long long step) {
    if (step == 0) return NULL;
    if ((step > 0 && start > end) || (step < 0 && start < end)) return NULL;

    long long distance;
    if (!checkedSubLongLong(end, start, &distance)) return NULL;
    long long count = distance / step + 1;
    if (count < 1 || count > INT_MAX) return NULL;

    long long* elems = malloc((size_t)count * sizeof(long long));
    if (!elems) return NULL;
    for (long long i = 0; i < count; i++) {
        long long offset;
        if (!checkedMulLongLong(i, step, &offset) || !checkedAddLongLong(start, offset, &elems[i])) {
            free(elems);
            return NULL;
        }
    }

    CombSet* result = constructCombset(elems, (int)count);
    free(elems);
    return result;
}

// Construct the arithmetic progression {first + i*diff : 0 <= i < terms}
CombSet* constructArithmeticProgressionSet(long long first, long long diff, int terms) {
    if (terms < 1) return NULL;

    long long* elems = malloc((size_t)terms * sizeof(long long));
    if (!elems) return NULL;
    for (int i = 0; i < terms; i++) {
        long long offset;
        if (!checkedMulLongLong((long long)i, diff, &offset)
                || !checkedAddLongLong(first, offset, &elems[i])) {
            free(elems);
            return NULL;
        }
    }

    CombSet* result = constructCombset(elems, terms);
    free(elems);
    return result;
}

// Construct the geometric progression {first * ratio^i : 0 <= i < terms}
CombSet* constructGeometricProgressionSet(long long first, long long ratio, int terms) {
    if (terms < 1) return NULL;

    long long* elems = malloc((size_t)terms * sizeof(long long));
    if (!elems) return NULL;

    long long current = first;
    for (int i = 0; i < terms; i++) {
        elems[i] = current;
        if (i + 1 < terms && !checkedMulLongLong(current, ratio, &current)) {
            free(elems);
            return NULL;
        }
    }

    CombSet* result = constructCombset(elems, terms);
    free(elems);
    return result;
}

// Frees the memory used by the CombSet
void freeCombset(CombSet* combset) {
    free(combset->set);
    free(combset);
}

// Make a copy of a CombSet
CombSet* copyCombset(CombSet* combset) {
    long long* newSet = malloc((size_t)combset->card * sizeof(long long));
    if (!newSet) return NULL;
    for (int i = 0; i < combset->card; i++) newSet[i] = combset->set[i];
    CombSet* result = constructCombset(newSet, combset->card);
    free(newSet);
    return result;
}

// Compare two CombSets for equality
bool compCombset(CombSet* A, CombSet* B) {
    if (!(A->card == B->card)) return false;
    for (int i = 0; i < A->card; i++) if (!(A->set[i] == B->set[i])) return false;
    return true;
}

// Check if A is a subset of B
bool isSubset(CombSet* A, CombSet* B) {
    if (B->card < A->card) return false;
    
    int a = 0;
    int b = 0;
    while (b < B->card) {
	if (A->set[a] == B->set[b]) {
	    a++;
	    b++;
	} else if (A->set[a] < B->set[b]) {
            return false;
	} else {
	    b++;
	}
    }

    return a == A->card;
}

/* ---------- Basic operations ---------- */

// Given CombSets A and B, return A+B
CombSet* addSets(CombSet* A, CombSet* B) {
    int n;
    if (!checkedCardProduct(A->card, B->card, &n)) return NULL;
    long long *buf = malloc((size_t)n * sizeof(long long));
    if (!buf) return NULL;
    int k = 0;
    for (int i = 0; i < A->card; i++) {
	for (int j = 0; j < B->card; j++) {
	    if (!checkedAddLongLong(A->set[i], B->set[j], &buf[k++])) {
            free(buf);
            return NULL;
        }
	}
    }

    CombSet* result = constructCombset(buf, n);
    free(buf);
    return result;
}

// Given CombSets A and B, return A-B
CombSet* subtractSets(CombSet* A, CombSet* B) {
    int n;
    if (!checkedCardProduct(A->card, B->card, &n)) return NULL;
    long long *buf = malloc((size_t)n * sizeof(long long));
    if (!buf) return NULL;
    int k = 0;
    for (int i = 0; i < A->card; i++) {
	for (int j = 0; j < B->card; j++) {
	    if (!checkedSubLongLong(A->set[i], B->set[j], &buf[k])) {
            free(buf);
            return NULL;
        }
	    k++;
	}
    }

    CombSet* result = constructCombset(buf, n);
    free(buf);
    return result;
}

// Given CombSets A and B, return A*B
CombSet* multiplySets(CombSet* A, CombSet* B) {
    int n;
    if (!checkedCardProduct(A->card, B->card, &n)) return NULL;
    long long *buf = malloc((size_t)n * sizeof(long long));
    if (!buf) return NULL;
    int k = 0;
    for (int i = 0; i < A->card; i++) {
	for (int j = 0; j < B->card; j++) {
	    if (!checkedMulLongLong(A->set[i], B->set[j], &buf[k])) {
            free(buf);
            return NULL;
        }
	    k++;
	}
    }

    CombSet* result = constructCombset(buf, n);
    free(buf);
    return result;
}

/* ---------- Derived sets ---------- */

// Return the additive doubling set of a CombSet
CombSet* ads(CombSet* combset) {
    return addSets(combset, combset);
}

// Return A + A + ... + A; k-fold sumset
CombSet* kads(CombSet* combset, int k) {
    if (k < 1) return NULL;
    CombSet* toReturn = copyCombset(combset);
    if (!toReturn) return NULL;
    while (k > 1) {
	CombSet* temp = addSets(toReturn, combset);
	freeCombset(toReturn);
    if (!temp) return NULL;
	toReturn = temp;
	k--;
    }

    return toReturn;
}

static int subsetSumsCollect(CombSet* combset, int index, int chosen, int subsetSize,
                              long long currentSum, long long* sums, int* count) {
    if (index == combset->card) {
        if (subsetSize < 0 || chosen == subsetSize) {
            sums[*count] = currentSum;
            (*count)++;
        }
        return 1;
    }

    if (!subsetSumsCollect(combset, index + 1, chosen, subsetSize, currentSum, sums, count)) return 0;
    long long nextSum;
    if (!checkedAddLongLong(currentSum, combset->set[index], &nextSum)) return 0;
    return subsetSumsCollect(combset, index + 1, chosen + 1, subsetSize, nextSum, sums, count);
}

CombSet* subsetSums(CombSet* combset, int subsetSize) {
    if (!combset) return NULL;
    if (subsetSize > combset->card || subsetSize < -1) return NULL;
    if (combset->card >= (int)(8 * sizeof(size_t))) return NULL;

    size_t maxCount = (size_t)1 << combset->card;
    long long* sums = malloc(maxCount * sizeof(long long));
    if (!sums) return NULL;

    int count = 0;
    if (!subsetSumsCollect(combset, 0, 0, subsetSize, 0, sums, &count)) {
        free(sums);
        return NULL;
    }
    CombSet* result = constructCombset(sums, count);
    free(sums);
    return result;
}

// Return the subtractive doubling set of a CombSet
CombSet* dds(CombSet* combset) {
    return subtractSets(combset, combset);
}

// Return A - A - ... - A; k-fold diffset
CombSet* kdds(CombSet* combset, int k) {
    if (k < 1) return NULL;
    CombSet* toReturn = copyCombset(combset);
    if (!toReturn) return NULL;
    while (k > 1) {
	CombSet* temp = subtractSets(toReturn, combset);
	freeCombset(toReturn);
    if (!temp) return NULL;
	toReturn = temp;
	k--;
    }

    return toReturn;
}

// Return the multiplicative doubling set of a CombSet
CombSet* mds(CombSet* combset) {
    return multiplySets(combset, combset);
}

// Return A * A * ... * A; k-fold product set
CombSet* kmds(CombSet* combset, int k) {
    if (k < 1) return NULL;
    CombSet* toReturn = copyCombset(combset);
    if (!toReturn) return NULL;
    while (k > 1) {
	CombSet* temp = multiplySets(toReturn, combset);
	freeCombset(toReturn);
    if (!temp) return NULL;
	toReturn = temp;
	k--;
    }

    return toReturn;
}

// Return the intersection of who CombSets
CombSet* setIntersection(CombSet* A, CombSet* B) {
    int maxSize = A->card * (A->card <= B->card) + B->card * (A->card > B->card);
    long long* newSet = malloc(maxSize * sizeof(long long));

    int a = 0;
    int b = 0;
    int c = 0;
    while (a < A->card && b < B->card) {
	if (A->set[a] < B->set[b]) a++;
	else if (A->set[a] > B->set[b]) b++;
	else {
	    newSet[c++] = A->set[a];
	    a++;
	    b++;
	}
    }
	
    if (c == 0) return NULL;
    CombSet* C = constructCombset(newSet, c);
    free(newSet);
    return C;
}

// Return the union of two CombSets
CombSet* setUnion(CombSet* A, CombSet* B) {
    int maxSize = A->card + B->card;
    long long* newSet = malloc(maxSize * sizeof(long long));

    int a = 0;
    int b = 0;
    int c = 0;
    while (a < A->card && b < B->card) {
	if (A->set[a] < B->set[b]) {
	    newSet[c] = A->set[a];
	    a++;
	    c++;
	}
	else if (A->set[a] > B->set[b]) {
	    newSet[c] = B->set[b];
	    b++;
	    c++;
	}
	else {
	    newSet[c++] = A->set[a];
	    a++;
	    b++;
	}
    }
    while (a < A->card) newSet[c++] = A->set[a++];
    while (b < B->card) newSet[c++] = B->set[b++];

    if (c == 0) return NULL;
    CombSet* C = constructCombset(newSet, c);
    free(newSet);
    return C;
}

/* ---------- Basic invariants ---------- */

// Return the diameter of a CombSet: max - min
bool getDiameterChecked(CombSet* combset, long long* out) {
    if (!combset || combset->card < 1 || !out) return false;
    return checkedSubLongLong(combset->set[combset->card - 1], combset->set[0], out);
}

// Return the diameter of a CombSet: max - min
long long getDiameter(CombSet* combset) {
    long long out = 0;
    return getDiameterChecked(combset, &out) ? out : 0;
}

// Return the density of the CombSet: card / (diameter+1)
Fraction getDensity(CombSet* combset) {
    long long diameter;
    long long denom;
    if (!getDiameterChecked(combset, &diameter) || !checkedAddLongLong(diameter, 1, &denom)) return (Fraction){0, 0};
    return constructFraction(combset->card, denom);
}

// Compute the doubling contstant of the CombSet: |A + A|/|A|
Fraction doublingConstant(CombSet* combset) {
    CombSet* combsetads = ads(combset);
    if (!combsetads) return (Fraction){0, 0};
    Fraction frac = constructFraction(combsetads->card, combset->card);
    freeCombset(combsetads);
    return frac;
}

/* ---------- Basic transformations ---------- */

// Add an element to the CombSet
void addElement(CombSet* combset, long long n) {
    if (combset->card == INT_MAX) return;
    long long* newSet = malloc((size_t)(combset->card + 1) * sizeof(long long));
    if (!newSet) return;
    for (int i = 0; i < combset->card; i++) newSet[i] = combset->set[i];
    newSet[combset->card] = n;
    
    free(combset->set);
    combset->set = newSet;
    combset->card++;
    normalizeSet(combset->set, &(combset->card));
}

// Remove an element from the CombSet
void removeElement(CombSet* combset, long long n) {
    for (int i = 0; i < combset->card; i++) {
	if (combset->set[i] == n) {
	    for (int j = i; j < combset->card-1; j++) combset->set[j] = combset->set[j+1];
	    combset->card--;
	    return;
	}
    }
}

// Negate every element of the CombSet and normalize the result
CombSet* negateSet(CombSet* combset) {
    long long *buf = malloc((size_t)combset->card * sizeof(long long));
    if (!buf) return NULL;
    for (int i = 0; i < combset->card; i++) {
        if (!checkedNegLongLong(combset->set[i], &buf[i])) {
            free(buf);
            return NULL;
        }
    }
    CombSet* result = constructCombset(buf, combset->card);
    free(buf);
    return result;
}

// Translate the CombSet by an integer n
CombSet* translateSet(CombSet* combset, long long n) {
    long long *buf = malloc((size_t)combset->card * sizeof(long long));
    if (!buf) return NULL;
    for (int i = 0; i < combset->card; i++) {
        if (!checkedAddLongLong(combset->set[i], n, &buf[i])) {
            free(buf);
            return NULL;
        }
    }
    CombSet* result = constructCombset(buf, combset->card);
    free(buf);
    return result;
}

// Dilate the CombSet by an integer n
CombSet* dilateSet(CombSet* combset, long long n) {
    long long *buf = malloc((size_t)combset->card * sizeof(long long));
    if (!buf) return NULL;
    for (int i = 0; i < combset->card; i++) {
        if (!checkedMulLongLong(combset->set[i], n, &buf[i])) {
            free(buf);
            return NULL;
        }
    }
    CombSet* result = constructCombset(buf, combset->card);
    free(buf);
    return result;
}

/* ---------- Extra properties ---------- */

// Return |A+A|
int adsCard(CombSet* combset) {
    CombSet* combsetads = ads(combset);
    if (!combsetads) return -1;
    int adscard = combsetads->card;
    freeCombset(combsetads);
    return adscard;
}

// Return |A-A|
int ddsCard(CombSet* combset) {
    CombSet* combsetdds = dds(combset);
    if (!combsetdds) return -1;
    int ddscard = combsetdds->card;
    freeCombset(combsetdds);
    return ddscard;
}

// Return |A*A|
int mdsCard(CombSet* combset) {
    CombSet* combsetmds = mds(combset);
    if (!combsetmds) return -1;
    int mdscard = combsetmds->card;
    freeCombset(combsetmds);
    return mdscard;
}

// Determine if a CombSet is an arithmetic progression
bool isArithmeticProgression(CombSet* combset) {
    if (combset->card == 1) return true;

    long long d;
    if (!checkedSubLongLong(combset->set[1], combset->set[0], &d)) return false;
    for (int i = 0; i < combset->card-1; i++) {
        long long step;
        if (!checkedSubLongLong(combset->set[i + 1], combset->set[i], &step) || step != d) return false;
    }

    return true;
}

// Determine if a CombSet is a geometric progression
bool isGeometricProgression(CombSet* combset) {
    if (combset->card == 1) return true;
    for (int i = 0; i < combset->card; i++) if (combset->set[i] == 0) return false;

    long long a0 = combset->set[0];
    long long a1 = combset->set[1];
    for (int i = 1; i < combset->card-1; i++) {
        long long lhs, rhs;
        if (!checkedMulLongLong(combset->set[i + 1], a0, &lhs)
                || !checkedMulLongLong(a1, combset->set[i], &rhs)
                || lhs != rhs) return false;
    }

    return true;
}

/* ---------- Distance measures ---------- */

// Compute the Ruzsa distance between two CombSets: |A - B|/sqrt(|A| * |B|)
long double ruzsaDistance(CombSet* A, CombSet* B) {
    if (A->card == 0 || B->card == 0) return 0;
    CombSet* diffset = subtractSets(A, B);
    if (!diffset) return NAN;
    long double result = logl((long double)diffset->card / sqrtl((long double)A->card * B->card));
    freeCombset(diffset);
    return result;
}

// Compute the Ruzsa distance between two CombSets: |A + B|/sqrt(|A| * |B|)
long double ruzsaDistancePositive(CombSet* A, CombSet* B) {
    if (A->card == 0 || B->card == 0) return 0;
    CombSet* sumset = addSets(A, B);
    if (!sumset) return NAN;
    long double result = logl((long double)sumset->card / sqrtl((long double)A->card * B->card));
    freeCombset(sumset);
    return result;
}

/* ---------- Representation functions ---------- */

// Compute the k-fold additive representation function of an int for a CombSet
long long kRepAdd(CombSet* combset, long long x, int k) {
    if (!combset || k <= 0) return 0;
    int n = combset->card;
    long long total = 1;
    for (int i = 0; i < k; i++) total *= n;

    int* index = calloc((size_t)k, sizeof(int));
    if (!index) return 0;
    long long count = 0;

    for (long long t = 0; t < total; t++) {
	long long s = 0;
	for (int i = 0; i < k; i++) s += combset->set[index[i]];
	if (s == x) count++;
	for (int i = 0; i < k; i++) {
	    if (++index[i] < n) break;
	    index[i] = 0;
	}
    }

    free(index);
    return count;
}

// Compute the k-fold difference representation function of an int for a CombSet
long long kRepDiff(CombSet* combset, long long x, int k) {
    if (!combset || k <= 0) return 0;
    int n = combset->card;
    long long total = 1;
    for (int i = 0; i < k; i++) total *= n;

    int* index = calloc((size_t)k, sizeof(int));
    if (!index) return 0;
    long long count = 0;

    for (long long t = 0; t < total; t++) {
	long long s = combset->set[index[0]];
	for (int i = 1; i < k; i++) s -= combset->set[index[i]];
	if (s == x) count++;
	for (int i = 0; i < k; i++) {
	    if (++index[i] < n) break;
	    index[i] = 0;
	}
    }

    free(index);
    return count;
}

// Compute the k-fold multiplicative representation function of an int for a CombSet
long long kRepMult(CombSet* combset, long long x, int k) {
    if (!combset || k <= 0) return 0;
    int n = combset->card;
    long long total = 1;
    for (int i = 0; i < k; i++) total *= n;

    int* index = calloc((size_t)k, sizeof(int));
    if (!index) return 0;
    long long count = 0;

    for (long long t = 0; t < total; t++) {
	long long s = 1;
	for (int i = 0; i < k; i++) s *= combset->set[index[i]];
	if (s == x) count++;
	for (int i = 0; i < k; i++) {
	    if (++index[i] < n) break;
	    index[i] = 0;
	}
    }

    free(index);
    return count;
}

// Compute the additive representation function of an int for a CombSet
long long repAdd(CombSet* combset, long long x) {
    return kRepAdd(combset, x, 2);
}

// Compute the difference representation function of an int for a CombSet
long long repDiff(CombSet* combset, long long x) {
    return kRepDiff(combset, x, 2);
}

// Compute the multiplicative representation function of an int for a CombSet
long long repMult(CombSet* combset, long long x) {
    return kRepMult(combset, x, 2);
}

/* ---------- Compute set energies ---------- */

// Compute the k-fold additive energy of a CombSet
long long kEnergyAdd(CombSet* A, int k) {
    if (!A || k <= 0) return 0;
    int n = A->card;
    long long total = 1;
    for (int i = 0; i < k; i++) total *= n;
    
    long long* sums = malloc(total * sizeof(long long));
    int* index = calloc((size_t)k, sizeof(int));
    if (!sums || !index) {
        free(sums);
        free(index);
        return 0;
    }
    
    // Enumerate all k-tuples, record their sums
    for (long long t = 0; t < total; t++) {
        long long s = 0;
        for (int i = 0; i < k; i++) s += A->set[index[i]];
        sums[t] = s;
        for (int i = 0; i < k; i++) {
            if (++index[i] < n) break;
            index[i] = 0;
        }
    }
    
    // Sort, then sum squares of run lengths
    qsort(sums, total, sizeof(long long), comp);
    long long energy = 0;
    long long i = 0;
    while (i < total) {
        long long j = i;
        while (j < total && sums[j] == sums[i]) j++;
        long long count = j - i;
        energy += count * count;
        i = j;
    }
    
    free(sums);
    free(index);
    return energy;
}

// Compute the k-fold difference energy of a CombSet
long long kEnergyDiff(CombSet* A, int k) {
    if (!A || k <= 0) return 0;
    int n = A->card;
    long long total = 1;
    for (int i = 0; i < k; i++) total *= n;
    
    long long* diffs = malloc(total * sizeof(long long));
    int* index = calloc((size_t)k, sizeof(int));
    if (!diffs || !index) {
        free(diffs);
        free(index);
        return 0;
    }
    
    // Enumerate all k-tuples, record their differences
    for (long long t = 0; t < total; t++) {
        long long s = A->set[index[0]];
        for (int i = 1; i < k; i++) s -= A->set[index[i]];
        diffs[t] = s;
        for (int i = 0; i < k; i++) {
            if (++index[i] < n) break;
            index[i] = 0;
        }
    }
    
    // Sort, then sum squares of run lengths
    qsort(diffs, total, sizeof(long long), comp);
    long long energy = 0;
    long long i = 0;
    while (i < total) {
        long long j = i;
        while (j < total && diffs[j] == diffs[i]) j++;
        long long count = j - i;
        energy += count * count;
        i = j;
    }
    
    free(diffs);
    free(index);
    return energy;
}

// Compute the k-fold multiplicative energy of a CombSet
long long kEnergyMult(CombSet* A, int k) {
    if (!A || k <= 0) return 0;
    int n = A->card;
    long long total = 1;
    for (int i = 0; i < k; i++) total *= n;
    
    long long* prods = malloc(total * sizeof(long long));
    int* index = calloc((size_t)k, sizeof(int));
    if (!prods || !index) {
        free(prods);
        free(index);
        return 0;
    }
    
    // Enumerate all k-tuples, record their products
    for (long long t = 0; t < total; t++) {
        long long s = 1;
        for (int i = 0; i < k; i++) s *= A->set[index[i]];
        prods[t] = s;
        for (int i = 0; i < k; i++) {
            if (++index[i] < n) break;
            index[i] = 0;
        }
    }
    
    // Sort, then sum squares of run lengths
    qsort(prods, total, sizeof(long long), comp);
    long long energy = 0;
    long long i = 0;
    while (i < total) {
        long long j = i;
        while (j < total && prods[j] == prods[i]) j++;
        long long count = j - i;
        energy += count * count;
        i = j;
    }
    
    free(prods);
    free(index);
    return energy;
}

// Compute the additive energy of a CombSet
long long addEnergy(CombSet* combset) {
    return kEnergyAdd(combset, 2);
}

// Compute the difference energy of a CombSet
long long diffEnergy(CombSet* combset) {
    return kEnergyDiff(combset, 2);
}

// Compute the multiplicative energy of a CombSet
long long multEnergy(CombSet* combset) {
    return kEnergyMult(combset, 2);
}
