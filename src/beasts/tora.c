#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<math.h>
#include<limits.h>
#include "hebi.h"
#include "sokko.h"
#include "usagi.h"
#include "tora.h"

/* ---------- Construct and free methods ---------- */

static bool checkedIntMul(int a, int b, int* out) {
    // Reject invalid output storage and negative inputs before doing bounded arithmetic
    if (!out || a < 0 || b < 0) return false;
    // Detect positive integer overflow before multiplying
    if (a != 0 && b > INT_MAX / a) return false;
    // Store the verified product for the caller
    *out = a * b;
    return true;
}

static bool checkedIntAdd(int a, int b, int* out) {
    // Reject invalid output storage before doing bounded arithmetic
    if (!out) return false;
    // Detect signed integer overflow or underflow before adding
    if ((b > 0 && a > INT_MAX - b) || (b < 0 && a < INT_MIN - b)) return false;
    // Store the verified sum for the caller
    *out = a + b;
    return true;
}

static bool checkedSizeMul(size_t a, size_t b, size_t* out) {
    // Reject invalid output storage before doing bounded size arithmetic
    if (!out) return false;
    // Detect size_t overflow before multiplying allocation sizes
    if (a != 0 && b > ((size_t)-1) / a) return false;
    // Store the verified product for the caller
    *out = a * b;
    return true;
}

static bool finiteComplex(ComplexNumber z) {
    // Accept a complex number only when both coordinates are finite
    return isfinite(z.real) && isfinite(z.imag);
}

static bool roundedIntInRange(long double x, int* out) {
    // Reject invalid storage and non-finite input before rounding
    if (!out || !isfinite(x)) return false;
    // Round to the nearest integer-valued long double
    long double r = roundl(x);
    // Make sure the rounded value fits in an int
    if (r < (long double)INT_MIN || r > (long double)INT_MAX) return false;
    // Return the rounded integer through the output parameter
    *out = (int)r;
    return true;
}

// Constructs a ConjugacyClass struct given a representative GroupElement and a Group
ConjugacyClass* constructConjugacyClass(Group* group, GroupElement* rep) {
    // Require both the ambient group and representative element
    if (!group || !rep) return NULL;

    // Allocate the wrapper that records the representative and class metadata
    ConjugacyClass* class = malloc(sizeof(ConjugacyClass));
    if (!class) return NULL;

    // Fill the fields that do not depend on enumerating the class
    class->group = group;
    class->rep = rep;
    class->elementOrder = elementOrder(group, rep);
    class->size = 0;

    // Ask USAGI for the element indices in this representative's conjugacy class
    int* indices = conjugacyClass(rep, &class->size);
    if (!indices) {
        free(class);
        return NULL;
    }
    class->indices = indices;

    // Return the fully populated class object to the caller
    return class;
}

// Free a ConjugacyClass struct
void freeConjugacyClass(ConjugacyClass* class) {
    // Treat NULL as an already-freed class
    if (!class) return;
    // Release the owned index array before releasing the wrapper
    free(class->indices);
    free(class);
}

// Construct a Representation struct given a group, a name, and a list of images of the generators
Representation* constructRepresentation(Group* group, char* repr, Matrix** images, int mdim, int dim) {
    // Require the group, display name, and full image array
    if (!group || !repr || !images) return NULL;

    // Allocate the representation wrapper
    Representation* rep = malloc(sizeof(Representation));
    if (!rep) return NULL;

    // Copy the representation label so the caller can keep or free its buffer
    char* newRepr = malloc(strlen(repr)+1);
    if (!newRepr) {
        free(rep);
        return NULL;
    }
    strcpy(newRepr, repr);

    // Store the metadata and take ownership of the supplied image matrices
    rep->repr = newRepr;
    rep->group = group;
    rep->images = images;
    rep->mdim = mdim;
    rep->dim = dim;

    // Return the assembled representation
    return rep;
}

// Free a Representation struct
void freeRepresentation(Representation* rep) {
    // Treat NULL as an already-freed representation
    if (!rep) return;
    // Release the owned label
    free(rep->repr);
    // Release every owned group image before releasing the image array
    if (rep->images) {
        for (int i = 0; i < rep->group->card; i++) freeMatrix(rep->images[i]);
        free(rep->images);
    }
    // Release the representation wrapper last
    free(rep);
}


// Construct a Character struct given a group, a name, a list of conjugacy classes, and a list of character values on those classes
Character* constructCharacter(Group* group, char* repr, ConjugacyClass** classes, ComplexNumber* values, int numClasses) {
    // Require all external data needed to define the character
    if (!group || !repr || !classes || !values) return NULL;

    // Allocate the character wrapper
    Character* character = malloc(sizeof(Character));
    if (!character) return NULL;

    // Copy the character label so the caller's buffer remains independent
    char* newRepr = malloc(strlen(repr)+1);
    if (!newRepr) {
        free(character);
        return NULL;
    }
    strcpy(newRepr, repr);

    // Store the class pointers and values, both of which are owned at wrapper level
    character->repr = newRepr;
    character->group = group;
    character->classes = classes;
    character->values = values;
    character->numClasses = numClasses;

    // Return the assembled character
    return character;
}

// Free a Character struct
void freeCharacter(Character* character) {
    // Treat NULL as an already-freed character
    if (!character) return;
    // Release the owned label, value array, and wrapper-level class pointer array
    free(character->repr);
    free(character->values);
    free(character->classes);
    // Release the character wrapper last
    free(character);
}

// Construct a CharacterTable struct given a group, a list of conjugacy classes, a list of irreducible characters, and a 2D array of character values
CharacterTable* constructCharacterTable(Group* group, ConjugacyClass** classes, Character** irreps, ComplexNumber** values, int numClasses, int numIrreps) {
    // Require all arrays that form the table
    if (!group || !classes || !irreps || !values) return NULL;

    // Allocate the table wrapper
    CharacterTable* table = malloc(sizeof(CharacterTable));
    if (!table) return NULL;

    // Store the table metadata and take ownership of the supplied arrays
    table->group = group;
    table->classes = classes;
    table->irreps = irreps;
    table->values = values;
    table->numClasses = numClasses;
    table->numIrreps = numIrreps;

    // Return the assembled table
    return table;
}

// Free a CharacterTable struct
void freeCharacterTable(CharacterTable* table) {
    // Treat NULL as an already-freed table
    if (!table) return;
    // Release the wrapper-level class and character pointer arrays
    free(table->classes);
    free(table->irreps);
    // Release the owned rectangular value storage
    if (table->values) {
        for (int i = 0; i < table->numIrreps; i++) free(table->values[i]);
        free(table->values);
    }
    // Release the table wrapper last
    free(table);
}


/* ---------- ConjugacyClass helpers ---------- */

// Gets the ConjugacyClass of a GroupElement
ConjugacyClass* getConjugacyClass(GroupElement* element) {
    // Require a representative element
    if (!element) return NULL;

    // Delegate construction to the shared class constructor
    return constructConjugacyClass(element->group, element);
}

// Gets the ConjugacyClasses of a Group; modifies numClasses to be the number of classes
ConjugacyClass** getConjugacyClasses(Group* group, int* numClasses) {
    // Require the group and output count slot
    if (!group || !numClasses) return NULL;

    // Allocate the largest possible class list, one class per element
    ConjugacyClass** classes = malloc(group->card * sizeof(ConjugacyClass*));
    if (!classes) return NULL;

    // Scan group elements and add a new class only when its representative is unseen
    int count = 0;
    for (int i = 0; i < group->card; i++) {
        GroupElement* rep = group->elements[i];
        bool isNewClass = true;
        // Compare the candidate representative against every class already collected
        for (int j = 0; j < count; j++) {
            for (int k = 0; k < classes[j]->size; k++) {
                if (rep->index == classes[j]->indices[k]) {
                    isNewClass = false;
                    break;
                }
            }
            if (!isNewClass) break;
        }
        // Materialize a conjugacy class when no earlier class contains this representative
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

    // Return the number of classes actually found and the owned class array
    *numClasses = count;
    return classes;
}

/* ---------- Representations ---------- */

// Construct the trivial representation of a group
Representation* trivialRepresentation(Group* G) {
    // Require a group before constructing images
    if (!G) return NULL;

    // Allocate one 1x1 image matrix for every group element
    Matrix** images = malloc(G->card * sizeof(Matrix*));
    if (!images) return NULL;
    // Fill every group element with the scalar identity matrix
    for (int i = 0; i < G->card; i++) {
        images[i] = constructMatrix(1, 1);
        if (!images[i]) {
            for (int j = 0; j < i; j++) freeMatrix(images[j]);
            free(images);
            return NULL;
        }
        setEntry(images[i], 0, 0, elemFromReal(1.0));
    }

    // Wrap the image matrices as the named trivial representation
    Representation* rep = constructRepresentation(G, "C1", images, 1, 1);
    if (!rep) {
        for (int i = 0; i < G->card; i++) freeMatrix(images[i]);
        free(images);
        return NULL;
    }
    // Return the completed representation
    return rep;
}

// Construct the regular representation of a Group
Representation* regularRepresentation(Group* G) {
    // Require a group before constructing permutation matrices
    if (!G) return NULL;

    // Use one basis vector for each group element
    int dim = G->card;

    // Allocate one permutation matrix image for each group element
    Matrix** images = malloc(G->card * sizeof(Matrix*));
    if (!images) return NULL;

    // Encode left multiplication by each group element on the basis of G
    for (int i = 0; i < G->card; i++) {
        images[i] = constructMatrix(dim, dim);
        if (!images[i]) {
            for (int j = 0; j < i; j++) freeMatrix(images[j]);
            free(images);
            return NULL;
        }
        for (int j = 0; j < G->card; j++) {
            GroupElement* product = groupMult(G->elements[i], G->elements[j]);
            if (!product || product->index < 0) {
                for (int k = 0; k <= i; k++) freeMatrix(images[k]);
                free(images);
                return NULL;
            }
            setEntry(images[i], product->index, j, elemFromReal(1.0));
        }
    }

    // Wrap the permutation matrices as the regular representation
    Representation* rep = constructRepresentation(G, "R", images, dim, dim);
    if (!rep) {
        for (int i = 0; i < G->card; i++) freeMatrix(images[i]);
        free(images);
        return NULL;
    }
    // Return the completed representation
    return rep;
}

// Computes the permutation representation of a Group, i.e. the reduced permutation representation
Representation* permutationRepresentation(Group* G) {
    // Require a group before looking for a coset action
    if (!G) return NULL;

    // Use a largest core-free subgroup so the coset action is faithful when possible
    SubGroup* H = largestCoreFreeSubgroup(G);
    if (!H) return regularRepresentation(G);

    // If H is the whole group, the permutation rep is the trivial rep
    if (H->card == G->card) {
        freeSubgroup(H);
        return trivialRepresentation(G);
    }

    // The representation dimension is the number of left cosets
    int dim = subgroupIndex(H);

    // Enumerate the cosets that form the basis of the permutation action
    GroupCoset** cosets = getLeftGroupCosets(H);
    if (!cosets) {
        freeSubgroup(H);
        return NULL;
    }

    // Allocate one permutation matrix image for each group element
    Matrix** images = malloc(G->card * sizeof(Matrix*));
    if (!images) {
        for (int j = 0; j < dim; j++) freeGroupCoset(cosets[j]);
        free(cosets);
        freeSubgroup(H);
        return NULL;
    }

    // For each group element, record how left multiplication permutes the cosets
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
            GroupElement* product = groupMult(G->elements[i], G->elements[cosets[j]->data.indexed.indices[0]]);
            if (!product || product->index < 0) {
                for (int k = 0; k <= i; k++) freeMatrix(images[k]);
                free(images);
                for (int k = 0; k < dim; k++) freeGroupCoset(cosets[k]);
                free(cosets);
                freeSubgroup(H);
                return NULL;
            }
            int prodIndex = product->index;
            for (int k = 0; k < dim; k++) {
                bool inCoset = false;
                for (int m = 0; m < H->card; m++) {
                    if (prodIndex == cosets[k]->data.indexed.indices[m]) { inCoset = true; break; }
                }
                if (inCoset) {
                    setEntry(images[i], k, j, elemFromReal(1.0));
                    break;
                }
            }
        }
    }

    // Wrap the coset permutation matrices as a representation
    Representation* rep = constructRepresentation(G, "P", images, dim, dim);
    if (!rep) {
        for (int i = 0; i < G->card; i++) freeMatrix(images[i]);
        free(images);
        for (int j = 0; j < dim; j++) freeGroupCoset(cosets[j]);
        free(cosets);
        freeSubgroup(H);
        return NULL;
    }

    // Release temporary coset data once the images have been built
    for (int j = 0; j < dim; j++) freeGroupCoset(cosets[j]);
    free(cosets);
    freeSubgroup(H);
    // Return the completed representation
    return rep;
}

// Gets the standard representation of a Group, i.e. the reduced permutation representation on the nontrivial conjugacy classes
Representation* standardRepresentation(Group* G) {
    // Require a group before reducing its permutation representation
    if (!G) return NULL;

    // Start from the permutation representation and remove the trivial line
    Representation* perm = permutationRepresentation(G);
    if (!perm) return NULL;
    int n = perm->dim;
    if (n <= 1) return perm; // trivial group, nothing to reduce

    // Allocate images on the quotient basis of dimension n - 1
    int sdim = n - 1;
    Matrix** images = malloc(G->card * sizeof(Matrix*));
    if (!images) {
        freeRepresentation(perm);
        return NULL;
    }

    // Express each permutation matrix on the standard basis e_a - e_0
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
                FieldElement ak = getEntry(perm->images[g], a, k);
                FieldElement a0 = getEntry(perm->images[g], a, 0);
                FieldElement val = elemSub(ak, a0);
                if (!elemIsZero(val, 1e-12))
                    setEntry(images[g], a-1, k-1, val);
            }
        }
    }

    // Wrap the reduced images as the standard representation
    Representation* rep = constructRepresentation(G, "S", images, sdim, sdim);
    if (!rep) {
        for (int i = 0; i < G->card; i++) freeMatrix(images[i]);
        free(images);
        freeRepresentation(perm);
        return NULL;
    }
    // Release the temporary permutation representation before returning
    freeRepresentation(perm);
    return rep;
}

// Project a Group G onto its abelianization Q = G/[G,G] as a GroupHomomorphism
// The codomain Q is a freshly-constructed Group; the caller is responsible for
// freeing both the homomorphism (via freeGroupHomomorphism) and the codomain
// (via freeGroup) when done
GroupHomomorphism* projectToAbelianization(Group* G) {
    // Require a group before computing its commutator quotient
    if (!G) return NULL;

    // Compute the commutator subgroup that becomes the kernel
    SubGroup* commutator = commutatorSubgroup(G);
    if (!commutator) return NULL;

    // Enumerate cosets of the commutator subgroup to label quotient elements
    GroupCoset** cosets = getLeftGroupCosets(commutator);
    if (!cosets) { freeSubgroup(commutator); return NULL; }
    int n = G->card / commutator->card;

    // Construct the quotient group that will be the homomorphism codomain
    Group* Q = quotientGroup(G, commutator);
    if (!Q) {
        for (int j = 0; j < n; j++) freeGroupCoset(cosets[j]);
        free(cosets);
        freeSubgroup(commutator);
        return NULL;
    }

    // Build the map from each ambient element to its coset index
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
            mapping[cosets[c]->data.indexed.indices[m]] = c;
        }
    }
    // Release temporary coset and subgroup data after extracting the quotient map
    for (int j = 0; j < n; j++) freeGroupCoset(cosets[j]);
    free(cosets);
    freeSubgroup(commutator);

    // Wrap the quotient map as a group homomorphism
    GroupHomomorphism* hom = constructGroupHomomorphism(G, Q, mapping, G->card);
    if (!hom) {
        free(mapping);
        freeGroup(Q);
        return NULL;
    }
    // Return the homomorphism, leaving ownership of Q with the caller via hom->codomain
    return hom;
}

// The 1D sign representation of the symmetric group S_n. Element i (in lex order
// matching constructSymmetricGroup) maps to (-1)^{permSign}
Representation* signRepresentation(Group* Sn) {
    // Require a candidate symmetric group
    if (!Sn) return NULL;

    // Recover n from |Sn| = n!
    int n = 1;
    int f = 1;
    while (f < Sn->card) { n++; f *= n; }
    if (f != Sn->card) return NULL;
    if (n == 1) return trivialRepresentation(Sn);

    // Initialize the first lexicographic permutation
    int* perm = malloc(n * sizeof(int));
    if (!perm) return NULL;
    for (int i = 0; i < n; i++) perm[i] = i;

    // Allocate one scalar image for each permutation
    Matrix** images = malloc(Sn->card * sizeof(Matrix*));
    if (!images) { free(perm); return NULL; }

    // Walk lexicographic permutations and assign +1 or -1 by parity
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
    // Release the permutation cursor after all images are built
    free(perm);

    // Wrap the scalar images as the sign representation
    Representation* rep = constructRepresentation(Sn, "Sgn", images, 1, 1);
    if (!rep) {
        for (int i = 0; i < Sn->card; i++) freeMatrix(images[i]);
        free(images);
        return NULL;
    }
    // Return the completed representation
    return rep;
}

// Dual representation V*: rho*(g) = (rho(g^{-1}))^T. Acts on V*
Representation* dualRepresentation(Representation* V) {
    // Require a representation and its ambient group
    if (!V) return NULL;
    Group* G = V->group;
    if (!G) return NULL;

    // Allocate one dual image matrix for each group element
    Matrix** images = malloc(G->card * sizeof(Matrix*));
    if (!images) return NULL;

    // For each element, find the inverse and transpose its original image
    for (int g = 0; g < G->card; g++) {
        // Find inverse index of g
        GroupElement* inverse = groupInverse(G->elements[g]);
        if (!inverse || inverse->index < 0) {
            for (int j = 0; j < g; j++) freeMatrix(images[j]);
            free(images);
            return NULL;
        }
        images[g] = transpose(V->images[inverse->index]);
        if (!images[g]) {
            for (int j = 0; j < g; j++) freeMatrix(images[j]);
            free(images);
            return NULL;
        }
    }

    // Wrap the transposed inverse images as the dual representation
    Representation* rep = constructRepresentation(G, "Dual", images, V->mdim, V->dim);
    if (!rep) {
        for (int i = 0; i < G->card; i++) freeMatrix(images[i]);
        free(images);
        return NULL;
    }
    // Return the completed representation
    return rep;
}

// Complex-conjugate representation: bar-rho(g) is rho(g) with each entry
// complex-conjugated (real entries unchanged)
Representation* conjugateRepresentation(Representation* V) {
    // Require a representation and its ambient group
    if (!V) return NULL;
    Group* G = V->group;
    if (!G) return NULL;

    // Allocate one conjugated image matrix for each group element
    Matrix** images = malloc(G->card * sizeof(Matrix*));
    if (!images) return NULL;

    // Copy every image while replacing entries by their complex conjugates
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
                FieldElement e = getEntry(A, i, j);
                if (!elemIsZero(e, 1e-15))
                    setEntry(M, i, j, elemConj(e));
            }
        }
        images[g] = M;
    }

    // Wrap the conjugated images as a representation
    Representation* rep = constructRepresentation(G, "Conj", images, V->mdim, V->dim);
    if (!rep) {
        for (int i = 0; i < G->card; i++) freeMatrix(images[i]);
        free(images);
        return NULL;
    }
    // Return the completed representation
    return rep;
}

/* ---------- Helpers for symmetric and wedge product indexing ---------- */

// For Sym^2 with i <= j: linear index k = i*n - i*(i-1)/2 + (j - i)
static int symIdx(int i, int j, int n) {
    // Normalize the unordered symmetric pair before indexing it
    if (i > j) { int t = i; i = j; j = t; }
    // Return the packed upper-triangular index for the pair
    return i * n - (i * (i - 1)) / 2 + (j - i);
}

// For Lambda^2 with i < j: linear index k = i*n - i*(i+1)/2 + (j - i - 1)
static int wedgeIdx(int i, int j, int n) {
    // Return the packed strictly upper-triangular index for the ordered pair
    return i * n - (i * (i + 1)) / 2 + (j - i - 1);
}

/* ---------- Helpers for get1Dreps ---------- */

// Propagate a partial 1D-character assignment via the multiplication table
// Returns -1 on contradiction, 0 otherwise. Modifies vals and set in place
static int propagateCharacter(Group* G, long double tol, ComplexNumber* vals, bool* set, int seed) {
    // Allocate a queue of newly assigned elements whose products must be propagated
    int* queue = malloc(G->card * sizeof(int));
    if (!queue) return -1;
    // Seed propagation with the element that just received a value
    int qLen = 0, qHead = 0;
    queue[qLen++] = seed;

    // Process each newly known element against every previously known value
    while (qHead < qLen) {
        int x = queue[qHead++];
        for (int y = 0; y < G->card; y++) {
            if (!set[y]) continue;
            // Compute the forced character values for xy and yx
            ComplexNumber pxy = complexMul(vals[x], vals[y]);
            ComplexNumber pyx = complexMul(vals[y], vals[x]);
            GroupElement* xy = groupMult(G->elements[x], G->elements[y]);
            GroupElement* yx = groupMult(G->elements[y], G->elements[x]);
            if (!xy || !yx || xy->index < 0 || yx->index < 0) {
                free(queue); return -1;
            }
            int zxy = xy->index;
            int zyx = yx->index;
            // Assign chi(xy) if unset, or reject the branch if it disagrees
            if (!set[zxy]) {
                set[zxy] = true; vals[zxy] = pxy;
                queue[qLen++] = zxy;
            } else if (!complexEq(vals[zxy], pxy, tol)) {
                free(queue); return -1;
            }
            // Assign chi(yx) if unset, or reject the branch if it disagrees
            if (!set[zyx]) {
                set[zyx] = true; vals[zyx] = pyx;
                queue[qLen++] = zyx;
            } else if (!complexEq(vals[zyx], pyx, tol)) {
                free(queue); return -1;
            }
        }
    }
    // Release the propagation queue after all forced values are checked
    free(queue);
    return 0;
}

// Build a 1D Representation from a complete value array on G
static Representation* build1Drep(Group* G, ComplexNumber* vals, int idx) {
    // Allocate one scalar matrix image for each group element
    Matrix** images = malloc(G->card * sizeof(Matrix*));
    if (!images) return NULL;
    // Store each assigned character value as the sole entry of a 1x1 image matrix
    for (int i = 0; i < G->card; i++) {
        images[i] = constructMatrix(1, 1);
        if (!images[i]) {
            for (int j = 0; j < i; j++) freeMatrix(images[j]);
            free(images);
            return NULL;
        }
        setEntry(images[i], 0, 0, elemFromComplex(vals[i]));
    }
    // Create a stable label for this generated linear representation
    char repr[32];
    snprintf(repr, sizeof(repr), "1D-%d", idx);
    // Wrap the scalar image matrices as a representation
    Representation* rep = constructRepresentation(G, repr, images, 1, 1);
    if (!rep) {
        for (int i = 0; i < G->card; i++) freeMatrix(images[i]);
        free(images);
        return NULL;
    }
    // Return the completed 1D representation
    return rep;
}

static void backtrack1D(Group* G, long double tol, ComplexNumber* vals, bool* set,
                        Representation*** out, int* count, int* cap) {
    // Choose the first group element whose character value is not assigned
    int i = -1;
    for (int j = 0; j < G->card; j++) if (!set[j]) { i = j; break; }

    // If every value is assigned, append the completed 1D representation
    if (i < 0) {
        Representation* rep = build1Drep(G, vals, *count);
        if (!rep) return;
        // Grow the output array when the number of results reaches capacity
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

    // The image of an element must be a root of unity whose order divides its order
    int o = elementOrder(G, G->elements[i]);
    if (o <= 0) return;

    // Allocate snapshots so each root-of-unity branch can be rolled back
    ComplexNumber* savedVals = malloc(G->card * sizeof(ComplexNumber));
    bool* savedSet = malloc(G->card * sizeof(bool));
    if (!savedVals || !savedSet) { free(savedVals); free(savedSet); return; }

    // Try every o-th root of unity as the value of the next unassigned element
    long double TAU = 2.0L * pi();
    for (int k = 0; k < o; k++) {
        // Save the current partial assignment before exploring this branch
        memcpy(savedVals, vals, G->card * sizeof(ComplexNumber));
        memcpy(savedSet, set, G->card * sizeof(bool));

        // Compute the candidate root of unity for this branch
        ComplexNumber omega;
        omega.real = (long double) cosl(TAU * (long double) k / (long double) o);
        omega.imag = (long double) sinl(TAU * (long double) k / (long double) o);

        // Assign the candidate value before propagating multiplication constraints
        vals[i] = omega;
        set[i] = true;

        // Continue recursively only if propagation finds no contradiction
        if (propagateCharacter(G, tol, vals, set, i) == 0) {
            backtrack1D(G, tol, vals, set, out, count, cap);
        }

        // Restore the incoming partial assignment before trying the next root
        memcpy(vals, savedVals, G->card * sizeof(ComplexNumber));
        memcpy(set, savedSet, G->card * sizeof(bool));
    }
    // Release snapshots after all branches have been explored
    free(savedVals);
    free(savedSet);
}

// Returns an array containing all 1D (linear) representations of G
// Sets *count to the number of reps; caller frees each via freeRepresentation
// and frees the array. The number returned equals |G/[G,G]|
Representation** get1Dreps(Group* G, int* count) {
    // Require the group and output count slot
    if (!G || !count) return NULL;
    // Start with no generated representations
    *count = 0;

    // Allocate a growable output array for generated representations
    int cap = 4;
    Representation** out = malloc(cap * sizeof(Representation*));
    if (!out) return NULL;

    // Track the partially assigned scalar character values during backtracking
    ComplexNumber* vals = malloc(G->card * sizeof(ComplexNumber));
    bool* set = calloc(G->card, sizeof(bool));
    if (!vals || !set) { free(out); free(vals); free(set); return NULL; }

    // Identity always maps to 1
    vals[0].real = 1.0; vals[0].imag = 0.0;
    set[0] = true;

    // Enumerate every consistent assignment of roots of unity to group elements
    backtrack1D(G, 1e-9, vals, set, &out, count, &cap);

    // Release the backtracking workspace and return the generated array
    free(vals);
    free(set);
    return out;
}

// External direct-product (cross) representation V (><) W of G x H, where GxH must be
// the direct product Group whose elements are indexed as (i * H->card + j). The image
// at index (i*|H|+j) is the Kronecker product rho_V(g_i) (x) rho_W(h_j)
Representation* crossProductReps(Representation* V, Representation* W, Group* GxH) {
    // Require both representations and the explicit product group
    if (!V || !W || !GxH) return NULL;
    // Extract the two source groups and verify they exist
    Group* G = V->group;
    Group* H = W->group;
    if (!G || !H) return NULL;
    // Verify the supplied product group has the expected cardinality
    int productCard;
    if (!checkedIntMul(G->card, H->card, &productCard) || GxH->card != productCard) return NULL;

    // Compute the tensor-product dimension with overflow checking
    int dim;
    if (!checkedIntMul(V->dim, W->dim, &dim)) return NULL;
    // Allocate one image for each element of the product group
    Matrix** images = malloc(GxH->card * sizeof(Matrix*));
    if (!images) return NULL;

    // Build images in product indexing order using Kronecker products
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

    // Wrap the Kronecker images as a representation of the product group
    Representation* rep = constructRepresentation(GxH, "Vx", images, dim, dim);
    if (!rep) {
        for (int i = 0; i < GxH->card; i++) freeMatrix(images[i]);
        free(images);
        return NULL;
    }
    // Return the completed external product representation
    return rep;
}

// Internal tensor product V (x) W of two representations of the SAME group G
// rho_{V (x) W}(g) = rho_V(g) (x) rho_W(g)
Representation* tensorProduct(Representation* V, Representation* W) {
    // Require two representations of isomorphic stored groups
    if (!V || !W) return NULL;
    if (!cmpGroups(V->group, W->group)) return NULL;
    // Use the first representation's group as the ambient group
    Group* G = V->group;

    // Compute the tensor-product dimension with overflow checking
    int dim;
    if (!checkedIntMul(V->dim, W->dim, &dim)) return NULL;
    // Allocate one image for each group element
    Matrix** images = malloc(G->card * sizeof(Matrix*));
    if (!images) return NULL;

    // Tensor the two images at each group element
    for (int i = 0; i < G->card; i++) {
        images[i] = tensorMatrices(V->images[i], W->images[i]);
        if (!images[i]) {
            for (int j = 0; j < i; j++) freeMatrix(images[j]);
            free(images);
            return NULL;
        }
    }

    // Wrap the tensor images as a representation
    Representation* rep = constructRepresentation(G, "VoW", images, dim, dim);
    if (!rep) {
        for (int i = 0; i < G->card; i++) freeMatrix(images[i]);
        free(images);
        return NULL;
    }
    // Return the completed internal tensor product
    return rep;
}

// Sym^2 V on basis {e_i e_j : i <= j}, with action g.(e_i e_j) = (g.e_i)(g.e_j)
// Coefficient of e_a e_b (a <= b) in g.(e_i e_j) is:
//   A[a][i] * A[a][j]                            if a == b
//   A[a][i] * A[b][j] + A[b][i] * A[a][j]        if a < b
// where A is the matrix of rho(g)
Representation* symmetricProduct(Representation* V) {
    // Require a representation before constructing its symmetric square
    if (!V) return NULL;
    // Extract the ambient group and source dimension
    Group* G = V->group;
    int n = V->dim;
    // Compute dim Sym^2(V) = n(n + 1)/2 with overflow checking
    int nPlusOne;
    int twiceSdim;
    if (!checkedIntAdd(n, 1, &nPlusOne) || !checkedIntMul(n, nPlusOne, &twiceSdim)) return NULL;
    int sdim = twiceSdim / 2;
    if (sdim < 1) return NULL;

    // Allocate one symmetric-square image for each group element
    Matrix** images = malloc(G->card * sizeof(Matrix*));
    if (!images) return NULL;

    // Build each image by expanding the action on symmetric basis pairs
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
                        FieldElement Aai = getEntry(A, a, i);
                        FieldElement Aaj = getEntry(A, a, j);
                        FieldElement Abi = getEntry(A, b, i);
                        FieldElement Abj = getEntry(A, b, j);
                        FieldElement val;
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

    // Wrap the symmetric-square images as a representation
    Representation* rep = constructRepresentation(G, "Sym2", images, sdim, sdim);
    if (!rep) {
        for (int i = 0; i < G->card; i++) freeMatrix(images[i]);
        free(images);
        return NULL;
    }
    // Return the completed symmetric square
    return rep;
}

// Lambda^2 V on basis {e_i ^ e_j : i < j}, with action g.(e_i ^ e_j) = (g.e_i) ^ (g.e_j)
// Coefficient of e_a ^ e_b (a < b) in g.(e_i ^ e_j) is A[a][i]*A[b][j] - A[b][i]*A[a][j]
Representation* wedgeProduct(Representation* V) {
    // Require a representation before constructing its exterior square
    if (!V) return NULL;
    // Extract the ambient group and source dimension
    Group* G = V->group;
    int n = V->dim;
    // Compute dim Lambda^2(V) = n(n - 1)/2 with overflow checking
    int nMinusOne;
    int twiceWdim;
    if (!checkedIntAdd(n, -1, &nMinusOne) || !checkedIntMul(n, nMinusOne, &twiceWdim)) return NULL;
    int wdim = twiceWdim / 2;
    if (wdim < 1) return NULL;

    // Allocate one exterior-square image for each group element
    Matrix** images = malloc(G->card * sizeof(Matrix*));
    if (!images) return NULL;

    // Build each image by expanding the action on wedge basis pairs
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
                        FieldElement Aai = getEntry(A, a, i);
                        FieldElement Aaj = getEntry(A, a, j);
                        FieldElement Abi = getEntry(A, b, i);
                        FieldElement Abj = getEntry(A, b, j);
                        FieldElement val = elemSub(elemMul(Aai, Abj), elemMul(Abi, Aaj));
                        if (!elemIsZero(val, 1e-15))
                            setEntry(M, row, col, val);
                    }
                }
            }
        }
        images[g] = M;
    }

    // Wrap the exterior-square images as a representation
    Representation* rep = constructRepresentation(G, "Wedge2", images, wdim, wdim);
    if (!rep) {
        for (int i = 0; i < G->card; i++) freeMatrix(images[i]);
        free(images);
        return NULL;
    }
    // Return the completed exterior square
    return rep;
}

// Restrict a representation V of an ambient group G to a subgroup H realised as Hgroup
// The caller supplies Hgroup, a Group whose i-th element corresponds to G->elements[H->data.indexed.indices[i]]
// (Such a Group can be built independently from H)
Representation* restrictRepresentation(Representation* V, SubGroup* H, Group* Hgroup) {
    // Require the representation, subgroup data, and realised subgroup group
    if (!V || !H || !Hgroup) return NULL;
    // Ensure the representation is defined on the subgroup's ambient group
    if (V->group != H->ambient) return NULL;
    // Ensure the realised subgroup has the same cardinality as H
    if (Hgroup->card != H->card) return NULL;

    // Allocate one restricted image for each subgroup element
    Matrix** images = malloc(H->card * sizeof(Matrix*));
    if (!images) return NULL;

    // Copy the ambient image at each subgroup element index
    for (int i = 0; i < H->card; i++) {
        images[i] = copyMatrix(V->images[H->data.indexed.indices[i]]);
        if (!images[i]) {
            for (int j = 0; j < i; j++) freeMatrix(images[j]);
            free(images);
            return NULL;
        }
    }

    // Wrap the copied images as a representation of Hgroup
    Representation* rep = constructRepresentation(Hgroup, "Res", images, V->mdim, V->dim);
    if (!rep) {
        for (int i = 0; i < H->card; i++) freeMatrix(images[i]);
        free(images);
        return NULL;
    }
    // Return the completed restriction
    return rep;
}

// Induce a representation V from a subgroup H (realised so that V->group's i-th element
// matches H->ambient->elements[H->data.indexed.indices[i]]) up to the ambient group G = H->ambient
// Result has dimension [G:H] * dim(V). For coset reps t_0, ..., t_{n-1}, the action of
// h on basis (t_a, v) is: choose b such that h * t_a in t_b * H, write h*t_a = t_b * k
// with k in H, then h.(t_a, v) = (t_b, rho_V(k) v). The block at (b, a) is rho_V(k);
// all other blocks are zero
Representation* inducedRepresentation(Representation* V, SubGroup* H) {
    // Require a subgroup representation and subgroup metadata
    if (!V || !H) return NULL;
    // Extract the ambient group and verify it exists
    Group* G = H->ambient;
    if (!G) return NULL;
    // Ensure V is realised on a group with one element per subgroup element
    if (V->group->card != H->card) return NULL;

    // Enumerate left cosets and compute the induced dimension
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
    // Store each coset representative and its inverse in the ambient group
    for (int a = 0; a < n; a++) {
        tIdx[a] = cosets[a]->data.indexed.indices[0];
        GroupElement* inverse = groupInverse(G->elements[tIdx[a]]);
        if (!inverse || inverse->index < 0) {
            free(tIdx); free(tInv);
            for (int j = 0; j < n; j++) freeGroupCoset(cosets[j]);
            free(cosets);
            return NULL;
        }
        tInv[a] = inverse->index;
    }

    // Map ambient-group index -> position in H->data.indexed.indices (or -1 if not in H)
    int* posInH = malloc(G->card * sizeof(int));
    if (!posInH) {
        free(tIdx); free(tInv);
        for (int j = 0; j < n; j++) freeGroupCoset(cosets[j]);
        free(cosets);
        return NULL;
    }
    for (int i = 0; i < G->card; i++) posInH[i] = -1;
    // Fill the lookup that converts ambient subgroup indices into V-image indices
    for (int i = 0; i < H->card; i++) posInH[H->data.indexed.indices[i]] = i;

    // Allocate one induced image for every ambient group element
    Matrix** images = malloc(G->card * sizeof(Matrix*));
    if (!images) {
        free(tIdx); free(tInv); free(posInH);
        for (int j = 0; j < n; j++) freeGroupCoset(cosets[j]);
        free(cosets);
        return NULL;
    }

    // Build the block matrix for the action of each ambient group element
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
            // Compute h * t_a and find which coset contains it
            GroupElement* htaElement = groupMult(G->elements[h], G->elements[tIdx[a]]);
            if (!htaElement || htaElement->index < 0) {
                freeMatrix(M);
                for (int j = 0; j < h; j++) freeMatrix(images[j]);
                free(images);
                free(tIdx); free(tInv); free(posInH);
                for (int j = 0; j < n; j++) freeGroupCoset(cosets[j]);
                free(cosets);
                return NULL;
            }
            int hta = htaElement->index;
            // Find coset b containing hta
            int b = -1;
            for (int bb = 0; bb < n; bb++) {
                for (int m = 0; m < H->card; m++) {
                    if (cosets[bb]->data.indexed.indices[m] == hta) { b = bb; break; }
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
            GroupElement* kElement = groupMult(G->elements[tInv[b]], G->elements[hta]);
            if (!kElement || kElement->index < 0) {
                freeMatrix(M);
                for (int j = 0; j < h; j++) freeMatrix(images[j]);
                free(images);
                free(tIdx); free(tInv); free(posInH);
                for (int j = 0; j < n; j++) freeGroupCoset(cosets[j]);
                free(cosets);
                return NULL;
            }
            int kAmbient = kElement->index;
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
                    FieldElement v = getEntry(B, r, c);
                    if (!elemIsZero(v, 1e-15))
                        setEntry(M, b * dV + r, a * dV + c, v);
                }
            }
        }
        images[h] = M;
    }

    // Wrap the block matrices as an induced representation
    Representation* rep = constructRepresentation(G, "Ind", images, dim, dim);
    // Release induction indexing workspace after the representation is attempted
    free(tIdx); free(tInv); free(posInH);
    for (int j = 0; j < n; j++) freeGroupCoset(cosets[j]);
    free(cosets);
    if (!rep) {
        for (int i = 0; i < G->card; i++) freeMatrix(images[i]);
        free(images);
        return NULL;
    }
    // Return the completed induced representation
    return rep;
}

/* ---------- Characters and character tables ---------- */

// Compute the character value of a representation rep on a conjugacy class class
// This is the trace of rep->images[g] for any g in class (all elements have the same trace)
ComplexNumber characterValue(Representation* rep, ConjugacyClass* class) {
    // Reject missing inputs by returning the zero complex value
    if (!rep || !class) return (ComplexNumber){0.0, 0.0};
    // Require the representation and class to belong to the same group
    Group* G = rep->group;
    if (!G || G != class->group) return (ComplexNumber){0.0, 0.0};
    // Evaluate the character as the trace on the class representative
    return elemToComplex(trace(rep->images[class->rep->index]));
}

// Build a Character struct from a Representation
// The returned Character holds a freshly-allocated classes array and values; per the
// codebase's freeing convention the caller must walk chi->classes and freeConjugacyClass
// each entry before calling freeCharacter(chi)
Character* characterOfRepresentation(Representation* rep) {
    // Require a representation with an ambient group
    if (!rep || !rep->group) return NULL;

    // Enumerate the conjugacy classes where the character is constant
    int n;
    ConjugacyClass** classes = getConjugacyClasses(rep->group, &n);
    if (!classes) return NULL;

    // Allocate one value for each conjugacy class
    ComplexNumber* values = malloc(n * sizeof(ComplexNumber));
    if (!values) {
        for (int i = 0; i < n; i++) freeConjugacyClass(classes[i]);
        free(classes);
        return NULL;
    }
    // Fill the character value on each class by tracing a representative image
    for (int i = 0; i < n; i++) values[i] = characterValue(rep, classes[i]);

    // Wrap the class list and value list as a Character object
    Character* chi = constructCharacter(rep->group, rep->repr, classes, values, n);
    if (!chi) {
        for (int i = 0; i < n; i++) freeConjugacyClass(classes[i]);
        free(classes);
        free(values);
        return NULL;
    }
    // Return the completed character
    return chi;
}

// Standard inner product of two characters of the same group:
//   <chi, psi> = (1/|G|) * sum_C |C| * chi(C) * conj(psi(C))
// The two characters may have classes arranged in different orders; matching is by
// looking up each class representative's index in the other side's class lists
ComplexNumber characterInnerProduct(Character* chi, Character* psi) {
    // Reject missing characters by returning zero
    if (!chi || !psi) return (ComplexNumber){0.0, 0.0};
    // Inner products are defined only for characters of the same group
    if (chi->group != psi->group) return (ComplexNumber){0.0, 0.0};
    // Use the common group order for normalization
    Group* G = chi->group;

    // Accumulate the weighted class sum for the standard character inner product
    ComplexNumber sum = {0.0, 0.0};
    for (int i = 0; i < chi->numClasses; i++) {
        ConjugacyClass* C = chi->classes[i];
        if (!C) return (ComplexNumber){0.0, 0.0};

        // Find the class in psi that contains C's representative
        ComplexNumber psiVal = {0.0, 0.0};
        bool matched = false;
        int repIdx = C->rep->index;
        // Match classes by membership rather than by array position
        for (int j = 0; j < psi->numClasses; j++) {
            ConjugacyClass* D = psi->classes[j];
            for (int k = 0; k < D->size; k++) {
                if (D->indices[k] == repIdx) { psiVal = psi->values[j]; matched = true; break; }
            }
            if (matched) break;
        }
        if (!matched) return (ComplexNumber){0.0, 0.0};

        // Add |C| chi(C) conjugate(psi(C)) to the running sum
        ComplexNumber term = complexMul(chi->values[i], complexConj(psiVal));
        term.real *= (long double) C->size;
        term.imag *= (long double) C->size;
        sum = complexAdd(sum, term);
    }

    // Divide by |G| to finish the normalized inner product
    sum.real /= (long double) G->card;
    sum.imag /= (long double) G->card;
    return sum;
}

// A representation is irreducible iff <chi, chi> = 1
bool isIrreducible(Representation* rep) {
    // Reject missing representations
    if (!rep) return false;
    // Compute the character attached to the representation
    Character* chi = characterOfRepresentation(rep);
    if (!chi) return false;

    // Apply the character inner-product criterion for irreducibility
    ComplexNumber inner = characterInnerProduct(chi, chi);
    bool irr = fabsl(inner.real - 1.0) < 1e-6 && fabsl(inner.imag) < 1e-6;

    // Release the temporary character and its owned conjugacy classes
    for (int i = 0; i < chi->numClasses; i++) freeConjugacyClass(chi->classes[i]);
    freeCharacter(chi);
    return irr;
}

// Decompose a representation V of G into irreducibles given a CharacterTable T of G
// Returns a freshly-allocated int array of length T->numIrreps where entry i is the
// multiplicity of T->irreps[i] in V (V = sum_i n_i V_i). Caller frees with free()
int* decomposeRepresentation(Representation* V, CharacterTable* T) {
    // Require a representation and a character table
    if (!V || !T) return NULL;
    // Decomposition uses a table for the same group as the representation
    if (V->group != T->group) return NULL;

    // Compute the character of the representation being decomposed
    Character* chi = characterOfRepresentation(V);
    if (!chi) return NULL;

    // Allocate the output multiplicity vector
    int* mults = malloc(T->numIrreps * sizeof(int));
    if (!mults) {
        for (int i = 0; i < chi->numClasses; i++) freeConjugacyClass(chi->classes[i]);
        freeCharacter(chi);
        return NULL;
    }
    // Multiplicity of each irrep is the inner product with its character
    for (int i = 0; i < T->numIrreps; i++) {
        ComplexNumber n = characterInnerProduct(chi, T->irreps[i]);
        if (!finiteComplex(n) || !roundedIntInRange(n.real, &mults[i])) {
            free(mults);
            for (int j = 0; j < chi->numClasses; j++) freeConjugacyClass(chi->classes[j]);
            freeCharacter(chi);
            return NULL;
        }
    }

    // Release the temporary character and return the multiplicities
    for (int i = 0; i < chi->numClasses; i++) freeConjugacyClass(chi->classes[i]);
    freeCharacter(chi);
    return mults;
}

/* ---------- Internal helpers: class algebra (Burnside-Dixon-Schneider) ---------- */

// Compute structure constants of the class algebra:
//   c[i*r*r + j*r + k] = coefficient of K_k in K_i K_j
//                     = #{ (x, y) : x in C_i, y in C_j, xy = z_k }
// where z_k is a fixed representative of class C_k
static int* classAlgebraStructureConstants(Group* G, ConjugacyClass** classes, int r) {
    // Check that r^3 fits before allocating the flat structure-constant array
    size_t rr;
    size_t rrr;
    if (r < 1 || !checkedSizeMul((size_t)r, (size_t)r, &rr)
            || !checkedSizeMul(rr, (size_t)r, &rrr)) {
        return NULL;
    }
    // Allocate zero-initialized constants c_ijk
    int* c = calloc(rrr, sizeof(int));
    if (!c) return NULL;

    // Precompute inverse-index lookup for ambient group elements
    int* invIdx = malloc(G->card * sizeof(int));
    if (!invIdx) { free(c); return NULL; }
    // Find x^{-1} for each group element index x
    for (int x = 0; x < G->card; x++) {
        GroupElement* inverse = groupInverse(G->elements[x]);
        if (!inverse || inverse->index < 0) { free(invIdx); free(c); return NULL; }
        invIdx[x] = inverse->index;
    }

    // Precompute: ambient-element index -> conjugacy class index
    int* classOf = malloc(G->card * sizeof(int));
    if (!classOf) { free(invIdx); free(c); return NULL; }
    // Initialize every element as unclassified before filling class memberships
    for (int g = 0; g < G->card; g++) classOf[g] = -1;
    // Map each ambient group element to the conjugacy class containing it
    for (int j = 0; j < r; j++) {
        for (int yi = 0; yi < classes[j]->size; yi++) {
            classOf[classes[j]->indices[yi]] = j;
        }
    }

    // Count products K_i K_j by solving y = x^{-1} z_k for each class representative z_k
    for (int i = 0; i < r; i++) {
        ConjugacyClass* Ci = classes[i];
        for (int k = 0; k < r; k++) {
            int z = classes[k]->rep->index;
            for (int xi = 0; xi < Ci->size; xi++) {
                int x = Ci->indices[xi];
                GroupElement* yElement = groupMult(G->elements[invIdx[x]], G->elements[z]);
                if (!yElement || yElement->index < 0) { free(invIdx); free(classOf); free(c); return NULL; }
                int y = yElement->index; // y = x^{-1} * z, so x * y = z
                int j = classOf[y];
                if (j >= 0) {
                    size_t idx = (size_t)i * rr + (size_t)j * (size_t)r + (size_t)k;
                    // Reject integer overflow in an individual structure constant
                    if (c[idx] == INT_MAX) { free(invIdx); free(classOf); free(c); return NULL; }
                    c[idx]++;
                }
            }
        }
    }
    // Release lookup tables and return the flat structure-constant tensor
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
    // Require the group and output count slot
    if (!G || !count) return NULL;
    // Start from an empty result until a valid full character set is found
    *count = 0;

    // Enumerate conjugacy classes; their number is the expected number of irreducibles
    int r;
    ConjugacyClass** classes = getConjugacyClasses(G, &r);
    if (!classes) return NULL;

    // Compute class-algebra multiplication constants in the conjugacy-class basis
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
    // Convert each class-sum multiplication operator into an r-by-r matrix
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
                setEntry(M[i], k, j, elemFromReal((long double)c[(size_t)i * (size_t)r * (size_t)r + (size_t)j * (size_t)r + (size_t)k]));
    }

    // Locate identity class once (used for degree sorting/validation)
    int idClassPos = 0;
    for (int j = 0; j < r; j++) {
        if (classes[j]->size == 1 && classes[j]->rep->index == 0) {
            idClassPos = j;
            break;
        }
    }

    // Prepare the accepted result pointer and a candidate pool shared across attempts
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

    // Different linear combinations can collide on larger groups (e.g. S5)
    // Retry with multiple deterministic seeds until we recover a valid full set
    for (int attempt = 0; attempt < 256 && !out; attempt++) {
        // Derive deterministic pseudo-random coefficients for this attempt
        unsigned int seed = 0x9E3779B9u ^ (unsigned int) (attempt * 0x85EBCA6Bu);

        // Allocate the generic linear combination of class-algebra matrices
        Matrix* M_total = constructMatrix(r, r);
        if (!M_total) break;

        // Allocate and fill coefficients alpha_i for the linear combination
        long double* alpha = malloc((size_t) r * sizeof(long double));
        if (!alpha) {
            freeMatrix(M_total);
            break;
        }
        alpha[0] = 0.0;
        for (int i = 1; i < r; i++) {
            seed = seed * 1103515245u + 12345u;
            alpha[i] = 1.0 + (long double) ((seed >> 8) & 0xFFFFFFu) / (long double) 0x1000000u;
        }

        // Form M_total = sum_i alpha_i M_i, skipping the identity class operator
        for (int i = 1; i < r; i++) {
            for (int row = 0; row < r; row++) {
                for (int col = 0; col < r; col++) {
                    FieldElement cur = getEntry(M_total, row, col);
                    FieldElement mrc = getEntry(M[i], row, col);
                    FieldElement add = elemMul(elemFromReal(alpha[i]), mrc);
                    setEntry(M_total, row, col, elemAdd(cur, add));
                }
            }
        }
        free(alpha);

        // Diagonalize the generic operator to get likely simultaneous eigenvectors
        Matrix** evecs = eigenvectors(M_total);
        freeMatrix(M_total);
        if (!evecs) continue;

        // Allocate temporary candidate characters extracted from this eigensystem
        Character** cand = malloc((size_t) r * sizeof(Character*));
        int candCount = 0;
        if (!cand) {
            for (int i = 0; i < r; i++) if (evecs[i]) freeMatrix(evecs[i]);
            free(evecs);
            break;
        }

        // Convert each usable eigenvector into one candidate irreducible character
        for (int a = 0; a < r; a++) {
            if (!evecs[a]) continue;
            Matrix* v = evecs[a];

            // Pick the largest vector coordinate to stabilize eigenvalue ratios
            int jmax = 0;
            long double vmax = elemAbs(getEntry(v, 0, 0));
            for (int j = 1; j < r; j++) {
                long double mag = elemAbs(getEntry(v, j, 0));
                if (mag > vmax) { vmax = mag; jmax = j; }
            }
            if (vmax < 1e-12) continue;
            ComplexNumber v_ref = elemToComplex(getEntry(v, jmax, 0));
            if (!finiteComplex(v_ref)) continue;

            // Allocate central-character eigenvalues omega_i with omega_identity = 1
            ComplexNumber* omega = malloc((size_t) r * sizeof(ComplexNumber));
            if (!omega) continue;
            omega[0].real = 1.0;
            omega[0].imag = 0.0;

            // Recover the scalar by which each class sum acts on the eigenline
            bool bad = false;
            for (int i = 1; i < r; i++) {
                Matrix* Mv = applyMatrix(M[i], v);
                if (!Mv) { bad = true; break; }
                ComplexNumber Mv_jmax = elemToComplex(getEntry(Mv, jmax, 0));
                omega[i] = complexDiv(Mv_jmax, v_ref);
                freeMatrix(Mv);
                if (!finiteComplex(omega[i])) { bad = true; break; }
            }
            if (bad) {
                free(omega);
                continue;
            }

            // Use character orthogonality to recover the irreducible dimension
            long double sum = 0.0;
            for (int j = 0; j < r; j++) {
                long double mag2 = omega[j].real * omega[j].real + omega[j].imag * omega[j].imag;
                sum += mag2 / (long double) classes[j]->size;
            }
            if (!isfinite(sum) || sum < 1e-15) {
                free(omega);
                continue;
            }
            long double dim_sq = (long double) G->card / sum;
            int dim;
            if (!isfinite(dim_sq) || !roundedIntInRange(sqrtl(dim_sq), &dim)) {
                free(omega);
                continue;
            }
            if (dim < 1) dim = 1;

            // Convert central-character eigenvalues into ordinary character values
            ComplexNumber* values = malloc((size_t) r * sizeof(ComplexNumber));
            if (!values) {
                free(omega);
                continue;
            }
            for (int j = 0; j < r; j++) {
                values[j].real = (long double) dim * omega[j].real / (long double) classes[j]->size;
                values[j].imag = (long double) dim * omega[j].imag / (long double) classes[j]->size;
                if (!finiteComplex(values[j])) {
                    bad = true;
                    break;
                }
                if (fabsl(values[j].real - roundl(values[j].real)) < 1e-6) values[j].real = roundl(values[j].real);
                if (fabsl(values[j].imag - roundl(values[j].imag)) < 1e-6) values[j].imag = roundl(values[j].imag);
                if (fabsl(values[j].imag) < 1e-9) values[j].imag = 0.0;
            }
            free(omega);
            if (bad) {
                free(values);
                continue;
            }

            // Copy the shared class pointers into a wrapper-level class array
            ConjugacyClass** classesCopy = malloc((size_t) r * sizeof(ConjugacyClass*));
            if (!classesCopy) {
                free(values);
                continue;
            }
            for (int j = 0; j < r; j++) classesCopy[j] = classes[j];

            // Build a candidate Character object for later validation
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

        // Release eigenvectors after all candidates for this attempt are extracted
        for (int i = 0; i < r; i++) if (evecs[i]) freeMatrix(evecs[i]);
        free(evecs);

        // Filter candidates for valid degree, norm one, and non-duplication
        for (int i = 0; i < candCount; i++) {
            Character* chi = cand[i];
            if (!chi) continue;

            // Validate that the identity-class value is a positive integer degree
            long double dReal = chi->values[idClassPos].real;
            int d;
            if (!roundedIntInRange(dReal, &d)) {
                freeCharacter(chi);
                continue;
            }
            if (d < 1 || fabsl(dReal - (long double) d) > 2e-2) {
                freeCharacter(chi);
                continue;
            }
            // Validate the irreducibility norm condition <chi,chi> = 1
            ComplexNumber self = characterInnerProduct(chi, chi);
            if (!finiteComplex(self) || fabsl(self.real - 1.0) > 3e-2 || fabsl(self.imag) > 3e-2) {
                freeCharacter(chi);
                continue;
            }

            // Reject characters already found in the accepted pool
            bool duplicate = false;
            for (int p = 0; p < poolCount && !duplicate; p++) {
                bool same = true;
                for (int j = 0; j < r; j++) {
                    if (fabsl(pool[p]->values[j].real - chi->values[j].real) > 1e-6 ||
                        fabsl(pool[p]->values[j].imag - chi->values[j].imag) > 1e-6) {
                        same = false;
                        break;
                    }
                }
                if (same) duplicate = true;
            }

            // Keep the candidate only if it is new and the pool still has room
            if (duplicate || poolCount >= r) {
                freeCharacter(chi);
            } else {
                pool[poolCount++] = chi;
            }
        }
        free(cand);

        // Try another coefficient seed until the pool has the expected size
        if (poolCount != r) continue;

        // Sort by degree, then lexicographically by character values to get stable order
        for (int i = 1; i < poolCount; i++) {
            for (int j = i; j > 0; j--) {
                long double da = pool[j]->values[idClassPos].real;
                long double db = pool[j - 1]->values[idClassPos].real;
                bool swap = false;
                if (da < db - 1e-9) swap = true;
                else if (fabsl(da - db) <= 1e-9) {
                    for (int k = 0; k < r; k++) {
                        long double ar = pool[j]->values[k].real;
                        long double br = pool[j - 1]->values[k].real;
                        if (fabsl(ar - br) > 1e-9) { swap = ar > br; break; }
                        long double ai = pool[j]->values[k].imag;
                        long double bi = pool[j - 1]->values[k].imag;
                        if (fabsl(ai - bi) > 1e-9) { swap = ai > bi; break; }
                    }
                }
                if (swap) {
                    Character* tmp = pool[j];
                    pool[j] = pool[j - 1];
                    pool[j - 1] = tmp;
                } else break;
            }
        }

        // Validate dimensions and self-inner-products for the complete candidate set
        bool valid = true;
        int sumSq = 0;
        for (int i = 0; i < poolCount; i++) {
            long double dReal = pool[i]->values[idClassPos].real;
            int d;
            if (!roundedIntInRange(dReal, &d)) { valid = false; break; }
            if (d < 1 || fabsl(dReal - (long double) d) > 2e-2) { valid = false; break; }
            int dSq;
            if (!checkedIntMul(d, d, &dSq) || !checkedIntAdd(sumSq, dSq, &sumSq)) { valid = false; break; }
            ComplexNumber self = characterInnerProduct(pool[i], pool[i]);
            if (!finiteComplex(self) || fabsl(self.real - 1.0) > 3e-2 || fabsl(self.imag) > 3e-2) { valid = false; break; }
        }
        // Validate pairwise orthogonality once the degree-square sum is correct
        if (valid && sumSq == G->card) {
            for (int i = 0; i < poolCount && valid; i++) {
                for (int j = i + 1; j < poolCount; j++) {
                    ComplexNumber ip = characterInnerProduct(pool[i], pool[j]);
                    if (fabsl(ip.real) > 3e-2 || fabsl(ip.imag) > 3e-2) {
                        valid = false;
                        break;
                    }
                }
            }
        } else {
            valid = false;
        }

        // Discard the whole pool if the character-table identities fail
        if (!valid) {
            for (int i = 0; i < poolCount; i++) freeCharacter(pool[i]);
            for (int i = 0; i < r; i++) pool[i] = NULL;
            poolCount = 0;
            continue;
        }

        // Relabel in sorted order so chi-1, chi-2, ... matches printed order
        for (int i = 0; i < poolCount; i++) {
            char label[32];
            snprintf(label, sizeof(label), "chi-%d", i + 1);
            char* fresh = malloc(strlen(label) + 1);
            if (!fresh) continue;
            strcpy(fresh, label);
            free(pool[i]->repr);
            pool[i]->repr = fresh;
        }

        // Promote the validated pool to the output result
        out = pool;
        outCount = poolCount;
        pool = NULL;
    }

    // The structure constants are no longer needed after the attempts complete
    free(c);

    // On failure, release every temporary matrix, class, and candidate character
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

    // On success, release only wrapper arrays whose class objects are shared by characters
    for (int i = 0; i < r; i++) freeMatrix(M[i]);
    free(M);
    free(classes); // ConjugacyClass objects stay alive: each Character holds them via classesCopy

    // Return the validated irreducible character list and its count
    *count = outCount;
    return out;
}

// Build the character table of G using the class-algebra method. Works for any finite
// group whose class-algebra matrices have well-conditioned eigenvalues
CharacterTable* characterTable(Group* G) {
    // Require a group before computing its character table
    if (!G) return NULL;

    // Compute all irreducible characters first
    int numIrreps;
    Character** irreps = allIrreducibleCharacters(G, &numIrreps);
    if (!irreps || numIrreps == 0) {
        free(irreps);
        return NULL;
    }
    // The number of classes is stored on every returned character
    int r = irreps[0]->numClasses;

    // Build the table's own classes array (pointer copies, the Character objects share
    // the same underlying ConjugacyClass*)
    ConjugacyClass** classes = malloc(r * sizeof(ConjugacyClass*));
    if (!classes) {
        for (int i = 0; i < irreps[0]->numClasses; i++) freeConjugacyClass(irreps[0]->classes[i]);
        for (int k = 0; k < numIrreps; k++) freeCharacter(irreps[k]);
        free(irreps);
        return NULL;
    }
    // Share the conjugacy class objects already owned through the characters
    for (int i = 0; i < r; i++) classes[i] = irreps[0]->classes[i];

    // Allocate a table-shaped copy of all character values
    ComplexNumber** values = malloc(numIrreps * sizeof(ComplexNumber*));
    if (!values) {
        free(classes);
        for (int i = 0; i < irreps[0]->numClasses; i++) freeConjugacyClass(irreps[0]->classes[i]);
        for (int k = 0; k < numIrreps; k++) freeCharacter(irreps[k]);
        free(irreps);
        return NULL;
    }
    // Copy each character row so the table can expose values directly
    for (int k = 0; k < numIrreps; k++) {
        values[k] = malloc(r * sizeof(ComplexNumber));
        for (int j = 0; j < r; j++) values[k][j] = irreps[k]->values[j];
    }

    // Wrap the classes, irreducibles, and copied value grid as a CharacterTable
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
    // Return the assembled character table
    return T;
}

/* ---------- Pretty-printing ---------- */

static void formatChiValue(ComplexNumber c, char* out, size_t outSize) {
    // Use a small tolerance to clean numerical noise from character values
    long double tol = 1e-6;
    // Split the complex value into mutable real and imaginary parts
    long double re = c.real;
    long double im = c.imag;
    // Snap nearly integral coordinates to exact-looking integers
    if (fabsl(re - roundl(re)) < tol) re = roundl(re);
    if (fabsl(im - roundl(im)) < tol) im = roundl(im);
    // Suppress tiny residual real or imaginary components
    if (fabsl(re) < tol) re = 0.0;
    if (fabsl(im) < tol) im = 0.0;
    // Format real, imaginary, and genuinely complex values compactly
    if (fabsl(im) < tol) {
        snprintf(out, outSize, "%Lg", re);
    } else if (fabsl(re) < tol) {
        snprintf(out, outSize, "%Lgi", im);
    } else {
        snprintf(out, outSize, "%Lg%+Lgi", re, im);
    }
}

static int intTextWidth(int x) {
    // Format the integer into a scratch buffer to measure its printed width
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", x);
    return (int)strlen(buf);
}

static void printCenteredCell(const char* text, int width) {
    // Measure the cell text to determine whether padding is needed
    int len = (int)strlen(text);
    // Print oversized text without centering so it remains intact
    if (len >= width) {
        printf(" %s ", text);
        return;
    }
    // Split the remaining cell width as left and right padding
    int pad = width - len;
    int left = pad / 2;
    int right = pad - left;
    // Emit one leading separator space, the centered text, and one trailing separator space
    printf(" ");
    for (int i = 0; i < left; i++) putchar(' ');
    printf("%s", text);
    for (int i = 0; i < right; i++) putchar(' ');
    printf(" ");
}

void printCharacterTable(CharacterTable* T) {
    // Handle missing tables with a simple diagnostic
    if (!T) { printf("(null character table)\n"); return; }
    // Cache the table dimensions for the formatting loops
    int n = T->numClasses;
    int r = T->numIrreps;

    // Compute the width needed for the left-side row labels
    int rowLabelWidth = (int)strlen("irrep");
    for (int k = 0; k < r; k++) {
        const char* lbl = (T->irreps[k] && T->irreps[k]->repr) ? T->irreps[k]->repr : "?";
        int w = (int)strlen(lbl);
        if (w > rowLabelWidth) rowLabelWidth = w;
    }
    if ((int)strlen("class rep") > rowLabelWidth) rowLabelWidth = (int)strlen("class rep");
    if ((int)strlen("class size") > rowLabelWidth) rowLabelWidth = (int)strlen("class size");
    if ((int)strlen("elt order") > rowLabelWidth) rowLabelWidth = (int)strlen("elt order");

    // Allocate per-class column widths
    int* colWidths = calloc((size_t)n, sizeof(int));
    if (!colWidths) {
        printf("(failed to allocate print buffer)\n");
        return;
    }
    // Compute each column width from headers, class metadata, and character values
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

    // Print the table title with the group order
    printf("Character table of group (|G| = %zu):\n\n", T->group->card);

    // Print the conjugacy-class representative row
    printf("%-*s |", rowLabelWidth, "class rep");
    for (int j = 0; j < n; j++) {
        const char* rep = (T->classes[j] && T->classes[j]->rep && T->classes[j]->rep->repr)
            ? T->classes[j]->rep->repr
            : "?";
        printCenteredCell(rep, colWidths[j]);
    }
    printf("\n");

    // Print the conjugacy-class size row
    printf("%-*s |", rowLabelWidth, "class size");
    for (int j = 0; j < n; j++) {
        char num[32];
        snprintf(num, sizeof(num), "%d", T->classes[j]->size);
        printCenteredCell(num, colWidths[j]);
    }
    printf("\n");

    // Print the representative element-order row
    printf("%-*s |", rowLabelWidth, "elt order");
    for (int j = 0; j < n; j++) {
        char num[32];
        snprintf(num, sizeof(num), "%d", T->classes[j]->elementOrder);
        printCenteredCell(num, colWidths[j]);
    }
    printf("\n");

    // Print a separator sized to the computed columns
    for (int i = 0; i < rowLabelWidth; i++) putchar('-');
    printf("-+");
    for (int j = 0; j < n; j++) {
        for (int i = 0; i < colWidths[j] + 2; i++) putchar('-');
    }
    printf("\n");

    // Print one row of formatted values for each irreducible character
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
    // Finish the table and release temporary width storage
    printf("\n");
    free(colWidths);
}
