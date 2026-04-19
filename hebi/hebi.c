#include<stdlib.h>
#include<stdio.h>
#include<math.h>
#include "hebi.h"

/* ---------- Helper methods ---------- */

// comp method for qsort
int comp(const void* a, const void* b) {
    return (*(long long*)a > *(long long*)b) - (*(long long*)a < *(long long*)b);
}

/* ---------- Fractions ---------- */

// Construct a Fraction with a given num and denom
Fraction* constructFraction(long long num, long long denom) {
    if (denom == 0) return NULL;
    Fraction* frac = malloc(sizeof(Fraction));

    long long g = llabs(num), b = llabs(denom);
    while (b) {
	long long t = b;
	b = g % b;
	g = t;
    }
    if (g == 0) g = 1;

    frac->num = num / g;
    frac->denom = denom / g;
    return frac;
}

// Compare Fractions
int compFractions(Fraction* p, Fraction* q) {
    if (!p || !q) return 2;

    long long n1 = p->num;
    long long d1 = p->denom;
    long long n2 = q->num;
    long long d2 = q->denom;

    if (n1 == n2 && d1 == d2) return 0;
    if (n1 != n2 && d1 == d2) return (n1 > n2) - (n1 < n2);

    return ((long double)n1/d1 > (long double)n2/d2) - ((long double)n1/d1 < (long double)n2/d2);
}

// Print a Fraction
void printFraction(Fraction* frac) {
    if (!frac) return;

    if (frac->denom == 1) {
        printf("%lld", frac->num);
        return;
    }

    printf("%lld/%lld", frac->num, frac->denom);
}

// Free a Fraction
void freeFraction(Fraction* frac) {
    if (!frac) return;
    free(frac);
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

