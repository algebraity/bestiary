#ifndef HEBI_H
#define HEBI_H

/* --------- Definitions of structs ---------- */

typedef struct Fraction {
    long long num;
    long long denom;
} Fraction;

/* ---------- Helper methods ---------- */
int comp(const void* a, const void* b);

/* ---------- Fractions ---------- */
Fraction* constructFraction(long long num, long long denom);
int compFractions(Fraction* p, Fraction* q);
void printFraction(Fraction* frac);
void freeFraction(Fraction* frac);

/* ---------- Transcendental constants ---------- */
long double pi(void);
long double e(void);
long double phi(void);

#endif
