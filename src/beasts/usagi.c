#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<math.h>
#include<limits.h>
#include "usagi.h"

/* ---------- Free methods ---------- */

// Free the memory associated with a GroupElement
void freeGroupElement(GroupElement* g) {
    if (!g) return;
    free(g->repr);
    free(g);
}

// Free the memory associated with a Group
void freeGroup(Group* group) {
    if (!group) return;
    for (int i = 0; i < group->card; i++) {
		freeGroupElement(group->elements[i]);
		free(group->table[i]);
    }
    free(group->elements);
    free(group->table);
    free(group);
}

// Free the memory associated with a SubGroup
void freeSubgroup(SubGroup* subgroup) {
    if (!subgroup) return;
    free(subgroup->indices);
    free(subgroup);
}

// Free the memroy associated with a GroupCoset
void freeGroupCoset(GroupCoset* coset) {
    if (!coset) return;
    free(coset->indices);
	free(coset);
}

// Free a GroupHomomorphism
void freeGroupHomomorphism(GroupHomomorphism* homo) {
    if (!homo) return;
    free(homo->mapping);
    free(homo);
}

// Free a RingHomomorphism
void freeRingHomomorphism(RingHomomorphism* homo) {
    if (!homo) return;
    free(homo->mapping);
    free(homo);
}

// Free the memory associated with a RingElement
void freeRingElement(RingElement* x) {
    if (!x) return;
    free(x->repr);
    free(x);
}

// Free the memory associated with a Ring
void freeRing(Ring* ring) {
    if (!ring) return;
    for (int i = 0; i < ring->card; i++) {
	freeRingElement(ring->elements[i]);
	free(ring->addTable[i]);
	free(ring->multTable[i]);
    }
    free(ring->elements);
    free(ring->addTable);
    free(ring->multTable);
    free(ring);
}

/* ---------- Construct methods (basic) ---------- */

// Validate that a matrix can be a table for a Group
bool validateGroupTable(int** table, int card) {
    if (!table || card < 1) return false;

    // Verify associativity
    for (int i = 0; i < card; i++) {
		for (int j = 0; j < card; j++) {
			if (table[i][j] < 0 || table[i][j] > card-1) return false;
	        for (int k = 0; k < card; k++) {
		    	if (table[table[i][j]][k] != table[i][table[j][k]]) return false;
	        }
	    }
    }

    // Verify that an identity exists
    for (int i = 0; i < card; i++) {
	    if (table[i][0] != i || table[0][i] != i) return false;
    }

    // Verify that an inverse exists
    for (int i = 0; i < card; i++) {
	bool invExists = false;
	for (int j = 0; j < card; j++) {
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
bool validateRingTables(int** addTable, int** multTable, int card) {
    if (!addTable || !multTable || card < 1) return false;

    // Verify associativity and the distributive property
    for (int i = 0; i < card; i++) {
	    for (int j = 0; j < card; j++) {
	        if (addTable[i][j] < 0 || addTable[i][j] > card-1 || multTable[i][j] < 0 || multTable[i][j] > card-1) return false;
	        for (int k = 0; k < card; k++) {
		        if (addTable[addTable[i][j]][k] != addTable[i][addTable[j][k]]) return false;
		        if (multTable[multTable[i][j]][k] != multTable[i][multTable[j][k]]) return false;
		        if (multTable[addTable[i][j]][k] != addTable[multTable[i][k]][multTable[j][k]]) return false;
		        if (multTable[k][addTable[i][j]] != addTable[multTable[k][i]][multTable[k][j]]) return false;
	        }
	    }
    }

    // Verify additive identity
    for (int i = 0; i < card; i++) {
	    if (addTable[i][0] != i || addTable[0][i] != i) return false;
    }

    // Verify the existence of additive inverse
    for (int i = 0; i < card; i++) {
	bool invExists = false;
	for (int j = 0; j < card; j++) {
	    if (addTable[i][j] == addTable[0][0] && addTable[j][i] == addTable[0][0]) {
		    invExists = true;
		    goto skip;
	    }
	}
	skip:
	    if (!invExists) return false;
    }

    // Verify that addition is commutative
    for (int i = 0; i < card; i++) {
        for (int j = 0; j < card; j++) {
            if (addTable[i][j] != addTable[j][i]) return false;
        }
    }

    // Verify that 0*x = 0
    for (int i = 0; i < card; i++) {
	    if (multTable[0][i] != 0 || multTable[i][0] != 0) return false;
    }
    
    return true;
}

// Construct a GroupElement
GroupElement* constructGroupElement(Group* group, char* repr) {
    if (!repr || repr[0] == '\0') return NULL;

    GroupElement* g = malloc(sizeof(GroupElement));
    if (!g) return NULL;

    char* newRepr = malloc(strlen(repr)+1);
    if (!newRepr) {
	free(g);
	return NULL;
    }
    strcpy(newRepr, repr);

    g->repr = newRepr;
    g->group = group;
    g->index = -1;
    
    return g;
}

// Construct a Group
Group* constructGroup(GroupElement** elements, int** table, int tableLen) {
    if (!elements || !table) return NULL;
    if (tableLen < 1) return NULL;
    if (!validateGroupTable(table, tableLen)) return NULL;

    Group* group = malloc(sizeof(Group));
    if (!group) return NULL;

    group->elements = elements;
    group->table = table;
    group->card = tableLen;

    for (int i = 0; i < tableLen; i++){
	group->elements[i]->index = i;
    }

    return group;
}

// Construct a Group, skipping the validation step
Group* constructGroupSkipValidate(GroupElement** elements, int** table, int tableLen) {
    if (!elements || !table) return NULL;
    if (tableLen < 1) return NULL;

    Group* group = malloc(sizeof(Group));
    if (!group) return NULL;

    group->elements = elements;
    group->table = table;
    group->card = tableLen;

    for (int i = 0; i < tableLen; i++){
	group->elements[i]->index = i;
    }

    return group;
}

// Construct a RingElement
RingElement* constructRingElement(Ring* ring, char* repr) {
    if (!repr || repr[0] == '\0') return NULL;

    RingElement* x = malloc(sizeof(RingElement));
    if (!x) return NULL;

    char* newRepr = malloc(strlen(repr)+1);
    if (!newRepr) {
	free(x);
	return NULL;
    }
    strcpy(newRepr, repr);

    x->repr = newRepr;
    x->ring = ring;
    x->index = -1;
    
    return x;
}

// Construct a Ring
Ring* constructRing(RingElement** elements, int** addTable, int** multTable, int tableLen) {
    if (!elements || !addTable || !multTable) return NULL;
    if (tableLen < 1) return NULL;
    if (!validateRingTables(addTable, multTable, tableLen)) return NULL;

    Ring* ring = malloc(sizeof(Ring));
    if (!ring) return NULL;

    ring->elements = elements;
    ring->addTable = addTable;
    ring->multTable = multTable;
    ring->card = tableLen;

    for (int i = 0; i < tableLen; i++){
	ring->elements[i]->index = i;
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
    if (n == 1) return trivialGroup();

	// Construct the elements and table arrays
    GroupElement** elements = malloc(n * sizeof(GroupElement*));
    if (!elements) return NULL;
    int** table = malloc(n * sizeof(int*));
    if (!table) {
		free(elements);
		return NULL;
    }

	// Fill in the elements and table
    for (int i = 0; i < n; i++) {
		char* repr = malloc(32);
		sprintf(repr, "%d", i);
		elements[i] = constructGroupElement(NULL, repr);
		if (!elements[i]) {
	    	for (int j = 0; j < i; j++) freeGroupElement(elements[j]);
	    	free(elements);
	    	free(table);
	    	return NULL;
		}
		table[i] = malloc(n * sizeof(int));
		if (!table[i]) {
	    	for (int j = 0; j <= i; j++) freeGroupElement(elements[j]);
	    	for (int j = 0; j < i; j++) free(table[j]);
	    	free(elements);
	    	free(table);
	    	return NULL;
		}
    }
    for (int i = 0; i < n; i++) {
		for (int j = 0; j < n; j++) {
	    	table[i][j] = (i + j) % n;
		}
    }

	// Construct the group
    Group* G = constructGroup(elements, table, n);
    if (!G) {
		for (int i = 0; i < n; i++) {
	    	freeGroupElement(elements[i]);
	    	free(table[i]);
		}
		free(elements);
		free(table);
    }
	for (int i = 0; i < n; i++) elements[i]->group = G;
	
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
		product = constructProductGroup(product, components[i]);
		if (!product) {
			for (int j = 0; j < k; j++) freeGroup(components[j]);
			free(components);
			return NULL;
		}
	}

	// Flatten element reprs to (a,b,c,...) form and set the group pointer
	for (int i = 0; i < product->card; i++) {
		char* flat = flattenRepr(product->elements[i]->repr);
		if (flat) { free(product->elements[i]->repr); product->elements[i]->repr = flat; }
		product->elements[i]->group = product;
	}
	for (int i = 0; i < k; i++) freeGroup(components[i]);
	free(components);

	return product;
}

// Construct the group Sn
Group* constructSymmetricGroup(int n) {
    if (n < 1) return NULL;
    if (n == 1) return trivialGroup();

    int card = factorial(n);
    if (card < 1) return NULL;

    // Generate all permutations in lexicographic order
    int** perms = malloc(card * sizeof(int*));
    if (!perms) return NULL;
    for (int p = 0; p < card; p++) {
        perms[p] = malloc(n * sizeof(int));
        if (!perms[p]) {
            for (int j = 0; j < p; j++) free(perms[j]);
            free(perms);
            return NULL;
        }
        if (p == 0) {
            for (int i = 0; i < n; i++) perms[0][i] = i;
        } else {
            memcpy(perms[p], perms[p - 1], n * sizeof(int));
            nextPermutation(perms[p], n);
        }
    }

    // Construct the elements
    GroupElement** elements = malloc(card * sizeof(GroupElement*));
    if (!elements) {
        for (int i = 0; i < card; i++) free(perms[i]);
        free(perms);
        return NULL;
    }
    for (int p = 0; p < card; p++) {
        char* repr = permRepr(perms[p], n);
        if (!repr) {
            for (int j = 0; j < p; j++) freeGroupElement(elements[j]);
            free(elements);
            for (int j = 0; j < card; j++) free(perms[j]);
            free(perms);
            return NULL;
        }
        elements[p] = constructGroupElement(NULL, repr);
        free(repr);
        if (!elements[p]) {
            for (int j = 0; j < p; j++) freeGroupElement(elements[j]);
            free(elements);
            for (int j = 0; j < card; j++) free(perms[j]);
            free(perms);
            return NULL;
        }
    }

    // Build the Cayley table
    int** table = malloc(card * sizeof(int*));
    if (!table) {
        for (int i = 0; i < card; i++) freeGroupElement(elements[i]);
        free(elements);
        for (int i = 0; i < card; i++) free(perms[i]);
        free(perms);
        return NULL;
    }
    int* composed = malloc(n * sizeof(int));
    if (!composed) {
        free(table);
        for (int i = 0; i < card; i++) freeGroupElement(elements[i]);
        free(elements);
        for (int i = 0; i < card; i++) free(perms[i]);
        free(perms);
        return NULL;
    }
    for (int a = 0; a < card; a++) {
        table[a] = malloc(card * sizeof(int));
        if (!table[a]) {
            for (int j = 0; j < a; j++) free(table[j]);
            free(table);
            free(composed);
            for (int j = 0; j < card; j++) freeGroupElement(elements[j]);
            free(elements);
            for (int j = 0; j < card; j++) free(perms[j]);
            free(perms);
            return NULL;
        }
        for (int b = 0; b < card; b++) {
            for (int i = 0; i < n; i++) composed[i] = perms[a][perms[b][i]];
            table[a][b] = permToIndex(composed, n);
        }
    }
    free(composed);

    // Free permutations
    for (int i = 0; i < card; i++) free(perms[i]);
    free(perms);

    // Construct the group
    Group* G = constructGroupSkipValidate(elements, table, card);
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

// Construct the group An
Group* constructAlternatingGroup(int n) {
    if (n < 1) return NULL;
    if (n <= 2) return trivialGroup();

    int fullCard = factorial(n);
    if (fullCard < 2) return NULL;
    int card = fullCard / 2;

    // Generate all permutations in lexicographic order
    int** allPerms = malloc(fullCard * sizeof(int*));
    if (!allPerms) return NULL;
    for (int p = 0; p < fullCard; p++) {
        allPerms[p] = malloc(n * sizeof(int));
        if (!allPerms[p]) {
            for (int j = 0; j < p; j++) free(allPerms[j]);
            free(allPerms);
            return NULL;
        }
        if (p == 0) {
            for (int i = 0; i < n; i++) allPerms[0][i] = i;
        } else {
            memcpy(allPerms[p], allPerms[p - 1], n * sizeof(int));
            nextPermutation(allPerms[p], n);
        }
    }

    // Build mapping from full permutation index to even permutation index
    int* evenMap = malloc(fullCard * sizeof(int));
    if (!evenMap) {
        for (int i = 0; i < fullCard; i++) free(allPerms[i]);
        free(allPerms);
        return NULL;
    }
    int** perms = malloc(card * sizeof(int*));
    if (!perms) {
        free(evenMap);
        for (int i = 0; i < fullCard; i++) free(allPerms[i]);
        free(allPerms);
        return NULL;
    }
    int evenCount = 0;
    for (int i = 0; i < fullCard; i++) {
        if (permSign(allPerms[i], n) == 0) {
            evenMap[i] = evenCount;
            perms[evenCount] = allPerms[i];
            evenCount++;
        } else {
            evenMap[i] = -1;
        }
    }

    // Construct the elements
    GroupElement** elements = malloc(card * sizeof(GroupElement*));
    if (!elements) {
        free(perms);
        free(evenMap);
        for (int i = 0; i < fullCard; i++) free(allPerms[i]);
        free(allPerms);
        return NULL;
    }
    for (int p = 0; p < card; p++) {
        char* repr = permRepr(perms[p], n);
        if (!repr) {
            for (int j = 0; j < p; j++) freeGroupElement(elements[j]);
            free(elements);
            free(perms);
            free(evenMap);
            for (int j = 0; j < fullCard; j++) free(allPerms[j]);
            free(allPerms);
            return NULL;
        }
        elements[p] = constructGroupElement(NULL, repr);
        free(repr);
        if (!elements[p]) {
            for (int j = 0; j < p; j++) freeGroupElement(elements[j]);
            free(elements);
            free(perms);
            free(evenMap);
            for (int j = 0; j < fullCard; j++) free(allPerms[j]);
            free(allPerms);
            return NULL;
        }
    }

    // Build the Cayley table
    int** table = malloc(card * sizeof(int*));
    if (!table) {
        for (int i = 0; i < card; i++) freeGroupElement(elements[i]);
        free(elements);
        free(perms);
        free(evenMap);
        for (int i = 0; i < fullCard; i++) free(allPerms[i]);
        free(allPerms);
        return NULL;
    }
    int* composed = malloc(n * sizeof(int));
    if (!composed) {
        free(table);
        for (int i = 0; i < card; i++) freeGroupElement(elements[i]);
        free(elements);
        free(perms);
        free(evenMap);
        for (int i = 0; i < fullCard; i++) free(allPerms[i]);
        free(allPerms);
        return NULL;
    }
    for (int a = 0; a < card; a++) {
        table[a] = malloc(card * sizeof(int));
        if (!table[a]) {
            for (int j = 0; j < a; j++) free(table[j]);
            free(table);
            free(composed);
            for (int j = 0; j < card; j++) freeGroupElement(elements[j]);
            free(elements);
            free(perms);
            free(evenMap);
            for (int j = 0; j < fullCard; j++) free(allPerms[j]);
            free(allPerms);
            return NULL;
        }
        for (int b = 0; b < card; b++) {
            for (int i = 0; i < n; i++) composed[i] = perms[a][perms[b][i]];
            table[a][b] = evenMap[permToIndex(composed, n)];
        }
    }
    free(composed);

    // Free permutations and mapping
    free(perms);
    free(evenMap);
    for (int i = 0; i < fullCard; i++) free(allPerms[i]);
    free(allPerms);

    // Construct the group
    Group* G = constructGroupSkipValidate(elements, table, card);
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

// Construct the Dihedral group Dn
Group* constructDihedralGroup(int n) {
    if (n < 1) return NULL;
    if (n == 1) {
        // D1 = Z2 = {e, s}
        return constructZnGroup(2);
    }

    int card = 2 * n;

    // Construct the elements: r^0=e, r^1, ..., r^(n-1), s, sr, ..., sr^(n-1)
    GroupElement** elements = malloc(card * sizeof(GroupElement*));
    if (!elements) return NULL;
    for (int i = 0; i < card; i++) {
        char repr[32];
        if (i == 0)           sprintf(repr, "e");
        else if (i == 1)      sprintf(repr, "r");
        else if (i < n)       sprintf(repr, "r^%d", i);
        else if (i == n)      sprintf(repr, "s");
        else if (i == n + 1)  sprintf(repr, "sr");
        else                  sprintf(repr, "sr^%d", i - n);

        elements[i] = constructGroupElement(NULL, repr);
        if (!elements[i]) {
            for (int j = 0; j < i; j++) freeGroupElement(elements[j]);
            free(elements);
            return NULL;
        }
    }

    // Build the Cayley table
    // Index i < n  represents r^i
    // Index n+i    represents s*r^i
    int** table = malloc(card * sizeof(int*));
    if (!table) {
        for (int i = 0; i < card; i++) freeGroupElement(elements[i]);
        free(elements);
        return NULL;
    }
    for (int x = 0; x < card; x++) {
        table[x] = malloc(card * sizeof(int));
        if (!table[x]) {
            for (int j = 0; j < x; j++) free(table[j]);
            free(table);
            for (int j = 0; j < card; j++) freeGroupElement(elements[j]);
            free(elements);
            return NULL;
        }
        for (int y = 0; y < card; y++) {
            int a, b;
            if (x < n && y < n) {
                // r^a * r^b
                table[x][y] = (x + y) % n;
            } else if (x < n && y >= n) {
                // r^a * s*r^b
                a = x; b = y - n;
                table[x][y] = n + ((b - a) % n + n) % n;
            } else if (x >= n && y < n) {
                // s*r^a * r^b
                a = x - n; b = y;
                table[x][y] = n + (a + b) % n;
            } else {
                // s*r^a * s*r^b
                a = x - n; b = y - n;
                table[x][y] = ((b - a) % n + n) % n;
            }
        }
    }

    // Construct the group
    Group* G = constructGroupSkipValidate(elements, table, card);
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
    Group* G = constructGroupSkipValidate(elements, table, card);
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
    if (n == 1) return trivialRing();

	// Construct the elements and table arrays
    RingElement** elements = malloc(n * sizeof(RingElement*));
    if (!elements) return NULL;
    int** addTable = malloc(n * sizeof(int*));
    if (!addTable) {
		free(elements);
		return NULL;
    }
    int** multTable = malloc(n * sizeof(int*));
    if (!multTable) {
		free(elements);
		free(addTable);
		return NULL;
    }

	// Fill in the elements and tables
    for (int i = 0; i < n; i++) {
		char* repr = malloc(32);
		sprintf(repr, "%d", i);
		elements[i] = constructRingElement(NULL, repr);
		if (!elements[i]) {
	    	for (int j = 0; j < i; j++) freeRingElement(elements[j]);
	    	free(elements);
	    	free(addTable);
	    	free(multTable);
	    	return NULL;
		}
		addTable[i] = malloc(n * sizeof(int));
		if (!addTable[i]) {
	    	for (int j = 0; j <= i; j++) freeRingElement(elements[j]);
	    	for (int j = 0; j < i; j++) free(addTable[j]);
	    	free(elements);
	    	free(addTable);
	    	free(multTable);
	    	return NULL;
		}
		multTable[i] = malloc(n * sizeof(int));
		if (!multTable[i]) {
	    	for (int j = 0; j <= i; j++) freeRingElement(elements[j]);
	    	for (int j = 0; j <= i; j++) free(addTable[j]);
	    	for (int j = 0; j < i; j++) free(multTable[j]);
	    	free(elements);
	    	free(addTable);
	    	free(multTable);
	    	return NULL;
		}
    }
    for (int i = 0; i < n; i++) {
		for (int j = 0; j < n; j++) {
	    	addTable[i][j] = (i + j) % n;
	    	multTable[i][j] = (i * j) % n;
		}
    }

	// Construct the ring
    Ring* R = constructRing(elements, addTable, multTable, n);
    if (!R) {
		for (int i = 0; i < n; i++) {
	    	freeRingElement(elements[i]);
	    	free(addTable[i]);
	    	free(multTable[i]);
		}
		free(elements);
		free(addTable);
		free(multTable);
		return NULL;
    }
	for (int i = 0; i < n; i++) elements[i]->ring = R;

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
		product = constructProductRing(product, components[i]);
		if (!product) {
			for (int j = 0; j < k; j++) freeRing(components[j]);
			free(components);
			return NULL;
		}
	}

	// Flatten element reprs to (a,b,c,...) form and set the ring pointer
	for (int i = 0; i < product->card; i++) {
		char* flat = flattenRepr(product->elements[i]->repr);
		if (flat) { free(product->elements[i]->repr); product->elements[i]->repr = flat; }
		product->elements[i]->ring = product;
	}
	for (int i = 0; i < k; i++) freeRing(components[i]);
	free(components);

	return product;
}

// Construct a finite field with p elements
Ring* primeFiniteField(int p) {
    if (!isPrime(p)) return NULL;

    return constructZnRing(p);
}

// Construct the finite field F_{p^k} = F_p[x] / (f(x)) for an irreducible f.
Ring* constructFiniteField(int p, int k) {
	if (k < 0) return NULL;
	if (k == 0) return trivialRing();
	if (!isPrime(p)) return NULL;
	if (k == 1) return primeFiniteField(p);

	// Find an irreducible monic polynomial f of degree k over F_p
	int numMonic = intPow(p, k);
	int* f = malloc((k + 1) * sizeof(int));
	if (!f) return NULL;
	f[k] = 1;
	bool found = false;
	for (int idx = 0; idx < numMonic; idx++) {
		int n = idx;
		for (int i = 0; i < k; i++) { f[i] = n % p; n /= p; }
		if (isIrreduciblePolyMod(f, k, p)) { found = true; break; }
	}
	if (!found) { free(f); return NULL; }

	int N = intPow(p, k);

	RingElement** elements = malloc(N * sizeof(RingElement*));
	if (!elements) { free(f); return NULL; }
	int** addTable = malloc(N * sizeof(int*));
	if (!addTable) { free(elements); free(f); return NULL; }
	int** multTable = malloc(N * sizeof(int*));
	if (!multTable) { free(addTable); free(elements); free(f); return NULL; }

	// Allocate per-row arrays first so cleanup is uniform on failure
	for (int i = 0; i < N; i++) {
		elements[i] = NULL;
		addTable[i] = NULL;
		multTable[i] = NULL;
	}

	// Build elements
	int* poly = malloc(k * sizeof(int));
	if (!poly) {
		free(elements); free(addTable); free(multTable); free(f);
		return NULL;
	}
	int bufLen = 16 * k + 16;
	char* buf = malloc(bufLen);
	if (!buf) {
		free(poly); free(elements); free(addTable); free(multTable); free(f);
		return NULL;
	}
	for (int i = 0; i < N; i++) {
		int n = i;
		for (int j = 0; j < k; j++) { poly[j] = n % p; n /= p; }
		polyReprBuf(poly, k, buf, bufLen);
		elements[i] = constructRingElement(NULL, buf);
		if (!elements[i]) {
			for (int j = 0; j < i; j++) freeRingElement(elements[j]);
			free(elements); free(addTable); free(multTable);
			free(poly); free(buf); free(f);
			return NULL;
		}
		addTable[i] = malloc(N * sizeof(int));
		multTable[i] = malloc(N * sizeof(int));
		if (!addTable[i] || !multTable[i]) {
			for (int j = 0; j <= i; j++) freeRingElement(elements[j]);
			for (int j = 0; j <= i; j++) { free(addTable[j]); free(multTable[j]); }
			free(elements); free(addTable); free(multTable);
			free(poly); free(buf); free(f);
			return NULL;
		}
	}
	free(poly);
	free(buf);

	// Fill in tables
	int* a = malloc(k * sizeof(int));
	int* b = malloc(k * sizeof(int));
	int* sum = malloc(k * sizeof(int));
	int* prod = malloc((2 * k - 1) * sizeof(int));
	int* rem = malloc(k * sizeof(int));
	if (!a || !b || !sum || !prod || !rem) {
		free(a); free(b); free(sum); free(prod); free(rem);
		for (int j = 0; j < N; j++) {
			freeRingElement(elements[j]);
			free(addTable[j]); free(multTable[j]);
		}
		free(elements); free(addTable); free(multTable); free(f);
		return NULL;
	}
	for (int i = 0; i < N; i++) {
		int ni = i;
		for (int t = 0; t < k; t++) { a[t] = ni % p; ni /= p; }
		for (int j = 0; j < N; j++) {
			int nj = j;
			for (int t = 0; t < k; t++) { b[t] = nj % p; nj /= p; }

			for (int t = 0; t < k; t++) sum[t] = (a[t] + b[t]) % p;
			int sumIdx = 0;
			for (int t = k - 1; t >= 0; t--) sumIdx = sumIdx * p + sum[t];
			addTable[i][j] = sumIdx;

			for (int t = 0; t < 2 * k - 1; t++) prod[t] = 0;
			for (int u = 0; u < k; u++) {
				if (a[u] == 0) continue;
				for (int v = 0; v < k; v++) {
					prod[u + v] = (prod[u + v] + a[u] * b[v]) % p;
				}
			}
			polyRemMod(prod, 2 * k - 1, f, k, p, rem);
			int prodIdx = 0;
			for (int t = k - 1; t >= 0; t--) prodIdx = prodIdx * p + rem[t];
			multTable[i][j] = prodIdx;
		}
	}
	free(a); free(b); free(sum); free(prod); free(rem); free(f);

	Ring* R = constructRing(elements, addTable, multTable, N);
	if (!R) {
		for (int i = 0; i < N; i++) {
			freeRingElement(elements[i]);
			free(addTable[i]); free(multTable[i]);
		}
		free(elements); free(addTable); free(multTable);
		return NULL;
	}
	for (int i = 0; i < N; i++) elements[i]->ring = R;
	return R;
}

// Construct the additive group of a Ring
Group* constructAddGroup(Ring* R) {
    if (!R) return NULL;

    int card = R->card;
    Group* G = malloc(sizeof(Group));
    GroupElement** elements = calloc((size_t)card, sizeof(GroupElement*));
    int** table = calloc((size_t)card, sizeof(int*));
    if (!G || !elements || !table) {
        free(G);
        freeGroupConstructionData(elements, table, card);
        return NULL;
    }

    for (int i = 0; i < card; i++) {
        const char* repr = (R->elements && R->elements[i] && R->elements[i]->repr) ? R->elements[i]->repr : "?";
        elements[i] = constructGroupElement(NULL, (char*)repr);
        table[i] = malloc((size_t)card * sizeof(int));
        if (!elements[i] || !table[i]) {
            free(G);
            freeGroupConstructionData(elements, table, card);
            return NULL;
        }
        for (int j = 0; j < card; j++) table[i][j] = R->addTable[i][j];
    }

    G->elements = elements;
    G->table = table;
    G->card = card;
    for (int i = 0; i < card; i++) {
        G->elements[i]->group = G;
        G->elements[i]->index = i;
    }
    return G;
}

// Construct the multiplicative unit group of a Ring
Group* constructUnitGroup(Ring* R) {
    if (!R || !hasMultIdentity(R)) return NULL;

    int* unitIndices = malloc((size_t)R->card * sizeof(int));
    int* indexMap = malloc((size_t)R->card * sizeof(int));
    if (!unitIndices || !indexMap) {
        free(unitIndices);
        free(indexMap);
        return NULL;
    }

    for (int i = 0; i < R->card; i++) indexMap[i] = -1;

    int unitCount = 0;
    unitIndices[unitCount] = 1;
    indexMap[1] = unitCount++;
    for (int i = 2; i < R->card; i++) {
        if (hasMultInverse(R->elements[i])) {
            indexMap[i] = unitCount;
            unitIndices[unitCount++] = i;
        }
    }

    Group* G = malloc(sizeof(Group));
    GroupElement** elements = calloc((size_t)unitCount, sizeof(GroupElement*));
    int** table = calloc((size_t)unitCount, sizeof(int*));
    if (!G || !elements || !table) {
        free(G);
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
            free(G);
            free(unitIndices);
            free(indexMap);
            freeGroupConstructionData(elements, table, unitCount);
            return NULL;
        }
        for (int j = 0; j < unitCount; j++) {
            int product = R->multTable[unitIndices[i]][unitIndices[j]];
            if (product < 0 || product >= R->card || indexMap[product] < 0) {
                free(G);
                free(unitIndices);
                free(indexMap);
                freeGroupConstructionData(elements, table, unitCount);
                return NULL;
            }
            table[i][j] = indexMap[product];
        }
    }

    G->elements = elements;
    G->table = table;
    G->card = unitCount;
    for (int i = 0; i < unitCount; i++) {
        G->elements[i]->group = G;
        G->elements[i]->index = i;
    }

    free(unitIndices);
    free(indexMap);
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

    for (int i = 0; i < H->card; i++) if (H->indices[i] != K->indices[i]) return false;
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
            if (A->indices[i] == B->indices[j]) {
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

    for (int i = 0; i < G->card; i++) {
        if (cmpGroupElements(g, G->elements[i])) return true;
    }

    return false;
}

// Return true if g is the identity of G, else false
bool isGroupIdentity(Group* G, GroupElement* g) {
    if (!G || !g) return false;

    return g == G->elements[0];
}

// Return the identity element of G
GroupElement* groupIdentity(Group* G) {
    if (!G) return NULL;

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
    if (R->card == 1) return false;

    RingElement* candidate = R->elements[1];
    if (!candidate) return false;
    for (int i = 0; i < R->card; i++) {
	    if (R->multTable[candidate->index][R->elements[i]->index] != R->elements[i]->index) return false;
	    if (R->multTable[R->elements[i]->index][candidate->index] != R->elements[i]->index) return false;
    }

    return true;
}

// Return true if x is the additive identity of R, else false
bool isRingMultIdentity(Ring* R, RingElement* x) {
    if (!R || !x) return false;
	
    if (!hasMultIdentity(R)) return false;
    return x == R->elements[1];
}

// Return the additive identity element of R
RingElement* ringMultIdentity(Ring* R) {
    if (!R) return NULL;

    if (!hasMultIdentity(R)) return NULL;
    return R->elements[1];
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

// Return the product of two elements of a Group
GroupElement* groupMult(GroupElement* g, GroupElement* h) {
    if (!g || !h) return NULL;
    if (g->group != h->group) return NULL;

    return g->group->elements[g->group->table[g->index][h->index]];
}

// Return the sum of two elements of a Ring
RingElement* ringAdd(RingElement* x, RingElement* y) {
    if (!x || !y) return NULL;
    if (x->ring != y->ring) return NULL;

    return x->ring->elements[x->ring->addTable[x->index][y->index]];
}

// Return the product of two elements of a Ring
RingElement* ringMult(RingElement* x, RingElement* y) {
    if (!x || !y) return NULL;
    if (x->ring != y->ring) return NULL;

    return x->ring->elements[x->ring->multTable[x->index][y->index]];
}

// Return the inverse of an element of a Group
GroupElement* groupInverse(GroupElement* g) {
    if (!g) return NULL;

    Group* G = g->group;
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

	Group* G = constructGroup(elements, table, 1);
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

	Ring* R = constructRing(elements, addTable, multTable, 1);
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

    // Check that all indices are valid
    for (int i = 0; i < indicesLen; i++) {
        if (indices[i] < 0 || indices[i] >= G->card) return NULL;
    }


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
            GroupElement* g = groupMult(G->elements[indices[i]], groupInverse(G->elements[indices[j]]));
            bool found = false;
            for (int k = 0; k < indicesLen; k++) {
                if (cmpGroupElements(G->elements[indices[k]], g)) {
                    found = true;
                    break;
                }
            }
            if (!found) return NULL;
        }
    }

    SubGroup* H = malloc(sizeof(SubGroup));
    if (!H) {
        return NULL;
    }
    H->ambient = G;
    H->indices = indices;
    H->card = indicesLen;

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

    for (int i = 0; i < G->card; i++) {
		for (int j = 0; j < H->card; j++) {
			GroupElement* g = G->elements[i];
			GroupElement* h = G->elements[H->indices[j]];
			GroupElement* conj = groupMult(groupMult(g, h), groupInverse(g));
			bool found = false;
			for (int k = 0; k < H->card; k++) {
				if (cmpGroupElements(G->elements[H->indices[k]], conj)) {
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
                int prodIndex = G->table[i][j];
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
    if (!G) return NULL;

    bool* seen = calloc(G->card, sizeof(bool));
    if (!seen) return NULL;

    int n = 0;
    for (int i = 0; i < G->card; i++) {
        GroupElement* conj = groupElementConjugate(G->elements[i], g);
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

    int* indices = malloc(G->card * sizeof(int));
    if (!indices) return NULL;
    int k = 0;

    for (int i = 0; i < G->card; i++) {
        GroupElement* g = G->elements[i];
        bool preserved = true;
        for (int j = 0; j < H->card; j++) {
            GroupElement* h = G->elements[H->indices[j]];
            GroupElement* conj = groupElementConjugate(g, h);
            bool inH = false;
            for (int m = 0; m < H->card; m++) {
                if (conj->index == H->indices[m]) { inH = true; break; }
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

    // Collect all conjugates ghg^{-1} for g in G, h in H
    bool* inSet = calloc(G->card, sizeof(bool));
    if (!inSet) return NULL;
    inSet[0] = true;  // identity is always included

    for (int i = 0; i < G->card; i++) {
        for (int j = 0; j < H->card; j++) {
            GroupElement* conj = groupElementConjugate(G->elements[i], G->elements[H->indices[j]]);
            if (!conj) {
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
		if (cmpGroupElements(g, H->ambient->elements[H->indices[i]])) return true;
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
			if (K->indices[i] == H->indices[j]) {
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

	int* indices = malloc(H->card * sizeof(int));
	if (!indices) return NULL;
	for (int i = 0; i < H->card; i++) {
	    GroupElement* conj = groupElementConjugate(g, H->ambient->elements[H->indices[i]]);
		if (!conj) {
			free(indices);
			return NULL;
		}
	    for (int j = 0; j < g->group->card; j++) {
		    if (cmpGroupElements(conj, g->group->elements[j])) {
			    indices[i] = j;
			    break;
		    }
	    }
	}

	SubGroup* K = constructSubgroup(H->ambient, indices, H->card);
	return K;
}

// List all subgroups of G.
// Sets *count to the number of subgroups returned.
// Caller is responsible for calling freeSubgroup on each and free on the array.
SubGroup** listAllSubgroups(Group* G, int* count) {
    if (!G || !count) return NULL;

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
                    if (H->indices[k] == gi) { inH = true; break; }
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
                for (int k = 0; k < H->card; k++) gens[k] = H->indices[k];
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
    for (int hi = 0; hi < H->card; hi++) {
        if (H->indices[hi] == 0) continue; // skip identity
        GroupElement* h = G->elements[H->indices[hi]];
        bool inAllConjugates = true;
        for (int gi = 0; gi < G->card; gi++) {
            GroupElement* conj = groupElementConjugate(G->elements[gi], h);
            if (!conj) return false;
            bool inH = false;
            for (int m = 0; m < H->card; m++) {
                if (conj->index == H->indices[m]) { inH = true; break; }
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
// the identity is at position 0 and the rest follow H->indices' order. The
// multiplication table is the restriction of G's table to H.
Group* subgroupAsGroup(SubGroup* H) {
    if (!H || !H->ambient) return NULL;
    Group* G = H->ambient;
    int n = H->card;
    if (n < 1) return NULL;

    // Permute H->indices so the ambient identity (index 0) comes first
    int* localToAmbient = malloc(n * sizeof(int));
    if (!localToAmbient) return NULL;
    int idPos = -1;
    for (int i = 0; i < n; i++) {
        if (H->indices[i] == 0) { idPos = i; break; }
    }
    if (idPos < 0) { free(localToAmbient); return NULL; }
    localToAmbient[0] = 0;
    int next = 1;
    for (int i = 0; i < n; i++) {
        if (i != idPos) localToAmbient[next++] = H->indices[i];
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
            int prodAmbient = G->table[localToAmbient[i]][localToAmbient[j]];
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

    Group* Hg = constructGroupSkipValidate(elements, table, n);
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
		if (elementOrder(H->ambient, H->ambient->elements[H->indices[i]]) == H->card) return true;
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
		if (g->index == coset->indices[i]) return true;
	}

	return false;
}

// Generate the coset gH of a subgroup H
GroupCoset* generateLeftGroupCoset(SubGroup* H, GroupElement* g) {
	if (!H || !g) return NULL;
	if (!cmpGroups(g->group, H->ambient)) return NULL;

	int* indices = malloc(H->card * sizeof(int));
    if (!indices) return NULL;
	for (int i = 0; i < H->card; i++) {
	    GroupElement* prod = groupMult(g, g->group->elements[H->indices[i]]);
        if (!prod) {
            free(indices);
            return NULL;
        }
	    for (int j = 0; j < g->group->card; j++) {
		    if (cmpGroupElements(prod, g->group->elements[j])) {
			    indices[i] = j;
			    break;
		    }
	    }
	}

	GroupCoset* coset = malloc(sizeof(GroupCoset));
    if (!coset) {
        free(indices);
        return NULL;
    }
    coset->group = H->ambient;
	coset->subgroup = H;
	coset->indices = indices;
	coset->isLeft = true;

	return coset;
}

// Generate the coset Hg of a subgroup H
GroupCoset* generateRightGroupCoset(SubGroup* H, GroupElement* g) {
	if (!H || !g) return NULL;
	if (!cmpGroups(g->group, H->ambient)) return NULL;

	int* indices = malloc(H->card * sizeof(int));
    if (!indices) return NULL;
	for (int i = 0; i < H->card; i++) {
	    GroupElement* prod = groupMult(g->group->elements[H->indices[i]], g);
        if (!prod) {
            free(indices);
            return NULL;
        }
	    for (int j = 0; j < g->group->card; j++) {
		    if (cmpGroupElements(prod, g->group->elements[j])) {
			    indices[i] = j;
			    break;
		    }
	    }
	}

	GroupCoset* coset = malloc(sizeof(GroupCoset));
    if (!coset) {
        free(indices);
        return NULL;
    }
    coset->group = H->ambient;
	coset->subgroup = H;
	coset->indices = indices;
	coset->isLeft = false;

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
				visited[coset->indices[j]] = true;
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
				visited[coset->indices[j]] = true;
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
		snprintf(repr, 32, "%sN", G->elements[cosetList[i]->indices[0]]->repr);
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
			GroupElement* prod = groupMult(G->elements[cosetList[i]->indices[0]], G->elements[cosetList[j]->indices[0]]);
			for (int k = 0; k < numCosets; k++) {
				if (isInGroupCoset(cosetList[k], prod)) {
					table[i][j] = k;
					break;
				}
			}
		}
	}

	for (int j = 0; j < numCosets; j++) freeGroupCoset(cosetList[j]);
	free(cosetList);
	Group* H = constructGroup(elements, table, numCosets);
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

    int newCard = G->card * H->card;

    // Allocate elements array
    GroupElement** elements = calloc(newCard, sizeof(GroupElement*));
    if (!elements) return NULL;

    // Build element representations as "(g,h)"
    for (int i = 0; i < G->card; i++) {
        for (int j = 0; j < H->card; j++) {
            int index = i * H->card + j;

            // Msut be size+4 for "(", ",", ")", and null terminator
            int reprLen = strlen(G->elements[i]->repr) + strlen(H->elements[j]->repr) + 4;
            char* repr = malloc(reprLen);
            if (!repr) {
                for (int m = 0; m < index; m++) freeGroupElement(elements[m]);
                free(elements);
                return NULL;
            }
            snprintf(repr, reprLen, "(%s,%s)", G->elements[i]->repr, H->elements[j]->repr);

            elements[index] = constructGroupElement(NULL, repr);
            free(repr);
            if (!elements[index]) {
                for (int m = 0; m < index; m++) freeGroupElement(elements[m]);
                free(elements);
                return NULL;
            }
        }
    }

    // Build the Cayley table
    int** table = malloc(newCard * sizeof(int*));
    if (!table) {
        for (int m = 0; m < newCard; m++) freeGroupElement(elements[m]);
        free(elements);
        return NULL;
    }

    for (int a = 0; a < newCard; a++) {
        table[a] = malloc(newCard * sizeof(int));
        if (!table[a]) {
            for (int m = 0; m < a; m++) free(table[m]);
            free(table);
            for (int m = 0; m < newCard; m++) freeGroupElement(elements[m]);
            free(elements);
            return NULL;
        }

        int i = a / H->card;   // G-component of left operand
        int j = a % H->card;   // H-component of left operand

        for (int b = 0; b < newCard; b++) {
            int k = b / H->card;   // G-component of right operand
            int l = b % H->card;   // H-component of right operand

            int gProdindex = G->table[i][k];
            int hProdindex = H->table[j][l];
            table[a][b] = gProdindex * H->card + hProdindex;
        }
    }

    Group* P = constructGroupSkipValidate(elements, table, newCard);
    if (!P) {
        for (int m = 0; m < newCard; m++) free(table[m]);
        free(table);
        for (int m = 0; m < newCard; m++) freeGroupElement(elements[m]);
        free(elements);
        return NULL;
    }

    // Assign the group pointer to each element
    for (int m = 0; m < newCard; m++) elements[m]->group = P;

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
        freeGroup(prod);
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
        freeRing(prod);
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

    int newCard = R->card * S->card;

    // Allocate elements array
    RingElement** elements = calloc(newCard, sizeof(RingElement*));
    if (!elements) return NULL;

    // Build element representations as "(x,y)"
    for (int i = 0; i < R->card; i++) {
        for (int j = 0; j < S->card; j++) {
            int index = i * S->card + j;

            // Must be size+4 for "(", ",", ")", and null terminator
            int reprLen = strlen(R->elements[i]->repr) + strlen(S->elements[j]->repr) + 4;
            char* repr = malloc(reprLen);
            if (!repr) {
                for (int m = 0; m < index; m++) freeRingElement(elements[m]);
                free(elements);
                return NULL;
            }
            snprintf(repr, reprLen, "(%s,%s)", R->elements[i]->repr, S->elements[j]->repr);

            elements[index] = constructRingElement(NULL, repr);
            free(repr);
            if (!elements[index]) {
                for (int m = 0; m < index; m++) freeRingElement(elements[m]);
                free(elements);
                return NULL;
            }
        }
    }

    // Build the addition and multiplication tables
    int** addTable = malloc(newCard * sizeof(int*));
    if (!addTable) {
        for (int m = 0; m < newCard; m++) freeRingElement(elements[m]);
        free(elements);
        return NULL;
    }
    int** multTable = malloc(newCard * sizeof(int*));
    if (!multTable) {
        free(addTable);
        for (int m = 0; m < newCard; m++) freeRingElement(elements[m]);
        free(elements);
        return NULL;
    }

    for (int a = 0; a < newCard; a++) {
        addTable[a] = malloc(newCard * sizeof(int));
        if (!addTable[a]) {
            for (int m = 0; m < a; m++) free(addTable[m]);
            free(addTable);
            free(multTable);
            for (int m = 0; m < newCard; m++) freeRingElement(elements[m]);
            free(elements);
            return NULL;
        }
        multTable[a] = malloc(newCard * sizeof(int));
        if (!multTable[a]) {
            for (int m = 0; m <= a; m++) free(addTable[m]);
            for (int m = 0; m < a; m++) free(multTable[m]);
            free(addTable);
            free(multTable);
            for (int m = 0; m < newCard; m++) freeRingElement(elements[m]);
            free(elements);
            return NULL;
        }

        int i = a / S->card;   // R-component of left operand
        int j = a % S->card;   // S-component of left operand

        for (int b = 0; b < newCard; b++) {
            int k = b / S->card;   // R-component of right operand
            int l = b % S->card;   // S-component of right operand

            int rAddIndex = R->addTable[i][k];
            int sAddIndex = S->addTable[j][l];
            addTable[a][b] = rAddIndex * S->card + sAddIndex;

            int rMultIndex = R->multTable[i][k];
            int sMultIndex = S->multTable[j][l];
            multTable[a][b] = rMultIndex * S->card + sMultIndex;
        }
    }

    Ring* P = constructRing(elements, addTable, multTable, newCard);
    if (!P) {
        for (int m = 0; m < newCard; m++) free(addTable[m]);
        free(addTable);
        for (int m = 0; m < newCard; m++) free(multTable[m]);
        free(multTable);
        for (int m = 0; m < newCard; m++) freeRingElement(elements[m]);
        free(elements);
        return NULL;
    }

    // Assign the ring pointer to each element
    for (int m = 0; m < newCard; m++) elements[m]->ring = P;

    return P;
}

/* ---------- Group homomorphisms ---------- */

// Constructs a GroupHomomorphism from G to H defined by the mapping indicesMapping
GroupHomomorphism* constructGroupHomomorphism(Group* domain, Group* codomain, int* indicesMapping, int indicesMappingSize) {
    if (!domain || !codomain || !indicesMapping) return NULL;
    if (indicesMappingSize != domain->card) return NULL;

    // Validate that indicesMapping is a valid homomorphism
    for (int i = 0; i < domain->card; i++) {
        for (int j = 0; j < domain->card; j++) {
            if (!cmpGroupElements(groupMult(codomain->elements[indicesMapping[i]], codomain->elements[indicesMapping[j]]), codomain->elements[indicesMapping[groupMult(domain->elements[i], domain->elements[j])->index]])) {
                return NULL;
            }
        }
    }

    GroupHomomorphism* homo = malloc(sizeof(GroupHomomorphism));
    if (!homo) return NULL;
    homo->domain = domain;
    homo->codomain = codomain;
    homo->mapping = indicesMapping;
    return homo;
}

// Return the kernel of a homomorphism as a SubGroup of homo->domain
SubGroup* groupHomomorphismKernel(GroupHomomorphism* homo) {
    if (!homo) return NULL;

    int* indices = malloc(homo->domain->card * sizeof(int));
    if (!indices) return NULL;
    int k = 0;
    for (int i = 0; i < homo->domain->card; i++) {
        if (homo->mapping[i] == 0) indices[k++] = i;
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

    return homo->codomain->elements[homo->mapping[g->index]];
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
        seen[homo->mapping[i]] = true;
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
            int prodCodIndex = homo->codomain->table[indices[i]][indices[j]];
            for (int m = 0; m < k; m++) {
                if (indices[m] == prodCodIndex) { table[i][j] = m; break; }
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

    Group* image = constructGroup(elements, table, k);
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
        if (homo->mapping[i] == 0) return false;
    }
    return true;
}

/* ---------- Ideals, subrings, and quotient rings ---------- */

// Construct a SubRing of a Ring
SubRing* constructSubring(Ring* R, int* indices, int indicesLen) {
    if (!R || !indices) return NULL;

    // Check that all indices are valid
    for (int i = 0; i < indicesLen; i++) {
        if (indices[i] < 0 || indices[i] >= R->card) return NULL;
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
            RingElement* x = ringAdd(R->elements[indices[i]], ringAddInverse(R->elements[indices[j]]));
            bool found = false;
            for (int k = 0; k < indicesLen; k++) {
                if (cmpRingElements(R->elements[indices[k]], x)) {
                    found = true;
                    break;
                }
            }
            if (!found) return NULL;
        }
    }

    // Check that the subring is closed under multiplication
    for (int i = 0; i < indicesLen; i++) {
        for (int j = 0; j < indicesLen; j++) {
            RingElement* x = ringMult(R->elements[indices[i]], R->elements[indices[j]]);
            bool found = false;
            for (int k = 0; k < indicesLen; k++) {
                if (cmpRingElements(R->elements[indices[k]], x)) {
                    found = true;
                    break;
                }
            }
            if (!found) return NULL;
        }
    }

    SubRing* S = malloc(sizeof(SubRing));
    if (!S) {
        return NULL;
    }
    S->ambient = R;
    S->indices = indices;
    S->card = indicesLen;

    return S;
}

// Returns true if two SubRings are equal, else false
bool cmpSubrings(SubRing* S, SubRing* T) {
    if (!S || !T) return false;
    if (!cmpRings(S->ambient, T->ambient)) return false;
    if (S->card != T->card) return false;

    for (int i = 0; i < S->card; i++) if (S->indices[i] != T->indices[i]) return false;
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

    // Check that all indices are valid
    for (int i = 0; i < indicesLen; i++) {
        if (indices[i] < 0 || indices[i] >= R->card) return NULL;
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
            RingElement* x = ringAdd(R->elements[indices[i]], ringAddInverse(R->elements[indices[j]]));
            bool found = false;
            for (int k = 0; k < indicesLen; k++) {
                if (cmpRingElements(R->elements[indices[k]], x)) {
                    found = true;
                    break;
                }
            }
            if (!found) return NULL;
        }
    }

    // Check that, for all r in R and x in I, rx is in I
    for (int i = 0; i < indicesLen; i++) {
        for (int j = 0; j < R->card; j++) {
            RingElement* x = ringMult(R->elements[j], R->elements[indices[i]]);
            bool found = false;
            for (int k = 0; k < indicesLen; k++) {
                if (cmpRingElements(R->elements[indices[k]], x)) {
                    found = true;
                    break;
                }
            }
            if (!found) return NULL;
        }
    }

    Ideal* I = malloc(sizeof(Ideal));
    if (!I) {
        return NULL;
    }
    I->ring = R;
    I->indices = indices;
    I->card = indicesLen;
    I->isLeft = true;

    return I;
}

// Construct a right Ideal of a Ring
Ideal* constructRightIdeal(Ring* R, int* indices, int indicesLen) {
    if (!R || !indices) return NULL;
    if (indicesLen < 1) return NULL;

    // Check that all indices are valid
    for (int i = 0; i < indicesLen; i++) {
        if (indices[i] < 0 || indices[i] >= R->card) return NULL;
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
            RingElement* x = ringAdd(R->elements[indices[i]], ringAddInverse(R->elements[indices[j]]));
            bool found = false;
            for (int k = 0; k < indicesLen; k++) {
                if (cmpRingElements(R->elements[indices[k]], x)) {
                    found = true;
                    break;
                }
            }
            if (!found) return NULL;
        }
    }

    // Check that, for all r in R and x in I, xr is in I
    for (int i = 0; i < indicesLen; i++) {
        for (int j = 0; j < R->card; j++) {
            RingElement* x = ringMult(R->elements[indices[i]], R->elements[j]);
            bool found = false;
            for (int k = 0; k < indicesLen; k++) {
                if (cmpRingElements(R->elements[indices[k]], x)) {
                    found = true;
                    break;
                }
            }
            if (!found) return NULL;
        }
    }

    Ideal* I = malloc(sizeof(Ideal));
    if (!I) {
        return NULL;
    }
    I->ring = R;
    I->indices = indices;
    I->card = indicesLen;
    I->isLeft = false;

    return I;
}

// Returns true if two Ideals are the same, else false
bool cmpIdeals(Ideal* I, Ideal* J) {
    if (!I || !J) return false;
    if (!cmpRings(I->ring, J->ring)) return false;
    if (I->card != J->card) return false;
    if (I->isLeft != J->isLeft) return false;

    for (int i = 0; i < I->card; i++) if (I->indices[i] != J->indices[i]) return false;
    return true;
}

// Add two Ideals of the same Ring
Ideal* addIdeals(Ideal* I, Ideal* J) {
    if (!I || !J) return NULL;
    if (!cmpRings(I->ring, J->ring)) return NULL;
    if (I->isLeft != J->isLeft) return NULL;

    Ring* R = I->ring;
    int* newIndices = malloc(I->card * J->card * sizeof(int));
    if (!newIndices) return NULL;

    int newCard = 0;
    for (int i = 0; i < I->card; i++) {
        for (int j = 0; j < J->card; j++) {
            int index = ringAdd(R->elements[I->indices[i]], R->elements[J->indices[j]])->index;
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
    if (I->isLeft) K = constructLeftIdeal(R, newIndices, newCard);
    else K = constructRightIdeal(R, newIndices, newCard);
    if (!K) {
        free(newIndices);
        return NULL;
    }

    K->ring = R;
    K->isLeft = I->isLeft;

    return K;
}

// Multiply two Ideals of the same Ring
Ideal* multIdeals(Ideal* I, Ideal* J) {
    if (!I || !J) return NULL;
    if (!cmpRings(I->ring, J->ring)) return NULL;
    if (I->isLeft != J->isLeft) return NULL;

    Ring* R = I->ring;
    int* newIndices = malloc(I->card * J->card * sizeof(int));
    if (!newIndices) return NULL;

    int newCard = 0;
    for (int i = 0; i < I->card; i++) {
        for (int j = 0; j < J->card; j++) {
            int index = ringMult(R->elements[I->indices[i]], R->elements[J->indices[j]])->index;
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
    if (I->isLeft) K = constructLeftIdeal(R, newIndices, newCard);
    else K = constructRightIdeal(R, newIndices, newCard);
    if (!K) {
        free(newIndices);
        return NULL;
    }

    K->ring = R;
    K->isLeft = I->isLeft;

    return K;
}

// Construct the quotient ring R / I for a two-sided Ideal I of R
Ring* quotientRing(Ring* R, Ideal* I) {
	if (!R || !I) return NULL;
	if (I->ring != R) return NULL;

	// Verify I is two-sided (required for the quotient to be well-defined)
	for (int i = 0; i < I->card; i++) {
		for (int j = 0; j < R->card; j++) {
			int lp = R->multTable[j][I->indices[i]];
			int rp = R->multTable[I->indices[i]][j];
			bool foundL = false, foundR = false;
			for (int m = 0; m < I->card; m++) {
				if (I->indices[m] == lp) foundL = true;
				if (I->indices[m] == rp) foundR = true;
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
			int y = R->addTable[x][I->indices[t]];
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
			int sumIdx = R->addTable[reps[i]][reps[j]];
			int prodIdx = R->multTable[reps[i]][reps[j]];
			addTable[i][j] = cosetOf[sumIdx];
			multTable[i][j] = cosetOf[prodIdx];
		}
	}

	free(cosetOf); free(reps);

	Ring* Q = constructRing(elements, addTable, multTable, numCosets);
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

    // Validate that indicesMapping is a valid homomorphism
    for (int i = 0; i < domain->card; i++) {
        for (int j = 0; j < domain->card; j++) {
            if (!cmpRingElements(ringAdd(codomain->elements[indicesMapping[i]], codomain->elements[indicesMapping[j]]), codomain->elements[indicesMapping[ringAdd(domain->elements[i], domain->elements[j])->index]]) || !cmpRingElements(ringMult(codomain->elements[indicesMapping[i]], codomain->elements[indicesMapping[j]]), codomain->elements[indicesMapping[ringMult(domain->elements[i], domain->elements[j])->index]])) {
                return NULL;
            }
        }
    }

    RingHomomorphism* homo = malloc(sizeof(RingHomomorphism));
    if (!homo) return NULL;
    homo->domain = domain;
    homo->codomain = codomain;
    homo->mapping = indicesMapping;
    return homo;
}

// Return the kernel of a ring homomorphism as an Ideal of homo->domain
Ideal* ringHomomorphismKernel(RingHomomorphism* homo) {
    if (!homo) return NULL;

    int* indices = malloc(homo->domain->card * sizeof(int));
    if (!indices) return NULL;
    int k = 0;
    for (int i = 0; i < homo->domain->card; i++) {
        if (homo->mapping[i] == 0) indices[k++] = i;
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

    return homo->codomain->elements[homo->mapping[x->index]];
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
        seen[homo->mapping[i]] = true;
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
            int prodCodIndex = homo->codomain->addTable[indices[i]][indices[j]];
            for (int m = 0; m < k; m++) {
                if (indices[m] == prodCodIndex) {
                    addTable[i][j] = m;
                    break; }
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
            int prodCodIndex = homo->codomain->multTable[indices[i]][indices[j]];
            for (int m = 0; m < k; m++) {
                if (indices[m] == prodCodIndex) {
                    multTable[i][j] = m;
                    break; }
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

    Ring* image = constructRing(elements, addTable, multTable, k);
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
        if (homo->mapping[i] == 0) return false;
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

    return (g->group->table[g->index][h->index] == g->group->elements[0]->index);
}

// Returns true if x is the additive inverse of y, otherwise false
bool isAddInverse(RingElement* x, RingElement* y) {
    if (!x || !y) return false;
    if (!cmpRings(x->ring, y->ring)) return false;

    return (x->ring->addTable[x->index][y->index] == x->ring->elements[0]->index);
}

// Returns true if x is the multplicative inverse of y, otherwise false
bool isMultInverse(RingElement* x, RingElement* y) {
    if (!x || !y) return false;
    if (!cmpRings(x->ring, y->ring)) return false;
    if (!hasMultIdentity(x->ring)) return false;
    if (cmpRingElements(x, x->ring->elements[0]) || cmpRingElements(y, y->ring->elements[0])) return false;

    return (x->ring->multTable[x->index][y->index] == x->ring->elements[1]->index) &&
           (x->ring->multTable[y->index][x->index] == x->ring->elements[1]->index);
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
