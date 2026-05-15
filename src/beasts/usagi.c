#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<math.h>
#include<limits.h>
#include "hebi.h"
#include "sokko.h"
#include "usagi.h"

static char* finiteFieldReprFromIndex(long long index, int p, int degree);
static int finiteFieldAddIndex(long long left, long long right, int p, int degree);
static int finiteFieldMultIndex(long long left, long long right, FiniteFieldRingData* ff);

/* ---------- Free methods ---------- */

static void freeGroupElementData(GroupElement* g) {
    if (!g) return;

    switch (g->type) {
        case GROUP_ELEM_PERMUTATION:
            free(g->data.perm);
            break;
        case GROUP_ELEM_PRODUCT:
            free(g->data.product.factors);
            break;
        case GROUP_ELEM_MATRIX:
            freeMatrix(g->data.matrix);
            break;
        case GROUP_ELEM_INDEXED:
        case GROUP_ELEM_INT:
        case GROUP_ELEM_ZN:
        case GROUP_ELEM_DIHEDRAL:
            break;
    }
}

static void freeRingElementData(RingElement* x) {
    if (!x) return;

    switch (x->type) {
        case RING_ELEM_PRODUCT:
            free(x->data.product.factors);
            break;
        case RING_ELEM_MATRIX:
            freeMatrix(x->data.matrix);
            break;
        case RING_ELEM_INDEXED:
        case RING_ELEM_INT:
        case RING_ELEM_ZN:
        case RING_ELEM_FF:
            break;
    }
}

static void freeGroupElements(GroupElement** elements, size_t count) {
    if (!elements) return;

    for (size_t i = 0; i < count; i++) {
        freeGroupElement(elements[i]);
    }
    free(elements);
}

static void freeRingElements(RingElement** elements, size_t count) {
    if (!elements) return;

    for (size_t i = 0; i < count; i++) {
        freeRingElement(elements[i]);
    }
    free(elements);
}

static void freeGroupData(Group* group) {
    if (!group) return;

    switch (group->type) {
        case GROUP_CAYLEY:
            if (group->data.cayley.table) {
                for (size_t i = 0; i < group->card; i++) {
                    free(group->data.cayley.table[i]);
                }
                free(group->data.cayley.table);
            }
            break;
        case GROUP_PRODUCT:
            free(group->data.product.factors);
            break;
        case GROUP_ZN:
        case GROUP_Z:
        case GROUP_SYMMETRIC:
        case GROUP_ALTERNATING:
        case GROUP_DIHEDRAL:
        case GROUP_QUOTIENT:
        case GROUP_MATRIX:
            break;
    }

    free(group->generators);
}

static void freeRingData(Ring* ring) {
    if (!ring) return;

    switch (ring->type) {
        case RING_CAYLEY:
            if (ring->data.cayley.addTable) {
                for (size_t i = 0; i < ring->card; i++) {
                    free(ring->data.cayley.addTable[i]);
                }
                free(ring->data.cayley.addTable);
            }
            if (ring->data.cayley.multTable) {
                for (size_t i = 0; i < ring->card; i++) {
                    free(ring->data.cayley.multTable[i]);
                }
                free(ring->data.cayley.multTable);
            }
            break;
        case RING_PRODUCT:
            free(ring->data.product.factors);
            break;
        case RING_ZN:
        case RING_Z:
        case RING_QUOTIENT:
        case RING_MATRIX:
            break;
        case RING_FF:
            free(ring->data.ff.modulus);
            break;
    }

    free(ring->generators);
}

static void freeSubgroupData(SubGroup* subgroup) {
    if (!subgroup) return;

    switch (subgroup->type) {
        case SUBGROUP_INDEXED:
            free(subgroup->data.indexed.indices);
            break;
        case SUBGROUP_GENERATED:
            if (subgroup->generators != subgroup->data.generated.generators) {
                free(subgroup->data.generated.generators);
            }
            break;
        case SUBGROUP_EXPLICIT:
            if (subgroup->elements != subgroup->data.explicitElements.elements) {
                free(subgroup->data.explicitElements.elements);
            }
            break;
        case SUBGROUP_PREDICATE:
            break;
    }

    free(subgroup->elements);
    free(subgroup->generators);
}

static void freeGroupCosetData(GroupCoset* coset) {
    if (!coset) return;

    switch (coset->type) {
        case GROUP_COSET_INDEXED:
            free(coset->data.indexed.indices);
            break;
        case GROUP_COSET_EXPLICIT:
            if (coset->elements != coset->data.explicitElements.elements) {
                free(coset->data.explicitElements.elements);
            }
            break;
        case GROUP_COSET_REPRESENTATIVE:
            break;
    }

    free(coset->elements);
}

static bool indexInList(int index, int* indices, int indicesLen) {
    for (int i = 0; i < indicesLen; i++) {
        if (indices[i] == index) return true;
    }

    return false;
}

static bool hasDuplicateIndices(int* indices, int indicesLen) {
    if (!indices) return false;

    for (int i = 0; i < indicesLen; i++) {
        for (int j = i + 1; j < indicesLen; j++) {
            if (indices[i] == indices[j]) return true;
        }
    }

    return false;
}

static void sortIndices(int* indices, int indicesLen) {
    if (!indices) return;

    for (int i = 1; i < indicesLen; i++) {
        int value = indices[i];
        int j = i - 1;
        while (j >= 0 && indices[j] > value) {
            indices[j + 1] = indices[j];
            j--;
        }
        indices[j + 1] = value;
    }
}

static int cayleyGroupInverseIndex(Group* G, int index) {
    if (!G || G->type != GROUP_CAYLEY || index < 0 || (size_t)index >= G->card) return -1;

    for (size_t i = 0; i < G->card; i++) {
        if (G->data.cayley.table[index][i] == 0 && G->data.cayley.table[i][index] == 0) return (int)i;
    }

    return -1;
}

static int cayleyRingAddInverseIndex(Ring* R, int index) {
    if (!R || R->type != RING_CAYLEY || index < 0 || (size_t)index >= R->card) return -1;

    for (size_t i = 0; i < R->card; i++) {
        if (R->data.cayley.addTable[index][i] == 0 && R->data.cayley.addTable[i][index] == 0) return (int)i;
    }

    return -1;
}

static GroupElement** subgroupElementView(Group* G, int* indices, int indicesLen) {
    GroupElement** elements = malloc(indicesLen * sizeof(GroupElement*));
    if (!elements) return NULL;

    for (int i = 0; i < indicesLen; i++) {
        elements[i] = G->elements[indices[i]];
    }

    return elements;
}

static RingElement** subringElementView(Ring* R, int* indices, int indicesLen) {
    RingElement** elements = malloc(indicesLen * sizeof(RingElement*));
    if (!elements) return NULL;

    for (int i = 0; i < indicesLen; i++) {
        elements[i] = R->elements[indices[i]];
    }

    return elements;
}

static int finiteGroupElementIndex(Group* group, const char* repr) {
    if (!group || !group->isFinite || !group->elements || !repr) return -1;

    for (size_t i = 0; i < group->card; i++) {
        if (group->elements[i] && group->elements[i]->repr && strcmp(group->elements[i]->repr, repr) == 0) return (int)i;
    }

    return -1;
}

static int finiteRingElementIndex(Ring* ring, const char* repr) {
    if (!ring || !ring->isFinite || !ring->elements || !repr) return -1;

    for (size_t i = 0; i < ring->card; i++) {
        if (ring->elements[i] && ring->elements[i]->repr && strcmp(ring->elements[i]->repr, repr) == 0) return (int)i;
    }

    return -1;
}

static GroupElement* canonicalGroupElement(Group* group, GroupElement* element) {
    if (!group || !element) return element;
    if (group->isFinite && group->elements && element->index >= 0 && (size_t)element->index < group->card) {
        GroupElement* canonical = group->elements[element->index];
        freeGroupElement(element);
        return canonical;
    }

    return element;
}

static RingElement* canonicalRingElement(Ring* ring, RingElement* element) {
    if (!ring || !element) return element;
    if (ring->isFinite && ring->elements && element->index >= 0 && (size_t)element->index < ring->card) {
        RingElement* canonical = ring->elements[element->index];
        freeRingElement(element);
        return canonical;
    }

    return element;
}

static bool parseLongLong(const char* repr, long long* value) {
    if (!repr || !value) return false;

    char* end = NULL;
    long long parsed = strtoll(repr, &end, 10);
    if (end == repr || *end != '\0') return false;

    *value = parsed;
    return true;
}

static long long normalizeMod(long long value, long long modulus) {
    long long result = value % modulus;
    if (result < 0) result += modulus;
    return result;
}

static GroupElement* allocateGroupElement(Group* group, GroupElementType type, int index, const char* repr) {
    if (!repr || repr[0] == '\0') return NULL;

    GroupElement* element = calloc(1, sizeof(GroupElement));
    if (!element) return NULL;

    element->repr = malloc(strlen(repr) + 1);
    if (!element->repr) {
        free(element);
        return NULL;
    }
    strcpy(element->repr, repr);

    element->group = group;
    element->index = index;
    element->type = type;
    return element;
}

static RingElement* allocateRingElement(Ring* ring, RingElementType type, int index, const char* repr) {
    if (!repr || repr[0] == '\0') return NULL;

    RingElement* element = calloc(1, sizeof(RingElement));
    if (!element) return NULL;

    element->repr = malloc(strlen(repr) + 1);
    if (!element->repr) {
        free(element);
        return NULL;
    }
    strcpy(element->repr, repr);

    element->ring = ring;
    element->index = index;
    element->type = type;
    return element;
}

static GroupElement* constructRawGroupElement(void* data) {
    char* repr = data;
    GroupElement* element = allocateGroupElement(NULL, GROUP_ELEM_INDEXED, -1, repr);
    if (!element) return NULL;

    element->data.indexValue = -1;
    return element;
}

static RingElement* constructRawRingElement(void* data) {
    char* repr = data;
    RingElement* element = allocateRingElement(NULL, RING_ELEM_INDEXED, -1, repr);
    if (!element) return NULL;

    element->data.indexValue = -1;
    return element;
}

static GroupElement* constructCayleyGroupElement(Group* group, void* data) {
    if (!group || !data || !group->isFinite || !group->elements) return NULL;

    int index = *(int*)data;
    if (index < 0 || (size_t)index >= group->card || !group->elements[index]) return NULL;

    GroupElement* element = allocateGroupElement(group, GROUP_ELEM_INDEXED, index, group->elements[index]->repr);
    if (!element) return NULL;

    element->data.indexValue = index;
    return element;
}

static GroupElement* constructZGroupElement(Group* group, void* data) {
    if (!group || !data) return NULL;

    long long value = *(long long*)data;
    char repr[64];
    snprintf(repr, sizeof(repr), "%lld", value);

    GroupElement* element = allocateGroupElement(group, GROUP_ELEM_INT, -1, repr);
    if (!element) return NULL;

    element->data.integer = value;
    return element;
}

static GroupElement* constructZnGroupElement(Group* group, void* data) {
    if (!group || !data || group->data.zn.modulus <= 0) return NULL;

    long long value = normalizeMod(*(long long*)data, group->data.zn.modulus);
    char repr[64];
    snprintf(repr, sizeof(repr), "%lld", value);
    int index = finiteGroupElementIndex(group, repr);

    GroupElement* element = allocateGroupElement(group, GROUP_ELEM_ZN, index, repr);
    if (!element) return NULL;

    element->data.znVal = value;
    return element;
}

static bool validPermutation(long long* perm, size_t degree) {
    if (!perm) return false;

    bool* seen = calloc(degree, sizeof(bool));
    if (!seen) return false;

    for (size_t i = 0; i < degree; i++) {
        if (perm[i] < 0 || (size_t)perm[i] >= degree || seen[perm[i]]) {
            free(seen);
            return false;
        }
        seen[perm[i]] = true;
    }

    free(seen);
    return true;
}

static GroupElement* constructPermutationGroupElement(Group* group, void* data, bool requireEven) {
    if (!group || !data) return NULL;

    size_t degree = group->data.permutation.degree;
    long long* perm = data;
    if (!validPermutation(perm, degree)) return NULL;

    int* intPerm = malloc(degree * sizeof(int));
    if (!intPerm) return NULL;
    for (size_t i = 0; i < degree; i++) intPerm[i] = (int)perm[i];

    if (requireEven && permSign(intPerm, (int)degree) != 0) {
        free(intPerm);
        return NULL;
    }

    char* repr = permRepr(intPerm, (int)degree);
    free(intPerm);
    if (!repr) return NULL;

    int index = finiteGroupElementIndex(group, repr);
    GroupElement* element = allocateGroupElement(group, GROUP_ELEM_PERMUTATION, index, repr);
    free(repr);
    if (!element) return NULL;

    element->data.perm = malloc(degree * sizeof(long long));
    if (!element->data.perm) {
        freeGroupElement(element);
        return NULL;
    }
    for (size_t i = 0; i < degree; i++) element->data.perm[i] = perm[i];

    return element;
}

static GroupElement* constructSymmetricGroupElement(Group* group, void* data) {
    return constructPermutationGroupElement(group, data, false);
}

static GroupElement* constructAlternatingGroupElement(Group* group, void* data) {
    return constructPermutationGroupElement(group, data, true);
}

static GroupElement* constructDihedralGroupElement(Group* group, void* data) {
    if (!group || !data || group->data.dihedral.n <= 0) return NULL;

    DihedralElementData raw = *(DihedralElementData*)data;
    DihedralElementData normalized;
    normalized.a = normalizeMod(raw.a, group->data.dihedral.n);
    normalized.b = raw.b;

    char repr[64];
    if (!normalized.b) {
        if (normalized.a == 0) snprintf(repr, sizeof(repr), "e");
        else if (normalized.a == 1) snprintf(repr, sizeof(repr), "r");
        else snprintf(repr, sizeof(repr), "r^%lld", normalized.a);
    } else {
        if (normalized.a == 0) snprintf(repr, sizeof(repr), "s");
        else if (normalized.a == 1) snprintf(repr, sizeof(repr), "sr");
        else snprintf(repr, sizeof(repr), "sr^%lld", normalized.a);
    }

    int index = finiteGroupElementIndex(group, repr);
    GroupElement* element = allocateGroupElement(group, GROUP_ELEM_DIHEDRAL, index, repr);
    if (!element) return NULL;

    element->data.dihedral = normalized;
    return element;
}

static GroupElement* constructProductGroupElement(Group* group, void* data) {
    if (!group || !data) return NULL;

    ProductGroupElementData* product = data;
    if (!product->factors || product->count == 0) return NULL;
    if (group->data.product.count != 0 && product->count != group->data.product.count) return NULL;

    size_t reprLen = 3;
    for (size_t i = 0; i < product->count; i++) {
        if (!product->factors[i] || !product->factors[i]->repr) return NULL;
        reprLen += strlen(product->factors[i]->repr) + 1;
    }

    char* repr = malloc(reprLen);
    if (!repr) return NULL;
    size_t pos = 0;
    pos += snprintf(repr + pos, reprLen - pos, "(");
    for (size_t i = 0; i < product->count; i++) {
        if (i > 0) pos += snprintf(repr + pos, reprLen - pos, ",");
        pos += snprintf(repr + pos, reprLen - pos, "%s", product->factors[i]->repr);
    }
    snprintf(repr + pos, reprLen - pos, ")");

    char* flat = flattenRepr(repr);
    char* elementRepr = flat ? flat : repr;
    int index = finiteGroupElementIndex(group, elementRepr);
    GroupElement* element = allocateGroupElement(group, GROUP_ELEM_PRODUCT, index, elementRepr);
    free(flat);
    free(repr);
    if (!element) return NULL;

    element->data.product.count = product->count;
    element->data.product.factors = malloc(product->count * sizeof(GroupElement*));
    if (!element->data.product.factors) {
        freeGroupElement(element);
        return NULL;
    }
    for (size_t i = 0; i < product->count; i++) element->data.product.factors[i] = product->factors[i];

    return element;
}

static GroupElement* constructQuotientGroupElement(Group* group, void* data) {
    return constructCayleyGroupElement(group, data);
}

static GroupElement* constructMatrixGroupElement(Group* group, void* data) {
    if (!group || !data) return NULL;

    GroupElement* element = allocateGroupElement(group, GROUP_ELEM_MATRIX, -1, "matrix");
    if (!element) return NULL;

    element->data.matrix = data;
    return element;
}

static RingElement* constructCayleyRingElement(Ring* ring, void* data) {
    if (!ring || !data || !ring->isFinite || !ring->elements) return NULL;

    int index = *(int*)data;
    if (index < 0 || (size_t)index >= ring->card || !ring->elements[index]) return NULL;

    RingElement* element = allocateRingElement(ring, RING_ELEM_INDEXED, index, ring->elements[index]->repr);
    if (!element) return NULL;

    element->data.indexValue = index;
    return element;
}

static RingElement* constructZRingElement(Ring* ring, void* data) {
    if (!ring || !data) return NULL;

    long long value = *(long long*)data;
    char repr[64];
    snprintf(repr, sizeof(repr), "%lld", value);

    RingElement* element = allocateRingElement(ring, RING_ELEM_INT, -1, repr);
    if (!element) return NULL;

    element->data.integer = value;
    return element;
}

static RingElement* constructZnRingElement(Ring* ring, void* data) {
    if (!ring || !data || ring->data.zn.modulus <= 0) return NULL;

    long long value = normalizeMod(*(long long*)data, ring->data.zn.modulus);
    char repr[64];
    snprintf(repr, sizeof(repr), "%lld", value);
    int index = finiteRingElementIndex(ring, repr);

    RingElement* element = allocateRingElement(ring, RING_ELEM_ZN, index, repr);
    if (!element) return NULL;

    element->data.znVal = value;
    return element;
}

static RingElement* constructFiniteFieldRingElement(Ring* ring, void* data) {
    if (!ring || !data || ring->data.ff.p <= 1 || ring->data.ff.degree < 1) return NULL;

    long long value = normalizeMod(*(long long*)data, (long long)ring->card);
    char* repr = finiteFieldReprFromIndex(value, ring->data.ff.p, ring->data.ff.degree);
    if (!repr) return NULL;

    int index = finiteRingElementIndex(ring, repr);
    RingElement* element = allocateRingElement(ring, RING_ELEM_FF, index, repr);
    free(repr);
    if (!element) return NULL;

    element->data.znVal = value;
    return element;
}

static RingElement* constructProductRingElement(Ring* ring, void* data) {
    if (!ring || !data) return NULL;

    ProductRingElementData* product = data;
    if (!product->factors || product->count == 0) return NULL;
    if (ring->data.product.count != 0 && product->count != ring->data.product.count) return NULL;

    size_t reprLen = 3;
    for (size_t i = 0; i < product->count; i++) {
        if (!product->factors[i] || !product->factors[i]->repr) return NULL;
        reprLen += strlen(product->factors[i]->repr) + 1;
    }

    char* repr = malloc(reprLen);
    if (!repr) return NULL;
    size_t pos = 0;
    pos += snprintf(repr + pos, reprLen - pos, "(");
    for (size_t i = 0; i < product->count; i++) {
        if (i > 0) pos += snprintf(repr + pos, reprLen - pos, ",");
        pos += snprintf(repr + pos, reprLen - pos, "%s", product->factors[i]->repr);
    }
    snprintf(repr + pos, reprLen - pos, ")");

    char* flat = flattenRepr(repr);
    char* elementRepr = flat ? flat : repr;
    int index = finiteRingElementIndex(ring, elementRepr);
    RingElement* element = allocateRingElement(ring, RING_ELEM_PRODUCT, index, elementRepr);
    free(flat);
    free(repr);
    if (!element) return NULL;

    element->data.product.count = product->count;
    element->data.product.factors = malloc(product->count * sizeof(RingElement*));
    if (!element->data.product.factors) {
        freeRingElement(element);
        return NULL;
    }
    for (size_t i = 0; i < product->count; i++) element->data.product.factors[i] = product->factors[i];

    return element;
}

static RingElement* constructQuotientRingElement(Ring* ring, void* data) {
    return constructCayleyRingElement(ring, data);
}

static RingElement* constructMatrixRingElement(Ring* ring, void* data) {
    if (!ring || !data) return NULL;

    RingElement* element = allocateRingElement(ring, RING_ELEM_MATRIX, -1, "matrix");
    if (!element) return NULL;

    element->data.matrix = data;
    return element;
}

// Free the memory associated with a GroupElement
void freeGroupElement(GroupElement* g) {
    if (!g) return;
    freeGroupElementData(g);
    free(g->repr);
    free(g);
}

// Free the memory associated with a Group
void freeGroup(Group* group) {
    if (!group) return;
    freeGroupElements(group->elements, group->card);
    freeGroupData(group);
    free(group);
}

// Free the memory associated with a SubGroup
void freeSubgroup(SubGroup* subgroup) {
    if (!subgroup) return;
    freeSubgroupData(subgroup);
    free(subgroup);
}

// Free the memroy associated with a GroupCoset
void freeGroupCoset(GroupCoset* coset) {
    if (!coset) return;
    freeGroupCosetData(coset);
	free(coset);
}

// Free a GroupHomomorphism
void freeGroupHomomorphism(GroupHomomorphism* homo) {
    if (!homo) return;
    switch (homo->type) {
        case GROUP_HOM_INDEXED:
            free(homo->data.indexed.mapping);
            break;
        case GROUP_HOM_FUNCTION:
            break;
    }
    free(homo);
}

// Free a RingHomomorphism
void freeRingHomomorphism(RingHomomorphism* homo) {
    if (!homo) return;
    switch (homo->type) {
        case RING_HOM_INDEXED:
            free(homo->data.indexed.mapping);
            break;
        case RING_HOM_FUNCTION:
            break;
    }
    free(homo);
}

// Free the memory associated with a RingElement
void freeRingElement(RingElement* x) {
    if (!x) return;
    freeRingElementData(x);
    free(x->repr);
    free(x);
}

// Free the memory associated with a Ring
void freeRing(Ring* ring) {
    if (!ring) return;
    freeRingElements(ring->elements, ring->card);
    freeRingData(ring);
    free(ring);
}

/* ---------- Construct methods (basic) ---------- */

// Validate that a matrix can be a table for a Group
bool validateGroupTable(int** table, size_t card) {
    if (!table || card == 0) return false;

    // Verify associativity
    for (size_t i = 0; i < card; i++) {
		for (size_t j = 0; j < card; j++) {
			if (table[i][j] < 0 || (size_t)table[i][j] >= card) return false;
	        for (size_t k = 0; k < card; k++) {
		    	if (table[table[i][j]][k] != table[i][table[j][k]]) return false;
	        }
	    }
    }

    // Verify that an identity exists
    for (size_t i = 0; i < card; i++) {
	    if (table[i][0] != i || table[0][i] != i) return false;
    }

    // Verify that an inverse exists
    for (size_t i = 0; i < card; i++) {
	bool invExists = false;
	for (size_t j = 0; j < card; j++) {
	    if (table[i][j] == table[0][0] && table[j][i] == table[0][0]) {
		    invExists = true;
		    goto skip;
	    }
	}
	skip:
	if (!invExists) return false;
    }

    return true;
}

// Validate that addTable and multTable can be used for a Ring
bool validateRingTables(int** addTable, int** multTable, size_t card) {
    if (!addTable || !multTable || card == 0) return false;

    // Verify associativity and the distributive property
    for (size_t i = 0; i < card; i++) {
	    for (size_t j = 0; j < card; j++) {
	        if (addTable[i][j] < 0 || (size_t)addTable[i][j] >= card || multTable[i][j] < 0 || (size_t)multTable[i][j] >= card) return false;
	        for (size_t k = 0; k < card; k++) {
		        if (addTable[addTable[i][j]][k] != addTable[i][addTable[j][k]]) return false;
		        if (multTable[multTable[i][j]][k] != multTable[i][multTable[j][k]]) return false;
		        if (multTable[addTable[i][j]][k] != addTable[multTable[i][k]][multTable[j][k]]) return false;
		        if (multTable[k][addTable[i][j]] != addTable[multTable[k][i]][multTable[k][j]]) return false;
	        }
	    }
    }

    // Verify additive identity
    for (size_t i = 0; i < card; i++) {
	    if (addTable[i][0] != i || addTable[0][i] != i) return false;
    }

    // Verify the existence of additive inverse
    for (size_t i = 0; i < card; i++) {
	bool invExists = false;
	for (size_t j = 0; j < card; j++) {
	    if (addTable[i][j] == addTable[0][0] && addTable[j][i] == addTable[0][0]) {
		    invExists = true;
		    goto skip;
	    }
	}
	skip:
	    if (!invExists) return false;
    }

    // Verify that addition is commutative
    for (size_t i = 0; i < card; i++) {
        for (size_t j = 0; j < card; j++) {
            if (addTable[i][j] != addTable[j][i]) return false;
        }
    }

    // Verify that 0*x = 0
    for (size_t i = 0; i < card; i++) {
	    if (multTable[0][i] != 0 || multTable[i][0] != 0) return false;
    }
    
    return true;
}

// Construct a GroupElement
GroupElement* constructGroupElement(Group* group, void* data) {
    if (!data) return NULL;
    if (!group) return constructRawGroupElement(data);

    switch (group->type) {
        case GROUP_CAYLEY:
            return constructCayleyGroupElement(group, data);
        case GROUP_ZN:
            return constructZnGroupElement(group, data);
        case GROUP_Z:
            return constructZGroupElement(group, data);
        case GROUP_SYMMETRIC:
            return constructSymmetricGroupElement(group, data);
        case GROUP_ALTERNATING:
            return constructAlternatingGroupElement(group, data);
        case GROUP_DIHEDRAL:
            return constructDihedralGroupElement(group, data);
        case GROUP_PRODUCT:
            return constructProductGroupElement(group, data);
        case GROUP_QUOTIENT:
            return constructQuotientGroupElement(group, data);
        case GROUP_MATRIX:
            return constructMatrixGroupElement(group, data);
    }

    return NULL;
}

// Construct a Group by type
Group* constructGroup(GroupType type, void* data) {
    switch (type) {
        case GROUP_CAYLEY: {
            if (!data) return NULL;
            TableGroupConstructionData* tableData = data;
            if (tableData->skipValidate) return constructTableGroupSkipValidate(tableData->elements, tableData->table, tableData->tableLen);
            return constructTableGroup(tableData->elements, tableData->table, tableData->tableLen);
        }
        case GROUP_ZN: {
            if (!data) return NULL;
            ZnGroupData* znData = data;
            return constructZnGroup((int)znData->modulus);
        }
        case GROUP_Z: {
            Group* group = calloc(1, sizeof(Group));
            if (!group) return NULL;
            group->type = GROUP_Z;
            group->isFinite = false;
            return group;
        }
        case GROUP_SYMMETRIC: {
            if (!data) return NULL;
            PermutationGroupData* permutationData = data;
            return constructSymmetricGroup((int)permutationData->degree);
        }
        case GROUP_ALTERNATING: {
            if (!data) return NULL;
            PermutationGroupData* permutationData = data;
            return constructAlternatingGroup((int)permutationData->degree);
        }
        case GROUP_DIHEDRAL: {
            if (!data) return NULL;
            DihedralGroupData* dihedralData = data;
            return constructDihedralGroup((int)dihedralData->n);
        }
        case GROUP_PRODUCT: {
            if (!data) return NULL;
            ProductGroupData* productData = data;
            if (!productData->factors || productData->count < 2) return NULL;
            Group* product = constructProductGroup(productData->factors[0], productData->factors[1]);
            if (!product) return NULL;
            for (size_t i = 2; i < productData->count; i++) {
                Group* next = constructProductGroup(product, productData->factors[i]);
                if (!next) {
                    freeGroup(product);
                    return NULL;
                }
                product = next;
            }
            return product;
        }
        case GROUP_QUOTIENT: {
            if (!data) return NULL;
            QuotientGroupData* quotientData = data;
            return quotientGroup(quotientData->ambient, quotientData->normal);
        }
        case GROUP_MATRIX:
            return NULL;
    }

    return NULL;
}

// Construct a Group from a Cayley table
Group* constructTableGroup(GroupElement** elements, int** table, int tableLen) {
    if (!elements || !table) return NULL;
    if (tableLen < 1) return NULL;
    if (!validateGroupTable(table, tableLen)) return NULL;

    return constructTableGroupSkipValidate(elements, table, tableLen);
}

// Construct a Group from a Cayley table, skipping the validation step
Group* constructTableGroupSkipValidate(GroupElement** elements, int** table, int tableLen) {
    if (!elements || !table) return NULL;
    if (tableLen < 1) return NULL;

    Group* group = calloc(1, sizeof(Group));
    if (!group) return NULL;

    group->type = GROUP_CAYLEY;
    group->elements = elements;
    group->card = (size_t)tableLen;
    group->isFinite = true;
    group->data.cayley.elements = elements;
    group->data.cayley.table = table;

    for (int i = 0; i < tableLen; i++){
	group->elements[i]->group = group;
	group->elements[i]->index = i;
	group->elements[i]->type = GROUP_ELEM_INDEXED;
	group->elements[i]->data.indexValue = i;
    }

    return group;
}

// Construct a RingElement
RingElement* constructRingElement(Ring* ring, void* data) {
    if (!data) return NULL;
    if (!ring) return constructRawRingElement(data);

    switch (ring->type) {
        case RING_CAYLEY:
            return constructCayleyRingElement(ring, data);
        case RING_ZN:
            return constructZnRingElement(ring, data);
        case RING_Z:
            return constructZRingElement(ring, data);
        case RING_FF:
            return constructFiniteFieldRingElement(ring, data);
        case RING_PRODUCT:
            return constructProductRingElement(ring, data);
        case RING_QUOTIENT:
            return constructQuotientRingElement(ring, data);
        case RING_MATRIX:
            return constructMatrixRingElement(ring, data);
    }

    return NULL;
}

// Construct a Ring by type
Ring* constructRing(RingType type, void* data) {
    switch (type) {
        case RING_CAYLEY: {
            if (!data) return NULL;
            TableRingConstructionData* tableData = data;
            if (tableData->skipValidate) return constructTableRingSkipValidate(tableData->elements, tableData->addTable, tableData->multTable, tableData->tableLen);
            return constructTableRing(tableData->elements, tableData->addTable, tableData->multTable, tableData->tableLen);
        }
        case RING_ZN: {
            if (!data) return NULL;
            ZnRingData* znData = data;
            return constructZnRing((int)znData->modulus);
        }
        case RING_FF: {
            if (!data) return NULL;
            FiniteFieldRingData* ffData = data;
            return constructFiniteField(ffData->p, ffData->degree);
        }
        case RING_Z: {
            Ring* ring = calloc(1, sizeof(Ring));
            if (!ring) return NULL;
            ring->type = RING_Z;
            ring->isFinite = false;
            return ring;
        }
        case RING_PRODUCT: {
            if (!data) return NULL;
            ProductRingData* productData = data;
            if (!productData->factors || productData->count < 2) return NULL;
            Ring* product = constructProductRing(productData->factors[0], productData->factors[1]);
            if (!product) return NULL;
            for (size_t i = 2; i < productData->count; i++) {
                Ring* next = constructProductRing(product, productData->factors[i]);
                if (!next) {
                    freeRing(product);
                    return NULL;
                }
                product = next;
            }
            return product;
        }
        case RING_QUOTIENT: {
            if (!data) return NULL;
            QuotientRingData* quotientData = data;
            return quotientRing(quotientData->ambient, quotientData->ideal);
        }
        case RING_MATRIX:
            return NULL;
    }

    return NULL;
}

// Construct a Ring from Cayley tables
Ring* constructTableRing(RingElement** elements, int** addTable, int** multTable, int tableLen) {
    if (!elements || !addTable || !multTable) return NULL;
    if (tableLen < 1) return NULL;
    if (!validateRingTables(addTable, multTable, tableLen)) return NULL;

    return constructTableRingSkipValidate(elements, addTable, multTable, tableLen);
}

// Construct a Ring from Cayley tables, skipping the validation step
Ring* constructTableRingSkipValidate(RingElement** elements, int** addTable, int** multTable, int tableLen) {
    if (!elements || !addTable || !multTable) return NULL;
    if (tableLen < 1) return NULL;

    Ring* ring = calloc(1, sizeof(Ring));
    if (!ring) return NULL;

    ring->type = RING_CAYLEY;
    ring->elements = elements;
    ring->card = (size_t)tableLen;
    ring->isFinite = true;
    ring->data.cayley.elements = elements;
    ring->data.cayley.addTable = addTable;
    ring->data.cayley.multTable = multTable;

    for (int i = 0; i < tableLen; i++){
	ring->elements[i]->ring = ring;
	ring->elements[i]->index = i;
	ring->elements[i]->type = RING_ELEM_INDEXED;
	ring->elements[i]->data.indexValue = i;
    }

    return ring;
}

/* ---------- Helpers functions ---------- */

// Recursively collect leaf reprs from a nested product repr, e.g. "((0,1),2)" → "0,1,2"
char* flattenReprInner(const char* s) {
    int len = strlen(s);
    if (s[0] != '(' || s[len-1] != ')') return strdup(s);

    int depth = 0, commaPos = -1;
    for (int i = 1; i < len - 1; i++) {
        if      (s[i] == '(') depth++;
        else if (s[i] == ')') depth--;
        else if (s[i] == ',' && depth == 0) { commaPos = i; break; }
    }
    if (commaPos == -1) return strdup(s);

    int leftLen = commaPos - 1;
    char* left = malloc(leftLen + 1);
    strncpy(left, s + 1, leftLen);
    left[leftLen] = '\0';

    int rightLen = len - commaPos - 2;
    char* right = malloc(rightLen + 1);
    strncpy(right, s + commaPos + 1, rightLen);
    right[rightLen] = '\0';

    char* flatLeft  = flattenReprInner(left);
    char* flatRight = flattenReprInner(right);
    free(left); free(right);

    char* result = malloc(strlen(flatLeft) + strlen(flatRight) + 2);
    sprintf(result, "%s,%s", flatLeft, flatRight);
    free(flatLeft); free(flatRight);
    return result;
}

// Flatten a nested product repr like "((0,1),2)" to "(0,1,2)"
char* flattenRepr(const char* repr) {
    char* inner = flattenReprInner(repr);
    if (!inner) return NULL;
    char* result = malloc(strlen(inner) + 3);
    if (!result) { free(inner); return NULL; }
    sprintf(result, "(%s)", inner);
    free(inner);
    return result;
}

// Helpers for F_{p^k} construction
static int intPow(int base, int exp) {
	int r = 1;
	for (int i = 0; i < exp; i++) r *= base;
	return r;
}

// Reduce polynomial a (length aLen, low coeff first) modulo monic f of degree k.
// f has length k+1 with f[k] == 1. Result written to out (length k).
static void polyRemMod(const int* a, int aLen, const int* f, int k, int p, int* out) {
	int* r = malloc(aLen * sizeof(int));
	if (!r) { for (int i = 0; i < k; i++) out[i] = 0; return; }
	for (int i = 0; i < aLen; i++) r[i] = a[i];
	for (int i = aLen - 1; i >= k; i--) {
		int c = r[i];
		if (c == 0) continue;
		for (int j = 0; j <= k; j++) {
			r[i - k + j] = ((r[i - k + j] - c * f[j]) % p + p) % p;
		}
	}
	for (int i = 0; i < k; i++) out[i] = (i < aLen) ? r[i] : 0;
	free(r);
}

// Returns true iff monic poly f of degree k is irreducible over F_p.
// Brute force: no monic poly of degree 1..k/2 divides f.
static bool isIrreduciblePolyMod(const int* f, int k, int p) {
	if (k < 2) return k == 1;
	int maxDeg = k / 2;
	int* g = malloc((maxDeg + 1) * sizeof(int));
	if (!g) return false;
	int* rem = malloc(k * sizeof(int));
	if (!rem) { free(g); return false; }
	for (int d = 1; d <= maxDeg; d++) {
		int count = intPow(p, d);
		for (int idx = 0; idx < count; idx++) {
			int n = idx;
			for (int i = 0; i < d; i++) { g[i] = n % p; n /= p; }
			g[d] = 1;
			polyRemMod(f, k + 1, g, d, p, rem);
			bool zero = true;
			for (int i = 0; i < d; i++) if (rem[i] != 0) { zero = false; break; }
			if (zero) { free(g); free(rem); return false; }
		}
	}
	free(g); free(rem);
	return true;
}

// Build a polynomial repr "a_0+a_1 x+a_2 x^2+..." in buf (size bufLen), omitting zero terms.
static void polyReprBuf(const int* poly, int k, char* buf, int bufLen) {
	int pos = 0;
	bool first = true;
	buf[0] = '\0';
	for (int j = 0; j < k; j++) {
		if (poly[j] == 0) continue;
		if (!first) pos += snprintf(buf + pos, bufLen - pos, "+");
		if (j == 0) {
			pos += snprintf(buf + pos, bufLen - pos, "%d", poly[j]);
		} else {
			if (poly[j] != 1) pos += snprintf(buf + pos, bufLen - pos, "%d", poly[j]);
			if (j == 1) pos += snprintf(buf + pos, bufLen - pos, "x");
			else pos += snprintf(buf + pos, bufLen - pos, "x^%d", j);
		}
		first = false;
	}
	if (first) snprintf(buf, bufLen, "0");
}

static char* finiteFieldReprFromIndex(long long index, int p, int degree) {
    if (p <= 1 || degree < 1) return NULL;

    int* poly = malloc(degree * sizeof(int));
    if (!poly) return NULL;

    long long n = index;
    for (int i = 0; i < degree; i++) {
        poly[i] = (int)(n % p);
        n /= p;
    }

    int bufLen = 16 * degree + 16;
    char* repr = malloc(bufLen);
    if (!repr) {
        free(poly);
        return NULL;
    }

    polyReprBuf(poly, degree, repr, bufLen);
    free(poly);
    return repr;
}

static int finiteFieldAddIndex(long long left, long long right, int p, int degree) {
    int index = 0;
    int place = 1;

    for (int i = 0; i < degree; i++) {
        int a = (int)(left % p);
        int b = (int)(right % p);
        int c = (a + b) % p;
        index += c * place;
        place *= p;
        left /= p;
        right /= p;
    }

    return index;
}

static int finiteFieldMultIndex(long long left, long long right, FiniteFieldRingData* ff) {
    if (!ff || !ff->modulus || ff->p <= 1 || ff->degree < 1) return -1;

    int p = ff->p;
    int degree = ff->degree;
    int* a = calloc(degree, sizeof(int));
    int* b = calloc(degree, sizeof(int));
    int* prod = calloc(2 * degree - 1, sizeof(int));
    int* rem = calloc(degree, sizeof(int));
    if (!a || !b || !prod || !rem) {
        free(a); free(b); free(prod); free(rem);
        return -1;
    }

    for (int i = 0; i < degree; i++) {
        a[i] = (int)(left % p);
        b[i] = (int)(right % p);
        left /= p;
        right /= p;
    }

    for (int i = 0; i < degree; i++) {
        if (a[i] == 0) continue;
        for (int j = 0; j < degree; j++) {
            prod[i + j] = (prod[i + j] + a[i] * b[j]) % p;
        }
    }

    polyRemMod(prod, 2 * degree - 1, ff->modulus, degree, p, rem);

    int index = 0;
    for (int i = degree - 1; i >= 0; i--) index = index * p + rem[i];

    free(a); free(b); free(prod); free(rem);
    return index;
}

static void freeGroupConstructionData(GroupElement** elements, int** table, int card) {
    if (elements) {
        for (int i = 0; i < card; i++) freeGroupElement(elements[i]);
        free(elements);
    }
    if (table) {
        for (int i = 0; i < card; i++) free(table[i]);
        free(table);
    }
}

/* ---------- Permutation helpers ---------- */

// Return true if p is prime, else false
bool isPrime(int p) {
    if (p < 2) return false;
    if (p == 2 || p == 3) return true;

    int max = sqrt(p);
    int prime = true;
    for (int i = 2; i <= max; i++) {
        if (p % i == 0) {
            prime = false;
            break;
        }
    }

    return prime;
}

// Compute n!
static int factorial(int n) {
    int result = 1;
    if (n < 0) return 0;
    for (int i = 2; i <= n; i++) {
        if (result > INT_MAX / i) return 0;
        result *= i;
    }
    return result;
}

// Convert a permutation to its lexicographic index
int permToIndex(int* perm, int n) {
    int index = 0;
    for (int i = 0; i < n; i++) {
        int count = 0;
        for (int j = i + 1; j < n; j++) {
            if (perm[j] < perm[i]) count++;
        }
        int f = 1;
        for (int j = 1; j <= n - 1 - i; j++) f *= j;
        index += count * f;
    }
    return index;
}

// Return 0 for even permutation, 1 for odd
int permSign(int* perm, int n) {
    int sign = 0;
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            if (perm[i] > perm[j]) sign ^= 1;
        }
    }
    return sign;
}

// Advance to the next permutation in lexicographic order (in-place)
bool nextPermutation(int* perm, int n) {
    int i = n - 2;
    while (i >= 0 && perm[i] >= perm[i + 1]) i--;
    if (i < 0) return false;
    int j = n - 1;
    while (perm[j] <= perm[i]) j--;
    int tmp = perm[i]; perm[i] = perm[j]; perm[j] = tmp;
    for (int l = i + 1, r = n - 1; l < r; l++, r--) {
        tmp = perm[l]; perm[l] = perm[r]; perm[r] = tmp;
    }
    return true;
}

// Build a repr string for a permutation in one-line notation, e.g. "[0,2,1]"
char* permRepr(int* perm, int n) {
    char* repr = malloc(n * 4 + 4);
    if (!repr) return NULL;
    int pos = sprintf(repr, "[");
    for (int i = 0; i < n; i++) {
        if (i > 0) pos += sprintf(repr + pos, ",");
        pos += sprintf(repr + pos, "%d", perm[i]);
    }
    sprintf(repr + pos, "]");
    return repr;
}

/* ---------- Common constructors ---------- */

// Construct the Group Z/Zn = {0, 1, 2, ..., n-1}
Group* constructZnGroup(int n) {
    if (n < 1) return NULL;

    Group* G = calloc(1, sizeof(Group));
    if (!G) return NULL;
    G->type = GROUP_ZN;
    G->card = (size_t)n;
    G->isFinite = true;
    G->data.zn.modulus = n;
    G->elements = calloc((size_t)n, sizeof(GroupElement*));
    if (!G->elements) {
        free(G);
        return NULL;
    }

    for (int i = 0; i < n; i++) {
        long long value = i;
        G->elements[i] = constructGroupElement(G, &value);
        if (!G->elements[i]) {
            freeGroup(G);
            return NULL;
        }
        G->elements[i]->index = i;
    }

	return G;
}

// Construct the Group Zn_1 x Zn_2 x ... x Zn_k
Group* constructZnProductGroup(int* vals, int k) {
    if (!vals) return NULL;
	if (k < 1) return NULL;
	if (k == 1) return constructZnGroup(vals[0]);

	// Construct the component groups
	Group** components = malloc(k * sizeof(Group*));
	if (!components) return NULL;
	for (int i = 0; i < k; i++) {
		components[i] = constructZnGroup(vals[i]);
		if (!components[i]) {
			for (int j = 0; j < i; j++) freeGroup(components[j]);
			free(components);
			return NULL;
		}
	}

	// Construct the cross product recursively
	Group* product = components[0];
	for (int i = 1; i < k; i++) {
		Group* next = constructProductGroup(product, components[i]);
		if (!next) {
			for (int j = 0; j < k; j++) freeGroup(components[j]);
			free(components);
			return NULL;
		}
		product = next;
	}

	// Flatten element reprs to (a,b,c,...) form and set the group pointer
	for (int i = 0; i < product->card; i++) {
		char* flat = flattenRepr(product->elements[i]->repr);
		if (flat) { free(product->elements[i]->repr); product->elements[i]->repr = flat; }
		product->elements[i]->group = product;
	}
	free(components);

	return product;
}

// Construct the group Sn
Group* constructSymmetricGroup(int n) {
    if (n < 1) return NULL;

    int card = factorial(n);
    if (card < 1) return NULL;

    Group* G = calloc(1, sizeof(Group));
    if (!G) return NULL;
    G->type = GROUP_SYMMETRIC;
    G->card = (size_t)card;
    G->isFinite = true;
    G->data.permutation.degree = (size_t)n;
    G->elements = calloc((size_t)card, sizeof(GroupElement*));
    if (!G->elements) {
        free(G);
        return NULL;
    }

    // Generate all permutations in lexicographic order
    int** perms = malloc(card * sizeof(int*));
    if (!perms) {
        freeGroup(G);
        return NULL;
    }
    for (int p = 0; p < card; p++) {
        perms[p] = malloc(n * sizeof(int));
        if (!perms[p]) {
            for (int j = 0; j < p; j++) free(perms[j]);
            free(perms);
            freeGroup(G);
            return NULL;
        }
        if (p == 0) {
            for (int i = 0; i < n; i++) perms[0][i] = i;
        } else {
            memcpy(perms[p], perms[p - 1], n * sizeof(int));
            nextPermutation(perms[p], n);
        }
    }

    long long* permData = malloc((size_t)n * sizeof(long long));
    if (!permData) {
        for (int i = 0; i < card; i++) free(perms[i]);
        free(perms);
        freeGroup(G);
        return NULL;
    }
    for (int p = 0; p < card; p++) {
        for (int i = 0; i < n; i++) permData[i] = perms[p][i];
        G->elements[p] = constructGroupElement(G, permData);
        if (!G->elements[p]) {
            free(permData);
            for (int j = 0; j < card; j++) free(perms[j]);
            free(perms);
            freeGroup(G);
            return NULL;
        }
        G->elements[p]->index = p;
    }
    free(permData);
    for (int i = 0; i < card; i++) free(perms[i]);
    free(perms);

    return G;
}

// Construct the group An
Group* constructAlternatingGroup(int n) {
    if (n < 1) return NULL;

    int fullCard = factorial(n);
    if (fullCard < 1) return NULL;
    int card = fullCard / 2;
    if (n <= 2) card = 1;

    Group* G = calloc(1, sizeof(Group));
    if (!G) return NULL;
    G->type = GROUP_ALTERNATING;
    G->card = (size_t)card;
    G->isFinite = true;
    G->data.permutation.degree = (size_t)n;
    G->elements = calloc((size_t)card, sizeof(GroupElement*));
    if (!G->elements) {
        free(G);
        return NULL;
    }

    // Generate all permutations in lexicographic order
    int** allPerms = malloc(fullCard * sizeof(int*));
    if (!allPerms) {
        freeGroup(G);
        return NULL;
    }
    for (int p = 0; p < fullCard; p++) {
        allPerms[p] = malloc(n * sizeof(int));
        if (!allPerms[p]) {
            for (int j = 0; j < p; j++) free(allPerms[j]);
            free(allPerms);
            freeGroup(G);
            return NULL;
        }
        if (p == 0) {
            for (int i = 0; i < n; i++) allPerms[0][i] = i;
        } else {
            memcpy(allPerms[p], allPerms[p - 1], n * sizeof(int));
            nextPermutation(allPerms[p], n);
        }
    }

    long long* permData = malloc((size_t)n * sizeof(long long));
    if (!permData) {
        for (int i = 0; i < fullCard; i++) free(allPerms[i]);
        free(allPerms);
        freeGroup(G);
        return NULL;
    }
    int evenCount = 0;
    for (int i = 0; i < fullCard; i++) {
        if (permSign(allPerms[i], n) == 0) {
            for (int j = 0; j < n; j++) permData[j] = allPerms[i][j];
            G->elements[evenCount] = constructGroupElement(G, permData);
            if (!G->elements[evenCount]) {
                free(permData);
                for (int j = 0; j < fullCard; j++) free(allPerms[j]);
                free(allPerms);
                freeGroup(G);
                return NULL;
            }
            G->elements[evenCount]->index = evenCount;
            evenCount++;
            if (evenCount == card) break;
        }
    }
    free(permData);
    for (int i = 0; i < fullCard; i++) free(allPerms[i]);
    free(allPerms);

    if (evenCount != card) {
        freeGroup(G);
        return NULL;
    }

    return G;
}

// Construct the Dihedral group Dn
Group* constructDihedralGroup(int n) {
    if (n < 1) return NULL;

    int card = 2 * n;
    Group* G = calloc(1, sizeof(Group));
    if (!G) return NULL;
    G->type = GROUP_DIHEDRAL;
    G->card = (size_t)card;
    G->isFinite = true;
    G->data.dihedral.n = n;
    G->elements = calloc((size_t)card, sizeof(GroupElement*));
    if (!G->elements) {
        free(G);
        return NULL;
    }

    for (int i = 0; i < card; i++) {
        DihedralElementData data = { i % n, i >= n };
        G->elements[i] = constructGroupElement(G, &data);
        if (!G->elements[i]) {
            freeGroup(G);
            return NULL;
        }
        G->elements[i]->index = i;
    }

    return G;
}

// Construct the Quaternion group Q8 = {1, -1, i, -i, j, -j, k, -k}
Group* constructQ8(void) {
    int card = 8;

    const char* reprs[] = {"1", "-1", "i", "-i", "j", "-j", "k", "-k"};

    GroupElement** elements = malloc(card * sizeof(GroupElement*));
    if (!elements) return NULL;
    for (int i = 0; i < card; i++) {
        elements[i] = constructGroupElement(NULL, (char*)reprs[i]);
        if (!elements[i]) {
            for (int j = 0; j < i; j++) freeGroupElement(elements[j]);
            free(elements);
            return NULL;
        }
    }

    // Build the Cayley table
    int neg[] = {1, 0, 3, 2, 5, 4, 7, 6};

    // Multiplication by i,j,k on each basis element (from the left)
    int imult[] = {2, 3, 1, 0, 6, 7, 5, 4};
    int jmult[] = {4, 5, 7, 6, 1, 0, 2, 3};
    int kmult[] = {6, 7, 4, 5, 3, 2, 1, 0};

    int** table = malloc(card * sizeof(int*));
    if (!table) {
        for (int i = 0; i < card; i++) freeGroupElement(elements[i]);
        free(elements);
        return NULL;
    }
    for (int a = 0; a < card; a++) {
        table[a] = malloc(card * sizeof(int));
        if (!table[a]) {
            for (int j = 0; j < a; j++) free(table[j]);
            free(table);
            for (int j = 0; j < card; j++) freeGroupElement(elements[j]);
            free(elements);
            return NULL;
        }
        for (int b = 0; b < card; b++) {
            switch (a) {
                case 0: table[a][b] = b;              break;
                case 1: table[a][b] = neg[b];         break;
                case 2: table[a][b] = imult[b];       break;
                case 3: table[a][b] = neg[imult[b]];  break;
                case 4: table[a][b] = jmult[b];       break;
                case 5: table[a][b] = neg[jmult[b]];  break;
                case 6: table[a][b] = kmult[b];       break;
                case 7: table[a][b] = neg[kmult[b]];  break;
            }
        }
    }

    // Construct the group
    Group* G = constructTableGroupSkipValidate(elements, table, card);
    if (!G) {
        for (int i = 0; i < card; i++) {
            freeGroupElement(elements[i]);
            free(table[i]);
        }
        free(elements);
        free(table);
        return NULL;
    }
    for (int i = 0; i < card; i++) elements[i]->group = G;

    return G;
}

// Construct the Ring Z/Zn = {0, 1, 2, ..., n-1}
Ring* constructZnRing(int n) {
    if (n < 1) return NULL;

    Ring* R = calloc(1, sizeof(Ring));
    if (!R) return NULL;
    R->type = RING_ZN;
    R->card = (size_t)n;
    R->isFinite = true;
    R->data.zn.modulus = n;
    R->elements = calloc((size_t)n, sizeof(RingElement*));
    if (!R->elements) {
        free(R);
        return NULL;
    }

    for (int i = 0; i < n; i++) {
        long long value = i;
        R->elements[i] = constructRingElement(R, &value);
        if (!R->elements[i]) {
            freeRing(R);
            return NULL;
        }
        R->elements[i]->index = i;
    }

	return R;
}

// Construct the Ring Zn_1 x Zn_2 x ... x Zn_k
Ring* constructZnProductRing(int* vals, int k) {
    if (!vals) return NULL;
	if (k < 1) return NULL;
	if (k == 1) return constructZnRing(vals[0]);

	// Construct the component rings
	Ring** components = malloc(k * sizeof(Ring*));
	if (!components) return NULL;
	for (int i = 0; i < k; i++) {
		components[i] = constructZnRing(vals[i]);
		if (!components[i]) {
			for (int j = 0; j < i; j++) freeRing(components[j]);
			free(components);
			return NULL;
		}
	}

	// Construct the product ring recursively
	Ring* product = components[0];
	for (int i = 1; i < k; i++) {
		Ring* next = constructProductRing(product, components[i]);
		if (!next) {
			for (int j = 0; j < k; j++) freeRing(components[j]);
			free(components);
			return NULL;
		}
		product = next;
	}

	// Flatten element reprs to (a,b,c,...) form and set the ring pointer
	for (int i = 0; i < product->card; i++) {
		char* flat = flattenRepr(product->elements[i]->repr);
		if (flat) { free(product->elements[i]->repr); product->elements[i]->repr = flat; }
		product->elements[i]->ring = product;
	}
	free(components);

	return product;
}

// Construct a finite field with p elements
Ring* primeFiniteField(int p) {
    if (!isPrime(p)) return NULL;

    return constructFiniteField(p, 1);
}

// Construct the finite field F_{p^k} = F_p[x] / (f(x)) for an irreducible f.
Ring* constructFiniteField(int p, int k) {
	if (k == 0) return trivialRing();
	if (k < 1) return NULL;
	if (!isPrime(p)) return NULL;

	int* f = malloc((k + 1) * sizeof(int));
	if (!f) return NULL;
	f[k] = 1;
    if (k == 1) {
        f[0] = 0;
    } else {
	    // Find an irreducible monic polynomial f of degree k over F_p
	    int numMonic = intPow(p, k);
	    bool found = false;
	    for (int idx = 0; idx < numMonic; idx++) {
		    int n = idx;
		    for (int i = 0; i < k; i++) { f[i] = n % p; n /= p; }
		    if (isIrreduciblePolyMod(f, k, p)) { found = true; break; }
	    }
	    if (!found) { free(f); return NULL; }
    }

	int N = intPow(p, k);
    if (N < 1) { free(f); return NULL; }

    Ring* R = calloc(1, sizeof(Ring));
    if (!R) { free(f); return NULL; }
    R->type = RING_FF;
    R->card = (size_t)N;
    R->isFinite = true;
    R->data.ff.p = p;
    R->data.ff.degree = k;
    R->data.ff.modulus = f;
    R->elements = calloc((size_t)N, sizeof(RingElement*));
    if (!R->elements) {
        freeRing(R);
        return NULL;
    }

	for (int i = 0; i < N; i++) {
        long long value = i;
        R->elements[i] = constructRingElement(R, &value);
        if (!R->elements[i]) {
            freeRing(R);
            return NULL;
        }
        R->elements[i]->index = i;
    }

	return R;
}

// Construct the additive group of a Ring
Group* constructAddGroup(Ring* R) {
    if (!R) return NULL;
    if (!R->isFinite || !R->elements) return NULL;

    int card = R->card;
    GroupElement** elements = calloc((size_t)card, sizeof(GroupElement*));
    int** table = calloc((size_t)card, sizeof(int*));
    if (!elements || !table) {
        freeGroupConstructionData(elements, table, card);
        return NULL;
    }

    for (int i = 0; i < card; i++) {
        const char* repr = (R->elements && R->elements[i] && R->elements[i]->repr) ? R->elements[i]->repr : "?";
        elements[i] = constructGroupElement(NULL, (char*)repr);
        table[i] = malloc((size_t)card * sizeof(int));
        if (!elements[i] || !table[i]) {
            freeGroupConstructionData(elements, table, card);
            return NULL;
        }
        for (int j = 0; j < card; j++) {
            RingElement* sum = ringAdd(R->elements[i], R->elements[j]);
            if (!sum || sum->index < 0 || sum->index >= card) {
                freeGroupConstructionData(elements, table, card);
                return NULL;
            }
            table[i][j] = sum->index;
        }
    }

    Group* G = constructTableGroupSkipValidate(elements, table, card);
    if (!G) {
        freeGroupConstructionData(elements, table, card);
        return NULL;
    }
    return G;
}

static bool ringElementActsAsMultIdentity(Ring* R, RingElement* candidate) {
    if (!R || !candidate || !R->isFinite || !R->elements) return false;

    for (int i = 0; i < R->card; i++) {
        RingElement* left = ringMult(candidate, R->elements[i]);
        RingElement* right = ringMult(R->elements[i], candidate);
        if (!cmpRingElements(left, R->elements[i]) || !cmpRingElements(right, R->elements[i])) return false;
    }

    return true;
}

// Construct the multiplicative unit group of a Ring
Group* constructUnitGroup(Ring* R) {
    if (!R) return NULL;
    if (!R->isFinite || !R->elements) return NULL;
    if (R->card < 2) return NULL;

    int identityIndex = -1;
    for (int i = 0; i < R->card; i++) {
        if (ringElementActsAsMultIdentity(R, R->elements[i])) {
            identityIndex = i;
            break;
        }
    }
    if (identityIndex < 0) return NULL;

    int* unitIndices = malloc((size_t)R->card * sizeof(int));
    int* indexMap = malloc((size_t)R->card * sizeof(int));
    if (!unitIndices || !indexMap) {
        free(unitIndices);
        free(indexMap);
        return NULL;
    }

    for (int i = 0; i < R->card; i++) indexMap[i] = -1;

    int unitCount = 0;
    unitIndices[unitCount] = identityIndex;
    indexMap[identityIndex] = unitCount++;
    for (int i = 0; i < R->card; i++) {
        if (i == identityIndex) continue;
        bool hasInverse = false;
        for (int j = 0; j < R->card; j++) {
            RingElement* left = ringMult(R->elements[i], R->elements[j]);
            RingElement* right = ringMult(R->elements[j], R->elements[i]);
            if (left && right && left->index == identityIndex && right->index == identityIndex) {
                hasInverse = true;
                break;
            }
        }
        if (hasInverse) {
            indexMap[i] = unitCount;
            unitIndices[unitCount++] = i;
        }
    }

    GroupElement** elements = calloc((size_t)unitCount, sizeof(GroupElement*));
    int** table = calloc((size_t)unitCount, sizeof(int*));
    if (!elements || !table) {
        free(unitIndices);
        free(indexMap);
        freeGroupConstructionData(elements, table, unitCount);
        return NULL;
    }

    for (int i = 0; i < unitCount; i++) {
        const char* repr = (R->elements && R->elements[unitIndices[i]] && R->elements[unitIndices[i]]->repr)
            ? R->elements[unitIndices[i]]->repr : "?";
        elements[i] = constructGroupElement(NULL, (char*)repr);
        table[i] = malloc((size_t)unitCount * sizeof(int));
        if (!elements[i] || !table[i]) {
            free(unitIndices);
            free(indexMap);
            freeGroupConstructionData(elements, table, unitCount);
            return NULL;
        }
        for (int j = 0; j < unitCount; j++) {
            RingElement* productElement = ringMult(R->elements[unitIndices[i]], R->elements[unitIndices[j]]);
            if (!productElement) {
                free(unitIndices);
                free(indexMap);
                freeGroupConstructionData(elements, table, unitCount);
                return NULL;
            }
            int product = productElement->index;
            if (product < 0 || product >= R->card || indexMap[product] < 0) {
                free(unitIndices);
                free(indexMap);
                freeGroupConstructionData(elements, table, unitCount);
                return NULL;
            }
            table[i][j] = indexMap[product];
        }
    }

    free(unitIndices);
    free(indexMap);
    Group* G = constructTableGroupSkipValidate(elements, table, unitCount);
    if (!G) {
        freeGroupConstructionData(elements, table, unitCount);
        return NULL;
    }
    return G;
}

/* ---------- Compare methods ---------- */

// Compare two GroupElements
bool cmpGroupElements(GroupElement* g, GroupElement* h) {
    if (!g || !h) return false;

    if (g->group != h->group) return false;
    if (g->index != h->index) return false;
    return true;
}

// Determine whether G and H point to the same Group
bool cmpGroups(Group* G, Group* H) {
    if (!G || !H) return false;

    return G == H;
}

// Determine whether two subgroups H and K are the same
bool cmpSubgroups(SubGroup* H, SubGroup* K) {
    if (!H || !K) return false;
    if (H->ambient != K->ambient) return false;
    if (H->card != K->card) return false;
    if (H->type != SUBGROUP_INDEXED || K->type != SUBGROUP_INDEXED) return false;

    for (int i = 0; i < H->card; i++) {
        if (!indexInList(H->data.indexed.indices[i], K->data.indexed.indices, (int)K->card)) return false;
    }
    return true;
}

// Determine whether two cosets are the same
bool cmpGroupCosets(GroupCoset* A, GroupCoset* B) {
	if (!A || !B) return false;
	if (A->group != B->group) return false;
	if (A->subgroup != B->subgroup) return false;
	if (A->isLeft != B->isLeft) return false;

    for (int i = 0; i < A->subgroup->card; i++) {
        bool found = false;
        for (int j = 0; j < B->subgroup->card; j++) {
            if (A->data.indexed.indices[i] == B->data.indexed.indices[j]) {
                found = true;
                break;
            }
        }
        if (found == false) return false;
    }

	return true;
}

// Compare two RingElements
bool cmpRingElements(RingElement* x, RingElement* y) {
    if (!x || !y) return false;

    if (x->ring != y->ring) return false;
    if (x->index != y->index) return false;
    return true;
}

// Determine whether R and S point to the same Ring
bool cmpRings(Ring* R, Ring* S) {
    if (!R || !S) return false;

    return R == S;
}

/* ---------- Basic properties ----------*/

// Return true if g is in G, else false
bool isInGroup(Group* G, GroupElement* g) {
    if (!G || !g) return false;
    if (!G->isFinite || !G->elements) return false;

    for (int i = 0; i < G->card; i++) {
        if (cmpGroupElements(g, G->elements[i])) return true;
    }

    return false;
}

// Return true if g is the identity of G, else false
bool isGroupIdentity(Group* G, GroupElement* g) {
    if (!G || !g) return false;
    if (g->group != G) return false;
    if (G->isFinite) return g->index == 0;

    return g == G->elements[0];
}

// Return the identity element of G
GroupElement* groupIdentity(Group* G) {
    if (!G) return NULL;
    if (!G->elements) return NULL;

    return G->elements[0];
}

// Return true if x is the additive identity of R, else false
bool isRingAddIdentity(Ring* R, RingElement* x) {
    if (!R || !x) return false;

    return x == R->elements[0];
}

// Return the additive identity element of R
RingElement* ringAddIdentity(Ring* R) {
    if (!R) return NULL;

    return R->elements[0];
}

// Return true if R has a mult identity, else false
bool hasMultIdentity(Ring* R) {
    if (!R) return false;
    for (int i = 0; i < R->card; i++) {
        if (ringElementActsAsMultIdentity(R, R->elements[i])) return true;
    }

    return false;
}

// Return true if x is the additive identity of R, else false
bool isRingMultIdentity(Ring* R, RingElement* x) {
    if (!R || !x) return false;

    return ringElementActsAsMultIdentity(R, x);
}

// Return the additive identity element of R
RingElement* ringMultIdentity(Ring* R) {
    if (!R) return NULL;

    for (int i = 0; i < R->card; i++) {
        if (ringElementActsAsMultIdentity(R, R->elements[i])) return R->elements[i];
    }

    return NULL;
}

// Return true if g and h commute, else false
bool elementsCommute(GroupElement* g, GroupElement* h) {
    if (!g || !h) return false;
    if (g->group != h->group) return false;

    return cmpGroupElements(groupMult(g, h), groupMult(h, g));
}

// Returns true if the group is trivial, else false
bool isTrivialGroup(Group* G) {
	if (!G) return false;

	return G->card == 1;
}

// Returns true if the ring is trivial, else false
bool isTrivialRing(Ring* R) {
	if (!R) return false;

	return R->card == 1;
}

// Returns the order of g in G
int elementOrder(Group* G, GroupElement* g) {
	if (!g) return -1;
    if (!cmpGroups(G, g->group)) return -1;

	int order = 1;
	GroupElement* prod = g;
	while (!isGroupIdentity(G, prod)) {
	    prod = groupMult(prod, g);
	    order++;
	    if (order > G->card) return -1; // Just in case: should not happen
	}

	return order;
}

// Returns the additive order of x in the additive group of R
int additiveOrder(Ring* R, RingElement* x) {
    if (!x) return -1;
    if (!cmpRings(R, x->ring)) return -1;

    int order = 1;
    RingElement* sum = x;
    while (!isRingAddIdentity(R, sum)) {
        sum = ringAdd(sum, x);
        order++;
        if (order > R->card) return -1;
    }

    return order;
}

// Returns the multiplicative order of x in the unit group of R
int multiplicativeOrder(Ring* R, RingElement* x) {
    if (!x) return -1;
    if (!cmpRings(R, x->ring)) return -1;
    if (!hasMultIdentity(R)) return -1;
    if (!ringMultInverse(x)) return -1;

    int order = 1;
    RingElement* prod = x;
    while (!isRingMultIdentity(R, prod)) {
        prod = ringMult(prod, x);
        order++;
        if (order > R->card) return -1;
    }

    return order;
}

// Return true if G is simple, false otherwise
bool isSimple(Group* G) {
    if (!G) return false;
    if (G->card == 1) return false;

    // Iterate over all subgroups and check that the only normal ones are trivial and the whole group
    int count = 0;
    SubGroup** subgroups = listAllSubgroups(G, &count);
    if (!subgroups) return false;

    bool simple = true;
    for (int i = 0; i < count; i++) {
        SubGroup* H = subgroups[i];
        if (H->card == 1 || H->card == G->card) continue;
        if (isNormalSubgroup(G, H)) {
            simple = false;
            break;
        }
    }

    for (int i = 0; i < count; i++) freeSubgroup(subgroups[i]);
    free(subgroups);
    return simple;
}

/* ---------- Basic operations ---------- */

static GroupElement* cayleyGroupMult(GroupElement* g, GroupElement* h) {
    Group* G = g->group;
    int index = G->data.cayley.table[g->index][h->index];
    return G->elements[index];
}

static GroupElement* znGroupMult(GroupElement* g, GroupElement* h) {
    long long value = normalizeMod(g->data.znVal + h->data.znVal, g->group->data.zn.modulus);
    return g->group->elements[value];
}

static GroupElement* zGroupMult(GroupElement* g, GroupElement* h) {
    long long value = g->data.integer + h->data.integer;
    return constructGroupElement(g->group, &value);
}

static GroupElement* permutationGroupMult(GroupElement* g, GroupElement* h) {
    size_t degree = g->group->data.permutation.degree;
    long long* perm = malloc(degree * sizeof(long long));
    if (!perm) return NULL;

    for (size_t i = 0; i < degree; i++) {
        long long hi = h->data.perm[i];
        if (hi < 0 || (size_t)hi >= degree) {
            free(perm);
            return NULL;
        }
        perm[i] = g->data.perm[hi];
    }

    GroupElement* product = constructGroupElement(g->group, perm);
    free(perm);
    return canonicalGroupElement(g->group, product);
}

static GroupElement* dihedralGroupMult(GroupElement* g, GroupElement* h) {
    long long n = g->group->data.dihedral.n;
    DihedralElementData value;
    value.b = g->data.dihedral.b ^ h->data.dihedral.b;
    if (h->data.dihedral.b) value.a = normalizeMod(h->data.dihedral.a - g->data.dihedral.a, n);
    else value.a = normalizeMod(g->data.dihedral.a + h->data.dihedral.a, n);

    return canonicalGroupElement(g->group, constructGroupElement(g->group, &value));
}

static GroupElement* productGroupMult(GroupElement* g, GroupElement* h) {
    if (g->data.product.count != h->data.product.count) return NULL;

    size_t count = g->data.product.count;
    GroupElement** factors = malloc(count * sizeof(GroupElement*));
    if (!factors) return NULL;

    for (size_t i = 0; i < count; i++) {
        factors[i] = groupMult(g->data.product.factors[i], h->data.product.factors[i]);
        if (!factors[i]) {
            free(factors);
            return NULL;
        }
    }

    ProductGroupElementData data = { factors, count };
    GroupElement* product = constructGroupElement(g->group, &data);
    free(factors);
    return canonicalGroupElement(g->group, product);
}

static GroupElement* matrixGroupMult(GroupElement* g, GroupElement* h) {
    Matrix* productMatrix = multiplyMatrices(g->data.matrix, h->data.matrix);
    if (!productMatrix) return NULL;

    return constructGroupElement(g->group, productMatrix);
}

static RingElement* cayleyRingAdd(RingElement* x, RingElement* y) {
    Ring* R = x->ring;
    int index = R->data.cayley.addTable[x->index][y->index];
    return R->elements[index];
}

static RingElement* cayleyRingMult(RingElement* x, RingElement* y) {
    Ring* R = x->ring;
    int index = R->data.cayley.multTable[x->index][y->index];
    return R->elements[index];
}

static RingElement* znRingAdd(RingElement* x, RingElement* y) {
    long long value = normalizeMod(x->data.znVal + y->data.znVal, x->ring->data.zn.modulus);
    return x->ring->elements[value];
}

static RingElement* znRingMult(RingElement* x, RingElement* y) {
    long long value = normalizeMod(x->data.znVal * y->data.znVal, x->ring->data.zn.modulus);
    return x->ring->elements[value];
}

static RingElement* zRingAdd(RingElement* x, RingElement* y) {
    long long value = x->data.integer + y->data.integer;
    return constructRingElement(x->ring, &value);
}

static RingElement* zRingMult(RingElement* x, RingElement* y) {
    long long value = x->data.integer * y->data.integer;
    return constructRingElement(x->ring, &value);
}

static RingElement* finiteFieldRingAdd(RingElement* x, RingElement* y) {
    long long value = finiteFieldAddIndex(x->data.znVal, y->data.znVal, x->ring->data.ff.p, x->ring->data.ff.degree);
    return x->ring->elements[value];
}

static RingElement* finiteFieldRingMult(RingElement* x, RingElement* y) {
    int value = finiteFieldMultIndex(x->data.znVal, y->data.znVal, &x->ring->data.ff);
    if (value < 0) return NULL;
    return x->ring->elements[value];
}

static RingElement* productRingAdd(RingElement* x, RingElement* y) {
    if (x->data.product.count != y->data.product.count) return NULL;

    size_t count = x->data.product.count;
    RingElement** factors = malloc(count * sizeof(RingElement*));
    if (!factors) return NULL;

    for (size_t i = 0; i < count; i++) {
        factors[i] = ringAdd(x->data.product.factors[i], y->data.product.factors[i]);
        if (!factors[i]) {
            free(factors);
            return NULL;
        }
    }

    ProductRingElementData data = { factors, count };
    RingElement* sum = constructRingElement(x->ring, &data);
    free(factors);
    return canonicalRingElement(x->ring, sum);
}

static RingElement* productRingMult(RingElement* x, RingElement* y) {
    if (x->data.product.count != y->data.product.count) return NULL;

    size_t count = x->data.product.count;
    RingElement** factors = malloc(count * sizeof(RingElement*));
    if (!factors) return NULL;

    for (size_t i = 0; i < count; i++) {
        factors[i] = ringMult(x->data.product.factors[i], y->data.product.factors[i]);
        if (!factors[i]) {
            free(factors);
            return NULL;
        }
    }

    ProductRingElementData data = { factors, count };
    RingElement* product = constructRingElement(x->ring, &data);
    free(factors);
    return canonicalRingElement(x->ring, product);
}

static RingElement* matrixRingAdd(RingElement* x, RingElement* y) {
    Matrix* sumMatrix = addMatrices(x->data.matrix, y->data.matrix);
    if (!sumMatrix) return NULL;

    return constructRingElement(x->ring, sumMatrix);
}

static RingElement* matrixRingMult(RingElement* x, RingElement* y) {
    Matrix* productMatrix = multiplyMatrices(x->data.matrix, y->data.matrix);
    if (!productMatrix) return NULL;

    return constructRingElement(x->ring, productMatrix);
}

// Return the product of two elements of a Group
GroupElement* groupMult(GroupElement* g, GroupElement* h) {
    if (!g || !h) return NULL;
    if (!g->group) return NULL;
    if (g->group != h->group) return NULL;

    switch (g->group->type) {
        case GROUP_CAYLEY:
            return cayleyGroupMult(g, h);
        case GROUP_ZN:
            return znGroupMult(g, h);
        case GROUP_Z:
            return zGroupMult(g, h);
        case GROUP_SYMMETRIC:
        case GROUP_ALTERNATING:
            return permutationGroupMult(g, h);
        case GROUP_DIHEDRAL:
            return dihedralGroupMult(g, h);
        case GROUP_PRODUCT:
            return productGroupMult(g, h);
        case GROUP_QUOTIENT:
            return cayleyGroupMult(g, h);
        case GROUP_MATRIX:
            return matrixGroupMult(g, h);
    }

    return NULL;
}

// Return the sum of two elements of a Ring
RingElement* ringAdd(RingElement* x, RingElement* y) {
    if (!x || !y) return NULL;
    if (x->ring != y->ring) return NULL;

    switch (x->ring->type) {
        case RING_CAYLEY:
            return cayleyRingAdd(x, y);
        case RING_ZN:
            return znRingAdd(x, y);
        case RING_Z:
            return zRingAdd(x, y);
        case RING_FF:
            return finiteFieldRingAdd(x, y);
        case RING_PRODUCT:
            return productRingAdd(x, y);
        case RING_QUOTIENT:
            return cayleyRingAdd(x, y);
        case RING_MATRIX:
            return matrixRingAdd(x, y);
    }

    return NULL;
}

// Return the product of two elements of a Ring
RingElement* ringMult(RingElement* x, RingElement* y) {
    if (!x || !y) return NULL;
    if (x->ring != y->ring) return NULL;

    switch (x->ring->type) {
        case RING_CAYLEY:
            return cayleyRingMult(x, y);
        case RING_ZN:
            return znRingMult(x, y);
        case RING_Z:
            return zRingMult(x, y);
        case RING_FF:
            return finiteFieldRingMult(x, y);
        case RING_PRODUCT:
            return productRingMult(x, y);
        case RING_QUOTIENT:
            return cayleyRingMult(x, y);
        case RING_MATRIX:
            return matrixRingMult(x, y);
    }

    return NULL;
}

// Return the inverse of an element of a Group
GroupElement* groupInverse(GroupElement* g) {
    if (!g) return NULL;

    Group* G = g->group;
    if (!G || !G->isFinite || !G->elements) return NULL;
    for (int i = 0; i < G->card; i++) {
	    if (cmpGroupElements(groupMult(g, G->elements[i]), groupIdentity(G)) && cmpGroupElements(groupMult(G->elements[i], g), groupIdentity(G))) return G->elements[i];
    }

    // Should not happen: just in case
    return NULL;
}

// Return the additive inverse of an element of a Ring
RingElement* ringAddInverse(RingElement* x) {
    if (!x) return NULL;

    Ring* R = x->ring;
    for (int i = 0; i < R->card; i++) {
	    if (cmpRingElements(ringAdd(x, R->elements[i]), ringAddIdentity(R)) && cmpRingElements(ringAdd(R->elements[i], x), ringAddIdentity(R))) return R->elements[i];
    }

    // Should not happen: just in case
    return NULL;
}

// Return the additive inverse of an element of a Ring
RingElement* ringMultInverse(RingElement* x) {
    if (!x) return NULL;

    Ring* R = x->ring;
    for (int i = 0; i < R->card; i++) {
	    if (cmpRingElements(ringMult(x, R->elements[i]), ringMultIdentity(R)) && cmpRingElements(ringMult(R->elements[i], x), ringMultIdentity(R))) return R->elements[i];
    }

    // Could very well happen; a ring needn't be a multiplicative group
    return NULL;
}

// Return g^k for a GroupElement g
GroupElement* groupExp(GroupElement* g, int k) {
    if (!g) return NULL;

    Group* G = g->group;
    if (!G) return NULL;
    GroupElement* toReturn = groupIdentity(G);
    if (!toReturn) return NULL;
    while (k > 0) {
	    toReturn = groupMult(toReturn, g);
	    k--;
    }

    GroupElement* ginv = NULL;
    if (k < 0) ginv = groupInverse(g);
    while (k < 0) {
	    toReturn = groupMult(toReturn, ginv);
	    k++;
    }

    return toReturn;
}

// Return kx for int k and RingElement x
RingElement* ringTimes(RingElement* x, int k) {
    if (!x) return NULL;

    Ring* R = x->ring;
    if (!R) return NULL;
    RingElement* toReturn = ringAddIdentity(R);
    if (!toReturn) return NULL;
    while (k > 0) {
	    toReturn = ringAdd(toReturn, x);
        k--;
    }

    RingElement* xinv = NULL;
    if (k < 0) xinv = ringAddInverse(x);
    while (k < 0) {
	    toReturn = ringAdd(toReturn, xinv);
	    k++;
    }

    return toReturn;
}

// Return x^k for int k and RingElement x
RingElement* ringExp(RingElement* x, int k) {
    if (!x) return NULL;
    if (k == 0 && !hasMultIdentity(x->ring)) return NULL;
    if (k == 0) return ringMultIdentity(x->ring);

    Ring* R = x->ring;
    if (!R) return NULL;
    RingElement* toReturn;
    if (k > 0) toReturn = x;
    else if (k < 0) toReturn = ringMultInverse(x);
    if (!toReturn) return NULL;
    while (k > 1) {
	    toReturn = ringMult(toReturn, x);
        k--;
    }
    
    RingElement* xinv = NULL;
    if (k < 0) xinv = ringMultInverse(x);
    while (k < -1) {
	    toReturn = ringMult(toReturn, xinv);
	    k++;
    }

    return toReturn;
}

// Returns ghg^{-1} for g, h in a group G
GroupElement* groupElementConjugate(GroupElement* g, GroupElement* h) {
	if (!g || !h) return NULL;
	if (!cmpGroups(g->group, h->group)) return NULL;

	return groupMult(groupMult(g, h), groupInverse(g));
}

// Return the commutator of g and h, i.e. g^{-1}h^{-1}gh
GroupElement* groupCommutator(GroupElement* g, GroupElement* h) {
	if (!g || !h) return NULL;
	if (!cmpGroups(g->group, h->group)) return NULL;

	return groupMult(groupMult(groupInverse(g), groupInverse(h)), groupMult(g, h));
}

// Return the associator ((gh)k)(g(hk))^{-1}
GroupElement* groupAssociator(GroupElement* g, GroupElement* h, GroupElement* k) {
	if (!g || !h || !k) return NULL;
	if (!cmpGroups(g->group, h->group) || !cmpGroups(g->group, k->group)) return NULL;

	GroupElement* left = groupMult(groupMult(g, h), k);
	GroupElement* right = groupMult(g, groupMult(h, k));
	return groupMult(left, groupInverse(right));
}

// Return the ring commutator xy-yx
RingElement* ringCommutator(RingElement* x, RingElement* y) {
	if (!x || !y) return NULL;
	if (!cmpRings(x->ring, y->ring)) return NULL;

	RingElement* xy = ringMult(x, y);
	RingElement* yx = ringMult(y, x);
	RingElement* negYx = ringAddInverse(yx);
	return negYx ? ringAdd(xy, negYx) : NULL;
}

// Return the ring associator (xy)z-x(yz)
RingElement* ringAssociator(RingElement* x, RingElement* y, RingElement* z) {
	if (!x || !y || !z) return NULL;
	if (!cmpRings(x->ring, y->ring) || !cmpRings(x->ring, z->ring)) return NULL;

	RingElement* xyZ = ringMult(ringMult(x, y), z);
	RingElement* xYz = ringMult(x, ringMult(y, z));
	RingElement* negXYz = ringAddInverse(xYz);
	return negXYz ? ringAdd(xyZ, negXYz) : NULL;
}

// Return the trivial group
Group* trivialGroup() {
	GroupElement* e = malloc(sizeof(GroupElement));
	if (!e) return NULL;
	e->repr = malloc(2);
	if (!e->repr) {
		free(e);
		return NULL;
	}
	strcpy(e->repr, "e");
	e->group = NULL;
	e->index = 0;

	GroupElement** elements = malloc(sizeof(GroupElement*));
	if (!elements) {
		free(e->repr);
		free(e);
		return NULL;
	}
	elements[0] = e;

	int** table = malloc(sizeof(int*));
	if (!table) {
		free(elements);
		free(e->repr);
		free(e);
		return NULL;
	}
	table[0] = malloc(sizeof(int));
	if (!table[0]) {
		free(table);
		free(elements);
		free(e->repr);
		free(e);
		return NULL;
	}
	table[0][0] = 0;

	Group* G = constructTableGroupSkipValidate(elements, table, 1);
	if (!G) {
		free(table[0]);
		free(table);
		free(elements);
		free(e->repr);
		free(e);
		return NULL;
	}
	e->group = G;

	return G;
}

// Return the trivial ring
Ring* trivialRing() {
	RingElement* e = malloc(sizeof(RingElement));
	if (!e) return NULL;
	e->repr = malloc(2);
	if (!e->repr) {
		free(e);
		return NULL;
	}
	strcpy(e->repr, "0");
	e->ring = NULL;
	e->index = 0;

	RingElement** elements = malloc(sizeof(RingElement*));
	if (!elements) {
		free(e->repr);
		free(e);
		return NULL;
	}
	elements[0] = e;

	int** addTable = malloc(sizeof(int*));
	if (!addTable) {
		free(elements);
		free(e->repr);
		free(e);
		return NULL;
	}
	addTable[0] = malloc(sizeof(int));
	if (!addTable[0]) {
		free(addTable);
		free(elements);
		free(e->repr);
		free(e);
		return NULL;
	}
	addTable[0][0] = 0;

	int** multTable = malloc(sizeof(int*));
	if (!multTable) {
		free(addTable[0]);
		free(addTable);
		free(elements);
		free(e->repr);
		free(e);
		return NULL;
	}
	multTable[0] = malloc(sizeof(int));
	if (!multTable[0]) {
		free(multTable);
		free(addTable[0]);
		free(addTable);
		free(elements);
		free(e->repr);
		free(e);
		return NULL;
	}
	multTable[0][0] = 0;

	Ring* R = constructTableRingSkipValidate(elements, addTable, multTable, 1);
	if (!R) {
		free(multTable[0]);
		free(multTable);
		free(addTable[0]);
		free(addTable);
		free(elements);
		free(e->repr);
		free(e);
		return NULL;
	}
	e->ring = R;

	return R;
}

/* ---------- Subgroups ---------- */

// Construct a SubGroup of a Group
SubGroup* constructSubgroup(Group* G, int* indices, int indicesLen) {
    if (!G || !indices) return NULL;
    if (indicesLen < 1) return NULL;
    if (!G->isFinite || !G->elements) return NULL;

    // Check that all indices are valid
    for (int i = 0; i < indicesLen; i++) {
        if (indices[i] < 0 || (size_t)indices[i] >= G->card) return NULL;
    }

    if (hasDuplicateIndices(indices, indicesLen)) return NULL;
    sortIndices(indices, indicesLen);

    // Check that the potential subgroup contains identity
    bool containsId = false;
    for (int i = 0; i < indicesLen; i++) {
        if (indices[i] == 0) {
            containsId = true;
            break;
        }
    }
    if (!containsId) return NULL;

    // Check that, for each x, y in the subgroup, xy^{-1} is in the subgroup
    for (int i = 0; i < indicesLen; i++) {
        for (int j = 0; j < indicesLen; j++) {
            GroupElement* inv = groupInverse(G->elements[indices[j]]);
            if (!inv || inv->index < 0) return NULL;
            GroupElement* prod = groupMult(G->elements[indices[i]], inv);
            if (!prod || prod->index < 0) return NULL;
            if (!indexInList(prod->index, indices, indicesLen)) return NULL;
        }
    }

    GroupElement** elements = subgroupElementView(G, indices, indicesLen);
    if (!elements) return NULL;

    SubGroup* H = calloc(1, sizeof(SubGroup));
    if (!H) {
        free(elements);
        return NULL;
    }
    H->ambient = G;
    H->card = (size_t)indicesLen;
    H->isFinite = true;
    H->elements = elements;
    H->type = SUBGROUP_INDEXED;
    H->data.indexed.indices = indices;

    return H;
}

// Returns true if H is a subgroup of G, else false
bool isSubgroup(Group* G, SubGroup* H) {
	if (!G || !H) return false;
	
	return cmpGroups(G, H->ambient);
}

// Return true is H is a normal subgroup of G, else false
bool isNormalSubgroup(Group* G, SubGroup* H) {
    if (!G || !H) return false;
    if (!isSubgroup(G, H)) return false;
    if (H->type != SUBGROUP_INDEXED) return false;

    for (int i = 0; i < G->card; i++) {
		for (int j = 0; j < H->card; j++) {
			GroupElement* g = G->elements[i];
			GroupElement* h = G->elements[H->data.indexed.indices[j]];
			GroupElement* conj = groupMult(groupMult(g, h), groupInverse(g));
			if (!conj || conj->index < 0 || (size_t)conj->index >= G->card) return false;
			bool found = false;
			for (int k = 0; k < H->card; k++) {
				if (cmpGroupElements(G->elements[H->data.indexed.indices[k]], conj)) {
					found = true;
					break;
				}
			}
			if (!found) return false;
		}
	}

    return true;
}

// Return the subgroup generated by elements G->elements[i] for i in genIndices
SubGroup* subgroupGeneratedBy(Group* G, int* genIndices, int genIndicesLen) {
    if (!G || !genIndices || genIndicesLen < 0) return NULL;
    if (!G->isFinite || !G->elements) return NULL;

    // Use a boolean array to track which elements are in the subgroup
    bool* inSubgroup = calloc(G->card, sizeof(bool));
    if (!inSubgroup) return NULL;

    // Start with the identity
    inSubgroup[0] = true;
    int card = 1;

    // Add the generators
    for (int i = 0; i < genIndicesLen; i++) {
        if (genIndices[i] < 0 || genIndices[i] >= G->card) {
            free(inSubgroup);
            return NULL;
        }
        if (!inSubgroup[genIndices[i]]) {
            inSubgroup[genIndices[i]] = true;
            card++;
        }
    }

    // Close under multiplication: keep multiplying pairs until no new elements appear
    bool changed = true;
    while (changed) {
        changed = false;
        for (int i = 0; i < G->card; i++) {
            if (!inSubgroup[i]) continue;
            for (int j = 0; j < G->card; j++) {
                if (!inSubgroup[j]) continue;
                GroupElement* prod = groupMult(G->elements[i], G->elements[j]);
                if (!prod || prod->index < 0) {
                    free(inSubgroup);
                    return NULL;
                }
                int prodIndex = prod->index;
                if (!inSubgroup[prodIndex]) {
                    inSubgroup[prodIndex] = true;
                    card++;
                    changed = true;
                }
            }
        }
    }

    // Convert the boolean array to an indices array
    int* indices = malloc(card * sizeof(int));
    if (!indices) {
        free(inSubgroup);
        return NULL;
    }
    int k = 0;
    for (int i = 0; i < G->card; i++) {
        if (inSubgroup[i]) indices[k++] = i;
    }
    free(inSubgroup);

    SubGroup* H = constructSubgroup(G, indices, card);
    return H;
}

// Returns a SubGroup with just one element
SubGroup* trivialSubgroup() {
    int* indices = malloc(sizeof(int));
    if (!indices) return NULL;

    indices[0] = 0;
    int indicesLen = 1;
    Group* G = trivialGroup();
    if (!G) {
        free(indices);
        return NULL;
    }

    return constructSubgroup(G, indices, indicesLen);
}


// Returns true if the SubGroup is trivial, else false
bool isTrivialSubgroup(SubGroup* H) {
	if (!H) return false;

	return H->card == 1;
}

// Return the center of G
SubGroup* groupCenter(Group* G) {
    if (!G) return NULL;
    if (!G->isFinite || !G->elements) return NULL;

    if (isCommutativeGroup(G)) {
	    int* indices = malloc(G->card * sizeof(int));
        if (!indices) return NULL;
	    for (int i = 0; i < G->card; i++) indices[i] = i;
	    SubGroup* Z = constructSubgroup(G, indices, G->card);
	    return Z;
    }

    int* indices = malloc(G->card * sizeof(int));
    if (!indices) return NULL;
    int k = 0;
    for (int i = 0; i < G->card; i++) {
	bool isCentral = true;
	for (int j = 0; j < G->card; j++) {
	    if (!elementsCommute(G->elements[i], G->elements[j])) {
		    isCentral = false;
		    break;
	    }
	}
	if (isCentral) indices[k++] = i;
    }

    SubGroup* Z = constructSubgroup(G, indices, k);
    return Z;
}

// Return the centralizer of g in G
SubGroup* groupCentralizer(Group* G, GroupElement* g) {
    if (!G || !g) return NULL;
    if (!cmpGroups(g->group, G)) return NULL;
    if (!G->isFinite || !G->elements) return NULL;

    int* indices = malloc(G->card * sizeof(int));
    if (!indices) return NULL;
    int k = 0;
    for (int i = 0; i < G->card; i++) {
	    if (elementsCommute(g, G->elements[i])) indices[k++] = i;
    }

    SubGroup* C = constructSubgroup(G, indices, k);
    return C;
}

// Returns the conjugacy class of g in g->group
int* conjugacyClass(GroupElement* g, int* count) {
    if (!g || !count) return NULL;
    Group* G = g->group;
    if (!G || !G->isFinite || !G->elements) return NULL;

    bool* seen = calloc(G->card, sizeof(bool));
    if (!seen) return NULL;

    int n = 0;
    for (int i = 0; i < G->card; i++) {
        GroupElement* conj = groupElementConjugate(G->elements[i], g);
        if (!conj || conj->index < 0 || (size_t)conj->index >= G->card) {
            free(seen);
            return NULL;
        }
        if (!seen[conj->index]) {
            seen[conj->index] = true;
            n++;
        }
    }

    int* indices = malloc(n * sizeof(int));
    if (!indices) { free(seen); return NULL; }
    int k = 0;
    for (int i = 0; i < G->card; i++) {
        if (seen[i]) indices[k++] = i;
    }
    free(seen);

    *count = n;
    return indices;
}

// Return the normalizer of H in G
SubGroup* groupNormalizer(SubGroup* H) {
    if (!H) return NULL;
    Group* G = H->ambient;
    if (!G) return NULL;
    if (H->type != SUBGROUP_INDEXED) return NULL;

    int* indices = malloc(G->card * sizeof(int));
    if (!indices) return NULL;
    int k = 0;

    for (int i = 0; i < G->card; i++) {
        GroupElement* g = G->elements[i];
        bool preserved = true;
        for (int j = 0; j < H->card; j++) {
            GroupElement* h = G->elements[H->data.indexed.indices[j]];
            GroupElement* conj = groupElementConjugate(g, h);
            if (!conj || conj->index < 0 || (size_t)conj->index >= G->card) {
                free(indices);
                return NULL;
            }
            bool inH = false;
            for (int m = 0; m < H->card; m++) {
                if (conj->index == H->data.indexed.indices[m]) { inH = true; break; }
            }
            if (!inH) { 
                preserved = false; 
                break; 
            }
        }
        if (preserved) indices[k++] = i;
    }

    SubGroup* N = constructSubgroup(G, indices, k);
    return N;
}

// Return the normal closure of H in its ambient group, i.e.
// the smallest normal subgroup of G = H->ambient containing H
SubGroup* normalClosure(SubGroup* H) {
    if (!H) return NULL;
    Group* G = H->ambient;
    if (!G) return NULL;
    if (H->type != SUBGROUP_INDEXED) return NULL;

    // Collect all conjugates ghg^{-1} for g in G, h in H
    bool* inSet = calloc(G->card, sizeof(bool));
    if (!inSet) return NULL;
    inSet[0] = true;  // identity is always included

    for (int i = 0; i < G->card; i++) {
        for (int j = 0; j < H->card; j++) {
            GroupElement* conj = groupElementConjugate(G->elements[i], G->elements[H->data.indexed.indices[j]]);
            if (!conj || conj->index < 0 || (size_t)conj->index >= G->card) {
                free(inSet);
                return NULL;
            }
            inSet[conj->index] = true;
        }
    }

    // Collect the conjugate indices into an array
    int conjCount = 0;
    for (int i = 0; i < G->card; i++) if (inSet[i]) conjCount++;
    int* conjIndices = malloc(conjCount * sizeof(int));
    if (!conjIndices) {
        free(inSet);
        return NULL;
    }
    int k = 0;
    for (int i = 0; i < G->card; i++) {
        if (inSet[i]) conjIndices[k++] = i;
    }
    free(inSet);

    // The normal closure is the subgroup generated by all those conjugates
    SubGroup* N = subgroupGeneratedBy(G, conjIndices, conjCount);
    free(conjIndices);
    return N;
}

// Return the commutator subgroup of G
SubGroup* commutatorSubgroup(Group* G) {
	if (!G) return NULL;
    if (isCommutativeGroup(G)) {
        int* indices = malloc(sizeof(int));
        if (!indices) return NULL;
        indices[0] = 0;
        return constructSubgroup(G, indices, 1);
    }

	int* indices = malloc(G->card * sizeof(int));
    if (!indices) return NULL;
	int card = 1;
	indices[0] = 0;
	for (int i = 0; i < G->card; i++) {
		for (int j = 0; j < G->card; j++) {
			GroupElement* comm = groupCommutator(G->elements[i], G->elements[j]);
			if (!comm) { free(indices); return NULL; }
			bool found = false;
			for (int k = 0; k < card; k++) {
				if (indices[k] == comm->index) {
					found = true;
					break;
				}
			}
			if (found == false) indices[card++] = comm->index;
		}
	}

	SubGroup* H = constructSubgroup(G, indices, card);
	return H;
}

// Return the abelianization of G
Group* groupAbelianization(Group* G) {
	if (!G) return NULL;

    SubGroup* cg = commutatorSubgroup(G);
    Group* qg = quotientGroup(G, cg);
	freeSubgroup(cg);
    return qg;
}

// Return true if g is in the subgroup H, else false
bool isInSubgroup(SubGroup* H, GroupElement* g) {
	if (!H || !g) return false;
	if (!cmpGroups(H->ambient, g->group)) return false;

	for (int i = 0; i < H->card; i++) {
		if (cmpGroupElements(g, H->ambient->elements[H->data.indexed.indices[i]])) return true;
	}

	return false;
}

// Returns true if H is the whole group G, else false
bool isWholeGroup(Group* G, SubGroup* H) {
	if (!G || !H) return false;
	if (!cmpGroups(G, H->ambient)) return false;
	if (!isSubgroup(G, H)) return false;

	return H->card == G->card;
}

// Return true if K is contained in H, else false
bool subgroupContains(SubGroup* H, SubGroup* K) {
	if (!H || !K) return false;
	if (!cmpGroups(H->ambient, K->ambient)) return false;
	if (!isSubgroup(H->ambient, H) || !isSubgroup(K->ambient, K)) return false;

	for (int i = 0; i < K->card; i++) {
		bool found = false;
		for (int j = 0; j < H->card; j++) {
			if (K->data.indexed.indices[i] == H->data.indexed.indices[j]) {
				found = true;
				break;
			}
		}
		if (!found) return false;
	}

	return true;
}

// Returns the index of a subgroup H in its ambient group G
int subgroupIndex(SubGroup* H) {
	if (!H) return -1;

	return H->ambient->card / H->card;
}

// Returns the conjugate of a subgroup, i.e. gHg^{-1} for g in G and H a subgroup of G
SubGroup* subgroupConjugate(GroupElement* g, SubGroup* H) {
	if (!g || !H) return NULL;
		if (!cmpGroups(g->group, H->ambient)) return NULL;
    if (H->type != SUBGROUP_INDEXED) return NULL;

	int* indices = malloc(H->card * sizeof(int));
	if (!indices) return NULL;
	for (int i = 0; i < H->card; i++) {
	    GroupElement* conj = groupElementConjugate(g, H->ambient->elements[H->data.indexed.indices[i]]);
		if (!conj || conj->index < 0 || (size_t)conj->index >= H->ambient->card) {
			free(indices);
			return NULL;
		}
	    indices[i] = conj->index;
	}

	SubGroup* K = constructSubgroup(H->ambient, indices, H->card);
	return K;
}

// List all subgroups of G.
// Sets *count to the number of subgroups returned.
// Caller is responsible for calling freeSubgroup on each and free on the array.
SubGroup** listAllSubgroups(Group* G, int* count) {
    if (!G || !count) return NULL;
    if (!G->isFinite || !G->elements) return NULL;

    // Start with a dynamic array of subgroups, initialized with the trivial subgroup {e}
    int capacity = 16;
    int n = 0;
    SubGroup** subgroups = malloc(capacity * sizeof(SubGroup*));
    if (!subgroups) return NULL;

    int* trivialIndices = malloc(sizeof(int));
    if (!trivialIndices) { free(subgroups); return NULL; }
    trivialIndices[0] = 0;
    SubGroup* trivial = constructSubgroup(G, trivialIndices, 1);
    if (!trivial) { free(trivialIndices); free(subgroups); return NULL; }
    subgroups[n++] = trivial;

    // For each known subgroup H and each g in G not in H, compute <H u {g}>
    // and add it to the list if it's new. Repeat until no new subgroups appear.
    int scanStart = 0;
    while (scanStart < n) {
        int scanEnd = n;  // snapshot: only scan subgroups known at the start of this pass
        for (int s = scanStart; s < scanEnd; s++) {
            SubGroup* H = subgroups[s];
            for (int gi = 0; gi < G->card; gi++) {
                // Skip if g is already in H
                bool inH = false;
                for (int k = 0; k < H->card; k++) {
                    if (H->data.indexed.indices[k] == gi) { inH = true; break; }
                }
                if (inH) continue;

                // Build generator list: H's elements plus g
                int genLen = H->card + 1;
                int* gens = malloc(genLen * sizeof(int));
                if (!gens) {
                    for (int i = 0; i < n; i++) freeSubgroup(subgroups[i]);
                    free(subgroups);
                    return NULL;
                }
                for (int k = 0; k < H->card; k++) gens[k] = H->data.indexed.indices[k];
                gens[H->card] = gi;

                SubGroup* K = subgroupGeneratedBy(G, gens, genLen);
                free(gens);
                if (!K) {
                    for (int i = 0; i < n; i++) freeSubgroup(subgroups[i]);
                    free(subgroups);
                    return NULL;
                }

                // Check if K is already in the list
                bool duplicate = false;
                for (int i = 0; i < n; i++) {
                    if (cmpSubgroups(K, subgroups[i])) {
                        duplicate = true;
                        break;
                    }
                }
                if (duplicate) {
                    freeSubgroup(K);
                    continue;
                }

                // Grow the array if needed
                if (n >= capacity) {
                    capacity *= 2;
                    SubGroup** bigger = realloc(subgroups, capacity * sizeof(SubGroup*));
                    if (!bigger) {
                        freeSubgroup(K);
                        for (int i = 0; i < n; i++) freeSubgroup(subgroups[i]);
                        free(subgroups);
                        return NULL;
                    }
                    subgroups = bigger;
                }
                subgroups[n++] = K;
            }
        }
        scanStart = scanEnd;
    }

    *count = n;
    return subgroups;
}

// List all normal subgroups of G.
// Sets *count to the number of normal subgroups returned.
// Caller is responsible for calling freeSubgroup on each and free on the array.
SubGroup** listAllNormalSubgroups(Group* G, int* count) {
    if (!G || !count) return NULL;

    int allCount = 0;
    SubGroup** all = listAllSubgroups(G, &allCount);
    if (!all) return NULL;

    SubGroup** normal = malloc(allCount * sizeof(SubGroup*));
    if (!normal) {
        for (int i = 0; i < allCount; i++) freeSubgroup(all[i]);
        free(all);
        return NULL;
    }

    int n = 0;
    for (int i = 0; i < allCount; i++) {
        if (isNormalSubgroup(G, all[i])) {
            normal[n++] = all[i];
        } else {
            freeSubgroup(all[i]);
        }
    }
    free(all);

    *count = n;
    return normal;
}

// List all maximal subgroups of G
SubGroup** listAllMaximalSubgroups(Group* G, int* count) {
    if (!G || !count) return NULL;

    int numSubgroups = 0;
    SubGroup** subgroups = listAllSubgroups(G, &numSubgroups);
    if (!subgroups) return NULL;

    SubGroup** maximal = malloc(numSubgroups * sizeof(SubGroup*));
    if (!maximal) {
        for (int i = 0; i < numSubgroups; i++) freeSubgroup(subgroups[i]);
        free(subgroups);
        return NULL;
    }

    int numMaxSubgroups = 0;
    for (int i = 0; i < numSubgroups; i++) {
        if (subgroups[i]->card == G->card) { freeSubgroup(subgroups[i]); continue; }
        bool isMaximal = true;
        for (int j = 0; j < numSubgroups; j++) {
            if (subgroups[j]->card <= subgroups[i]->card) continue;
            if (subgroups[j]->card == G->card) continue;
            if (subgroupContains(subgroups[j], subgroups[i])) {
                isMaximal = false;
                break;
            }
        }
        if (isMaximal) maximal[numMaxSubgroups++] = subgroups[i];
        else freeSubgroup(subgroups[i]);
    }
    free(subgroups);

    *count = numMaxSubgroups;
    return maximal;
}

// Returns true iff core_G(H) = {e}, i.e. no non-identity element of H is
// fixed by all conjugations.  (core = intersection of all conjugates of H)
static bool hasTrivialCore(SubGroup* H) {
    Group* G = H->ambient;
    if (!G || H->type != SUBGROUP_INDEXED) return false;
    for (int hi = 0; hi < H->card; hi++) {
        if (H->data.indexed.indices[hi] == 0) continue; // skip identity
        GroupElement* h = G->elements[H->data.indexed.indices[hi]];
        bool inAllConjugates = true;
        for (int gi = 0; gi < G->card; gi++) {
            GroupElement* conj = groupElementConjugate(G->elements[gi], h);
            if (!conj || conj->index < 0 || (size_t)conj->index >= G->card) return false;
            bool inH = false;
            for (int m = 0; m < H->card; m++) {
                if (conj->index == H->data.indexed.indices[m]) { inH = true; break; }
            }
            if (!inH) { inAllConjugates = false; break; }
        }
        if (inAllConjugates) return false; // h lies in the core
    }
    return true;
}

// Get the largest subgroup of G with trivial core
SubGroup* largestCoreFreeSubgroup(Group* G) {
    if (!G) return NULL;

    int numSubgroups = 0;
    SubGroup** subgroups = listAllSubgroups(G, &numSubgroups);
    if (!subgroups) return NULL;

    SubGroup* largest = NULL;
    for (int i = 0; i < numSubgroups; i++) {
        SubGroup* H = subgroups[i];
        if (H->card == G->card) { freeSubgroup(H); continue; }
        if (hasTrivialCore(H)) {
            if (!largest || H->card > largest->card) {
                if (largest) freeSubgroup(largest);
                largest = H;
            } else {
                freeSubgroup(H);
            }
        } else {
            freeSubgroup(H);
        }
    }
    free(subgroups);

    return largest;
}

// Realise a SubGroup H of G as a standalone Group, with elements indexed so that
// the identity is at position 0 and the rest follow H->data.indexed.indices' order. The
// multiplication table is the restriction of G's table to H.
Group* subgroupAsGroup(SubGroup* H) {
    if (!H || !H->ambient) return NULL;
    Group* G = H->ambient;
    int n = H->card;
    if (n < 1) return NULL;
    if (H->type != SUBGROUP_INDEXED) return NULL;

    // Permute H->data.indexed.indices so the ambient identity (index 0) comes first
    int* localToAmbient = malloc(n * sizeof(int));
    if (!localToAmbient) return NULL;
    int idPos = -1;
    for (int i = 0; i < n; i++) {
        if (H->data.indexed.indices[i] == 0) { idPos = i; break; }
    }
    if (idPos < 0) { free(localToAmbient); return NULL; }
    localToAmbient[0] = 0;
    int next = 1;
    for (int i = 0; i < n; i++) {
        if (i != idPos) localToAmbient[next++] = H->data.indexed.indices[i];
    }

    GroupElement** elements = calloc(n, sizeof(GroupElement*));
    if (!elements) { free(localToAmbient); return NULL; }
    for (int i = 0; i < n; i++) {
        elements[i] = constructGroupElement(NULL, G->elements[localToAmbient[i]]->repr);
        if (!elements[i]) {
            for (int j = 0; j < i; j++) freeGroupElement(elements[j]);
            free(elements);
            free(localToAmbient);
            return NULL;
        }
    }

    int** table = malloc(n * sizeof(int*));
    if (!table) {
        for (int i = 0; i < n; i++) freeGroupElement(elements[i]);
        free(elements);
        free(localToAmbient);
        return NULL;
    }
    for (int i = 0; i < n; i++) {
        table[i] = malloc(n * sizeof(int));
        if (!table[i]) {
            for (int j = 0; j < i; j++) free(table[j]);
            free(table);
            for (int j = 0; j < n; j++) freeGroupElement(elements[j]);
            free(elements);
            free(localToAmbient);
            return NULL;
        }
        for (int j = 0; j < n; j++) {
            GroupElement* prod = groupMult(G->elements[localToAmbient[i]], G->elements[localToAmbient[j]]);
            if (!prod || prod->index < 0) {
                for (int kk = 0; kk <= i; kk++) free(table[kk]);
                free(table);
                for (int kk = 0; kk < n; kk++) freeGroupElement(elements[kk]);
                free(elements);
                free(localToAmbient);
                return NULL;
            }
            int prodAmbient = prod->index;
            int pos = -1;
            for (int k = 0; k < n; k++) {
                if (localToAmbient[k] == prodAmbient) { pos = k; break; }
            }
            if (pos < 0) {
                for (int kk = 0; kk <= i; kk++) free(table[kk]);
                free(table);
                for (int kk = 0; kk < n; kk++) freeGroupElement(elements[kk]);
                free(elements);
                free(localToAmbient);
                return NULL;
            }
            table[i][j] = pos;
        }
    }

    free(localToAmbient);

    Group* Hg = constructTableGroupSkipValidate(elements, table, n);
    if (!Hg) {
        for (int i = 0; i < n; i++) { freeGroupElement(elements[i]); free(table[i]); }
        free(elements);
        free(table);
        return NULL;
    }
    for (int i = 0; i < n; i++) elements[i]->group = Hg;
    return Hg;
}

/* ---------- Cyclic groups ---------- */

// Returns true if G is cyclic, else false
bool isCyclicGroup(Group* G) {
	if (!G) return false;

	for (int i = 0; i < G->card; i++) {
		if (elementOrder(G, G->elements[i]) == G->card) return true;
	}

	return false;
}

// Returns true if H is a cyclic subgroup of G, else false
bool isCyclicSubgroup(SubGroup* H) {
	if (!H) return false;
	if (!isSubgroup(H->ambient, H)) return false;

	for (int i = 0; i < H->card; i++) {
		if (elementOrder(H->ambient, H->ambient->elements[H->data.indexed.indices[i]]) == H->card) return true;
	}

	return false;
}

// Returns true if g generates a cyclic group G, else false
bool generatesCyclicGroup(Group* G, GroupElement* g) {
	if (!G || !g) return false;
	if (!cmpGroups(G, g->group)) return false;

	return elementOrder(G, g) == G->card;
}

// Returns true if g generates a cyclic subgroup H of G, else false
bool generatesCyclicSubgroup(SubGroup* H, GroupElement* g) {
	if (!H || !g) return false;
	if (!isSubgroup(H->ambient, H)) return false;
	if (!isInSubgroup(H, g)) return false;

	return elementOrder(H->ambient, g) == H->card;
}

// Get the cyclic subgroup generated by g in G
SubGroup* getCyclicSubgroup(GroupElement* g) {
	if (!g) return NULL;

	Group* G = g->group;
	int order = elementOrder(G, g);
    if (order < 1) return NULL;
	int* indices = malloc(order * sizeof(int));
	if (!indices) return NULL;

	GroupElement* prod = groupIdentity(G);
	for (int i = 0; i < order; i++) {
		indices[i] = prod->index;
		prod = groupMult(prod, g);
	}

	SubGroup* H = constructSubgroup(G, indices, order);
	return H;
}

/* ---------- Quotient groups ---------- */

// Return true if g is in the coset, else false
bool isInGroupCoset(GroupCoset* coset, GroupElement* g) {
	if (!coset || !g) return false;
	if (!cmpGroups(coset->group, g->group)) return false;

	for (int i = 0; i < coset->subgroup->card; i++) {
		if (g->index == coset->data.indexed.indices[i]) return true;
	}

	return false;
}

// Generate the coset gH of a subgroup H
GroupCoset* generateLeftGroupCoset(SubGroup* H, GroupElement* g) {
	if (!H || !g) return NULL;
	if (!cmpGroups(g->group, H->ambient)) return NULL;
    switch (H->type) {
        case SUBGROUP_INDEXED:
            break;
        case SUBGROUP_GENERATED:
        case SUBGROUP_EXPLICIT:
        case SUBGROUP_PREDICATE:
            return NULL;
    }
	    if (g->index < 0 || (size_t)g->index >= H->ambient->card) return NULL;

		int* indices = malloc(H->card * sizeof(int));
	    if (!indices) return NULL;
		for (int i = 0; i < H->card; i++) {
	        GroupElement* prod = groupMult(g, H->ambient->elements[H->data.indexed.indices[i]]);
	        if (!prod || prod->index < 0) {
	            free(indices);
	            return NULL;
	        }
	        indices[i] = prod->index;
		}

    GroupElement** elements = subgroupElementView(g->group, indices, H->card);
    if (!elements) {
        free(indices);
        return NULL;
    }

	GroupCoset* coset = calloc(1, sizeof(GroupCoset));
    if (!coset) {
        free(elements);
        free(indices);
        return NULL;
    }
    coset->group = H->ambient;
	coset->subgroup = H;
    coset->card = H->card;
    coset->isFinite = true;
	coset->isLeft = true;
    coset->elements = elements;
    coset->representative = g;
    coset->type = GROUP_COSET_INDEXED;
	coset->data.indexed.indices = indices;

	return coset;
}

// Generate the coset Hg of a subgroup H
GroupCoset* generateRightGroupCoset(SubGroup* H, GroupElement* g) {
	if (!H || !g) return NULL;
		if (!cmpGroups(g->group, H->ambient)) return NULL;
    switch (H->type) {
        case SUBGROUP_INDEXED:
            break;
        case SUBGROUP_GENERATED:
        case SUBGROUP_EXPLICIT:
        case SUBGROUP_PREDICATE:
            return NULL;
    }
	    if (g->index < 0 || (size_t)g->index >= H->ambient->card) return NULL;

		int* indices = malloc(H->card * sizeof(int));
	    if (!indices) return NULL;
		for (int i = 0; i < H->card; i++) {
	        GroupElement* prod = groupMult(H->ambient->elements[H->data.indexed.indices[i]], g);
	        if (!prod || prod->index < 0) {
	            free(indices);
	            return NULL;
	        }
	        indices[i] = prod->index;
		}

    GroupElement** elements = subgroupElementView(g->group, indices, H->card);
    if (!elements) {
        free(indices);
        return NULL;
    }

	GroupCoset* coset = calloc(1, sizeof(GroupCoset));
    if (!coset) {
        free(elements);
        free(indices);
        return NULL;
    }
    coset->group = H->ambient;
	coset->subgroup = H;
    coset->card = H->card;
    coset->isFinite = true;
	coset->isLeft = false;
    coset->elements = elements;
    coset->representative = g;
    coset->type = GROUP_COSET_INDEXED;
	coset->data.indexed.indices = indices;

	return coset;
}

// Generate all left cosets of a SubGroup H in G
GroupCoset** getLeftGroupCosets(SubGroup* H) {
	if (!H) return NULL;

	int numGroupCosets = H->ambient->card / H->card;
	GroupCoset** cosets = malloc(numGroupCosets * sizeof(GroupCoset*));
	if (!cosets) return NULL;

	bool* visited = calloc(H->ambient->card, sizeof(bool));
	if (!visited) { free(cosets); return NULL; }

	int k = 0;
	for (int i = 0; i < H->ambient->card && k < numGroupCosets; i++) {
		if (!visited[i]) {
			GroupCoset* coset = generateLeftGroupCoset(H, H->ambient->elements[i]);
			if (!coset) { for (int m = 0; m < k; m++) freeGroupCoset(cosets[m]); free(visited); free(cosets); return NULL; }
			cosets[k++] = coset;
			for (int j = 0; j < coset->subgroup->card; j++)
				visited[coset->data.indexed.indices[j]] = true;
		}
	}

	free(visited);
	return cosets;
}

// Generate all right cosets of a SubGroup H in G
GroupCoset** getRightGroupCosets(SubGroup* H) {
	if (!H) return NULL;

	int numGroupCosets = H->ambient->card / H->card;
	GroupCoset** cosets = malloc(numGroupCosets * sizeof(GroupCoset*));
	if (!cosets) return NULL;

	bool* visited = calloc(H->ambient->card, sizeof(bool));
	if (!visited) { free(cosets); return NULL; }

	int k = 0;
	for (int i = 0; i < H->ambient->card && k < numGroupCosets; i++) {
		if (!visited[i]) {
			GroupCoset* coset = generateRightGroupCoset(H, H->ambient->elements[i]);
			if (!coset) { for (int m = 0; m < k; m++) freeGroupCoset(cosets[m]); free(visited); free(cosets); return NULL; }
			cosets[k++] = coset;
			for (int j = 0; j < coset->subgroup->card; j++)
				visited[coset->data.indexed.indices[j]] = true;
		}
	}

	free(visited);
	return cosets;
}

// Construct and return the quotient group G/N
Group* quotientGroup(Group* G, SubGroup* N) {
	if (!G || !N) return NULL;
	if (!isNormalSubgroup(G, N)) return NULL;

	GroupCoset** cosetList = getLeftGroupCosets(N);
	if (!cosetList) return NULL;
	int numCosets = G->card / N->card;

	// calloc so uninitialized slots are NULL — safe to freeGroupElement in cleanup
	GroupElement** elements = calloc(numCosets, sizeof(GroupElement*));
	if (!elements) {
		for (int j = 0; j < numCosets; j++) freeGroupCoset(cosetList[j]);
		free(cosetList);
		return NULL;
	}

	// Generate the representatives for the quotient group
	for (int i = 0; i < numCosets; i++) {
		char* repr = malloc(32 * sizeof(char));
		if (!repr) {
			for (int j = 0; j < i; j++) freeGroupElement(elements[j]);
			free(elements);
			for (int j = 0; j < numCosets; j++) freeGroupCoset(cosetList[j]);
			free(cosetList);
			return NULL;
		}
		snprintf(repr, 32, "%sN", G->elements[cosetList[i]->data.indexed.indices[0]]->repr);
		elements[i] = constructGroupElement(NULL, repr);
		free(repr);
		if (!elements[i]) {
			for (int j = 0; j < i; j++) freeGroupElement(elements[j]);
			free(elements);
			for (int j = 0; j < numCosets; j++) freeGroupCoset(cosetList[j]);
			free(cosetList);
			return NULL;
		}
	}

	// Generate the multiplication table for the quotient group
	int** table = malloc(numCosets * sizeof(int*));
	if (!table) {
		for (int j = 0; j < numCosets; j++) freeGroupElement(elements[j]);
		free(elements);
		for (int j = 0; j < numCosets; j++) freeGroupCoset(cosetList[j]);
		free(cosetList);
		return NULL;
	}
	for (int i = 0; i < numCosets; i++) {
		table[i] = malloc(numCosets * sizeof(int));
		if (!table[i]) {
			for (int j = 0; j < i; j++) free(table[j]);
			free(table);
			for (int j = 0; j < numCosets; j++) freeGroupElement(elements[j]);
			free(elements);
			for (int j = 0; j < numCosets; j++) freeGroupCoset(cosetList[j]);
			free(cosetList);
			return NULL;
		}
		for (int j = 0; j < numCosets; j++) {
			GroupElement* prod = groupMult(G->elements[cosetList[i]->data.indexed.indices[0]], G->elements[cosetList[j]->data.indexed.indices[0]]);
			if (!prod || prod->index < 0 || (size_t)prod->index >= G->card) {
				for (int k = 0; k <= i; k++) free(table[k]);
				free(table);
				for (int k = 0; k < numCosets; k++) freeGroupElement(elements[k]);
				free(elements);
				for (int k = 0; k < numCosets; k++) freeGroupCoset(cosetList[k]);
				free(cosetList);
				return NULL;
			}
			bool found = false;
			for (int k = 0; k < numCosets; k++) {
				if (isInGroupCoset(cosetList[k], prod)) {
					table[i][j] = k;
					found = true;
					break;
				}
			}
			if (!found) {
				for (int k = 0; k <= i; k++) free(table[k]);
				free(table);
				for (int k = 0; k < numCosets; k++) freeGroupElement(elements[k]);
				free(elements);
				for (int k = 0; k < numCosets; k++) freeGroupCoset(cosetList[k]);
				free(cosetList);
				return NULL;
			}
		}
	}

	for (int j = 0; j < numCosets; j++) freeGroupCoset(cosetList[j]);
	free(cosetList);
	Group* H = constructTableGroupSkipValidate(elements, table, numCosets);
	if (!H) {
		for (int i = 0; i < numCosets; i++) {
			freeGroupElement(elements[i]);
			free(table[i]);
		}
		free(elements);
		free(table);
		return NULL;
	}
	for (int i = 0; i < numCosets; i++) elements[i]->group = H;
	return H;
}

/* ---------- Product groups ---------- */

// Construct the direct product of two groups
Group* constructProductGroup(Group* G, Group* H) {
    if (!G || !H) return NULL;
    if (!G->isFinite || !H->isFinite || !G->elements || !H->elements) return NULL;

    int newCard = G->card * H->card;
    Group* P = calloc(1, sizeof(Group));
    if (!P) return NULL;
    P->type = GROUP_PRODUCT;
    P->card = (size_t)newCard;
    P->isFinite = true;
    P->data.product.count = 2;
    P->data.product.factors = malloc(2 * sizeof(Group*));
    P->elements = calloc((size_t)newCard, sizeof(GroupElement*));
    if (!P->data.product.factors || !P->elements) {
        freeGroup(P);
        return NULL;
    }
    P->data.product.factors[0] = G;
    P->data.product.factors[1] = H;

    // Build element representations as "(g,h)"
    for (int i = 0; i < G->card; i++) {
        for (int j = 0; j < H->card; j++) {
            int index = i * H->card + j;
            GroupElement* factors[2] = { G->elements[i], H->elements[j] };
            ProductGroupElementData data = { factors, 2 };
            P->elements[index] = constructGroupElement(P, &data);
            if (!P->elements[index]) {
                freeGroup(P);
                return NULL;
            }
            P->elements[index]->index = index;
        }
    }

    return P;
}

// Return G x G x ... x G (k copies)
Group* kfoldProductGroup(Group* G, int k) {
    if (!G || k < 1) return NULL;

    if (k == 1) {
        return G;
    }

    Group* prod = constructProductGroup(G, G);
    if (!prod) return NULL;

    for (int i = 2; i < k; i++) {
        Group* next = constructProductGroup(G, prod);
        if (!next) {
            freeGroup(prod);
            return NULL;
        }
        prod = next;
    }

    // Flatten element reprs to (a,b,c,...) form
    for (int m = 0; m < prod->card; m++) {
        char* flat = flattenRepr(prod->elements[m]->repr);
        if (flat) { free(prod->elements[m]->repr); prod->elements[m]->repr = flat; }
    }

    return prod;
}

// Return R x R x ... x R (k copies)
Ring* kfoldProductRing(Ring* R, int k) {
    if (!R || k < 1) return NULL;

    if (k == 1) {
        return R;
    }

    Ring* prod = constructProductRing(R, R);
    if (!prod) return NULL;

    for (int i = 2; i < k; i++) {
        Ring* next = constructProductRing(R, prod);
        if (!next) {
            freeRing(prod);
            return NULL;
        }
        prod = next;
    }

    // Flatten element reprs to (a,b,c,...) form
    for (int m = 0; m < prod->card; m++) {
        char* flat = flattenRepr(prod->elements[m]->repr);
        if (flat) { free(prod->elements[m]->repr); prod->elements[m]->repr = flat; }
    }

    return prod;
}

// Construct the direct product of two rings
Ring* constructProductRing(Ring* R, Ring* S) {
    if (!R || !S) return NULL;
    if (!R->isFinite || !S->isFinite || !R->elements || !S->elements) return NULL;

    int newCard = R->card * S->card;
    Ring* P = calloc(1, sizeof(Ring));
    if (!P) return NULL;
    P->type = RING_PRODUCT;
    P->card = (size_t)newCard;
    P->isFinite = true;
    P->data.product.count = 2;
    P->data.product.factors = malloc(2 * sizeof(Ring*));
    P->elements = calloc((size_t)newCard, sizeof(RingElement*));
    if (!P->data.product.factors || !P->elements) {
        freeRing(P);
        return NULL;
    }
    P->data.product.factors[0] = R;
    P->data.product.factors[1] = S;

    // Build element representations as "(x,y)"
    for (int i = 0; i < R->card; i++) {
        for (int j = 0; j < S->card; j++) {
            int index = i * S->card + j;
            RingElement* factors[2] = { R->elements[i], S->elements[j] };
            ProductRingElementData data = { factors, 2 };
            P->elements[index] = constructRingElement(P, &data);
            if (!P->elements[index]) {
                freeRing(P);
                return NULL;
            }
            P->elements[index]->index = index;
        }
    }

    return P;
}

/* ---------- Group homomorphisms ---------- */

// Constructs a GroupHomomorphism from G to H defined by the mapping indicesMapping
GroupHomomorphism* constructGroupHomomorphism(Group* domain, Group* codomain, int* indicesMapping, int indicesMappingSize) {
    if (!domain || !codomain || !indicesMapping) return NULL;
    if (indicesMappingSize != domain->card) return NULL;
    if (!domain->isFinite || !codomain->isFinite) return NULL;
    if (!domain->elements || !codomain->elements) return NULL;

    // Validate that indicesMapping is a valid homomorphism
    for (int i = 0; i < domain->card; i++) {
        if (indicesMapping[i] < 0 || (size_t)indicesMapping[i] >= codomain->card) return NULL;
        for (int j = 0; j < domain->card; j++) {
            GroupElement* domainProd = groupMult(domain->elements[i], domain->elements[j]);
            GroupElement* codomainProd = groupMult(codomain->elements[indicesMapping[i]], codomain->elements[indicesMapping[j]]);
            if (!domainProd || !codomainProd || domainProd->index < 0 || codomainProd->index < 0) return NULL;
            if (codomainProd->index != indicesMapping[domainProd->index]) return NULL;
        }
    }

    GroupHomomorphism* homo = calloc(1, sizeof(GroupHomomorphism));
    if (!homo) return NULL;
    homo->domain = domain;
    homo->codomain = codomain;
    homo->type = GROUP_HOM_INDEXED;
    homo->data.indexed.mapping = indicesMapping;
    homo->data.indexed.count = (size_t)indicesMappingSize;
    return homo;
}

// Return the kernel of a homomorphism as a SubGroup of homo->domain
SubGroup* groupHomomorphismKernel(GroupHomomorphism* homo) {
    if (!homo) return NULL;

    int* indices = malloc(homo->domain->card * sizeof(int));
    if (!indices) return NULL;
    int k = 0;
    for (int i = 0; i < homo->domain->card; i++) {
        if (homo->data.indexed.mapping[i] == 0) indices[k++] = i;
    }
    SubGroup* ker = constructSubgroup(homo->domain, indices, k);
    if (!ker) {
        free(indices);
        return NULL;
    }

    return ker;
}

// Returns f(g), where f is a homo and g is an element of homo->domain
GroupElement* groupElementImage(GroupHomomorphism* homo, GroupElement* g) {
    if (!homo || !g) return NULL;
    if (!cmpGroups(homo->domain, g->group)) return NULL;

    return homo->codomain->elements[homo->data.indexed.mapping[g->index]];
}

// Returns the image of a GroupHomomorphism as a Group
Group* groupHomomorphismImage(GroupHomomorphism* homo) {
    if (!homo) return NULL;
    
    // Create a list of all indices which are in the image
    int maxCard = homo->codomain->card;
    bool* seen = malloc(maxCard * sizeof(bool));
    if (!seen) return NULL;
    for (int i = 0; i < maxCard; i++) seen[i] = false;
    for (int i = 0; i < homo->domain->card; i++) {
        seen[homo->data.indexed.mapping[i]] = true;
    }

    // Generate a list of the indices of elements in Im(homo)
    int* indices = malloc(maxCard * sizeof(int));
    if (!indices) {
        free(seen);
        return NULL;
    }
    int k = 0;
    for (int i = 0; i < maxCard; i++) {
        if (seen[i]) indices[k++] = i;
    }

    // Generate the Cayley table of the group
    int** table = malloc(k * sizeof(int*));
    if (!table) {
        free(seen);
        free(indices);
        return NULL;
    }
    for (int i = 0; i < k; i++) {
        table[i] = malloc(k * sizeof(int));
        if (!table[i]) {
            free(seen);
            free(indices);
            for (int j = 0; j < i; j++) free(table[j]);
            free(table);
            return NULL;
        }
    }
    for (int i = 0; i < k; i++) {
        for (int j = 0; j < k; j++) {
            GroupElement* prod = groupMult(homo->codomain->elements[indices[i]], homo->codomain->elements[indices[j]]);
            if (!prod || prod->index < 0) {
                free(seen);
                free(indices);
                for (int m = 0; m < k; m++) free(table[m]);
                free(table);
                return NULL;
            }
            int prodCodIndex = prod->index;
            bool found = false;
            for (int m = 0; m < k; m++) {
                if (indices[m] == prodCodIndex) { table[i][j] = m; found = true; break; }
            }
            if (!found) {
                free(seen);
                free(indices);
                for (int m = 0; m < k; m++) free(table[m]);
                free(table);
                return NULL;
            }
        }
    }

    // Generate the Group corresponding to those elements
    GroupElement** elements = malloc(k * sizeof(GroupElement*));
    if (!elements) {
        free(seen);
        free(indices);
        for (int i = 0; i < k; i++) free(table[i]);
        free(table);
        return NULL;
    }
    for (int i = 0; i < k; i++) {
        elements[i] = constructGroupElement(NULL, homo->codomain->elements[indices[i]]->repr);
        if (!elements[i]) {
            for (int j = 0; j < i; j++) freeGroupElement(elements[j]);
            free(elements);
            free(seen);
            free(indices);
            for (int j = 0; j < k; j++) free(table[j]);
            free(table);
            return NULL;
        }
    }

    Group* image = constructTableGroupSkipValidate(elements, table, k);
    free(seen);
    free(indices);
    if (!image) {
        for (int i = 0; i < k; i++) freeGroupElement(elements[i]);
        free(elements);
        for (int i = 0; i < k; i++) free(table[i]);
        free(table);
        return NULL;
    }

    for (int i = 0; i < k; i++) elements[i]->group = image;
    return image;
}

// Returns true if a homomorphism is an isomorphism, else false
bool isGroupIsomorphism(GroupHomomorphism* homo) {
    if (!homo) return false;
    if (homo->domain->card != homo->codomain->card) return false;
    
    for (int i = 1; i < homo->domain->card; i++) {
        if (homo->data.indexed.mapping[i] == 0) return false;
    }
    return true;
}

/* ---------- Ideals, subrings, and quotient rings ---------- */

// Construct a SubRing of a Ring
SubRing* constructSubring(Ring* R, int* indices, int indicesLen) {
    if (!R || !indices) return NULL;
    if (indicesLen < 1) return NULL;
    if (!R->isFinite || !R->elements) return NULL;

    // Check that all indices are valid
    for (int i = 0; i < indicesLen; i++) {
        if (indices[i] < 0 || (size_t)indices[i] >= R->card) return NULL;
    }


    // Check that the potential subring contains additive identity
    bool containsId = false;
    for (int i = 0; i < indicesLen; i++) {
        if (indices[i] == 0) {
            containsId = true;
            break;
        }
    }
    if (!containsId) return NULL;

    // Check that, for each x, y in the subring, x - y is in the subring
    for (int i = 0; i < indicesLen; i++) {
        for (int j = 0; j < indicesLen; j++) {
            RingElement* inv = ringAddInverse(R->elements[indices[j]]);
            if (!inv || inv->index < 0) return NULL;
            RingElement* diff = ringAdd(R->elements[indices[i]], inv);
            if (!diff || diff->index < 0) return NULL;
            if (!indexInList(diff->index, indices, indicesLen)) return NULL;
        }
    }

    // Check that the subring is closed under multiplication
    for (int i = 0; i < indicesLen; i++) {
        for (int j = 0; j < indicesLen; j++) {
            RingElement* prod = ringMult(R->elements[indices[i]], R->elements[indices[j]]);
            if (!prod || prod->index < 0) return NULL;
            if (!indexInList(prod->index, indices, indicesLen)) return NULL;
        }
    }

    RingElement** elements = subringElementView(R, indices, indicesLen);
    if (!elements) return NULL;

    SubRing* S = calloc(1, sizeof(SubRing));
    if (!S) {
        free(elements);
        return NULL;
    }
    S->ambient = R;
    S->card = (size_t)indicesLen;
    S->isFinite = true;
    S->elements = elements;
    S->type = SUBRING_INDEXED;
    S->data.indexed.indices = indices;

    return S;
}

// Returns true if two SubRings are equal, else false
bool cmpSubrings(SubRing* S, SubRing* T) {
    if (!S || !T) return false;
    if (!cmpRings(S->ambient, T->ambient)) return false;
    if (S->card != T->card) return false;

    for (int i = 0; i < S->card; i++) if (S->data.indexed.indices[i] != T->data.indexed.indices[i]) return false;
    return true;
}

// Return true if the SubRing is trivial, else false
bool isTrivialSubring(SubRing* S) {
    if (!S) return false;

    return (S->card == 1);
}

// Return true if a SubRing is the whole Ring, else false
bool isWholeRing(SubRing* S) {
    if (!S) return false;

    return (S->card == S->ambient->card);
}

// Construct a left Ideal of a Ring
Ideal* constructLeftIdeal(Ring* R, int* indices, int indicesLen) {
    if (!R || !indices) return NULL;
    if (indicesLen < 1) return NULL;
    if (!R->isFinite || !R->elements) return NULL;

    // Check that all indices are valid
    for (int i = 0; i < indicesLen; i++) {
        if (indices[i] < 0 || (size_t)indices[i] >= R->card) return NULL;
    }


    // Check that the potential subring contains additive identity
    bool containsId = false;
    for (int i = 0; i < indicesLen; i++) {
        if (indices[i] == 0) {
            containsId = true;
            break;
        }
    }
    if (!containsId) return NULL;

    // Check that, for each x, y in the Ideal, x - y is in the Ideal (additive subgroup)
    for (int i = 0; i < indicesLen; i++) {
        for (int j = 0; j < indicesLen; j++) {
            RingElement* inv = ringAddInverse(R->elements[indices[j]]);
            if (!inv || inv->index < 0) return NULL;
            RingElement* diff = ringAdd(R->elements[indices[i]], inv);
            if (!diff || diff->index < 0) return NULL;
            if (!indexInList(diff->index, indices, indicesLen)) return NULL;
        }
    }

    // Check that, for all r in R and x in I, rx is in I
    for (int i = 0; i < indicesLen; i++) {
        for (int j = 0; j < R->card; j++) {
            RingElement* prod = ringMult(R->elements[j], R->elements[indices[i]]);
            if (!prod || prod->index < 0) return NULL;
            if (!indexInList(prod->index, indices, indicesLen)) return NULL;
        }
    }

    RingElement** elements = subringElementView(R, indices, indicesLen);
    if (!elements) return NULL;

    Ideal* I = calloc(1, sizeof(Ideal));
    if (!I) {
        free(elements);
        return NULL;
    }
    I->ring = R;
    I->card = (size_t)indicesLen;
    I->isFinite = true;
    I->elements = elements;
    I->side = IDEAL_LEFT;
    I->type = IDEAL_INDEXED;
    I->data.indexed.indices = indices;

    return I;
}

// Construct a right Ideal of a Ring
Ideal* constructRightIdeal(Ring* R, int* indices, int indicesLen) {
    if (!R || !indices) return NULL;
    if (indicesLen < 1) return NULL;
    if (!R->isFinite || !R->elements) return NULL;

    // Check that all indices are valid
    for (int i = 0; i < indicesLen; i++) {
        if (indices[i] < 0 || (size_t)indices[i] >= R->card) return NULL;
    }

    // Check that the potential subring contains additive identity
    bool containsId = false;
    for (int i = 0; i < indicesLen; i++) {
        if (indices[i] == 0) {
            containsId = true;
            break;
        }
    }
    if (!containsId) return NULL;

    // Check that, for each x, y in the Ideal, x - y is in the Ideal (additive subgroup)
    for (int i = 0; i < indicesLen; i++) {
        for (int j = 0; j < indicesLen; j++) {
            RingElement* inv = ringAddInverse(R->elements[indices[j]]);
            if (!inv || inv->index < 0) return NULL;
            RingElement* diff = ringAdd(R->elements[indices[i]], inv);
            if (!diff || diff->index < 0) return NULL;
            if (!indexInList(diff->index, indices, indicesLen)) return NULL;
        }
    }

    // Check that, for all r in R and x in I, xr is in I
    for (int i = 0; i < indicesLen; i++) {
        for (int j = 0; j < R->card; j++) {
            RingElement* prod = ringMult(R->elements[indices[i]], R->elements[j]);
            if (!prod || prod->index < 0) return NULL;
            if (!indexInList(prod->index, indices, indicesLen)) return NULL;
        }
    }

    RingElement** elements = subringElementView(R, indices, indicesLen);
    if (!elements) return NULL;

    Ideal* I = calloc(1, sizeof(Ideal));
    if (!I) {
        free(elements);
        return NULL;
    }
    I->ring = R;
    I->card = (size_t)indicesLen;
    I->isFinite = true;
    I->elements = elements;
    I->side = IDEAL_RIGHT;
    I->type = IDEAL_INDEXED;
    I->data.indexed.indices = indices;

    return I;
}

// Returns true if two Ideals are the same, else false
bool cmpIdeals(Ideal* I, Ideal* J) {
    if (!I || !J) return false;
    if (!cmpRings(I->ring, J->ring)) return false;
    if (I->card != J->card) return false;
    if (I->side != J->side) return false;

    for (int i = 0; i < I->card; i++) if (I->data.indexed.indices[i] != J->data.indexed.indices[i]) return false;
    return true;
}

// Add two Ideals of the same Ring
Ideal* addIdeals(Ideal* I, Ideal* J) {
    if (!I || !J) return NULL;
    if (!cmpRings(I->ring, J->ring)) return NULL;
    if (I->side != J->side) return NULL;

    Ring* R = I->ring;
    int* newIndices = malloc(I->card * J->card * sizeof(int));
    if (!newIndices) return NULL;

    int newCard = 0;
    for (int i = 0; i < I->card; i++) {
        for (int j = 0; j < J->card; j++) {
            int index = ringAdd(R->elements[I->data.indexed.indices[i]], R->elements[J->data.indexed.indices[j]])->index;
            bool found = false;
            for (int k = 0; k < newCard; k++) {
                if (newIndices[k] == index) {
                    found = true;
                }
            }
            if (!found) newIndices[newCard++] = index;
        }
    }

    Ideal* K;
    if (I->side == IDEAL_LEFT) K = constructLeftIdeal(R, newIndices, newCard);
    else K = constructRightIdeal(R, newIndices, newCard);
    if (!K) {
        free(newIndices);
        return NULL;
    }

    K->ring = R;
    K->side = I->side;

    return K;
}

// Multiply two Ideals of the same Ring
Ideal* multIdeals(Ideal* I, Ideal* J) {
    if (!I || !J) return NULL;
    if (!cmpRings(I->ring, J->ring)) return NULL;
    if (I->side != J->side) return NULL;

    Ring* R = I->ring;
    int* newIndices = malloc(I->card * J->card * sizeof(int));
    if (!newIndices) return NULL;

    int newCard = 0;
    for (int i = 0; i < I->card; i++) {
        for (int j = 0; j < J->card; j++) {
            int index = ringMult(R->elements[I->data.indexed.indices[i]], R->elements[J->data.indexed.indices[j]])->index;
            bool found = false;
            for (int k = 0; k < newCard; k++) {
                if (newIndices[k] == index) {
                    found = true;
                }
            }
            if (!found) newIndices[newCard++] = index;
        }
    }

    Ideal* K;
    if (I->side == IDEAL_LEFT) K = constructLeftIdeal(R, newIndices, newCard);
    else K = constructRightIdeal(R, newIndices, newCard);
    if (!K) {
        free(newIndices);
        return NULL;
    }

    K->ring = R;
    K->side = I->side;

    return K;
}

// Construct the quotient ring R / I for a two-sided Ideal I of R
Ring* quotientRing(Ring* R, Ideal* I) {
	if (!R || !I) return NULL;
	if (I->ring != R) return NULL;

	// Verify I is two-sided (required for the quotient to be well-defined)
	for (int i = 0; i < I->card; i++) {
		for (int j = 0; j < R->card; j++) {
			RingElement* left = ringMult(R->elements[j], R->elements[I->data.indexed.indices[i]]);
			RingElement* right = ringMult(R->elements[I->data.indexed.indices[i]], R->elements[j]);
			if (!left || !right || left->index < 0 || right->index < 0) return NULL;
			int lp = left->index;
			int rp = right->index;
			bool foundL = false, foundR = false;
			for (int m = 0; m < I->card; m++) {
				if (I->data.indexed.indices[m] == lp) foundL = true;
				if (I->data.indexed.indices[m] == rp) foundR = true;
			}
			if (!foundL || !foundR) return NULL;
		}
	}

	if (R->card % I->card != 0) return NULL;
	int numCosets = R->card / I->card;

	// Assign each element of R to its additive coset; pick one rep per coset
	int* cosetOf = malloc(R->card * sizeof(int));
	if (!cosetOf) return NULL;
	for (int i = 0; i < R->card; i++) cosetOf[i] = -1;

	int* reps = malloc(numCosets * sizeof(int));
	if (!reps) { free(cosetOf); return NULL; }

	int assigned = 0;
	for (int x = 0; x < R->card; x++) {
		if (cosetOf[x] != -1) continue;
		if (assigned >= numCosets) { free(cosetOf); free(reps); return NULL; }
		reps[assigned] = x;
		for (int t = 0; t < I->card; t++) {
			RingElement* yElement = ringAdd(R->elements[x], R->elements[I->data.indexed.indices[t]]);
			if (!yElement || yElement->index < 0) { free(cosetOf); free(reps); return NULL; }
			int y = yElement->index;
			cosetOf[y] = assigned;
		}
		assigned++;
	}
	if (assigned != numCosets) { free(cosetOf); free(reps); return NULL; }

	// Build element reprs "<rep>+I"
	RingElement** elements = calloc(numCosets, sizeof(RingElement*));
	if (!elements) { free(cosetOf); free(reps); return NULL; }
	for (int i = 0; i < numCosets; i++) {
		const char* rrepr = R->elements[reps[i]]->repr;
		int rlen = strlen(rrepr) + 4;
		char* repr = malloc(rlen);
		if (!repr) {
			for (int j = 0; j < i; j++) freeRingElement(elements[j]);
			free(elements); free(cosetOf); free(reps);
			return NULL;
		}
		snprintf(repr, rlen, "%s+I", rrepr);
		elements[i] = constructRingElement(NULL, repr);
		free(repr);
		if (!elements[i]) {
			for (int j = 0; j < i; j++) freeRingElement(elements[j]);
			free(elements); free(cosetOf); free(reps);
			return NULL;
		}
	}

	// Build tables
	int** addTable = malloc(numCosets * sizeof(int*));
	int** multTable = malloc(numCosets * sizeof(int*));
	if (!addTable || !multTable) {
		free(addTable); free(multTable);
		for (int j = 0; j < numCosets; j++) freeRingElement(elements[j]);
		free(elements); free(cosetOf); free(reps);
		return NULL;
	}
	for (int i = 0; i < numCosets; i++) {
		addTable[i] = malloc(numCosets * sizeof(int));
		multTable[i] = malloc(numCosets * sizeof(int));
		if (!addTable[i] || !multTable[i]) {
			for (int j = 0; j <= i; j++) { free(addTable[j]); free(multTable[j]); }
			free(addTable); free(multTable);
			for (int j = 0; j < numCosets; j++) freeRingElement(elements[j]);
			free(elements); free(cosetOf); free(reps);
			return NULL;
		}
		for (int j = 0; j < numCosets; j++) {
			RingElement* sum = ringAdd(R->elements[reps[i]], R->elements[reps[j]]);
			RingElement* prod = ringMult(R->elements[reps[i]], R->elements[reps[j]]);
			if (!sum || !prod || sum->index < 0 || prod->index < 0) {
				for (int k = 0; k <= i; k++) { free(addTable[k]); free(multTable[k]); }
				free(addTable); free(multTable);
				for (int k = 0; k < numCosets; k++) freeRingElement(elements[k]);
				free(elements); free(cosetOf); free(reps);
				return NULL;
			}
			int sumIdx = sum->index;
			int prodIdx = prod->index;
			addTable[i][j] = cosetOf[sumIdx];
			multTable[i][j] = cosetOf[prodIdx];
		}
	}

	free(cosetOf); free(reps);

	Ring* Q = constructTableRingSkipValidate(elements, addTable, multTable, numCosets);
	if (!Q) {
		for (int i = 0; i < numCosets; i++) {
			freeRingElement(elements[i]);
			free(addTable[i]); free(multTable[i]);
		}
		free(elements); free(addTable); free(multTable);
		return NULL;
	}
	for (int i = 0; i < numCosets; i++) elements[i]->ring = Q;
	return Q;
}

/* ---------- Ring homomorphisms ---------- */

// Constructs a RingHomomorphism from R to S defined by the mapping indicesMapping
RingHomomorphism* constructRingHomomorphism(Ring* domain, Ring* codomain, int* indicesMapping, int indicesMappingSize) {
    if (!domain || !codomain || !indicesMapping) return NULL;
    if (indicesMappingSize != domain->card) return NULL;
    if (!domain->isFinite || !codomain->isFinite) return NULL;
    if (!domain->elements || !codomain->elements) return NULL;

    // Validate that indicesMapping is a valid homomorphism
    for (int i = 0; i < domain->card; i++) {
        if (indicesMapping[i] < 0 || (size_t)indicesMapping[i] >= codomain->card) return NULL;
        for (int j = 0; j < domain->card; j++) {
            RingElement* domainSum = ringAdd(domain->elements[i], domain->elements[j]);
            RingElement* domainProd = ringMult(domain->elements[i], domain->elements[j]);
            RingElement* codomainSum = ringAdd(codomain->elements[indicesMapping[i]], codomain->elements[indicesMapping[j]]);
            RingElement* codomainProd = ringMult(codomain->elements[indicesMapping[i]], codomain->elements[indicesMapping[j]]);
            if (!domainSum || !domainProd || !codomainSum || !codomainProd) return NULL;
            if (domainSum->index < 0 || domainProd->index < 0 || codomainSum->index < 0 || codomainProd->index < 0) return NULL;
            if (codomainSum->index != indicesMapping[domainSum->index]) return NULL;
            if (codomainProd->index != indicesMapping[domainProd->index]) return NULL;
        }
    }

    RingHomomorphism* homo = calloc(1, sizeof(RingHomomorphism));
    if (!homo) return NULL;
    homo->domain = domain;
    homo->codomain = codomain;
    homo->type = RING_HOM_INDEXED;
    homo->data.indexed.mapping = indicesMapping;
    homo->data.indexed.count = (size_t)indicesMappingSize;
    return homo;
}

// Return the kernel of a ring homomorphism as an Ideal of homo->domain
Ideal* ringHomomorphismKernel(RingHomomorphism* homo) {
    if (!homo) return NULL;

    int* indices = malloc(homo->domain->card * sizeof(int));
    if (!indices) return NULL;
    int k = 0;
    for (int i = 0; i < homo->domain->card; i++) {
        if (homo->data.indexed.mapping[i] == 0) indices[k++] = i;
    }
    Ideal* ker = constructLeftIdeal(homo->domain, indices, k);
    if (!ker) {
        free(indices);
        return NULL;
    }

    return ker;
}

// Returns f(g), where f is a ring homomorphism and g is an element of homo->domain
RingElement* ringElementImage(RingHomomorphism* homo, RingElement* x) {
    if (!homo || !x) return NULL;
    if (!cmpRings(homo->domain, x->ring)) return NULL;

    return homo->codomain->elements[homo->data.indexed.mapping[x->index]];
}

// Returns the image of a RingHomomorphism as an Ideal
Ring* ringHomomorphismImage(RingHomomorphism* homo) {
    if (!homo) return NULL;
    
    // Create a list of all indices which are in the image
    int maxCard = homo->codomain->card;
    bool* seen = malloc(maxCard * sizeof(bool));
    if (!seen) return NULL;
    for (int i = 0; i < maxCard; i++) seen[i] = false;
    for (int i = 0; i < homo->domain->card; i++) {
        seen[homo->data.indexed.mapping[i]] = true;
    }

    // Generate a list of the indices of elements in Im(homo)
    int* indices = malloc(maxCard * sizeof(int));
    if (!indices) {
        free(seen);
        return NULL;
    }
    int k = 0;
    for (int i = 0; i < maxCard; i++) {
        if (seen[i]) indices[k++] = i;
    }

    // Generate the additive table of the ring
    int** addTable = malloc(k * sizeof(int*));
    if (!addTable) {
        free(seen);
        free(indices);
        return NULL;
    }
    for (int i = 0; i < k; i++) {
        addTable[i] = malloc(k * sizeof(int));
        if (!addTable[i]) {
            free(seen);
            free(indices);
            for (int j = 0; j < i; j++) free(addTable[j]);
            free(addTable);
            return NULL;
        }
    }
    for (int i = 0; i < k; i++) {
        for (int j = 0; j < k; j++) {
            RingElement* sum = ringAdd(homo->codomain->elements[indices[i]], homo->codomain->elements[indices[j]]);
            if (!sum || sum->index < 0) {
                free(seen);
                free(indices);
                for (int m = 0; m < k; m++) free(addTable[m]);
                free(addTable);
                return NULL;
            }
            int prodCodIndex = sum->index;
            bool found = false;
            for (int m = 0; m < k; m++) {
                if (indices[m] == prodCodIndex) {
                    addTable[i][j] = m;
                    found = true;
                    break; }
            }
            if (!found) {
                free(seen);
                free(indices);
                for (int m = 0; m < k; m++) free(addTable[m]);
                free(addTable);
                return NULL;
            }
        }
    }

    // Generate the multiplicative table of the ring
    int** multTable = malloc(k * sizeof(int*));
    if (!multTable) {
        free(seen);
        free(indices);
        for (int j = 0; j < k; j++) free(addTable[j]);
        free(addTable);
        return NULL;
    }
    for (int i = 0; i < k; i++) {
        multTable[i] = malloc(k * sizeof(int));
        if (!multTable[i]) {
            free(seen);
            free(indices);
            for (int j = 0; j < k; j++) free(addTable[j]);
            free(addTable);
            for (int j = 0; j < i; j++) free(multTable[j]);
            free(multTable);
            return NULL;
        }
    }
    for (int i = 0; i < k; i++) {
        for (int j = 0; j < k; j++) {
            RingElement* prod = ringMult(homo->codomain->elements[indices[i]], homo->codomain->elements[indices[j]]);
            if (!prod || prod->index < 0) {
                free(seen);
                free(indices);
                for (int m = 0; m < k; m++) free(addTable[m]);
                free(addTable);
                for (int m = 0; m < k; m++) free(multTable[m]);
                free(multTable);
                return NULL;
            }
            int prodCodIndex = prod->index;
            bool found = false;
            for (int m = 0; m < k; m++) {
                if (indices[m] == prodCodIndex) {
                    multTable[i][j] = m;
                    found = true;
                    break; }
            }
            if (!found) {
                free(seen);
                free(indices);
                for (int m = 0; m < k; m++) free(addTable[m]);
                free(addTable);
                for (int m = 0; m < k; m++) free(multTable[m]);
                free(multTable);
                return NULL;
            }
        }
    }

    // Generate the Group corresponding to those elements
    RingElement** elements = malloc(k * sizeof(RingElement*));
    if (!elements) {
        free(seen);
        free(indices);
        for (int i = 0; i < k; i++) free(addTable[i]);
        free(addTable);
        for (int i = 0; i < k; i++) free(multTable[i]);
        free(multTable);
        return NULL;
    }
    for (int i = 0; i < k; i++) {
        elements[i] = constructRingElement(NULL, homo->codomain->elements[indices[i]]->repr);
        if (!elements[i]) {
            for (int j = 0; j < i; j++) freeRingElement(elements[j]);
            free(elements);
            free(seen);
            free(indices);
            for (int i = 0; i < k; i++) free(addTable[i]);
            free(addTable);
            for (int i = 0; i < k; i++) free(multTable[i]);
            free(multTable);
            return NULL;
        }
    }

    Ring* image = constructTableRingSkipValidate(elements, addTable, multTable, k);
    free(seen);
    free(indices);
    if (!image) {
        for (int i = 0; i < k; i++) freeRingElement(elements[i]);
        free(elements);
        for (int i = 0; i < k; i++) free(addTable[i]);
        free(addTable);
        for (int i = 0; i < k; i++) free(multTable[i]);
        free(multTable);
        return NULL;
    }

    for (int i = 0; i < k; i++) elements[i]->ring = image;
    return image;
}

// Returns true if a RingHomomorphism is an isomorphism, else false
bool isRingIsomorphism(RingHomomorphism* homo) {
    if (!homo) return false;
    if (homo->domain->card != homo->codomain->card) return false;

    for (int i = 1; i < homo->domain->card; i++) {
        if (homo->data.indexed.mapping[i] == 0) return false;
    }
    return true;
}

/* ---------- Classifications ---------- */

// Determine if G is commutative
bool isCommutativeGroup(Group* G) {
    if (!G) return false;

    for (int i = 0; i < G->card; i++) {
	GroupElement* g = G->elements[i];
	for (int j = i; j < G->card; j++) {
	    GroupElement* h = G->elements[j];
            if (!cmpGroupElements(groupMult(g, h), groupMult(h, g))) return false;
		}
    }

    return true;
}

// Returns true if g is the inverse of h, otherwise false
bool isInverse(GroupElement* g, GroupElement* h) {
    if (!g || !h) return false;
    if (!cmpGroups(g->group, h->group)) return false;

    GroupElement* identity = groupIdentity(g->group);
    return cmpGroupElements(groupMult(g, h), identity) && cmpGroupElements(groupMult(h, g), identity);
}

// Returns true if x is the additive inverse of y, otherwise false
bool isAddInverse(RingElement* x, RingElement* y) {
    if (!x || !y) return false;
    if (!cmpRings(x->ring, y->ring)) return false;

    RingElement* zero = ringAddIdentity(x->ring);
    return cmpRingElements(ringAdd(x, y), zero) && cmpRingElements(ringAdd(y, x), zero);
}

// Returns true if x is the multplicative inverse of y, otherwise false
bool isMultInverse(RingElement* x, RingElement* y) {
    if (!x || !y) return false;
    if (!cmpRings(x->ring, y->ring)) return false;
    RingElement* one = ringMultIdentity(x->ring);
    if (!one) return false;
    if (cmpRingElements(x, x->ring->elements[0]) || cmpRingElements(y, y->ring->elements[0])) return false;

    return cmpRingElements(ringMult(x, y), one) && cmpRingElements(ringMult(y, x), one);
}

// Returns true if an element of a ring has mult inverse, otherwise false
bool hasMultInverse(RingElement* x) {
    if (!x) return false;
    if (cmpRingElements(x, x->ring->elements[0])) return false;

    for (int i = 1; i < x->ring->card; i++) {
	    if (isMultInverse(x, x->ring->elements[i])) return true;
    }

    return false;
}

// Returns true if x is a zero divisor, false otherwise
// Note: 0 is NOT considered a zero divisor
bool isZeroDivisor(RingElement* x) {
    if (!x) return false;
    if (cmpRingElements(x, x->ring->elements[0])) return false;

    RingElement* id = ringAddIdentity(x->ring);
    for (int i = 1; i < x->ring->card; i++) {
	    if (cmpRingElements(ringMult(x, x->ring->elements[i]), id) || cmpRingElements(ringMult(x->ring->elements[i], x), id)) return true;
    }

    return false;
}

// Return true if the Ring contains a zero divisor, false otherwise
// Note: 0 is NOT considered a zero divisor
bool hasZeroDivisors(Ring* R) {
    if (!R) return false;
    if (R->card == 1) return false;

    for (int i = 1; i < R->card; i++) {
	    if (isZeroDivisor(R->elements[i])) return true;
    }

    return false;
}

// Returns true if the Ring is commutative, false otherwise
bool isCommutativeRing(Ring* R) {
    if (!R) return false;

    for (int i = 0; i < R->card; i++) {
	    for (int j = i; j < R->card; j++) {
	        if (!cmpRingElements(ringMult(R->elements[i], R->elements[j]), ringMult(R->elements[j], R->elements[i]))) return false;
	    }
    }

    return true;
}

// Returns true if the Ring is an integral domain, false otherwise
bool isIntegralDomain(Ring* R) {
   if (!R) return false;
   if (!isCommutativeRing(R)) return false;
   if (!hasMultIdentity(R)) return false;
   
   return !hasZeroDivisors(R);
}

// Returns true if the Ring is a division ring, false otherwise
bool isDivisionRing(Ring* R) {
    if (!R) return false;
    if (!hasMultIdentity(R)) return false;

    for (int i = 1; i < R->card; i++) {
	    if (!hasMultInverse(R->elements[i])) return false;
    }

    return true;
}

// Returns true if the Ring is a field, false otherwise
bool isField(Ring* R) {
    if (!R) return false;
        if (!isDivisionRing(R)) return false;

    return isCommutativeRing(R);
}
