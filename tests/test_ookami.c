// test_combset.c
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "ookami.h"

static void section(const char* title) {
    printf("\n===== %s =====\n", title);
}

int main(void) {
    /* ---------- Construction ---------- */
    section("Construction");
    long long a[] = {1, 2, 3};
    long long b[] = {2, 4, 7};
    long long ap[] = {2, 5, 8, 11, 14};     // arithmetic progression
    long long gp[] = {2, 6, 18, 54};         // geometric progression
    long long dup[] = {5, 1, 3, 1, 5, 2};    // duplicates + unsorted

    CombSet* A  = constructCombset(a, 3);
    CombSet* B  = constructCombset(b, 3);
    CombSet* AP = constructCombset(ap, 5);
    CombSet* GP = constructCombset(gp, 4);
    CombSet* D  = constructCombset(dup, 6);

    printf("A  = "); printSet(A);
    printf("B  = "); printSet(B);
    printf("AP = "); printSet(AP);
    printf("GP = "); printSet(GP);
    printf("D  = "); printSet(D);  // should be normalized to {1,2,3,5}

    CombSet* Acopy = copyCombset(A);
    printf("copy(A) = "); printSet(Acopy);
    printf("compCombset(A, copy(A)) = %d\n", compCombset(A, Acopy));
    printf("compCombset(A, B)       = %d\n", compCombset(A, B));

    /* ---------- Basic operations ---------- */
    section("Basic operations");
    CombSet* sum  = addSets(A, B);
    CombSet* diff = subtractSets(A, B);
    CombSet* prod = multiplySets(A, B);
    printf("A+B = "); printSet(sum);
    printf("A-B = "); printSet(diff);
    printf("A*B = "); printSet(prod);

    /* ---------- Derived sets ---------- */
    section("Derived sets");
    CombSet* Aads = ads(A);
    CombSet* Adds = dds(A);
    CombSet* Amds = mds(A);
    printf("ads(A) = A+A   = "); printSet(Aads);
    printf("dds(A) = A-A   = "); printSet(Adds);
    printf("mds(A) = A*A   = "); printSet(Amds);

    CombSet* A3add = kads(A, 3);
    CombSet* A3sub = kdds(A, 3);
    CombSet* A3mul = kmds(A, 3);
    printf("kads(A, 3) = "); printSet(A3add);
    printf("kdds(A, 3) = "); printSet(A3sub);
    printf("kmds(A, 3) = "); printSet(A3mul);

    /* ---------- Set comparisons / intersection ---------- */
    section("Comparisons / intersection");
    printf("isSubset(A, A+B)    = %d\n", isSubset(A, sum));
    printf("isSubset(A, B)      = %d\n", isSubset(A, B));

    long long c[] = {2, 3, 5, 7};
    CombSet* C = constructCombset(c, 4);
    printf("C = "); printSet(C);
    CombSet* AintC = setIntersection(A, C);
    printf("A ∩ C = ");
    if (AintC) { printSet(AintC); freeCombset(AintC); }
    else printf("(empty)\n");

    CombSet* AintB = setIntersection(A, B);
    printf("A ∩ B = ");
    if (AintB) { printSet(AintB); freeCombset(AintB); }
    else printf("(empty)\n");

    /* ---------- Basic invariants ---------- */
    section("Basic invariants");
    printf("diameter(A)  = %lld\n", getDiameter(A));
    printf("diameter(AP) = %lld\n", getDiameter(AP));

    Fraction densA  = getDensity(A);
    Fraction densAP = getDensity(AP);
    Fraction dcA    = doublingConstant(A);
    Fraction dcAP   = doublingConstant(AP);
    printf("density(A)   = %lld/%lld\n", densA.num, densA.denom);
    printf("density(AP)  = %lld/%lld\n", densAP.num, densAP.denom);
    printf("doubling(A)  = %lld/%lld\n", dcA.num, dcA.denom);
    printf("doubling(AP) = %lld/%lld\n", dcAP.num, dcAP.denom);

    /* ---------- Transformations ---------- */
    section("Transformations");
    CombSet* Tplus  = translateSet(A, 10);
    CombSet* Tminus = translateSet(A, -1);
    CombSet* Dpos   = dilateSet(A, 3);
    CombSet* Dneg   = dilateSet(A, -2);
    printf("A + 10 = "); printSet(Tplus);
    printf("A - 1  = "); printSet(Tminus);
    printf("3*A    = "); printSet(Dpos);
    printf("-2*A   = "); printSet(Dneg);
    freeCombset(Tplus); freeCombset(Tminus); freeCombset(Dpos); freeCombset(Dneg);

    CombSet* M = copyCombset(A);
    printf("M          = "); printSet(M);
    addElement(M, 5);
    printf("M + {5}    = "); printSet(M);
    addElement(M, 2);  // duplicate — should be a no-op after normalize
    printf("M + {2}    = "); printSet(M);
    removeElement(M, 3);
    printf("M - {3}    = "); printSet(M);
    removeElement(M, 99);  // not present
    printf("M - {99}   = "); printSet(M);
    freeCombset(M);

    /* ---------- Extra properties ---------- */
    section("Extra properties");
    printf("|A+A| = %d\n", adsCard(A));
    printf("|A-A| = %d\n", ddsCard(A));
    printf("|A*A| = %d\n", mdsCard(A));
    printf("isArithmeticProgression(A)  = %d\n", isArithmeticProgression(A));
    printf("isArithmeticProgression(AP) = %d\n", isArithmeticProgression(AP));
    printf("isArithmeticProgression(GP) = %d\n", isArithmeticProgression(GP));
    printf("isGeometricProgression(A)   = %d\n", isGeometricProgression(A));
    printf("isGeometricProgression(AP)  = %d\n", isGeometricProgression(AP));
    printf("isGeometricProgression(GP)  = %d\n", isGeometricProgression(GP));

    /* ---------- Distance measures ---------- */
    section("Distance measures");
    printf("ruzsaDistance(A, B)         = %Lf\n", ruzsaDistance(A, B));
    printf("ruzsaDistancePositive(A, B) = %Lf\n", ruzsaDistancePositive(A, B));
    printf("ruzsaDistance(A, A)         = %Lf\n", ruzsaDistance(A, A));

    /* ---------- Representation functions ---------- */
    section("Representation functions");
    // For A = {1,2,3}: r_add(4) should count (1,3),(2,2),(3,1) = 3
    printf("repAdd(A, 4)     = %lld  (expect 3)\n", repAdd(A, 4));
    printf("repAdd(A, 2)     = %lld  (expect 1: (1,1))\n", repAdd(A, 2));
    printf("repDiff(A, 0)    = %lld  (expect 3)\n", repDiff(A, 0));
    printf("repDiff(A, 1)    = %lld  (expect 2)\n", repDiff(A, 1));
    printf("repMult(A, 6)    = %lld  (expect 2: (2,3),(3,2))\n", repMult(A, 6));
    printf("kRepAdd(A, 6, 3) = %lld\n", kRepAdd(A, 6, 3));
    printf("kRepDiff(A, 0, 3)= %lld\n", kRepDiff(A, 0, 3));
    printf("kRepMult(A, 8, 3)= %lld\n", kRepMult(A, 8, 3));

    /* ---------- Energies ---------- */
    section("Energies");
    // For an arithmetic progression of length n, E(A) = (2n^3 + n)/3
    // AP has length 5: (250 + 5)/3 = 85
    printf("addEnergy(A)         = %lld\n", addEnergy(A));
    printf("diffEnergy(A)        = %lld\n", diffEnergy(A));
    printf("multEnergy(A)        = %lld\n", multEnergy(A));
    printf("addEnergy(AP)        = %lld  (expect 85 for 5-term AP)\n", addEnergy(AP));
    printf("kEnergyAdd(A, 3)     = %lld\n", kEnergyAdd(A, 3));
    printf("kEnergyDiff(A, 3)    = %lld\n", kEnergyDiff(A, 3));
    printf("kEnergyMult(A, 3)    = %lld\n", kEnergyMult(A, 3));

    /* ---------- Cleanup ---------- */
    freeCombset(sum); freeCombset(diff); freeCombset(prod);
    freeCombset(Aads); freeCombset(Adds); freeCombset(Amds);
    freeCombset(A3add); freeCombset(A3sub); freeCombset(A3mul);
    freeCombset(C);
    freeCombset(Acopy);
    freeCombset(A); freeCombset(B); freeCombset(AP); freeCombset(GP); freeCombset(D);
    return 0;
}
