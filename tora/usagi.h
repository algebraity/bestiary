#ifndef USAGI_H
#define USAGI_H

#include <stdbool.h>

typedef struct Group Group;
typedef struct GroupElement GroupElement;
typedef struct SubGroup SubGroup;
typedef struct GroupCoset GroupCoset;
typedef struct GroupHomomorphism GroupHomomorphism;
typedef struct Ring Ring;
typedef struct RingElement RingElement;
typedef struct SubRing SubRing;
typedef struct Ideal Ideal;
typedef struct RingHomomorphism RingHomomorphism;

struct GroupElement {
    char* repr;
    Group* group;
    int index;
};

struct Group {
    GroupElement** elements;
    int** table;
    int card;
};

struct SubGroup {
    Group* ambient;
    int* indices;
    int card;
};

struct GroupCoset {
    Group* group;
    SubGroup* subgroup;
    int* indices; // indices of the representatives of the cosets
    bool isLeft;
};

struct GroupHomomorphism {
    Group* domain;
    Group* codomain;
    int* mapping; // mapping[i] = j means domain->elements[i] maps to codomain->elements[j]
};

struct RingElement {
    char* repr;
    Ring* ring;
    int index;
};

struct Ring {
    RingElement** elements;
    int** addTable;
    int** multTable;
    int card;
};

struct SubRing {
    Ring* ambient;
    int* indices;
    int card;
};

struct Ideal {
    Ring* ring;
    int* indices;
    int card;
    int isLeft;
};

struct RingHomomorphism {
    Ring* domain;
    Ring* codomain;
    int* mapping;
};

/* ---------- Free methods ---------- */
void freeGroupElement(GroupElement* g);
void freeGroup(Group* group);
void freeSubgroup(SubGroup* subgroup);
void freeGroupCoset(GroupCoset* coset);
void freeGroupHomomorphism(GroupHomomorphism* homo);
void freeRingHomomorphism(RingHomomorphism* homo);
void freeRingElement(RingElement* x);
void freeRing(Ring* ring);

/* ---------- Construct methods ---------- */
bool validateGroupTable(int** table, int card);
bool validateRingTables(int** addTable, int** multTable, int card);
GroupElement* constructGroupElement(Group* group, char* repr);
Group* constructGroup(GroupElement** elements, int** table, int tableLen);
Group* constructGroupSkipValidate(GroupElement** elements, int** table, int tableLen);
RingElement* constructRingElement(Ring* ring, char* repr);
Ring* constructRing(RingElement** elements, int** addTable, int** multTable, int tableLen);

/* ---------- Repr helpers ---------- */
char* flattenReprInner(const char* s);
char* flattenRepr(const char* repr);

/* ---------- Permutation helpers ---------- */
int factorial(int n);
int permToIndex(int* perm, int n);
int permSign(int* perm, int n);
bool nextPermutation(int* perm, int n);
char* permRepr(int* perm, int n);

/* ---------- Common constructors ---------- */
Group* constructZnGroup(int n);
Group* constructZnProductGroup(int* vals, int k);
Group* constructSymmetricGroup(int n);
Group* constructAlternatingGroup(int n);
Group* constructDihedralGroup(int n);
Group* constructQ8(void);
Ring* constructZnRing(int n);
Ring* constructZnProductRing(int* vals, int k);
Ring* primeFiniteField(int p);
bool isPrime(int p);

/* ---------- Compare methods ---------- */
bool cmpGroupElements(GroupElement* g, GroupElement* h);
bool cmpGroups(Group* G, Group* H);
bool cmpSubgroups(SubGroup* H, SubGroup* K);
bool cmpGroupCosets(GroupCoset* A, GroupCoset* B);
bool cmpRingElements(RingElement* x, RingElement* y);
bool cmpRings(Ring* R, Ring* S);

/* ---------- Basic properties ---------- */
bool isInGroup(Group* G, GroupElement* g);
bool isGroupIdentity(Group* G, GroupElement* g);
GroupElement* groupIdentity(Group* G);
bool isRingAddIdentity(Ring* R, RingElement* x);
RingElement* ringAddIdentity(Ring* R);
bool hasMultIdentity(Ring* R);
bool isRingMultIdentity(Ring* R, RingElement* x);
RingElement* ringMultIdentity(Ring* R);
bool elementsCommute(GroupElement* g, GroupElement* h);
bool isTrivialGroup(Group* G);
bool isTrivialRing(Ring* R);
int elementOrder(Group* G, GroupElement* g);
int additiveOrder(Ring* R, RingElement* x);
int multiplicativeOrder(Ring* R, RingElement* x);

/* ---------- Basic operations ---------- */
GroupElement* groupMult(GroupElement* g, GroupElement* h);
RingElement* ringAdd(RingElement* x, RingElement* y);
RingElement* ringMult(RingElement* x, RingElement* y);
GroupElement* groupInverse(GroupElement* g);
RingElement* ringAddInverse(RingElement* x);
RingElement* ringMultInverse(RingElement* x);
GroupElement* groupExp(GroupElement* g, int k);
RingElement* ringTimes(RingElement* x, int k);
RingElement* ringExp(RingElement* x, int k);
GroupElement* groupElementConjugate(GroupElement* g, GroupElement* h);
GroupElement* groupCommutator(GroupElement* g, GroupElement* h);
Group* trivialGroup(void);
Ring* trivialRing(void);

/* ---------- Subgroups ---------- */
SubGroup* constructSubgroup(Group* G, int* indices, int indicesLen);
bool isSubgroup(Group* G, SubGroup* H);
bool isNormalSubgroup(Group* G, SubGroup* H);
SubGroup* subgroupGeneratedBy(Group* G, int* genIndices, int genIndicesLen);
SubGroup* trivialSubgroup(void);
bool isTrivialSubgroup(SubGroup* H);
SubGroup* groupCenter(Group* G);
SubGroup* groupCentralizer(Group* G, GroupElement* g);
SubGroup* groupNormalizer(SubGroup* H);
int* conjugacyClass(GroupElement* g, int* count);
SubGroup* normalClosure(SubGroup* H);
bool isInSubgroup(SubGroup* H, GroupElement* g);
bool isWholeGroup(Group* G, SubGroup* H);
bool subgroupContains(SubGroup* H, SubGroup* K);
int  subgroupIndex(SubGroup* H);
SubGroup* subgroupConjugate(GroupElement* g, SubGroup* H);
SubGroup* commutatorSubgroup(Group* G);
Group* groupAbelianization(Group* G);
SubGroup** listAllSubgroups(Group* G, int* count);
SubGroup** listAllNormalSubgroups(Group* G, int* count);
SubGroup** listAllMaximalSubgroups(Group* G, int* count);
SubGroup* largestCoreFreeSubgroup(Group* G);

/* ---------- Cyclic groups ---------- */
bool isCyclicGroup(Group* G);
bool isCyclicSubgroup(SubGroup* H);
bool generatesCyclicGroup(Group* G, GroupElement* g);
bool generatesCyclicSubgroup(SubGroup* H, GroupElement* g);
SubGroup* getCyclicSubgroup(GroupElement* g);

/* ---------- Quotient groups ---------- */
bool isInGroupCoset(GroupCoset* coset, GroupElement* g);
GroupCoset* generateLeftGroupCoset(SubGroup* H, GroupElement* g);
GroupCoset* generateRightGroupCoset(SubGroup* H, GroupElement* g);
GroupCoset** getLeftGroupCosets(SubGroup* H);
GroupCoset** getRightGroupCosets(SubGroup* H);
Group* quotientGroup(Group* G, SubGroup* N);

/* ---------- Derived groups ---------- */
Group* constructProductGroup(Group* G, Group* H);
Group* kfoldProductGroup(Group* G, int k);
Ring* constructProductRing(Ring* R, Ring* S);
Ring* kfoldProductRing(Ring* R, int k);

/* ---------- Group homomorphisms ---------- */
GroupHomomorphism* constructGroupHomomorphism(Group* domain, Group* codomain, int* indicesMapping, int indicesMappingSize);
SubGroup* groupHomomorphismKernel(GroupHomomorphism* homo);
GroupElement* groupElementImage(GroupHomomorphism* homo, GroupElement* g);
Group* groupHomomorphismImage(GroupHomomorphism* homo);

/* ---------- Ideals, subrings, and quotient rings ---------- */
SubRing* constructSubring(Ring* R, int* indices, int indicesLen);
bool cmpSubrings(SubRing* S, SubRing* T);
bool isTrivialSubring(SubRing* S);
bool isWholeRing(SubRing* S);
Ideal* constructLeftIdeal(Ring* R, int* indices, int indicesLen);
Ideal* constructRightIdeal(Ring* R, int* indices, int indicesLen);
bool cmpIdeals(Ideal* I, Ideal* J);
Ideal* addIdeals(Ideal* I, Ideal* J);
Ideal* multIdeals(Ideal* I, Ideal* J);

/* ---------- Ring homomorphisms ---------- */
RingHomomorphism* constructRingHomomorphism(Ring* domain, Ring* codomain, int* indicesMapping, int indicesMappingSize);
Ideal* ringHomomorphismKernel(RingHomomorphism* homo);
RingElement* ringElementImage(RingHomomorphism* homo, RingElement* x);
Ring* ringHomomorphismImage(RingHomomorphism* homo);
bool isRingIsomorphism(RingHomomorphism* homo);

/* ---------- Classifications ---------- */
bool isCommutativeGroup(Group* G);
bool isCommutativeRing(Ring* R);
bool isSimple(Group* G);
bool isInverse(GroupElement* g, GroupElement* h);
bool isAddInverse(RingElement* x, RingElement* y);
bool isMultInverse(RingElement* x, RingElement* y);
bool hasMultInverse(RingElement* x);
bool isZeroDivisor(RingElement* x);
bool hasZeroDivisors(Ring* R);
bool isIntegralDomain(Ring* R);
bool isDivisionRing(Ring* R);
bool isField(Ring* R);
bool isGroupIsomorphism(GroupHomomorphism* homo);
bool isRingIsomorphism(RingHomomorphism* homo);

#endif