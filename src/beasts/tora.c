#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<math.h>
#include "hebi.h"
#include "sokko.h"
#include "usagi.h"
#include "tora.h"

/* ---------- Construct and free methods ---------- */

// Constructs a ConjugacyClass struct given a representative GroupElement and a Group
ConjugacyClass* constructConjugacyClass(Group* group, GroupElement* rep) {
    if (!group || !rep) return NULL;

    ConjugacyClass* class = malloc(sizeof(ConjugacyClass));
    if (!class) return NULL;

    class->group = group;
    class->rep = rep;
    class->elementOrder = elementOrder(group, rep);
    class->size = 0;

    int* indices = conjugacyClass(rep, &class->size);
    if (!indices) {
        free(class);
        return NULL;
    }
    class->indices = indices;

    return class;
}

// Free a ConjugacyClass struct
void freeConjugacyClass(ConjugacyClass* class) {
    if (!class) return;
    free(class->indices);
    free(class);
}

// Construct a Representation struct given a group, a name, and a list of images of the generators
Representation* constructRepresentation(Group* group, char* repr, Matrix** images, int mdim, int dim) {
    if (!group || !repr || !images) return NULL;

    Representation* rep = malloc(sizeof(Representation));
    if (!rep) return NULL;

    char* newRepr = malloc(strlen(repr)+1);
    if (!newRepr) {
        free(rep);
        return NULL;
    }
    strcpy(newRepr, repr);

    rep->repr = newRepr;
    rep->group = group;
    rep->images = images;
    rep->mdim = mdim;
    rep->dim = dim;

    return rep;
}

// Free a Representation struct
void freeRepresentation(Representation* rep) {
    if (!rep) return;
    free(rep->repr);
    if (rep->images) {
        for (int i = 0; i < rep->group->card; i++) freeMatrix(rep->images[i]);
        free(rep->images);
    }
    free(rep);
}


// Construct a Character struct given a group, a name, a list of conjugacy classes, and a list of character values on those classes
Character* constructCharacter(Group* group, char* repr, ConjugacyClass** classes, ComplexNumber* values, int numClasses) {
    if (!group || !repr || !classes || !values) return NULL;

    Character* character = malloc(sizeof(Character));
    if (!character) return NULL;

    char* newRepr = malloc(strlen(repr)+1);
    if (!newRepr) {
        free(character);
        return NULL;
    }
    strcpy(newRepr, repr);

    character->repr = newRepr;
    character->group = group;
    character->classes = classes;
    character->values = values;
    character->numClasses = numClasses;

    return character;
}

// Free a Character struct
void freeCharacter(Character* character) {
    if (!character) return;
    free(character->repr);
    free(character->values);
    free(character->classes);
    free(character);
}

// Construct a CharacterTable struct given a group, a list of conjugacy classes, a list of irreducible characters, and a 2D array of character values
CharacterTable* constructCharacterTable(Group* group, ConjugacyClass** classes, Character** irreps, ComplexNumber** values, int numClasses, int numIrreps) {
    if (!group || !classes || !irreps || !values) return NULL;

    CharacterTable* table = malloc(sizeof(CharacterTable));
    if (!table) return NULL;

    table->group = group;
    table->classes = classes;
    table->irreps = irreps;
    table->values = values;
    table->numClasses = numClasses;
    table->numIrreps = numIrreps;

    return table;
}

// Free a CharacterTable struct
void freeCharacterTable(CharacterTable* table) {
    if (!table) return;
    free(table->classes);
    free(table->irreps);
    if (table->values) {
        for (int i = 0; i < table->numIrreps; i++) free(table->values[i]);
        free(table->values);
    }
    free(table);
}


/* ---------- ConjugacyClass helpers ---------- */

// Gets the ConjugacyClass of a GroupElement
ConjugacyClass* getConjugacyClass(GroupElement* element) {
    if (!element) return NULL;

    return constructConjugacyClass(element->group, element);
}

// Gets the ConjugacyClasses of a Group; modifies numClasses to be the number of classes
ConjugacyClass** getConjugacyClasses(Group* group, int* numClasses) {
    if (!group || !numClasses) return NULL;

    ConjugacyClass** classes = malloc(group->card * sizeof(ConjugacyClass*));
    if (!classes) return NULL;

    int count = 0;
    for (int i = 0; i < group->card; i++) {
        GroupElement* rep = group->elements[i];
        bool isNewClass = true;
        for (int j = 0; j < count; j++) {
            for (int k = 0; k < classes[j]->size; k++) {
                if (rep->index == classes[j]->indices[k]) {
                    isNewClass = false;
                    break;
                }
            }
            if (!isNewClass) break;
        }
        if (isNewClass) {
            classes[count] = constructConjugacyClass(group, rep);
            if (!classes[count]) {
                for (int j = 0; j < count; j++) freeConjugacyClass(classes[j]);
                free(classes);
                return NULL;
            }
            count++;
        }
    }

    *numClasses = count;
    return classes;
}

/* ---------- Representations ---------- */

// Construct the trivial representation of a group
Representation* trivialRepresentation(Group* G) {
    if (!G) return NULL;

    Matrix** images = malloc(G->card * sizeof(Matrix*));
    if (!images) return NULL;
    for (int i = 0; i < G->card; i++) {
        images[i] = constructMatrix(1, 1);
        if (!images[i]) {
            for (int j = 0; j < i; j++) freeMatrix(images[j]);
            free(images);
            return NULL;
        }
        setEntry(images[i], 0, 0, elemFromReal(1.0));
    }

    Representation* rep = constructRepresentation(G, "C1", images, 1, 1);
    if (!rep) {
        for (int i = 0; i < G->card; i++) freeMatrix(images[i]);
        free(images);
        return NULL;
    }
    return rep;
}

// Construct the regular representation of a Group
Representation* regularRepresentation(Group* G) {
    if (!G) return NULL;

    int dim = G->card;

    Matrix** images = malloc(G->card * sizeof(Matrix*));
    if (!images) return NULL;

    for (int i = 0; i < G->card; i++) {
        images[i] = constructMatrix(dim, dim);
        if (!images[i]) {
            for (int j = 0; j < i; j++) freeMatrix(images[j]);
            free(images);
            return NULL;
        }
        for (int j = 0; j < G->card; j++) {
            setEntry(images[i], G->table[i][j], j, elemFromReal(1.0));
        }
    }

    Representation* rep = constructRepresentation(G, "R", images, dim, dim);
    if (!rep) {
        for (int i = 0; i < G->card; i++) freeMatrix(images[i]);
        free(images);
        return NULL;
    }
    return rep;
}

// Computes the permutation representation of a Group, i.e. the reduced permutation representation
Representation* permutationRepresentation(Group* G) {
    if (!G) return NULL;

    SubGroup* H = largestCoreFreeSubgroup(G);
    if (!H) return regularRepresentation(G);

    // If H is the whole group, the permutation rep is the trivial rep
    if (H->card == G->card) {
        freeSubgroup(H);
        return trivialRepresentation(G);
    }

    int dim = subgroupIndex(H);

    GroupCoset** cosets = getLeftGroupCosets(H);
    if (!cosets) {
        freeSubgroup(H);
        return NULL;
    }

    Matrix** images = malloc(G->card * sizeof(Matrix*));
    if (!images) {
        for (int j = 0; j < dim; j++) freeGroupCoset(cosets[j]);
        free(cosets);
        freeSubgroup(H);
        return NULL;
    }

    for (int i = 0; i < G->card; i++) {
        images[i] = constructMatrix(dim, dim);
        if (!images[i]) {
            for (int j = 0; j < i; j++) freeMatrix(images[j]);
            free(images);
            for (int j = 0; j < dim; j++) freeGroupCoset(cosets[j]);
            free(cosets);
            freeSubgroup(H);
            return NULL;
        }
        for (int j = 0; j < dim; j++) {
            int prodIndex = G->table[i][cosets[j]->indices[0]];
            for (int k = 0; k < dim; k++) {
                bool inCoset = false;
                for (int m = 0; m < H->card; m++) {
                    if (prodIndex == cosets[k]->indices[m]) { inCoset = true; break; }
                }
                if (inCoset) {
                    setEntry(images[i], k, j, elemFromReal(1.0));
                    break;
                }
            }
        }
    }

    Representation* rep = constructRepresentation(G, "P", images, dim, dim);
    if (!rep) {
        for (int i = 0; i < G->card; i++) freeMatrix(images[i]);
        free(images);
        for (int j = 0; j < dim; j++) freeGroupCoset(cosets[j]);
        free(cosets);
        freeSubgroup(H);
        return NULL;
    }

    for (int j = 0; j < dim; j++) freeGroupCoset(cosets[j]);
    free(cosets);
    freeSubgroup(H);
    return rep;
}

// Gets the standard representation of a Group, i.e. the reduced permutation representation on the nontrivial conjugacy classes
Representation* standardRepresentation(Group* G) {
    if (!G) return NULL;

    Representation* perm = permutationRepresentation(G);
    if (!perm) return NULL;
    int n = perm->dim;
    if (n <= 1) return perm; // trivial group, nothing to reduce

    int sdim = n - 1;
    Matrix** images = malloc(G->card * sizeof(Matrix*));
    if (!images) {
        freeRepresentation(perm);
        return NULL;
    }

    for (int g = 0; g < G->card; g++) {
        images[g] = constructMatrix(sdim, sdim);
        if (!images[g]) {
            for (int j = 0; j < g; j++) freeMatrix(images[j]);
            free(images);
            freeRepresentation(perm);
            return NULL;
        }
        for (int a = 1; a < n; a++) {
            for (int k = 1; k < n; k++) {
                MatrixElement ak = getEntry(perm->images[g], a, k);
                MatrixElement a0 = getEntry(perm->images[g], a, 0);
                MatrixElement val = elemSub(ak, a0);
                if (!elemIsZero(val, 1e-12))
                    setEntry(images[g], a-1, k-1, val);
            }
        }
    }

    Representation* rep = constructRepresentation(G, "S", images, sdim, sdim);
    if (!rep) {
        for (int i = 0; i < G->card; i++) freeMatrix(images[i]);
        free(images);
        freeRepresentation(perm);
        return NULL;
    }
    freeRepresentation(perm);
    return rep;
}

// Project a Group G onto its abelianization Q = G/[G,G] as a GroupHomomorphism.
// The codomain Q is a freshly-constructed Group; the caller is responsible for
// freeing both the homomorphism (via freeGroupHomomorphism) and the codomain
// (via freeGroup) when done.
GroupHomomorphism* projectToAbelianization(Group* G) {
    if (!G) return NULL;

    SubGroup* commutator = commutatorSubgroup(G);
    if (!commutator) return NULL;

    GroupCoset** cosets = getLeftGroupCosets(commutator);
    if (!cosets) { freeSubgroup(commutator); return NULL; }
    int n = G->card / commutator->card;

    Group* Q = quotientGroup(G, commutator);
    if (!Q) {
        for (int j = 0; j < n; j++) freeGroupCoset(cosets[j]);
        free(cosets);
        freeSubgroup(commutator);
        return NULL;
    }

    int* mapping = malloc(G->card * sizeof(int));
    if (!mapping) {
        freeGroup(Q);
        for (int j = 0; j < n; j++) freeGroupCoset(cosets[j]);
        free(cosets);
        freeSubgroup(commutator);
        return NULL;
    }
    for (int g = 0; g < G->card; g++) mapping[g] = -1;
    for (int c = 0; c < n; c++) {
        for (int m = 0; m < commutator->card; m++) {
            mapping[cosets[c]->indices[m]] = c;
        }
    }
    for (int j = 0; j < n; j++) freeGroupCoset(cosets[j]);
    free(cosets);
    freeSubgroup(commutator);

    GroupHomomorphism* hom = constructGroupHomomorphism(G, Q, mapping, G->card);
    if (!hom) {
        free(mapping);
        freeGroup(Q);
        return NULL;
    }
    return hom;
}

// The 1D sign representation of the symmetric group S_n. Element i (in lex order
// matching constructSymmetricGroup) maps to (-1)^{permSign}.
Representation* signRepresentation(Group* Sn) {
    if (!Sn) return NULL;

    // Recover n from |Sn| = n!
    int n = 1;
    int f = 1;
    while (f < Sn->card) { n++; f *= n; }
    if (f != Sn->card) return NULL;
    if (n == 1) return trivialRepresentation(Sn);

    int* perm = malloc(n * sizeof(int));
    if (!perm) return NULL;
    for (int i = 0; i < n; i++) perm[i] = i;

    Matrix** images = malloc(Sn->card * sizeof(Matrix*));
    if (!images) { free(perm); return NULL; }

    for (int idx = 0; idx < Sn->card; idx++) {
        images[idx] = constructMatrix(1, 1);
        if (!images[idx]) {
            for (int j = 0; j < idx; j++) freeMatrix(images[j]);
            free(images);
            free(perm);
            return NULL;
        }
        int s = permSign(perm, n); // 0 for even, 1 for odd
        setEntry(images[idx], 0, 0, elemFromReal(s == 0 ? 1.0 : -1.0));
        if (idx + 1 < Sn->card) nextPermutation(perm, n);
    }
    free(perm);

    Representation* rep = constructRepresentation(Sn, "Sgn", images, 1, 1);
    if (!rep) {
        for (int i = 0; i < Sn->card; i++) freeMatrix(images[i]);
        free(images);
        return NULL;
    }
    return rep;
}

// Dual representation V*: rho*(g) = (rho(g^{-1}))^T. Acts on V*.
Representation* dualRepresentation(Representation* V) {
    if (!V) return NULL;
    Group* G = V->group;
    if (!G) return NULL;

    Matrix** images = malloc(G->card * sizeof(Matrix*));
    if (!images) return NULL;

    for (int g = 0; g < G->card; g++) {
        // Find inverse index of g
        int gInv = -1;
        for (int x = 0; x < G->card; x++) {
            if (G->table[g][x] == 0) { gInv = x; break; }
        }
        if (gInv < 0) {
            for (int j = 0; j < g; j++) freeMatrix(images[j]);
            free(images);
            return NULL;
        }
        images[g] = transpose(V->images[gInv]);
        if (!images[g]) {
            for (int j = 0; j < g; j++) freeMatrix(images[j]);
            free(images);
            return NULL;
        }
    }

    Representation* rep = constructRepresentation(G, "Dual", images, V->mdim, V->dim);
    if (!rep) {
        for (int i = 0; i < G->card; i++) freeMatrix(images[i]);
        free(images);
        return NULL;
    }
    return rep;
}

// Complex-conjugate representation: bar-rho(g) is rho(g) with each entry
// complex-conjugated (real entries unchanged).
Representation* conjugateRepresentation(Representation* V) {
    if (!V) return NULL;
    Group* G = V->group;
    if (!G) return NULL;

    Matrix** images = malloc(G->card * sizeof(Matrix*));
    if (!images) return NULL;

    for (int g = 0; g < G->card; g++) {
        Matrix* A = V->images[g];
        Matrix* M = constructMatrix(A->numRows, A->numCols);
        if (!M) {
            for (int j = 0; j < g; j++) freeMatrix(images[j]);
            free(images);
            return NULL;
        }
        for (int i = 0; i < A->numRows; i++) {
            for (int j = 0; j < A->numCols; j++) {
                MatrixElement e = getEntry(A, i, j);
                if (!elemIsZero(e, 1e-15))
                    setEntry(M, i, j, elemConj(e));
            }
        }
        images[g] = M;
    }

    Representation* rep = constructRepresentation(G, "Conj", images, V->mdim, V->dim);
    if (!rep) {
        for (int i = 0; i < G->card; i++) freeMatrix(images[i]);
        free(images);
        return NULL;
    }
    return rep;
}

/* ---------- Helpers for symmetric and wedge product indexing ---------- */

// For Sym^2 with i <= j: linear index k = i*n - i*(i-1)/2 + (j - i)
static int symIdx(int i, int j, int n) {
    if (i > j) { int t = i; i = j; j = t; }
    return i * n - (i * (i - 1)) / 2 + (j - i);
}

// For Lambda^2 with i < j: linear index k = i*n - i*(i+1)/2 + (j - i - 1)
static int wedgeIdx(int i, int j, int n) {
    return i * n - (i * (i + 1)) / 2 + (j - i - 1);
}

/* ---------- Helpers for get1Dreps ---------- */

// Propagate a partial 1D-character assignment via the multiplication table.
// Returns -1 on contradiction, 0 otherwise. Modifies vals and set in place.
static int propagateCharacter(Group* G, double tol, ComplexNumber* vals, bool* set, int seed) {
    int* queue = malloc(G->card * sizeof(int));
    if (!queue) return -1;
    int qLen = 0, qHead = 0;
    queue[qLen++] = seed;

    while (qHead < qLen) {
        int x = queue[qHead++];
        for (int y = 0; y < G->card; y++) {
            if (!set[y]) continue;
            ComplexNumber pxy = complexMul(vals[x], vals[y]);
            ComplexNumber pyx = complexMul(vals[y], vals[x]);
            int zxy = G->table[x][y];
            int zyx = G->table[y][x];
            if (!set[zxy]) {
                set[zxy] = true; vals[zxy] = pxy;
                queue[qLen++] = zxy;
            } else if (!complexEq(vals[zxy], pxy, tol)) {
                free(queue); return -1;
            }
            if (!set[zyx]) {
                set[zyx] = true; vals[zyx] = pyx;
                queue[qLen++] = zyx;
            } else if (!complexEq(vals[zyx], pyx, tol)) {
                free(queue); return -1;
            }
        }
    }
    free(queue);
    return 0;
}

// Build a 1D Representation from a complete value array on G
static Representation* build1Drep(Group* G, ComplexNumber* vals, int idx) {
    Matrix** images = malloc(G->card * sizeof(Matrix*));
    if (!images) return NULL;
    for (int i = 0; i < G->card; i++) {
        images[i] = constructMatrix(1, 1);
        if (!images[i]) {
            for (int j = 0; j < i; j++) freeMatrix(images[j]);
            free(images);
            return NULL;
        }
        setEntry(images[i], 0, 0, elemFromComplex(vals[i]));
    }
    char repr[32];
    snprintf(repr, sizeof(repr), "1D-%d", idx);
    Representation* rep = constructRepresentation(G, repr, images, 1, 1);
    if (!rep) {
        for (int i = 0; i < G->card; i++) freeMatrix(images[i]);
        free(images);
        return NULL;
    }
    return rep;
}

static void backtrack1D(Group* G, double tol, ComplexNumber* vals, bool* set,
                        Representation*** out, int* count, int* cap) {
    int i = -1;
    for (int j = 0; j < G->card; j++) if (!set[j]) { i = j; break; }

    if (i < 0) {
        Representation* rep = build1Drep(G, vals, *count);
        if (!rep) return;
        if (*count == *cap) {
            int newCap = (*cap) * 2;
            Representation** grow = realloc(*out, newCap * sizeof(Representation*));
            if (!grow) { freeRepresentation(rep); return; }
            *out = grow;
            *cap = newCap;
        }
        (*out)[(*count)++] = rep;
        return;
    }

    int o = elementOrder(G, G->elements[i]);
    if (o <= 0) return;

    ComplexNumber* savedVals = malloc(G->card * sizeof(ComplexNumber));
    bool* savedSet = malloc(G->card * sizeof(bool));
    if (!savedVals || !savedSet) { free(savedVals); free(savedSet); return; }

    long double TAU = 2.0L * pi();
    for (int k = 0; k < o; k++) {
        memcpy(savedVals, vals, G->card * sizeof(ComplexNumber));
        memcpy(savedSet, set, G->card * sizeof(bool));

        ComplexNumber omega;
        omega.real = (double) cosl(TAU * (long double) k / (long double) o);
        omega.imag = (double) sinl(TAU * (long double) k / (long double) o);

        vals[i] = omega;
        set[i] = true;

        if (propagateCharacter(G, tol, vals, set, i) == 0) {
            backtrack1D(G, tol, vals, set, out, count, cap);
        }

        memcpy(vals, savedVals, G->card * sizeof(ComplexNumber));
        memcpy(set, savedSet, G->card * sizeof(bool));
    }
    free(savedVals);
    free(savedSet);
}

// Returns an array containing all 1D (linear) representations of G.
// Sets *count to the number of reps; caller frees each via freeRepresentation
// and frees the array. The number returned equals |G/[G,G]|.
Representation** get1Dreps(Group* G, int* count) {
    if (!G || !count) return NULL;
    *count = 0;

    int cap = 4;
    Representation** out = malloc(cap * sizeof(Representation*));
    if (!out) return NULL;

    ComplexNumber* vals = malloc(G->card * sizeof(ComplexNumber));
    bool* set = calloc(G->card, sizeof(bool));
    if (!vals || !set) { free(out); free(vals); free(set); return NULL; }

    // Identity always maps to 1
    vals[0].real = 1.0; vals[0].imag = 0.0;
    set[0] = true;

    backtrack1D(G, 1e-9, vals, set, &out, count, &cap);

    free(vals);
    free(set);
    return out;
}

// External direct-product (cross) representation V (><) W of G x H, where GxH must be
// the direct product Group whose elements are indexed as (i * H->card + j). The image
// at index (i*|H|+j) is the Kronecker product rho_V(g_i) (x) rho_W(h_j).
Representation* crossProductReps(Representation* V, Representation* W, Group* GxH) {
    if (!V || !W || !GxH) return NULL;
    Group* G = V->group;
    Group* H = W->group;
    if (!G || !H) return NULL;
    if (GxH->card != G->card * H->card) return NULL;

    int dim = V->dim * W->dim;
    Matrix** images = malloc(GxH->card * sizeof(Matrix*));
    if (!images) return NULL;

    for (int i = 0; i < G->card; i++) {
        for (int j = 0; j < H->card; j++) {
            int idx = i * H->card + j;
            images[idx] = tensorMatrices(V->images[i], W->images[j]);
            if (!images[idx]) {
                for (int m = 0; m < idx; m++) freeMatrix(images[m]);
                free(images);
                return NULL;
            }
        }
    }

    Representation* rep = constructRepresentation(GxH, "Vx", images, dim, dim);
    if (!rep) {
        for (int i = 0; i < GxH->card; i++) freeMatrix(images[i]);
        free(images);
        return NULL;
    }
    return rep;
}

// Internal tensor product V (x) W of two representations of the SAME group G.
// rho_{V (x) W}(g) = rho_V(g) (x) rho_W(g)
Representation* tensorProduct(Representation* V, Representation* W) {
    if (!V || !W) return NULL;
    if (!cmpGroups(V->group, W->group)) return NULL;
    Group* G = V->group;

    int dim = V->dim * W->dim;
    Matrix** images = malloc(G->card * sizeof(Matrix*));
    if (!images) return NULL;

    for (int i = 0; i < G->card; i++) {
        images[i] = tensorMatrices(V->images[i], W->images[i]);
        if (!images[i]) {
            for (int j = 0; j < i; j++) freeMatrix(images[j]);
            free(images);
            return NULL;
        }
    }

    Representation* rep = constructRepresentation(G, "VoW", images, dim, dim);
    if (!rep) {
        for (int i = 0; i < G->card; i++) freeMatrix(images[i]);
        free(images);
        return NULL;
    }
    return rep;
}

// Sym^2 V on basis {e_i e_j : i <= j}, with action g.(e_i e_j) = (g.e_i)(g.e_j).
// Coefficient of e_a e_b (a <= b) in g.(e_i e_j) is:
//   A[a][i] * A[a][j]                            if a == b
//   A[a][i] * A[b][j] + A[b][i] * A[a][j]        if a < b
// where A is the matrix of rho(g).
Representation* symmetricProduct(Representation* V) {
    if (!V) return NULL;
    Group* G = V->group;
    int n = V->dim;
    int sdim = n * (n + 1) / 2;
    if (sdim < 1) return NULL;

    Matrix** images = malloc(G->card * sizeof(Matrix*));
    if (!images) return NULL;

    for (int g = 0; g < G->card; g++) {
        Matrix* A = V->images[g];
        Matrix* M = constructMatrix(sdim, sdim);
        if (!M) {
            for (int j = 0; j < g; j++) freeMatrix(images[j]);
            free(images);
            return NULL;
        }
        for (int i = 0; i < n; i++) {
            for (int j = i; j < n; j++) {
                int col = symIdx(i, j, n);
                for (int a = 0; a < n; a++) {
                    for (int b = a; b < n; b++) {
                        int row = symIdx(a, b, n);
                        MatrixElement Aai = getEntry(A, a, i);
                        MatrixElement Aaj = getEntry(A, a, j);
                        MatrixElement Abi = getEntry(A, b, i);
                        MatrixElement Abj = getEntry(A, b, j);
                        MatrixElement val;
                        if (a == b) {
                            val = elemMul(Aai, Aaj);
                        } else {
                            val = elemAdd(elemMul(Aai, Abj), elemMul(Abi, Aaj));
                        }
                        if (!elemIsZero(val, 1e-15))
                            setEntry(M, row, col, val);
                    }
                }
            }
        }
        images[g] = M;
    }

    Representation* rep = constructRepresentation(G, "Sym2", images, sdim, sdim);
    if (!rep) {
        for (int i = 0; i < G->card; i++) freeMatrix(images[i]);
        free(images);
        return NULL;
    }
    return rep;
}

// Lambda^2 V on basis {e_i ^ e_j : i < j}, with action g.(e_i ^ e_j) = (g.e_i) ^ (g.e_j).
// Coefficient of e_a ^ e_b (a < b) in g.(e_i ^ e_j) is A[a][i]*A[b][j] - A[b][i]*A[a][j].
Representation* wedgeProduct(Representation* V) {
    if (!V) return NULL;
    Group* G = V->group;
    int n = V->dim;
    int wdim = n * (n - 1) / 2;
    if (wdim < 1) return NULL;

    Matrix** images = malloc(G->card * sizeof(Matrix*));
    if (!images) return NULL;

    for (int g = 0; g < G->card; g++) {
        Matrix* A = V->images[g];
        Matrix* M = constructMatrix(wdim, wdim);
        if (!M) {
            for (int j = 0; j < g; j++) freeMatrix(images[j]);
            free(images);
            return NULL;
        }
        for (int i = 0; i < n; i++) {
            for (int j = i + 1; j < n; j++) {
                int col = wedgeIdx(i, j, n);
                for (int a = 0; a < n; a++) {
                    for (int b = a + 1; b < n; b++) {
                        int row = wedgeIdx(a, b, n);
                        MatrixElement Aai = getEntry(A, a, i);
                        MatrixElement Aaj = getEntry(A, a, j);
                        MatrixElement Abi = getEntry(A, b, i);
                        MatrixElement Abj = getEntry(A, b, j);
                        MatrixElement val = elemSub(elemMul(Aai, Abj), elemMul(Abi, Aaj));
                        if (!elemIsZero(val, 1e-15))
                            setEntry(M, row, col, val);
                    }
                }
            }
        }
        images[g] = M;
    }

    Representation* rep = constructRepresentation(G, "Wedge2", images, wdim, wdim);
    if (!rep) {
        for (int i = 0; i < G->card; i++) freeMatrix(images[i]);
        free(images);
        return NULL;
    }
    return rep;
}

// Restrict a representation V of an ambient group G to a subgroup H realised as Hgroup.
// The caller supplies Hgroup, a Group whose i-th element corresponds to G->elements[H->indices[i]].
// (Such a Group can be built independently from H.)
Representation* restrictRepresentation(Representation* V, SubGroup* H, Group* Hgroup) {
    if (!V || !H || !Hgroup) return NULL;
    if (V->group != H->ambient) return NULL;
    if (Hgroup->card != H->card) return NULL;

    Matrix** images = malloc(H->card * sizeof(Matrix*));
    if (!images) return NULL;

    for (int i = 0; i < H->card; i++) {
        images[i] = copyMatrix(V->images[H->indices[i]]);
        if (!images[i]) {
            for (int j = 0; j < i; j++) freeMatrix(images[j]);
            free(images);
            return NULL;
        }
    }

    Representation* rep = constructRepresentation(Hgroup, "Res", images, V->mdim, V->dim);
    if (!rep) {
        for (int i = 0; i < H->card; i++) freeMatrix(images[i]);
        free(images);
        return NULL;
    }
    return rep;
}

// Induce a representation V from a subgroup H (realised so that V->group's i-th element
// matches H->ambient->elements[H->indices[i]]) up to the ambient group G = H->ambient.
// Result has dimension [G:H] * dim(V). For coset reps t_0, ..., t_{n-1}, the action of
// h on basis (t_a, v) is: choose b such that h * t_a in t_b * H, write h*t_a = t_b * k
// with k in H, then h.(t_a, v) = (t_b, rho_V(k) v). The block at (b, a) is rho_V(k);
// all other blocks are zero.
Representation* inducedRepresentation(Representation* V, SubGroup* H) {
    if (!V || !H) return NULL;
    Group* G = H->ambient;
    if (!G) return NULL;
    if (V->group->card != H->card) return NULL;

    GroupCoset** cosets = getLeftGroupCosets(H);
    if (!cosets) return NULL;
    int n = G->card / H->card;
    int dV = V->dim;
    int dim = n * dV;

    // Coset rep indices in G and inverses of coset reps
    int* tIdx = malloc(n * sizeof(int));
    int* tInv = malloc(n * sizeof(int));
    if (!tIdx || !tInv) {
        free(tIdx); free(tInv);
        for (int j = 0; j < n; j++) freeGroupCoset(cosets[j]);
        free(cosets);
        return NULL;
    }
    for (int a = 0; a < n; a++) {
        tIdx[a] = cosets[a]->indices[0];
        tInv[a] = -1;
        for (int x = 0; x < G->card; x++) {
            if (G->table[tIdx[a]][x] == 0) { tInv[a] = x; break; }
        }
        if (tInv[a] < 0) {
            free(tIdx); free(tInv);
            for (int j = 0; j < n; j++) freeGroupCoset(cosets[j]);
            free(cosets);
            return NULL;
        }
    }

    // Map ambient-group index -> position in H->indices (or -1 if not in H)
    int* posInH = malloc(G->card * sizeof(int));
    if (!posInH) {
        free(tIdx); free(tInv);
        for (int j = 0; j < n; j++) freeGroupCoset(cosets[j]);
        free(cosets);
        return NULL;
    }
    for (int i = 0; i < G->card; i++) posInH[i] = -1;
    for (int i = 0; i < H->card; i++) posInH[H->indices[i]] = i;

    Matrix** images = malloc(G->card * sizeof(Matrix*));
    if (!images) {
        free(tIdx); free(tInv); free(posInH);
        for (int j = 0; j < n; j++) freeGroupCoset(cosets[j]);
        free(cosets);
        return NULL;
    }

    for (int h = 0; h < G->card; h++) {
        Matrix* M = constructMatrix(dim, dim);
        if (!M) {
            for (int j = 0; j < h; j++) freeMatrix(images[j]);
            free(images);
            free(tIdx); free(tInv); free(posInH);
            for (int j = 0; j < n; j++) freeGroupCoset(cosets[j]);
            free(cosets);
            return NULL;
        }
        for (int a = 0; a < n; a++) {
            int hta = G->table[h][tIdx[a]];
            // Find coset b containing hta
            int b = -1;
            for (int bb = 0; bb < n; bb++) {
                for (int m = 0; m < H->card; m++) {
                    if (cosets[bb]->indices[m] == hta) { b = bb; break; }
                }
                if (b >= 0) break;
            }
            if (b < 0) {
                freeMatrix(M);
                for (int j = 0; j < h; j++) freeMatrix(images[j]);
                free(images);
                free(tIdx); free(tInv); free(posInH);
                for (int j = 0; j < n; j++) freeGroupCoset(cosets[j]);
                free(cosets);
                return NULL;
            }
            // k = t_b^{-1} * h * t_a, an element of H
            int kAmbient = G->table[tInv[b]][hta];
            int kLocal = posInH[kAmbient];
            if (kLocal < 0) {
                freeMatrix(M);
                for (int j = 0; j < h; j++) freeMatrix(images[j]);
                free(images);
                free(tIdx); free(tInv); free(posInH);
                for (int j = 0; j < n; j++) freeGroupCoset(cosets[j]);
                free(cosets);
                return NULL;
            }
            // Place V->images[kLocal] in block (b, a)
            Matrix* B = V->images[kLocal];
            for (int r = 0; r < dV; r++) {
                for (int c = 0; c < dV; c++) {
                    MatrixElement v = getEntry(B, r, c);
                    if (!elemIsZero(v, 1e-15))
                        setEntry(M, b * dV + r, a * dV + c, v);
                }
            }
        }
        images[h] = M;
    }

    Representation* rep = constructRepresentation(G, "Ind", images, dim, dim);
    free(tIdx); free(tInv); free(posInH);
    for (int j = 0; j < n; j++) freeGroupCoset(cosets[j]);
    free(cosets);
    if (!rep) {
        for (int i = 0; i < G->card; i++) freeMatrix(images[i]);
        free(images);
        return NULL;
    }
    return rep;
}

/* ---------- Characters and character tables ---------- */

// Compute the character value of a representation rep on a conjugacy class class.
// This is the trace of rep->images[g] for any g in class (all elements have the same trace).
ComplexNumber characterValue(Representation* rep, ConjugacyClass* class) {
    if (!rep || !class) return (ComplexNumber){0.0, 0.0};
    Group* G = rep->group;
    if (!G || G != class->group) return (ComplexNumber){0.0, 0.0};
    return elemToComplex(trace(rep->images[class->rep->index]));
}

// Build a Character struct from a Representation.
// The returned Character holds a freshly-allocated classes array and values; per the
// codebase's freeing convention the caller must walk chi->classes and freeConjugacyClass
// each entry before calling freeCharacter(chi).
Character* characterOfRepresentation(Representation* rep) {
    if (!rep || !rep->group) return NULL;

    int n;
    ConjugacyClass** classes = getConjugacyClasses(rep->group, &n);
    if (!classes) return NULL;

    ComplexNumber* values = malloc(n * sizeof(ComplexNumber));
    if (!values) {
        for (int i = 0; i < n; i++) freeConjugacyClass(classes[i]);
        free(classes);
        return NULL;
    }
    for (int i = 0; i < n; i++) values[i] = characterValue(rep, classes[i]);

    Character* chi = constructCharacter(rep->group, rep->repr, classes, values, n);
    if (!chi) {
        for (int i = 0; i < n; i++) freeConjugacyClass(classes[i]);
        free(classes);
        free(values);
        return NULL;
    }
    return chi;
}

// Standard inner product of two characters of the same group:
//   <chi, psi> = (1/|G|) * sum_C |C| * chi(C) * conj(psi(C))
// The two characters may have classes arranged in different orders; matching is by
// looking up each class representative's index in the other side's class lists.
ComplexNumber characterInnerProduct(Character* chi, Character* psi) {
    if (!chi || !psi) return (ComplexNumber){0.0, 0.0};
    if (chi->group != psi->group) return (ComplexNumber){0.0, 0.0};
    Group* G = chi->group;

    ComplexNumber sum = {0.0, 0.0};
    for (int i = 0; i < chi->numClasses; i++) {
        ConjugacyClass* C = chi->classes[i];
        if (!C) return (ComplexNumber){0.0, 0.0};

        // Find the class in psi that contains C's representative
        ComplexNumber psiVal = {0.0, 0.0};
        bool matched = false;
        int repIdx = C->rep->index;
        for (int j = 0; j < psi->numClasses; j++) {
            ConjugacyClass* D = psi->classes[j];
            for (int k = 0; k < D->size; k++) {
                if (D->indices[k] == repIdx) { psiVal = psi->values[j]; matched = true; break; }
            }
            if (matched) break;
        }
        if (!matched) return (ComplexNumber){0.0, 0.0};

        ComplexNumber term = complexMul(chi->values[i], complexConj(psiVal));
        term.real *= (double) C->size;
        term.imag *= (double) C->size;
        sum = complexAdd(sum, term);
    }

    sum.real /= (double) G->card;
    sum.imag /= (double) G->card;
    return sum;
}

// A representation is irreducible iff <chi, chi> = 1.
bool isIrreducible(Representation* rep) {
    if (!rep) return false;
    Character* chi = characterOfRepresentation(rep);
    if (!chi) return false;

    ComplexNumber inner = characterInnerProduct(chi, chi);
    bool irr = fabs(inner.real - 1.0) < 1e-6 && fabs(inner.imag) < 1e-6;

    for (int i = 0; i < chi->numClasses; i++) freeConjugacyClass(chi->classes[i]);
    freeCharacter(chi);
    return irr;
}

// Decompose a representation V of G into irreducibles given a CharacterTable T of G.
// Returns a freshly-allocated int array of length T->numIrreps where entry i is the
// multiplicity of T->irreps[i] in V (V = sum_i n_i V_i). Caller frees with free().
int* decomposeRepresentation(Representation* V, CharacterTable* T) {
    if (!V || !T) return NULL;
    if (V->group != T->group) return NULL;

    Character* chi = characterOfRepresentation(V);
    if (!chi) return NULL;

    int* mults = malloc(T->numIrreps * sizeof(int));
    if (!mults) {
        for (int i = 0; i < chi->numClasses; i++) freeConjugacyClass(chi->classes[i]);
        freeCharacter(chi);
        return NULL;
    }
    for (int i = 0; i < T->numIrreps; i++) {
        ComplexNumber n = characterInnerProduct(chi, T->irreps[i]);
        mults[i] = (int) round(n.real);
    }

    for (int i = 0; i < chi->numClasses; i++) freeConjugacyClass(chi->classes[i]);
    freeCharacter(chi);
    return mults;
}

/* ---------- Internal helpers: class algebra (Burnside-Dixon-Schneider) ---------- */

// Compute structure constants of the class algebra:
//   c[i*r*r + j*r + k] = coefficient of K_k in K_i K_j
//                     = #{ (x, y) : x in C_i, y in C_j, xy = z_k }
// where z_k is a fixed representative of class C_k.
static int* classAlgebraStructureConstants(Group* G, ConjugacyClass** classes, int r) {
    int* c = calloc((size_t) r * r * r, sizeof(int));
    if (!c) return NULL;

    // Precompute inverse-index lookup for ambient group elements
    int* invIdx = malloc(G->card * sizeof(int));
    if (!invIdx) { free(c); return NULL; }
    for (int x = 0; x < G->card; x++) {
        invIdx[x] = -1;
        for (int t = 0; t < G->card; t++) {
            if (G->table[x][t] == 0) { invIdx[x] = t; break; }
        }
        if (invIdx[x] < 0) { free(invIdx); free(c); return NULL; }
    }

    // Precompute: ambient-element index -> conjugacy class index
    int* classOf = malloc(G->card * sizeof(int));
    if (!classOf) { free(invIdx); free(c); return NULL; }
    for (int g = 0; g < G->card; g++) classOf[g] = -1;
    for (int j = 0; j < r; j++) {
        for (int yi = 0; yi < classes[j]->size; yi++) {
            classOf[classes[j]->indices[yi]] = j;
        }
    }

    for (int i = 0; i < r; i++) {
        ConjugacyClass* Ci = classes[i];
        for (int k = 0; k < r; k++) {
            int z = classes[k]->rep->index;
            for (int xi = 0; xi < Ci->size; xi++) {
                int x = Ci->indices[xi];
                int y = G->table[invIdx[x]][z]; // y = x^{-1} * z, so x * y = z
                int j = classOf[y];
                if (j >= 0) c[i * r * r + j * r + k]++;
            }
        }
    }
    free(invIdx);
    free(classOf);
    return c;
}

// Compute all r irreducible characters of G via simultaneous diagonalisation of the class
// algebra (Burnside-Dixon-Schneider). Returns an array of Character*; each Character has
// its own classes-array but the underlying ConjugacyClass objects are shared. Cleanup
// pattern (caller):
//     for (int i = 0; i < out[0]->numClasses; i++) freeConjugacyClass(out[0]->classes[i]);
//     for (int k = 0; k < count; k++) freeCharacter(out[k]);
//     free(out);
Character** allIrreducibleCharacters(Group* G, int* count) {
    if (!G || !count) return NULL;
    *count = 0;

    int r;
    ConjugacyClass** classes = getConjugacyClasses(G, &r);
    if (!classes) return NULL;

    int* c = classAlgebraStructureConstants(G, classes, r);
    if (!c) {
        for (int i = 0; i < r; i++) freeConjugacyClass(classes[i]);
        free(classes);
        return NULL;
    }

    // Build M[i] for i = 0..r-1: (M[i])_{kj} = c[i*r*r + j*r + k]
    Matrix** M = malloc(r * sizeof(Matrix*));
    if (!M) {
        free(c);
        for (int i = 0; i < r; i++) freeConjugacyClass(classes[i]);
        free(classes);
        return NULL;
    }
    for (int i = 0; i < r; i++) {
        M[i] = constructMatrix(r, r);
        if (!M[i]) {
            for (int x = 0; x < i; x++) freeMatrix(M[x]);
            free(M); free(c);
            for (int x = 0; x < r; x++) freeConjugacyClass(classes[x]);
            free(classes);
            return NULL;
        }
        for (int k = 0; k < r; k++)
            for (int j = 0; j < r; j++)
                setEntry(M[i], k, j, elemFromReal((double) c[i * r * r + j * r + k]));
    }

    // Locate identity class once (used for degree sorting/validation)
    int idClassPos = 0;
    for (int j = 0; j < r; j++) {
        if (classes[j]->size == 1 && classes[j]->rep->index == 0) {
            idClassPos = j;
            break;
        }
    }

    Character** out = NULL;
    int outCount = 0;
    Character** pool = calloc((size_t) r, sizeof(Character*));
    int poolCount = 0;
    if (!pool) {
        free(c);
        for (int i = 0; i < r; i++) freeMatrix(M[i]);
        free(M);
        for (int i = 0; i < r; i++) freeConjugacyClass(classes[i]);
        free(classes);
        return NULL;
    }

    // Different linear combinations can collide on larger groups (e.g. S5).
    // Retry with multiple deterministic seeds until we recover a valid full set.
    for (int attempt = 0; attempt < 256 && !out; attempt++) {
        unsigned int seed = 0x9E3779B9u ^ (unsigned int) (attempt * 0x85EBCA6Bu);

        Matrix* M_total = constructMatrix(r, r);
        if (!M_total) break;

        double* alpha = malloc((size_t) r * sizeof(double));
        if (!alpha) {
            freeMatrix(M_total);
            break;
        }
        alpha[0] = 0.0;
        for (int i = 1; i < r; i++) {
            seed = seed * 1103515245u + 12345u;
            alpha[i] = 1.0 + (double) ((seed >> 8) & 0xFFFFFFu) / (double) 0x1000000u;
        }

        for (int i = 1; i < r; i++) {
            for (int row = 0; row < r; row++) {
                for (int col = 0; col < r; col++) {
                    MatrixElement cur = getEntry(M_total, row, col);
                    MatrixElement mrc = getEntry(M[i], row, col);
                    MatrixElement add = elemMul(elemFromReal(alpha[i]), mrc);
                    setEntry(M_total, row, col, elemAdd(cur, add));
                }
            }
        }
        free(alpha);

        Matrix** evecs = eigenvectors(M_total);
        freeMatrix(M_total);
        if (!evecs) continue;

        Character** cand = malloc((size_t) r * sizeof(Character*));
        int candCount = 0;
        if (!cand) {
            for (int i = 0; i < r; i++) if (evecs[i]) freeMatrix(evecs[i]);
            free(evecs);
            break;
        }

        for (int a = 0; a < r; a++) {
            if (!evecs[a]) continue;
            Matrix* v = evecs[a];

            int jmax = 0;
            double vmax = elemAbs(getEntry(v, 0, 0));
            for (int j = 1; j < r; j++) {
                double mag = elemAbs(getEntry(v, j, 0));
                if (mag > vmax) { vmax = mag; jmax = j; }
            }
            if (vmax < 1e-12) continue;
            ComplexNumber v_ref = elemToComplex(getEntry(v, jmax, 0));

            ComplexNumber* omega = malloc((size_t) r * sizeof(ComplexNumber));
            if (!omega) continue;
            omega[0].real = 1.0;
            omega[0].imag = 0.0;

            bool bad = false;
            for (int i = 1; i < r; i++) {
                Matrix* Mv = applyMatrix(M[i], v);
                if (!Mv) { bad = true; break; }
                ComplexNumber Mv_jmax = elemToComplex(getEntry(Mv, jmax, 0));
                omega[i] = complexDiv(Mv_jmax, v_ref);
                freeMatrix(Mv);
            }
            if (bad) {
                free(omega);
                continue;
            }

            double sum = 0.0;
            for (int j = 0; j < r; j++) {
                double mag2 = omega[j].real * omega[j].real + omega[j].imag * omega[j].imag;
                sum += mag2 / (double) classes[j]->size;
            }
            if (sum < 1e-15) {
                free(omega);
                continue;
            }
            double dim_sq = (double) G->card / sum;
            int dim = (int) round(sqrt(dim_sq));
            if (dim < 1) dim = 1;

            ComplexNumber* values = malloc((size_t) r * sizeof(ComplexNumber));
            if (!values) {
                free(omega);
                continue;
            }
            for (int j = 0; j < r; j++) {
                values[j].real = (double) dim * omega[j].real / (double) classes[j]->size;
                values[j].imag = (double) dim * omega[j].imag / (double) classes[j]->size;
                if (fabs(values[j].real - round(values[j].real)) < 1e-6) values[j].real = round(values[j].real);
                if (fabs(values[j].imag - round(values[j].imag)) < 1e-6) values[j].imag = round(values[j].imag);
                if (fabs(values[j].imag) < 1e-9) values[j].imag = 0.0;
            }
            free(omega);

            ConjugacyClass** classesCopy = malloc((size_t) r * sizeof(ConjugacyClass*));
            if (!classesCopy) {
                free(values);
                continue;
            }
            for (int j = 0; j < r; j++) classesCopy[j] = classes[j];

            char label[32];
            snprintf(label, sizeof(label), "chi-%d", candCount + 1);
            Character* chi = constructCharacter(G, label, classesCopy, values, r);
            if (!chi) {
                free(classesCopy);
                free(values);
                continue;
            }
            cand[candCount++] = chi;
        }

        for (int i = 0; i < r; i++) if (evecs[i]) freeMatrix(evecs[i]);
        free(evecs);

        for (int i = 0; i < candCount; i++) {
            Character* chi = cand[i];
            if (!chi) continue;

            double dReal = chi->values[idClassPos].real;
            int d = (int) round(dReal);
            if (d < 1 || fabs(dReal - (double) d) > 2e-2) {
                freeCharacter(chi);
                continue;
            }
            ComplexNumber self = characterInnerProduct(chi, chi);
            if (fabs(self.real - 1.0) > 3e-2 || fabs(self.imag) > 3e-2) {
                freeCharacter(chi);
                continue;
            }

            bool duplicate = false;
            for (int p = 0; p < poolCount && !duplicate; p++) {
                bool same = true;
                for (int j = 0; j < r; j++) {
                    if (fabs(pool[p]->values[j].real - chi->values[j].real) > 1e-6 ||
                        fabs(pool[p]->values[j].imag - chi->values[j].imag) > 1e-6) {
                        same = false;
                        break;
                    }
                }
                if (same) duplicate = true;
            }

            if (duplicate || poolCount >= r) {
                freeCharacter(chi);
            } else {
                pool[poolCount++] = chi;
            }
        }
        free(cand);

        if (poolCount != r) continue;

        // Sort by degree, then lexicographically by character values to get stable order.
        for (int i = 1; i < poolCount; i++) {
            for (int j = i; j > 0; j--) {
                double da = pool[j]->values[idClassPos].real;
                double db = pool[j - 1]->values[idClassPos].real;
                bool swap = false;
                if (da < db - 1e-9) swap = true;
                else if (fabs(da - db) <= 1e-9) {
                    for (int k = 0; k < r; k++) {
                        double ar = pool[j]->values[k].real;
                        double br = pool[j - 1]->values[k].real;
                        if (fabs(ar - br) > 1e-9) { swap = ar > br; break; }
                        double ai = pool[j]->values[k].imag;
                        double bi = pool[j - 1]->values[k].imag;
                        if (fabs(ai - bi) > 1e-9) { swap = ai > bi; break; }
                    }
                }
                if (swap) {
                    Character* tmp = pool[j];
                    pool[j] = pool[j - 1];
                    pool[j - 1] = tmp;
                } else break;
            }
        }

        bool valid = true;
        int sumSq = 0;
        for (int i = 0; i < poolCount; i++) {
            double dReal = pool[i]->values[idClassPos].real;
            int d = (int) round(dReal);
            if (d < 1 || fabs(dReal - (double) d) > 2e-2) { valid = false; break; }
            sumSq += d * d;
            ComplexNumber self = characterInnerProduct(pool[i], pool[i]);
            if (fabs(self.real - 1.0) > 3e-2 || fabs(self.imag) > 3e-2) { valid = false; break; }
        }
        if (valid && sumSq == G->card) {
            for (int i = 0; i < poolCount && valid; i++) {
                for (int j = i + 1; j < poolCount; j++) {
                    ComplexNumber ip = characterInnerProduct(pool[i], pool[j]);
                    if (fabs(ip.real) > 3e-2 || fabs(ip.imag) > 3e-2) {
                        valid = false;
                        break;
                    }
                }
            }
        } else {
            valid = false;
        }

        if (!valid) {
            for (int i = 0; i < poolCount; i++) freeCharacter(pool[i]);
            for (int i = 0; i < r; i++) pool[i] = NULL;
            poolCount = 0;
            continue;
        }

        // Relabel in sorted order so chi-1, chi-2, ... matches printed order.
        for (int i = 0; i < poolCount; i++) {
            char label[32];
            snprintf(label, sizeof(label), "chi-%d", i + 1);
            char* fresh = malloc(strlen(label) + 1);
            if (!fresh) continue;
            strcpy(fresh, label);
            free(pool[i]->repr);
            pool[i]->repr = fresh;
        }

        out = pool;
        outCount = poolCount;
        pool = NULL;
    }

    free(c);

    if (!out) {
        if (pool) {
            for (int i = 0; i < poolCount; i++) freeCharacter(pool[i]);
            free(pool);
        }
        for (int i = 0; i < r; i++) freeMatrix(M[i]);
        free(M);
        for (int i = 0; i < r; i++) freeConjugacyClass(classes[i]);
        free(classes);
        return NULL;
    }

    for (int i = 0; i < r; i++) freeMatrix(M[i]);
    free(M);
    free(classes); // ConjugacyClass objects stay alive: each Character holds them via classesCopy

    *count = outCount;
    return out;
}

// Build the character table of G using the class-algebra method. Works for any finite
// group whose class-algebra matrices have well-conditioned eigenvalues.
CharacterTable* characterTable(Group* G) {
    if (!G) return NULL;

    int numIrreps;
    Character** irreps = allIrreducibleCharacters(G, &numIrreps);
    if (!irreps || numIrreps == 0) {
        free(irreps);
        return NULL;
    }
    int r = irreps[0]->numClasses;

    // Build the table's own classes array (pointer copies, the Character objects share
    // the same underlying ConjugacyClass*).
    ConjugacyClass** classes = malloc(r * sizeof(ConjugacyClass*));
    if (!classes) {
        for (int i = 0; i < irreps[0]->numClasses; i++) freeConjugacyClass(irreps[0]->classes[i]);
        for (int k = 0; k < numIrreps; k++) freeCharacter(irreps[k]);
        free(irreps);
        return NULL;
    }
    for (int i = 0; i < r; i++) classes[i] = irreps[0]->classes[i];

    ComplexNumber** values = malloc(numIrreps * sizeof(ComplexNumber*));
    if (!values) {
        free(classes);
        for (int i = 0; i < irreps[0]->numClasses; i++) freeConjugacyClass(irreps[0]->classes[i]);
        for (int k = 0; k < numIrreps; k++) freeCharacter(irreps[k]);
        free(irreps);
        return NULL;
    }
    for (int k = 0; k < numIrreps; k++) {
        values[k] = malloc(r * sizeof(ComplexNumber));
        for (int j = 0; j < r; j++) values[k][j] = irreps[k]->values[j];
    }

    CharacterTable* T = constructCharacterTable(G, classes, irreps, values, r, numIrreps);
    if (!T) {
        for (int k = 0; k < numIrreps; k++) free(values[k]);
        free(values);
        free(classes);
        for (int i = 0; i < irreps[0]->numClasses; i++) freeConjugacyClass(irreps[0]->classes[i]);
        for (int k = 0; k < numIrreps; k++) freeCharacter(irreps[k]);
        free(irreps);
        return NULL;
    }
    return T;
}

/* ---------- Pretty-printing ---------- */

static void formatChiValue(ComplexNumber c, char* out, size_t outSize) {
    double tol = 1e-6;
    double re = c.real;
    double im = c.imag;
    if (fabs(re - round(re)) < tol) re = round(re);
    if (fabs(im - round(im)) < tol) im = round(im);
    if (fabs(re) < tol) re = 0.0;
    if (fabs(im) < tol) im = 0.0;
    if (fabs(im) < tol) {
        snprintf(out, outSize, "%g", re);
    } else if (fabs(re) < tol) {
        snprintf(out, outSize, "%gi", im);
    } else {
        snprintf(out, outSize, "%g%+gi", re, im);
    }
}

static int intTextWidth(int x) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", x);
    return (int)strlen(buf);
}

static void printCenteredCell(const char* text, int width) {
    int len = (int)strlen(text);
    if (len >= width) {
        printf(" %s ", text);
        return;
    }
    int pad = width - len;
    int left = pad / 2;
    int right = pad - left;
    printf(" ");
    for (int i = 0; i < left; i++) putchar(' ');
    printf("%s", text);
    for (int i = 0; i < right; i++) putchar(' ');
    printf(" ");
}

void printCharacterTable(CharacterTable* T) {
    if (!T) { printf("(null character table)\n"); return; }
    int n = T->numClasses;
    int r = T->numIrreps;

    int rowLabelWidth = (int)strlen("irrep");
    for (int k = 0; k < r; k++) {
        const char* lbl = (T->irreps[k] && T->irreps[k]->repr) ? T->irreps[k]->repr : "?";
        int w = (int)strlen(lbl);
        if (w > rowLabelWidth) rowLabelWidth = w;
    }
    if ((int)strlen("class rep") > rowLabelWidth) rowLabelWidth = (int)strlen("class rep");
    if ((int)strlen("class size") > rowLabelWidth) rowLabelWidth = (int)strlen("class size");
    if ((int)strlen("elt order") > rowLabelWidth) rowLabelWidth = (int)strlen("elt order");

    int* colWidths = calloc((size_t)n, sizeof(int));
    if (!colWidths) {
        printf("(failed to allocate print buffer)\n");
        return;
    }
    for (int j = 0; j < n; j++) {
        int width = (int)strlen((T->classes[j] && T->classes[j]->rep && T->classes[j]->rep->repr)
                                    ? T->classes[j]->rep->repr
                                    : "?");
        int sizeW = intTextWidth(T->classes[j]->size);
        int ordW = intTextWidth(T->classes[j]->elementOrder);
        if (sizeW > width) width = sizeW;
        if (ordW > width) width = ordW;
        for (int k = 0; k < r; k++) {
            char val[64];
            formatChiValue(T->values[k][j], val, sizeof(val));
            int vw = (int)strlen(val);
            if (vw > width) width = vw;
        }
        colWidths[j] = width;
    }

    printf("Character table of group (|G| = %d):\n\n", T->group->card);

    printf("%-*s |", rowLabelWidth, "class rep");
    for (int j = 0; j < n; j++) {
        const char* rep = (T->classes[j] && T->classes[j]->rep && T->classes[j]->rep->repr)
            ? T->classes[j]->rep->repr
            : "?";
        printCenteredCell(rep, colWidths[j]);
    }
    printf("\n");

    printf("%-*s |", rowLabelWidth, "class size");
    for (int j = 0; j < n; j++) {
        char num[32];
        snprintf(num, sizeof(num), "%d", T->classes[j]->size);
        printCenteredCell(num, colWidths[j]);
    }
    printf("\n");

    printf("%-*s |", rowLabelWidth, "elt order");
    for (int j = 0; j < n; j++) {
        char num[32];
        snprintf(num, sizeof(num), "%d", T->classes[j]->elementOrder);
        printCenteredCell(num, colWidths[j]);
    }
    printf("\n");

    for (int i = 0; i < rowLabelWidth; i++) putchar('-');
    printf("-+");
    for (int j = 0; j < n; j++) {
        for (int i = 0; i < colWidths[j] + 2; i++) putchar('-');
    }
    printf("\n");

    for (int k = 0; k < r; k++) {
        const char* lbl = T->irreps[k]->repr ? T->irreps[k]->repr : "?";
        printf("%-*s |", rowLabelWidth, lbl);
        for (int j = 0; j < n; j++) {
            char val[64];
            formatChiValue(T->values[k][j], val, sizeof(val));
            printCenteredCell(val, colWidths[j]);
        }
        printf("\n");
    }
    printf("\n");
    free(colWidths);
}

