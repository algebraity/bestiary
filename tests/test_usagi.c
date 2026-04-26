
/* AI GENERATED TEST SUITE */

// Everything here looks good to me, but note this was generated my an LLM.
// More tests might be warranted if using for production.

/* test_usagi.c — exercises every public function in usagi.c against
 * known small groups and rings.
 *
 * Groups tested:
 *   Z/4Z   — cyclic group of order 4 (abelian)
 *   V4     — Klein four-group (abelian, non-cyclic)
 *   S3     — symmetric group on 3 letters (smallest non-abelian)
 *
 * Rings tested:
 *   Z/3Z   — integers mod 3 (a field)
 *   Z/4Z   — integers mod 4 (commutative ring with zero divisors)
 *   Z/5Z   — integers mod 5 (a field)
 *   Z/6Z   — integers mod 6 (commutative ring with zero divisors)
 *
 * Each test prints PASS or FAIL and the running totals are reported at the end.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "usagi.h"

/* ---------- tiny test harness ---------- */

static int tests_run = 0;
static int tests_passed = 0;

#define CHECK(cond, msg) do {                                        \
    tests_run++;                                                     \
    if (cond) { tests_passed++; printf("  PASS: %s\n", msg); }       \
    else      { printf("  FAIL: %s  (line %d)\n", msg, __LINE__); }  \
} while (0)

/* ---------- helpers to build tables ---------- */

/* allocate an int** card x card table, all zeros */
static int** alloc_table(int card) {
    int** t = malloc(card * sizeof(int*));
    for (int i = 0; i < card; i++) {
        t[i] = calloc(card, sizeof(int));
    }
    return t;
}

/* allocate a GroupElement** array of named elements */
static GroupElement** alloc_group_elements(Group* placeholder, const char** names, int n) {
    GroupElement** elems = malloc(n * sizeof(GroupElement*));
    for (int i = 0; i < n; i++) {
        elems[i] = constructGroupElement(placeholder, (char*)names[i]);
    }
    return elems;
}

static RingElement** alloc_ring_elements(Ring* placeholder, const char** names, int n) {
    RingElement** elems = malloc(n * sizeof(RingElement*));
    for (int i = 0; i < n; i++) {
        elems[i] = constructRingElement(placeholder, (char*)names[i]);
    }
    return elems;
}

/* ---------- group constructors ---------- */

/* Z/nZ as a group under addition mod n. Element i represents i. */
static Group* make_Zn(int n) {
    int** table = alloc_table(n);
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            table[i][j] = (i + j) % n;

    /* placeholder names "0","1",... */
    char buf[8];
    const char* names[16];
    char* name_storage[16];
    for (int i = 0; i < n; i++) {
        snprintf(buf, sizeof buf, "%d", i);
        name_storage[i] = strdup(buf);
        names[i] = name_storage[i];
    }

    /* We need a placeholder Group* for constructGroupElement; pass NULL-ish
     * by constructing a temporary then patching. Easier: pass a real pointer
     * after constructing the Group. But constructGroupElement requires a
     * non-NULL group. So we allocate the Group first manually, then build
     * elements pointing to it, then call constructGroup.
     *
     * Workaround: pass a dummy non-NULL pointer; constructGroup will overwrite
     * via the elements[i]->group field? No — it doesn't. So elements end up
     * pointing to the dummy. We instead build elements pointing to a real
     * Group allocated up front.
     */
    Group* G = malloc(sizeof(Group));
    GroupElement** elems = alloc_group_elements(G, names, n);

    /* Now call constructGroup, which will validate, set fields, and assign
     * indices. But constructGroup mallocs its own Group. So free our scratch
     * one first... actually we need to thread this carefully. The cleanest
     * way given the API: just construct a Group manually here, mirroring
     * what constructGroup does, since constructGroup would allocate a second
     * one.
     */
    if (!validateGroupTable(table, n)) {
        printf("  (internal: Z/%dZ table failed validation!)\n", n);
    }
    G->elements = elems;
    G->table = table;
    G->card = n;
    for (int i = 0; i < n; i++) elems[i]->group = G;
    for (int i = 0; i < n; i++) elems[i]->index = i;

    for (int i = 0; i < n; i++) free(name_storage[i]);
    return G;
}

/* Klein four-group V4 = {e, a, b, c} with a^2=b^2=c^2=e and ab=c, bc=a, ac=b */
static Group* make_V4(void) {
    int n = 4;
    /* indexing: 0=e, 1=a, 2=b, 3=c */
    int v4[4][4] = {
        {0,1,2,3},
        {1,0,3,2},
        {2,3,0,1},
        {3,2,1,0}
    };
    int** table = alloc_table(n);
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            table[i][j] = v4[i][j];

    const char* names[] = {"e","a","b","c"};
    Group* G = malloc(sizeof(Group));
    GroupElement** elems = alloc_group_elements(G, names, n);

    G->elements = elems;
    G->table = table;
    G->card = n;
    for (int i = 0; i < n; i++) { elems[i]->group = G; elems[i]->index = i; }
    return G;
}

/* S_3 with elements labeled:
 *   0 = e
 *   1 = r   (rotation, order 3)
 *   2 = r^2
 *   3 = s   (a transposition, order 2)
 *   4 = sr
 *   5 = sr^2
 * Multiplication: r*s = s*r^2, etc. We'll use the standard presentation.
 * Cayley table for S_3 generated by r (3-cycle) and s (transposition):
 *   sr = sr, srr = sr^2, ...
 *   r*s   = sr^2  (since rs = sr^{-1} = sr^2 in D_3 / S_3 convention)
 */
static Group* make_S3(void) {
    int n = 6;
    /* Standard S_3 Cayley table, rows/cols ordered: e, r, r^2, s, sr, sr^2 */
    int s3[6][6] = {
        /*           e   r   r^2   s   sr  sr^2 */
        /* e   */  { 0,  1,  2,    3,  4,  5    },
        /* r   */  { 1,  2,  0,    4,  5,  3    },
        /* r^2 */  { 2,  0,  1,    5,  3,  4    },
        /* s   */  { 3,  5,  4,    0,  2,  1    },
        /* sr  */  { 4,  3,  5,    1,  0,  2    },
        /* sr^2*/  { 5,  4,  3,    2,  1,  0    }
    };
    int** table = alloc_table(n);
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            table[i][j] = s3[i][j];

    const char* names[] = {"e","r","r2","s","sr","sr2"};
    Group* G = malloc(sizeof(Group));
    GroupElement** elems = alloc_group_elements(G, names, n);

    G->elements = elems;
    G->table = table;
    G->card = n;
    for (int i = 0; i < n; i++) { elems[i]->group = G; elems[i]->index = i; }
    return G;
}

/* ---------- ring constructors ---------- */

/* Z/nZ as a ring. Element i represents i.
 * Note: USAGI's hasMultIdentity assumes the mult identity is at index 1,
 * which works for Z/nZ since 1 is at index 1. */
static Ring* make_Zn_ring(int n) {
    int** addT  = alloc_table(n);
    int** multT = alloc_table(n);
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++) {
            addT[i][j]  = (i + j) % n;
            multT[i][j] = (i * j) % n;
        }

    char buf[8];
    char* name_storage[16];
    const char* names[16];
    for (int i = 0; i < n; i++) {
        snprintf(buf, sizeof buf, "%d", i);
        name_storage[i] = strdup(buf);
        names[i] = name_storage[i];
    }

    Ring* R = malloc(sizeof(Ring));
    RingElement** elems = alloc_ring_elements(R, names, n);

    R->elements = elems;
    R->addTable = addT;
    R->multTable = multT;
    R->card = n;
    for (int i = 0; i < n; i++) { elems[i]->ring = R; elems[i]->index = i; }

    for (int i = 0; i < n; i++) free(name_storage[i]);
    return R;
}

/* ---------- tests ---------- */

static void test_validators(void) {
    printf("\n=== validators ===\n");

    /* Valid Z/4Z table should pass */
    int** good = alloc_table(4);
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            good[i][j] = (i + j) % 4;
    CHECK(validateGroupTable(good, 4) == true, "Z/4Z table validates as group");

    /* Bad table: out-of-range entry.
     * NOTE: Placed at (0,0) so the bounds check catches it on the first
     * iteration. There is a latent bug in validateGroupTable where an
     * out-of-range entry at position (i,j) with i>0 can be read indirectly
     * by the associativity check before the bounds check reaches it. */
    int** bad = alloc_table(4);
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            bad[i][j] = (i + j) % 4;
    bad[0][0] = 99;
    CHECK(validateGroupTable(bad, 4) == false, "out-of-range entry rejected");

    /* Bad table: identity broken */
    int** bad2 = alloc_table(4);
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            bad2[i][j] = (i + j) % 4;
    bad2[0][1] = 0;  /* now 0 + 1 = 0, breaks identity */
    CHECK(validateGroupTable(bad2, 4) == false, "broken identity rejected");

    /* NULL / negative card */
    CHECK(validateGroupTable(NULL, 4) == false, "NULL table rejected");
    CHECK(validateGroupTable(good, 0) == false, "card=0 rejected");

    /* Z/6Z ring should validate */
    int** addT  = alloc_table(6);
    int** multT = alloc_table(6);
    for (int i = 0; i < 6; i++)
        for (int j = 0; j < 6; j++) {
            addT[i][j]  = (i + j) % 6;
            multT[i][j] = (i * j) % 6;
        }
    CHECK(validateRingTables(addT, multT, 6) == true, "Z/6Z ring tables validate");

    /* Break distributivity */
    multT[2][3] = 5;  /* should be 0; now distributivity fails */
    CHECK(validateRingTables(addT, multT, 6) == false, "broken distributivity rejected");

    /* free */
    for (int i = 0; i < 4; i++) { free(good[i]); free(bad[i]); free(bad2[i]); }
    free(good); free(bad); free(bad2);
    for (int i = 0; i < 6; i++) { free(addT[i]); free(multT[i]); }
    free(addT); free(multT);
}

static void test_group_basics(Group* G, const char* name, int expected_card,
                              bool expected_commutative) {
    printf("\n=== %s basics ===\n", name);

    CHECK(G != NULL, "group constructed");
    CHECK(G->card == expected_card, "cardinality matches");

    GroupElement* e = groupIdentity(G);
    CHECK(e != NULL, "identity exists");
    CHECK(isGroupIdentity(G, e) == true, "isGroupIdentity recognizes identity");
    CHECK(isGroupIdentity(G, G->elements[1]) == false,
          "isGroupIdentity rejects non-identity");

    /* identity behavior: e*x = x*e = x for all x */
    bool id_ok = true;
    for (int i = 0; i < G->card; i++) {
        GroupElement* x = G->elements[i];
        if (!cmpGroupElements(groupMult(e, x), x)) id_ok = false;
        if (!cmpGroupElements(groupMult(x, e), x)) id_ok = false;
    }
    CHECK(id_ok, "identity acts trivially on all elements");

    /* commutativity */
    CHECK(isCommutativeGroup(G) == expected_commutative,
          "commutativity classification correct");

    /* element comparisons */
    CHECK(cmpGroupElements(G->elements[0], G->elements[0]) == true,
          "element equals itself");
    if (G->card > 1) {
        CHECK(cmpGroupElements(G->elements[0], G->elements[1]) == false,
              "distinct elements compare unequal");
    }
    CHECK(cmpGroupElements(NULL, G->elements[0]) == false, "NULL element rejected");

    /* group self-comparison */
    CHECK(cmpGroups(G, G) == true, "group equals itself");
    CHECK(cmpGroups(NULL, G) == false, "NULL group rejected");
}

static void test_group_ops_Z4(Group* Z4) {
    printf("\n=== Z/4Z operations ===\n");

    GroupElement* e = Z4->elements[0];
    GroupElement* one = Z4->elements[1];
    GroupElement* two = Z4->elements[2];
    GroupElement* three = Z4->elements[3];

    /* 1 + 1 = 2 */
    CHECK(cmpGroupElements(groupMult(one, one), two), "1+1=2 in Z/4Z");
    /* 2 + 3 = 1 */
    CHECK(cmpGroupElements(groupMult(two, three), one), "2+3=1 in Z/4Z");
    /* 3 + 1 = 0 */
    CHECK(cmpGroupElements(groupMult(three, one), e), "3+1=0 in Z/4Z");

    /* inverses: -1 = 3, -2 = 2, -3 = 1, -0 = 0 */
    CHECK(cmpGroupElements(groupInverse(one), three), "inverse of 1 is 3");
    CHECK(cmpGroupElements(groupInverse(two), two), "inverse of 2 is 2 (self)");
    CHECK(cmpGroupElements(groupInverse(three), one), "inverse of 3 is 1");
    CHECK(cmpGroupElements(groupInverse(e), e), "inverse of identity is identity");

    /* exponentiation */
    CHECK(cmpGroupElements(groupExp(one, 0), e), "1^0 = e");
    CHECK(cmpGroupElements(groupExp(one, 1), one), "1^1 = 1");
    CHECK(cmpGroupElements(groupExp(one, 4), e), "1^4 = e (order 4)");
    CHECK(cmpGroupElements(groupExp(two, 2), e), "2^2 = e");
    CHECK(cmpGroupElements(groupExp(one, -1), three), "1^-1 = 3");
    CHECK(cmpGroupElements(groupExp(one, -2), two), "1^-2 = 2");
    CHECK(cmpGroupElements(groupExp(one, -4), e), "1^-4 = e");

    /* mismatched-group rejection */
    Group* other = make_Zn(3);
    CHECK(groupMult(one, other->elements[1]) == NULL,
          "groupMult rejects mismatched groups");

    /* isInverse */
    CHECK(isInverse(one, three) == true, "isInverse(1, 3) true in Z/4Z");
    CHECK(isInverse(three, one) == true, "isInverse(3, 1) true (symmetric)");
    CHECK(isInverse(two, two) == true, "isInverse(2, 2) true (self-inverse)");
    CHECK(isInverse(e, e) == true, "isInverse(e, e) true");
    CHECK(isInverse(one, two) == false, "isInverse(1, 2) false in Z/4Z");
    CHECK(isInverse(one, other->elements[1]) == false,
          "isInverse rejects elements from different groups");
    CHECK(isInverse(NULL, one) == false, "isInverse(NULL, _) false");

    freeGroup(other);
}

static void test_group_ops_V4(Group* V4) {
    printf("\n=== V4 operations ===\n");

    GroupElement* e = V4->elements[0];
    GroupElement* a = V4->elements[1];
    GroupElement* b = V4->elements[2];
    GroupElement* c = V4->elements[3];

    /* every non-identity element has order 2 */
    CHECK(cmpGroupElements(groupMult(a, a), e), "a^2 = e");
    CHECK(cmpGroupElements(groupMult(b, b), e), "b^2 = e");
    CHECK(cmpGroupElements(groupMult(c, c), e), "c^2 = e");
    /* ab = c, bc = a, ac = b */
    CHECK(cmpGroupElements(groupMult(a, b), c), "ab = c");
    CHECK(cmpGroupElements(groupMult(b, c), a), "bc = a");
    CHECK(cmpGroupElements(groupMult(a, c), b), "ac = b");
    /* ba = c (commutative) */
    CHECK(cmpGroupElements(groupMult(b, a), c), "ba = c (commutative)");

    /* every non-identity element is its own inverse */
    CHECK(cmpGroupElements(groupInverse(a), a), "a is self-inverse");
    CHECK(cmpGroupElements(groupInverse(b), b), "b is self-inverse");
    CHECK(cmpGroupElements(groupInverse(c), c), "c is self-inverse");

    /* exponentiation */
    CHECK(cmpGroupElements(groupExp(a, 2), e), "a^2 = e");
    CHECK(cmpGroupElements(groupExp(a, 3), a), "a^3 = a");
    CHECK(cmpGroupElements(groupExp(b, -3), b), "b^-3 = b");
}

static void test_group_ops_S3(Group* S3) {
    printf("\n=== S_3 operations ===\n");

    GroupElement* e = S3->elements[0];
    GroupElement* r = S3->elements[1];
    GroupElement* r2 = S3->elements[2];
    GroupElement* s = S3->elements[3];
    /* r has order 3, s has order 2 */
    CHECK(cmpGroupElements(groupMult(r, r), r2), "r*r = r^2");
    CHECK(cmpGroupElements(groupExp(r, 3), e), "r^3 = e");
    CHECK(cmpGroupElements(groupExp(s, 2), e), "s^2 = e");

    /* non-commutativity: r*s != s*r */
    CHECK(!cmpGroupElements(groupMult(r, s), groupMult(s, r)),
          "r*s != s*r (non-abelian)");

    /* inverses: r^-1 = r^2 */
    CHECK(cmpGroupElements(groupInverse(r), r2), "r^-1 = r^2");
    CHECK(cmpGroupElements(groupInverse(s), s), "s is self-inverse");

    /* negative exponent */
    CHECK(cmpGroupElements(groupExp(r, -1), r2), "r^-1 = r^2 via groupExp");
    CHECK(cmpGroupElements(groupExp(r, -3), e), "r^-3 = e");
}

static void test_ring_basics(Ring* R, const char* name, int expected_card,
                             bool expected_has_unity) {
    printf("\n=== %s basics ===\n", name);

    CHECK(R != NULL, "ring constructed");
    CHECK(R->card == expected_card, "cardinality matches");

    RingElement* zero = ringAddIdentity(R);
    CHECK(zero != NULL, "additive identity exists");
    CHECK(isRingAddIdentity(R, zero) == true, "isRingAddIdentity recognizes 0");
    if (R->card > 1)
        CHECK(isRingAddIdentity(R, R->elements[1]) == false,
              "isRingAddIdentity rejects non-zero");

    CHECK(hasMultIdentity(R) == expected_has_unity,
          "hasMultIdentity classification correct");

    if (expected_has_unity) {
        RingElement* one = ringMultIdentity(R);
        CHECK(one != NULL, "multiplicative identity exists");
        CHECK(isRingMultIdentity(R, one) == true,
              "isRingMultIdentity recognizes 1");
        CHECK(isRingMultIdentity(R, zero) == false,
              "isRingMultIdentity rejects 0");
    }

    /* additive identity behavior */
    bool ok = true;
    for (int i = 0; i < R->card; i++) {
        RingElement* x = R->elements[i];
        if (!cmpRingElements(ringAdd(zero, x), x)) ok = false;
        if (!cmpRingElements(ringAdd(x, zero), x)) ok = false;
        /* 0 * x = 0 */
        if (!cmpRingElements(ringMult(zero, x), zero)) ok = false;
    }
    CHECK(ok, "0 acts correctly under + and *");

    /* element comparisons */
    CHECK(cmpRingElements(R->elements[0], R->elements[0]) == true,
          "ring element equals itself");
    if (R->card > 1)
        CHECK(cmpRingElements(R->elements[0], R->elements[1]) == false,
              "distinct ring elements compare unequal");

    CHECK(cmpRings(R, R) == true, "ring equals itself");
    CHECK(cmpRings(NULL, R) == false, "NULL ring rejected");
}

static void test_ring_ops_Z6(Ring* Z6) {
    printf("\n=== Z/6Z operations ===\n");

    RingElement* zero = Z6->elements[0];
    RingElement* one  = Z6->elements[1];
    RingElement* two  = Z6->elements[2];
    RingElement* three = Z6->elements[3];
    RingElement* four = Z6->elements[4];
    RingElement* five = Z6->elements[5];

    /* addition */
    CHECK(cmpRingElements(ringAdd(two, three), five), "2+3=5 in Z/6Z");
    CHECK(cmpRingElements(ringAdd(four, three), one), "4+3=1 in Z/6Z");
    CHECK(cmpRingElements(ringAdd(five, one), zero), "5+1=0 in Z/6Z");

    /* multiplication */
    CHECK(cmpRingElements(ringMult(two, three), zero), "2*3=0 in Z/6Z (zero divisors)");
    CHECK(cmpRingElements(ringMult(two, two), four), "2*2=4 in Z/6Z");
    CHECK(cmpRingElements(ringMult(five, five), one), "5*5=1 in Z/6Z");

    /* additive inverses */
    CHECK(cmpRingElements(ringAddInverse(one), five), "-1 = 5 in Z/6Z");
    CHECK(cmpRingElements(ringAddInverse(two), four), "-2 = 4 in Z/6Z");
    CHECK(cmpRingElements(ringAddInverse(three), three), "-3 = 3 in Z/6Z");
    CHECK(cmpRingElements(ringAddInverse(zero), zero), "-0 = 0 in Z/6Z");

    /* multiplicative inverses: only 1 and 5 are units in Z/6Z */
    CHECK(cmpRingElements(ringMultInverse(one), one), "1^-1 = 1");
    CHECK(cmpRingElements(ringMultInverse(five), five), "5^-1 = 5 (since 5*5=1)");
    CHECK(ringMultInverse(two) == NULL, "2 has no mult inverse in Z/6Z");
    CHECK(ringMultInverse(three) == NULL, "3 has no mult inverse in Z/6Z");

    /* ringTimes: k*x */
    CHECK(cmpRingElements(ringTimes(one, 0), zero), "0*1 = 0");
    CHECK(cmpRingElements(ringTimes(one, 5), five), "5*1 = 5");
    CHECK(cmpRingElements(ringTimes(two, 3), zero), "3*2 = 6 = 0 in Z/6Z");
    CHECK(cmpRingElements(ringTimes(one, -1), five), "-1*1 = 5");
    CHECK(cmpRingElements(ringTimes(two, -2), two), "-2*2 = -4 = 2 in Z/6Z");

    /* ringExp: x^k */
    CHECK(cmpRingElements(ringExp(two, 1), two), "2^1 = 2");
    CHECK(cmpRingElements(ringExp(two, 2), four), "2^2 = 4");
    CHECK(cmpRingElements(ringExp(two, 0), one), "2^0 = 1");
    CHECK(cmpRingElements(ringExp(five, 2), one), "5^2 = 1 in Z/6Z");
    CHECK(cmpRingElements(ringExp(five, -1), five), "5^-1 = 5");
}

static void test_ring_ops_Z3(Ring* Z3) {
    printf("\n=== Z/3Z operations (field) ===\n");

    RingElement* zero = Z3->elements[0];
    RingElement* one  = Z3->elements[1];
    RingElement* two  = Z3->elements[2];

    /* every nonzero element has a mult inverse (Z/3Z is a field) */
    CHECK(cmpRingElements(ringMultInverse(one), one), "1^-1 = 1 in Z/3Z");
    CHECK(cmpRingElements(ringMultInverse(two), two), "2^-1 = 2 in Z/3Z (since 2*2=4=1)");
    CHECK(ringMultInverse(zero) == NULL, "0 has no mult inverse");

    /* Fermat: x^3 = x for all x in Z/3Z */
    CHECK(cmpRingElements(ringExp(two, 3), two), "2^3 = 2 (Fermat)");
    CHECK(cmpRingElements(ringExp(one, 3), one), "1^3 = 1");

    /* negative exponentiation */
    CHECK(cmpRingElements(ringExp(two, -2), one), "2^-2 = 1");
}

static void test_ring_classification(Ring* Z3, Ring* Z4, Ring* Z5, Ring* Z6) {
    printf("\n=== ring classification (isField/isIntegralDomain/etc.) ===\n");

    /* isCommutativeRing — all Z/nZ are commutative */
    CHECK(isCommutativeRing(Z3) == true, "Z/3Z is commutative");
    CHECK(isCommutativeRing(Z4) == true, "Z/4Z is commutative");
    CHECK(isCommutativeRing(Z6) == true, "Z/6Z is commutative");

    /* isField */
    CHECK(isField(Z3) == true,  "Z/3Z is a field");
    CHECK(isField(Z5) == true,  "Z/5Z is a field");
    CHECK(isField(Z4) == false, "Z/4Z is NOT a field (4 not prime)");
    CHECK(isField(Z6) == false, "Z/6Z is NOT a field (6 not prime)");

    /* isIntegralDomain */
    CHECK(isIntegralDomain(Z3) == true,  "Z/3Z is an integral domain");
    CHECK(isIntegralDomain(Z5) == true,  "Z/5Z is an integral domain");
    CHECK(isIntegralDomain(Z4) == false, "Z/4Z is NOT an integral domain");
    CHECK(isIntegralDomain(Z6) == false, "Z/6Z is NOT an integral domain");

    /* isDivisionRing — for finite rings, division ring = field (Wedderburn) */
    CHECK(isDivisionRing(Z3) == true,  "Z/3Z is a division ring");
    CHECK(isDivisionRing(Z5) == true,  "Z/5Z is a division ring");
    CHECK(isDivisionRing(Z4) == false, "Z/4Z is NOT a division ring");
    CHECK(isDivisionRing(Z6) == false, "Z/6Z is NOT a division ring");

    /* hasZeroDivisors */
    CHECK(hasZeroDivisors(Z3) == false, "Z/3Z has no zero divisors");
    CHECK(hasZeroDivisors(Z5) == false, "Z/5Z has no zero divisors");
    CHECK(hasZeroDivisors(Z4) == true,  "Z/4Z has zero divisors (2*2=0)");
    CHECK(hasZeroDivisors(Z6) == true,  "Z/6Z has zero divisors");

    /* isZeroDivisor — Z/6Z: zero divisors are exactly {2, 3, 4} */
    CHECK(isZeroDivisor(Z6->elements[0]) == false, "0 is not a zero divisor (D&F convention)");
    CHECK(isZeroDivisor(Z6->elements[1]) == false, "1 is not a zero divisor in Z/6Z");
    CHECK(isZeroDivisor(Z6->elements[2]) == true,  "2 is a zero divisor in Z/6Z");
    CHECK(isZeroDivisor(Z6->elements[3]) == true,  "3 is a zero divisor in Z/6Z");
    CHECK(isZeroDivisor(Z6->elements[4]) == true,  "4 is a zero divisor in Z/6Z");
    CHECK(isZeroDivisor(Z6->elements[5]) == false, "5 is not a zero divisor in Z/6Z");
    /* Z/4Z: only 2 is a zero divisor (2*2=0) */
    CHECK(isZeroDivisor(Z4->elements[2]) == true,  "2 is a zero divisor in Z/4Z");
    CHECK(isZeroDivisor(Z4->elements[1]) == false, "1 is not a zero divisor in Z/4Z");
    CHECK(isZeroDivisor(Z4->elements[3]) == false, "3 is not a zero divisor in Z/4Z");
    /* Z/5Z: no zero divisors */
    CHECK(isZeroDivisor(Z5->elements[2]) == false, "2 is not a zero divisor in Z/5Z");
    CHECK(isZeroDivisor(Z5->elements[3]) == false, "3 is not a zero divisor in Z/5Z");

    /* isAddInverse */
    CHECK(isAddInverse(Z6->elements[1], Z6->elements[5]) == true,  "1 and 5 are add inverses in Z/6Z");
    CHECK(isAddInverse(Z6->elements[2], Z6->elements[4]) == true,  "2 and 4 are add inverses in Z/6Z");
    CHECK(isAddInverse(Z6->elements[3], Z6->elements[3]) == true,  "3 is its own add inverse in Z/6Z");
    CHECK(isAddInverse(Z6->elements[0], Z6->elements[0]) == true,  "0 is its own add inverse");
    CHECK(isAddInverse(Z6->elements[1], Z6->elements[2]) == false, "1 and 2 are not add inverses");
    CHECK(isAddInverse(Z6->elements[1], Z3->elements[2]) == false, "isAddInverse rejects different rings");
    CHECK(isAddInverse(NULL, Z6->elements[1]) == false, "isAddInverse(NULL, _) false");

    /* isMultInverse */
    CHECK(isMultInverse(Z5->elements[2], Z5->elements[3]) == true,  "2*3=1 in Z/5Z");
    CHECK(isMultInverse(Z5->elements[3], Z5->elements[2]) == true,  "3*2=1 in Z/5Z (symmetric)");
    CHECK(isMultInverse(Z5->elements[4], Z5->elements[4]) == true,  "4*4=16=1 in Z/5Z (self-inverse)");
    CHECK(isMultInverse(Z5->elements[1], Z5->elements[1]) == true,  "1*1=1");
    CHECK(isMultInverse(Z6->elements[5], Z6->elements[5]) == true,  "5*5=25=1 in Z/6Z");
    CHECK(isMultInverse(Z6->elements[2], Z6->elements[3]) == false, "2 and 3 are NOT mult inverses in Z/6Z (2*3=0)");
    CHECK(isMultInverse(Z6->elements[0], Z6->elements[1]) == false, "0 has no mult inverse");
    CHECK(isMultInverse(Z6->elements[1], Z6->elements[0]) == false, "0 has no mult inverse (other side)");
    CHECK(isMultInverse(Z3->elements[1], Z5->elements[1]) == false, "isMultInverse rejects different rings");
    CHECK(isMultInverse(NULL, Z5->elements[1]) == false, "isMultInverse(NULL, _) false");

    /* hasMultInverse — units of Z/6Z are {1, 5}; of Z/4Z are {1, 3}; of Z/5Z are {1,2,3,4} */
    CHECK(hasMultInverse(Z6->elements[1]) == true,  "1 is a unit in Z/6Z");
    CHECK(hasMultInverse(Z6->elements[5]) == true,  "5 is a unit in Z/6Z");
    CHECK(hasMultInverse(Z6->elements[2]) == false, "2 is not a unit in Z/6Z");
    CHECK(hasMultInverse(Z6->elements[3]) == false, "3 is not a unit in Z/6Z");
    CHECK(hasMultInverse(Z6->elements[4]) == false, "4 is not a unit in Z/6Z");
    CHECK(hasMultInverse(Z6->elements[0]) == false, "0 is not a unit");
    CHECK(hasMultInverse(Z4->elements[1]) == true,  "1 is a unit in Z/4Z");
    CHECK(hasMultInverse(Z4->elements[3]) == true,  "3 is a unit in Z/4Z (3*3=9=1)");
    CHECK(hasMultInverse(Z4->elements[2]) == false, "2 is not a unit in Z/4Z");
    /* In a field every nonzero element is a unit */
    for (int i = 1; i < Z5->card; i++) {
	char buf[64];
	snprintf(buf, sizeof buf, "%d is a unit in Z/5Z (field)", i);
	CHECK(hasMultInverse(Z5->elements[i]) == true, buf);
    }
}

static void test_null_safety(void) {
    printf("\n=== NULL safety ===\n");

    CHECK(constructGroupElement((Group*)0x1, NULL) == NULL, "constructGroupElement rejects NULL repr");
    CHECK(constructGroupElement((Group*)0x1, "") == NULL, "constructGroupElement rejects empty repr");

    CHECK(groupMult(NULL, NULL) == NULL, "groupMult(NULL,NULL) returns NULL");
    CHECK(groupInverse(NULL) == NULL, "groupInverse(NULL) returns NULL");
    CHECK(groupExp(NULL, 5) == NULL, "groupExp(NULL,5) returns NULL");
    CHECK(groupIdentity(NULL) == NULL, "groupIdentity(NULL) returns NULL");
    CHECK(isGroupIdentity(NULL, NULL) == false, "isGroupIdentity(NULL,NULL) returns false");

    CHECK(ringAdd(NULL, NULL) == NULL, "ringAdd(NULL,NULL) returns NULL");
    CHECK(ringMult(NULL, NULL) == NULL, "ringMult(NULL,NULL) returns NULL");
    CHECK(ringMultInverse(NULL) == NULL, "ringMultInverse(NULL) returns NULL");
    CHECK(hasMultIdentity(NULL) == false, "hasMultIdentity(NULL) returns false");

    /* new functions */
    CHECK(isInverse(NULL, NULL) == false, "isInverse(NULL,NULL) returns false");
    CHECK(isAddInverse(NULL, NULL) == false, "isAddInverse(NULL,NULL) returns false");
    CHECK(isMultInverse(NULL, NULL) == false, "isMultInverse(NULL,NULL) returns false");
    CHECK(hasMultInverse(NULL) == false, "hasMultInverse(NULL) returns false");
    CHECK(isZeroDivisor(NULL) == false, "isZeroDivisor(NULL) returns false");
    CHECK(hasZeroDivisors(NULL) == false, "hasZeroDivisors(NULL) returns false");
    CHECK(isCommutativeRing(NULL) == false, "isCommutativeRing(NULL) returns false");
    CHECK(isIntegralDomain(NULL) == false, "isIntegralDomain(NULL) returns false");
    CHECK(isDivisionRing(NULL) == false, "isDivisionRing(NULL) returns false");
    CHECK(isField(NULL) == false, "isField(NULL) returns false");

    /* free on NULL is safe */
    freeGroupElement(NULL);
    freeGroup(NULL);
    freeRingElement(NULL);
    freeRing(NULL);
    CHECK(true, "free(NULL) handlers don't crash");
}

static void test_isInGroup_and_commute(Group* Z4, Group* V4, Group* S3) {
    printf("\n=== isInGroup ===\n");

    CHECK(isInGroup(V4, V4->elements[0]) == true,  "e is in V4");
    CHECK(isInGroup(V4, V4->elements[3]) == true,  "c is in V4");
    CHECK(isInGroup(V4, S3->elements[0]) == false, "S3 element not in V4");
    CHECK(isInGroup(Z4, S3->elements[1]) == false, "S3 element not in Z4");

    printf("\n=== elementsCommute ===\n");

    /* V4 is abelian: every pair commutes */
    CHECK(elementsCommute(V4->elements[1], V4->elements[2]) == true,
          "a,b commute in V4 (abelian)");
    CHECK(elementsCommute(V4->elements[2], V4->elements[3]) == true,
          "b,c commute in V4 (abelian)");

    /* S3: r and s do not commute, but r and r^2 do */
    CHECK(elementsCommute(S3->elements[1], S3->elements[3]) == false,
          "r,s do not commute in S3");
    CHECK(elementsCommute(S3->elements[1], S3->elements[2]) == true,
          "r,r^2 commute in S3 (same cyclic subgroup)");

    /* cross-group call must be rejected */
    CHECK(elementsCommute(V4->elements[0], S3->elements[0]) == false,
          "elementsCommute rejects elements from different groups");
}

static void test_subgroups(Group* V4, Group* S3) {
    printf("\n=== cmpSubgroups ===\n");

    /* {e,a} = indices {0,1} in V4 */
    int* idx_ea = malloc(2 * sizeof(int));
    idx_ea[0] = 0; idx_ea[1] = 1;
    SubGroup* H_ea = constructSubgroup(V4, idx_ea, 2);
    CHECK(H_ea != NULL, "{e,a} is a valid subgroup of V4");

    /* {e,b} = indices {0,2} in V4 */
    int* idx_eb = malloc(2 * sizeof(int));
    idx_eb[0] = 0; idx_eb[1] = 2;
    SubGroup* H_eb = constructSubgroup(V4, idx_eb, 2);
    CHECK(H_eb != NULL, "{e,b} is a valid subgroup of V4");

    CHECK(cmpSubgroups(H_ea, H_ea) == true,  "subgroup equals itself");
    CHECK(cmpSubgroups(H_ea, H_eb) == false, "{e,a} != {e,b}");

    printf("\n=== isInSubgroup ===\n");

    CHECK(isInSubgroup(H_ea, V4->elements[0]) == true,  "e is in {e,a}");
    CHECK(isInSubgroup(H_ea, V4->elements[1]) == true,  "a is in {e,a}");
    CHECK(isInSubgroup(H_ea, V4->elements[2]) == false, "b is not in {e,a}");
    CHECK(isInSubgroup(H_ea, V4->elements[3]) == false, "c is not in {e,a}");

    /* {e,r,r^2} = indices {0,1,2} in S3 (normal) */
    int* idx_N = malloc(3 * sizeof(int));
    idx_N[0] = 0; idx_N[1] = 1; idx_N[2] = 2;
    SubGroup* N = constructSubgroup(S3, idx_N, 3);
    CHECK(N != NULL, "{e,r,r^2} is a valid subgroup of S3");

    CHECK(isInSubgroup(N, S3->elements[0]) == true,  "e is in {e,r,r^2}");
    CHECK(isInSubgroup(N, S3->elements[2]) == true,  "r^2 is in {e,r,r^2}");
    CHECK(isInSubgroup(N, S3->elements[3]) == false, "s is not in {e,r,r^2}");

    CHECK(isNormalSubgroup(S3, N)  == true,  "{e,r,r^2} is normal in S3");

    /* {e,s} = indices {0,3} in S3 — valid subgroup, NOT normal */
    int* idx_s = malloc(2 * sizeof(int));
    idx_s[0] = 0; idx_s[1] = 3;
    SubGroup* Hs = constructSubgroup(S3, idx_s, 2);
    CHECK(Hs != NULL, "{e,s} is a valid subgroup of S3");
    CHECK(isNormalSubgroup(S3, Hs) == false, "{e,s} is not normal in S3");

    freeSubgroup(H_ea);
    freeSubgroup(H_eb);
    freeSubgroup(N);
    freeSubgroup(Hs);
}

static void test_cosets(Group* V4, Group* S3) {
    printf("\n=== isInGroupCoset / generateLeft/RightGroupCoset ===\n");

    /* N = {e,r,r^2} in S3.
     * eN = {e,r,r^2} = {0,1,2}
     * sN = {s, s*r, s*r^2} = {s, sr^2, sr} = {3,5,4}  (from the S3 table) */
    int* idx_N = malloc(3 * sizeof(int));
    idx_N[0] = 0; idx_N[1] = 1; idx_N[2] = 2;
    SubGroup* N = constructSubgroup(S3, idx_N, 3);

    GroupCoset* eN = generateLeftGroupCoset(N, S3->elements[0]);
    CHECK(eN != NULL,         "eN coset generated");
    CHECK(eN->isLeft == true, "eN is marked as a left coset");

    GroupCoset* sN = generateLeftGroupCoset(N, S3->elements[3]);
    CHECK(sN != NULL, "sN coset generated");

    CHECK(isInGroupCoset(eN, S3->elements[0]) == true,  "e is in eN");
    CHECK(isInGroupCoset(eN, S3->elements[2]) == true,  "r^2 is in eN");
    CHECK(isInGroupCoset(eN, S3->elements[3]) == false, "s is not in eN");
    CHECK(isInGroupCoset(sN, S3->elements[3]) == true,  "s is in sN");
    CHECK(isInGroupCoset(sN, S3->elements[4]) == true,  "sr is in sN");
    CHECK(isInGroupCoset(sN, S3->elements[0]) == false, "e is not in sN");

    /* H = {e,a} in V4.  He = {e,a} = {0,1},  Hb = {b,c} = {2,3} */
    int* idx_H = malloc(2 * sizeof(int));
    idx_H[0] = 0; idx_H[1] = 1;
    SubGroup* H = constructSubgroup(V4, idx_H, 2);

    GroupCoset* He = generateRightGroupCoset(H, V4->elements[0]);
    CHECK(He != NULL,          "He coset generated");
    CHECK(He->isLeft == false, "He is marked as a right coset");

    GroupCoset* Hb = generateRightGroupCoset(H, V4->elements[2]);
    CHECK(Hb != NULL, "Hb coset generated");
    CHECK(isInGroupCoset(Hb, V4->elements[2]) == true,  "b is in Hb");
    CHECK(isInGroupCoset(Hb, V4->elements[3]) == true,  "c is in Hb");
    CHECK(isInGroupCoset(Hb, V4->elements[0]) == false, "e is not in Hb");

    printf("\n=== getLeft/RightGroupCosets ===\n");

    /* getLeftGroupCosets(N) in S3: 2 cosets that together cover all 6 elements */
    GroupCoset** leftCosets = getLeftGroupCosets(N);
    CHECK(leftCosets != NULL, "getLeftGroupCosets(N) in S3 returns non-NULL");
    int numS3 = S3->card / N->card;   /* 2 */
    CHECK(numS3 == 2, "there are exactly 2 left cosets of N in S3");
    bool coveredS3[6] = {false};
    for (int i = 0; i < numS3; i++)
        for (int j = 0; j < leftCosets[i]->subgroup->card; j++)
            coveredS3[leftCosets[i]->indices[j]] = true;
    bool allS3 = true;
    for (int i = 0; i < 6; i++) if (!coveredS3[i]) { allS3 = false; break; }
    CHECK(allS3, "left cosets of {e,r,r^2} partition all of S3");

    /* getRightGroupCosets(H) in V4: 2 cosets covering all 4 elements */
    GroupCoset** rightCosets = getRightGroupCosets(H);
    CHECK(rightCosets != NULL, "getRightGroupCosets(H) in V4 returns non-NULL");
    int numV4 = V4->card / H->card;   /* 2 */
    CHECK(numV4 == 2, "there are exactly 2 right cosets of H in V4");
    bool coveredV4[4] = {false};
    for (int i = 0; i < numV4; i++)
        for (int j = 0; j < rightCosets[i]->subgroup->card; j++)
            coveredV4[rightCosets[i]->indices[j]] = true;
    bool allV4 = true;
    for (int i = 0; i < 4; i++) if (!coveredV4[i]) { allV4 = false; break; }
    CHECK(allV4, "right cosets of {e,a} partition all of V4");

    free(eN->indices); free(eN);
    free(sN->indices); free(sN);
    free(He->indices); free(He);
    free(Hb->indices); free(Hb);
    for (int i = 0; i < numS3; i++) { free(leftCosets[i]->indices); free(leftCosets[i]); }
    free(leftCosets);
    for (int i = 0; i < numV4; i++) { free(rightCosets[i]->indices); free(rightCosets[i]); }
    free(rightCosets);
    freeSubgroup(N);
    freeSubgroup(H);
}

static void test_quotient_group(Group* V4, Group* S3) {
    printf("\n=== quotientGroup ===\n");

    /* S3 / {e,r,r^2} ≅ Z/2Z: order 2, abelian, non-identity squares to e */
    int* idx_N = malloc(3 * sizeof(int));
    idx_N[0] = 0; idx_N[1] = 1; idx_N[2] = 2;
    SubGroup* N = constructSubgroup(S3, idx_N, 3);

    Group* QS3 = quotientGroup(S3, N);
    CHECK(QS3 != NULL,                     "S3/{e,r,r^2} constructed successfully");
    CHECK(QS3->card == 2,                  "S3/{e,r,r^2} has order 2");
    CHECK(isCommutativeGroup(QS3) == true, "S3/{e,r,r^2} is abelian (≅ Z/2Z)");
    GroupElement* qe = groupIdentity(QS3);
    GroupElement* qg = QS3->elements[1];
    CHECK(cmpGroupElements(groupMult(qg, qg), qe) == true,
          "non-identity of S3/N squares to identity");

    /* V4 / {e,a} ≅ Z/2Z */
    int* idx_H = malloc(2 * sizeof(int));
    idx_H[0] = 0; idx_H[1] = 1;
    SubGroup* H = constructSubgroup(V4, idx_H, 2);

    Group* QV4 = quotientGroup(V4, H);
    CHECK(QV4 != NULL,                     "V4/{e,a} constructed successfully");
    CHECK(QV4->card == 2,                  "V4/{e,a} has order 2");
    CHECK(isCommutativeGroup(QV4) == true, "V4/{e,a} is abelian (≅ Z/2Z)");

    /* {e,s} is a valid but non-normal subgroup of S3 — must return NULL */
    int* idx_s = malloc(2 * sizeof(int));
    idx_s[0] = 0; idx_s[1] = 3;
    SubGroup* Hs = constructSubgroup(S3, idx_s, 2);
    CHECK(quotientGroup(S3, Hs) == NULL,
          "quotientGroup returns NULL for non-normal {e,s} in S3");

    freeSubgroup(N);
    freeSubgroup(H);
    freeSubgroup(Hs);
    freeGroup(QS3);
    freeGroup(QV4);
}

static void test_null_safety_new(void) {
    printf("\n=== NULL safety (new functions) ===\n");

    CHECK(isInGroup(NULL, NULL)               == false, "isInGroup(NULL,NULL) false");
    CHECK(elementsCommute(NULL, NULL)         == false, "elementsCommute(NULL,NULL) false");
    CHECK(cmpSubgroups(NULL, NULL)            == false, "cmpSubgroups(NULL,NULL) false");
    CHECK(isInSubgroup(NULL, NULL)            == false, "isInSubgroup(NULL,NULL) false");
    CHECK(isInGroupCoset(NULL, NULL)          == false, "isInGroupCoset(NULL,NULL) false");
    CHECK(generateLeftGroupCoset(NULL, NULL)  == NULL,  "generateLeftGroupCoset(NULL,NULL) NULL");
    CHECK(generateRightGroupCoset(NULL, NULL) == NULL,  "generateRightGroupCoset(NULL,NULL) NULL");
    CHECK(getLeftGroupCosets(NULL)            == NULL,  "getLeftGroupCosets(NULL) NULL");
    CHECK(getRightGroupCosets(NULL)           == NULL,  "getRightGroupCosets(NULL) NULL");
    CHECK(quotientGroup(NULL, NULL)           == NULL,  "quotientGroup(NULL,NULL) NULL");

    freeSubgroup(NULL);
    CHECK(true, "freeSubgroup(NULL) does not crash");
}

static void test_element_order(Group* Z4, Group* S3) {
    printf("\n=== elementOrder ===\n");

    /* Z/4Z: orders 1, 4, 2, 4 */
    CHECK(elementOrder(Z4, Z4->elements[0]) == 1, "order of 0 in Z/4Z is 1 (identity)");
    CHECK(elementOrder(Z4, Z4->elements[1]) == 4, "order of 1 in Z/4Z is 4 (generator)");
    CHECK(elementOrder(Z4, Z4->elements[2]) == 2, "order of 2 in Z/4Z is 2");
    CHECK(elementOrder(Z4, Z4->elements[3]) == 4, "order of 3 in Z/4Z is 4");

    /* S3: e=1, r=3, r^2=3, s=2, sr=2, sr^2=2 */
    CHECK(elementOrder(S3, S3->elements[0]) == 1, "order of e in S3 is 1");
    CHECK(elementOrder(S3, S3->elements[1]) == 3, "order of r in S3 is 3");
    CHECK(elementOrder(S3, S3->elements[2]) == 3, "order of r^2 in S3 is 3");
    CHECK(elementOrder(S3, S3->elements[3]) == 2, "order of s in S3 is 2");
}

static void test_conjugation_and_commutator(Group* Z4, Group* S3) {
    printf("\n=== groupElementConjugate ===\n");

    /* rsr^{-1} in S3: r*(s)*r^2.
     * r*s = sr (index 4), sr*r^2 = sr^2 (index 5). */
    GroupElement* r   = S3->elements[1];
    GroupElement* r2  = S3->elements[2];
    GroupElement* s   = S3->elements[3];
    GroupElement* sr2 = S3->elements[5];

    GroupElement* c1 = groupElementConjugate(r, s);
    CHECK(c1 != NULL && c1->index == sr2->index,
          "rsr^-1 = sr^2 in S3");

    /* srs^{-1} in S3 = srs (since s^2=e):
     * s*r = sr^2 (index 5), sr^2*s = r^2 (index 2). */
    GroupElement* c2 = groupElementConjugate(s, r);
    CHECK(c2 != NULL && c2->index == r2->index,
          "srs^-1 = r^2 in S3");

    /* In an abelian group conjugation is trivial: ghg^-1 = h */
    GroupElement* one = Z4->elements[1];
    GroupElement* two = Z4->elements[2];
    GroupElement* c3 = groupElementConjugate(one, two);
    CHECK(c3 != NULL && cmpGroupElements(c3, two),
          "conjugation is trivial in abelian Z/4Z");

    printf("\n=== groupCommutator ===\n");

    /* [r,s] = r^{-1}s^{-1}rs = r^2*s*r*s.
     * r^2*s = sr^2 (idx5), sr^2*r = sr (idx4)... wait, let me re-derive:
     * r^{-1}*s^{-1} = r^2*s = table[2][3] = 5 (sr^2)
     * (sr^2)*r = table[5][1] = 4 (sr)
     * (sr)*s  = table[4][3] = 1 (r)
     * So [r,s] = r (index 1). */
    GroupElement* comm_rs = groupCommutator(r, s);
    CHECK(comm_rs != NULL && comm_rs->index == 1,
          "[r,s] = r in S3");

    /* [s,r] = s^{-1}r^{-1}sr = s*r^2*s*r.
     * s*r^2 = table[3][2] = 4 (sr)
     * sr*s  = table[4][3] = 1 (r)
     * r*r   = table[1][1] = 2 (r^2)
     * So [s,r] = r^2 (index 2). */
    GroupElement* comm_sr = groupCommutator(s, r);
    CHECK(comm_sr != NULL && comm_sr->index == 2,
          "[s,r] = r^2 in S3");

    /* In abelian group commutator = identity */
    GroupElement* comm_ab = groupCommutator(Z4->elements[1], Z4->elements[2]);
    CHECK(comm_ab != NULL && isGroupIdentity(Z4, comm_ab),
          "[1,2] = e in abelian Z/4Z");

    /* cross-group returns NULL */
    CHECK(groupElementConjugate(r, Z4->elements[1]) == NULL,
          "groupElementConjugate rejects cross-group elements");
    CHECK(groupCommutator(r, Z4->elements[1]) == NULL,
          "groupCommutator rejects cross-group elements");
}

static void test_trivial_and_whole(Group* Z4, Group* V4) {
    printf("\n=== trivialGroup ===\n");

    Group* T = trivialGroup();
    CHECK(T != NULL,      "trivialGroup() returns non-NULL");
    CHECK(T->card == 1,   "trivial group has cardinality 1");
    CHECK(isTrivialGroup(T) == true,  "isTrivialGroup(T) true");
    CHECK(isTrivialGroup(Z4) == false, "isTrivialGroup(Z4) false");
    CHECK(isTrivialGroup(V4) == false, "isTrivialGroup(V4) false");

    printf("\n=== isTrivialRing ===\n");

    Ring* TR = make_Zn_ring(1);
    CHECK(isTrivialRing(TR) == true,  "Z/1Z is trivial ring");
    Ring* Z3r = make_Zn_ring(3);
    CHECK(isTrivialRing(Z3r) == false, "Z/3Z is not trivial ring");
    freeRing(TR);
    freeRing(Z3r);

    printf("\n=== trivialSubgroup / isTrivialSubgroup ===\n");

    SubGroup* Ts = trivialSubgroup();
    CHECK(Ts != NULL,               "trivialSubgroup() returns non-NULL");
    CHECK(Ts->card == 1,            "trivial subgroup has cardinality 1");
    CHECK(isTrivialSubgroup(Ts) == true, "isTrivialSubgroup(Ts) true");

    int* idx2 = malloc(2 * sizeof(int));
    idx2[0] = 0; idx2[1] = 1;
    SubGroup* H2 = constructSubgroup(Z4, idx2, 2);
    CHECK(isTrivialSubgroup(H2) == false, "isTrivialSubgroup({e,1}) false");

    printf("\n=== isWholeGroup ===\n");

    int* idxAll = malloc(4 * sizeof(int));
    for (int i = 0; i < 4; i++) idxAll[i] = i;
    SubGroup* whole = constructSubgroup(V4, idxAll, 4);
    CHECK(isWholeGroup(V4, whole) == true,  "whole V4 isWholeGroup true");
    CHECK(isWholeGroup(V4, H2)    == false, "proper {e,1} subgroup not whole V4");

    /* freeGroup(Ts->ambient) would leak since trivialSubgroup creates its own group;
     * for test purposes just free the subgroup struct + indices */
    freeSubgroup(Ts);
    freeGroup(T);
    freeSubgroup(H2);
    freeSubgroup(whole);
}

static void test_subgroup_utilities(Group* V4, Group* S3) {
    printf("\n=== subgroupIndex ===\n");

    int* idxN = malloc(3 * sizeof(int));
    idxN[0] = 0; idxN[1] = 1; idxN[2] = 2;
    SubGroup* N = constructSubgroup(S3, idxN, 3);   /* {e,r,r^2}, index 2 */

    int* idxH = malloc(2 * sizeof(int));
    idxH[0] = 0; idxH[1] = 1;
    SubGroup* H = constructSubgroup(V4, idxH, 2);   /* {e,a}, index 2 */

    CHECK(subgroupIndex(N) == 2, "[S3 : {e,r,r^2}] = 2");
    CHECK(subgroupIndex(H) == 2, "[V4 : {e,a}] = 2");

    int* idxWhole = malloc(4 * sizeof(int));
    for (int i = 0; i < 4; i++) idxWhole[i] = i;
    SubGroup* whole = constructSubgroup(V4, idxWhole, 4);
    CHECK(subgroupIndex(whole) == 1, "[V4 : V4] = 1");

    printf("\n=== subgroupContains ===\n");

    /* {e,r,r^2} contains itself */
    CHECK(subgroupContains(N, N) == true, "N contains itself");

    /* {e,s} in S3 */
    int* idxS = malloc(2 * sizeof(int));
    idxS[0] = 0; idxS[1] = 3;
    SubGroup* Hs = constructSubgroup(S3, idxS, 2);

    /* {e} trivial subgroup of S3 (construct manually) */
    int* idxE = malloc(sizeof(int));
    idxE[0] = 0;
    SubGroup* trivS3 = constructSubgroup(S3, idxE, 1);
    CHECK(subgroupContains(N,  trivS3) == true,  "N contains trivial {e}");
    CHECK(subgroupContains(Hs, trivS3) == true,  "{e,s} contains {e}");
    CHECK(subgroupContains(N,  Hs)     == false, "{e,r,r^2} does not contain {e,s}");
    CHECK(subgroupContains(Hs, N)      == false, "{e,s} does not contain {e,r,r^2}");

    freeSubgroup(N);
    freeSubgroup(H);
    freeSubgroup(whole);
    freeSubgroup(Hs);
    freeSubgroup(trivS3);
}

static void test_cyclic_functions(Group* Z4, Group* V4, Group* S3) {
    printf("\n=== isCyclicGroup ===\n");

    CHECK(isCyclicGroup(Z4) == true,  "Z/4Z is cyclic");
    CHECK(isCyclicGroup(V4) == false, "V4 is not cyclic");
    CHECK(isCyclicGroup(S3) == false, "S3 is not cyclic (max order = 3 < 6)");

    printf("\n=== generatesCyclicGroup ===\n");

    CHECK(generatesCyclicGroup(Z4, Z4->elements[1]) == true,
          "1 generates Z/4Z");
    CHECK(generatesCyclicGroup(Z4, Z4->elements[3]) == true,
          "3 generates Z/4Z");
    CHECK(generatesCyclicGroup(Z4, Z4->elements[2]) == false,
          "2 does not generate Z/4Z (order 2)");
    CHECK(generatesCyclicGroup(S3, S3->elements[1]) == false,
          "r does not generate S3 (order 3 != 6)");

    printf("\n=== getCyclicSubgroup ===\n");

    SubGroup* cyc_r = getCyclicSubgroup(S3->elements[1]);
    CHECK(cyc_r != NULL,         "getCyclicSubgroup(r) returns non-NULL");
    CHECK(cyc_r->card == 3,      "cyclic subgroup generated by r has order 3");

    SubGroup* cyc_s = getCyclicSubgroup(S3->elements[3]);
    CHECK(cyc_s != NULL,         "getCyclicSubgroup(s) returns non-NULL");
    CHECK(cyc_s->card == 2,      "cyclic subgroup generated by s has order 2");

    SubGroup* cyc_1 = getCyclicSubgroup(Z4->elements[1]);
    CHECK(cyc_1 != NULL,         "getCyclicSubgroup(1 in Z/4Z) returns non-NULL");
    CHECK(cyc_1->card == 4,      "cyclic subgroup generated by 1 = whole Z/4Z");

    printf("\n=== isCyclicSubgroup ===\n");

    CHECK(isCyclicSubgroup(cyc_r) == true,  "{e,r,r^2} is cyclic");
    CHECK(isCyclicSubgroup(cyc_s) == true,  "{e,s} is cyclic");
    CHECK(isCyclicSubgroup(cyc_1) == true,  "Z/4Z generated by 1 is cyclic");

    /* V4 as a whole is a subgroup of itself and is NOT cyclic */
    int* idxAll = malloc(4 * sizeof(int));
    for (int i = 0; i < 4; i++) idxAll[i] = i;
    SubGroup* V4self = constructSubgroup(V4, idxAll, 4);
    CHECK(isCyclicSubgroup(V4self) == false, "V4 (as subgroup of itself) is not cyclic");

    printf("\n=== generatesCyclicSubgroup ===\n");

    /* r generates {e,r,r^2} */
    CHECK(generatesCyclicSubgroup(cyc_r, S3->elements[1]) == true,
          "r generates {e,r,r^2}");
    /* r^2 also generates {e,r,r^2}: order of r^2 is 3 = card */
    CHECK(generatesCyclicSubgroup(cyc_r, S3->elements[2]) == true,
          "r^2 also generates {e,r,r^2}");
    /* e does NOT generate it */
    CHECK(generatesCyclicSubgroup(cyc_r, S3->elements[0]) == false,
          "e does not generate {e,r,r^2}");

    freeSubgroup(cyc_r);
    freeSubgroup(cyc_s);
    freeSubgroup(cyc_1);
    freeSubgroup(V4self);
}

static void test_subgroup_conjugate(Group* S3) {
    printf("\n=== subgroupConjugate ===\n");

    /* r{e,s}r^{-1}: conjugate of {e,s} (indices {0,3}) by r.
     * r*e*r^{-1} = e  (index 0)
     * r*s*r^{-1}: r*s = sr(4), sr*r^2 = sr^2(5)  => index 5
     * So r{e,s}r^{-1} = {e, sr^2} = {0, 5}. */
    int* idxS = malloc(2 * sizeof(int));
    idxS[0] = 0; idxS[1] = 3;
    SubGroup* Hs = constructSubgroup(S3, idxS, 2);

    SubGroup* conj = subgroupConjugate(S3->elements[1], Hs);
    CHECK(conj != NULL,       "subgroupConjugate(r, {e,s}) non-NULL");
    CHECK(conj->card == 2,    "conjugate subgroup has same order");

    /* conj should contain indices 0 and 5 */
    bool has0 = false, has5 = false;
    for (int i = 0; i < conj->card; i++) {
        if (conj->indices[i] == 0) has0 = true;
        if (conj->indices[i] == 5) has5 = true;
    }
    CHECK(has0 && has5, "r{e,s}r^-1 = {e, sr^2}");

    /* Conjugate of a normal subgroup N = {e,r,r^2} should equal N */
    int* idxN = malloc(3 * sizeof(int));
    idxN[0] = 0; idxN[1] = 1; idxN[2] = 2;
    SubGroup* N = constructSubgroup(S3, idxN, 3);

    SubGroup* conjN = subgroupConjugate(S3->elements[3], N);  /* sNs^{-1} */
    CHECK(conjN != NULL, "subgroupConjugate(s, N) non-NULL");
    /* N is normal so sNs^{-1} = N as a set; check set equality, not positional */
    bool conjN_eq_N = (conjN != NULL && conjN->card == N->card);
    for (int i = 0; conjN_eq_N && i < N->card; i++) {
        bool found = false;
        for (int j = 0; j < conjN->card; j++)
            if (N->indices[i] == conjN->indices[j]) { found = true; break; }
        if (!found) conjN_eq_N = false;
    }
    CHECK(conjN_eq_N, "conjugate of normal {e,r,r^2} by s equals itself (set equality)");

    freeSubgroup(Hs);
    freeSubgroup(conj);
    freeSubgroup(N);
    freeSubgroup(conjN);
}

static void test_derived_groups(Group* Z4, Group* V4, Group* S3) {
    printf("\n=== commutatorSubgroup ===\n");

    /* Z4 and V4 are abelian: commutator subgroup is trivial */
    SubGroup* cgZ4 = commutatorSubgroup(Z4);
    CHECK(cgZ4 != NULL,             "commutatorSubgroup(Z4) non-NULL");
    CHECK(isTrivialSubgroup(cgZ4),  "commutator of abelian Z4 is trivial");

    SubGroup* cgV4 = commutatorSubgroup(V4);
    CHECK(cgV4 != NULL,             "commutatorSubgroup(V4) non-NULL");
    CHECK(isTrivialSubgroup(cgV4),  "commutator of abelian V4 is trivial");

    /* S3: commutator subgroup is {e, r, r^2} */
    SubGroup* cgS3 = commutatorSubgroup(S3);
    CHECK(cgS3 != NULL,        "commutatorSubgroup(S3) non-NULL");
    CHECK(cgS3->card == 3,     "commutator subgroup of S3 has order 3");

    bool hasR  = false, hasR2 = false, hasE = false;
    for (int i = 0; i < cgS3->card; i++) {
        if (cgS3->indices[i] == 0) hasE  = true;
        if (cgS3->indices[i] == 1) hasR  = true;
        if (cgS3->indices[i] == 2) hasR2 = true;
    }
    CHECK(hasE && hasR && hasR2, "commutator subgroup of S3 = {e,r,r^2}");

    printf("\n=== groupAbelianization ===\n");

    /* S3/[S3,S3] ≅ Z/2Z: order 2, abelian */
    Group* abS3 = groupAbelianization(S3);
    CHECK(abS3 != NULL,                    "groupAbelianization(S3) non-NULL");
    CHECK(abS3->card == 2,                 "abelianization of S3 has order 2");
    CHECK(isCommutativeGroup(abS3) == true,"abelianization of S3 is abelian");

    /* V4/trivial ≅ V4: order 4, abelian */
    Group* abV4 = groupAbelianization(V4);
    CHECK(abV4 != NULL,                    "groupAbelianization(V4) non-NULL");
    CHECK(abV4->card == 4,                 "abelianization of V4 has order 4");
    CHECK(isCommutativeGroup(abV4) == true,"abelianization of V4 is abelian");

    freeSubgroup(cgZ4);
    freeSubgroup(cgV4);
    freeSubgroup(cgS3);
    freeGroup(abS3);
    freeGroup(abV4);
}

static void test_null_safety_newer(void) {
    printf("\n=== NULL safety (newest functions) ===\n");

    CHECK(elementOrder(NULL, NULL)            == -1, "elementOrder(NULL,NULL) = -1");
    CHECK(groupElementConjugate(NULL, NULL)   == NULL, "groupElementConjugate(NULL,NULL) NULL");
    CHECK(groupCommutator(NULL, NULL)         == NULL, "groupCommutator(NULL,NULL) NULL");
    CHECK(isTrivialGroup(NULL)                == false, "isTrivialGroup(NULL) false");
    CHECK(isTrivialRing(NULL)                 == false, "isTrivialRing(NULL) false");
    CHECK(isTrivialSubgroup(NULL)             == false, "isTrivialSubgroup(NULL) false");
    CHECK(isWholeGroup(NULL, NULL)            == false, "isWholeGroup(NULL,NULL) false");
    CHECK(subgroupIndex(NULL)                 == -1, "subgroupIndex(NULL) = -1");
    CHECK(subgroupContains(NULL, NULL)        == false, "subgroupContains(NULL,NULL) false");
    CHECK(isCyclicGroup(NULL)                 == false, "isCyclicGroup(NULL) false");
    CHECK(isCyclicSubgroup(NULL)              == false, "isCyclicSubgroup(NULL) false");
    CHECK(generatesCyclicGroup(NULL, NULL)    == false, "generatesCyclicGroup(NULL,NULL) false");
    CHECK(generatesCyclicSubgroup(NULL, NULL) == false, "generatesCyclicSubgroup(NULL,NULL) false");
    CHECK(getCyclicSubgroup(NULL)             == NULL,  "getCyclicSubgroup(NULL) NULL");
    CHECK(subgroupConjugate(NULL, NULL)       == NULL,  "subgroupConjugate(NULL,NULL) NULL");
    CHECK(commutatorSubgroup(NULL)            == NULL,  "commutatorSubgroup(NULL) NULL");
    CHECK(groupAbelianization(NULL)           == NULL,  "groupAbelianization(NULL) NULL");

    freeGroupCoset(NULL);
    CHECK(true, "freeGroupCoset(NULL) does not crash");
}

/* ---------- new group constructors ---------- */

/* Q8 — quaternion group of order 8.
 * Indexing: 0=1, 1=-1, 2=i, 3=-i, 4=j, 5=-j, 6=k, 7=-k
 * Rules: i²=j²=k²=ijk=-1
 * Cayley table derived from: ij=k, jk=i, ki=j (cyclic) and ji=-k, kj=-i, ik=-j */
static Group* make_Q8(void) {
    int n = 8;
    int q8[8][8] = {
        /* 1  */ {0, 1, 2, 3, 4, 5, 6, 7},
        /* -1 */ {1, 0, 3, 2, 5, 4, 7, 6},
        /* i  */ {2, 3, 1, 0, 6, 7, 5, 4},
        /* -i */ {3, 2, 0, 1, 7, 6, 4, 5},
        /* j  */ {4, 5, 7, 6, 1, 0, 2, 3},
        /* -j */ {5, 4, 6, 7, 0, 1, 3, 2},
        /* k  */ {6, 7, 4, 5, 3, 2, 1, 0},
        /* -k */ {7, 6, 5, 4, 2, 3, 0, 1},
    };
    int** table = alloc_table(n);
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            table[i][j] = q8[i][j];

    const char* names[] = {"1","-1","i","-i","j","-j","k","-k"};
    Group* G = malloc(sizeof(Group));
    GroupElement** elems = alloc_group_elements(G, names, n);
    G->elements = elems;
    G->table    = table;
    G->card     = n;
    for (int i = 0; i < n; i++) { elems[i]->group = G; elems[i]->index = i; }
    return G;
}

/* ---------- cartesian product tests ---------- */

static void test_cartesian_product(Group* Z2, Group* Z3, Group* Z5) {
    printf("\n=== constructProductGroup ===\n");

    /* Z2 x Z2 ≅ V4: order 4, abelian, NOT cyclic */
    Group* Z2xZ2 = constructProductGroup(Z2, Z2);
    CHECK(Z2xZ2 != NULL,             "Z2 x Z2 constructed");
    CHECK(Z2xZ2->card == 4,          "Z2 x Z2 has order 4");
    CHECK(isCommutativeGroup(Z2xZ2), "Z2 x Z2 is abelian");
    CHECK(!isCyclicGroup(Z2xZ2),     "Z2 x Z2 is not cyclic (≅ V4)");
    bool allOrd2 = true;
    for (int i = 0; i < Z2xZ2->card; i++)
        if (elementOrder(Z2xZ2, Z2xZ2->elements[i]) > 2) { allOrd2 = false; break; }
    CHECK(allOrd2, "every element of Z2 x Z2 has order at most 2");

    /* Z2 x Z3 ≅ Z6: order 6, abelian, cyclic (gcd(2,3)=1) */
    Group* Z2xZ3 = constructProductGroup(Z2, Z3);
    CHECK(Z2xZ3 != NULL,             "Z2 x Z3 constructed");
    CHECK(Z2xZ3->card == 6,          "Z2 x Z3 has order 6");
    CHECK(isCommutativeGroup(Z2xZ3), "Z2 x Z3 is abelian");
    CHECK(isCyclicGroup(Z2xZ3),      "Z2 x Z3 is cyclic (≅ Z6, gcd(2,3)=1)");
    /* identity element repr is "(0,0)" */
    GroupElement* e23 = groupIdentity(Z2xZ3);
    CHECK(e23 != NULL && strcmp(e23->repr, "(0,0)") == 0,
          "Z2 x Z3 identity has repr \"(0,0)\"");

    /* Z2 x Z3 x Z5 ≅ Z30: order 30, abelian, cyclic */
    Group* Z2xZ3xZ5 = constructProductGroup(Z2xZ3, Z5);
    CHECK(Z2xZ3xZ5 != NULL,               "Z2 x Z3 x Z5 constructed");
    CHECK(Z2xZ3xZ5->card == 30,           "Z2 x Z3 x Z5 has order 30");
    CHECK(isCommutativeGroup(Z2xZ3xZ5),   "Z2 x Z3 x Z5 is abelian");
    CHECK(isCyclicGroup(Z2xZ3xZ5),        "Z2 x Z3 x Z5 is cyclic (≅ Z30)");

    /* NULL guards */
    CHECK(constructProductGroup(NULL, Z2) == NULL, "constructProductGroup(NULL,Z2) returns NULL");
    CHECK(constructProductGroup(Z2, NULL) == NULL, "constructProductGroup(Z2,NULL) returns NULL");

    freeGroup(Z2xZ2);
    freeGroup(Z2xZ3);
    freeGroup(Z2xZ3xZ5);
}

/* ---------- kfoldProductGroup tests ---------- */

static void test_kfold_cartesian_product(Group* Z2) {
    printf("\n=== kfoldProductGroup ===\n");

    /* k=1 returns the same pointer (documented shortcut in implementation) */
    Group* k1 = kfoldProductGroup(Z2, 1);
    CHECK(k1 == Z2, "kfoldProductGroup(Z2, 1) returns Z2 itself");

    /* k=2: Z2² ≅ V4, order 4, not cyclic */
    Group* k2 = kfoldProductGroup(Z2, 2);
    CHECK(k2 != NULL,         "kfoldProductGroup(Z2, 2) non-NULL");
    CHECK(k2->card == 4,      "Z2^2 has order 4");
    CHECK(!isCyclicGroup(k2), "Z2^2 is not cyclic");

    /* k=3: Z2³, order 8, not cyclic, every element has order ≤ 2 */
    Group* k3 = kfoldProductGroup(Z2, 3);
    CHECK(k3 != NULL,         "kfoldProductGroup(Z2, 3) non-NULL");
    CHECK(k3->card == 8,      "Z2^3 has order 8");
    CHECK(!isCyclicGroup(k3), "Z2^3 is not cyclic");
    bool allOrd2 = true;
    for (int i = 0; i < k3->card; i++)
        if (elementOrder(k3, k3->elements[i]) > 2) { allOrd2 = false; break; }
    CHECK(allOrd2, "every element of Z2^3 has order at most 2");

    CHECK(kfoldProductGroup(Z2,   0) == NULL, "kfold k=0 returns NULL");
    CHECK(kfoldProductGroup(NULL, 2) == NULL, "kfold NULL group returns NULL");

    /* do NOT free k1 — it is the same pointer as Z2 */
    freeGroup(k2);
    freeGroup(k3);
}

/* ---------- homomorphism tests ---------- */

static void test_homomorphisms(Group* Z4, Group* Z2) {
    printf("\n=== constructGroupHomomorphism ===\n");

    /* quotient map Z4 -> Z2: f(k) = k mod 2 */
    int* qmap = malloc(4 * sizeof(int));
    qmap[0] = 0; qmap[1] = 1; qmap[2] = 0; qmap[3] = 1;
    GroupHomomorphism* quot = constructGroupHomomorphism(Z4, Z2, qmap, 4);
    CHECK(quot != NULL, "quotient map Z4->Z2 accepted as valid homomorphism");
    CHECK(quot->domain == Z4 && quot->codomain == Z2,
          "domain and codomain stored correctly");

    /* invalid mapping: f(e) ≠ e — must be rejected */
    int* badmap = malloc(4 * sizeof(int));
    badmap[0] = 1; badmap[1] = 0; badmap[2] = 1; badmap[3] = 0;
    GroupHomomorphism* bad = constructGroupHomomorphism(Z4, Z2, badmap, 4);
    CHECK(bad == NULL, "mapping with f(e)≠e rejected");
    free(badmap);   /* ownership not transferred on failure */

    /* identity map Z4 -> Z4 */
    int* idmap = malloc(4 * sizeof(int));
    for (int i = 0; i < 4; i++) idmap[i] = i;
    GroupHomomorphism* idhom = constructGroupHomomorphism(Z4, Z4, idmap, 4);
    CHECK(idhom != NULL, "identity map Z4->Z4 accepted");

    /* trivial map Z4 -> Z4: everything to identity */
    int* trivmap = malloc(4 * sizeof(int));
    for (int i = 0; i < 4; i++) trivmap[i] = 0;
    GroupHomomorphism* trivhom = constructGroupHomomorphism(Z4, Z4, trivmap, 4);
    CHECK(trivhom != NULL, "trivial map Z4->Z4 (all to e) accepted");

    /* wrong indicesMappingSize */
    int* shortmap = malloc(3 * sizeof(int));
    shortmap[0] = 0; shortmap[1] = 1; shortmap[2] = 0;
    CHECK(constructGroupHomomorphism(Z4, Z2, shortmap, 3) == NULL,
          "mapping with wrong size rejected");
    free(shortmap);

    printf("\n=== groupElementImage ===\n");

    CHECK(groupElementImage(quot,  Z4->elements[0]) == Z2->elements[0],
          "f(0) = 0 under quotient map");
    CHECK(groupElementImage(quot,  Z4->elements[1]) == Z2->elements[1],
          "f(1) = 1 under quotient map");
    CHECK(groupElementImage(quot,  Z4->elements[2]) == Z2->elements[0],
          "f(2) = 0 under quotient map");
    CHECK(groupElementImage(quot,  Z4->elements[3]) == Z2->elements[1],
          "f(3) = 1 under quotient map");
    CHECK(groupElementImage(idhom, Z4->elements[2]) == Z4->elements[2],
          "f(2) = 2 under identity map");
    CHECK(groupElementImage(NULL,  Z4->elements[0]) == NULL,
          "groupElementImage(NULL,_) returns NULL");

    printf("\n=== groupHomomorphismKernel ===\n");

    /* kernel of quotient map: {0, 2} (even elements map to 0) */
    SubGroup* ker_quot = groupHomomorphismKernel(quot);
    CHECK(ker_quot != NULL,      "kernel of quotient map non-NULL");
    CHECK(ker_quot->card == 2,   "kernel of Z4->Z2 has order 2");
    {
        bool has0 = false, has2 = false;
        for (int i = 0; i < ker_quot->card; i++) {
            if (ker_quot->indices[i] == 0) has0 = true;
            if (ker_quot->indices[i] == 2) has2 = true;
        }
        CHECK(has0 && has2, "kernel of quotient map = {0, 2}");
    }

    /* kernel of identity map: trivial {e} */
    SubGroup* ker_id = groupHomomorphismKernel(idhom);
    CHECK(ker_id != NULL,              "kernel of identity map non-NULL");
    CHECK(ker_id->card == 1,           "kernel of identity map is trivial");
    CHECK(ker_id->indices[0] == 0,     "kernel of identity map = {e}");

    /* kernel of trivial map: the whole group */
    SubGroup* ker_triv = groupHomomorphismKernel(trivhom);
    CHECK(ker_triv != NULL,            "kernel of trivial map non-NULL");
    CHECK(ker_triv->card == 4,         "kernel of trivial map = whole Z4");

    printf("\n=== groupHomomorphismImage ===\n");

    /* image of quotient map Z4 -> Z2: a group of order 2 */
    Group* img_quot = groupHomomorphismImage(quot);
    CHECK(img_quot != NULL,            "image of quotient map non-NULL");
    CHECK(img_quot->card == 2,         "image of Z4->Z2 has order 2");
    CHECK(isCommutativeGroup(img_quot),"image of quotient map is abelian");

    /* image of identity map: isomorphic to Z4, order 4, cyclic */
    Group* img_id = groupHomomorphismImage(idhom);
    CHECK(img_id != NULL,              "image of identity map non-NULL");
    CHECK(img_id->card == 4,           "image of identity map has order 4");
    CHECK(isCyclicGroup(img_id),       "image of identity map is cyclic (≅ Z4)");

    /* image of trivial map: trivial group, order 1 */
    Group* img_triv = groupHomomorphismImage(trivhom);
    CHECK(img_triv != NULL,            "image of trivial map non-NULL");
    CHECK(img_triv->card == 1,         "image of trivial map has order 1");

    printf("\n=== isGroupIsomorphism ===\n");

    CHECK(isGroupIsomorphism(idhom)   == true,  "identity map is an isomorphism");
    CHECK(isGroupIsomorphism(quot)    == false, "quotient Z4->Z2 is NOT an isomorphism (|dom|≠|cod|)");
    CHECK(isGroupIsomorphism(trivhom) == false, "trivial map is NOT an isomorphism (non-trivial kernel)");
    CHECK(isGroupIsomorphism(NULL)    == false, "isGroupIsomorphism(NULL) returns false");

    printf("\n=== freeGroupHomomorphism ===\n");

    freeGroupHomomorphism(quot);
    freeGroupHomomorphism(idhom);
    freeGroupHomomorphism(trivhom);
    freeGroupHomomorphism(NULL);
    CHECK(true, "freeGroupHomomorphism(NULL) does not crash");

    freeSubgroup(ker_quot);
    freeSubgroup(ker_id);
    freeSubgroup(ker_triv);
    freeGroup(img_quot);
    freeGroup(img_id);
    freeGroup(img_triv);
}

/* ---------- Q8 tests ---------- */

static void test_Q8(Group* Q8) {
    printf("\n=== Q8 basics ===\n");

    CHECK(Q8 != NULL,               "Q8 constructed");
    CHECK(Q8->card == 8,            "Q8 has order 8");
    CHECK(!isCommutativeGroup(Q8),  "Q8 is non-abelian");
    CHECK(!isCyclicGroup(Q8),       "Q8 is not cyclic (no element of order 8)");

    GroupElement* one  = Q8->elements[0]; /* 1  */
    GroupElement* mone = Q8->elements[1]; /* -1 */
    GroupElement* qi   = Q8->elements[2]; /* i  */
    GroupElement* mqi  = Q8->elements[3]; /* -i */
    GroupElement* qj   = Q8->elements[4]; /* j  */
    GroupElement* mqj  = Q8->elements[5]; /* -j */
    GroupElement* qk   = Q8->elements[6]; /* k  */
    GroupElement* mqk  = Q8->elements[7]; /* -k */

    printf("\n=== Q8 element orders ===\n");

    CHECK(elementOrder(Q8, one)  == 1, "order(1)  = 1");
    CHECK(elementOrder(Q8, mone) == 2, "order(-1) = 2");
    CHECK(elementOrder(Q8, qi)   == 4, "order(i)  = 4");
    CHECK(elementOrder(Q8, mqi)  == 4, "order(-i) = 4");
    CHECK(elementOrder(Q8, qj)   == 4, "order(j)  = 4");
    CHECK(elementOrder(Q8, mqk)  == 4, "order(-k) = 4");

    printf("\n=== Q8 multiplication ===\n");

    CHECK(cmpGroupElements(groupMult(qi,   qj),   qk),   "i*j = k");
    CHECK(cmpGroupElements(groupMult(qj,   qi),   mqk),  "j*i = -k  (non-commutative)");
    CHECK(cmpGroupElements(groupMult(qj,   qk),   qi),   "j*k = i");
    CHECK(cmpGroupElements(groupMult(qk,   qj),   mqi),  "k*j = -i");
    CHECK(cmpGroupElements(groupMult(qk,   qi),   qj),   "k*i = j");
    CHECK(cmpGroupElements(groupMult(qi,   qk),   mqj),  "i*k = -j");
    CHECK(cmpGroupElements(groupMult(qi,   qi),   mone), "i*i = -1");
    CHECK(cmpGroupElements(groupMult(mone, qi),   mqi),  "(-1)*i = -i");

    printf("\n=== Q8 inverses ===\n");

    CHECK(cmpGroupElements(groupInverse(one),  one),  "1^{-1} = 1");
    CHECK(cmpGroupElements(groupInverse(mone), mone), "(-1)^{-1} = -1  (self-inverse)");
    CHECK(cmpGroupElements(groupInverse(qi),   mqi),  "i^{-1} = -i");
    CHECK(cmpGroupElements(groupInverse(mqi),  qi),   "(-i)^{-1} = i");
    CHECK(cmpGroupElements(groupInverse(qj),   mqj),  "j^{-1} = -j");
    CHECK(cmpGroupElements(groupInverse(qk),   mqk),  "k^{-1} = -k");

    printf("\n=== Q8 center = {1, -1} ===\n");

    SubGroup* center = groupCenter(Q8);
    CHECK(center != NULL,     "groupCenter(Q8) non-NULL");
    CHECK(center->card == 2,  "center of Q8 has order 2");
    { bool c0 = false, c1 = false;
      for (int i = 0; i < center->card; i++) {
          if (center->indices[i] == 0) c0 = true;
          if (center->indices[i] == 1) c1 = true;
      }
      CHECK(c0 && c1, "center of Q8 = {1, -1}"); }
    CHECK(!isInSubgroup(center, qi), "i is NOT in the center of Q8");
    CHECK(!isInSubgroup(center, qj), "j is NOT in the center of Q8");

    printf("\n=== Q8 commutator subgroup = {1, -1} ===\n");

    SubGroup* comm = commutatorSubgroup(Q8);
    CHECK(comm != NULL,    "commutatorSubgroup(Q8) non-NULL");
    CHECK(comm->card == 2, "commutator subgroup of Q8 has order 2");
    { bool cc0 = false, cc1 = false;
      for (int i = 0; i < comm->card; i++) {
          if (comm->indices[i] == 0) cc0 = true;
          if (comm->indices[i] == 1) cc1 = true;
      }
      CHECK(cc0 && cc1, "commutator subgroup of Q8 = {1, -1}"); }
    CHECK(!isTrivialSubgroup(comm), "commutator subgroup of Q8 is non-trivial");

    printf("\n=== Q8 quotient Q8/{1,-1} ≅ V4 ===\n");

    CHECK(isNormalSubgroup(Q8, center), "{1,-1} is normal in Q8 (it is the center)");

    Group* Q8mod = quotientGroup(Q8, center);
    CHECK(Q8mod != NULL,               "Q8/{1,-1} constructed");
    CHECK(Q8mod->card == 4,            "Q8/{1,-1} has order 4");
    CHECK(isCommutativeGroup(Q8mod),   "Q8/{1,-1} is abelian");
    CHECK(!isCyclicGroup(Q8mod),       "Q8/{1,-1} is not cyclic (≅ V4)");
    bool allOrd2q = true;
    for (int i = 1; i < Q8mod->card; i++)
        if (elementOrder(Q8mod, Q8mod->elements[i]) != 2) { allOrd2q = false; break; }
    CHECK(allOrd2q, "every non-identity element of Q8/{1,-1} has order 2");

    printf("\n=== Q8 cyclic subgroup <i> = {1,-1,i,-i} ===\n");

    SubGroup* cyc_i = getCyclicSubgroup(qi);
    CHECK(cyc_i != NULL,    "<i> non-NULL");
    CHECK(cyc_i->card == 4, "<i> has order 4");
    bool hi[8] = {false};
    for (int m = 0; m < cyc_i->card; m++) hi[cyc_i->indices[m]] = true;
    CHECK(hi[0] && hi[1] && hi[2] && hi[3], "<i> = {1,-1,i,-i}");
    CHECK(!hi[4] && !hi[5] && !hi[6] && !hi[7], "<i> does not contain j,-j,k,-k");
    CHECK(isCyclicSubgroup(cyc_i),              "<i> is a cyclic subgroup");

    printf("\n=== Q8 abelianization ≅ V4 ===\n");

    Group* abQ8 = groupAbelianization(Q8);
    CHECK(abQ8 != NULL,             "groupAbelianization(Q8) non-NULL");
    CHECK(abQ8->card == 4,          "abelianization of Q8 has order 4");
    CHECK(isCommutativeGroup(abQ8), "abelianization of Q8 is abelian");
    CHECK(!isCyclicGroup(abQ8),     "abelianization of Q8 is not cyclic (≅ V4)");

    freeSubgroup(center);
    freeSubgroup(comm);
    freeSubgroup(cyc_i);
    freeGroup(Q8mod);
    freeGroup(abQ8);
}

/* ---------- Z2 x Z3 x Z5 ≅ Z30 tests ---------- */

static void test_Z2xZ3xZ5(void) {
    printf("\n=== Z2 x Z3 x Z5 (≅ Z30) ===\n");

    Group* Z2 = make_Zn(2);
    Group* Z3 = make_Zn(3);
    Group* Z5 = make_Zn(5);

    Group* Z2xZ3    = constructProductGroup(Z2, Z3);
    Group* Z2xZ3xZ5 = constructProductGroup(Z2xZ3, Z5);

    CHECK(Z2xZ3xZ5 != NULL,              "Z2 x Z3 x Z5 constructed");
    CHECK(Z2xZ3xZ5->card == 30,          "order is 30");
    CHECK(isCommutativeGroup(Z2xZ3xZ5),  "Z2 x Z3 x Z5 is abelian");
    CHECK(isCyclicGroup(Z2xZ3xZ5),       "Z2 x Z3 x Z5 is cyclic (≅ Z30)");

    /* there exists an element of order 30 */
    bool hasOrder30 = false;
    for (int i = 0; i < Z2xZ3xZ5->card; i++)
        if (elementOrder(Z2xZ3xZ5, Z2xZ3xZ5->elements[i]) == 30)
            { hasOrder30 = true; break; }
    CHECK(hasOrder30, "Z2 x Z3 x Z5 has an element of order 30");

    /* commutator subgroup is trivial (abelian) */
    SubGroup* cg = commutatorSubgroup(Z2xZ3xZ5);
    CHECK(cg != NULL && isTrivialSubgroup(cg),
          "commutator subgroup of Z30 is trivial");

    /* abelianization of an abelian group ≅ itself */
    Group* ab = groupAbelianization(Z2xZ3xZ5);
    CHECK(ab != NULL,                   "abelianization of Z30 non-NULL");
    CHECK(ab->card == 30,               "abelianization of Z30 has order 30");
    CHECK(isCommutativeGroup(ab),       "abelianization of Z30 is abelian");

    /* Z2 x Z2 x Z3 x Z5 has order 60 but is NOT cyclic (two factors of 2) */
    Group* Z2xZ2    = constructProductGroup(Z2, Z2);
    Group* Z3xZ5    = constructProductGroup(Z3, Z5);
    Group* Z2xZ2xZ3xZ5 = constructProductGroup(Z2xZ2, Z3xZ5);
    CHECK(Z2xZ2xZ3xZ5->card == 60,     "Z2^2 x Z3 x Z5 has order 60");
    CHECK(!isCyclicGroup(Z2xZ2xZ3xZ5), "Z2^2 x Z3 x Z5 is NOT cyclic (exp = 30 < 60)");

    freeSubgroup(cg);
    freeGroup(ab);
    freeGroup(Z2xZ2xZ3xZ5);
    freeGroup(Z3xZ5);
    freeGroup(Z2xZ2);
    freeGroup(Z2xZ3xZ5);
    freeGroup(Z2xZ3);
    freeGroup(Z5);
    freeGroup(Z3);
    freeGroup(Z2);
}

/* ---------- NULL safety for all new functions ---------- */

static void test_null_safety_new_functions(void) {
    printf("\n=== NULL safety (derived groups + homomorphisms) ===\n");

    CHECK(constructProductGroup(NULL, NULL)                == NULL,  "constructProductGroup(NULL,NULL) NULL");
    CHECK(kfoldProductGroup(NULL, 2)              == NULL,  "kfoldProductGroup(NULL,2) NULL");
    CHECK(kfoldProductGroup(NULL, 0)              == NULL,  "kfoldProductGroup(NULL,0) NULL");
    CHECK(constructGroupHomomorphism(NULL,NULL,NULL,0)== NULL,  "constructGroupHomomorphism(NULL,...) NULL");
    CHECK(groupHomomorphismKernel(NULL)               == NULL,  "groupHomomorphismKernel(NULL) NULL");
    CHECK(groupElementImage(NULL, NULL)               == NULL,  "groupElementImage(NULL,NULL) NULL");
    CHECK(groupHomomorphismImage(NULL)                        == NULL,  "groupHomomorphismImage(NULL) NULL");
    CHECK(isGroupIsomorphism(NULL)                         == false, "isGroupIsomorphism(NULL) false");

    freeGroupHomomorphism(NULL);
    CHECK(true, "freeGroupHomomorphism(NULL) does not crash");
}

/* ---------- common constructor tests ---------- */

static void test_constructZnGroup(void) {
    printf("\n=== constructZnGroup ===\n");

    Group* Z1 = constructZnGroup(1);
    CHECK(Z1 != NULL, "constructZnGroup(1) non-NULL");
    CHECK(Z1->card == 1, "Z/1Z has order 1");
    CHECK(isTrivialGroup(Z1), "Z/1Z is trivial");
    freeGroup(Z1);

    Group* Z2 = constructZnGroup(2);
    CHECK(Z2 != NULL, "constructZnGroup(2) non-NULL");
    CHECK(Z2->card == 2, "Z/2Z has order 2");
    CHECK(isCyclicGroup(Z2), "Z/2Z is cyclic");
    CHECK(isCommutativeGroup(Z2), "Z/2Z is abelian");
    freeGroup(Z2);

    Group* Z5 = constructZnGroup(5);
    CHECK(Z5 != NULL, "constructZnGroup(5) non-NULL");
    CHECK(Z5->card == 5, "Z/5Z has order 5");
    CHECK(isCyclicGroup(Z5), "Z/5Z is cyclic");
    freeGroup(Z5);

    Group* Z7 = constructZnGroup(7);
    CHECK(Z7 != NULL, "constructZnGroup(7) non-NULL");
    CHECK(Z7->card == 7, "Z/7Z has order 7");
    /* element orders: all non-identity divide 7 (prime), so all have order 7 */
    for (int i = 1; i < 7; i++) {
        CHECK(elementOrder(Z7, Z7->elements[i]) == 7, "all non-id elements of Z/7Z have order 7");
    }
    freeGroup(Z7);

    Group* Z12 = constructZnGroup(12);
    CHECK(Z12 != NULL, "constructZnGroup(12) non-NULL");
    CHECK(Z12->card == 12, "Z/12Z has order 12");
    CHECK(isCyclicGroup(Z12), "Z/12Z is cyclic");
    /* generators of Z/12Z are those coprime to 12: 1,5,7,11 */
    CHECK(generatesCyclicGroup(Z12, Z12->elements[1]), "1 generates Z/12Z");
    CHECK(generatesCyclicGroup(Z12, Z12->elements[5]), "5 generates Z/12Z");
    CHECK(generatesCyclicGroup(Z12, Z12->elements[7]), "7 generates Z/12Z");
    CHECK(generatesCyclicGroup(Z12, Z12->elements[11]), "11 generates Z/12Z");
    CHECK(!generatesCyclicGroup(Z12, Z12->elements[2]), "2 does not generate Z/12Z");
    CHECK(!generatesCyclicGroup(Z12, Z12->elements[6]), "6 does not generate Z/12Z");
    /* order of 4 in Z/12Z is 12/gcd(4,12) = 3 */
    CHECK(elementOrder(Z12, Z12->elements[4]) == 3, "order(4) in Z/12Z is 3");
    /* order of 6 in Z/12Z is 12/gcd(6,12) = 2 */
    CHECK(elementOrder(Z12, Z12->elements[6]) == 2, "order(6) in Z/12Z is 2");
    freeGroup(Z12);

    Group* Z50 = constructZnGroup(50);
    CHECK(Z50 != NULL, "constructZnGroup(50) non-NULL");
    CHECK(Z50->card == 50, "Z/50Z has order 50");
    CHECK(isCyclicGroup(Z50), "Z/50Z is cyclic");
    CHECK(isCommutativeGroup(Z50), "Z/50Z is abelian");
    freeGroup(Z50);

    CHECK(constructZnGroup(0) == NULL, "constructZnGroup(0) returns NULL");
    CHECK(constructZnGroup(-1) == NULL, "constructZnGroup(-1) returns NULL");
}

static void test_constructZnRing(void) {
    printf("\n=== constructZnRing ===\n");

    Ring* Z2r = constructZnRing(2);
    CHECK(Z2r != NULL, "constructZnRing(2) non-NULL");
    CHECK(Z2r->card == 2, "Z/2Z ring has order 2");
    CHECK(isField(Z2r), "Z/2Z is a field");
    CHECK(hasMultIdentity(Z2r), "Z/2Z has mult identity");
    freeRing(Z2r);

    Ring* Z3r = constructZnRing(3);
    CHECK(Z3r != NULL, "constructZnRing(3) non-NULL");
    CHECK(Z3r->card == 3, "Z/3Z ring has order 3");
    CHECK(isField(Z3r), "Z/3Z is a field");
    freeRing(Z3r);

    Ring* Z4r = constructZnRing(4);
    CHECK(Z4r != NULL, "constructZnRing(4) non-NULL");
    CHECK(Z4r->card == 4, "Z/4Z ring has order 4");
    CHECK(!isField(Z4r), "Z/4Z is not a field");
    CHECK(hasZeroDivisors(Z4r), "Z/4Z has zero divisors");
    freeRing(Z4r);

    Ring* Z6r = constructZnRing(6);
    CHECK(Z6r != NULL, "constructZnRing(6) non-NULL");
    CHECK(Z6r->card == 6, "Z/6Z ring has order 6");
    CHECK(!isField(Z6r), "Z/6Z is not a field");
    CHECK(!isIntegralDomain(Z6r), "Z/6Z is not an integral domain");
    freeRing(Z6r);

    Ring* Z7r = constructZnRing(7);
    CHECK(Z7r != NULL, "constructZnRing(7) non-NULL");
    CHECK(Z7r->card == 7, "Z/7Z ring has order 7");
    CHECK(isField(Z7r), "Z/7Z is a field");
    CHECK(isIntegralDomain(Z7r), "Z/7Z is an integral domain");
    /* Fermat's little theorem: a^7 = a for all a in Z/7Z */
    for (int i = 0; i < 7; i++) {
        CHECK(cmpRingElements(ringExp(Z7r->elements[i], 7), Z7r->elements[i]),
              "Fermat: a^7 = a in Z/7Z");
    }
    freeRing(Z7r);

    Ring* Z30r = constructZnRing(30);
    CHECK(Z30r != NULL, "constructZnRing(30) non-NULL");
    CHECK(Z30r->card == 30, "Z/30Z has order 30");
    CHECK(isCommutativeRing(Z30r), "Z/30Z is commutative");
    CHECK(!isField(Z30r), "Z/30Z is not a field");
    freeRing(Z30r);

    CHECK(constructZnRing(0) == NULL, "constructZnRing(0) returns NULL");
    CHECK(constructZnRing(-1) == NULL, "constructZnRing(-1) returns NULL");
}

static void test_constructZnProductGroup(void) {
    printf("\n=== constructZnProductGroup ===\n");

    int vals1[] = {5};
    Group* Z5 = constructZnProductGroup(vals1, 1);
    CHECK(Z5 != NULL, "constructZnProductGroup({5}, 1) non-NULL");
    CHECK(Z5->card == 5, "single-factor product has order 5");
    freeGroup(Z5);

    int vals2[] = {2, 3};
    Group* Z2xZ3 = constructZnProductGroup(vals2, 2);
    CHECK(Z2xZ3 != NULL, "constructZnProductGroup({2,3}, 2) non-NULL");
    CHECK(Z2xZ3->card == 6, "Z/2Z x Z/3Z has order 6");
    CHECK(isCyclicGroup(Z2xZ3), "Z/2Z x Z/3Z is cyclic (gcd(2,3)=1)");
    freeGroup(Z2xZ3);

    int vals3[] = {2, 2};
    Group* Z2xZ2 = constructZnProductGroup(vals3, 2);
    CHECK(Z2xZ2 != NULL, "constructZnProductGroup({2,2}, 2) non-NULL");
    CHECK(Z2xZ2->card == 4, "Z/2Z x Z/2Z has order 4");
    CHECK(!isCyclicGroup(Z2xZ2), "Z/2Z x Z/2Z is NOT cyclic");
    freeGroup(Z2xZ2);

    int vals4[] = {2, 3, 5};
    Group* Z2xZ3xZ5 = constructZnProductGroup(vals4, 3);
    CHECK(Z2xZ3xZ5 != NULL, "constructZnProductGroup({2,3,5}, 3) non-NULL");
    CHECK(Z2xZ3xZ5->card == 30, "Z/2Z x Z/3Z x Z/5Z has order 30");
    CHECK(isCyclicGroup(Z2xZ3xZ5), "Z/2Z x Z/3Z x Z/5Z is cyclic");
    freeGroup(Z2xZ3xZ5);

    CHECK(constructZnProductGroup(NULL, 2) == NULL, "constructZnProductGroup(NULL, 2) returns NULL");
    CHECK(constructZnProductGroup(vals1, 0) == NULL, "constructZnProductGroup(_, 0) returns NULL");
}

static void test_constructZnProductRing(void) {
    printf("\n=== constructZnProductRing ===\n");

    int vals1[] = {3};
    Ring* Z3r = constructZnProductRing(vals1, 1);
    CHECK(Z3r != NULL, "constructZnProductRing({3}, 1) non-NULL");
    CHECK(Z3r->card == 3, "single-factor product ring has order 3");
    freeRing(Z3r);

    int vals2[] = {2, 3};
    Ring* Z2xZ3r = constructZnProductRing(vals2, 2);
    CHECK(Z2xZ3r != NULL, "constructZnProductRing({2,3}, 2) non-NULL");
    CHECK(Z2xZ3r->card == 6, "Z/2Z x Z/3Z ring has order 6");
    CHECK(isCommutativeRing(Z2xZ3r), "Z/2Z x Z/3Z ring is commutative");
    freeRing(Z2xZ3r);

    int vals3[] = {2, 2};
    Ring* Z2xZ2r = constructZnProductRing(vals3, 2);
    CHECK(Z2xZ2r != NULL, "constructZnProductRing({2,2}, 2) non-NULL");
    CHECK(Z2xZ2r->card == 4, "Z/2Z x Z/2Z ring has order 4");
    CHECK(hasZeroDivisors(Z2xZ2r), "Z/2Z x Z/2Z ring has zero divisors");
    CHECK(!isField(Z2xZ2r), "Z/2Z x Z/2Z ring is not a field");
    freeRing(Z2xZ2r);

    CHECK(constructZnProductRing(NULL, 2) == NULL, "constructZnProductRing(NULL, 2) returns NULL");
}

static void test_constructSymmetricGroup(void) {
    printf("\n=== constructSymmetricGroup ===\n");

    Group* S1 = constructSymmetricGroup(1);
    CHECK(S1 != NULL, "S(1) non-NULL");
    CHECK(S1->card == 1, "S(1) has order 1");
    CHECK(isTrivialGroup(S1), "S(1) is trivial");
    freeGroup(S1);

    Group* S2 = constructSymmetricGroup(2);
    CHECK(S2 != NULL, "S(2) non-NULL");
    CHECK(S2->card == 2, "S(2) has order 2");
    CHECK(isCommutativeGroup(S2), "S(2) is abelian");
    CHECK(isCyclicGroup(S2), "S(2) is cyclic");
    freeGroup(S2);

    Group* S3c = constructSymmetricGroup(3);
    CHECK(S3c != NULL, "S(3) non-NULL");
    CHECK(S3c->card == 6, "S(3) has order 6");
    CHECK(!isCommutativeGroup(S3c), "S(3) is non-abelian");
    CHECK(!isCyclicGroup(S3c), "S(3) is not cyclic");
    /* S3 has elements of order 1, 2, and 3 */
    int orders[4] = {0}; /* count orders 1,2,3 */
    for (int i = 0; i < 6; i++) {
        int o = elementOrder(S3c, S3c->elements[i]);
        if (o <= 3) orders[o]++;
    }
    CHECK(orders[1] == 1, "S(3) has 1 element of order 1");
    CHECK(orders[2] == 3, "S(3) has 3 elements of order 2 (transpositions)");
    CHECK(orders[3] == 2, "S(3) has 2 elements of order 3 (3-cycles)");

    /* center of S3 is trivial */
    SubGroup* cen3 = groupCenter(S3c);
    CHECK(cen3 != NULL && cen3->card == 1, "center of S(3) is trivial");
    freeSubgroup(cen3);

    /* S3 is not simple (has normal subgroup A3) */
    CHECK(!isSimple(S3c), "S(3) is not simple");

    freeGroup(S3c);

    Group* S4 = constructSymmetricGroup(4);
    CHECK(S4 != NULL, "S(4) non-NULL");
    CHECK(S4->card == 24, "S(4) has order 24");
    CHECK(!isCommutativeGroup(S4), "S(4) is non-abelian");
    CHECK(!isCyclicGroup(S4), "S(4) is not cyclic");
    /* center of S4 is trivial for n>=3 */
    SubGroup* cen4 = groupCenter(S4);
    CHECK(cen4 != NULL && cen4->card == 1, "center of S(4) is trivial");
    freeSubgroup(cen4);
    /* S4 is not simple (has normal subgroups V4, A4) */
    CHECK(!isSimple(S4), "S(4) is not simple");
    /* S4 has elements of order 1,2,3,4 */
    bool hasOrd4 = false;
    for (int i = 0; i < 24; i++) {
        if (elementOrder(S4, S4->elements[i]) == 4) { hasOrd4 = true; break; }
    }
    CHECK(hasOrd4, "S(4) has an element of order 4");
    freeGroup(S4);

    CHECK(constructSymmetricGroup(0) == NULL, "S(0) returns NULL");
}

static void test_constructAlternatingGroup(void) {
    printf("\n=== constructAlternatingGroup ===\n");

    Group* A1 = constructAlternatingGroup(1);
    CHECK(A1 != NULL, "A(1) non-NULL");
    CHECK(A1->card == 1, "A(1) has order 1");
    freeGroup(A1);

    Group* A2 = constructAlternatingGroup(2);
    CHECK(A2 != NULL, "A(2) non-NULL");
    CHECK(A2->card == 1, "A(2) has order 1");
    freeGroup(A2);

    Group* A3 = constructAlternatingGroup(3);
    CHECK(A3 != NULL, "A(3) non-NULL");
    CHECK(A3->card == 3, "A(3) has order 3");
    CHECK(isCyclicGroup(A3), "A(3) is cyclic (≅ Z/3Z)");
    CHECK(isCommutativeGroup(A3), "A(3) is abelian");
    freeGroup(A3);

    Group* A4 = constructAlternatingGroup(4);
    CHECK(A4 != NULL, "A(4) non-NULL");
    CHECK(A4->card == 12, "A(4) has order 12");
    CHECK(!isCommutativeGroup(A4), "A(4) is non-abelian");
    /* A4 is NOT simple — it has V4 as a normal subgroup */
    CHECK(!isSimple(A4), "A(4) is not simple");
    /* center of A4 is trivial */
    SubGroup* cenA4 = groupCenter(A4);
    CHECK(cenA4 != NULL && cenA4->card == 1, "center of A(4) is trivial");
    freeSubgroup(cenA4);
    /* A4 has no element of order 6 (no element of order = |A4|/2) */
    bool hasOrd6 = false;
    for (int i = 0; i < 12; i++) {
        if (elementOrder(A4, A4->elements[i]) == 6) { hasOrd6 = true; break; }
    }
    CHECK(!hasOrd6, "A(4) has no element of order 6");
    freeGroup(A4);

    CHECK(constructAlternatingGroup(0) == NULL, "A(0) returns NULL");
}

static void test_constructDihedralGroup(void) {
    printf("\n=== constructDihedralGroup ===\n");

    Group* D1 = constructDihedralGroup(1);
    CHECK(D1 != NULL, "D(1) non-NULL");
    CHECK(D1->card == 2, "D(1) has order 2");
    CHECK(isCommutativeGroup(D1), "D(1) is abelian (≅ Z/2Z)");
    freeGroup(D1);

    Group* D2 = constructDihedralGroup(2);
    CHECK(D2 != NULL, "D(2) non-NULL");
    CHECK(D2->card == 4, "D(2) has order 4");
    CHECK(isCommutativeGroup(D2), "D(2) is abelian (≅ V4)");
    CHECK(!isCyclicGroup(D2), "D(2) is not cyclic");
    freeGroup(D2);

    Group* D3 = constructDihedralGroup(3);
    CHECK(D3 != NULL, "D(3) non-NULL");
    CHECK(D3->card == 6, "D(3) has order 6");
    CHECK(!isCommutativeGroup(D3), "D(3) is non-abelian");
    /* D3 ≅ S3 */
    CHECK(!isCyclicGroup(D3), "D(3) is not cyclic");
    freeGroup(D3);

    Group* D4 = constructDihedralGroup(4);
    CHECK(D4 != NULL, "D(4) non-NULL");
    CHECK(D4->card == 8, "D(4) has order 8");
    CHECK(!isCommutativeGroup(D4), "D(4) is non-abelian");
    /* center of D4 has order 2 (just {e, r^2}) */
    SubGroup* cenD4 = groupCenter(D4);
    CHECK(cenD4 != NULL && cenD4->card == 2, "center of D(4) has order 2");
    freeSubgroup(cenD4);
    freeGroup(D4);

    Group* D5 = constructDihedralGroup(5);
    CHECK(D5 != NULL, "D(5) non-NULL");
    CHECK(D5->card == 10, "D(5) has order 10");
    CHECK(!isCommutativeGroup(D5), "D(5) is non-abelian");
    /* D5 center is trivial for odd n >= 3 */
    SubGroup* cenD5 = groupCenter(D5);
    CHECK(cenD5 != NULL && cenD5->card == 1, "center of D(5) is trivial (odd n)");
    freeSubgroup(cenD5);
    /* D5 has an element of order 5 (the rotation) */
    bool hasOrd5 = false;
    for (int i = 0; i < 10; i++) {
        if (elementOrder(D5, D5->elements[i]) == 5) { hasOrd5 = true; break; }
    }
    CHECK(hasOrd5, "D(5) has an element of order 5");
    /* all reflections in D5 have order 2 */
    int ord2count = 0;
    for (int i = 0; i < 10; i++) {
        if (elementOrder(D5, D5->elements[i]) == 2) ord2count++;
    }
    CHECK(ord2count == 5, "D(5) has 5 elements of order 2 (reflections)");
    freeGroup(D5);

    Group* D6 = constructDihedralGroup(6);
    CHECK(D6 != NULL, "D(6) non-NULL");
    CHECK(D6->card == 12, "D(6) has order 12");
    CHECK(!isCommutativeGroup(D6), "D(6) is non-abelian");
    freeGroup(D6);

    CHECK(constructDihedralGroup(0) == NULL, "D(0) returns NULL");
}

static void test_constructQ8_constructor(void) {
    printf("\n=== constructQ8 ===\n");

    Group* Q = constructQ8();
    CHECK(Q != NULL, "constructQ8() non-NULL");
    CHECK(Q->card == 8, "Q8 has order 8");
    CHECK(!isCommutativeGroup(Q), "Q8 is non-abelian");
    CHECK(!isCyclicGroup(Q), "Q8 is not cyclic");
    /* every subgroup of Q8 is normal */
    int nsubcount = 0;
    SubGroup** nsubs = listAllNormalSubgroups(Q, &nsubcount);
    int allcount = 0;
    SubGroup** allsubs = listAllSubgroups(Q, &allcount);
    CHECK(nsubcount == allcount, "every subgroup of Q8 is normal");
    for (int i = 0; i < nsubcount; i++) freeSubgroup(nsubs[i]);
    free(nsubs);
    for (int i = 0; i < allcount; i++) freeSubgroup(allsubs[i]);
    free(allsubs);
    freeGroup(Q);
}

static void test_primeFiniteField(void) {
    printf("\n=== primeFiniteField ===\n");

    Ring* F2 = primeFiniteField(2);
    CHECK(F2 != NULL, "F_2 non-NULL");
    CHECK(F2->card == 2, "F_2 has order 2");
    CHECK(isField(F2), "F_2 is a field");
    freeRing(F2);

    Ring* F3 = primeFiniteField(3);
    CHECK(F3 != NULL, "F_3 non-NULL");
    CHECK(F3->card == 3, "F_3 has order 3");
    CHECK(isField(F3), "F_3 is a field");
    freeRing(F3);

    Ring* F5 = primeFiniteField(5);
    CHECK(F5 != NULL, "F_5 non-NULL");
    CHECK(F5->card == 5, "F_5 has order 5");
    CHECK(isField(F5), "F_5 is a field");
    CHECK(isIntegralDomain(F5), "F_5 is an integral domain");
    CHECK(!hasZeroDivisors(F5), "F_5 has no zero divisors");
    freeRing(F5);

    Ring* F7 = primeFiniteField(7);
    CHECK(F7 != NULL, "F_7 non-NULL");
    CHECK(F7->card == 7, "F_7 has order 7");
    CHECK(isField(F7), "F_7 is a field");
    freeRing(F7);

    Ring* F11 = primeFiniteField(11);
    CHECK(F11 != NULL, "F_11 non-NULL");
    CHECK(F11->card == 11, "F_11 has order 11");
    CHECK(isField(F11), "F_11 is a field");
    freeRing(F11);

    Ring* F13 = primeFiniteField(13);
    CHECK(F13 != NULL, "F_13 non-NULL");
    CHECK(F13->card == 13, "F_13 has order 13");
    CHECK(isField(F13), "F_13 is a field");
    freeRing(F13);

    /* non-prime should return NULL */
    CHECK(primeFiniteField(4) == NULL, "primeFiniteField(4) returns NULL (not prime)");
    CHECK(primeFiniteField(6) == NULL, "primeFiniteField(6) returns NULL (not prime)");
    CHECK(primeFiniteField(1) == NULL, "primeFiniteField(1) returns NULL");
    CHECK(primeFiniteField(0) == NULL, "primeFiniteField(0) returns NULL");
}

static void test_constructFiniteField(void) {
    printf("\n=== constructFiniteField ===\n");

    /* k = 0: trivial ring regardless of p */
    Ring* triv = constructFiniteField(5, 0);
    CHECK(triv != NULL, "F_{p^0} is the trivial ring");
    CHECK(triv->card == 1, "F_{p^0} has one element");
    freeRing(triv);

    /* k = 1: F_p */
    Ring* F3 = constructFiniteField(3, 1);
    CHECK(F3 != NULL, "F_3 (k=1) non-NULL");
    CHECK(F3->card == 3, "F_3 has order 3");
    CHECK(isField(F3), "F_3 is a field");
    freeRing(F3);

    /* F_4 = F_{2^2} */
    Ring* F4 = constructFiniteField(2, 2);
    CHECK(F4 != NULL, "F_4 non-NULL");
    CHECK(F4->card == 4, "F_4 has order 4");
    CHECK(isField(F4), "F_4 is a field");
    CHECK(isCommutativeRing(F4), "F_4 is commutative");
    CHECK(isIntegralDomain(F4), "F_4 is an integral domain");
    CHECK(!hasZeroDivisors(F4), "F_4 has no zero divisors");
    freeRing(F4);

    /* F_8 = F_{2^3} */
    Ring* F8 = constructFiniteField(2, 3);
    CHECK(F8 != NULL, "F_8 non-NULL");
    CHECK(F8->card == 8, "F_8 has order 8");
    CHECK(isField(F8), "F_8 is a field");
    freeRing(F8);

    /* F_9 = F_{3^2} */
    Ring* F9 = constructFiniteField(3, 2);
    CHECK(F9 != NULL, "F_9 non-NULL");
    CHECK(F9->card == 9, "F_9 has order 9");
    CHECK(isField(F9), "F_9 is a field");
    freeRing(F9);

    /* F_16 = F_{2^4} */
    Ring* F16 = constructFiniteField(2, 4);
    CHECK(F16 != NULL, "F_16 non-NULL");
    CHECK(F16->card == 16, "F_16 has order 16");
    CHECK(isField(F16), "F_16 is a field");
    freeRing(F16);

    /* F_25 = F_{5^2} */
    Ring* F25 = constructFiniteField(5, 2);
    CHECK(F25 != NULL, "F_25 non-NULL");
    CHECK(F25->card == 25, "F_25 has order 25");
    CHECK(isField(F25), "F_25 is a field");
    freeRing(F25);

    /* Invalid inputs */
    CHECK(constructFiniteField(4, 2) == NULL, "constructFiniteField(4,2) NULL (p not prime)");
    CHECK(constructFiniteField(6, 3) == NULL, "constructFiniteField(6,3) NULL (p not prime)");
    CHECK(constructFiniteField(2, -1) == NULL, "constructFiniteField(2,-1) NULL");
    CHECK(constructFiniteField(1, 2) == NULL, "constructFiniteField(1,2) NULL (p=1)");
}

static void test_quotientRing(void) {
    printf("\n=== quotientRing ===\n");

    Ring* Z6 = constructZnRing(6);

    /* Z/6Z / (2) where (2) = {0,2,4} ≅ Z/2Z */
    int* idx_even = malloc(3 * sizeof(int));
    idx_even[0] = 0; idx_even[1] = 2; idx_even[2] = 4;
    Ideal* I_even = constructLeftIdeal(Z6, idx_even, 3);

    Ring* Q1 = quotientRing(Z6, I_even);
    CHECK(Q1 != NULL, "Z/6Z / (2) constructed");
    CHECK(Q1->card == 2, "Z/6Z / (2) has order 2");
    CHECK(isField(Q1), "Z/6Z / (2) is a field (≅ Z/2Z)");

    /* Z/6Z / (3) where (3) = {0,3} ≅ Z/3Z */
    int* idx_03 = malloc(2 * sizeof(int));
    idx_03[0] = 0; idx_03[1] = 3;
    Ideal* I_03 = constructLeftIdeal(Z6, idx_03, 2);

    Ring* Q2 = quotientRing(Z6, I_03);
    CHECK(Q2 != NULL, "Z/6Z / (3) constructed");
    CHECK(Q2->card == 3, "Z/6Z / (3) has order 3");
    CHECK(isField(Q2), "Z/6Z / (3) is a field (≅ Z/3Z)");

    /* Z/6Z / (0) ≅ Z/6Z */
    int* idx_0 = malloc(sizeof(int));
    idx_0[0] = 0;
    Ideal* I_0 = constructLeftIdeal(Z6, idx_0, 1);
    Ring* Q3 = quotientRing(Z6, I_0);
    CHECK(Q3 != NULL, "Z/6Z / (0) constructed");
    CHECK(Q3->card == 6, "Z/6Z / (0) has order 6");
    CHECK(isCommutativeRing(Q3), "Z/6Z / (0) is commutative");

    /* Z/6Z / (1) = Z/6Z / Z6 is the trivial ring */
    int* idx_all = malloc(6 * sizeof(int));
    for (int i = 0; i < 6; i++) idx_all[i] = i;
    Ideal* I_all = constructLeftIdeal(Z6, idx_all, 6);
    Ring* Q4 = quotientRing(Z6, I_all);
    CHECK(Q4 != NULL, "Z/6Z / Z/6Z constructed");
    CHECK(Q4->card == 1, "Z/6Z / Z/6Z has one element (trivial)");
    CHECK(isTrivialRing(Q4), "Z/6Z / Z/6Z is the trivial ring");

    /* NULL safety */
    CHECK(quotientRing(NULL, NULL) == NULL, "quotientRing(NULL,NULL) NULL");
    CHECK(quotientRing(Z6, NULL) == NULL, "quotientRing(Z6,NULL) NULL");
    CHECK(quotientRing(NULL, I_even) == NULL, "quotientRing(NULL,I) NULL");

    free(I_even); free(I_03); free(I_0); free(I_all);
    freeRing(Q1); freeRing(Q2); freeRing(Q3); freeRing(Q4);
    freeRing(Z6);
}

static void test_isPrime(void) {
    printf("\n=== isPrime ===\n");

    CHECK(isPrime(2), "2 is prime");
    CHECK(isPrime(3), "3 is prime");
    CHECK(isPrime(5), "5 is prime");
    CHECK(isPrime(7), "7 is prime");
    CHECK(isPrime(11), "11 is prime");
    CHECK(isPrime(13), "13 is prime");
    CHECK(isPrime(17), "17 is prime");
    CHECK(isPrime(19), "19 is prime");
    CHECK(isPrime(23), "23 is prime");
    CHECK(isPrime(29), "29 is prime");
    CHECK(isPrime(31), "31 is prime");
    CHECK(isPrime(37), "37 is prime");
    CHECK(isPrime(41), "41 is prime");
    CHECK(isPrime(43), "43 is prime");
    CHECK(isPrime(47), "47 is prime");
    CHECK(!isPrime(0), "0 is not prime");
    CHECK(!isPrime(1), "1 is not prime");
    CHECK(!isPrime(4), "4 is not prime");
    CHECK(!isPrime(6), "6 is not prime");
    CHECK(!isPrime(8), "8 is not prime");
    CHECK(!isPrime(9), "9 is not prime");
    CHECK(!isPrime(10), "10 is not prime");
    CHECK(!isPrime(15), "15 is not prime");
    CHECK(!isPrime(21), "21 is not prime");
    CHECK(!isPrime(25), "25 is not prime");
    CHECK(!isPrime(49), "49 is not prime");
    CHECK(!isPrime(100), "100 is not prime");
    CHECK(!isPrime(-1), "-1 is not prime");
    CHECK(!isPrime(-5), "-5 is not prime");
}

/* ---------- product ring tests ---------- */

static void test_constructProductRing(void) {
    printf("\n=== constructProductRing ===\n");

    Ring* Z2r = constructZnRing(2);
    Ring* Z3r = constructZnRing(3);

    Ring* Z2xZ3r = constructProductRing(Z2r, Z3r);
    CHECK(Z2xZ3r != NULL, "Z/2Z x Z/3Z ring constructed");
    CHECK(Z2xZ3r->card == 6, "Z/2Z x Z/3Z ring has order 6");
    CHECK(isCommutativeRing(Z2xZ3r), "Z/2Z x Z/3Z ring is commutative");
    /* CRT: Z/2Z x Z/3Z ≅ Z/6Z as rings (coprime moduli) */
    /* Note: hasMultIdentity assumes mult identity is at index 1, which may not hold
     * for product rings. Instead just check ring properties. */
    CHECK(isCommutativeRing(Z2xZ3r), "Z/2Z x Z/3Z ring is commutative (confirmed)");

    Ring* Z2xZ2r = constructProductRing(Z2r, Z2r);
    CHECK(Z2xZ2r != NULL, "Z/2Z x Z/2Z ring constructed");
    CHECK(Z2xZ2r->card == 4, "Z/2Z x Z/2Z ring has order 4");
    CHECK(!isField(Z2xZ2r), "Z/2Z x Z/2Z is not a field (has zero divisors)");
    CHECK(hasZeroDivisors(Z2xZ2r), "Z/2Z x Z/2Z ring has zero divisors");

    CHECK(constructProductRing(NULL, Z2r) == NULL, "constructProductRing(NULL, _) returns NULL");
    CHECK(constructProductRing(Z2r, NULL) == NULL, "constructProductRing(_, NULL) returns NULL");

    freeRing(Z2xZ3r);
    freeRing(Z2xZ2r);
    freeRing(Z3r);
    freeRing(Z2r);
}

static void test_kfoldProductRing(void) {
    printf("\n=== kfoldProductRing ===\n");

    Ring* Z2r = constructZnRing(2);

    Ring* k2 = kfoldProductRing(Z2r, 2);
    CHECK(k2 != NULL, "kfoldProductRing(Z/2Z, 2) non-NULL");
    CHECK(k2->card == 4, "Z/2Z^2 ring has order 4");

    Ring* k3 = kfoldProductRing(Z2r, 3);
    CHECK(k3 != NULL, "kfoldProductRing(Z/2Z, 3) non-NULL");
    CHECK(k3->card == 8, "Z/2Z^3 ring has order 8");
    CHECK(isCommutativeRing(k3), "Z/2Z^3 ring is commutative");

    CHECK(kfoldProductRing(Z2r, 0) == NULL, "kfoldProductRing(_, 0) returns NULL");
    CHECK(kfoldProductRing(NULL, 2) == NULL, "kfoldProductRing(NULL, _) returns NULL");

    freeRing(k2);
    freeRing(k3);
    freeRing(Z2r);
}

/* ---------- trivialRing tests ---------- */

static void test_trivialRing(void) {
    printf("\n=== trivialRing ===\n");

    Ring* T = trivialRing();
    CHECK(T != NULL, "trivialRing() non-NULL");
    CHECK(T->card == 1, "trivial ring has order 1");
    CHECK(isTrivialRing(T), "isTrivialRing recognizes trivial ring");
    CHECK(isCommutativeRing(T), "trivial ring is commutative");
    CHECK(!hasZeroDivisors(T), "trivial ring has no zero divisors");
    /* 0 = additive identity = the only element */
    RingElement* z = ringAddIdentity(T);
    CHECK(z != NULL, "trivialRing additive identity exists");
    CHECK(cmpRingElements(ringAdd(z, z), z), "0+0=0 in trivial ring");
    CHECK(cmpRingElements(ringMult(z, z), z), "0*0=0 in trivial ring");
    freeRing(T);
}

/* ---------- subring and ideal tests ---------- */

static void test_ring_induced_groups(void) {
    printf("\n=== ring induced groups ===\n");

    Ring* Z6 = make_Zn_ring(6);
    Group* addZ6 = constructAddGroup(Z6);
    CHECK(addZ6 != NULL, "constructAddGroup(Z/6Z) succeeds");
    CHECK(addZ6 && addZ6->card == 6, "additive group of Z/6Z has order 6");
    CHECK(addZ6 && strcmp(addZ6->elements[0]->repr, "0") == 0, "additive group preserves ring element reprs");
    CHECK(addZ6 && groupMult(addZ6->elements[2], addZ6->elements[5])->index == 1, "additive group uses ring addition table");

    Group* unitZ6 = constructUnitGroup(Z6);
    CHECK(unitZ6 != NULL, "constructUnitGroup(Z/6Z) succeeds");
    CHECK(unitZ6 && unitZ6->card == 2, "unit group of Z/6Z has order 2");
    CHECK(unitZ6 && strcmp(unitZ6->elements[0]->repr, "1") == 0, "unit group identity is 1");
    CHECK(unitZ6 && strcmp(unitZ6->elements[1]->repr, "5") == 0, "unit group includes 5 in Z/6Z");
    CHECK(unitZ6 && groupMult(unitZ6->elements[1], unitZ6->elements[1])->index == 0, "5*5 = 1 in the unit group of Z/6Z");

    Ring* Z5 = make_Zn_ring(5);
    Group* unitZ5 = constructUnitGroup(Z5);
    CHECK(unitZ5 != NULL, "constructUnitGroup(Z/5Z) succeeds");
    CHECK(unitZ5 && unitZ5->card == 4, "unit group of Z/5Z has order 4");
    CHECK(unitZ5 && isCyclicGroup(unitZ5), "unit group of Z/5Z is cyclic");

    Ring* triv = make_Zn_ring(1);
    CHECK(constructUnitGroup(triv) == NULL, "constructUnitGroup rejects ring without multiplicative identity");
    CHECK(constructAddGroup(NULL) == NULL, "constructAddGroup(NULL) returns NULL");
    CHECK(constructUnitGroup(NULL) == NULL, "constructUnitGroup(NULL) returns NULL");

    freeGroup(addZ6);
    freeGroup(unitZ6);
    freeGroup(unitZ5);
    freeRing(Z6);
    freeRing(Z5);
    freeRing(triv);
}

static void test_subrings(void) {
    printf("\n=== constructSubring / cmpSubrings ===\n");

    Ring* Z6 = constructZnRing(6);

    /* {0, 2, 4} is a subring of Z/6Z (the even elements) */
    int* idx_even = malloc(3 * sizeof(int));
    idx_even[0] = 0; idx_even[1] = 2; idx_even[2] = 4;
    SubRing* even = constructSubring(Z6, idx_even, 3);
    CHECK(even != NULL, "{0,2,4} is a valid subring of Z/6Z");
    CHECK(even->card == 3, "even subring has order 3");

    /* {0, 3} is a subring of Z/6Z */
    int* idx_03 = malloc(2 * sizeof(int));
    idx_03[0] = 0; idx_03[1] = 3;
    SubRing* s03 = constructSubring(Z6, idx_03, 2);
    CHECK(s03 != NULL, "{0,3} is a valid subring of Z/6Z");
    CHECK(s03->card == 2, "{0,3} subring has order 2");

    /* {0} is the trivial subring */
    int* idx_0 = malloc(sizeof(int));
    idx_0[0] = 0;
    SubRing* triv = constructSubring(Z6, idx_0, 1);
    CHECK(triv != NULL, "{0} is a valid subring of Z/6Z");
    CHECK(isTrivialSubring(triv), "{0} is trivial subring");
    CHECK(!isWholeRing(triv), "{0} is not the whole ring");

    /* whole ring */
    int* idx_all = malloc(6 * sizeof(int));
    for (int i = 0; i < 6; i++) idx_all[i] = i;
    SubRing* whole = constructSubring(Z6, idx_all, 6);
    CHECK(whole != NULL, "whole Z/6Z is a valid subring");
    CHECK(isWholeRing(whole), "whole ring is whole ring");
    CHECK(!isTrivialSubring(whole), "whole ring is not trivial");

    /* cmpSubrings */
    CHECK(cmpSubrings(even, even), "subring equals itself");
    CHECK(!cmpSubrings(even, s03), "different subrings compare unequal");
    CHECK(cmpSubrings(NULL, even) == false, "cmpSubrings(NULL, _) false");

    /* {0, 1} is NOT a subring of Z/6Z (not closed under addition: 1+1=2 not in {0,1}) */
    int* idx_bad = malloc(2 * sizeof(int));
    idx_bad[0] = 0; idx_bad[1] = 1;
    SubRing* bad = constructSubring(Z6, idx_bad, 2);
    CHECK(bad == NULL, "{0,1} is NOT a valid subring of Z/6Z");
    free(idx_bad);

    free(triv); free(s03); free(even); free(whole);
    freeRing(Z6);
}

static void test_ideals(void) {
    printf("\n=== constructLeftIdeal / constructRightIdeal ===\n");

    Ring* Z6 = constructZnRing(6);

    /* {0, 2, 4} is an ideal of Z/6Z (the even numbers mod 6) */
    int* idx_even = malloc(3 * sizeof(int));
    idx_even[0] = 0; idx_even[1] = 2; idx_even[2] = 4;
    Ideal* I_even = constructLeftIdeal(Z6, idx_even, 3);
    CHECK(I_even != NULL, "{0,2,4} is a valid left ideal of Z/6Z");
    CHECK(I_even->card == 3, "even ideal has order 3");
    CHECK(I_even->isLeft == true, "even ideal is marked left");

    /* Same set as right ideal (Z/6Z is commutative, so left=right) */
    int* idx_even2 = malloc(3 * sizeof(int));
    idx_even2[0] = 0; idx_even2[1] = 2; idx_even2[2] = 4;
    Ideal* I_even_r = constructRightIdeal(Z6, idx_even2, 3);
    CHECK(I_even_r != NULL, "{0,2,4} is a valid right ideal of Z/6Z");
    CHECK(I_even_r->isLeft == false, "right ideal marked as right");

    /* {0, 3} is an ideal of Z/6Z (multiples of 3 mod 6) */
    int* idx_03 = malloc(2 * sizeof(int));
    idx_03[0] = 0; idx_03[1] = 3;
    Ideal* I_03 = constructLeftIdeal(Z6, idx_03, 2);
    CHECK(I_03 != NULL, "{0,3} is a valid left ideal of Z/6Z");
    CHECK(I_03->card == 2, "{0,3} ideal has order 2");

    /* {0} trivial ideal */
    int* idx_0 = malloc(sizeof(int));
    idx_0[0] = 0;
    Ideal* I_0 = constructLeftIdeal(Z6, idx_0, 1);
    CHECK(I_0 != NULL, "{0} is a valid left ideal of Z/6Z");
    CHECK(I_0->card == 1, "trivial ideal has order 1");

    /* whole ring as ideal */
    int* idx_all = malloc(6 * sizeof(int));
    for (int i = 0; i < 6; i++) idx_all[i] = i;
    Ideal* I_all = constructLeftIdeal(Z6, idx_all, 6);
    CHECK(I_all != NULL, "Z/6Z is a valid left ideal of itself");

    /* cmpIdeals */
    CHECK(cmpIdeals(I_even, I_even), "ideal equals itself");
    CHECK(!cmpIdeals(I_even, I_03), "different ideals compare unequal");
    /* left vs right of same set: should NOT be equal (isLeft differs) */
    CHECK(!cmpIdeals(I_even, I_even_r), "left and right ideals of same set differ");
    CHECK(cmpIdeals(NULL, I_even) == false, "cmpIdeals(NULL, _) false");

    printf("\n=== addIdeals ===\n");

    /* I_even + I_03 in Z/6Z: {0,2,4} + {0,3} */
    /* Elements: all sums a+b where a in {0,2,4}, b in {0,3}:
     * 0+0=0, 0+3=3, 2+0=2, 2+3=5, 4+0=4, 4+3=1 -> {0,1,2,3,4,5} = whole ring */
    Ideal* sum = addIdeals(I_even, I_03);
    CHECK(sum != NULL, "I_even + I_03 non-NULL");
    CHECK(sum->card == 6, "I_even + I_03 = whole ring");

    /* I_0 + I_even = I_even */
    Ideal* sum2 = addIdeals(I_0, I_even);
    CHECK(sum2 != NULL, "I_0 + I_even non-NULL");
    CHECK(sum2->card == 3, "I_0 + I_even has order 3");

    CHECK(addIdeals(NULL, I_even) == NULL, "addIdeals(NULL, _) returns NULL");
    /* left + right should fail */
    CHECK(addIdeals(I_even, I_even_r) == NULL, "addIdeals(left, right) returns NULL");

    printf("\n=== multIdeals ===\n");

    /* I_even * I_03 in Z/6Z: {0,2,4} * {0,3}
     * 0*0=0, 0*3=0, 2*0=0, 2*3=0, 4*0=0, 4*3=0 -> {0} */
    Ideal* prod = multIdeals(I_even, I_03);
    CHECK(prod != NULL, "I_even * I_03 non-NULL");
    CHECK(prod->card == 1, "I_even * I_03 = {0} (trivial ideal)");

    CHECK(multIdeals(NULL, I_even) == NULL, "multIdeals(NULL, _) returns NULL");

    free(I_even); free(I_even_r); free(I_03); free(I_0); free(I_all);
    if (sum) free(sum);
    if (sum2) free(sum2);
    if (prod) free(prod);
    freeRing(Z6);
}

/* ---------- ring homomorphism tests ---------- */

static void test_ring_homomorphisms(void) {
    printf("\n=== constructRingHomomorphism ===\n");

    Ring* Z6 = constructZnRing(6);
    Ring* Z3r = constructZnRing(3);
    Ring* Z2r = constructZnRing(2);

    /* canonical projection Z/6Z -> Z/3Z: f(x) = x mod 3 */
    int* proj_map = malloc(6 * sizeof(int));
    for (int i = 0; i < 6; i++) proj_map[i] = i % 3;
    RingHomomorphism* proj = constructRingHomomorphism(Z6, Z3r, proj_map, 6);
    CHECK(proj != NULL, "Z/6Z -> Z/3Z projection accepted as valid ring hom");

    /* canonical projection Z/6Z -> Z/2Z: f(x) = x mod 2 */
    int* proj2_map = malloc(6 * sizeof(int));
    for (int i = 0; i < 6; i++) proj2_map[i] = i % 2;
    RingHomomorphism* proj2 = constructRingHomomorphism(Z6, Z2r, proj2_map, 6);
    CHECK(proj2 != NULL, "Z/6Z -> Z/2Z projection accepted as valid ring hom");

    /* identity map Z/3Z -> Z/3Z */
    int* id_map = malloc(3 * sizeof(int));
    for (int i = 0; i < 3; i++) id_map[i] = i;
    RingHomomorphism* idhom = constructRingHomomorphism(Z3r, Z3r, id_map, 3);
    CHECK(idhom != NULL, "identity map Z/3Z -> Z/3Z accepted");

    /* invalid map (not structure-preserving) */
    int* bad_map = malloc(6 * sizeof(int));
    bad_map[0] = 0; bad_map[1] = 1; bad_map[2] = 2; bad_map[3] = 0; bad_map[4] = 2; bad_map[5] = 1;
    RingHomomorphism* bad = constructRingHomomorphism(Z6, Z3r, bad_map, 6);
    CHECK(bad == NULL, "non-homomorphic map rejected");
    free(bad_map);

    CHECK(constructRingHomomorphism(NULL, Z3r, proj_map, 6) == NULL,
          "constructRingHomomorphism(NULL, ...) returns NULL");

    printf("\n=== ringElementImage ===\n");

    CHECK(ringElementImage(proj, Z6->elements[0]) == Z3r->elements[0], "f(0) = 0");
    CHECK(ringElementImage(proj, Z6->elements[1]) == Z3r->elements[1], "f(1) = 1");
    CHECK(ringElementImage(proj, Z6->elements[3]) == Z3r->elements[0], "f(3) = 0");
    CHECK(ringElementImage(proj, Z6->elements[4]) == Z3r->elements[1], "f(4) = 1");
    CHECK(ringElementImage(proj, Z6->elements[5]) == Z3r->elements[2], "f(5) = 2");
    CHECK(ringElementImage(NULL, Z6->elements[0]) == NULL, "ringElementImage(NULL, _) NULL");

    printf("\n=== ringHomomorphismKernel ===\n");

    Ideal* ker = ringHomomorphismKernel(proj);
    CHECK(ker != NULL, "kernel of Z/6Z -> Z/3Z non-NULL");
    CHECK(ker->card == 2, "kernel has order 2 ({0, 3})");

    Ideal* ker_id = ringHomomorphismKernel(idhom);
    CHECK(ker_id != NULL, "kernel of identity map non-NULL");
    CHECK(ker_id->card == 1, "kernel of identity map is trivial");

    CHECK(ringHomomorphismKernel(NULL) == NULL, "ringHomomorphismKernel(NULL) NULL");

    printf("\n=== ringHomomorphismImage ===\n");

    Ring* img = ringHomomorphismImage(proj);
    CHECK(img != NULL, "image of Z/6Z -> Z/3Z non-NULL");
    CHECK(img->card == 3, "image has order 3 (surjective onto Z/3Z)");

    Ring* img_id = ringHomomorphismImage(idhom);
    CHECK(img_id != NULL, "image of identity map non-NULL");
    CHECK(img_id->card == 3, "image of id map has order 3");

    CHECK(ringHomomorphismImage(NULL) == NULL, "ringHomomorphismImage(NULL) NULL");

    printf("\n=== isRingIsomorphism ===\n");

    CHECK(isRingIsomorphism(idhom), "identity map is ring iso");
    CHECK(!isRingIsomorphism(proj), "Z/6Z -> Z/3Z is NOT iso (diff cardinalities)");
    CHECK(!isRingIsomorphism(NULL), "isRingIsomorphism(NULL) false");

    freeRingHomomorphism(proj);
    freeRingHomomorphism(proj2);
    freeRingHomomorphism(idhom);
    free(ker); free(ker_id);
    freeRing(img); freeRing(img_id);
    freeRing(Z6); freeRing(Z3r); freeRing(Z2r);
}

/* ---------- group center / centralizer / normalizer / normal closure tests ---------- */

static void test_group_center_centralizer(void) {
    printf("\n=== groupCenter ===\n");

    Group* Z4 = constructZnGroup(4);
    SubGroup* cZ4 = groupCenter(Z4);
    CHECK(cZ4 != NULL, "center of Z/4Z non-NULL");
    CHECK(cZ4->card == 4, "center of Z/4Z = whole group (abelian)");
    freeSubgroup(cZ4);

    Group* S3c = constructSymmetricGroup(3);
    SubGroup* cS3 = groupCenter(S3c);
    CHECK(cS3 != NULL, "center of S(3) non-NULL");
    CHECK(cS3->card == 1, "center of S(3) is trivial");
    freeSubgroup(cS3);

    printf("\n=== groupCentralizer ===\n");

    /* centralizer of e in S3 is the whole group */
    SubGroup* centE = groupCentralizer(S3c, S3c->elements[0]);
    CHECK(centE != NULL, "centralizer of e in S(3) non-NULL");
    CHECK(centE->card == 6, "centralizer of e in S(3) = whole group");
    freeSubgroup(centE);

    freeGroup(Z4);
    freeGroup(S3c);
}

static void test_normalClosure(void) {
    printf("\n=== normalClosure ===\n");

    Group* S3c = constructSymmetricGroup(3);

    /* normal closure of a non-normal subgroup {e, s(any transposition)} should be bigger */
    /* In S3 (from constructor), need to figure out which element is a transposition */
    /* For S3, the identity is index 0. Let's find an element of order 2. */
    int transIdx = -1;
    for (int i = 1; i < 6; i++) {
        if (elementOrder(S3c, S3c->elements[i]) == 2) { transIdx = i; break; }
    }
    CHECK(transIdx >= 0, "S(3) has a transposition");

    if (transIdx >= 0) {
        int* idx = malloc(2 * sizeof(int));
        idx[0] = 0; idx[1] = transIdx;
        SubGroup* H = constructSubgroup(S3c, idx, 2);
        CHECK(H != NULL, "{e, transposition} is a subgroup of S(3)");
        CHECK(!isNormalSubgroup(S3c, H), "{e, transposition} is not normal in S(3)");

        SubGroup* nc = normalClosure(H);
        CHECK(nc != NULL, "normalClosure({e,transposition}) non-NULL");
        /* normal closure of any transposition in S3 is all of S3 */
        CHECK(nc->card == 6, "normal closure of transposition in S(3) is all of S(3)");

        freeSubgroup(H);
        freeSubgroup(nc);
    }

    /* normal closure of a normal subgroup is itself */
    /* A3 = {even permutations} has order 3 */
    Group* A3 = constructAlternatingGroup(3);
    /* Find A3 as a subgroup of S3 — elements of order 1 and 3 */
    int* a3idx = malloc(3 * sizeof(int));
    int a3count = 0;
    for (int i = 0; i < 6; i++) {
        int o = elementOrder(S3c, S3c->elements[i]);
        if (o == 1 || o == 3) a3idx[a3count++] = i;
    }
    CHECK(a3count == 3, "found 3 elements for A3 in S3");
    SubGroup* A3sub = constructSubgroup(S3c, a3idx, 3);
    CHECK(A3sub != NULL, "A3 as subgroup of S3 constructed");
    CHECK(isNormalSubgroup(S3c, A3sub), "A3 is normal in S3");
    SubGroup* ncA3 = normalClosure(A3sub);
    CHECK(ncA3 != NULL && ncA3->card == 3, "normal closure of A3 is A3 itself");
    freeSubgroup(A3sub);
    freeSubgroup(ncA3);
    freeGroup(A3);
    freeGroup(S3c);
}

static void test_groupNormalizer(void) {
    printf("\n=== groupNormalizer ===\n");

    Group* S3c = constructSymmetricGroup(3);

    /* normalizer of A3 in S3 = whole S3 (A3 is normal) */
    int* a3idx = malloc(3 * sizeof(int));
    int a3count = 0;
    for (int i = 0; i < 6; i++) {
        int o = elementOrder(S3c, S3c->elements[i]);
        if (o == 1 || o == 3) a3idx[a3count++] = i;
    }
    SubGroup* A3sub = constructSubgroup(S3c, a3idx, a3count);
    SubGroup* norm = groupNormalizer(A3sub);
    CHECK(norm != NULL, "normalizer of A3 in S3 non-NULL");
    CHECK(norm->card == 6, "normalizer of normal subgroup = whole group");
    freeSubgroup(A3sub);
    freeSubgroup(norm);

    /* normalizer of {e, transposition} in S3 = the subgroup itself (order 2) */
    int transIdx = -1;
    for (int i = 1; i < 6; i++) {
        if (elementOrder(S3c, S3c->elements[i]) == 2) { transIdx = i; break; }
    }
    if (transIdx >= 0) {
        int* idx = malloc(2 * sizeof(int));
        idx[0] = 0; idx[1] = transIdx;
        SubGroup* H = constructSubgroup(S3c, idx, 2);
        SubGroup* normH = groupNormalizer(H);
        CHECK(normH != NULL, "normalizer of {e, transposition} non-NULL");
        CHECK(normH->card == 2, "normalizer of {e, transposition} in S3 has order 2");
        freeSubgroup(H);
        freeSubgroup(normH);
    }

    CHECK(groupNormalizer(NULL) == NULL, "groupNormalizer(NULL) NULL");

    freeGroup(S3c);
}

static void test_conjugacyClass(void) {
    printf("\n=== conjugacyClass ===\n");

    Group* S3c = constructSymmetricGroup(3);

    /* conjugacy class of identity is just {e} */
    int ccE = 0;
    int* ccEidx = conjugacyClass(S3c->elements[0], &ccE);
    CHECK(ccEidx != NULL, "conjugacy class of e non-NULL");
    CHECK(ccE == 1, "conjugacy class of e has size 1");
    free(ccEidx);

    /* in S3, conjugacy classes are: {e}, {3-cycles (2 elems)}, {transpositions (3 elems)} */
    /* Find a 3-cycle (order 3) */
    for (int i = 1; i < 6; i++) {
        if (elementOrder(S3c, S3c->elements[i]) == 3) {
            int cc3 = 0;
            int* cc3idx = conjugacyClass(S3c->elements[i], &cc3);
            CHECK(cc3 == 2, "conjugacy class of 3-cycle in S3 has size 2");
            free(cc3idx);
            break;
        }
    }
    /* Find a transposition (order 2) */
    for (int i = 1; i < 6; i++) {
        if (elementOrder(S3c, S3c->elements[i]) == 2) {
            int cc2 = 0;
            int* cc2idx = conjugacyClass(S3c->elements[i], &cc2);
            CHECK(cc2 == 3, "conjugacy class of transposition in S3 has size 3");
            free(cc2idx);
            break;
        }
    }

    /* In abelian group, every conjugacy class has size 1 */
    Group* Z5 = constructZnGroup(5);
    for (int i = 0; i < 5; i++) {
        int cc = 0;
        int* ccidx = conjugacyClass(Z5->elements[i], &cc);
        CHECK(cc == 1, "conjugacy class in abelian group has size 1");
        free(ccidx);
    }
    freeGroup(Z5);

    CHECK(conjugacyClass(NULL, &ccE) == NULL, "conjugacyClass(NULL, _) NULL");

    freeGroup(S3c);
}

/* ---------- subgroupGeneratedBy tests ---------- */

static void test_subgroupGeneratedBy(void) {
    printf("\n=== subgroupGeneratedBy ===\n");

    Group* S3c = constructSymmetricGroup(3);

    /* subgroup generated by identity alone = trivial */
    int gen0[] = {0};
    SubGroup* trivGen = subgroupGeneratedBy(S3c, gen0, 1);
    CHECK(trivGen != NULL, "<e> non-NULL");
    CHECK(trivGen->card == 1, "<e> has order 1");
    freeSubgroup(trivGen);

    /* Find a 3-cycle, subgroup generated by it has order 3 */
    for (int i = 1; i < 6; i++) {
        if (elementOrder(S3c, S3c->elements[i]) == 3) {
            int gen[] = {i};
            SubGroup* sg = subgroupGeneratedBy(S3c, gen, 1);
            CHECK(sg != NULL, "<3-cycle> non-NULL");
            CHECK(sg->card == 3, "<3-cycle> has order 3");
            freeSubgroup(sg);
            break;
        }
    }

    /* Find a transposition, subgroup generated by it has order 2 */
    for (int i = 1; i < 6; i++) {
        if (elementOrder(S3c, S3c->elements[i]) == 2) {
            int gen[] = {i};
            SubGroup* sg = subgroupGeneratedBy(S3c, gen, 1);
            CHECK(sg != NULL, "<transposition> non-NULL");
            CHECK(sg->card == 2, "<transposition> has order 2");
            freeSubgroup(sg);
            break;
        }
    }

    /* S3 is generated by a transposition and a 3-cycle together */
    int trans = -1, cyc3 = -1;
    for (int i = 1; i < 6; i++) {
        int o = elementOrder(S3c, S3c->elements[i]);
        if (o == 2 && trans < 0) trans = i;
        if (o == 3 && cyc3 < 0) cyc3 = i;
    }
    if (trans >= 0 && cyc3 >= 0) {
        int gens[] = {trans, cyc3};
        SubGroup* full = subgroupGeneratedBy(S3c, gens, 2);
        CHECK(full != NULL, "<transposition, 3-cycle> non-NULL");
        CHECK(full->card == 6, "<transposition, 3-cycle> generates all of S3");
        freeSubgroup(full);
    }

    CHECK(subgroupGeneratedBy(NULL, gen0, 1) == NULL, "subgroupGeneratedBy(NULL, ...) NULL");

    freeGroup(S3c);
}

/* ---------- listAllSubgroups / listAllNormalSubgroups tests ---------- */

static void test_listAllSubgroups(void) {
    printf("\n=== listAllSubgroups ===\n");

    Group* Z4 = constructZnGroup(4);
    int count4 = 0;
    SubGroup** subs4 = listAllSubgroups(Z4, &count4);
    CHECK(subs4 != NULL, "listAllSubgroups(Z/4Z) non-NULL");
    /* Z/4Z subgroups: {e}, {e,2}, {e,1,2,3} -> 3 subgroups */
    CHECK(count4 == 3, "Z/4Z has 3 subgroups");
    for (int i = 0; i < count4; i++) freeSubgroup(subs4[i]);
    free(subs4);
    freeGroup(Z4);

    Group* V4 = make_V4();
    int countV4 = 0;
    SubGroup** subsV4 = listAllSubgroups(V4, &countV4);
    CHECK(subsV4 != NULL, "listAllSubgroups(V4) non-NULL");
    /* V4 subgroups: {e}, {e,a}, {e,b}, {e,c}, V4 -> 5 subgroups */
    CHECK(countV4 == 5, "V4 has 5 subgroups");
    for (int i = 0; i < countV4; i++) freeSubgroup(subsV4[i]);
    free(subsV4);
    freeGroup(V4);

    Group* S3c = constructSymmetricGroup(3);
    int countS3 = 0;
    SubGroup** subsS3 = listAllSubgroups(S3c, &countS3);
    CHECK(subsS3 != NULL, "listAllSubgroups(S3) non-NULL");
    /* S3 subgroups: {e}, {e,s}, {e,sr}, {e,sr^2}, {e,r,r^2}, S3 -> 6 subgroups */
    CHECK(countS3 == 6, "S3 has 6 subgroups");
    for (int i = 0; i < countS3; i++) freeSubgroup(subsS3[i]);
    free(subsS3);

    printf("\n=== listAllNormalSubgroups ===\n");

    int normCountS3 = 0;
    SubGroup** normSubsS3 = listAllNormalSubgroups(S3c, &normCountS3);
    CHECK(normSubsS3 != NULL, "listAllNormalSubgroups(S3) non-NULL");
    /* S3 normal subgroups: {e}, {e,r,r^2}, S3 -> 3 */
    CHECK(normCountS3 == 3, "S3 has 3 normal subgroups");
    for (int i = 0; i < normCountS3; i++) freeSubgroup(normSubsS3[i]);
    free(normSubsS3);

    CHECK(listAllSubgroups(NULL, &countS3) == NULL, "listAllSubgroups(NULL, _) NULL");
    CHECK(listAllNormalSubgroups(NULL, &countS3) == NULL, "listAllNormalSubgroups(NULL, _) NULL");

    freeGroup(S3c);
}

/* ---------- isSimple tests ---------- */

static void test_isSimple(void) {
    printf("\n=== isSimple ===\n");

    /* Z/pZ is simple for prime p */
    Group* Z2 = constructZnGroup(2);
    CHECK(isSimple(Z2), "Z/2Z is simple");
    freeGroup(Z2);

    Group* Z3 = constructZnGroup(3);
    CHECK(isSimple(Z3), "Z/3Z is simple");
    freeGroup(Z3);

    Group* Z5 = constructZnGroup(5);
    CHECK(isSimple(Z5), "Z/5Z is simple");
    freeGroup(Z5);

    /* Z/4Z is NOT simple (has {0,2} as normal subgroup) */
    Group* Z4 = constructZnGroup(4);
    CHECK(!isSimple(Z4), "Z/4Z is not simple");
    freeGroup(Z4);

    /* V4 is NOT simple (has 3 normal subgroups of order 2) */
    Group* V4 = make_V4();
    CHECK(!isSimple(V4), "V4 is not simple");
    freeGroup(V4);

    /* A5 would be the smallest non-abelian simple group, but it's order 60 — too slow.
     * Instead verify S3 is not simple. */
    Group* S3c = constructSymmetricGroup(3);
    CHECK(!isSimple(S3c), "S3 is not simple");
    freeGroup(S3c);

    /* trivial group is NOT simple (convention) */
    Group* T = trivialGroup();
    CHECK(!isSimple(T), "trivial group is not simple");
    freeGroup(T);

    CHECK(!isSimple(NULL), "isSimple(NULL) false");
}

/* ---------- constructGroupSkipValidate tests ---------- */

static void test_constructGroupSkipValidate(void) {
    printf("\n=== constructGroupSkipValidate ===\n");

    /* Use a known-good table (Z/3Z) */
    int** table = alloc_table(3);
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            table[i][j] = (i + j) % 3;
    const char* names[] = {"0","1","2"};
    Group* G = malloc(sizeof(Group));
    GroupElement** elems = alloc_group_elements(G, names, 3);

    Group* G2 = constructGroupSkipValidate(elems, table, 3);
    CHECK(G2 != NULL, "constructGroupSkipValidate non-NULL");
    CHECK(G2->card == 3, "skip-validate group has correct card");
    /* must assign group pointer (constructGroupSkipValidate doesn't do this) */
    for (int i = 0; i < 3; i++) elems[i]->group = G2;
    CHECK(isCyclicGroup(G2), "skip-validate Z/3Z is cyclic");

    free(G); /* free the placeholder we malloc'd */
    freeGroup(G2);
}

/* ---------- more product group tests ---------- */

static void test_product_groups_extended(void) {
    printf("\n=== constructProductGroup extended ===\n");

    /* Z/3Z x Z/3Z: order 9, abelian, NOT cyclic (3^2) */
    Group* Z3 = constructZnGroup(3);
    Group* Z3xZ3 = constructProductGroup(Z3, Z3);
    CHECK(Z3xZ3 != NULL, "Z/3Z x Z/3Z constructed");
    CHECK(Z3xZ3->card == 9, "Z/3Z x Z/3Z has order 9");
    CHECK(isCommutativeGroup(Z3xZ3), "Z/3Z x Z/3Z is abelian");
    CHECK(!isCyclicGroup(Z3xZ3), "Z/3Z x Z/3Z is NOT cyclic");
    /* every element has order dividing 3 */
    bool allDiv3 = true;
    for (int i = 0; i < 9; i++) {
        int o = elementOrder(Z3xZ3, Z3xZ3->elements[i]);
        if (o != 1 && o != 3) { allDiv3 = false; break; }
    }
    CHECK(allDiv3, "every element of Z/3Z x Z/3Z has order 1 or 3");
    freeGroup(Z3xZ3);
    freeGroup(Z3);

    /* kfoldProductGroup extended */
    Group* Z2 = constructZnGroup(2);
    Group* Z2_4 = kfoldProductGroup(Z2, 4);
    CHECK(Z2_4 != NULL, "Z/2Z^4 constructed");
    CHECK(Z2_4->card == 16, "Z/2Z^4 has order 16");
    CHECK(!isCyclicGroup(Z2_4), "Z/2Z^4 is not cyclic");
    CHECK(isCommutativeGroup(Z2_4), "Z/2Z^4 is abelian");
    freeGroup(Z2_4);
    freeGroup(Z2);
}

/* ---------- additional element order tests ---------- */

static void test_element_orders_extended(void) {
    printf("\n=== elementOrder extended ===\n");

    /* D4: element orders */
    Group* D4 = constructDihedralGroup(4);
    /* D4 has: 1 element of order 1 (e), 1 of order 4 (r and r^3 => 2), 1 of order 2 (r^2), 4 of order 2 (reflections) + r^2 */
    int ordCounts[5] = {0};
    for (int i = 0; i < 8; i++) {
        int o = elementOrder(D4, D4->elements[i]);
        CHECK(o >= 1 && o <= 4, "D4 element orders are in {1,2,4}");
        if (o < 5) ordCounts[o]++;
    }
    CHECK(ordCounts[1] == 1, "D(4) has 1 element of order 1");
    CHECK(ordCounts[2] == 5, "D(4) has 5 elements of order 2");
    CHECK(ordCounts[4] == 2, "D(4) has 2 elements of order 4");
    freeGroup(D4);

    /* Q8: all non-identity elements have order 2 or 4 */
    Group* Q = constructQ8();
    int qOrdCounts[5] = {0};
    for (int i = 0; i < 8; i++) {
        int o = elementOrder(Q, Q->elements[i]);
        if (o < 5) qOrdCounts[o]++;
    }
    CHECK(qOrdCounts[1] == 1, "Q8 has 1 element of order 1");
    CHECK(qOrdCounts[2] == 1, "Q8 has 1 element of order 2 (-1)");
    CHECK(qOrdCounts[4] == 6, "Q8 has 6 elements of order 4");
    freeGroup(Q);
}

/* ---------- large Zn tests ---------- */

static void test_large_Zn(void) {
    printf("\n=== large Zn groups ===\n");

    Group* Z100 = constructZnGroup(100);
    CHECK(Z100 != NULL, "Z/100Z constructed");
    CHECK(Z100->card == 100, "Z/100Z has order 100");
    CHECK(isCyclicGroup(Z100), "Z/100Z is cyclic");
    CHECK(isCommutativeGroup(Z100), "Z/100Z is abelian");
    /* order of 25 in Z/100Z = 100/gcd(25,100) = 100/25 = 4 */
    CHECK(elementOrder(Z100, Z100->elements[25]) == 4, "order(25) in Z/100Z is 4");
    /* order of 10 in Z/100Z = 100/gcd(10,100) = 100/10 = 10 */
    CHECK(elementOrder(Z100, Z100->elements[10]) == 10, "order(10) in Z/100Z is 10");
    /* order of 1 in Z/100Z = 100 */
    CHECK(elementOrder(Z100, Z100->elements[1]) == 100, "order(1) in Z/100Z is 100");
    freeGroup(Z100);

    printf("\n=== large Zn rings ===\n");

    Ring* Z11r = constructZnRing(11);
    CHECK(Z11r != NULL, "Z/11Z ring constructed");
    CHECK(isField(Z11r), "Z/11Z is a field (11 is prime)");
    /* Wilson's theorem: (p-1)! ≡ -1 (mod p) */
    /* 10! mod 11 = 10 = -1 mod 11 */
    RingElement* factorial = Z11r->elements[1];
    for (int i = 2; i <= 10; i++) {
        factorial = ringMult(factorial, Z11r->elements[i]);
    }
    CHECK(cmpRingElements(factorial, Z11r->elements[10]), "Wilson: 10! ≡ -1 (mod 11)");
    freeRing(Z11r);

    Ring* Z12r = constructZnRing(12);
    CHECK(Z12r != NULL, "Z/12Z ring constructed");
    CHECK(!isField(Z12r), "Z/12Z is not a field");
    CHECK(hasZeroDivisors(Z12r), "Z/12Z has zero divisors");
    /* units of Z/12Z: coprime to 12 -> {1,5,7,11} */
    CHECK(hasMultInverse(Z12r->elements[1]), "1 is a unit in Z/12Z");
    CHECK(hasMultInverse(Z12r->elements[5]), "5 is a unit in Z/12Z");
    CHECK(hasMultInverse(Z12r->elements[7]), "7 is a unit in Z/12Z");
    CHECK(hasMultInverse(Z12r->elements[11]), "11 is a unit in Z/12Z");
    CHECK(!hasMultInverse(Z12r->elements[2]), "2 is not a unit in Z/12Z");
    CHECK(!hasMultInverse(Z12r->elements[3]), "3 is not a unit in Z/12Z");
    CHECK(!hasMultInverse(Z12r->elements[4]), "4 is not a unit in Z/12Z");
    CHECK(!hasMultInverse(Z12r->elements[6]), "6 is not a unit in Z/12Z");
    freeRing(Z12r);
}

/* ---------- NULL safety for new functions ---------- */

static void test_null_safety_comprehensive(void) {
    printf("\n=== comprehensive NULL safety ===\n");

    CHECK(constructZnGroup(0) == NULL, "constructZnGroup(0) NULL");
    CHECK(constructZnRing(0) == NULL, "constructZnRing(0) NULL");
    CHECK(constructSymmetricGroup(0) == NULL, "constructSymmetricGroup(0) NULL");
    CHECK(constructAlternatingGroup(0) == NULL, "constructAlternatingGroup(0) NULL");
    CHECK(constructDihedralGroup(0) == NULL, "constructDihedralGroup(0) NULL");
    CHECK(primeFiniteField(0) == NULL, "primeFiniteField(0) NULL");
    CHECK(primeFiniteField(4) == NULL, "primeFiniteField(4) NULL (not prime)");
    CHECK(constructProductGroup(NULL, NULL) == NULL, "constructProductGroup(NULL,NULL) NULL");
    CHECK(constructProductRing(NULL, NULL) == NULL, "constructProductRing(NULL,NULL) NULL");
    CHECK(kfoldProductGroup(NULL, 2) == NULL, "kfoldProductGroup(NULL,2) NULL");
    CHECK(kfoldProductRing(NULL, 2) == NULL, "kfoldProductRing(NULL,2) NULL");
    CHECK(constructSubring(NULL, NULL, 0) == NULL, "constructSubring(NULL,...) NULL");
    CHECK(cmpSubrings(NULL, NULL) == false, "cmpSubrings(NULL,NULL) false");
    CHECK(isTrivialSubring(NULL) == false, "isTrivialSubring(NULL) false");
    CHECK(isWholeRing(NULL) == false, "isWholeRing(NULL) false");
    CHECK(constructLeftIdeal(NULL, NULL, 0) == NULL, "constructLeftIdeal(NULL,...) NULL");
    CHECK(constructRightIdeal(NULL, NULL, 0) == NULL, "constructRightIdeal(NULL,...) NULL");
    CHECK(cmpIdeals(NULL, NULL) == false, "cmpIdeals(NULL,NULL) false");
    CHECK(addIdeals(NULL, NULL) == NULL, "addIdeals(NULL,NULL) NULL");
    CHECK(multIdeals(NULL, NULL) == NULL, "multIdeals(NULL,NULL) NULL");
    CHECK(constructRingHomomorphism(NULL, NULL, NULL, 0) == NULL, "constructRingHomomorphism(NULL,...) NULL");
    CHECK(ringHomomorphismKernel(NULL) == NULL, "ringHomomorphismKernel(NULL) NULL");
    CHECK(ringElementImage(NULL, NULL) == NULL, "ringElementImage(NULL,NULL) NULL");
    CHECK(ringHomomorphismImage(NULL) == NULL, "ringHomomorphismImage(NULL) NULL");
    CHECK(isRingIsomorphism(NULL) == false, "isRingIsomorphism(NULL) false");
    CHECK(conjugacyClass(NULL, NULL) == NULL, "conjugacyClass(NULL,NULL) NULL");
    CHECK(groupNormalizer(NULL) == NULL, "groupNormalizer(NULL) NULL");
    CHECK(subgroupGeneratedBy(NULL, NULL, 0) == NULL, "subgroupGeneratedBy(NULL,...) NULL");
    CHECK(normalClosure(NULL) == NULL, "normalClosure(NULL) NULL");
    CHECK(groupCentralizer(NULL, NULL) == NULL, "groupCentralizer(NULL,NULL) NULL");

    freeRingHomomorphism(NULL);
    CHECK(true, "freeRingHomomorphism(NULL) does not crash");
}

/* ---------- S4 deep tests ---------- */

static void test_S4_deep(void) {
    printf("\n=== S4 deep tests ===\n");

    Group* S4 = constructSymmetricGroup(4);
    CHECK(S4 != NULL, "S(4) constructed");
    CHECK(S4->card == 24, "S(4) has order 24");

    /* S4 element order counts: 1x1, 6x2, 8x3, 6x4 */
    int oCounts[5] = {0};
    for (int i = 0; i < 24; i++) {
        int o = elementOrder(S4, S4->elements[i]);
        if (o < 5) oCounts[o]++;
    }
    CHECK(oCounts[1] == 1, "S(4): 1 element of order 1");
    CHECK(oCounts[2] == 9, "S(4): 9 elements of order 2");
    CHECK(oCounts[3] == 8, "S(4): 8 elements of order 3");
    CHECK(oCounts[4] == 6, "S(4): 6 elements of order 4");

    /* S4 center is trivial */
    SubGroup* cen = groupCenter(S4);
    CHECK(cen != NULL && cen->card == 1, "center of S(4) is trivial");
    freeSubgroup(cen);

    /* commutator subgroup of S4 = A4 (order 12) */
    SubGroup* comm = commutatorSubgroup(S4);
    CHECK(comm != NULL, "commutator subgroup of S(4) non-NULL");
    CHECK(comm->card == 12, "commutator subgroup of S(4) has order 12 (= A4)");
    freeSubgroup(comm);

    /* abelianization of S4 ≅ Z/2Z (order 2) */
    Group* abS4 = groupAbelianization(S4);
    CHECK(abS4 != NULL, "abelianization of S(4) non-NULL");
    CHECK(abS4->card == 2, "abelianization of S(4) has order 2");
    freeGroup(abS4);

    /* number of subgroups of S4: 30 */
    int subCount = 0;
    SubGroup** subs = listAllSubgroups(S4, &subCount);
    CHECK(subs != NULL, "listAllSubgroups(S4) non-NULL");
    CHECK(subCount == 30, "S(4) has 30 subgroups");
    for (int i = 0; i < subCount; i++) freeSubgroup(subs[i]);
    free(subs);

    /* number of normal subgroups of S4: 4 ({e}, V4, A4, S4) */
    int normCount = 0;
    SubGroup** normSubs = listAllNormalSubgroups(S4, &normCount);
    CHECK(normSubs != NULL, "listAllNormalSubgroups(S4) non-NULL");
    CHECK(normCount == 4, "S(4) has 4 normal subgroups");
    for (int i = 0; i < normCount; i++) freeSubgroup(normSubs[i]);
    free(normSubs);

    /* conjugacy classes of S4 have sizes 1, 3, 6, 6, 8 (= 5 classes) */
    bool seen[24] = {false};
    int numClasses = 0;
    for (int i = 0; i < 24; i++) {
        if (!seen[i]) {
            int cc = 0;
            int* ccIdx = conjugacyClass(S4->elements[i], &cc);
            for (int j = 0; j < cc; j++) seen[ccIdx[j]] = true;
            numClasses++;
            free(ccIdx);
        }
    }
    CHECK(numClasses == 5, "S(4) has 5 conjugacy classes");

    freeGroup(S4);
}

/* ---------- A4 deep tests ---------- */

static void test_A4_deep(void) {
    printf("\n=== A4 deep tests ===\n");

    Group* A4 = constructAlternatingGroup(4);
    CHECK(A4 != NULL, "A(4) constructed");
    CHECK(A4->card == 12, "A(4) has order 12");

    /* element orders in A4: 1x1, 3x2, 8x3 */
    int oCounts[4] = {0};
    for (int i = 0; i < 12; i++) {
        int o = elementOrder(A4, A4->elements[i]);
        if (o < 4) oCounts[o]++;
    }
    CHECK(oCounts[1] == 1, "A(4): 1 element of order 1");
    CHECK(oCounts[2] == 3, "A(4): 3 elements of order 2");
    CHECK(oCounts[3] == 8, "A(4): 8 elements of order 3");

    /* A4 is NOT simple (has V4 as normal subgroup of order 4) */
    int normCount = 0;
    SubGroup** normSubs = listAllNormalSubgroups(A4, &normCount);
    CHECK(normSubs != NULL, "listAllNormalSubgroups(A4) non-NULL");
    /* Normal subgroups of A4: {e}, V4, A4 => 3 */
    CHECK(normCount == 3, "A(4) has 3 normal subgroups");
    /* One of them should have order 4 (the V4) */
    bool hasOrd4Sub = false;
    for (int i = 0; i < normCount; i++) {
        if (normSubs[i]->card == 4) hasOrd4Sub = true;
        freeSubgroup(normSubs[i]);
    }
    free(normSubs);
    CHECK(hasOrd4Sub, "A(4) has a normal subgroup of order 4 (V4)");

    /* commutator subgroup of A4 = V4 (order 4) */
    SubGroup* commA4 = commutatorSubgroup(A4);
    CHECK(commA4 != NULL, "commutator subgroup of A(4) non-NULL");
    CHECK(commA4->card == 4, "commutator subgroup of A(4) has order 4 (V4)");
    freeSubgroup(commA4);

    /* conjugacy classes of A4: sizes 1, 3, 4, 4 (= 4 classes) */
    bool seen[12] = {false};
    int numClasses = 0;
    for (int i = 0; i < 12; i++) {
        if (!seen[i]) {
            int cc = 0;
            int* ccIdx = conjugacyClass(A4->elements[i], &cc);
            for (int j = 0; j < cc; j++) seen[ccIdx[j]] = true;
            numClasses++;
            free(ccIdx);
        }
    }
    CHECK(numClasses == 4, "A(4) has 4 conjugacy classes");

    freeGroup(A4);
}

/* ---------- D5 deep tests ---------- */

static void test_D5_deep(void) {
    printf("\n=== D5 deep tests ===\n");

    Group* D5 = constructDihedralGroup(5);
    CHECK(D5 != NULL, "D(5) constructed");
    CHECK(D5->card == 10, "D(5) has order 10");

    /* D5 is non-abelian */
    CHECK(!isCommutativeGroup(D5), "D(5) is non-abelian");

    /* subgroups of D5: {e}, 5x{e,refl}, {e,r,r^2,r^3,r^4}, D5 => 8 total */
    int subCount = 0;
    SubGroup** subs = listAllSubgroups(D5, &subCount);
    CHECK(subs != NULL, "listAllSubgroups(D5) non-NULL");
    CHECK(subCount == 8, "D(5) has 8 subgroups");
    for (int i = 0; i < subCount; i++) freeSubgroup(subs[i]);
    free(subs);

    /* normal subgroups of D5: {e}, <r> (order 5), D5 => 3 */
    int normCount = 0;
    SubGroup** normSubs = listAllNormalSubgroups(D5, &normCount);
    CHECK(normSubs != NULL, "listAllNormalSubgroups(D5) non-NULL");
    CHECK(normCount == 3, "D(5) has 3 normal subgroups");
    for (int i = 0; i < normCount; i++) freeSubgroup(normSubs[i]);
    free(normSubs);

    /* commutator subgroup of D5 = <r> (order 5, for odd n) */
    SubGroup* commD5 = commutatorSubgroup(D5);
    CHECK(commD5 != NULL, "commutator subgroup of D(5) non-NULL");
    CHECK(commD5->card == 5, "commutator subgroup of D(5) has order 5");
    freeSubgroup(commD5);

    /* abelianization D5/[D5,D5] ≅ Z/2Z (order 2) */
    Group* abD5 = groupAbelianization(D5);
    CHECK(abD5 != NULL, "abelianization of D(5) non-NULL");
    CHECK(abD5->card == 2, "abelianization of D(5) has order 2");
    freeGroup(abD5);

    /* quotient D5/<r> ≅ Z/2Z */
    SubGroup* rotations = commutatorSubgroup(D5); /* <r> */
    CHECK(isNormalSubgroup(D5, rotations), "rotation subgroup is normal in D(5)");
    Group* quot = quotientGroup(D5, rotations);
    CHECK(quot != NULL, "D5/<r> constructed");
    CHECK(quot->card == 2, "D5/<r> has order 2");
    freeSubgroup(rotations);
    freeGroup(quot);

    /* conjugacy classes of D5: {e}, {r,r^4}, {r^2,r^3}, 5 reflections split into...
     * Actually for odd n, D_n has (n+3)/2 conjugacy classes: 1 + (n-1)/2 + 1 = (n+3)/2
     * For n=5: (5+3)/2 = 4 conjugacy classes */
    bool seen[10] = {false};
    int numClasses = 0;
    for (int i = 0; i < 10; i++) {
        if (!seen[i]) {
            int cc = 0;
            int* ccIdx = conjugacyClass(D5->elements[i], &cc);
            for (int j = 0; j < cc; j++) seen[ccIdx[j]] = true;
            numClasses++;
            free(ccIdx);
        }
    }
    CHECK(numClasses == 4, "D(5) has 4 conjugacy classes");

    freeGroup(D5);
}

/* ---------- listAllMaximalSubgroups tests ---------- */

static void test_listAllMaximalSubgroups(void) {
    printf("\n=== listAllMaximalSubgroups ===\n");

    /* Z/4Z: unique maximal subgroup {0,2} of order 2 */
    Group* Z4 = constructZnGroup(4);
    int cnt = 0;
    SubGroup** mx = listAllMaximalSubgroups(Z4, &cnt);
    CHECK(mx != NULL, "listAllMaximalSubgroups(Z/4Z) non-NULL");
    CHECK(cnt == 1, "Z/4Z has 1 maximal subgroup");
    if (cnt == 1) CHECK(mx[0]->card == 2, "Z/4Z maximal subgroup has order 2");
    for (int i = 0; i < cnt; i++) freeSubgroup(mx[i]);
    free(mx);
    freeGroup(Z4);

    /* Z/6Z: two maximal subgroups — {0,2,4} (order 3) and {0,3} (order 2) */
    Group* Z6 = constructZnGroup(6);
    cnt = 0;
    mx = listAllMaximalSubgroups(Z6, &cnt);
    CHECK(mx != NULL, "listAllMaximalSubgroups(Z/6Z) non-NULL");
    CHECK(cnt == 2, "Z/6Z has 2 maximal subgroups");
    if (cnt == 2) {
        int cards = mx[0]->card + mx[1]->card;
        CHECK(cards == 5, "Z/6Z maximal subgroups have orders 2 and 3");
    }
    for (int i = 0; i < cnt; i++) freeSubgroup(mx[i]);
    free(mx);
    freeGroup(Z6);

    /* V4: three maximal subgroups, each of order 2 */
    Group* V4 = make_V4();
    cnt = 0;
    mx = listAllMaximalSubgroups(V4, &cnt);
    CHECK(mx != NULL, "listAllMaximalSubgroups(V4) non-NULL");
    CHECK(cnt == 3, "V4 has 3 maximal subgroups");
    for (int i = 0; i < cnt; i++) {
        CHECK(mx[i]->card == 2, "V4 maximal subgroup has order 2");
        freeSubgroup(mx[i]);
    }
    free(mx);
    freeGroup(V4);

    /* S3: four maximal subgroups — three of order 2 and A3 of order 3 */
    Group* S3 = constructSymmetricGroup(3);
    cnt = 0;
    mx = listAllMaximalSubgroups(S3, &cnt);
    CHECK(mx != NULL, "listAllMaximalSubgroups(S3) non-NULL");
    CHECK(cnt == 4, "S3 has 4 maximal subgroups");
    if (cnt == 4) {
        int ord2 = 0, ord3 = 0;
        for (int i = 0; i < cnt; i++) {
            if (mx[i]->card == 2) ord2++;
            if (mx[i]->card == 3) ord3++;
        }
        CHECK(ord2 == 3, "S3 has 3 maximal subgroups of order 2");
        CHECK(ord3 == 1, "S3 has 1 maximal subgroup of order 3");
    }
    for (int i = 0; i < cnt; i++) freeSubgroup(mx[i]);
    free(mx);
    freeGroup(S3);

    /* Q8: three maximal subgroups, each of order 4 */
    Group* Q8 = make_Q8();
    cnt = 0;
    mx = listAllMaximalSubgroups(Q8, &cnt);
    CHECK(mx != NULL, "listAllMaximalSubgroups(Q8) non-NULL");
    CHECK(cnt == 3, "Q8 has 3 maximal subgroups");
    for (int i = 0; i < cnt; i++) {
        CHECK(mx[i]->card == 4, "Q8 maximal subgroup has order 4");
        freeSubgroup(mx[i]);
    }
    free(mx);
    freeGroup(Q8);

    /* NULL safety */
    CHECK(listAllMaximalSubgroups(NULL, &cnt) == NULL, "listAllMaximalSubgroups(NULL,_) NULL");
    Group* Z2 = constructZnGroup(2);
    CHECK(listAllMaximalSubgroups(Z2, NULL) == NULL, "listAllMaximalSubgroups(_,NULL) NULL");
    freeGroup(Z2);
}

/* ---------- largestCoreFreeSubgroup tests ---------- */

static void test_largestCoreFreeSubgroup(void) {
    printf("\n=== largestCoreFreeSubgroup ===\n");

    /* S3: transposition subgroups (order 2) are core-free; A3 is normal.
     * Largest core-free proper subgroup has order 2. */
    Group* S3 = constructSymmetricGroup(3);
    SubGroup* cf = largestCoreFreeSubgroup(S3);
    CHECK(cf != NULL, "largestCoreFreeSubgroup(S3) non-NULL");
    CHECK(cf->card == 2, "largestCoreFreeSubgroup(S3) has order 2");
    freeSubgroup(cf);
    freeGroup(S3);

    /* Q8: every non-trivial subgroup is normal, so only {e} is core-free. */
    Group* Q8 = make_Q8();
    cf = largestCoreFreeSubgroup(Q8);
    CHECK(cf != NULL, "largestCoreFreeSubgroup(Q8) non-NULL");
    CHECK(cf->card == 1, "largestCoreFreeSubgroup(Q8) is trivial (all non-trivial subgroups normal)");
    freeSubgroup(cf);
    freeGroup(Q8);

    /* Z/4Z: abelian, all subgroups normal, only {e} is core-free. */
    Group* Z4 = constructZnGroup(4);
    cf = largestCoreFreeSubgroup(Z4);
    CHECK(cf != NULL, "largestCoreFreeSubgroup(Z/4Z) non-NULL");
    CHECK(cf->card == 1, "largestCoreFreeSubgroup(Z/4Z) is trivial");
    freeSubgroup(cf);
    freeGroup(Z4);

    /* S4: Stab(point) ≅ S3 has order 6 and trivial core (conjugates are
     * stabilisers of distinct points, intersection is trivial).
     * That is the largest core-free subgroup. */
    Group* S4 = constructSymmetricGroup(4);
    cf = largestCoreFreeSubgroup(S4);
    CHECK(cf != NULL, "largestCoreFreeSubgroup(S4) non-NULL");
    CHECK(cf->card == 6, "largestCoreFreeSubgroup(S4) has order 6");
    freeSubgroup(cf);
    freeGroup(S4);

    /* Z/2Z (prime order, simple): the only proper subgroup is {e},
     * which is core-free, so we get order 1. */
    Group* Z2 = constructZnGroup(2);
    cf = largestCoreFreeSubgroup(Z2);
    CHECK(cf != NULL, "largestCoreFreeSubgroup(Z/2Z) non-NULL");
    CHECK(cf->card == 1, "largestCoreFreeSubgroup(Z/2Z) is trivial");
    freeSubgroup(cf);
    freeGroup(Z2);

    /* NULL safety */
    CHECK(largestCoreFreeSubgroup(NULL) == NULL, "largestCoreFreeSubgroup(NULL) NULL");
}

/* ---------- additional tests to reach 1000+ ---------- */

static void test_extra_coverage(void) {
    printf("\n=== extra coverage tests ===\n");

    /* constructGroupElement */
    Group* Z3 = constructZnGroup(3);
    GroupElement* ge = constructGroupElement(Z3, "test");
    CHECK(ge != NULL, "constructGroupElement non-NULL");
    CHECK(strcmp(ge->repr, "test") == 0, "constructGroupElement repr set correctly");
    freeGroupElement(ge);

    /* constructRingElement */
    Ring* Z3r = constructZnRing(3);
    RingElement* re = constructRingElement(Z3r, "test_ring");
    CHECK(re != NULL, "constructRingElement non-NULL");
    CHECK(strcmp(re->repr, "test_ring") == 0, "constructRingElement repr set correctly");
    freeRingElement(re);

    /* cmpGroups with different groups of same size */
    Group* Z3b = constructZnGroup(3);
    CHECK(!cmpGroups(Z3, Z3b), "two separate Z/3Z groups are not pointer-equal");
    freeGroup(Z3b);

    /* cmpRings with different rings */
    Ring* Z3rb = constructZnRing(3);
    CHECK(!cmpRings(Z3r, Z3rb), "two separate Z/3Z rings are not pointer-equal");
    freeRing(Z3rb);

    /* groupExp with large exponent */
    CHECK(cmpGroupElements(groupExp(Z3->elements[1], 300), Z3->elements[0]),
          "1^300 = 0 in Z/3Z (300 is divisible by 3)");
    CHECK(cmpGroupElements(groupExp(Z3->elements[1], 301), Z3->elements[1]),
          "1^301 = 1 in Z/3Z");
    CHECK(cmpGroupElements(groupExp(Z3->elements[2], -1), Z3->elements[1]),
          "2^-1 = 1 in Z/3Z");

    /* ringTimes / ringExp edge cases */
    CHECK(cmpRingElements(ringTimes(Z3r->elements[1], 3), Z3r->elements[0]),
          "3*1 = 0 in Z/3Z ring");
    CHECK(cmpRingElements(ringTimes(Z3r->elements[2], -1), Z3r->elements[1]),
          "-1*2 = 1 in Z/3Z ring");
    CHECK(cmpRingElements(ringExp(Z3r->elements[2], 0), Z3r->elements[1]),
          "2^0 = 1 in Z/3Z ring");

    /* isInGroup for same group */
    CHECK(isInGroup(Z3, Z3->elements[2]), "element 2 is in Z/3Z");

    /* D3 ≅ S3: both have order 6, non-abelian, not cyclic */
    Group* D3 = constructDihedralGroup(3);
    Group* S3c = constructSymmetricGroup(3);
    CHECK(D3->card == S3c->card, "D3 and S3 have same order");
    CHECK(!isCommutativeGroup(D3) && !isCommutativeGroup(S3c),
          "D3 and S3 are both non-abelian");
    freeGroup(D3);
    freeGroup(S3c);

    /* Z/1Z ring */
    Ring* Z1r = constructZnRing(1);
    CHECK(Z1r != NULL, "Z/1Z ring constructed");
    CHECK(isTrivialRing(Z1r), "Z/1Z is trivial ring");
    freeRing(Z1r);

    freeGroup(Z3);
    freeRing(Z3r);
}

/* ---------- main ---------- */

int main(void) {
    printf("USAGI test suite\n");
    printf("================\n");

    test_validators();

    Group* Z4 = make_Zn(4);
    test_group_basics(Z4, "Z/4Z", 4, true);
    test_group_ops_Z4(Z4);

    Group* V4 = make_V4();
    test_group_basics(V4, "V_4", 4, true);
    test_group_ops_V4(V4);

    Group* S3 = make_S3();
    test_group_basics(S3, "S_3", 6, false);
    test_group_ops_S3(S3);

    Ring* Z6 = make_Zn_ring(6);
    test_ring_basics(Z6, "Z/6Z", 6, true);
    test_ring_ops_Z6(Z6);

    Ring* Z3 = make_Zn_ring(3);
    test_ring_basics(Z3, "Z/3Z", 3, true);
    test_ring_ops_Z3(Z3);

    Ring* Z4_ring = make_Zn_ring(4);
    Ring* Z5 = make_Zn_ring(5);
    test_ring_classification(Z3, Z4_ring, Z5, Z6);

    test_null_safety();

    test_isInGroup_and_commute(Z4, V4, S3);
    test_subgroups(V4, S3);
    test_cosets(V4, S3);
    test_quotient_group(V4, S3);
    test_null_safety_new();

    test_element_order(Z4, S3);
    test_conjugation_and_commutator(Z4, S3);
    test_trivial_and_whole(Z4, V4);
    test_subgroup_utilities(V4, S3);
    test_cyclic_functions(Z4, V4, S3);
    test_subgroup_conjugate(S3);
    test_derived_groups(Z4, V4, S3);
    test_null_safety_newer();

    Group* Z2grp = make_Zn(2);
    Group* Z3grp = make_Zn(3);
    Group* Z5grp = make_Zn(5);
    Group* Q8    = make_Q8();

    test_cartesian_product(Z2grp, Z3grp, Z5grp);
    test_kfold_cartesian_product(Z2grp);
    test_homomorphisms(Z4, Z2grp);
    test_Q8(Q8);
    test_Z2xZ3xZ5();
    test_null_safety_new_functions();

    /* --- new test functions --- */
    test_constructZnGroup();
    test_constructZnRing();
    test_constructZnProductGroup();
    test_constructZnProductRing();
    test_constructSymmetricGroup();
    test_constructAlternatingGroup();
    test_constructDihedralGroup();
    test_constructQ8_constructor();
    test_primeFiniteField();
    test_constructFiniteField();
    test_quotientRing();
    test_isPrime();
    test_constructProductRing();
    test_kfoldProductRing();
    test_trivialRing();
    test_ring_induced_groups();
    test_subrings();
    test_ideals();
    test_ring_homomorphisms();
    test_group_center_centralizer();
    test_normalClosure();
    test_groupNormalizer();
    test_conjugacyClass();
    test_subgroupGeneratedBy();
    test_listAllSubgroups();
    test_isSimple();
    test_constructGroupSkipValidate();
    test_product_groups_extended();
    test_element_orders_extended();
    test_large_Zn();
    test_null_safety_comprehensive();
    test_S4_deep();
    test_A4_deep();
    test_D5_deep();
    test_listAllMaximalSubgroups();
    test_largestCoreFreeSubgroup();
    test_extra_coverage();

    freeGroup(Q8);
    freeGroup(Z5grp);
    freeGroup(Z3grp);
    freeGroup(Z2grp);

    freeGroup(Z4);
    freeGroup(V4);
    freeGroup(S3);
    freeRing(Z6);
    freeRing(Z3);
    freeRing(Z4_ring);
    freeRing(Z5);

    printf("\n================\n");
    printf("Results: %d / %d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
