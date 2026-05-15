#ifndef USAGI_H
#define USAGI_H

#include <stdbool.h>
#include <stddef.h>
#include "hebi.h"
#include "sokko.h"

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

typedef enum {
    GROUP_ELEM_INDEXED,
    GROUP_ELEM_INT,
    GROUP_ELEM_ZN,
    GROUP_ELEM_PERMUTATION,
    GROUP_ELEM_DIHEDRAL,
    GROUP_ELEM_PRODUCT,
    GROUP_ELEM_MATRIX
} GroupElementType;

typedef enum {
    GROUP_CAYLEY,
    GROUP_ZN,
    GROUP_Z,
    GROUP_SYMMETRIC,
    GROUP_ALTERNATING,
    GROUP_DIHEDRAL,
    GROUP_Q8,
    GROUP_PRODUCT,
    GROUP_QUOTIENT,
    GROUP_MATRIX
} GroupType;

typedef enum {
    RING_ELEM_INDEXED,
    RING_ELEM_INT,
    RING_ELEM_ZN,
    RING_ELEM_FF,
    RING_ELEM_PRODUCT,
    RING_ELEM_MATRIX
} RingElementType;

typedef enum {
    RING_CAYLEY,
    RING_ZN,
    RING_Z,
    RING_PRODUCT,
    RING_QUOTIENT,
    RING_MATRIX,
    RING_FF
} RingType;

typedef struct {
    long long a;
    bool b;
} DihedralElementData;

typedef struct {
    GroupElement** factors;
    size_t count;
} ProductGroupElementData;

typedef struct {
    RingElement** factors;
    size_t count;
} ProductRingElementData;

typedef struct {
    GroupElement** elements;
    int** table;
} CayleyGroupData;

typedef struct {
    GroupElement** elements;
    int** table;
    int tableLen;
    bool skipValidate;
} TableGroupConstructionData;

typedef struct {
    long long modulus;
} ZnGroupData;

typedef struct {
    size_t degree;
} PermutationGroupData;

typedef struct {
    long long n;
} DihedralGroupData;

typedef struct {
    Group** factors;
    size_t count;
} ProductGroupData;

typedef struct {
    Group* ambient;
    SubGroup* normal;
    int* normalIndices;
    size_t normalCard;
    int* representativeIndices;
    size_t quotientCard;
} QuotientGroupData;

typedef struct {
    GroupElement* (*identity)(Group* group);
    GroupElement* (*multiply)(GroupElement* left, GroupElement* right);
    GroupElement* (*inverse)(GroupElement* element);
    bool (*equals)(GroupElement* left, GroupElement* right);
} GroupOps;

typedef struct {
    RingElement** elements;
    int** addTable;
    int** multTable;
} CayleyRingData;

typedef struct {
    RingElement** elements;
    int** addTable;
    int** multTable;
    int tableLen;
    bool skipValidate;
} TableRingConstructionData;

typedef struct {
    long long modulus;
} ZnRingData;

typedef struct {
    int p;
    int degree;
    int* modulus;
} FiniteFieldRingData;

typedef struct {
    Ring** factors;
    size_t count;
} ProductRingData;

typedef struct {
    Ring* ambient;
    Ideal* ideal;
} QuotientRingData;

typedef struct {
    RingElement* (*zero)(Ring* ring);
    RingElement* (*one)(Ring* ring);
    RingElement* (*add)(RingElement* left, RingElement* right);
    RingElement* (*multiply)(RingElement* left, RingElement* right);
    RingElement* (*addInverse)(RingElement* element);
    RingElement* (*multInverse)(RingElement* element);
    bool (*equals)(RingElement* left, RingElement* right);
} RingOps;

typedef enum {
    SUBGROUP_INDEXED,
    SUBGROUP_GENERATED,
    SUBGROUP_EXPLICIT,
    SUBGROUP_PREDICATE
} SubGroupType;

typedef enum {
    GROUP_COSET_INDEXED,
    GROUP_COSET_REPRESENTATIVE,
    GROUP_COSET_EXPLICIT
} GroupCosetType;

typedef enum {
    GROUP_HOM_INDEXED,
    GROUP_HOM_FUNCTION
} GroupHomomorphismType;

typedef enum {
    SUBRING_INDEXED,
    SUBRING_GENERATED,
    SUBRING_EXPLICIT,
    SUBRING_PREDICATE
} SubRingType;

typedef enum {
    IDEAL_LEFT,
    IDEAL_RIGHT,
    IDEAL_TWO_SIDED
} IdealSide;

typedef enum {
    IDEAL_INDEXED,
    IDEAL_GENERATED,
    IDEAL_EXPLICIT,
    IDEAL_PREDICATE
} UsagiIdealType;

typedef enum {
    RING_HOM_INDEXED,
    RING_HOM_FUNCTION
} RingHomomorphismType;

typedef struct {
    int* indices;
} IndexedSubGroupData;

typedef struct {
    GroupElement** generators;
    size_t count;
} GeneratedSubGroupData;

typedef struct {
    GroupElement** elements;
    size_t count;
} ExplicitSubGroupData;

typedef struct {
    bool (*contains)(SubGroup* subgroup, GroupElement* element);
} PredicateSubGroupData;

typedef struct {
    bool (*contains)(SubGroup* subgroup, GroupElement* element);
    bool (*equals)(SubGroup* left, SubGroup* right);
} SubGroupOps;

typedef struct {
    int* indices;
} IndexedGroupCosetData;

typedef struct {
    GroupElement* representative;
} RepresentativeGroupCosetData;

typedef struct {
    GroupElement** elements;
    size_t count;
} ExplicitGroupCosetData;

typedef struct {
    bool (*contains)(GroupCoset* coset, GroupElement* element);
    bool (*equals)(GroupCoset* left, GroupCoset* right);
} GroupCosetOps;

typedef struct {
    int* mapping;
    size_t count;
} IndexedGroupHomomorphismData;

typedef struct {
    GroupElement* (*apply)(GroupHomomorphism* homomorphism, GroupElement* element);
} FunctionGroupHomomorphismData;

typedef struct {
    int* indices;
} IndexedSubRingData;

typedef struct {
    RingElement** generators;
    size_t count;
} GeneratedSubRingData;

typedef struct {
    RingElement** elements;
    size_t count;
} ExplicitSubRingData;

typedef struct {
    bool (*contains)(SubRing* subring, RingElement* element);
} PredicateSubRingData;

typedef struct {
    bool (*contains)(SubRing* subring, RingElement* element);
    bool (*equals)(SubRing* left, SubRing* right);
} SubRingOps;

typedef struct {
    int* indices;
} IndexedIdealData;

typedef struct {
    RingElement** generators;
    size_t count;
} GeneratedIdealData;

typedef struct {
    RingElement** elements;
    size_t count;
} ExplicitIdealData;

typedef struct {
    bool (*contains)(Ideal* ideal, RingElement* element);
} PredicateIdealData;

typedef struct {
    bool (*contains)(Ideal* ideal, RingElement* element);
    bool (*equals)(Ideal* left, Ideal* right);
} IdealOps;

typedef struct {
    int* mapping;
    size_t count;
} IndexedRingHomomorphismData;

typedef struct {
    RingElement* (*apply)(RingHomomorphism* homomorphism, RingElement* element);
} FunctionRingHomomorphismData;

struct GroupElement {
    char* repr;
    Group* group;
    int index; // >= 0 when the group is finite and indexed, else -1
    GroupElementType type;
    union {
        int indexValue;                                 // Cayley or finite indexed element
        long long integer;                              // Z
        long long znVal;                                // Zn
        long long* perm;                                // permutation in Sn or An
        DihedralElementData dihedral;                   // s^{b mod 2} * r^a
        ProductGroupElementData product;                // direct-product element
        Matrix* matrix;
        void* ptr;
    } data;
};

struct Group {
    GroupType type;
    size_t card;
    bool isFinite;
    GroupElement** elements;            // NULL when the group is not finite/enumerated
    GroupElement** generators;
    size_t numGenerators;
    GroupOps ops;
    union {
        CayleyGroupData cayley;
        ZnGroupData zn;
        PermutationGroupData permutation;
        DihedralGroupData dihedral;
        ProductGroupData product;
        QuotientGroupData quotient;
        Matrix* matrixPrototype;
        void* ptr;
    } data;
};

struct SubGroup {
    Group* ambient;
    size_t card;
    bool isFinite;
    GroupElement** elements;            // NULL when the subgroup is not finite/enumerated
    GroupElement** generators;
    size_t numGenerators;
    SubGroupType type;
    SubGroupOps ops;
    union {
        IndexedSubGroupData indexed;
        GeneratedSubGroupData generated;
        ExplicitSubGroupData explicitElements;
        PredicateSubGroupData predicate;
        void* ptr;
    } data;
};

struct GroupCoset {
    Group* group;
    SubGroup* subgroup;
    size_t card;
    bool isFinite;
    bool isLeft;
    GroupElement** elements;            // NULL when the coset is not finite/enumerated
    GroupElement* representative;
    GroupCosetType type;
    GroupCosetOps ops;
    union {
        IndexedGroupCosetData indexed;
        RepresentativeGroupCosetData representativeData;
        ExplicitGroupCosetData explicitElements;
        void* ptr;
    } data;
};

struct GroupHomomorphism {
    Group* domain;
    Group* codomain;
    GroupHomomorphismType type;
    union {
        IndexedGroupHomomorphismData indexed;
        FunctionGroupHomomorphismData function;
        void* ptr;
    } data;
};

struct RingElement {
    char* repr;
    Ring* ring;
    int index; // >= 0 when the ring is finite and indexed, else -1
    RingElementType type;
    union {
        int indexValue;                                 // Cayley or finite indexed element
        long long integer;                              // Z
        long long znVal;                                // Zn
        ProductRingElementData product;                 // direct-product element
        Matrix* matrix;
        void* ptr;
    } data;
};

struct Ring {
    RingType type;
    size_t card;
    bool isFinite;
    RingElement** elements;             // NULL when the ring is not finite/enumerated
    RingElement** generators;
    size_t numGenerators;
    RingOps ops;
    union {
        CayleyRingData cayley;
        ZnRingData zn;
        FiniteFieldRingData ff;
        ProductRingData product;
        QuotientRingData quotient;
        Matrix* matrixPrototype;
        void* ptr;
    } data;
};

struct SubRing {
    Ring* ambient;
    size_t card;
    bool isFinite;
    RingElement** elements;             // NULL when the subring is not finite/enumerated
    RingElement** generators;
    size_t numGenerators;
    SubRingType type;
    SubRingOps ops;
    union {
        IndexedSubRingData indexed;
        GeneratedSubRingData generated;
        ExplicitSubRingData explicitElements;
        PredicateSubRingData predicate;
        void* ptr;
    } data;
};

struct Ideal {
    Ring* ring;
    size_t card;
    bool isFinite;
    RingElement** elements;             // NULL when the ideal is not finite/enumerated
    RingElement** generators;
    size_t numGenerators;
    IdealSide side;
    UsagiIdealType type;
    IdealOps ops;
    union {
        IndexedIdealData indexed;
        GeneratedIdealData generated;
        ExplicitIdealData explicitElements;
        PredicateIdealData predicate;
        void* ptr;
    } data;
};

struct RingHomomorphism {
    Ring* domain;
    Ring* codomain;
    RingHomomorphismType type;
    union {
        IndexedRingHomomorphismData indexed;
        FunctionRingHomomorphismData function;
        void* ptr;
    } data;
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
bool validateGroupTable(int** table, size_t card);
bool validateRingTables(int** addTable, int** multTable, size_t card);
GroupElement* constructGroupElement(Group* group, void* data);
Group* constructGroup(GroupType type, void* data);
Group* constructTableGroup(GroupElement** elements, int** table, int tableLen);
Group* constructTableGroupSkipValidate(GroupElement** elements, int** table, int tableLen);
RingElement* constructRingElement(Ring* ring, void* data);
Ring* constructRing(RingType type, void* data);
Ring* constructTableRing(RingElement** elements, int** addTable, int** multTable, int tableLen);
Ring* constructTableRingSkipValidate(RingElement** elements, int** addTable, int** multTable, int tableLen);

/* ---------- Repr helpers ---------- */
char* flattenReprInner(const char* s);
char* flattenRepr(const char* repr);

/* ---------- Permutation helpers ---------- */
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
Ring* constructFiniteField(int p, int k);
Ring* constructFiniteFieldOfOrder(int q);
Group* constructAddGroup(Ring* R);
Group* constructUnitGroup(Ring* R);
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
GroupElement* groupAssociator(GroupElement* g, GroupElement* h, GroupElement* k);
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
Group* subgroupAsGroup(SubGroup* H);

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
Ring* quotientRing(Ring* R, Ideal* I);

/* ---------- Ring homomorphisms ---------- */
RingHomomorphism* constructRingHomomorphism(Ring* domain, Ring* codomain, int* indicesMapping, int indicesMappingSize);
Ideal* ringHomomorphismKernel(RingHomomorphism* homo);
RingElement* ringElementImage(RingHomomorphism* homo, RingElement* x);
RingElement* ringCommutator(RingElement* x, RingElement* y);
RingElement* ringAssociator(RingElement* x, RingElement* y, RingElement* z);
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
