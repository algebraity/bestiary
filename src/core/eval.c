#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<math.h>
#include<limits.h>
#include "eval.h"
#include "hebi.h"
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

/* ---------- Environment ---------- */

// Construct an Env whose scope chains upward through `parent`
Env* envNew(Env* parent) {
    Env* e = calloc(1, sizeof(Env));
    e->parent = parent;
    return e;
}

// Free an Env and every binding it holds (values included)
void envFree(Env* env) {
    if (!env) return;
    EnvEntry* cur = env->head;
    while (cur) {
        EnvEntry* next = cur->next;
        valFree(cur->value);
        free(cur->name);
        free(cur);
        cur = next;
    }
    free(env);
}

// Look up `name` anywhere up the parent chain; returns 1 on hit
int envGet(Env* env, const char* name, Value* out) {
    for (Env* e = env; e; e = e->parent) {
        for (EnvEntry* x = e->head; x; x = x->next) {
            if (strcmp(x->name, name) == 0) {
                if (out) *out = x->value;
                return 1;
            }
        }
    }
    return 0;
}

// Bind `name` to `v`, replacing any existing binding in `env`
void envSet(Env* env, const char* name, Value v) {
    for (EnvEntry* x = env->head; x; x = x->next) {
        if (strcmp(x->name, name) == 0) {
            valFree(x->value);
            x->value = v;
            return;
        }
    }
    EnvEntry* e = calloc(1, sizeof(EnvEntry));
    e->name  = dupstr(name);
    e->value = v;
    e->next  = env->head;
    env->head = e;
}

/* ---------- Command registry ---------- */

static CommandEntry* g_commands = NULL;

// Register (or replace) a command by name
void registerCommand(const char* name, int arity, CommandFn fn) {
    for (CommandEntry* c = g_commands; c; c = c->next) {
        if (strcmp(c->name, name) == 0) {
            c->arity = arity;
            c->fn    = fn;
            return;
        }
    }
    CommandEntry* e = calloc(1, sizeof(CommandEntry));
    e->name  = dupstr(name);            // registry owns the name buffer
    e->arity = arity;
    e->fn    = fn;
    e->next  = g_commands;
    g_commands = e;
}

// Look up a registered command, or NULL if absent
const CommandEntry* lookupCommand(const char* name) {
    for (CommandEntry* c = g_commands; c; c = c->next)
        if (strcmp(c->name, name) == 0) return c;
    return NULL;
}

const CommandEntry* commandRegistry(void) {
    return g_commands;
}

/* ---------- Eval context ---------- */

EvalContext* evalCtxNew(void) {
    EvalContext* ctx = calloc(1, sizeof(EvalContext));
    ctx->env = envNew(NULL);
    return ctx;
}

void evalCtxFree(EvalContext* ctx) {
    if (!ctx) return;
    envFree(ctx->env);
    free(ctx);
}

/* ---------- Dispatcher ---------- */

// Invoke a registered command with an already-evaluated args array.
// Calling convention: callee frees each arg it used; we free the array.
// Unknown name or wrong arity -> VAL_ERROR (and args are freed here).
static Value dispatch(EvalContext* ctx, const char* name, Value* args, size_t nargs) {
    const CommandEntry* e = lookupCommand(name);
    if (!e) {
        for (size_t i = 0; i < nargs; i++) valFree(args[i]);
        free(args);
        char buf[128];
        snprintf(buf, sizeof(buf), "unknown command: \\%s", name);
        return valError(buf);
    }
    if (e->arity >= 0 && (int)nargs != e->arity) {
        for (size_t i = 0; i < nargs; i++) valFree(args[i]);
        free(args);
        char buf[160];
        snprintf(buf, sizeof(buf), "\\%s expects %d args, got %zu", name, e->arity, nargs);
        return valError(buf);
    }
    Value r = e->fn(ctx, args, nargs);
    free(args);
    return r;
}

// Evaluate every node in `nodes` into a fresh Value array
static Value* evalArgs(EvalContext* ctx, AstNode** nodes, size_t n) {
    Value* out = calloc(n, sizeof(Value));
    for (size_t i = 0; i < n; i++) out[i] = eval(ctx, nodes[i]);
    return out;
}

static int valueToMatrixElement(Value v, MatrixElement* out) {
    if (v.kind == VAL_COMPLEX) {
        *out = elemFromComplex(v.as.cplx);
        return 1;
    }
    if (valIsNumeric(v)) {
        *out = elemFromReal(valToDouble(v));
        return 1;
    }
    return 0;
}

static Value matrixOpError(const char* msg, Value lhs, Value rhs) {
    valFree(lhs);
    valFree(rhs);
    return valError(msg);
}

static int valueToScalarElement(Value v, MatrixElement* out) {
    return valueToMatrixElement(v, out);
}

static int intSqrtExact(long long n, long long* root) {
    if (n < 0) return 0;
    long double r = floorl(sqrtl((long double)n));
    long long rr = (long long)r;
    if (rr * rr == n) {
        if (root) *root = rr;
        return 1;
    }
    if ((rr + 1) > 0 && (rr + 1) * (rr + 1) == n) {
        if (root) *root = rr + 1;
        return 1;
    }
    return 0;
}

static int valueIsVector(Value v) {
    return v.kind == VAL_VECTOR;
}

static int valueIsBody(Value v) {
    return v.kind == VAL_BODY;
}

static int valueIsForce(Value v) {
    return v.kind == VAL_FORCE;
}

static int valueIsGroup(Value v) {
    return v.kind == VAL_GROUP;
}

static int valueIsRing(Value v) {
    return v.kind == VAL_RING;
}

static int valueIsGroupElement(Value v) {
    return v.kind == VAL_GROUP_ELEMENT;
}

static int valueIsRingElement(Value v) {
    return v.kind == VAL_RING_ELEMENT;
}

static int valueIsSubgroup(Value v) {
    return v.kind == VAL_SUBGROUP;
}

static int valueIsGroupCoset(Value v) {
    return v.kind == VAL_GROUP_COSET;
}

static int valueIsGroupHomomorphism(Value v) {
    return v.kind == VAL_GROUP_HOMOMORPHISM;
}

static int valueIsSubring(Value v) {
    return v.kind == VAL_SUBRING;
}

static int valueIsIdeal(Value v) {
    return v.kind == VAL_IDEAL;
}

static int valueIsRingHomomorphism(Value v) {
    return v.kind == VAL_RING_HOMOMORPHISM;
}

static int valueIsCombSet(Value v) {
    return v.kind == VAL_COMBSET;
}

static int valueIsEmptyCombSet(Value v) {
    return valueIsCombSet(v) && v.as.ptr == NULL;
}

static int combsetCard(Value v) {
    CombSet* combset = (CombSet*)v.as.ptr;
    return combset ? combset->card : 0;
}

static int valueToCombSetInt(Value v, long long* out) {
    if (v.kind != VAL_INT) return 0;
    if (out) *out = v.as.i;
    return 1;
}

static int valueToBodyMass(Value v, double* out) {
    if (!valIsNumeric(v)) return 0;
    if (out) *out = valToDouble(v);
    return 1;
}

static int valueToRealScalar(Value v, double* out) {
    if (!valIsNumeric(v)) return 0;
    if (out) *out = valToDouble(v);
    return 1;
}

static int doubleIsInt(double x) {
    double whole;
    return isfinite(x) && modf(x, &whole) == 0.0 && whole >= (double)INT_MIN && whole <= (double)INT_MAX;
}

static int valueToUsagiInt(Value v, int* out) {
    if (!valIsNumeric(v)) return 0;
    double x = valToDouble(v);
    if (!doubleIsInt(x)) return 0;
    if (out) *out = (int)x;
    return 1;
}

static int valueToUsagiIntVector(Value v, int** outVals, int* outCount) {
    if (!outVals || !outCount || !valueIsVector(v)) return 0;

    Vector* vector = (Vector*)v.as.ptr;
    int count = vector ? vector->numRows : 0;
    if (count < 1) return 0;

    int* vals = calloc((size_t)count, sizeof(int));
    if (!vals) return 0;

    for (int i = 0; i < count; i++) {
        MatrixElement elem = getEntry((Matrix*)vector, i, 0);
        if (elem.isComplex || !doubleIsInt(elem.value.real)) {
            free(vals);
            return 0;
        }
        vals[i] = (int)elem.value.real;
    }

    *outVals = vals;
    *outCount = count;
    return 1;
}

static int valueToStringLiteral(Value v, const char** out) {
    if (v.kind != VAL_STRING || !v.as.str) return 0;
    if (out) *out = v.as.str;
    return 1;
}

static int compareIntsAsc(const void* lhs, const void* rhs) {
    int a = *(const int*)lhs;
    int b = *(const int*)rhs;
    return (a > b) - (a < b);
}

static int normalizeIndexList(int* indices, int count) {
    if (!indices || count <= 0) return count;
    qsort(indices, (size_t)count, sizeof(int), compareIntsAsc);
    int out = 1;
    for (int i = 1; i < count; i++) {
        if (indices[i] != indices[out - 1]) indices[out++] = indices[i];
    }
    return out;
}

static int collectGroupElementIndices(Value value, Group* requiredGroup, int** outIndices, int* outCount) {
    if (!outIndices || !outCount) return 0;

    if (value.kind == VAL_LIST) {
        int count = (int)value.as.list.n;
        if (count < 1) return 0;
        int* indices = calloc((size_t)count, sizeof(int));
        if (!indices) return 0;
        Group* baseGroup = requiredGroup;
        for (int i = 0; i < count; i++) {
            Value item = value.as.list.items[i];
            if (!valueIsGroupElement(item)) {
                free(indices);
                return 0;
            }
            GroupElement* element = (GroupElement*)item.as.ptr;
            if (!element || !element->group) {
                free(indices);
                return 0;
            }
            if (!baseGroup) baseGroup = element->group;
            if (!cmpGroups(baseGroup, element->group)) {
                free(indices);
                return 0;
            }
            indices[i] = element->index;
        }
        count = normalizeIndexList(indices, count);
        *outIndices = indices;
        *outCount = count;
        return 1;
    }

    if (!valueIsGroupElement(value)) return 0;
    GroupElement* element = (GroupElement*)value.as.ptr;
    if (!element || !element->group || (requiredGroup && !cmpGroups(requiredGroup, element->group))) return 0;
    int* indices = malloc(sizeof(int));
    if (!indices) return 0;
    indices[0] = element->index;
    *outIndices = indices;
    *outCount = 1;
    return 1;
}

static int collectRingElementIndices(Value value, Ring* requiredRing, int** outIndices, int* outCount) {
    if (!outIndices || !outCount) return 0;

    if (value.kind == VAL_LIST) {
        int count = (int)value.as.list.n;
        if (count < 1) return 0;
        int* indices = calloc((size_t)count, sizeof(int));
        if (!indices) return 0;
        Ring* baseRing = requiredRing;
        for (int i = 0; i < count; i++) {
            Value item = value.as.list.items[i];
            if (!valueIsRingElement(item)) {
                free(indices);
                return 0;
            }
            RingElement* element = (RingElement*)item.as.ptr;
            if (!element || !element->ring) {
                free(indices);
                return 0;
            }
            if (!baseRing) baseRing = element->ring;
            if (!cmpRings(baseRing, element->ring)) {
                free(indices);
                return 0;
            }
            indices[i] = element->index;
        }
        count = normalizeIndexList(indices, count);
        *outIndices = indices;
        *outCount = count;
        return 1;
    }

    if (!valueIsRingElement(value)) return 0;
    RingElement* element = (RingElement*)value.as.ptr;
    if (!element || !element->ring || (requiredRing && !cmpRings(requiredRing, element->ring))) return 0;
    int* indices = malloc(sizeof(int));
    if (!indices) return 0;
    indices[0] = element->index;
    *outIndices = indices;
    *outCount = 1;
    return 1;
}

static int collectGroupHomomorphismMapping(Value value, Group* domain, Group* codomain, int** outMapping) {
    if (!domain || !codomain || !outMapping) return 0;
    if (value.kind != VAL_LIST || value.as.list.n != (size_t)domain->card) return 0;

    int* mapping = malloc((size_t)domain->card * sizeof(int));
    if (!mapping) return 0;
    for (int i = 0; i < domain->card; i++) mapping[i] = -1;

    for (size_t i = 0; i < value.as.list.n; i++) {
        Value entry = value.as.list.items[i];
        if (entry.kind != VAL_LIST || entry.as.list.n != 2) {
            free(mapping);
            return 0;
        }
        Value lhs = entry.as.list.items[0];
        Value rhs = entry.as.list.items[1];
        if (!valueIsGroupElement(lhs) || !valueIsGroupElement(rhs)) {
            free(mapping);
            return 0;
        }
        GroupElement* src = (GroupElement*)lhs.as.ptr;
        GroupElement* dst = (GroupElement*)rhs.as.ptr;
        if (!src || !dst || !cmpGroups(src->group, domain) || !cmpGroups(dst->group, codomain)) {
            free(mapping);
            return 0;
        }
        if (mapping[src->index] >= 0) {
            free(mapping);
            return 0;
        }
        mapping[src->index] = dst->index;
    }

    for (int i = 0; i < domain->card; i++) {
        if (mapping[i] < 0) {
            free(mapping);
            return 0;
        }
    }
    *outMapping = mapping;
    return 1;
}

static int collectRingHomomorphismMapping(Value value, Ring* domain, Ring* codomain, int** outMapping) {
    if (!domain || !codomain || !outMapping) return 0;
    if (value.kind != VAL_LIST || value.as.list.n != (size_t)domain->card) return 0;

    int* mapping = malloc((size_t)domain->card * sizeof(int));
    if (!mapping) return 0;
    for (int i = 0; i < domain->card; i++) mapping[i] = -1;

    for (size_t i = 0; i < value.as.list.n; i++) {
        Value entry = value.as.list.items[i];
        if (entry.kind != VAL_LIST || entry.as.list.n != 2) {
            free(mapping);
            return 0;
        }
        Value lhs = entry.as.list.items[0];
        Value rhs = entry.as.list.items[1];
        if (!valueIsRingElement(lhs) || !valueIsRingElement(rhs)) {
            free(mapping);
            return 0;
        }
        RingElement* src = (RingElement*)lhs.as.ptr;
        RingElement* dst = (RingElement*)rhs.as.ptr;
        if (!src || !dst || !cmpRings(src->ring, domain) || !cmpRings(dst->ring, codomain)) {
            free(mapping);
            return 0;
        }
        if (mapping[src->index] >= 0) {
            free(mapping);
            return 0;
        }
        mapping[src->index] = dst->index;
    }

    for (int i = 0; i < domain->card; i++) {
        if (mapping[i] < 0) {
            free(mapping);
            return 0;
        }
    }
    *outMapping = mapping;
    return 1;
}

static void appendInlineGroupSummary(char* buf, size_t bufSize, Group* group) {
    if (!buf || bufSize == 0) return;
    size_t used = strlen(buf);
    if (!group) {
        snprintf(buf + used, bufSize - used, "<group null>");
        return;
    }
    used += snprintf(buf + used, bufSize - used, "<group card=%d; elements=[", group->card);
    int limit = group->card < 6 ? group->card : 6;
    for (int i = 0; i < limit && used < bufSize; i++) {
        used += snprintf(buf + used, bufSize - used, "%s%s",
                         i ? ", " : "",
                         (group->elements && group->elements[i] && group->elements[i]->repr)
                            ? group->elements[i]->repr : "?");
    }
    if (group->card > limit && used < bufSize) used += snprintf(buf + used, bufSize - used, ", ...");
    if (used < bufSize) snprintf(buf + used, bufSize - used, "]>");
}

static void appendInlineRingSummary(char* buf, size_t bufSize, Ring* ring) {
    if (!buf || bufSize == 0) return;
    size_t used = strlen(buf);
    if (!ring) {
        snprintf(buf + used, bufSize - used, "<ring null>");
        return;
    }
    used += snprintf(buf + used, bufSize - used, "<ring card=%d; elements=[", ring->card);
    int limit = ring->card < 6 ? ring->card : 6;
    for (int i = 0; i < limit && used < bufSize; i++) {
        used += snprintf(buf + used, bufSize - used, "%s%s",
                         i ? ", " : "",
                         (ring->elements && ring->elements[i] && ring->elements[i]->repr)
                            ? ring->elements[i]->repr : "?");
    }
    if (ring->card > limit && used < bufSize) used += snprintf(buf + used, bufSize - used, ", ...");
    if (used < bufSize) snprintf(buf + used, bufSize - used, "]>");
}

static void appendOptionalOrder(char* buf, size_t bufSize, int order) {
    size_t used = strlen(buf);
    if (used >= bufSize) return;
    snprintf(buf + used, bufSize - used, "%s", order < 0 ? "none" : "");
    if (order >= 0) {
        used = strlen(buf);
        if (used < bufSize) snprintf(buf + used, bufSize - used, "%d", order);
    }
}

static void appendInlineSubgroupElements(char* buf, size_t bufSize, SubGroup* subgroup) {
    size_t used = strlen(buf);
    if (used >= bufSize) return;
    if (!subgroup || !subgroup->ambient) {
        snprintf(buf + used, bufSize - used, "[]");
        return;
    }
    used += snprintf(buf + used, bufSize - used, "[");
    int limit = subgroup->card < 8 ? subgroup->card : 8;
    for (int i = 0; i < limit && used < bufSize; i++) {
        GroupElement* element = subgroup->ambient->elements[subgroup->indices[i]];
        used += snprintf(buf + used, bufSize - used, "%s%s",
                         i ? ", " : "",
                         (element && element->repr) ? element->repr : "?");
    }
    if (subgroup->card > limit && used < bufSize) used += snprintf(buf + used, bufSize - used, ", ...");
    if (used < bufSize) snprintf(buf + used, bufSize - used, "]");
}

static void appendInlineSubgroupSummary(char* buf, size_t bufSize, SubGroup* subgroup) {
    size_t used = strlen(buf);
    if (used >= bufSize) return;
    if (!subgroup) {
        snprintf(buf + used, bufSize - used, "<subgroup null>");
        return;
    }
    used += snprintf(buf + used, bufSize - used, "<subgroup card=%d; elements=", subgroup->card);
    appendInlineSubgroupElements(buf, bufSize, subgroup);
    used = strlen(buf);
    if (used < bufSize) snprintf(buf + used, bufSize - used, ">");
}

static Value groupCosetArrayToList(GroupCoset** cosets, int count) {
    Value* items = calloc((size_t)count, sizeof(Value));
    if (!items) {
        if (cosets) {
            for (int i = 0; i < count; i++) freeGroupCoset(cosets[i]);
            free(cosets);
        }
        return valError("failed to allocate coset list output");
    }
    for (int i = 0; i < count; i++) items[i] = valPtr(VAL_GROUP_COSET, cosets[i]);
    free(cosets);
    return valList(items, (size_t)count);
}

static Value vectorArrayToList(Vector** vectors, int count) {
    if (count <= 0) {
        free(vectors);
        return valList(NULL, 0);
    }

    Value* items = calloc((size_t)count, sizeof(Value));
    if (!items) {
        if (vectors) {
            for (int i = 0; i < count; i++) freeVector(vectors[i]);
            free(vectors);
        }
        return valError("failed to allocate vector list output");
    }

    for (int i = 0; i < count; i++) items[i] = valPtr(VAL_VECTOR, vectors[i]);
    free(vectors);
    return valList(items, (size_t)count);
}

static void freeSubgroupArray(SubGroup** subgroups, int count) {
    if (!subgroups) return;
    for (int i = 0; i < count; i++) freeSubgroup(subgroups[i]);
    free(subgroups);
}

static void appendGroupElementIndexList(char* buf, size_t bufSize, Group* group, int* indices, int count) {
    size_t used = strlen(buf);
    if (used >= bufSize) return;
    used += snprintf(buf + used, bufSize - used, "[");
    int limit = count < 8 ? count : 8;
    for (int i = 0; i < limit && used < bufSize; i++) {
        GroupElement* element = (group && indices && indices[i] >= 0 && indices[i] < group->card)
            ? group->elements[indices[i]]
            : NULL;
        used += snprintf(buf + used, bufSize - used, "%s%s",
                         i ? ", " : "",
                         (element && element->repr) ? element->repr : "?");
    }
    if (count > limit && used < bufSize) used += snprintf(buf + used, bufSize - used, ", ...");
    if (used < bufSize) snprintf(buf + used, bufSize - used, "]");
}

static Value subgroupListSymbol(const char* label, Group* group, SubGroup** subgroups, int count) {
    char* buf = calloc(8192, 1);
    if (!buf) return valError("failed to allocate subgroup list output");
    size_t used = 0;
    if (label && *label) used += snprintf(buf + used, 8192 - used, "%s of ", label);
    appendInlineGroupSummary(buf, 8192, group);
    used = strlen(buf);
    if (used < 8192) snprintf(buf + used, 8192 - used, ": ");
    for (int i = 0; i < count; i++) {
        used = strlen(buf);
        if (used < 8192) snprintf(buf + used, 8192 - used, "%sH%d(card=%d, elements=", i ? "; " : "", i + 1, subgroups[i]->card);
        appendInlineSubgroupElements(buf, 8192, subgroups[i]);
        used = strlen(buf);
        if (used < 8192) snprintf(buf + used, 8192 - used, ")");
    }
    Value out = valSymbol(buf);
    free(buf);
    return out;
}

static int combsetContains(CombSet* combset, long long x) {
    if (!combset) return 0;
    for (int i = 0; i < combset->card; i++) {
        if (combset->set[i] == x) return 1;
    }
    return 0;
}

static Vector* vectorFromRealArray(double* data, int dim) {
    if (!data || dim < 1) return NULL;
    Vector* vector = constructVector(dim);
    if (!vector) return NULL;
    for (int i = 0; i < dim; i++) {
        setEntry(vector, i, 0, elemFromReal(data[i]));
    }
    return vector;
}

static int unpackBodyList(Value* args, size_t nargs, Body*** outBodies, size_t* outCount) {
    if (!outBodies || !outCount) return 0;

    if (nargs == 1 && args[0].kind == VAL_LIST) {
        size_t count = args[0].as.list.n;
        Body** bodies = calloc(count, sizeof(Body*));
        if (!bodies) return 0;
        for (size_t i = 0; i < count; i++) {
            if (!valueIsBody(args[0].as.list.items[i])) {
                free(bodies);
                return 0;
            }
            bodies[i] = (Body*)args[0].as.list.items[i].as.ptr;
        }
        *outBodies = bodies;
        *outCount = count;
        return 1;
    }

    Body** bodies = calloc(nargs, sizeof(Body*));
    if (!bodies) return 0;
    for (size_t i = 0; i < nargs; i++) {
        if (!valueIsBody(args[i])) {
            free(bodies);
            return 0;
        }
        bodies[i] = (Body*)args[i].as.ptr;
    }
    *outBodies = bodies;
    *outCount = nargs;
    return 1;
}

static int unpackBodyPair(Value* args, size_t nargs, Value* lhs, Value* rhs) {
    if (nargs == 2) {
        *lhs = args[0];
        *rhs = args[1];
        return 1;
    }
    if (nargs == 1 && args[0].kind == VAL_LIST && args[0].as.list.n == 2) {
        *lhs = valClone(args[0].as.list.items[0]);
        *rhs = valClone(args[0].as.list.items[1]);
        valFree(args[0]);
        return 1;
    }
    return 0;
}

static ComplexNumber valueToComplex(Value v) {
    switch (v.kind) {
        case VAL_COMPLEX:
            return v.as.cplx;
        case VAL_INT:
            return (ComplexNumber){ .real = (double)v.as.i, .imag = 0.0 };
        case VAL_DECIMAL:
            return (ComplexNumber){ .real = v.as.d, .imag = 0.0 };
        case VAL_FRACTION:
            return (ComplexNumber){ .real = valToDouble(v), .imag = 0.0 };
        default:
            return (ComplexNumber){ .real = NAN, .imag = NAN };
    }
}

static int valueIsComplexNumeric(Value v) {
    return v.kind == VAL_COMPLEX || valIsNumeric(v);
}

static Value valueFromMatrixElement(MatrixElement elem) {
    if (elemIsNan(elem)) return valError("matrix operation returned NaN");
    if (elem.isComplex) {
        ComplexNumber c = elem.value.complex;
        if (c.imag == 0.0) return valDecimal(c.real);
        return valComplex(c);
    }
    return valDecimal(elem.value.real);
}

static Value matrixUnaryError(const char* msg, Value arg) {
    valFree(arg);
    return valError(msg);
}

static Value vectorBinaryError(const char* msg, Value lhs, Value rhs) {
    valFree(lhs);
    valFree(rhs);
    return valError(msg);
}

static Value vectorUnaryError(const char* msg, Value arg) {
    valFree(arg);
    return valError(msg);
}

static Value combsetBinaryError(const char* msg, Value lhs, Value rhs) {
    valFree(lhs);
    valFree(rhs);
    return valError(msg);
}

static Value combsetUnaryError(const char* msg, Value arg) {
    valFree(arg);
    return valError(msg);
}

static Value wrapCombSetResult(CombSet* combset, const char* emptyMsg, Value lhs, Value rhs) {
    valFree(lhs);
    valFree(rhs);
    if (!combset && emptyMsg) return valError(emptyMsg);
    return valPtr(VAL_COMBSET, combset);
}

static int unpackVectorPair(Value* args, size_t nargs, Value* lhs, Value* rhs) {
    if (nargs == 2) {
        *lhs = args[0];
        *rhs = args[1];
        return 1;
    }
    if (nargs == 1 && args[0].kind == VAL_LIST && args[0].as.list.n == 2) {
        *lhs = valClone(args[0].as.list.items[0]);
        *rhs = valClone(args[0].as.list.items[1]);
        valFree(args[0]);
        return 1;
    }
    return 0;
}

/* ---------- AST walker ---------- */

// Walk an AST subtree and produce a Value
Value eval(EvalContext* ctx, AstNode* node) {
    if (!node) return valNone();

    switch (node->kind) {
        case AST_NUMBER:  return valInt(node->as.number);
        case AST_DECIMAL: return valDecimal(node->as.decimal);
        case AST_STRING:  return valString(node->as.ident);

        case AST_IDENT: {
            Value v;
            // envGet returns a borrowed view; clone so the caller can
            // freely valFree without disturbing env.
            if (envGet(ctx->env, node->as.ident, &v)) return valClone(v);
            if (strcmp(node->as.ident, "i") == 0) {
                return valComplex((ComplexNumber){ .real = 0.0, .imag = 1.0 });
            }
            // Unbound -> carry as a symbol; commands may consume it.
            return valSymbol(node->as.ident);
        }

        case AST_BINOP: {
            Value* args = calloc(2, sizeof(Value));
            args[0] = eval(ctx, node->as.binop.lhs);
            args[1] = eval(ctx, node->as.binop.rhs);
            const char* name = NULL;
            switch (node->as.binop.op) {
                case OP_ADD: name = "+"; break;
                case OP_SUB: name = "-"; break;
                case OP_MUL: name = "*"; break;
                case OP_DIV: name = "/"; break;
                case OP_CMD: name = node->as.binop.opname; break;
                default:     name = "?"; break;
            }
            return dispatch(ctx, name, args, 2);
        }

        case AST_UNARY: {
            Value* args = calloc(1, sizeof(Value));
            args[0] = eval(ctx, node->as.unary.rand);
            const char* name = node->as.unary.op == OP_NEG ? "u-" : "u+";
            return dispatch(ctx, name, args, 1);
        }

        case AST_POWER: {
            Value* args = calloc(2, sizeof(Value));
            args[0] = eval(ctx, node->as.power.base);
            args[1] = eval(ctx, node->as.power.exp);
            return dispatch(ctx, "^", args, 2);
        }

        case AST_SUBSCRIPT: {
            Value* args = calloc(2, sizeof(Value));
            args[0] = eval(ctx, node->as.subscript.base);
            args[1] = eval(ctx, node->as.subscript.sub);
            return dispatch(ctx, "_", args, 2);
        }

        case AST_CALL: {
            Value* args = evalArgs(ctx, node->as.call.args, node->as.call.nargs);
            Value result = dispatch(ctx, node->as.call.name, args, node->as.call.nargs);
            if (result.kind != VAL_ERROR
                    && node->as.call.nargs >= 1
                    && node->as.call.args[0]
                    && node->as.call.args[0]->kind == AST_IDENT
                    && (strcmp(node->as.call.name, "append") == 0
                        || strcmp(node->as.call.name, "remove") == 0)) {
                envSet(ctx->env, node->as.call.args[0]->as.ident, valClone(result));
            }
            return result;
        }

        case AST_TUPLE: {
            Value* items = calloc(node->as.tuple.n, sizeof(Value));
            for (size_t i = 0; i < node->as.tuple.n; i++)
                items[i] = eval(ctx, node->as.tuple.items[i]);
            return valList(items, node->as.tuple.n);
        }

        case AST_SET: {
            size_t count = node->as.tuple.n;
            if (count == 0) return valPtr(VAL_COMBSET, NULL);

            long long* elems = calloc(count, sizeof(long long));
            if (!elems) return valError("failed to allocate CombSet elements");

            for (size_t i = 0; i < count; i++) {
                Value item = eval(ctx, node->as.tuple.items[i]);
                if (item.kind != VAL_INT) {
                    char buf[160];
                    snprintf(buf, sizeof(buf),
                             "CombSet entries must be integers; item %zu is %s",
                             i + 1, valKindName(item.kind));
                    valFree(item);
                    free(elems);
                    return valError(buf);
                }
                elems[i] = item.as.i;
                valFree(item);
            }

            CombSet* combset = constructCombset(elems, (int)count);
            free(elems);
            if (!combset) return valError("failed to construct CombSet");
            return valPtr(VAL_COMBSET, combset);
        }

        case AST_MATRIX: {
            size_t nrows = node->as.matrix.nrows;
            if (nrows == 0) return valError("empty matrix literal");

            size_t ncols = node->as.matrix.rowlens[0];
            if (ncols == 0) return valError("matrix rows must not be empty");

            for (size_t r = 1; r < nrows; r++) {
                if (node->as.matrix.rowlens[r] != ncols)
                    return valError("matrix rows must all have the same length");
            }

            size_t total = 0;
            for (size_t r = 0; r < nrows; r++) total += node->as.matrix.rowlens[r];

            MatrixElement* elems = calloc(total, sizeof(MatrixElement));
            if (!elems) return valError("failed to allocate matrix entries");

            size_t k = 0;
            for (size_t r = 0; r < nrows; r++) {
                for (size_t c = 0; c < ncols; c++) {
                    Value cell = eval(ctx, node->as.matrix.flat[k]);
                    if (!valueToMatrixElement(cell, &elems[k])) {
                        char buf[160];
                        snprintf(buf, sizeof(buf),
                                 "matrix entry at row %zu, col %zu is %s; expected numeric value",
                                 r + 1, c + 1, valKindName(cell.kind));
                        valFree(cell);
                        free(elems);
                        return valError(buf);
                    }
                    valFree(cell);
                    k++;
                }
            }

            if (nrows == 1) {
                Vector* vector = constructVectorFromArray((int)ncols, elems, (int)ncols);
                free(elems);
                if (!vector) return valError("failed to construct vector");
                return valPtr(VAL_VECTOR, vector);
            }

            Matrix* matrix = constructMatrixFromArray((int)nrows, (int)ncols, elems, (int)total);
            free(elems);
            if (!matrix) return valError("failed to construct matrix");
            return valPtr(VAL_MATRIX, matrix);
        }

        case AST_ASSIGN: {
            // envSet takes ownership of the rhs value; assignment itself
            // evaluates to VAL_NONE so we avoid deep-copying owned
            // payloads (strings, lists, BEAST pointers).
            Value v = eval(ctx, node->as.assign.rhs);
            if (v.kind == VAL_ERROR) return v;
            envSet(ctx->env, node->as.assign.name, v);
            return valNone();
        }

        case AST_SEQ: {
            Value last = valNone();
            for (size_t i = 0; i < node->as.seq.n; i++) {
                if (i > 0) valFree(last);
                last = eval(ctx, node->as.seq.stmts[i]);
            }
            return last;
        }
    }
    return valError("unhandled ast node");
}

/* ---------- Built-in operators (numeric promotion) ----------
 * Promotion strategy for +, -, *, /:
 *   int/fraction op int/fraction -> exact fraction arithmetic where needed
 *   any op decimal -> decimal via valToDouble
 * Register your own override to handle e.g. matrix+matrix, group
 * element products, set unions, etc. Later registrations win.
 */

static Fraction valueToFraction(Value v) {
    if (v.kind == VAL_FRACTION) return v.as.frac;
    return constructFraction(v.as.i, 1);
}

// Shared numeric kernel for the four scalar binops
static Value numericBinop(Value a, Value b, char op) {
    if (valueIsComplexNumeric(a) && valueIsComplexNumeric(b)
            && (a.kind == VAL_COMPLEX || b.kind == VAL_COMPLEX)) {
        ComplexNumber x = valueToComplex(a);
        ComplexNumber y = valueToComplex(b);
        ComplexNumber out;
        switch (op) {
            case '+': out = complexAdd(x, y); break;
            case '-': out = complexSub(x, y); break;
            case '*': out = complexMul(x, y); break;
            case '/':
                if (y.real == 0.0 && y.imag == 0.0) return valError("division by zero");
                out = complexDiv(x, y);
                break;
            default:
                return valError("unsupported complex operator");
        }
        if (out.imag == 0.0) return valDecimal(out.real);
        return valComplex(out);
    }
    if (a.kind == VAL_INT && b.kind == VAL_INT) {
        long long x = a.as.i, y = b.as.i;
        switch (op) {
            case '+': return valInt(x + y);
            case '-': return valInt(x - y);
            case '*': return valInt(x * y);
            case '/':
                if (y == 0) return valError("division by zero");
                if (x % y == 0) return valInt(x / y);
                return valFraction(constructFraction(x, y));
        }
    }
    if ((a.kind == VAL_INT || a.kind == VAL_FRACTION)
            && (b.kind == VAL_INT || b.kind == VAL_FRACTION)) {
        Fraction x = valueToFraction(a);
        Fraction y = valueToFraction(b);
        Fraction out;
        switch (op) {
            case '+': out = addFractions(x, y); break;
            case '-': out = subtractFractions(x, y); break;
            case '*': out = multiplyFractions(x, y); break;
            case '/':
                if (y.num == 0) return valError("division by zero");
                out = divideFractions(x, y);
                break;
            default:
                return valError("unsupported fraction operator");
        }
        if (out.denom == 0) return valError("division by zero");
        if (out.denom == 1) return valInt(out.num);
        return valFraction(out);
    }
    if (valIsNumeric(a) && valIsNumeric(b)) {
        double x = valToDouble(a), y = valToDouble(b);
        switch (op) {
            case '+': return valDecimal(x + y);
            case '-': return valDecimal(x - y);
            case '*': return valDecimal(x * y);
            case '/':
                if (y == 0) return valError("division by zero");
                return valDecimal(x / y);
        }
    }
    char buf[160];
    snprintf(buf, sizeof(buf), "'%c' not defined for (%s, %s)",
             op, valKindName(a.kind), valKindName(b.kind));
    return valError(buf);
}

// a + b
static Value bi_add(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (valueIsIdeal(a[0]) && valueIsIdeal(a[1])) {
        Ideal* sum = addIdeals((Ideal*)a[0].as.ptr, (Ideal*)a[1].as.ptr);
        if (!sum) return vectorBinaryError("ideal '+' requires two ideals of the same ring and side", a[0], a[1]);
        valFree(a[0]);
        valFree(a[1]);
        return valPtr(VAL_IDEAL, sum);
    }
    if (valueIsRingElement(a[0]) && valueIsRingElement(a[1])) {
        RingElement* sum = ringAdd((RingElement*)a[0].as.ptr, (RingElement*)a[1].as.ptr);
        if (!sum) return vectorBinaryError("ring '+' requires two elements from the same ring", a[0], a[1]);
        valFree(a[0]);
        valFree(a[1]);
        return valPtr(VAL_RING_ELEMENT, sum);
    }
    if (valueIsCombSet(a[0]) && a[1].kind == VAL_INT) {
        if (valueIsEmptyCombSet(a[0])) return wrapCombSetResult(NULL, NULL, a[0], a[1]);
        CombSet* out = translateSet((CombSet*)a[0].as.ptr, a[1].as.i);
        return wrapCombSetResult(out, "CombSet '+' translation failed", a[0], a[1]);
    }
    if (valueIsCombSet(a[0]) && valueIsCombSet(a[1])) {
        if (valueIsEmptyCombSet(a[0]) || valueIsEmptyCombSet(a[1])) {
            return wrapCombSetResult(NULL, NULL, a[0], a[1]);
        }
        CombSet* sum = addSets((CombSet*)a[0].as.ptr, (CombSet*)a[1].as.ptr);
        return wrapCombSetResult(sum, "CombSet '+' produced an unsupported empty result", a[0], a[1]);
    }
    if (valueIsVector(a[0]) && valueIsVector(a[1])) {
        Vector* sum = addVectors((Vector*)a[0].as.ptr, (Vector*)a[1].as.ptr);
        if (!sum) return vectorBinaryError("vector '+' requires same dimension vectors", a[0], a[1]);
        valFree(a[0]);
        valFree(a[1]);
        return valPtr(VAL_VECTOR, sum);
    }
    if (a[0].kind == VAL_MATRIX && a[1].kind == VAL_MATRIX) {
        Matrix* sum = addMatrices((Matrix*)a[0].as.ptr, (Matrix*)a[1].as.ptr);
        if (!sum) return matrixOpError("matrix '+' requires same dimensions", a[0], a[1]);
        valFree(a[0]);
        valFree(a[1]);
        return valPtr(VAL_MATRIX, sum);
    }
    Value r = numericBinop(a[0], a[1], '+');
    valFree(a[0]); valFree(a[1]);
    return r;
}

// a - b
static Value bi_sub(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (valueIsRingElement(a[0]) && valueIsRingElement(a[1])) {
        RingElement* rhsInv = ringAddInverse((RingElement*)a[1].as.ptr);
        RingElement* diff = rhsInv ? ringAdd((RingElement*)a[0].as.ptr, rhsInv) : NULL;
        if (!diff) return vectorBinaryError("ring '-' requires two elements from the same ring", a[0], a[1]);
        valFree(a[0]);
        valFree(a[1]);
        return valPtr(VAL_RING_ELEMENT, diff);
    }
    if (valueIsCombSet(a[0]) && valueIsCombSet(a[1])) {
        if (valueIsEmptyCombSet(a[0]) || valueIsEmptyCombSet(a[1])) {
            return wrapCombSetResult(NULL, NULL, a[0], a[1]);
        }
        CombSet* diff = subtractSets((CombSet*)a[0].as.ptr, (CombSet*)a[1].as.ptr);
        return wrapCombSetResult(diff, "CombSet '-' produced an unsupported empty result", a[0], a[1]);
    }
    if (valueIsVector(a[0]) && valueIsVector(a[1])) {
        Vector* diff = subtractVectors((Vector*)a[0].as.ptr, (Vector*)a[1].as.ptr);
        if (!diff) return vectorBinaryError("vector '-' requires same dimension vectors", a[0], a[1]);
        valFree(a[0]);
        valFree(a[1]);
        return valPtr(VAL_VECTOR, diff);
    }
    if (a[0].kind == VAL_MATRIX && a[1].kind == VAL_MATRIX) {
        Matrix* diff = subtractMatrices((Matrix*)a[0].as.ptr, (Matrix*)a[1].as.ptr);
        if (!diff) return matrixOpError("matrix '-' requires same dimensions", a[0], a[1]);
        valFree(a[0]);
        valFree(a[1]);
        return valPtr(VAL_MATRIX, diff);
    }
    Value r = numericBinop(a[0], a[1], '-');
    valFree(a[0]); valFree(a[1]);
    return r;
}

// a * b
static Value bi_mul(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (valueIsIdeal(a[0]) && valueIsIdeal(a[1])) {
        Ideal* prod = multIdeals((Ideal*)a[0].as.ptr, (Ideal*)a[1].as.ptr);
        if (!prod) return vectorBinaryError("ideal '*' requires two ideals of the same ring and side", a[0], a[1]);
        valFree(a[0]);
        valFree(a[1]);
        return valPtr(VAL_IDEAL, prod);
    }
    if (valueIsGroupElement(a[0]) && valueIsGroupElement(a[1])) {
        GroupElement* prod = groupMult((GroupElement*)a[0].as.ptr, (GroupElement*)a[1].as.ptr);
        if (!prod) return vectorBinaryError("group '*' requires two elements from the same group", a[0], a[1]);
        valFree(a[0]);
        valFree(a[1]);
        return valPtr(VAL_GROUP_ELEMENT, prod);
    }
    if (valueIsRingElement(a[0]) && valueIsRingElement(a[1])) {
        RingElement* prod = ringMult((RingElement*)a[0].as.ptr, (RingElement*)a[1].as.ptr);
        if (!prod) return vectorBinaryError("ring '*' requires two elements from the same ring", a[0], a[1]);
        valFree(a[0]);
        valFree(a[1]);
        return valPtr(VAL_RING_ELEMENT, prod);
    }
    if (valueIsRingElement(a[0]) && a[1].kind == VAL_INT) {
        RingElement* prod = ringTimes((RingElement*)a[0].as.ptr, (int)a[1].as.i);
        if (!prod) return vectorBinaryError("ring '*' with an integer requires a valid ring element", a[0], a[1]);
        valFree(a[0]);
        valFree(a[1]);
        return valPtr(VAL_RING_ELEMENT, prod);
    }
    if (a[0].kind == VAL_INT && valueIsRingElement(a[1])) {
        RingElement* prod = ringTimes((RingElement*)a[1].as.ptr, (int)a[0].as.i);
        if (!prod) return vectorBinaryError("integer '*' ring requires a valid ring element", a[0], a[1]);
        valFree(a[0]);
        valFree(a[1]);
        return valPtr(VAL_RING_ELEMENT, prod);
    }
    if (a[0].kind == VAL_INT && valueIsCombSet(a[1])) {
        int k = (int)a[0].as.i;
        if (k == 0) return combsetBinaryError("CombSet scalar '*' expects a non-zero integer multiplier", a[0], a[1]);
        if (valueIsEmptyCombSet(a[1])) return wrapCombSetResult(NULL, NULL, a[0], a[1]);

        CombSet* out = k > 0
            ? kads((CombSet*)a[1].as.ptr, k)
            : kdds((CombSet*)a[1].as.ptr, -k);
        return wrapCombSetResult(out, "CombSet scalar '*' produced an unsupported empty result", a[0], a[1]);
    }
    if (valueIsCombSet(a[0]) && a[1].kind == VAL_INT) {
        if (valueIsEmptyCombSet(a[0])) return wrapCombSetResult(NULL, NULL, a[0], a[1]);
        CombSet* out = dilateSet((CombSet*)a[0].as.ptr, a[1].as.i);
        return wrapCombSetResult(out, "CombSet right '*' dilation failed", a[0], a[1]);
    }
    if (valueIsCombSet(a[0]) && valueIsCombSet(a[1])) {
        if (valueIsEmptyCombSet(a[0]) || valueIsEmptyCombSet(a[1])) {
            return wrapCombSetResult(NULL, NULL, a[0], a[1]);
        }
        CombSet* prod = multiplySets((CombSet*)a[0].as.ptr, (CombSet*)a[1].as.ptr);
        return wrapCombSetResult(prod, "CombSet '*' produced an unsupported empty result", a[0], a[1]);
    }
    if (valueIsVector(a[0]) && valueIsVector(a[1])) {
        Value dot = valueFromMatrixElement(vectorDotProduct((Vector*)a[0].as.ptr, (Vector*)a[1].as.ptr));
        if (dot.kind == VAL_ERROR) return vectorBinaryError("vector '*' requires same dimension vectors", a[0], a[1]);
        valFree(a[0]);
        valFree(a[1]);
        return dot;
    }
    if (a[0].kind == VAL_MATRIX && valueIsVector(a[1])) {
        Vector* applied = (Vector*)applyMatrix((Matrix*)a[0].as.ptr, (Vector*)a[1].as.ptr);
        if (!applied) return vectorBinaryError("matrix '*' requires compatible matrix/vector dimensions", a[0], a[1]);
        valFree(a[0]);
        valFree(a[1]);
        return valPtr(VAL_VECTOR, applied);
    }
    if (a[0].kind == VAL_MATRIX && a[1].kind == VAL_MATRIX) {
        Matrix* prod = multiplyMatrices((Matrix*)a[0].as.ptr, (Matrix*)a[1].as.ptr);
        if (!prod) return matrixOpError("matrix '*' requires compatible dimensions", a[0], a[1]);
        valFree(a[0]);
        valFree(a[1]);
        return valPtr(VAL_MATRIX, prod);
    }
    if (valueIsVector(a[0])) {
        MatrixElement scalar;
        if (valueToScalarElement(a[1], &scalar)) {
            Vector* prod = scaleVector((Vector*)a[0].as.ptr, scalar);
            if (!prod) return vectorBinaryError("vector '*' failed during scalar multiplication", a[0], a[1]);
            valFree(a[0]);
            valFree(a[1]);
            return valPtr(VAL_VECTOR, prod);
        }
    }
    if (valueIsVector(a[1])) {
        MatrixElement scalar;
        if (valueToScalarElement(a[0], &scalar)) {
            Vector* prod = scaleVector((Vector*)a[1].as.ptr, scalar);
            if (!prod) return vectorBinaryError("vector '*' failed during scalar multiplication", a[0], a[1]);
            valFree(a[0]);
            valFree(a[1]);
            return valPtr(VAL_VECTOR, prod);
        }
    }
    if (a[0].kind == VAL_MATRIX) {
        MatrixElement scalar;
        if (valueToScalarElement(a[1], &scalar)) {
            Matrix* prod = multByConstant((Matrix*)a[0].as.ptr, scalar);
            if (!prod) return matrixOpError("matrix '*' failed during scalar multiplication", a[0], a[1]);
            valFree(a[0]);
            valFree(a[1]);
            return valPtr(VAL_MATRIX, prod);
        }
    }
    if (a[1].kind == VAL_MATRIX) {
        MatrixElement scalar;
        if (valueToScalarElement(a[0], &scalar)) {
            Matrix* prod = multByConstant((Matrix*)a[1].as.ptr, scalar);
            if (!prod) return matrixOpError("matrix '*' failed during scalar multiplication", a[0], a[1]);
            valFree(a[0]);
            valFree(a[1]);
            return valPtr(VAL_MATRIX, prod);
        }
    }
    Value r = numericBinop(a[0], a[1], '*');
    valFree(a[0]); valFree(a[1]);
    return r;
}

// A \otimes B
static Value bi_otimes(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (a[0].kind == VAL_MATRIX && a[1].kind == VAL_MATRIX) {
        Matrix* tensor = tensorMatrices((Matrix*)a[0].as.ptr, (Matrix*)a[1].as.ptr);
        if (!tensor) return matrixOpError("\\otimes requires two matrices", a[0], a[1]);
        valFree(a[0]);
        valFree(a[1]);
        return valPtr(VAL_MATRIX, tensor);
    }
    return matrixOpError("\\otimes requires two matrices", a[0], a[1]);
}

// v1 \cdot v2
static Value bi_cdot(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (valueIsVector(a[0]) && valueIsVector(a[1])) {
        Value dot = valueFromMatrixElement(vectorDotProduct((Vector*)a[0].as.ptr, (Vector*)a[1].as.ptr));
        if (dot.kind == VAL_ERROR) return vectorBinaryError("\\cdot requires same dimension vectors", a[0], a[1]);
        valFree(a[0]);
        valFree(a[1]);
        return dot;
    }
    return vectorBinaryError("\\cdot requires two vectors", a[0], a[1]);
}

// v1 \times v2
static Value bi_times(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (valueIsGroup(a[0]) && valueIsGroup(a[1])) {
        Group* prod = constructProductGroup((Group*)a[0].as.ptr, (Group*)a[1].as.ptr);
        if (!prod) return vectorBinaryError("\\times requires two valid groups", a[0], a[1]);
        valFree(a[0]);
        valFree(a[1]);
        return valPtr(VAL_GROUP, prod);
    }
    if (valueIsRing(a[0]) && valueIsRing(a[1])) {
        Ring* prod = constructProductRing((Ring*)a[0].as.ptr, (Ring*)a[1].as.ptr);
        if (!prod) return vectorBinaryError("\\times requires two valid rings", a[0], a[1]);
        valFree(a[0]);
        valFree(a[1]);
        return valPtr(VAL_RING, prod);
    }
    if (valueIsVector(a[0]) && valueIsVector(a[1])) {
        Vector* cross = crossProduct((Vector*)a[0].as.ptr, (Vector*)a[1].as.ptr);
        if (!cross) return vectorBinaryError("\\times requires two 3D vectors", a[0], a[1]);
        valFree(a[0]);
        valFree(a[1]);
        return valPtr(VAL_VECTOR, cross);
    }
    return vectorBinaryError("\\times requires two groups, two rings, or two 3D vectors", a[0], a[1]);
}

// a / b
static Value bi_div(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (valueIsGroup(a[0]) && valueIsSubgroup(a[1])) {
        Group* quot = quotientGroup((Group*)a[0].as.ptr, (SubGroup*)a[1].as.ptr);
        if (!quot) return vectorBinaryError("group '/' requires a normal subgroup of the given group", a[0], a[1]);
        valFree(a[0]);
        valFree(a[1]);
        return valPtr(VAL_GROUP, quot);
    }
    if (valueIsRing(a[0]) && valueIsIdeal(a[1])) {
        Ring* quot = quotientRing((Ring*)a[0].as.ptr, (Ideal*)a[1].as.ptr);
        if (!quot) return vectorBinaryError("ring '/' requires an ideal of the given ring", a[0], a[1]);
        valFree(a[0]);
        valFree(a[1]);
        return valPtr(VAL_RING, quot);
    }
    if (valueIsGroupElement(a[0]) && valueIsGroupElement(a[1])) {
        GroupElement* rhsInv = groupInverse((GroupElement*)a[1].as.ptr);
        GroupElement* quot = rhsInv ? groupMult((GroupElement*)a[0].as.ptr, rhsInv) : NULL;
        if (!quot) return vectorBinaryError("group '/' requires two elements from the same group", a[0], a[1]);
        valFree(a[0]);
        valFree(a[1]);
        return valPtr(VAL_GROUP_ELEMENT, quot);
    }
    if (valueIsRingElement(a[0]) && valueIsRingElement(a[1])) {
        RingElement* rhsInv = ringMultInverse((RingElement*)a[1].as.ptr);
        RingElement* quot = rhsInv ? ringMult((RingElement*)a[0].as.ptr, rhsInv) : NULL;
        if (!quot) return vectorBinaryError("ring '/' requires two elements from the same ring and a multiplicative inverse", a[0], a[1]);
        valFree(a[0]);
        valFree(a[1]);
        return valPtr(VAL_RING_ELEMENT, quot);
    }
    Value r = numericBinop(a[0], a[1], '/');
    valFree(a[0]); valFree(a[1]);
    return r;
}

// -x
static Value bi_neg(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    Value v = a[0];
    Value r;
    if      (valueIsVector(v))       {
        Vector* neg = negativeVector((Vector*)v.as.ptr);
        r = neg ? valPtr(VAL_VECTOR, neg) : valError("unary '-' on invalid vector");
    }
    else if (valueIsRingElement(v))  {
        RingElement* neg = ringAddInverse((RingElement*)v.as.ptr);
        r = neg ? valPtr(VAL_RING_ELEMENT, neg) : valError("unary '-' on invalid ring element");
    }
    else if (valueIsCombSet(v))      {
        CombSet* neg = v.as.ptr ? negateSet((CombSet*)v.as.ptr) : NULL;
        r = valPtr(VAL_COMBSET, neg);
    }
    else if (v.kind == VAL_INT)      r = valInt(-v.as.i);
    else if (v.kind == VAL_DECIMAL)  r = valDecimal(-v.as.d);
    else if (v.kind == VAL_FRACTION) r = valFraction(constructFraction(-v.as.frac.num, v.as.frac.denom));
    else if (v.kind == VAL_COMPLEX)  r = valComplex(complexNeg(v.as.cplx));
    else                             r = valError("unary '-' on non-numeric");
    valFree(v);
    return r;
}

// +x (no-op, kept for symmetry)
static Value bi_pos(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    return a[0];
}

// x ^ y
static Value bi_pow(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    Value b = a[0], e = a[1];
    Value r;
    if (valueIsCombSet(b)) {
        if (e.kind != VAL_INT || e.as.i < 1) {
            r = valError("CombSet '^' expects a positive integer exponent");
        } else if (valueIsEmptyCombSet(b)) {
            r = valPtr(VAL_COMBSET, NULL);
        } else {
            CombSet* out = kmds((CombSet*)b.as.ptr, (int)e.as.i);
            r = out ? valPtr(VAL_COMBSET, out) : valError("CombSet '^' failed");
        }
    } else if (valueIsGroupElement(b)) {
        int exponent;
        if (!valueToUsagiInt(e, &exponent)) {
            r = valError("group element '^' expects an integer exponent");
        } else {
            GroupElement* out = groupExp((GroupElement*)b.as.ptr, exponent);
            r = out ? valPtr(VAL_GROUP_ELEMENT, out) : valError("group element '^' failed");
        }
    } else if (valueIsRingElement(b)) {
        int exponent;
        if (!valueToUsagiInt(e, &exponent)) {
            r = valError("ring element '^' expects an integer exponent");
        } else {
            RingElement* out = ringExp((RingElement*)b.as.ptr, exponent);
            r = out ? valPtr(VAL_RING_ELEMENT, out) : valError("ring element '^' failed");
        }
    } else if (b.kind == VAL_MATRIX) {
        Matrix* out = NULL;
        if (e.kind == VAL_INT) {
            out = matrixPow((Matrix*)b.as.ptr, (int)e.as.i);
            if (!out) {
                r = valError("matrix power requires a square matrix and valid integer exponent");
            } else {
                r = valPtr(VAL_MATRIX, out);
            }
        } else if (e.kind == VAL_SYMBOL && e.as.str) {
            if (strcmp(e.as.str, "T") == 0) {
                out = transpose((Matrix*)b.as.ptr);
                r = out ? valPtr(VAL_MATRIX, out) : valError("matrix transpose failed");
            } else if (strcmp(e.as.str, "t") == 0) {
                out = adjoint((Matrix*)b.as.ptr);
                r = out ? valPtr(VAL_MATRIX, out) : valError("matrix adjoint failed");
            } else {
                r = valError("matrix '^' only supports integer exponents, T, and t");
            }
        } else {
            r = valError("matrix '^' only supports integer exponents, T, and t");
        }
    } else if (valIsNumeric(b) && valIsNumeric(e)) {
        r = valDecimal(pow(valToDouble(b), valToDouble(e)));
    } else {
        r = valError("'^' not defined for these types; register your own");
    }
    valFree(b); valFree(e);
    return r;
}

/* ---------- Example BEAST-backed commands ---------- */

// \pi -- transcendental constant from hebi
static Value bi_pi(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)a; (void)n;
    return valDecimal((double)pi());
}

// \e -- transcendental constant from hebi
static Value bi_e(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)a; (void)n;
    return valDecimal((double)e());
}

// \phi -- golden ratio from hebi
static Value bi_phi(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)a; (void)n;
    return valDecimal ((double)phi());
}

// \frac{a}{b} -- build a hebi Fraction from two integer arguments
static Value bi_frac(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    Value num = a[0], den = a[1];
    Value r;
    if (num.kind == VAL_INT && den.kind == VAL_INT) {
        r = valFraction(constructFraction(num.as.i, den.as.i));
    } else {
        r = valError("\\frac expects two integers");
    }
    valFree(num); valFree(den);
    return r;
}

static Value bi_sqrt(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    Value x = a[0];
    Value r;

    if (x.kind == VAL_INT) {
        long long root;
        if (intSqrtExact(x.as.i, &root)) r = valInt(root);
        else                             r = valDecimal(sqrt((double)x.as.i));
        valFree(x);
        return r;
    }

    if (x.kind == VAL_FRACTION) {
        if (x.as.frac.denom == 0) {
            valFree(x);
            return valError("\\sqrt undefined for invalid fraction");
        }
        long long numRoot, denRoot;
        if (intSqrtExact(x.as.frac.num, &numRoot) && intSqrtExact(x.as.frac.denom, &denRoot)) {
            r = valFraction(constructFraction(numRoot, denRoot));
        } else {
            r = valDecimal(sqrt(valToDouble(x)));
        }
        valFree(x);
        return r;
    }

    if (x.kind == VAL_DECIMAL) {
        r = valDecimal(sqrt(x.as.d));
        valFree(x);
        return r;
    }

    r = valError("\\sqrt expects one numeric argument");
    valFree(x);
    return r;
}

static Value bi_imatrix(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    Value dim = a[0];
    if (dim.kind != VAL_INT || dim.as.i < 1) {
        valFree(dim);
        return valError("\\imatrix expects one positive integer");
    }

    Matrix* matrix = idMatrix((int)dim.as.i);
    valFree(dim);
    if (!matrix) return valError("\\imatrix failed to construct matrix");
    return valPtr(VAL_MATRIX, matrix);
}

static Value bi_zeromatrix(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    Value dim = a[0];
    if (dim.kind != VAL_INT || dim.as.i < 1) {
        valFree(dim);
        return valError("\\zeromatrix expects one positive integer");
    }

    Matrix* matrix = constructMatrix((int)dim.as.i, (int)dim.as.i);
    valFree(dim);
    if (!matrix) return valError("\\zeromatrix failed to construct matrix");
    return valPtr(VAL_MATRIX, matrix);
}

static Value bi_issquare(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (a[0].kind != VAL_MATRIX) return matrixUnaryError("\\issquare expects one matrix", a[0]);
    bool square = isSquare((Matrix*)a[0].as.ptr);
    valFree(a[0]);
    return valBool(square);
}

static Value bi_det(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (a[0].kind != VAL_MATRIX) return matrixUnaryError("\\det expects one matrix", a[0]);
    Matrix* matrix = (Matrix*)a[0].as.ptr;
    if (!isSquare(matrix)) return matrixUnaryError("\\det expects a square matrix", a[0]);
    Value r = valueFromMatrixElement(determinant(matrix));
    valFree(a[0]);
    return r;
}

static Value bi_copy(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (a[0].kind != VAL_MATRIX) return matrixUnaryError("\\copy expects one matrix", a[0]);
    Matrix* copy = copyMatrix((Matrix*)a[0].as.ptr);
    if (!copy) return matrixUnaryError("\\copy failed", a[0]);
    valFree(a[0]);
    return valPtr(VAL_MATRIX, copy);
}

static Value bi_transpose(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (a[0].kind != VAL_MATRIX) return matrixUnaryError("\\transpose expects one matrix", a[0]);
    Matrix* out = transpose((Matrix*)a[0].as.ptr);
    if (!out) return matrixUnaryError("\\transpose failed", a[0]);
    valFree(a[0]);
    return valPtr(VAL_MATRIX, out);
}

static Value bi_adjoint(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (a[0].kind != VAL_MATRIX) return matrixUnaryError("\\adjoint expects one matrix", a[0]);
    Matrix* out = adjoint((Matrix*)a[0].as.ptr);
    if (!out) return matrixUnaryError("\\adjoint failed", a[0]);
    valFree(a[0]);
    return valPtr(VAL_MATRIX, out);
}

static Value bi_inverse(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (a[0].kind != VAL_MATRIX) return matrixUnaryError("\\inverse expects one matrix", a[0]);
    Matrix* out = invertMatrix((Matrix*)a[0].as.ptr);
    if (!out) return matrixUnaryError("\\inverse expects an invertible square matrix", a[0]);
    valFree(a[0]);
    return valPtr(VAL_MATRIX, out);
}

static Value bi_rowReduce(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (a[0].kind != VAL_MATRIX) return matrixUnaryError("\\rowReduce expects one matrix", a[0]);
    Matrix* out = reduceRows((Matrix*)a[0].as.ptr);
    if (!out) return matrixUnaryError("\\rowReduce failed", a[0]);
    valFree(a[0]);
    return valPtr(VAL_MATRIX, out);
}

static Value bi_columnReduce(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (a[0].kind != VAL_MATRIX) return matrixUnaryError("\\columnReduce expects one matrix", a[0]);
    Matrix* out = reduceColumns((Matrix*)a[0].as.ptr);
    if (!out) return matrixUnaryError("\\columnReduce failed", a[0]);
    valFree(a[0]);
    return valPtr(VAL_MATRIX, out);
}

static Value bi_rowSpace(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (a[0].kind != VAL_MATRIX) return matrixUnaryError("\\rowSpace expects one matrix", a[0]);

    int count = 0;
    Vector** basis = rowSpace((Matrix*)a[0].as.ptr, &count);
    valFree(a[0]);
    if (!basis && count > 0) return valError("\\rowSpace failed");
    return vectorArrayToList(basis, count);
}

static Value bi_columnSpace(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (a[0].kind != VAL_MATRIX) return matrixUnaryError("\\columnSpace expects one matrix", a[0]);

    int count = 0;
    Vector** basis = columnSpace((Matrix*)a[0].as.ptr, &count);
    valFree(a[0]);
    if (!basis && count > 0) return valError("\\columnSpace failed");
    return vectorArrayToList(basis, count);
}

static Value bi_solveLinEq(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (a[0].kind != VAL_MATRIX) return matrixOpError("\\solveLinEq expects a matrix and a vector", a[0], a[1]);
    if (!(a[1].kind == VAL_VECTOR || a[1].kind == VAL_MATRIX)) {
        return matrixOpError("\\solveLinEq expects a matrix and a vector", a[0], a[1]);
    }

    Matrix* solution = solveLinEq((Matrix*)a[0].as.ptr, (Matrix*)a[1].as.ptr);
    if (!solution) {
        return matrixOpError("\\solveLinEq requires a square matrix and compatible column vector", a[0], a[1]);
    }

    valFree(a[0]);
    valFree(a[1]);
    return valPtr(solution->numCols == 1 ? VAL_VECTOR : VAL_MATRIX, solution);
}

static Value bi_issym(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (a[0].kind != VAL_MATRIX) return matrixUnaryError("\\issym expects one matrix", a[0]);
    bool result = isSymmetric((Matrix*)a[0].as.ptr);
    valFree(a[0]);
    return valBool(result);
}

static Value bi_isantisym(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (a[0].kind != VAL_MATRIX) return matrixUnaryError("\\isantisym expects one matrix", a[0]);
    bool result = isAntisymmetric((Matrix*)a[0].as.ptr);
    valFree(a[0]);
    return valBool(result);
}

static Value bi_isunitary(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (a[0].kind != VAL_MATRIX) return matrixUnaryError("\\isunitary expects one matrix", a[0]);
    bool result = isUnitary((Matrix*)a[0].as.ptr);
    valFree(a[0]);
    return valBool(result);
}

static Value bi_isorthog(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (a[0].kind != VAL_MATRIX) return matrixUnaryError("\\isorthog expects one matrix", a[0]);
    bool result = isOrthogonal((Matrix*)a[0].as.ptr);
    valFree(a[0]);
    return valBool(result);
}

static Value bi_rank(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (a[0].kind != VAL_MATRIX) return matrixUnaryError("\\rank expects one matrix", a[0]);
    int result = rank((Matrix*)a[0].as.ptr);
    if (result < 0) return matrixUnaryError("\\rank failed", a[0]);
    valFree(a[0]);
    return valInt(result);
}

static Value bi_nullity(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (a[0].kind != VAL_MATRIX) return matrixUnaryError("\\nullity expects one matrix", a[0]);
    int result = nullity((Matrix*)a[0].as.ptr);
    if (result < 0) return matrixUnaryError("\\nullity failed", a[0]);
    valFree(a[0]);
    return valInt(result);
}

static Value bi_trace(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (a[0].kind != VAL_MATRIX) return matrixUnaryError("\\trace expects one matrix", a[0]);
    Matrix* matrix = (Matrix*)a[0].as.ptr;
    if (!isSquare(matrix)) return matrixUnaryError("\\trace expects a square matrix", a[0]);
    Value result = valueFromMatrixElement(trace(matrix));
    valFree(a[0]);
    return result;
}

static Value bi_fnorm(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (a[0].kind != VAL_MATRIX) return matrixUnaryError("\\fnorm expects one matrix", a[0]);
    double result = frobeniusNorm((Matrix*)a[0].as.ptr);
    valFree(a[0]);
    return valDecimal(result);
}

static Value bi_eigenvalues(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (a[0].kind != VAL_MATRIX) return matrixUnaryError("\\eigenvalues expects one matrix", a[0]);

    Matrix* matrix = (Matrix*)a[0].as.ptr;
    MatrixElement* eigs = NULL;
    size_t count = (size_t)matrix->numRows;

    if (matrix->numRows != matrix->numCols) {
        return matrixUnaryError("\\eigenvalues expects a square matrix", a[0]);
    }

    eigs = eigenvalues(matrix);

    if (!eigs) return matrixUnaryError("\\eigenvalues failed", a[0]);

    Value* items = calloc(count, sizeof(Value));
    if (!items) {
        free(eigs);
        return matrixUnaryError("\\eigenvalues failed to allocate result list", a[0]);
    }
    for (size_t i = 0; i < count; i++) items[i] = valueFromMatrixElement(eigs[i]);

    free(eigs);
    valFree(a[0]);
    return valList(items, count);
}

static Value bi_eigenvectors(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (a[0].kind != VAL_MATRIX) return matrixUnaryError("\\eigenvectors expects one matrix", a[0]);

    Matrix* matrix = (Matrix*)a[0].as.ptr;
    Matrix** eigvecs = NULL;
    size_t count = (size_t)matrix->numRows;

    if (matrix->numRows != matrix->numCols) {
        return matrixUnaryError("\\eigenvectors expects a square matrix", a[0]);
    }

    eigvecs = eigenvectors(matrix);

    if (!eigvecs) return matrixUnaryError("\\eigenvectors failed", a[0]);

    Value* items = calloc(count, sizeof(Value));
    if (!items) {
        for (size_t i = 0; i < count; i++) freeMatrix(eigvecs[i]);
        free(eigvecs);
        return matrixUnaryError("\\eigenvectors failed to allocate result list", a[0]);
    }
    for (size_t i = 0; i < count; i++) {
        if (eigvecs[i]) items[i] = valPtr(VAL_VECTOR, eigvecs[i]);
        else            items[i] = valNone();
    }

    free(eigvecs);
    valFree(a[0]);
    return valList(items, count);
}

static Value bi_l2norm(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (valueIsCombSet(a[0])) {
        int card = combsetCard(a[0]);
        valFree(a[0]);
        return valInt(card);
    }
    if (!valueIsVector(a[0])) return vectorUnaryError("\\l2norm expects one vector", a[0]);
    double norm = l2Norm((Vector*)a[0].as.ptr);
    valFree(a[0]);
    return valDecimal(norm);
}

static Value bi_isSubset(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsCombSet(a[0]) || !valueIsCombSet(a[1])) {
        return combsetBinaryError("\\isSubset expects two CombSets", a[0], a[1]);
    }
    bool result;
    if (valueIsEmptyCombSet(a[0])) result = true;
    else if (valueIsEmptyCombSet(a[1])) result = false;
    else result = isSubset((CombSet*)a[0].as.ptr, (CombSet*)a[1].as.ptr);
    valFree(a[0]);
    valFree(a[1]);
    return valBool(result);
}

static Value bi_cap(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsCombSet(a[0]) || !valueIsCombSet(a[1])) {
        return combsetBinaryError("\\cap expects two CombSets", a[0], a[1]);
    }
    if (valueIsEmptyCombSet(a[0]) || valueIsEmptyCombSet(a[1])) {
        return wrapCombSetResult(NULL, NULL, a[0], a[1]);
    }
    CombSet* out = setIntersection((CombSet*)a[0].as.ptr, (CombSet*)a[1].as.ptr);
    return wrapCombSetResult(out, NULL, a[0], a[1]);
}

static Value bi_cup(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsCombSet(a[0]) || !valueIsCombSet(a[1])) {
        return combsetBinaryError("\\cup expects two CombSets", a[0], a[1]);
    }
    if (valueIsEmptyCombSet(a[0])) {
        Value result = valClone(a[1]);
        valFree(a[0]);
        valFree(a[1]);
        return result;
    }
    if (valueIsEmptyCombSet(a[1])) {
        Value result = valClone(a[0]);
        valFree(a[0]);
        valFree(a[1]);
        return result;
    }
    CombSet* out = setUnion((CombSet*)a[0].as.ptr, (CombSet*)a[1].as.ptr);
    return wrapCombSetResult(out, "CombSet union failed", a[0], a[1]);
}

static Value bi_diam(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsCombSet(a[0])) return combsetUnaryError("\\diam expects one CombSet", a[0]);
    if (valueIsEmptyCombSet(a[0])) return combsetUnaryError("\\diam is undefined for the empty set", a[0]);
    long long diam = getDiameter((CombSet*)a[0].as.ptr);
    valFree(a[0]);
    return valInt(diam);
}

static Value bi_dc(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsCombSet(a[0])) return combsetUnaryError("\\dc expects one CombSet", a[0]);
    if (valueIsEmptyCombSet(a[0])) return combsetUnaryError("\\dc is undefined for the empty set", a[0]);
    Fraction dc = doublingConstant((CombSet*)a[0].as.ptr);
    valFree(a[0]);
    if (dc.denom == 1) return valInt(dc.num);
    return valFraction(dc);
}

static Value bi_density(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsCombSet(a[0])) return combsetUnaryError("\\density expects one CombSet", a[0]);
    if (valueIsEmptyCombSet(a[0])) return combsetUnaryError("\\density is undefined for the empty set", a[0]);
    Fraction density = getDensity((CombSet*)a[0].as.ptr);
    valFree(a[0]);
    if (density.denom == 1) return valInt(density.num);
    return valFraction(density);
}

static Value bi_translate(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsCombSet(a[0]) || a[1].kind != VAL_INT) {
        return combsetBinaryError("\\translate expects a CombSet and an integer", a[0], a[1]);
    }
    if (valueIsEmptyCombSet(a[0])) return wrapCombSetResult(NULL, NULL, a[0], a[1]);
    CombSet* out = translateSet((CombSet*)a[0].as.ptr, a[1].as.i);
    return wrapCombSetResult(out, "\\translate failed", a[0], a[1]);
}

static Value bi_dilate(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsCombSet(a[0]) || a[1].kind != VAL_INT) {
        return combsetBinaryError("\\dilate expects a CombSet and an integer", a[0], a[1]);
    }
    if (valueIsEmptyCombSet(a[0])) return wrapCombSetResult(NULL, NULL, a[0], a[1]);
    CombSet* out = dilateSet((CombSet*)a[0].as.ptr, a[1].as.i);
    return wrapCombSetResult(out, "\\dilate failed", a[0], a[1]);
}

static Value bi_append(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    long long x;
    if (!valueIsCombSet(a[0]) || !valueToCombSetInt(a[1], &x)) {
        return combsetBinaryError("\\append expects a CombSet and an integer", a[0], a[1]);
    }
    if (valueIsEmptyCombSet(a[0])) {
        long long elem = x;
        valFree(a[0]);
        valFree(a[1]);
        CombSet* out = constructCombset(&elem, 1);
        return out ? valPtr(VAL_COMBSET, out) : valError("\\append failed");
    }
    addElement((CombSet*)a[0].as.ptr, x);
    valFree(a[1]);
    return a[0];
}

static Value bi_remove(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    long long x;
    if (!valueIsCombSet(a[0]) || !valueToCombSetInt(a[1], &x)) {
        return combsetBinaryError("\\remove expects a CombSet and an integer", a[0], a[1]);
    }
    if (valueIsEmptyCombSet(a[0])) {
        return combsetBinaryError("\\remove failed: element not in set", a[0], a[1]);
    }
    if (!combsetContains((CombSet*)a[0].as.ptr, x)) {
        return combsetBinaryError("\\remove failed: element not in set", a[0], a[1]);
    }
    removeElement((CombSet*)a[0].as.ptr, x);
    valFree(a[1]);
    if (((CombSet*)a[0].as.ptr)->card == 0) {
        valFree(a[0]);
        return valPtr(VAL_COMBSET, NULL);
    }
    return a[0];
}

static Value bi_adsCard(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsCombSet(a[0])) return combsetUnaryError("\\adsCard expects one CombSet", a[0]);
    if (valueIsEmptyCombSet(a[0])) {
        valFree(a[0]);
        return valInt(0);
    }
    int result = adsCard((CombSet*)a[0].as.ptr);
    valFree(a[0]);
    return valInt(result);
}

static Value bi_ddsCard(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsCombSet(a[0])) return combsetUnaryError("\\ddsCard expects one CombSet", a[0]);
    if (valueIsEmptyCombSet(a[0])) {
        valFree(a[0]);
        return valInt(0);
    }
    int result = ddsCard((CombSet*)a[0].as.ptr);
    valFree(a[0]);
    return valInt(result);
}

static Value bi_mdsCard(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsCombSet(a[0])) return combsetUnaryError("\\mdsCard expects one CombSet", a[0]);
    if (valueIsEmptyCombSet(a[0])) {
        valFree(a[0]);
        return valInt(0);
    }
    int result = mdsCard((CombSet*)a[0].as.ptr);
    valFree(a[0]);
    return valInt(result);
}

static Value bi_isAP(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsCombSet(a[0])) return combsetUnaryError("\\isAP expects one CombSet", a[0]);
    if (valueIsEmptyCombSet(a[0])) {
        valFree(a[0]);
        return valBool(true);
    }
    bool result = isArithmeticProgression((CombSet*)a[0].as.ptr);
    valFree(a[0]);
    return valBool(result);
}

static Value bi_isGP(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsCombSet(a[0])) return combsetUnaryError("\\isGP expects one CombSet", a[0]);
    if (valueIsEmptyCombSet(a[0])) {
        valFree(a[0]);
        return valBool(true);
    }
    bool result = isGeometricProgression((CombSet*)a[0].as.ptr);
    valFree(a[0]);
    return valBool(result);
}

static Value bi_rd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsCombSet(a[0]) || !valueIsCombSet(a[1])) {
        return combsetBinaryError("\\ruzsaDistance expects two CombSets", a[0], a[1]);
    }
    double result = (double)(valueIsEmptyCombSet(a[0]) || valueIsEmptyCombSet(a[1])
        ? 0.0L
        : ruzsaDistance((CombSet*)a[0].as.ptr, (CombSet*)a[1].as.ptr));
    valFree(a[0]);
    valFree(a[1]);
    return valDecimal(result);
}

static Value bi_rdpos(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsCombSet(a[0]) || !valueIsCombSet(a[1])) {
        return combsetBinaryError("\\ruzsaDistancePositive expects two CombSets", a[0], a[1]);
    }
    double result = (double)(valueIsEmptyCombSet(a[0]) || valueIsEmptyCombSet(a[1])
        ? 0.0L
        : ruzsaDistancePositive((CombSet*)a[0].as.ptr, (CombSet*)a[1].as.ptr));
    valFree(a[0]);
    valFree(a[1]);
    return valDecimal(result);
}

static Value bi_repAdd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    long long x;
    if (!valueIsCombSet(a[0]) || !valueToCombSetInt(a[1], &x)) {
        return combsetBinaryError("\\repAdd expects a CombSet and an integer", a[0], a[1]);
    }
    long long result = valueIsEmptyCombSet(a[0]) ? 0 : repAdd((CombSet*)a[0].as.ptr, x);
    valFree(a[0]);
    valFree(a[1]);
    return valInt(result);
}

static Value bi_kRepAdd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    long long k, x;
    if (!valueIsCombSet(a[0]) || !valueToCombSetInt(a[1], &k) || !valueToCombSetInt(a[2], &x)) {
        valFree(a[0]);
        valFree(a[1]);
        valFree(a[2]);
        return valError("\\kRepAdd expects a CombSet, an integer k, and an integer x");
    }
    if (k < 1) {
        valFree(a[0]);
        valFree(a[1]);
        valFree(a[2]);
        return valError("\\kRepAdd expects k >= 1");
    }
    long long result = valueIsEmptyCombSet(a[0]) ? 0 : kRepAdd((CombSet*)a[0].as.ptr, x, (int)k);
    valFree(a[0]);
    valFree(a[1]);
    valFree(a[2]);
    return valInt(result);
}

static Value bi_repDiff(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    long long x;
    if (!valueIsCombSet(a[0]) || !valueToCombSetInt(a[1], &x)) {
        return combsetBinaryError("\\repDiff expects a CombSet and an integer", a[0], a[1]);
    }
    long long result = valueIsEmptyCombSet(a[0]) ? 0 : repDiff((CombSet*)a[0].as.ptr, x);
    valFree(a[0]);
    valFree(a[1]);
    return valInt(result);
}

static Value bi_kRepDiff(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    long long k, x;
    if (!valueIsCombSet(a[0]) || !valueToCombSetInt(a[1], &k) || !valueToCombSetInt(a[2], &x)) {
        valFree(a[0]);
        valFree(a[1]);
        valFree(a[2]);
        return valError("\\kRepDiff expects a CombSet, an integer k, and an integer x");
    }
    if (k < 1) {
        valFree(a[0]);
        valFree(a[1]);
        valFree(a[2]);
        return valError("\\kRepDiff expects k >= 1");
    }
    long long result = valueIsEmptyCombSet(a[0]) ? 0 : kRepDiff((CombSet*)a[0].as.ptr, x, (int)k);
    valFree(a[0]);
    valFree(a[1]);
    valFree(a[2]);
    return valInt(result);
}

static Value bi_repMult(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    long long x;
    if (!valueIsCombSet(a[0]) || !valueToCombSetInt(a[1], &x)) {
        return combsetBinaryError("\\repMult expects a CombSet and an integer", a[0], a[1]);
    }
    long long result = valueIsEmptyCombSet(a[0]) ? 0 : repMult((CombSet*)a[0].as.ptr, x);
    valFree(a[0]);
    valFree(a[1]);
    return valInt(result);
}

static Value bi_kRepMult(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    long long k, x;
    if (!valueIsCombSet(a[0]) || !valueToCombSetInt(a[1], &k) || !valueToCombSetInt(a[2], &x)) {
        valFree(a[0]);
        valFree(a[1]);
        valFree(a[2]);
        return valError("\\kRepMult expects a CombSet, an integer k, and an integer x");
    }
    if (k < 1) {
        valFree(a[0]);
        valFree(a[1]);
        valFree(a[2]);
        return valError("\\kRepMult expects k >= 1");
    }
    long long result = valueIsEmptyCombSet(a[0]) ? 0 : kRepMult((CombSet*)a[0].as.ptr, x, (int)k);
    valFree(a[0]);
    valFree(a[1]);
    valFree(a[2]);
    return valInt(result);
}

static Value bi_energyAdd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsCombSet(a[0])) return combsetUnaryError("\\energyAdd expects one CombSet", a[0]);
    long long result = valueIsEmptyCombSet(a[0]) ? 0 : addEnergy((CombSet*)a[0].as.ptr);
    valFree(a[0]);
    return valInt(result);
}

static Value bi_kEnergyAdd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    long long k;
    if (!valueIsCombSet(a[0]) || !valueToCombSetInt(a[1], &k)) {
        return combsetBinaryError("\\kEnergyAdd expects a CombSet and an integer k", a[0], a[1]);
    }
    if (k < 1) return combsetBinaryError("\\kEnergyAdd expects k >= 1", a[0], a[1]);
    long long result = valueIsEmptyCombSet(a[0]) ? 0 : kEnergyAdd((CombSet*)a[0].as.ptr, (int)k);
    valFree(a[0]);
    valFree(a[1]);
    return valInt(result);
}

static Value bi_energyDiff(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsCombSet(a[0])) return combsetUnaryError("\\energyDiff expects one CombSet", a[0]);
    long long result = valueIsEmptyCombSet(a[0]) ? 0 : diffEnergy((CombSet*)a[0].as.ptr);
    valFree(a[0]);
    return valInt(result);
}

static Value bi_kEnergyDiff(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    long long k;
    if (!valueIsCombSet(a[0]) || !valueToCombSetInt(a[1], &k)) {
        return combsetBinaryError("\\kEnergyDiff expects a CombSet and an integer k", a[0], a[1]);
    }
    if (k < 1) return combsetBinaryError("\\kEnergyDiff expects k >= 1", a[0], a[1]);
    long long result = valueIsEmptyCombSet(a[0]) ? 0 : kEnergyDiff((CombSet*)a[0].as.ptr, (int)k);
    valFree(a[0]);
    valFree(a[1]);
    return valInt(result);
}

static Value bi_energyMult(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsCombSet(a[0])) return combsetUnaryError("\\energyMult expects one CombSet", a[0]);
    long long result = valueIsEmptyCombSet(a[0]) ? 0 : multEnergy((CombSet*)a[0].as.ptr);
    valFree(a[0]);
    return valInt(result);
}

static Value bi_kEnergyMult(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    long long k;
    if (!valueIsCombSet(a[0]) || !valueToCombSetInt(a[1], &k)) {
        return combsetBinaryError("\\kEnergyMult expects a CombSet and an integer k", a[0], a[1]);
    }
    if (k < 1) return combsetBinaryError("\\kEnergyMult expects k >= 1", a[0], a[1]);
    long long result = valueIsEmptyCombSet(a[0]) ? 0 : kEnergyMult((CombSet*)a[0].as.ptr, (int)k);
    valFree(a[0]);
    valFree(a[1]);
    return valInt(result);
}

static Value bi_force(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (a[0].kind != VAL_STRING || !valueIsVector(a[1]) || !valueIsVector(a[2])) {
        valFree(a[0]);
        valFree(a[1]);
        valFree(a[2]);
        return valError("\\force expects a quoted name string, a vector, and a tail-position vector");
    }

    Force* force = constructForce(dupstr(a[0].as.str),
                                  copyMatrix((Matrix*)a[1].as.ptr),
                                  copyMatrix((Matrix*)a[2].as.ptr));
    valFree(a[0]);
    valFree(a[1]);
    valFree(a[2]);
    if (!force) return valError("\\force failed to construct force");
    return valPtr(VAL_FORCE, force);
}

static Value bi_body(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    double mass;
    if (!valueToBodyMass(a[0], &mass) || !valueIsVector(a[1]) || !valueIsVector(a[2])) {
        valFree(a[0]);
        valFree(a[1]);
        valFree(a[2]);
        return valError("\\body expects a mass, a position vector, and a velocity vector");
    }

    Body* body = constructBody(mass,
                               copyMatrix((Matrix*)a[1].as.ptr),
                               copyMatrix((Matrix*)a[2].as.ptr));
    valFree(a[0]);
    valFree(a[1]);
    valFree(a[2]);
    if (!body) return valError("\\body failed to construct body");
    return valPtr(VAL_BODY, body);
}

static Value bi_addForce(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (a[0].kind != VAL_BODY || a[1].kind != VAL_FORCE) {
        valFree(a[0]);
        valFree(a[1]);
        return valError("\\addForce expects a body and a force");
    }
    Body* body = (Body*)a[0].as.ptr;
    Force* force = (Force*)a[1].as.ptr;
    if (!body || !force) {
        valFree(a[0]);
        valFree(a[1]);
        return valError("\\addForce expects a valid body and force");
    }
    if (body->nForces >= MAX_FORCES) {
        valFree(a[0]);
        valFree(a[1]);
        return valError("\\addForce failed: body already has the maximum number of forces");
    }
    addForce(body, force);
    valFree(a[1]);
    return a[0];
}

static Value bi_removeForce(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (a[0].kind != VAL_BODY || a[1].kind != VAL_FORCE) {
        valFree(a[0]);
        valFree(a[1]);
        return valError("\\removeForce expects a body and a force");
    }
    Body* body = (Body*)a[0].as.ptr;
    Force* force = (Force*)a[1].as.ptr;
    if (!body || !force) {
        valFree(a[0]);
        valFree(a[1]);
        return valError("\\removeForce expects a valid body and force");
    }
    int found = 0;
    for (int i = 0; i < body->nForces; i++) {
        if (body->forces[i] == force) {
            found = 1;
            break;
        }
    }
    if (!found) {
        valFree(a[0]);
        valFree(a[1]);
        return valError("\\removeForce failed: force is not attached to the body");
    }
    removeForce(body, force);
    valFree(a[1]);
    return a[0];
}

static Value bi_displacement(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsVector(a[0]) || !valueIsVector(a[1])) {
        return vectorBinaryError("\\displacement expects two vectors", a[0], a[1]);
    }
    Vector* p1 = (Vector*)a[0].as.ptr;
    double* data = displacement(p1, (Vector*)a[1].as.ptr);
    Vector* out = vectorFromRealArray(data, p1->numRows);
    free(data);
    if (!out) return vectorBinaryError("\\displacement requires vectors of equal dimension", a[0], a[1]);
    valFree(a[0]);
    valFree(a[1]);
    return valPtr(VAL_VECTOR, out);
}

static Value bi_averageVelocity(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    double time;
    if (!valueIsVector(a[0]) || !valueIsVector(a[1]) || !valueToRealScalar(a[2], &time)) {
        valFree(a[0]); valFree(a[1]); valFree(a[2]);
        return valError("\\averageVelocity expects two vectors and a time");
    }
    Vector* p1 = (Vector*)a[0].as.ptr;
    double* data = averageVelocity(p1, (Vector*)a[1].as.ptr, time);
    Vector* out = vectorFromRealArray(data, p1->numRows);
    free(data);
    if (!out) {
        valFree(a[0]); valFree(a[1]); valFree(a[2]);
        return valError("\\averageVelocity requires vectors of equal dimension and time > 0");
    }
    valFree(a[0]); valFree(a[1]); valFree(a[2]);
    return valPtr(VAL_VECTOR, out);
}

static Value bi_averageAcceleration(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    double time;
    if (!valueIsVector(a[0]) || !valueIsVector(a[1]) || !valueIsVector(a[2]) || !valueToRealScalar(a[3], &time)) {
        valFree(a[0]); valFree(a[1]); valFree(a[2]); valFree(a[3]);
        return valError("\\averageAcceleration expects three vectors and a time");
    }
    Vector* p1 = (Vector*)a[0].as.ptr;
    double* data = averageAcceleration(p1, (Vector*)a[1].as.ptr, (Vector*)a[2].as.ptr, time);
    Vector* out = vectorFromRealArray(data, p1->numRows);
    free(data);
    if (!out) {
        valFree(a[0]); valFree(a[1]); valFree(a[2]); valFree(a[3]);
        return valError("\\averageAcceleration requires matching vector dimensions and time > 0");
    }
    valFree(a[0]); valFree(a[1]); valFree(a[2]); valFree(a[3]);
    return valPtr(VAL_VECTOR, out);
}

static Value bi_velocityAtTime(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    double time;
    if (!valueIsVector(a[0]) || !valueIsVector(a[1]) || !valueToRealScalar(a[2], &time)) {
        valFree(a[0]); valFree(a[1]); valFree(a[2]);
        return valError("\\velocityAtTime expects two vectors and a time");
    }
    Vector* out = velocityAtTime((Vector*)a[0].as.ptr, (Vector*)a[1].as.ptr, time);
    if (!out) {
        valFree(a[0]); valFree(a[1]); valFree(a[2]);
        return valError("\\velocityAtTime requires matching vector dimensions and time >= 0");
    }
    valFree(a[0]); valFree(a[1]); valFree(a[2]);
    return valPtr(VAL_VECTOR, out);
}

static Value bi_positionAtTime(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    double time;
    if (!valueIsVector(a[0]) || !valueIsVector(a[1]) || !valueIsVector(a[2]) || !valueToRealScalar(a[3], &time)) {
        valFree(a[0]); valFree(a[1]); valFree(a[2]); valFree(a[3]);
        return valError("\\positionAtTime expects three vectors and a time");
    }
    Vector* out = positionAtTime((Vector*)a[0].as.ptr, (Vector*)a[1].as.ptr, (Vector*)a[2].as.ptr, time);
    if (!out) {
        valFree(a[0]); valFree(a[1]); valFree(a[2]); valFree(a[3]);
        return valError("\\positionAtTime requires matching vector dimensions and time >= 0");
    }
    valFree(a[0]); valFree(a[1]); valFree(a[2]); valFree(a[3]);
    return valPtr(VAL_VECTOR, out);
}

static Value bi_speedAtPosition(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsVector(a[0]) || !valueIsVector(a[1]) || !valueIsVector(a[2]) || !valueIsVector(a[3])) {
        valFree(a[0]); valFree(a[1]); valFree(a[2]); valFree(a[3]);
        return valError("\\speedAtPosition expects four vectors");
    }
    Vector* out = speedAtPosition((Vector*)a[0].as.ptr, (Vector*)a[1].as.ptr,
                                  (Vector*)a[2].as.ptr, (Vector*)a[3].as.ptr);
    if (!out) {
        valFree(a[0]); valFree(a[1]); valFree(a[2]); valFree(a[3]);
        return valError("\\speedAtPosition requires matching vector dimensions and a nonnegative discriminant");
    }
    valFree(a[0]); valFree(a[1]); valFree(a[2]); valFree(a[3]);
    return valPtr(VAL_VECTOR, out);
}

static Value bi_velocityAtPosition(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsVector(a[0]) || !valueIsVector(a[1]) || !valueIsVector(a[2]) || !valueIsVector(a[3])) {
        valFree(a[0]); valFree(a[1]); valFree(a[2]); valFree(a[3]);
        return valError("\\velocityAtPosition expects four vectors");
    }
    Vector* out = velocityAtPosition((Vector*)a[0].as.ptr, (Vector*)a[1].as.ptr,
                                     (Vector*)a[2].as.ptr, (Vector*)a[3].as.ptr);
    if (!out) {
        valFree(a[0]); valFree(a[1]); valFree(a[2]); valFree(a[3]);
        return valError("\\velocityAtPosition requires matching vector dimensions and a reachable position");
    }
    valFree(a[0]); valFree(a[1]); valFree(a[2]); valFree(a[3]);
    return valPtr(VAL_VECTOR, out);
}

static Value bi_projectileInfo(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    double initVel, angle, initHeight;
    if (!valueToRealScalar(a[0], &initVel) || !valueToRealScalar(a[1], &angle) || !valueToRealScalar(a[2], &initHeight)) {
        valFree(a[0]); valFree(a[1]); valFree(a[2]);
        return valError("\\projectileInfo expects initial velocity, angle (in deg), and initial height");
    }
    ProjectileInfo* info = getProjectileInfo(initVel, angle, initHeight);
    valFree(a[0]); valFree(a[1]); valFree(a[2]);
    if (!info) return valError("\\projectileInfo requires initVel >= 0 and 0 <= angle <= 90");
    char buf[256];
    snprintf(buf, sizeof(buf), "(range: %g, peakHeight: %g, timeOfFlight: %g)",
             info->range, info->peakHeight, info->timeOfFlight);
    free(info);
    return valSymbol(buf);
}

static Value bi_centripetalAcceleration(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    double vel, radius;
    if (!valueToRealScalar(a[0], &vel) || !valueToRealScalar(a[1], &radius)) {
        valFree(a[0]); valFree(a[1]);
        return valError("\\centripetalAcceleration expects velocity and radius");
    }
    double result = centripetalAcceleration(vel, radius);
    valFree(a[0]); valFree(a[1]);
    if (isnan(result)) return valError("\\centripetalAcceleration requires velocity >= 0 and radius > 0");
    return valDecimal(result);
}

static Value bi_angularVelocity(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    double vel, radius;
    if (!valueToRealScalar(a[0], &vel) || !valueToRealScalar(a[1], &radius)) {
        valFree(a[0]); valFree(a[1]);
        return valError("\\angularVelocity expects velocity and radius");
    }
    double result = angularVelocity(vel, radius);
    valFree(a[0]); valFree(a[1]);
    if (isnan(result)) return valError("\\angularVelocity requires velocity >= 0 and radius > 0");
    return valDecimal(result);
}

static Value bi_netForce(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsBody(a[0])) {
        valFree(a[0]);
        return valError("\\netForce expects one body");
    }
    Vector* out = netForce((Body*)a[0].as.ptr);
    valFree(a[0]);
    if (!out) return valError("\\netForce failed");
    return valPtr(VAL_VECTOR, out);
}

static Value bi_accelerationFromForce(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsBody(a[0])) {
        valFree(a[0]);
        return valError("\\accelerationFromForce expects one body");
    }
    Vector* out = accelerationFromForce((Body*)a[0].as.ptr);
    valFree(a[0]);
    if (!out) return valError("\\accelerationFromForce failed");
    return valPtr(VAL_VECTOR, out);
}

static Value bi_gravityForce(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsBody(a[0])) {
        valFree(a[0]);
        return valError("\\gravityForce expects one body");
    }
    Force* out = gravityForce((Body*)a[0].as.ptr);
    valFree(a[0]);
    if (!out) return valError("\\gravityForce failed");
    return valPtr(VAL_FORCE, out);
}

static Value bi_normalForce(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsBody(a[0]) || !valueIsVector(a[1])) {
        valFree(a[0]); valFree(a[1]);
        return valError("\\normalForce expects a body and a surface-normal vector");
    }
    Force* out = normalForce((Body*)a[0].as.ptr, (Vector*)a[1].as.ptr);
    valFree(a[0]); valFree(a[1]);
    if (!out) return valError("\\normalForce failed");
    return valPtr(VAL_FORCE, out);
}

static Value bi_frictionForce(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    double mu;
    if (!valueIsForce(a[0]) || !valueToRealScalar(a[1], &mu) || !valueIsVector(a[2])) {
        valFree(a[0]); valFree(a[1]); valFree(a[2]);
        return valError("\\frictionForce expects a normal force, coefficient mu, and a direction vector");
    }
    Force* out = frictionForce((Force*)a[0].as.ptr, mu, (Vector*)a[2].as.ptr);
    valFree(a[0]); valFree(a[1]); valFree(a[2]);
    if (!out) return valError("\\frictionForce failed");
    return valPtr(VAL_FORCE, out);
}

static Value bi_momentum(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsBody(a[0])) {
        valFree(a[0]);
        return valError("\\momentum expects one body");
    }
    Vector* out = momentumVector((Body*)a[0].as.ptr);
    valFree(a[0]);
    if (!out) return valError("\\momentum failed");
    return valPtr(VAL_VECTOR, out);
}

static Value bi_kineticEnergy(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsBody(a[0])) {
        valFree(a[0]);
        return valError("\\kineticEnergy expects one body");
    }
    double result = kineticEnergy((Body*)a[0].as.ptr);
    valFree(a[0]);
    if (isnan(result)) return valError("\\kineticEnergy failed");
    return valDecimal(result);
}

static Value bi_gravPotentialEnergy(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    double height;
    if (!valueIsBody(a[0]) || !valueToRealScalar(a[1], &height)) {
        valFree(a[0]); valFree(a[1]);
        return valError("\\gravPotentialEnergy expects a body and a height");
    }
    double result = gravPotentialEnergy((Body*)a[0].as.ptr, height);
    valFree(a[0]); valFree(a[1]);
    if (isnan(result)) return valError("\\gravPotentialEnergy failed");
    return valDecimal(result);
}

static Value bi_springPotentialEnergy(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    double k, x;
    if (!valueToRealScalar(a[0], &k) || !valueToRealScalar(a[1], &x)) {
        valFree(a[0]); valFree(a[1]);
        return valError("\\springPotentialEnergy expects k and x");
    }
    double result = springPotentialEnergy(k, x);
    valFree(a[0]); valFree(a[1]);
    if (isnan(result)) return valError("\\springPotentialEnergy requires k >= 0 and x >= 0");
    return valDecimal(result);
}

static Value bi_work(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsForce(a[0]) || !valueIsVector(a[1])) {
        valFree(a[0]); valFree(a[1]);
        return valError("\\work expects a force and a displacement vector");
    }
    double result = work((Force*)a[0].as.ptr, (Vector*)a[1].as.ptr);
    valFree(a[0]); valFree(a[1]);
    if (isnan(result)) return valError("\\work requires matching vector dimensions");
    return valDecimal(result);
}

static Value bi_power_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsForce(a[0]) || !valueIsVector(a[1])) {
        valFree(a[0]); valFree(a[1]);
        return valError("\\power expects a force and a velocity vector");
    }
    double result = power((Force*)a[0].as.ptr, (Vector*)a[1].as.ptr);
    valFree(a[0]); valFree(a[1]);
    if (isnan(result)) return valError("\\power requires matching vector dimensions");
    return valDecimal(result);
}

static Value bi_impulse(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    double time;
    if (!valueIsForce(a[0]) || !valueToRealScalar(a[1], &time)) {
        valFree(a[0]); valFree(a[1]);
        return valError("\\impulse expects a force and a time");
    }
    Vector* out = impulseVector((Force*)a[0].as.ptr, time);
    valFree(a[0]); valFree(a[1]);
    if (!out) return valError("\\impulse failed");
    return valPtr(VAL_VECTOR, out);
}

static Value bi_centerOfMass(EvalContext* c, Value* a, size_t n) {
    (void)c;
    Body** bodies = NULL;
    size_t count = 0;
    if (!unpackBodyList(a, n, &bodies, &count) || count == 0) {
        for (size_t i = 0; i < n; i++) valFree(a[i]);
        free(bodies);
        return valError("\\centerOfMass expects one or more bodies");
    }
    Vector* out = centerOfMass(bodies, (int)count);
    free(bodies);
    for (size_t i = 0; i < n; i++) valFree(a[i]);
    if (!out) return valError("\\centerOfMass failed");
    return valPtr(VAL_VECTOR, out);
}

static Value bi_centerOfMassVelocity(EvalContext* c, Value* a, size_t n) {
    (void)c;
    Body** bodies = NULL;
    size_t count = 0;
    if (!unpackBodyList(a, n, &bodies, &count) || count == 0) {
        for (size_t i = 0; i < n; i++) valFree(a[i]);
        free(bodies);
        return valError("\\centerOfMassVelocity expects one or more bodies");
    }
    Vector* out = centerOfMassVelocity(bodies, (int)count);
    free(bodies);
    for (size_t i = 0; i < n; i++) valFree(a[i]);
    if (!out) return valError("\\centerOfMassVelocity failed");
    return valPtr(VAL_VECTOR, out);
}

static Value bi_elasticCollision(EvalContext* c, Value* a, size_t n) {
    (void)c;
    Value lhs, rhs;
    if (!unpackBodyPair(a, n, &lhs, &rhs) || !valueIsBody(lhs) || !valueIsBody(rhs)) {
        for (size_t i = 0; i < n; i++) valFree(a[i]);
        return valError("\\elasticCollision expects two bodies");
    }
    elasticCollision1D((Body*)lhs.as.ptr, (Body*)rhs.as.ptr);
    Value* items = calloc(2, sizeof(Value));
    items[0] = lhs;
    items[1] = rhs;
    return valList(items, 2);
}

static Value bi_inelasticCollision(EvalContext* c, Value* a, size_t n) {
    (void)c;
    Value lhs, rhs;
    if (!unpackBodyPair(a, n, &lhs, &rhs) || !valueIsBody(lhs) || !valueIsBody(rhs)) {
        for (size_t i = 0; i < n; i++) valFree(a[i]);
        return valError("\\inelasticCollision expects two bodies");
    }
    inelasticCollision1D((Body*)lhs.as.ptr, (Body*)rhs.as.ptr);
    Value* items = calloc(2, sizeof(Value));
    items[0] = lhs;
    items[1] = rhs;
    return valList(items, 2);
}

static Value bi_moiPoint(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    double m, r;
    if (!valueToRealScalar(a[0], &m) || !valueToRealScalar(a[1], &r)) {
        valFree(a[0]); valFree(a[1]);
        return valError("\\momentOfInertiaPoint expects mass and radius");
    }
    double result = momentOfInertiaPoint(m, r);
    valFree(a[0]); valFree(a[1]);
    if (isnan(result)) return valError("\\momentOfInertiaPoint requires m >= 0 and r >= 0");
    return valDecimal(result);
}

static Value bi_moiRod(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    double m, L;
    if (!valueToRealScalar(a[0], &m) || !valueToRealScalar(a[1], &L)) {
        valFree(a[0]); valFree(a[1]);
        return valError("\\momentOfInertiaRod expects mass and length");
    }
    double result = momentOfInertiaRod(m, L);
    valFree(a[0]); valFree(a[1]);
    if (isnan(result)) return valError("\\momentOfInertiaRod requires m >= 0 and L >= 0");
    return valDecimal(result);
}

static Value bi_moiDisk(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    double m, r;
    if (!valueToRealScalar(a[0], &m) || !valueToRealScalar(a[1], &r)) {
        valFree(a[0]); valFree(a[1]);
        return valError("\\momentOfInertiaDisk expects mass and radius");
    }
    double result = momentOfInertiaDisk(m, r);
    valFree(a[0]); valFree(a[1]);
    if (isnan(result)) return valError("\\momentOfInertiaDisk requires m >= 0 and R >= 0");
    return valDecimal(result);
}

static Value bi_parallelAxis(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    double Icm, m, d;
    if (!valueToRealScalar(a[0], &Icm) || !valueToRealScalar(a[1], &m) || !valueToRealScalar(a[2], &d)) {
        valFree(a[0]); valFree(a[1]); valFree(a[2]);
        return valError("\\parallelAxis expects I_cm, mass, and displacement");
    }
    double result = parallelAxisTheorem(Icm, m, d);
    valFree(a[0]); valFree(a[1]); valFree(a[2]);
    if (isnan(result)) return valError("\\parallelAxis requires I_cm >= 0 and m >= 0");
    return valDecimal(result);
}

static Value bi_torque(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsForce(a[0]) || !valueIsVector(a[1])) {
        valFree(a[0]); valFree(a[1]);
        return valError("\\torque expects a force and a pivot vector");
    }
    Vector* out = torque((Force*)a[0].as.ptr, (Vector*)a[1].as.ptr);
    valFree(a[0]); valFree(a[1]);
    if (!out) return valError("\\torque failed");
    return valPtr(VAL_VECTOR, out);
}

static Value bi_angularMomentumCmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsBody(a[0]) || !valueIsVector(a[1])) {
        valFree(a[0]); valFree(a[1]);
        return valError("\\angularMomentum expects a body and a pivot vector");
    }
    Vector* out = angularMomentum((Body*)a[0].as.ptr, (Vector*)a[1].as.ptr);
    valFree(a[0]); valFree(a[1]);
    if (!out) return valError("\\angularMomentum failed");
    return valPtr(VAL_VECTOR, out);
}

static Value bi_rotationalKineticEnergy(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    double I, omega;
    if (!valueToRealScalar(a[0], &I) || !valueToRealScalar(a[1], &omega)) {
        valFree(a[0]); valFree(a[1]);
        return valError("\\rotationalKineticEnergy expects moment of inertia and angular velocity");
    }
    double result = rotationalKineticEnergy(I, omega);
    valFree(a[0]); valFree(a[1]);
    if (isnan(result)) return valError("\\rotationalKineticEnergy requires I >= 0");
    return valDecimal(result);
}

static Value bi_angularAccelerationFromTorque(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    double netTorqueValue, I;
    if (!valueToRealScalar(a[0], &netTorqueValue) || !valueToRealScalar(a[1], &I)) {
        valFree(a[0]); valFree(a[1]);
        return valError("\\angularAccelerationFromTorque expects net torque and moment of inertia");
    }
    double result = angularAccelerationFromTorque(netTorqueValue, I);
    valFree(a[0]); valFree(a[1]);
    if (isnan(result)) return valError("\\angularAccelerationFromTorque requires I > 0");
    return valDecimal(result);
}

static Value bi_ZnGroup(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    int modulus;
    if (!valueToUsagiInt(a[0], &modulus)) {
        valFree(a[0]);
        return valError("\\ZnGroup expects one integer modulus");
    }
    Group* out = constructZnGroup(modulus);
    valFree(a[0]);
    if (!out) return valError("\\ZnGroup requires modulus >= 1");
    return valPtr(VAL_GROUP, out);
}

static Value bi_ZnProductGroup(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    int* vals = NULL;
    int count = 0;
    if (!valueToUsagiIntVector(a[0], &vals, &count)) {
        valFree(a[0]);
        return valError("\\ZnProductGroup expects one vector of integer moduli");
    }
    Group* out = constructZnProductGroup(vals, count);
    free(vals);
    valFree(a[0]);
    if (!out) return valError("\\ZnProductGroup requires a nonempty vector of moduli >= 1");
    return valPtr(VAL_GROUP, out);
}

static Value bi_SymmetricGroup(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    int degree;
    if (!valueToUsagiInt(a[0], &degree)) {
        valFree(a[0]);
        return valError("\\SymmetricGroup expects one integer degree");
    }
    Group* out = constructSymmetricGroup(degree);
    valFree(a[0]);
    if (!out) return valError("\\SymmetricGroup requires degree >= 1");
    return valPtr(VAL_GROUP, out);
}

static Value bi_AlternatingGroup(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    int degree;
    if (!valueToUsagiInt(a[0], &degree)) {
        valFree(a[0]);
        return valError("\\AlternatingGroup expects one integer degree");
    }
    Group* out = constructAlternatingGroup(degree);
    valFree(a[0]);
    if (!out) return valError("\\AlternatingGroup requires degree >= 1");
    return valPtr(VAL_GROUP, out);
}

static Value bi_DihedralGroup(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    int degree;
    if (!valueToUsagiInt(a[0], &degree)) {
        valFree(a[0]);
        return valError("\\dihedralGroup expects one integer degree");
    }
    Group* out = constructDihedralGroup(degree);
    valFree(a[0]);
    if (!out) return valError("\\dihedralGroup requires degree >= 1");
    return valPtr(VAL_GROUP, out);
}

static Value bi_ZnRing(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    int modulus;
    if (!valueToUsagiInt(a[0], &modulus)) {
        valFree(a[0]);
        return valError("\\ZnRing expects one integer modulus");
    }
    Ring* out = constructZnRing(modulus);
    valFree(a[0]);
    if (!out) return valError("\\ZnRing requires modulus >= 1");
    return valPtr(VAL_RING, out);
}

static Value bi_ZnProductRing(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    int* vals = NULL;
    int count = 0;
    if (!valueToUsagiIntVector(a[0], &vals, &count)) {
        valFree(a[0]);
        return valError("\\ZnProductRing expects one vector of integer moduli");
    }
    Ring* out = constructZnProductRing(vals, count);
    free(vals);
    valFree(a[0]);
    if (!out) return valError("\\ZnProductRing requires a nonempty vector of moduli >= 1");
    return valPtr(VAL_RING, out);
}

static Value bi_primeFiniteField(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    int p;
    if (!valueToUsagiInt(a[0], &p)) {
        valFree(a[0]);
        return valError("\\primeFiniteField expects one integer prime");
    }
    Ring* out = primeFiniteField(p);
    valFree(a[0]);
    if (!out) return valError("\\primeFiniteField requires a prime p >= 2");
    return valPtr(VAL_RING, out);
}

static Value bi_finiteField(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    int p, k;
    if (!valueToUsagiInt(a[0], &p) || !valueToUsagiInt(a[1], &k)) {
        valFree(a[0]); valFree(a[1]);
        return valError("\\finiteField expects a prime p and a nonnegative integer k");
    }
    Ring* out = constructFiniteField(p, k);
    valFree(a[0]); valFree(a[1]);
    if (!out) return valError("\\finiteField requires a prime p >= 2 and k >= 0");
    return valPtr(VAL_RING, out);
}

static Value bi_quaternionGroup(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)a; (void)n;
    Group* out = constructQ8();
    if (!out) return valError("\\quaternionGroup failed");
    return valPtr(VAL_GROUP, out);
}

static Value bi_isPrime_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    int p;
    if (!valueToUsagiInt(a[0], &p)) {
        valFree(a[0]);
        return valError("\\isPrime expects one integer");
    }
    valFree(a[0]);
    return valBool(isPrime(p));
}

static Value bi_factorial_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    int input;
    if (!valueToUsagiInt(a[0], &input)) {
        valFree(a[0]);
        return valError("\\factorial expects one integer");
    }
    valFree(a[0]);
    if (input < 0) return valError("\\factorial requires n >= 0");
    return valInt(factorial(input));
}

static Value bi_listElements(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsGroup(a[0]) && !valueIsRing(a[0])) {
        valFree(a[0]);
        return valError("\\listElements expects one group or ring");
    }
    int count = 0;
    ValueKind elementKind;
    if (valueIsGroup(a[0])) {
        Group* group = (Group*)a[0].as.ptr;
        if (!group || group->card < 0 || !group->elements) {
            valFree(a[0]);
            return valError("\\listElements failed");
        }
        count = group->card;
        elementKind = VAL_GROUP_ELEMENT;
    } else {
        Ring* ring = (Ring*)a[0].as.ptr;
        if (!ring || ring->card < 0 || !ring->elements) {
            valFree(a[0]);
            return valError("\\listElements failed");
        }
        count = ring->card;
        elementKind = VAL_RING_ELEMENT;
    }
    Value* items = calloc((size_t)count, sizeof(Value));
    if (!items) {
        valFree(a[0]);
        return valError("\\listElements failed to allocate output");
    }
    if (valueIsGroup(a[0])) {
        Group* group = (Group*)a[0].as.ptr;
        for (int i = 0; i < count; i++) items[i] = valPtr(elementKind, group->elements[i]);
    } else {
        Ring* ring = (Ring*)a[0].as.ptr;
        for (int i = 0; i < count; i++) items[i] = valPtr(elementKind, ring->elements[i]);
    }
    valFree(a[0]);
    return valList(items, (size_t)count);
}

static Value bi_getElement(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    const char* repr = NULL;
    if ((!valueIsGroup(a[0]) && !valueIsRing(a[0])) || !valueToStringLiteral(a[1], &repr)) {
        valFree(a[0]); valFree(a[1]);
        return valError("\\getElement expects a group or ring and a quoted repr string");
    }
    if (valueIsGroup(a[0])) {
        Group* group = (Group*)a[0].as.ptr;
        if (!group || !group->elements) {
            valFree(a[0]); valFree(a[1]);
            return valError("\\getElement failed");
        }
        GroupElement* match = NULL;
        for (int i = 0; i < group->card; i++) {
            GroupElement* element = group->elements[i];
            if (element && element->repr && strcmp(element->repr, repr) == 0) {
                match = element;
                break;
            }
        }
        valFree(a[0]); valFree(a[1]);
        if (!match) return valError("\\getElement could not find that repr in the structure");
        return valPtr(VAL_GROUP_ELEMENT, match);
    }

    Ring* ring = (Ring*)a[0].as.ptr;
    if (!ring || !ring->elements) {
        valFree(a[0]); valFree(a[1]);
        return valError("\\getElement failed");
    }
    RingElement* match = NULL;
    for (int i = 0; i < ring->card; i++) {
        RingElement* element = ring->elements[i];
        if (element && element->repr && strcmp(element->repr, repr) == 0) {
            match = element;
            break;
        }
    }
    valFree(a[0]); valFree(a[1]);
    if (!match) return valError("\\getElement could not find that repr in the structure");
    return valPtr(VAL_RING_ELEMENT, match);
}

static Value bi_groupElementConjugate_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsGroupElement(a[0]) || !valueIsGroupElement(a[1])) {
        return vectorBinaryError("\\groupElementConjugate expects two group elements", a[0], a[1]);
    }
    GroupElement* out = groupElementConjugate((GroupElement*)a[0].as.ptr, (GroupElement*)a[1].as.ptr);
    if (!out) return vectorBinaryError("\\groupElementConjugate requires two elements from the same group", a[0], a[1]);
    valFree(a[0]);
    valFree(a[1]);
    return valPtr(VAL_GROUP_ELEMENT, out);
}

static Value bi_groupCommutator_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsGroupElement(a[0]) || !valueIsGroupElement(a[1])) {
        return vectorBinaryError("\\groupCommutator expects two group elements", a[0], a[1]);
    }
    GroupElement* out = groupCommutator((GroupElement*)a[0].as.ptr, (GroupElement*)a[1].as.ptr);
    if (!out) return vectorBinaryError("\\groupCommutator requires two elements from the same group", a[0], a[1]);
    valFree(a[0]);
    valFree(a[1]);
    return valPtr(VAL_GROUP_ELEMENT, out);
}

static Value bi_elementOrderGroup(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsGroupElement(a[0])) {
        valFree(a[0]);
        return valError("\\elementOrderGroup expects one group element");
    }
    GroupElement* element = (GroupElement*)a[0].as.ptr;
    int order = elementOrder(element ? element->group : NULL, element);
    valFree(a[0]);
    if (order < 0) return valError("\\elementOrderGroup failed");
    return valInt(order);
}

static Value bi_additiveOrder_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsRingElement(a[0])) {
        valFree(a[0]);
        return valError("\\additiveOrder expects one ring element");
    }
    RingElement* element = (RingElement*)a[0].as.ptr;
    int order = additiveOrder(element ? element->ring : NULL, element);
    valFree(a[0]);
    if (order < 0) return valError("\\additiveOrder failed");
    return valInt(order);
}

static Value bi_multiplicativeOrder_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsRingElement(a[0])) {
        valFree(a[0]);
        return valError("\\multiplicativeOrder expects one ring element");
    }
    RingElement* element = (RingElement*)a[0].as.ptr;
    int order = multiplicativeOrder(element ? element->ring : NULL, element);
    valFree(a[0]);
    if (order < 0) return valError("\\multiplicativeOrder failed");
    return valInt(order);
}

static Value bi_isInGroup_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsGroup(a[0]) || !valueIsGroupElement(a[1])) {
        return vectorBinaryError("\\isInGroup expects a group and a group element", a[0], a[1]);
    }
    bool result = isInGroup((Group*)a[0].as.ptr, (GroupElement*)a[1].as.ptr);
    valFree(a[0]);
    valFree(a[1]);
    return valBool(result);
}

static Value bi_isGroupIdentity_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsGroup(a[0]) || !valueIsGroupElement(a[1])) {
        return vectorBinaryError("\\isGroupIdentity expects a group and a group element", a[0], a[1]);
    }
    bool result = isGroupIdentity((Group*)a[0].as.ptr, (GroupElement*)a[1].as.ptr);
    valFree(a[0]);
    valFree(a[1]);
    return valBool(result);
}

static Value bi_groupIdentity_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsGroup(a[0])) {
        valFree(a[0]);
        return valError("\\groupIdentity expects one group");
    }
    GroupElement* out = groupIdentity((Group*)a[0].as.ptr);
    valFree(a[0]);
    if (!out) return valError("\\groupIdentity failed");
    return valPtr(VAL_GROUP_ELEMENT, out);
}

static Value bi_isAddIdentity_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsRing(a[0]) || !valueIsRingElement(a[1])) {
        return vectorBinaryError("\\isAddIdentity expects a ring and a ring element", a[0], a[1]);
    }
    bool result = isRingAddIdentity((Ring*)a[0].as.ptr, (RingElement*)a[1].as.ptr);
    valFree(a[0]);
    valFree(a[1]);
    return valBool(result);
}

static Value bi_isMultIdentity_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsRing(a[0]) || !valueIsRingElement(a[1])) {
        return vectorBinaryError("\\isMultIdentity expects a ring and a ring element", a[0], a[1]);
    }
    bool result = isRingMultIdentity((Ring*)a[0].as.ptr, (RingElement*)a[1].as.ptr);
    valFree(a[0]);
    valFree(a[1]);
    return valBool(result);
}

static Value bi_hasMultIdentity_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsRing(a[0])) {
        valFree(a[0]);
        return valError("\\hasMultIdentity expects one ring");
    }
    bool result = hasMultIdentity((Ring*)a[0].as.ptr);
    valFree(a[0]);
    return valBool(result);
}

static Value bi_elementsCommute_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsGroupElement(a[0]) || !valueIsGroupElement(a[1])) {
        return vectorBinaryError("\\elementsCommute expects two group elements", a[0], a[1]);
    }
    bool result = elementsCommute((GroupElement*)a[0].as.ptr, (GroupElement*)a[1].as.ptr);
    valFree(a[0]);
    valFree(a[1]);
    return valBool(result);
}

static Value bi_isTrivialGroup_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsGroup(a[0])) {
        valFree(a[0]);
        return valError("\\isTrivialGroup expects one group");
    }
    bool result = isTrivialGroup((Group*)a[0].as.ptr);
    valFree(a[0]);
    return valBool(result);
}

static Value bi_isTrivialRing_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsRing(a[0])) {
        valFree(a[0]);
        return valError("\\isTrivialRing expects one ring");
    }
    bool result = isTrivialRing((Ring*)a[0].as.ptr);
    valFree(a[0]);
    return valBool(result);
}

static Value bi_trivialGroup_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)a; (void)n;
    Group* out = trivialGroup();
    if (!out) return valError("\\trivialGroup failed");
    return valPtr(VAL_GROUP, out);
}

static Value bi_trivialRing_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)a; (void)n;
    Ring* out = trivialRing();
    if (!out) return valError("\\trivialRing failed");
    return valPtr(VAL_RING, out);
}

static Value bi_groupElementInfo_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsGroupElement(a[0])) {
        valFree(a[0]);
        return valError("\\groupElementInfo expects one group element");
    }
    GroupElement* element = (GroupElement*)a[0].as.ptr;
    char buf[1024] = {0};
    if (!element) {
        valFree(a[0]);
        return valError("\\groupElementInfo failed");
    }
    snprintf(buf, sizeof(buf), "<groupElement repr=%s; order=", element->repr ? element->repr : "?");
    appendOptionalOrder(buf, sizeof(buf), elementOrder(element->group, element));
    size_t used = strlen(buf);
    if (used < sizeof(buf)) snprintf(buf + used, sizeof(buf) - used, "; group=");
    appendInlineGroupSummary(buf, sizeof(buf), element->group);
    used = strlen(buf);
    if (used < sizeof(buf)) snprintf(buf + used, sizeof(buf) - used, ">");
    valFree(a[0]);
    return valSymbol(buf);
}

static Value bi_ringElementInfo_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsRingElement(a[0])) {
        valFree(a[0]);
        return valError("\\ringElementInfo expects one ring element");
    }
    RingElement* element = (RingElement*)a[0].as.ptr;
    char buf[1024] = {0};
    if (!element) {
        valFree(a[0]);
        return valError("\\ringElementInfo failed");
    }
    snprintf(buf, sizeof(buf), "<ringElement repr=%s; additiveOrder=", element->repr ? element->repr : "?");
    appendOptionalOrder(buf, sizeof(buf), additiveOrder(element->ring, element));
    size_t used = strlen(buf);
    if (used < sizeof(buf)) snprintf(buf + used, sizeof(buf) - used, "; multiplicativeOrder=");
    appendOptionalOrder(buf, sizeof(buf), multiplicativeOrder(element->ring, element));
    used = strlen(buf);
    if (used < sizeof(buf)) snprintf(buf + used, sizeof(buf) - used, "; ring=");
    appendInlineRingSummary(buf, sizeof(buf), element->ring);
    used = strlen(buf);
    if (used < sizeof(buf)) snprintf(buf + used, sizeof(buf) - used, ">");
    valFree(a[0]);
    return valSymbol(buf);
}

static Value bi_isCommutativeGroup_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsGroup(a[0])) {
        valFree(a[0]);
        return valError("\\isCommutativeGroup expects one group");
    }
    bool result = isCommutativeGroup((Group*)a[0].as.ptr);
    valFree(a[0]);
    return valBool(result);
}

static Value bi_isCommutativeRing_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsRing(a[0])) {
        valFree(a[0]);
        return valError("\\isCommutativeRing expects one ring");
    }
    bool result = isCommutativeRing((Ring*)a[0].as.ptr);
    valFree(a[0]);
    return valBool(result);
}

static Value bi_isSimple_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsGroup(a[0])) {
        valFree(a[0]);
        return valError("\\isSimple expects one group");
    }
    bool result = isSimple((Group*)a[0].as.ptr);
    valFree(a[0]);
    return valBool(result);
}

static Value bi_isInverse_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsGroupElement(a[0]) || !valueIsGroupElement(a[1])) {
        return vectorBinaryError("\\isInverse expects two group elements", a[0], a[1]);
    }
    bool result = isInverse((GroupElement*)a[0].as.ptr, (GroupElement*)a[1].as.ptr);
    valFree(a[0]);
    valFree(a[1]);
    return valBool(result);
}

static Value bi_isAddInverse_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsRingElement(a[0]) || !valueIsRingElement(a[1])) {
        return vectorBinaryError("\\isAddInverse expects two ring elements", a[0], a[1]);
    }
    bool result = isAddInverse((RingElement*)a[0].as.ptr, (RingElement*)a[1].as.ptr);
    valFree(a[0]);
    valFree(a[1]);
    return valBool(result);
}

static Value bi_isMultInverse_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsRingElement(a[0]) || !valueIsRingElement(a[1])) {
        return vectorBinaryError("\\isMultInverse expects two ring elements", a[0], a[1]);
    }
    bool result = isMultInverse((RingElement*)a[0].as.ptr, (RingElement*)a[1].as.ptr);
    valFree(a[0]);
    valFree(a[1]);
    return valBool(result);
}

static Value bi_hasMultInverse_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsRingElement(a[0])) {
        valFree(a[0]);
        return valError("\\hasMultInverse expects one ring element");
    }
    bool result = hasMultInverse((RingElement*)a[0].as.ptr);
    valFree(a[0]);
    return valBool(result);
}

static Value bi_isZeroDivisor_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsRingElement(a[0])) {
        valFree(a[0]);
        return valError("\\isZeroDivisor expects one ring element");
    }
    bool result = isZeroDivisor((RingElement*)a[0].as.ptr);
    valFree(a[0]);
    return valBool(result);
}

static Value bi_hasZeroDivisors_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsRing(a[0])) {
        valFree(a[0]);
        return valError("\\hasZeroDivisors expects one ring");
    }
    bool result = hasZeroDivisors((Ring*)a[0].as.ptr);
    valFree(a[0]);
    return valBool(result);
}

static Value bi_isIntegralDomain_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsRing(a[0])) {
        valFree(a[0]);
        return valError("\\isIntegralDomain expects one ring");
    }
    bool result = isIntegralDomain((Ring*)a[0].as.ptr);
    valFree(a[0]);
    return valBool(result);
}

static Value bi_isDivisionRing_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsRing(a[0])) {
        valFree(a[0]);
        return valError("\\isDivisionRing expects one ring");
    }
    bool result = isDivisionRing((Ring*)a[0].as.ptr);
    valFree(a[0]);
    return valBool(result);
}

static Value bi_isField_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsRing(a[0])) {
        valFree(a[0]);
        return valError("\\isField expects one ring");
    }
    bool result = isField((Ring*)a[0].as.ptr);
    valFree(a[0]);
    return valBool(result);
}

static Value bi_listSubgroups_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsGroup(a[0])) {
        valFree(a[0]);
        return valError("\\listSubgroups expects one group");
    }
    Group* group = (Group*)a[0].as.ptr;
    int count = 0;
    SubGroup** subgroups = listAllSubgroups(group, &count);
    if (!subgroups) {
        valFree(a[0]);
        return valError("\\listSubgroups failed");
    }
    char* buf = calloc(8192, 1);
    if (!buf) {
        for (int i = 0; i < count; i++) freeSubgroup(subgroups[i]);
        free(subgroups);
        valFree(a[0]);
        return valError("\\listSubgroups failed to allocate output");
    }
    appendInlineGroupSummary(buf, 8192, group);
    size_t used = strlen(buf);
    if (used < 8192) snprintf(buf + used, 8192 - used, ": ");
    for (int i = 0; i < count; i++) {
        used = strlen(buf);
        if (used < 8192) snprintf(buf + used, 8192 - used, "%sH%d(card=%d, elements=", i ? "; " : "", i + 1, subgroups[i]->card);
        appendInlineSubgroupElements(buf, 8192, subgroups[i]);
        used = strlen(buf);
        if (used < 8192) snprintf(buf + used, 8192 - used, ")");
    }
    for (int i = 0; i < count; i++) freeSubgroup(subgroups[i]);
    free(subgroups);
    valFree(a[0]);
    Value out = valSymbol(buf);
    free(buf);
    return out;
}

static Value bi_getSubgroup_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    int idx;
    if (!valueIsGroup(a[0]) || !valueToUsagiInt(a[1], &idx)) {
        valFree(a[0]); valFree(a[1]);
        return valError("\\getSubgroup expects a group and a 1-based integer index");
    }
    if (idx < 1) {
        valFree(a[0]); valFree(a[1]);
        return valError("\\getSubgroup expects an index >= 1");
    }
    Group* group = (Group*)a[0].as.ptr;
    int count = 0;
    SubGroup** subgroups = listAllSubgroups(group, &count);
    if (!subgroups) {
        valFree(a[0]); valFree(a[1]);
        return valError("\\getSubgroup failed");
    }
    if (idx > count) {
        for (int i = 0; i < count; i++) freeSubgroup(subgroups[i]);
        free(subgroups);
        valFree(a[0]); valFree(a[1]);
        return valError("\\getSubgroup index out of range");
    }
    SubGroup* chosen = subgroups[idx - 1];
    for (int i = 0; i < count; i++) {
        if (i != idx - 1) freeSubgroup(subgroups[i]);
    }
    free(subgroups);
    valFree(a[0]); valFree(a[1]);
    return valPtr(VAL_SUBGROUP, chosen);
}

static Value bi_subgroupInfo_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsSubgroup(a[0])) {
        valFree(a[0]);
        return valError("\\subgroupInfo expects one subgroup");
    }
    SubGroup* subgroup = (SubGroup*)a[0].as.ptr;
    if (!subgroup) {
        valFree(a[0]);
        return valError("\\subgroupInfo failed");
    }
    char buf[4096] = {0};
    appendInlineSubgroupSummary(buf, sizeof(buf), subgroup);
    size_t used = strlen(buf);
    if (used < sizeof(buf)) snprintf(buf + used, sizeof(buf) - used, "; ambient=");
    appendInlineGroupSummary(buf, sizeof(buf), subgroup->ambient);
    valFree(a[0]);
    return valSymbol(buf);
}

static Value bi_isSubgroup_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsGroup(a[0]) || !valueIsSubgroup(a[1])) {
        valFree(a[0]);
        valFree(a[1]);
        return valError("\\isSubgroup expects a group and a subgroup");
    }
    bool result = isSubgroup((Group*)a[0].as.ptr, (SubGroup*)a[1].as.ptr);
    valFree(a[0]);
    valFree(a[1]);
    return valBool(result);
}

static Value bi_isNormalSubgroup_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsGroup(a[0]) || !valueIsSubgroup(a[1])) {
        valFree(a[0]);
        valFree(a[1]);
        return valError("\\isNormalSubgroup expects a group and a subgroup");
    }
    bool result = isNormalSubgroup((Group*)a[0].as.ptr, (SubGroup*)a[1].as.ptr);
    valFree(a[0]);
    valFree(a[1]);
    return valBool(result);
}

static Value bi_trivialSubgroup_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)a; (void)n;
    SubGroup* out = trivialSubgroup();
    if (!out) return valError("\\trivialSubgroup failed");
    return valPtr(VAL_SUBGROUP, out);
}

static Value bi_isTrivialSubgroup_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsSubgroup(a[0])) {
        valFree(a[0]);
        return valError("\\isTrivialSubgroup expects one subgroup");
    }
    bool result = isTrivialSubgroup((SubGroup*)a[0].as.ptr);
    valFree(a[0]);
    return valBool(result);
}

static Value bi_groupCenter_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsGroup(a[0])) {
        valFree(a[0]);
        return valError("\\groupCenter expects one group");
    }
    SubGroup* out = groupCenter((Group*)a[0].as.ptr);
    valFree(a[0]);
    if (!out) return valError("\\groupCenter failed");
    return valPtr(VAL_SUBGROUP, out);
}

static Value bi_groupCentralizer_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsGroup(a[0]) || !valueIsGroupElement(a[1])) {
        valFree(a[0]);
        valFree(a[1]);
        return valError("\\groupCentralizer expects a group and a group element");
    }
    SubGroup* out = groupCentralizer((Group*)a[0].as.ptr, (GroupElement*)a[1].as.ptr);
    valFree(a[0]);
    valFree(a[1]);
    if (!out) return valError("\\groupCentralizer failed");
    return valPtr(VAL_SUBGROUP, out);
}

static Value bi_groupNormalizer_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsSubgroup(a[0])) {
        valFree(a[0]);
        return valError("\\groupNormalizer expects one subgroup");
    }
    SubGroup* out = groupNormalizer((SubGroup*)a[0].as.ptr);
    valFree(a[0]);
    if (!out) return valError("\\groupNormalizer failed");
    return valPtr(VAL_SUBGROUP, out);
}

static Value bi_conjugacyClass_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsGroupElement(a[0])) {
        valFree(a[0]);
        return valError("\\conjugacyClass expects one group element");
    }
    GroupElement* element = (GroupElement*)a[0].as.ptr;
    int count = 0;
    int* indices = conjugacyClass(element, &count);
    if (!indices) {
        valFree(a[0]);
        return valError("\\conjugacyClass failed");
    }
    char buf[2048] = {0};
    snprintf(buf, sizeof(buf), "<conjugacyClass of %s in ", (element && element->repr) ? element->repr : "?");
    appendInlineGroupSummary(buf, sizeof(buf), element ? element->group : NULL);
    size_t used = strlen(buf);
    if (used < sizeof(buf)) snprintf(buf + used, sizeof(buf) - used, "; elements=");
    appendGroupElementIndexList(buf, sizeof(buf), element ? element->group : NULL, indices, count);
    used = strlen(buf);
    if (used < sizeof(buf)) snprintf(buf + used, sizeof(buf) - used, ">");
    free(indices);
    valFree(a[0]);
    return valSymbol(buf);
}

static Value bi_normalClosure_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsSubgroup(a[0])) {
        valFree(a[0]);
        return valError("\\normalClosure expects one subgroup");
    }
    SubGroup* out = normalClosure((SubGroup*)a[0].as.ptr);
    valFree(a[0]);
    if (!out) return valError("\\normalClosure failed");
    return valPtr(VAL_SUBGROUP, out);
}

static Value bi_isInSubgroup_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsSubgroup(a[0]) || !valueIsGroupElement(a[1])) {
        valFree(a[0]);
        valFree(a[1]);
        return valError("\\isInSubgroup expects a subgroup and a group element");
    }
    bool result = isInSubgroup((SubGroup*)a[0].as.ptr, (GroupElement*)a[1].as.ptr);
    valFree(a[0]);
    valFree(a[1]);
    return valBool(result);
}

static Value bi_subgroupContains_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsSubgroup(a[0]) || !valueIsSubgroup(a[1])) {
        valFree(a[0]);
        valFree(a[1]);
        return valError("\\subgroupContains expects two subgroups");
    }
    bool result = subgroupContains((SubGroup*)a[0].as.ptr, (SubGroup*)a[1].as.ptr);
    valFree(a[0]);
    valFree(a[1]);
    return valBool(result);
}

static Value bi_subgroupIndex_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsSubgroup(a[0])) {
        valFree(a[0]);
        return valError("\\subgroupIndex expects one subgroup");
    }
    int idx = subgroupIndex((SubGroup*)a[0].as.ptr);
    valFree(a[0]);
    if (idx < 0) return valError("\\subgroupIndex failed");
    return valInt(idx);
}

static Value bi_subgroupConjugate_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsGroupElement(a[0]) || !valueIsSubgroup(a[1])) {
        valFree(a[0]);
        valFree(a[1]);
        return valError("\\subgroupConjugate expects a group element and a subgroup");
    }
    SubGroup* out = subgroupConjugate((GroupElement*)a[0].as.ptr, (SubGroup*)a[1].as.ptr);
    valFree(a[0]);
    valFree(a[1]);
    if (!out) return valError("\\subgroupConjugate failed");
    return valPtr(VAL_SUBGROUP, out);
}

static Value bi_commutatorSubgroup_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsGroup(a[0])) {
        valFree(a[0]);
        return valError("\\commutatorSubgroup expects one group");
    }
    SubGroup* out = commutatorSubgroup((Group*)a[0].as.ptr);
    valFree(a[0]);
    if (!out) return valError("\\commutatorSubgroup failed");
    return valPtr(VAL_SUBGROUP, out);
}

static Value bi_groupAbelianization_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsGroup(a[0])) {
        valFree(a[0]);
        return valError("\\groupAbelianization expects one group");
    }
    Group* out = groupAbelianization((Group*)a[0].as.ptr);
    valFree(a[0]);
    if (!out) return valError("\\groupAbelianization failed");
    return valPtr(VAL_GROUP, out);
}

static Value bi_listNormalSubgroups_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsGroup(a[0])) {
        valFree(a[0]);
        return valError("\\listNormalSubgroups expects one group");
    }
    Group* group = (Group*)a[0].as.ptr;
    int count = 0;
    SubGroup** subgroups = listAllNormalSubgroups(group, &count);
    if (!subgroups) {
        valFree(a[0]);
        return valError("\\listNormalSubgroups failed");
    }
    Value out = subgroupListSymbol("normal subgroups", group, subgroups, count);
    freeSubgroupArray(subgroups, count);
    valFree(a[0]);
    return out;
}

static Value bi_listMaximalSubgroups_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsGroup(a[0])) {
        valFree(a[0]);
        return valError("\\listMaximalSubgroups expects one group");
    }
    Group* group = (Group*)a[0].as.ptr;
    int count = 0;
    SubGroup** subgroups = listAllMaximalSubgroups(group, &count);
    if (!subgroups) {
        valFree(a[0]);
        return valError("\\listMaximalSubgroups failed");
    }
    Value out = subgroupListSymbol("maximal subgroups", group, subgroups, count);
    freeSubgroupArray(subgroups, count);
    valFree(a[0]);
    return out;
}

static Value bi_largestCoreFreeSubgroup_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsGroup(a[0])) {
        valFree(a[0]);
        return valError("\\largestCoreFreeSubgroup expects one group");
    }
    SubGroup* out = largestCoreFreeSubgroup((Group*)a[0].as.ptr);
    valFree(a[0]);
    if (!out) return valError("\\largestCoreFreeSubgroup failed");
    return valPtr(VAL_SUBGROUP, out);
}

static Value bi_isCyclicGroup_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsGroup(a[0])) {
        valFree(a[0]);
        return valError("\\isCyclicGroup expects one group");
    }
    bool result = isCyclicGroup((Group*)a[0].as.ptr);
    valFree(a[0]);
    return valBool(result);
}

static Value bi_isCyclicSubgroup_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsSubgroup(a[0])) {
        valFree(a[0]);
        return valError("\\isCyclicSubgroup expects one subgroup");
    }
    bool result = isCyclicSubgroup((SubGroup*)a[0].as.ptr);
    valFree(a[0]);
    return valBool(result);
}

static Value bi_generatesCyclicGroup_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsGroup(a[0]) || !valueIsGroupElement(a[1])) {
        valFree(a[0]);
        valFree(a[1]);
        return valError("\\generatesCyclicGroup expects a group and a group element");
    }
    bool result = generatesCyclicGroup((Group*)a[0].as.ptr, (GroupElement*)a[1].as.ptr);
    valFree(a[0]);
    valFree(a[1]);
    return valBool(result);
}

static Value bi_generatesCyclicSubgroup_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsSubgroup(a[0]) || !valueIsGroupElement(a[1])) {
        valFree(a[0]);
        valFree(a[1]);
        return valError("\\generatesCyclicSubgroup expects a subgroup and a group element");
    }
    bool result = generatesCyclicSubgroup((SubGroup*)a[0].as.ptr, (GroupElement*)a[1].as.ptr);
    valFree(a[0]);
    valFree(a[1]);
    return valBool(result);
}

static Value bi_getCyclicSubgroup_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsGroupElement(a[0])) {
        valFree(a[0]);
        return valError("\\getCyclicSubgroup expects one group element");
    }
    SubGroup* out = getCyclicSubgroup((GroupElement*)a[0].as.ptr);
    valFree(a[0]);
    if (!out) return valError("\\getCyclicSubgroup failed");
    return valPtr(VAL_SUBGROUP, out);
}

static Value bi_isInCoset_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsGroupCoset(a[0]) || !valueIsGroupElement(a[1])) {
        valFree(a[0]);
        valFree(a[1]);
        return valError("\\isInCoset expects a coset and a group element");
    }
    bool result = isInGroupCoset((GroupCoset*)a[0].as.ptr, (GroupElement*)a[1].as.ptr);
    valFree(a[0]);
    valFree(a[1]);
    return valBool(result);
}

static Value bi_leftCoset_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsSubgroup(a[0]) || !valueIsGroupElement(a[1])) {
        valFree(a[0]);
        valFree(a[1]);
        return valError("\\leftCoset expects a subgroup and a group element");
    }
    GroupCoset* out = generateLeftGroupCoset((SubGroup*)a[0].as.ptr, (GroupElement*)a[1].as.ptr);
    valFree(a[0]);
    valFree(a[1]);
    if (!out) return valError("\\leftCoset failed");
    return valPtr(VAL_GROUP_COSET, out);
}

static Value bi_rightCoset_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsSubgroup(a[0]) || !valueIsGroupElement(a[1])) {
        valFree(a[0]);
        valFree(a[1]);
        return valError("\\rightCoset expects a subgroup and a group element");
    }
    GroupCoset* out = generateRightGroupCoset((SubGroup*)a[0].as.ptr, (GroupElement*)a[1].as.ptr);
    valFree(a[0]);
    valFree(a[1]);
    if (!out) return valError("\\rightCoset failed");
    return valPtr(VAL_GROUP_COSET, out);
}

static Value bi_listLeftCosets_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsSubgroup(a[0])) {
        valFree(a[0]);
        return valError("\\listLeftCosets expects one subgroup");
    }
    SubGroup* subgroup = (SubGroup*)a[0].as.ptr;
    int count = subgroupIndex(subgroup);
    GroupCoset** cosets = getLeftGroupCosets(subgroup);
    valFree(a[0]);
    if (!cosets || count < 0) return valError("\\listLeftCosets failed");
    return groupCosetArrayToList(cosets, count);
}

static Value bi_listRightCosets_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsSubgroup(a[0])) {
        valFree(a[0]);
        return valError("\\listRightCosets expects one subgroup");
    }
    SubGroup* subgroup = (SubGroup*)a[0].as.ptr;
    int count = subgroupIndex(subgroup);
    GroupCoset** cosets = getRightGroupCosets(subgroup);
    valFree(a[0]);
    if (!cosets || count < 0) return valError("\\listRightCosets failed");
    return groupCosetArrayToList(cosets, count);
}

static Value bi_kProductGroup_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    int k;
    if (!valueIsGroup(a[0]) || !valueToUsagiInt(a[1], &k)) {
        valFree(a[0]);
        valFree(a[1]);
        return valError("\\kProductGroup expects a group and a positive integer");
    }
    Group* out = kfoldProductGroup((Group*)a[0].as.ptr, k);
    valFree(a[0]);
    valFree(a[1]);
    if (!out) return valError("\\kProductGroup failed");
    return valPtr(VAL_GROUP, out);
}

static Value bi_kProductRing_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    int k;
    if (!valueIsRing(a[0]) || !valueToUsagiInt(a[1], &k)) {
        valFree(a[0]);
        valFree(a[1]);
        return valError("\\kProductRing expects a ring and a positive integer");
    }
    Ring* out = kfoldProductRing((Ring*)a[0].as.ptr, k);
    valFree(a[0]);
    valFree(a[1]);
    if (!out) return valError("\\kProductRing failed");
    return valPtr(VAL_RING, out);
}

static Value bi_subring_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    int* indices = NULL;
    int count = 0;
    if (!collectRingElementIndices(a[0], NULL, &indices, &count)) {
        valFree(a[0]);
        return valError("\\subring expects a ring element or a list of ring elements");
    }
    Ring* ring = NULL;
    if (a[0].kind == VAL_LIST) ring = ((RingElement*)a[0].as.list.items[0].as.ptr)->ring;
    else ring = ((RingElement*)a[0].as.ptr)->ring;
    SubRing* out = constructSubring(ring, indices, count);
    if (!out) free(indices);
    valFree(a[0]);
    if (!out) return valError("\\subring failed");
    return valPtr(VAL_SUBRING, out);
}

static Value bi_leftIdeal_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    int* indices = NULL;
    int count = 0;
    if (!collectRingElementIndices(a[0], NULL, &indices, &count)) {
        valFree(a[0]);
        return valError("\\leftIdeal expects a ring element or a list of ring elements");
    }
    Ring* ring = NULL;
    if (a[0].kind == VAL_LIST) ring = ((RingElement*)a[0].as.list.items[0].as.ptr)->ring;
    else ring = ((RingElement*)a[0].as.ptr)->ring;
    Ideal* out = constructLeftIdeal(ring, indices, count);
    if (!out) free(indices);
    valFree(a[0]);
    if (!out) return valError("\\leftIdeal failed");
    return valPtr(VAL_IDEAL, out);
}

static Value bi_rightIdeal_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    int* indices = NULL;
    int count = 0;
    if (!collectRingElementIndices(a[0], NULL, &indices, &count)) {
        valFree(a[0]);
        return valError("\\rightIdeal expects a ring element or a list of ring elements");
    }
    Ring* ring = NULL;
    if (a[0].kind == VAL_LIST) ring = ((RingElement*)a[0].as.list.items[0].as.ptr)->ring;
    else ring = ((RingElement*)a[0].as.ptr)->ring;
    Ideal* out = constructRightIdeal(ring, indices, count);
    if (!out) free(indices);
    valFree(a[0]);
    if (!out) return valError("\\rightIdeal failed");
    return valPtr(VAL_IDEAL, out);
}

static Value bi_isTrivialSubring_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsSubring(a[0])) {
        valFree(a[0]);
        return valError("\\isTrivialSubring expects one subring");
    }
    bool result = isTrivialSubring((SubRing*)a[0].as.ptr);
    valFree(a[0]);
    return valBool(result);
}

static Value bi_subgroupGeneratedBy_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    int* indices = NULL;
    int count = 0;
    if (!valueIsGroup(a[0]) || !collectGroupElementIndices(a[1], (Group*)a[0].as.ptr, &indices, &count)) {
        valFree(a[0]);
        valFree(a[1]);
        return valError("\\subgroupGeneratedBy expects a group and a group element or list of group elements from that group");
    }
    SubGroup* out = subgroupGeneratedBy((Group*)a[0].as.ptr, indices, count);
    free(indices);
    valFree(a[0]);
    valFree(a[1]);
    if (!out) return valError("\\subgroupGeneratedBy failed");
    return valPtr(VAL_SUBGROUP, out);
}

static Value bi_subgroupAsGroup_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsSubgroup(a[0])) {
        valFree(a[0]);
        return valError("\\subgroupAsGroup expects one subgroup");
    }
    Group* out = subgroupAsGroup((SubGroup*)a[0].as.ptr);
    valFree(a[0]);
    if (!out) return valError("\\subgroupAsGroup failed");
    return valPtr(VAL_GROUP, out);
}

static Value bi_groupHomomorphism_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    int* mapping = NULL;
    if (!valueIsGroup(a[0]) || !valueIsGroup(a[1]) ||
        !collectGroupHomomorphismMapping(a[2], (Group*)a[0].as.ptr, (Group*)a[1].as.ptr, &mapping)) {
        valFree(a[0]);
        valFree(a[1]);
        valFree(a[2]);
        return valError("\\groupHomomorphism expects two groups and a complete mapping list [g1:h1, g2:h2, ...]");
    }
    GroupHomomorphism* out = constructGroupHomomorphism((Group*)a[0].as.ptr, (Group*)a[1].as.ptr, mapping, ((Group*)a[0].as.ptr)->card);
    if (!out) free(mapping);
    valFree(a[0]);
    valFree(a[1]);
    valFree(a[2]);
    if (!out) return valError("\\groupHomomorphism failed");
    return valPtr(VAL_GROUP_HOMOMORPHISM, out);
}

static Value bi_ringHomomorphism_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    int* mapping = NULL;
    if (!valueIsRing(a[0]) || !valueIsRing(a[1]) ||
        !collectRingHomomorphismMapping(a[2], (Ring*)a[0].as.ptr, (Ring*)a[1].as.ptr, &mapping)) {
        valFree(a[0]);
        valFree(a[1]);
        valFree(a[2]);
        return valError("\\ringHomomorphism expects two rings and a complete mapping list [x1:y1, x2:y2, ...]");
    }
    RingHomomorphism* out = constructRingHomomorphism((Ring*)a[0].as.ptr, (Ring*)a[1].as.ptr, mapping, ((Ring*)a[0].as.ptr)->card);
    if (!out) free(mapping);
    valFree(a[0]);
    valFree(a[1]);
    valFree(a[2]);
    if (!out) return valError("\\ringHomomorphism failed");
    return valPtr(VAL_RING_HOMOMORPHISM, out);
}

static Value bi_groupHomomorphismKernel_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsGroupHomomorphism(a[0])) {
        valFree(a[0]);
        return valError("\\groupHomomorphismKernel expects one group homomorphism");
    }
    SubGroup* out = groupHomomorphismKernel((GroupHomomorphism*)a[0].as.ptr);
    valFree(a[0]);
    if (!out) return valError("\\groupHomomorphismKernel failed");
    return valPtr(VAL_SUBGROUP, out);
}

static Value bi_ringHomomorphismKernel_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsRingHomomorphism(a[0])) {
        valFree(a[0]);
        return valError("\\ringHomomorphismKernel expects one ring homomorphism");
    }
    Ideal* out = ringHomomorphismKernel((RingHomomorphism*)a[0].as.ptr);
    valFree(a[0]);
    if (!out) return valError("\\ringHomomorphismKernel failed");
    return valPtr(VAL_IDEAL, out);
}

static Value bi_groupHomomorphismImage_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsGroupHomomorphism(a[0])) {
        valFree(a[0]);
        return valError("\\groupHomomorphismImage expects one group homomorphism");
    }
    Group* out = groupHomomorphismImage((GroupHomomorphism*)a[0].as.ptr);
    valFree(a[0]);
    if (!out) return valError("\\groupHomomorphismImage failed");
    return valPtr(VAL_GROUP, out);
}

static Value bi_ringHomomorphismImage_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsRingHomomorphism(a[0])) {
        valFree(a[0]);
        return valError("\\ringHomomorphismImage expects one ring homomorphism");
    }
    Ring* out = ringHomomorphismImage((RingHomomorphism*)a[0].as.ptr);
    valFree(a[0]);
    if (!out) return valError("\\ringHomomorphismImage failed");
    return valPtr(VAL_RING, out);
}

static Value bi_isGroupIsomorphism_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsGroupHomomorphism(a[0])) {
        valFree(a[0]);
        return valError("\\isGroupIsomorphism expects one group homomorphism");
    }
    bool result = isGroupIsomorphism((GroupHomomorphism*)a[0].as.ptr);
    valFree(a[0]);
    return valBool(result);
}

static Value bi_isRingIsomorphism_cmd(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsRingHomomorphism(a[0])) {
        valFree(a[0]);
        return valError("\\isRingIsomorphism expects one ring homomorphism");
    }
    bool result = isRingIsomorphism((RingHomomorphism*)a[0].as.ptr);
    valFree(a[0]);
    return valBool(result);
}

static Value bi_normalize(EvalContext* c, Value* a, size_t n) {
    (void)c; (void)n;
    if (!valueIsVector(a[0])) return vectorUnaryError("\\normalize expects one vector", a[0]);
    Vector* out = normalizeVector((Vector*)a[0].as.ptr);
    if (!out) return vectorUnaryError("\\normalize failed", a[0]);
    valFree(a[0]);
    return valPtr(VAL_VECTOR, out);
}

static Value bi_vdist(EvalContext* c, Value* a, size_t n) {
    (void)c;
    Value lhs, rhs;
    if (!unpackVectorPair(a, n, &lhs, &rhs)) return valError("\\vdist expects two vectors");
    if (!valueIsVector(lhs) || !valueIsVector(rhs)) return vectorBinaryError("\\vdist expects two vectors", lhs, rhs);
    double dist = vectorDistance((Vector*)lhs.as.ptr, (Vector*)rhs.as.ptr);
    if (isnan(dist)) return vectorBinaryError("\\vdist requires same dimension vectors", lhs, rhs);
    valFree(lhs);
    valFree(rhs);
    return valDecimal(dist);
}

static Value bi_vangle(EvalContext* c, Value* a, size_t n) {
    (void)c;
    Value lhs, rhs;
    if (!unpackVectorPair(a, n, &lhs, &rhs)) return valError("\\vangle expects two vectors");
    if (!valueIsVector(lhs) || !valueIsVector(rhs)) return vectorBinaryError("\\vangle expects two vectors", lhs, rhs);
    double angle = vectorAngle((Vector*)lhs.as.ptr, (Vector*)rhs.as.ptr);
    if (isnan(angle)) return vectorBinaryError("\\vangle requires compatible non-zero vectors", lhs, rhs);
    valFree(lhs);
    valFree(rhs);
    return valDecimal(angle);
}

static Value bi_vproj(EvalContext* c, Value* a, size_t n) {
    (void)c;
    Value lhs, rhs;
    if (!unpackVectorPair(a, n, &lhs, &rhs)) return valError("\\vproj expects two vectors");
    if (!valueIsVector(lhs) || !valueIsVector(rhs)) return vectorBinaryError("\\vproj expects two vectors", lhs, rhs);
    Vector* proj = vectorProjectOnto((Vector*)lhs.as.ptr, (Vector*)rhs.as.ptr);
    if (!proj) return vectorBinaryError("\\vproj requires compatible vectors", lhs, rhs);
    valFree(lhs);
    valFree(rhs);
    return valPtr(VAL_VECTOR, proj);
}

/* ---------- Built-in registration ---------- */
 
/* Call once at startup; add more registerCommand calls later, e.g.:
 *   registerCommand("det",    1, bi_det);       // sokko:  determinant(Matrix*)
 *   registerCommand("otimes", 2, bi_otimes);    // sokko:  tensorMatrices(...)
 *   registerCommand("cup",    2, bi_cup);       // ookami: setUnion(...)
 *   registerCommand("order",  1, bi_order);     // usagi:  elementOrder(...)
 * For matrix environments, register e.g.:
 *   registerCommand("__matrix__",  1, bi_matrix);
 *   registerCommand("__pmatrix__", 1, bi_pmatrix);  // or fall back to __matrix__
 */
void registerBuiltins(void) {
    registerCommand("+",  2, bi_add);
    registerCommand("-",  2, bi_sub);
    registerCommand("*",  2, bi_mul);
    registerCommand("/",  2, bi_div);
    registerCommand("cdot",  2, bi_cdot);
    registerCommand("times",  2, bi_times);
    registerCommand("otimes",  2, bi_otimes);
    registerCommand("cap",  2, bi_cap);
    registerCommand("cup",  2, bi_cup);
    registerCommand("u-",  1, bi_neg);
    registerCommand("u+",  1, bi_pos);
    registerCommand("^",  2, bi_pow);
    registerCommand("pi",  0, bi_pi);
    registerCommand("e",  0, bi_e);
    registerCommand("phi",  0, bi_phi);
    registerCommand("frac",  2, bi_frac);
    registerCommand("sqrt",  1, bi_sqrt);
    registerCommand("imatrix",  1, bi_imatrix);
    registerCommand("zeromatrix",  1, bi_zeromatrix);
    registerCommand("isSquare",  1, bi_issquare);
    registerCommand("isSymmetric",  1, bi_issym);
    registerCommand("isAntisymmetric",  1, bi_isantisym);
    registerCommand("isUnitary",  1, bi_isunitary);
    registerCommand("isOrthogonal",  1, bi_isorthog);
    registerCommand("rank",  1, bi_rank);
    registerCommand("nullity",  1, bi_nullity);
    registerCommand("trace",  1, bi_trace);
    registerCommand("frobeniusNorm",  1, bi_fnorm);
    registerCommand("det",  1, bi_det);
    registerCommand("rref",  1, bi_rowReduce);
    registerCommand("eigenvalues",  1, bi_eigenvalues);
    registerCommand("eigenvectors",  1, bi_eigenvectors);
    registerCommand("columnReduce",  1, bi_columnReduce);
    registerCommand("rowSpace",  1, bi_rowSpace);
    registerCommand("columnSpace",  1, bi_columnSpace);
    registerCommand("solveLinEq",  2, bi_solveLinEq);
    registerCommand("l2Norm",  1, bi_l2norm);
    registerCommand("normalize",  1, bi_normalize);
    registerCommand("vdist", -1, bi_vdist);
    registerCommand("vangle", -1, bi_vangle);
    registerCommand("vproj", -1, bi_vproj);
    registerCommand("force",  3, bi_force);
    registerCommand("body",  3, bi_body);
    registerCommand("copy",  1, bi_copy);
    registerCommand("transpose",  1, bi_transpose);
    registerCommand("adjoint",  1, bi_adjoint);
    registerCommand("inverse",  1, bi_inverse);
    registerCommand("translate",  2, bi_translate);
    registerCommand("dilate",  2, bi_dilate);
    registerCommand("append",  2, bi_append);
    registerCommand("remove",  2, bi_remove);
    registerCommand("addForce",  2, bi_addForce);
    registerCommand("removeForce",  2, bi_removeForce);
    registerCommand("displacement",  2, bi_displacement);
    registerCommand("avgVelocity",  3, bi_averageVelocity);
    registerCommand("avgAccel",  4, bi_averageAcceleration);
    registerCommand("velocityAtTime",  3, bi_velocityAtTime);
    registerCommand("positionAtTime",  4, bi_positionAtTime);
    registerCommand("speedAtPosition",  4, bi_speedAtPosition);
    registerCommand("velocityAtPosition",  4, bi_velocityAtPosition);
    registerCommand("projectileInfo",  3, bi_projectileInfo);
    registerCommand("centripetalAcceleration",  2, bi_centripetalAcceleration);
    registerCommand("angularVelocity",  2, bi_angularVelocity);
    registerCommand("netForce",  1, bi_netForce);
    registerCommand("accelerationFromForce",  1, bi_accelerationFromForce);
    registerCommand("gravityForce",  1, bi_gravityForce);
    registerCommand("normalForce",  2, bi_normalForce);
    registerCommand("frictionForce",  3, bi_frictionForce);
    registerCommand("momentum",  1, bi_momentum);
    registerCommand("kineticEnergy",  1, bi_kineticEnergy);
    registerCommand("gravPotentialEnergy",  2, bi_gravPotentialEnergy);
    registerCommand("springPotentialEnergy",  2, bi_springPotentialEnergy);
    registerCommand("work",  2, bi_work);
    registerCommand("power",  2, bi_power_cmd);
    registerCommand("impulse",  2, bi_impulse);
    registerCommand("centerOfMass", -1, bi_centerOfMass);
    registerCommand("centerOfMassVelocity", -1, bi_centerOfMassVelocity);
    registerCommand("elasticCollision", -1, bi_elasticCollision);
    registerCommand("inelasticCollision", -1, bi_inelasticCollision);
    registerCommand("momentOfInertiaPoint",  2, bi_moiPoint);
    registerCommand("momentOfInertiaRod",  2, bi_moiRod);
    registerCommand("momentOfInertiaDisk",  2, bi_moiDisk);
    registerCommand("parallelAxis",  3, bi_parallelAxis);
    registerCommand("torque",  2, bi_torque);
    registerCommand("angularMomentum",  2, bi_angularMomentumCmd);
    registerCommand("rotationalKineticEnergy",  2, bi_rotationalKineticEnergy);
    registerCommand("angularAccelerationFromTorque",  2, bi_angularAccelerationFromTorque);
    registerCommand("ZnGroup",  1, bi_ZnGroup);
    registerCommand("ZnProductGroup",  1, bi_ZnProductGroup);
    registerCommand("Sn",  1, bi_SymmetricGroup);
    registerCommand("An",  1, bi_AlternatingGroup);
    registerCommand("Dn",  1, bi_DihedralGroup);
    registerCommand("ZnRing",  1, bi_ZnRing);
    registerCommand("ZnProductRing",  1, bi_ZnProductRing);
    registerCommand("primeField",  1, bi_primeFiniteField);
    registerCommand("finiteField",  2, bi_finiteField);
    registerCommand("Q8",  0, bi_quaternionGroup);
    registerCommand("isPrime",  1, bi_isPrime_cmd);
    registerCommand("factorial",  1, bi_factorial_cmd);
    registerCommand("listElements",  1, bi_listElements);
    registerCommand("getElement",  2, bi_getElement);
    registerCommand("groupElementConjugate",  2, bi_groupElementConjugate_cmd);
    registerCommand("groupCommutator",  2, bi_groupCommutator_cmd);
    registerCommand("elementOrderGroup",  1, bi_elementOrderGroup);
    registerCommand("additiveOrder",  1, bi_additiveOrder_cmd);
    registerCommand("multiplicativeOrder",  1, bi_multiplicativeOrder_cmd);
    registerCommand("isInGroup",  2, bi_isInGroup_cmd);
    registerCommand("isGroupIdentity",  2, bi_isGroupIdentity_cmd);
    registerCommand("groupIdentity",  1, bi_groupIdentity_cmd);
    registerCommand("isAddIdentity",  2, bi_isAddIdentity_cmd);
    registerCommand("isMultIdentity",  2, bi_isMultIdentity_cmd);
    registerCommand("hasMultIdentity",  1, bi_hasMultIdentity_cmd);
    registerCommand("elementsCommute",  2, bi_elementsCommute_cmd);
    registerCommand("isTrivialGroup",  1, bi_isTrivialGroup_cmd);
    registerCommand("isTrivialRing",  1, bi_isTrivialRing_cmd);
    registerCommand("trivialGroup",  0, bi_trivialGroup_cmd);
    registerCommand("trivialRing",  0, bi_trivialRing_cmd);
    registerCommand("groupElementInfo",  1, bi_groupElementInfo_cmd);
    registerCommand("ringElementInfo",  1, bi_ringElementInfo_cmd);
    registerCommand("isSubgroup",  2, bi_isSubgroup_cmd);
    registerCommand("isNormalSubgroup",  2, bi_isNormalSubgroup_cmd);
    registerCommand("trivialSubgroup",  0, bi_trivialSubgroup_cmd);
    registerCommand("isTrivialSubgroup",  1, bi_isTrivialSubgroup_cmd);
    registerCommand("groupCenter",  1, bi_groupCenter_cmd);
    registerCommand("groupCentralizer",  2, bi_groupCentralizer_cmd);
    registerCommand("groupNormalizer",  1, bi_groupNormalizer_cmd);
    registerCommand("conjugacyClass",  1, bi_conjugacyClass_cmd);
    registerCommand("normalClosure",  1, bi_normalClosure_cmd);
    registerCommand("isInSubgroup",  2, bi_isInSubgroup_cmd);
    registerCommand("subgroupContains",  2, bi_subgroupContains_cmd);
    registerCommand("subgroupIndex",  1, bi_subgroupIndex_cmd);
    registerCommand("subgroupConjugate",  2, bi_subgroupConjugate_cmd);
    registerCommand("commutatorSubgroup",  1, bi_commutatorSubgroup_cmd);
    registerCommand("abelianization",  1, bi_groupAbelianization_cmd);
    registerCommand("listNormalSubgroups",  1, bi_listNormalSubgroups_cmd);
    registerCommand("listMaximalSubgroups",  1, bi_listMaximalSubgroups_cmd);
    registerCommand("largestCoreFreeSubgroup",  1, bi_largestCoreFreeSubgroup_cmd);
    registerCommand("isCyclicGroup",  1, bi_isCyclicGroup_cmd);
    registerCommand("isCyclicSubgroup",  1, bi_isCyclicSubgroup_cmd);
    registerCommand("generatesCyclicGroup",  2, bi_generatesCyclicGroup_cmd);
    registerCommand("generatesCyclicSubgroup",  2, bi_generatesCyclicSubgroup_cmd);
    registerCommand("getCyclicSubgroup",  1, bi_getCyclicSubgroup_cmd);
    registerCommand("isInCoset",  2, bi_isInCoset_cmd);
    registerCommand("leftCoset",  2, bi_leftCoset_cmd);
    registerCommand("rightCoset",  2, bi_rightCoset_cmd);
    registerCommand("listLeftCosets",  1, bi_listLeftCosets_cmd);
    registerCommand("listRightCosets",  1, bi_listRightCosets_cmd);
    registerCommand("kProductGroup",  2, bi_kProductGroup_cmd);
    registerCommand("kProductRing",  2, bi_kProductRing_cmd);
    registerCommand("subring",  1, bi_subring_cmd);
    registerCommand("leftIdeal",  1, bi_leftIdeal_cmd);
    registerCommand("rightIdeal",  1, bi_rightIdeal_cmd);
    registerCommand("isTrivialSubring",  1, bi_isTrivialSubring_cmd);
    registerCommand("subgroupGeneratedBy",  2, bi_subgroupGeneratedBy_cmd);
    registerCommand("subgroupAsGroup",  1, bi_subgroupAsGroup_cmd);
    registerCommand("groupHomomorphism",  3, bi_groupHomomorphism_cmd);
    registerCommand("ringHomomorphism",  3, bi_ringHomomorphism_cmd);
    registerCommand("groupHomomorphismKernel",  1, bi_groupHomomorphismKernel_cmd);
    registerCommand("ringHomomorphismKernel",  1, bi_ringHomomorphismKernel_cmd);
    registerCommand("groupHomomorphismImage",  1, bi_groupHomomorphismImage_cmd);
    registerCommand("ringHomomorphismImage",  1, bi_ringHomomorphismImage_cmd);
    registerCommand("isGroupIsomorphism",  1, bi_isGroupIsomorphism_cmd);
    registerCommand("isRingIsomorphism",  1, bi_isRingIsomorphism_cmd);
    registerCommand("listSubgroups",  1, bi_listSubgroups_cmd);
    registerCommand("getSubgroup",  2, bi_getSubgroup_cmd);
    registerCommand("subgroupInfo",  1, bi_subgroupInfo_cmd);
    registerCommand("isCommutativeGroup",  1, bi_isCommutativeGroup_cmd);
    registerCommand("isCommutativeRing",  1, bi_isCommutativeRing_cmd);
    registerCommand("isSimple",  1, bi_isSimple_cmd);
    registerCommand("isInverse",  2, bi_isInverse_cmd);
    registerCommand("isAddInverse",  2, bi_isAddInverse_cmd);
    registerCommand("isMultInverse",  2, bi_isMultInverse_cmd);
    registerCommand("hasMultInverse",  1, bi_hasMultInverse_cmd);
    registerCommand("isZeroDivisor",  1, bi_isZeroDivisor_cmd);
    registerCommand("hasZeroDivisors",  1, bi_hasZeroDivisors_cmd);
    registerCommand("isIntegralDomain",  1, bi_isIntegralDomain_cmd);
    registerCommand("isDivisionRing",  1, bi_isDivisionRing_cmd);
    registerCommand("isField",  1, bi_isField_cmd);
    registerCommand("isSubset",  2, bi_isSubset);
    registerCommand("diameter",  1, bi_diam);
    registerCommand("dconst",  1, bi_dc);
    registerCommand("density",  1, bi_density);
    registerCommand("adsCard",  1, bi_adsCard);
    registerCommand("ddsCard",  1, bi_ddsCard);
    registerCommand("mdsCard",  1, bi_mdsCard);
    registerCommand("isAP",  1, bi_isAP);
    registerCommand("isGP",  1, bi_isGP);
    registerCommand("ruzsaDistance",  2, bi_rd);
    registerCommand("ruzsaDistancePositive",  2, bi_rdpos);
    registerCommand("repAdd",  2, bi_repAdd);
    registerCommand("kRepAdd",  3, bi_kRepAdd);
    registerCommand("repDiff",  2, bi_repDiff);
    registerCommand("kRepDiff",  3, bi_kRepDiff);
    registerCommand("repMult",  2, bi_repMult);
    registerCommand("kRepMult",  3, bi_kRepMult);
    registerCommand("energyAdd",  1, bi_energyAdd);
    registerCommand("kEnergyAdd",  2, bi_kEnergyAdd);
    registerCommand("energyDiff",  1, bi_energyDiff);
    registerCommand("kEnergyDiff",  2, bi_kEnergyDiff);
    registerCommand("energyMult",  1, bi_energyMult);
    registerCommand("kEnergyMult",  2, bi_kEnergyMult);
}
