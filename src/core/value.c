#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include "value.h"
#include "ookami.h"
#include "poni.h"
#include "sokko.h"
#include "usagi.h"

/* ---------- Helper methods ---------- */

// Duplicate a C string (returns NULL on NULL input)
static char* dupstr(const char* s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char* r = malloc(n + 1);
    memcpy(r, s, n + 1);
    return r;
}

static void printInlineVector(Vector* vector) {
    if (!vector) {
        printf("null");
        return;
    }
    putchar('[');
    for (int i = 0; i < vector->numRows; i++) {
        if (i) printf(", ");
        MatrixElement elem = getEntry((Matrix*)vector, i, 0);
        if (elem.isComplex) {
            printf("(%g + %gi)", elem.value.complex.real, elem.value.complex.imag);
        } else {
            printf("%g", elem.value.real);
        }
    }
    putchar(']');
}

static void printInlineGroup(Group* group) {
    if (!group) {
        printf("<group null>");
        return;
    }
    printf("<group card=%d; elements=[", group->card);
    int limit = group->card < 6 ? group->card : 6;
    for (int i = 0; i < limit; i++) {
        if (i) printf(", ");
        printf("%s", (group->elements && group->elements[i] && group->elements[i]->repr)
                         ? group->elements[i]->repr
                         : "?");
    }
    if (group->card > limit) printf(", ...");
    printf("]>");
}

static void printInlineRing(Ring* ring) {
    if (!ring) {
        printf("<ring null>");
        return;
    }
    printf("<ring card=%d; elements=[", ring->card);
    int limit = ring->card < 6 ? ring->card : 6;
    for (int i = 0; i < limit; i++) {
        if (i) printf(", ");
        printf("%s", (ring->elements && ring->elements[i] && ring->elements[i]->repr)
                         ? ring->elements[i]->repr
                         : "?");
    }
    if (ring->card > limit) printf(", ...");
    printf("]>");
}

static void printInlineSubgroup(SubGroup* subgroup) {
    if (!subgroup) {
        printf("<subgroup null>");
        return;
    }
    printf("<subgroup of ");
    printInlineGroup(subgroup->ambient);
    printf("; card=%d; elements=[", subgroup->card);
    int limit = subgroup->card < 6 ? subgroup->card : 6;
    for (int i = 0; i < limit; i++) {
        if (i) printf(", ");
        int idx = subgroup->indices ? subgroup->indices[i] : -1;
        GroupElement* element = (subgroup->ambient && idx >= 0 && idx < subgroup->ambient->card)
            ? subgroup->ambient->elements[idx]
            : NULL;
        printf("%s", (element && element->repr) ? element->repr : "?");
    }
    if (subgroup->card > limit) printf(", ...");
    printf("]>");
}

static void printInlineCoset(GroupCoset* coset) {
    if (!coset) {
        printf("<coset null>");
        return;
    }
    printf("<%sCoset of ", coset->isLeft ? "left" : "right");
    printInlineSubgroup(coset->subgroup);
    printf("; elements=[");
    int limit = (coset->subgroup && coset->subgroup->card < 6) ? coset->subgroup->card : 6;
    for (int i = 0; i < limit; i++) {
        if (i) printf(", ");
        int idx = coset->indices ? coset->indices[i] : -1;
        GroupElement* element = (coset->group && idx >= 0 && idx < coset->group->card)
            ? coset->group->elements[idx]
            : NULL;
        printf("%s", (element && element->repr) ? element->repr : "?");
    }
    if (coset->subgroup && coset->subgroup->card > limit) printf(", ...");
    printf("]>");
}

static void printInlineSubring(SubRing* subring) {
    if (!subring) {
        printf("<subring null>");
        return;
    }
    printf("<subring of ");
    printInlineRing(subring->ambient);
    printf("; card=%d; elements=[", subring->card);
    int limit = subring->card < 6 ? subring->card : 6;
    for (int i = 0; i < limit; i++) {
        if (i) printf(", ");
        int idx = subring->indices ? subring->indices[i] : -1;
        RingElement* element = (subring->ambient && idx >= 0 && idx < subring->ambient->card)
            ? subring->ambient->elements[idx]
            : NULL;
        printf("%s", (element && element->repr) ? element->repr : "?");
    }
    if (subring->card > limit) printf(", ...");
    printf("]>");
}

static void printInlineIdeal(Ideal* ideal) {
    if (!ideal) {
        printf("<ideal null>");
        return;
    }
    printf("<%sIdeal of ", ideal->isLeft ? "left" : "right");
    printInlineRing(ideal->ring);
    printf("; card=%d; elements=[", ideal->card);
    int limit = ideal->card < 6 ? ideal->card : 6;
    for (int i = 0; i < limit; i++) {
        if (i) printf(", ");
        int idx = ideal->indices ? ideal->indices[i] : -1;
        RingElement* element = (ideal->ring && idx >= 0 && idx < ideal->ring->card)
            ? ideal->ring->elements[idx]
            : NULL;
        printf("%s", (element && element->repr) ? element->repr : "?");
    }
    if (ideal->card > limit) printf(", ...");
    printf("]>");
}

static void printInlineGroupHomomorphism(GroupHomomorphism* homo) {
    if (!homo) {
        printf("<groupHomomorphism null>");
        return;
    }
    printf("<groupHomomorphism domain=");
    printInlineGroup(homo->domain);
    printf("; codomain=");
    printInlineGroup(homo->codomain);
    printf(">");
}

static void printInlineRingHomomorphism(RingHomomorphism* homo) {
    if (!homo) {
        printf("<ringHomomorphism null>");
        return;
    }
    printf("<ringHomomorphism domain=");
    printInlineRing(homo->domain);
    printf("; codomain=");
    printInlineRing(homo->codomain);
    printf(">");
}

/* ---------- Construct methods ---------- */

Value valNone(void)               { Value v = {0}; v.kind = VAL_NONE;     return v; }
Value valError(const char* msg)   { Value v = {0}; v.kind = VAL_ERROR;    v.as.str = dupstr(msg); return v; }
Value valBool(bool b)             { Value v = {0}; v.kind = VAL_BOOL;     v.as.b = b;             return v; }
Value valInt(long long n)         { Value v = {0}; v.kind = VAL_INT;      v.as.i = n;             return v; }
Value valDecimal(double x)        { Value v = {0}; v.kind = VAL_DECIMAL;  v.as.d = x;             return v; }
Value valFraction(Fraction f)     { Value v = {0}; v.kind = VAL_FRACTION; v.as.frac = f;          return v; }
Value valComplex(ComplexNumber c) { Value v = {0}; v.kind = VAL_COMPLEX;  v.as.cplx = c;          return v; }
Value valString(const char* s)    { Value v = {0}; v.kind = VAL_STRING;   v.as.str = dupstr(s);   return v; }
Value valSymbol(const char* s)    { Value v = {0}; v.kind = VAL_SYMBOL;   v.as.str = dupstr(s);   return v; }

Value valList(Value* items, size_t n) {
    Value v = {0};
    v.kind = VAL_LIST;
    v.as.list.items = items;
    v.as.list.n     = n;
    return v;
}

Value valPtr(ValueKind kind, void* p) {
    Value v = {0};
    v.kind = kind;
    v.as.ptr = p;
    return v;
}

/* ---------- Free and clone ---------- */

// Free a Value's own payload (strings, list arrays). See note in value.h
void valFree(Value v) {
    switch (v.kind) {
        case VAL_MATRIX:
        case VAL_VECTOR:
            freeMatrix((Matrix*)v.as.ptr);
            break;
        case VAL_COMBSET:
            if (v.as.ptr) freeCombset((CombSet*)v.as.ptr);
            break;
        case VAL_STRING:
        case VAL_SYMBOL:
        case VAL_ERROR:
            free(v.as.str);
            break;
        case VAL_LIST:
            for (size_t i = 0; i < v.as.list.n; i++) valFree(v.as.list.items[i]);
            free(v.as.list.items);
            break;
        default:
            // scalar or opaque pointer: nothing owned by the Value itself
            break;
    }
}

// Produce an independently-freeable copy of v
Value valClone(Value v) {
    switch (v.kind) {
        case VAL_MATRIX:
        case VAL_VECTOR: {
            Matrix* copy = copyMatrix((Matrix*)v.as.ptr);
            return valPtr(v.kind, copy);
        }
        case VAL_COMBSET:
            return valPtr(VAL_COMBSET,
                          v.as.ptr ? copyCombset((CombSet*)v.as.ptr) : NULL);
        case VAL_STRING: return valString(v.as.str);
        case VAL_SYMBOL: return valSymbol(v.as.str);
        case VAL_ERROR:  return valError(v.as.str);
        case VAL_LIST: {
            Value* items = calloc(v.as.list.n, sizeof(Value));
            for (size_t i = 0; i < v.as.list.n; i++) items[i] = valClone(v.as.list.items[i]);
            return valList(items, v.as.list.n);
        }
        default:
            return v;
    }
}

/* ---------- Introspection ---------- */

// Human-readable name for a ValueKind
const char* valKindName(ValueKind k) {
    switch (k) {
        case VAL_NONE:          return "none";
        case VAL_ERROR:         return "error";
        case VAL_BOOL:          return "bool";
        case VAL_INT:           return "int";
        case VAL_DECIMAL:       return "decimal";
        case VAL_FRACTION:      return "fraction";
        case VAL_COMPLEX:       return "complex";
        case VAL_STRING:        return "string";
        case VAL_SYMBOL:        return "symbol";
        case VAL_LIST:          return "list";
        case VAL_MATRIX:        return "matrix";
        case VAL_VECTOR:        return "vector";
        case VAL_COMBSET:       return "combset";
        case VAL_GROUP:         return "group";
        case VAL_GROUP_ELEMENT: return "group_element";
        case VAL_SUBGROUP:      return "subgroup";
        case VAL_GROUP_COSET:   return "group_coset";
        case VAL_GROUP_HOMOMORPHISM: return "group_homomorphism";
        case VAL_RING:          return "ring";
        case VAL_RING_ELEMENT:  return "ring_element";
        case VAL_SUBRING:       return "subring";
        case VAL_IDEAL:         return "ideal";
        case VAL_RING_HOMOMORPHISM: return "ring_homomorphism";
        case VAL_BODY:          return "body";
        case VAL_FORCE:         return "force";
    }
    return "?";
}

// Print a Value in a human-readable form
void valPrint(Value v) {
    switch (v.kind) {
        case VAL_NONE:     printf("()"); break;
        case VAL_ERROR:    printf("<error: %s>", v.as.str ? v.as.str : ""); break;
        case VAL_BOOL:     printf(v.as.b ? "true" : "false"); break;
        case VAL_INT:      printf("%lld", v.as.i); break;
        case VAL_DECIMAL:  printf("%g", v.as.d); break;
        case VAL_FRACTION: printFraction(v.as.frac); break;
        case VAL_COMPLEX:  printf("(%g + %gi)", v.as.cplx.real, v.as.cplx.imag); break;
        case VAL_STRING:   printf("\"%s\"", v.as.str ? v.as.str : ""); break;
        case VAL_SYMBOL:   printf("%s", v.as.str ? v.as.str : ""); break;
        case VAL_LIST:
            putchar('(');
            for (size_t i = 0; i < v.as.list.n; i++) {
                if (i) printf(", ");
                valPrint(v.as.list.items[i]);
            }
            putchar(')');
            break;
        case VAL_MATRIX:
        case VAL_VECTOR:
            putchar('\n');
            printMatrix((Matrix*)v.as.ptr);
            break;
        case VAL_COMBSET: {
            CombSet* combset = (CombSet*)v.as.ptr;
            putchar('{');
            if (combset) {
                for (int i = 0; i < combset->card; i++) {
                    if (i) printf(", ");
                    printf("%lld", combset->set[i]);
                }
            }
            putchar('}');
            break;
        }
        case VAL_BODY: {
            Body* body = (Body*)v.as.ptr;
            if (!body) { printf("<body null>"); break; }
            printf("<body mass=%g; pos=", body->mass);
            printInlineVector(body->pos);
            printf("; velocity=");
            printInlineVector(body->velocity);
            printf("; forces=%d>", body->nForces);
            break;
        }
        case VAL_FORCE: {
            Force* force = (Force*)v.as.ptr;
            if (!force) { printf("<force null>"); break; }
            printf("<force \"%s\": vector=", force->name ? force->name : "");
            printInlineVector(force->vector);
            printf("; tailPos=");
            printInlineVector(force->tailPos);
            printf("; magnitude=%g>", force->vector ? l2Norm(force->vector) : 0.0);
            break;
        }
        case VAL_GROUP:
            printInlineGroup((Group*)v.as.ptr);
            break;
        case VAL_GROUP_ELEMENT: {
            GroupElement* element = (GroupElement*)v.as.ptr;
            printf("%s", (element && element->repr) ? element->repr : "<group_element null>");
            break;
        }
        case VAL_SUBGROUP: {
            printInlineSubgroup((SubGroup*)v.as.ptr);
            break;
        }
        case VAL_GROUP_COSET: {
            printInlineCoset((GroupCoset*)v.as.ptr);
            break;
        }
        case VAL_GROUP_HOMOMORPHISM: {
            printInlineGroupHomomorphism((GroupHomomorphism*)v.as.ptr);
            break;
        }
        case VAL_RING:
            printInlineRing((Ring*)v.as.ptr);
            break;
        case VAL_RING_ELEMENT: {
            RingElement* element = (RingElement*)v.as.ptr;
            printf("%s", (element && element->repr) ? element->repr : "<ring_element null>");
            break;
        }
        case VAL_SUBRING: {
            printInlineSubring((SubRing*)v.as.ptr);
            break;
        }
        case VAL_IDEAL: {
            printInlineIdeal((Ideal*)v.as.ptr);
            break;
        }
        case VAL_RING_HOMOMORPHISM: {
            printInlineRingHomomorphism((RingHomomorphism*)v.as.ptr);
            break;
        }
        default:
            printf("<%s %p>", valKindName(v.kind), v.as.ptr);
            break;
    }
}

/* ---------- Numeric promotion helpers ---------- */

// True iff v is VAL_INT / VAL_DECIMAL / VAL_FRACTION
bool valIsNumeric(Value v) {
    return v.kind == VAL_INT || v.kind == VAL_DECIMAL || v.kind == VAL_FRACTION;
}

// Collapse a numeric value to a double (0.0 on non-numeric)
double valToDouble(Value v) {
    switch (v.kind) {
        case VAL_INT:      return (double)v.as.i;
        case VAL_DECIMAL:  return v.as.d;
        case VAL_FRACTION:
            return v.as.frac.denom ? (double)v.as.frac.num / (double)v.as.frac.denom : 0.0;
        case VAL_BOOL:     return v.as.b ? 1.0 : 0.0;
        default:           return 0.0;
    }
}
