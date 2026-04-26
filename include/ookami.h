#ifndef OOKAMI_H
#define OOKAMI_H

#include<stdbool.h>
#include "hebi.h"

/* ---------- Definition of structs ---------- */

typedef struct CombSet {
    long long *set;
    int card;
} CombSet;

/* ---------- Helper methods ---------- */

int comp(const void* a, const void* b);

/* ---------- Main CombSet methods ---------- */

void normalizeSet(long long* set, int* card);
void printSet(CombSet* combset);
CombSet* constructCombset(long long* baseSet, int card);
CombSet* constructRangeSet(long long start, long long end, long long step);
CombSet* constructArithmeticProgressionSet(long long first, long long diff, int terms);
CombSet* constructGeometricProgressionSet(long long first, long long ratio, int terms);
void freeCombset(CombSet* combset);
CombSet* copyCombset(CombSet* combset);
bool compCombset(CombSet* A, CombSet* B);
bool isSubset(CombSet* A, CombSet* B);

/* ---------- Basic operations ---------- */

CombSet* addSets(CombSet* A, CombSet* B);
CombSet* subtractSets(CombSet* A, CombSet* B);
CombSet* multiplySets(CombSet* A, CombSet* B);

/* ---------- Derived sets ---------- */

CombSet* ads(CombSet* combset);
CombSet* kads(CombSet* combset, int k);
CombSet* dds(CombSet* combset);
CombSet* kdds(CombSet* combset, int k);
CombSet* mds(CombSet* combset);
CombSet* kmds(CombSet* combset, int k);
CombSet* subsetSums(CombSet* combset, int subsetSize);
CombSet* setIntersection(CombSet* A, CombSet* B);
CombSet* setUnion(CombSet* A, CombSet* B);

/* ---------- Basic invariants ---------- */

long long getDiameter(CombSet* combset);
Fraction getDensity(CombSet* combset);
Fraction doublingConstant(CombSet* combset);

/* ---------- Basic transformations ---------- */

void addElement(CombSet* combset, long long n);
void removeElement(CombSet* combset, long long n);
CombSet* negateSet(CombSet* combset);
CombSet* translateSet(CombSet* combset, long long n);
CombSet* dilateSet(CombSet* combset, long long n);

/* ---------- Extra properties ---------- */

int adsCard(CombSet* combset);
int ddsCard(CombSet* combset);
int mdsCard(CombSet* combset);
bool isArithmeticProgression(CombSet* combset);
bool isGeometricProgression(CombSet* combset);

/* ---------- Distance measures ---------- */

long double ruzsaDistance(CombSet* A, CombSet* B);
long double ruzsaDistancePositive(CombSet* A, CombSet* B);

/* ---------- Representation functions ---------- */

long long kRepAdd(CombSet* combset, long long x, int k);
long long kRepDiff(CombSet* combset, long long x, int k);
long long kRepMult(CombSet* combset, long long x, int k);
long long repAdd(CombSet* combset, long long x);
long long repDiff(CombSet* combset, long long x);
long long repMult(CombSet* combset, long long x);

/* ---------- Compute set energies ---------- */

long long kEnergyAdd(CombSet* A, int k);
long long kEnergyDiff(CombSet* A, int k);
long long kEnergyMult(CombSet* A, int k);
long long addEnergy(CombSet* combset);
long long diffEnergy(CombSet* combset);
long long multEnergy(CombSet* combset);

#endif
