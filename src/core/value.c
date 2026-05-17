#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<math.h>
#include "value.h"
#include "kuma.h"
#include "neko.h"
#include "ookami.h"
#include "poni.h"
#include "sokko.h"
#include "tora.h"
#include "usagi.h"

/* ---------- Helper methods ---------- */

// Duplicate a C string (returns NULL on NULL input)
static char* dupstr(const char* s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char* r = malloc(n + 1);
    if (!r) return NULL;
    memcpy(r, s, n + 1);
    return r;
}

static bool fieldUsable(Field* field) {
    FieldElement zero = zeroFieldElement(field);
    bool usable = fieldElementIsValid(&zero);
    freeFieldElement(&zero);
    return usable;
}

static void freeValueField(ValueField* field) {
    if (!field) return;
    freeField(&field->field);
    freeValueField(field->base);
    free(field);
}

static ValueField* cloneValueField(Field* field) {
    ValueField* out;
    if (!fieldUsable(field)) return NULL;

    // Allocate the owned field wrapper before dispatching on the field type
    out = calloc(1, sizeof(ValueField));
    if (!out) return NULL;
    switch (field->type) {
        case QQ:
            out->field = constructQQField();
            break;
        case RR:
            out->field = constructRRField();
            break;
        case CC:
            out->field = constructCCField();
            break;
        case FF:
            out->field = constructFFField(field->data.ff.p, field->data.ff.degree,
                    field->data.ff.modulus);
            break;
        case FF_QEXT:
            out->base = cloneValueField(field->data.ffqext.baseField);
            if (!out->base) {
                freeValueField(out);
                return NULL;
            }
            out->field.repr = field->repr;
            out->field.chr = field->chr;
            out->field.type = FF_QEXT;
            out->field.data.ffqext.baseField = &out->base->field;
            out->field.data.ffqext.genRepr = field->data.ffqext.genRepr;
            out->field.data.ffqext.radicand = malloc(sizeof(FieldElement));
            if (!out->field.data.ffqext.radicand) {
                freeValueField(out);
                return NULL;
            }
            *out->field.data.ffqext.radicand =
                copyFieldElementToField(&out->base->field, *field->data.ffqext.radicand);
            break;
        case NF:
            out->base = cloneValueField(field->data.nf.baseField);
            if (!out->base) {
                freeValueField(out);
                return NULL;
            }
            out->field.repr = field->repr;
            out->field.chr = field->chr;
            out->field.type = NF;
            out->field.data.nf.baseField = &out->base->field;
            out->field.data.nf.gen.repr = field->data.nf.gen.repr;
            out->field.data.nf.gen.degree = field->data.nf.gen.degree;
            out->field.data.nf.gen.minPolyCoeffs =
                calloc(field->data.nf.gen.degree + 1, sizeof(FieldElement));
            if (!out->field.data.nf.gen.minPolyCoeffs) {
                freeValueField(out);
                return NULL;
            }
            for (size_t i = 0; i <= field->data.nf.gen.degree; i++) {
                out->field.data.nf.gen.minPolyCoeffs[i] =
                    copyFieldElementToField(&out->base->field,
                            field->data.nf.gen.minPolyCoeffs[i]);
                if (!fieldElementIsValid(&out->field.data.nf.gen.minPolyCoeffs[i])) {
                    freeValueField(out);
                    return NULL;
                }
            }
            break;
    }

    // Validate the copied field through the public element construction API
    if (!fieldUsable(&out->field)) {
        freeValueField(out);
        return NULL;
    }
    return out;
}

static ValueFieldElement* cloneValueFieldElement(FieldElement element) {
    ValueFieldElement* out;
    if (!fieldElementIsValid(&element)) return NULL;

    // Copy the field first so the element can point into the owned field chain
    out = calloc(1, sizeof(ValueFieldElement));
    if (!out) return NULL;
    out->field = cloneValueField(element.field);
    if (!out->field) {
        free(out);
        return NULL;
    }
    out->element = copyFieldElementToField(&out->field->field, element);
    if (!fieldElementIsValid(&out->element)) {
        freeValueField(out->field);
        free(out);
        return NULL;
    }
    return out;
}

static ValueCDAlgebra* cloneValueCDAlgebra(CDAlgebra algebra) {
    ValueCDAlgebra* out;
    FieldElement* params = NULL;
    if (!cdAlgebraIsValid(&algebra)) return NULL;

    // Copy the base field before rebuilding the algebra over that copy
    out = calloc(1, sizeof(ValueCDAlgebra));
    if (!out) return NULL;
    out->field = cloneValueField(algebra.field);
    if (!out->field) {
        free(out);
        return NULL;
    }
    if (algebra.degree > 0) {
        params = calloc(algebra.degree, sizeof(FieldElement));
        if (!params) {
            freeValueField(out->field);
            free(out);
            return NULL;
        }
        for (size_t i = 0; i < algebra.degree; i++) {
            params[i] = copyFieldElementToField(&out->field->field, algebra.params[i]);
            if (!fieldElementIsValid(&params[i])) {
                for (size_t j = 0; j <= i; j++) freeFieldElement(&params[j]);
                free(params);
                freeValueField(out->field);
                free(out);
                return NULL;
            }
        }
    }
    out->algebra = constructCDAlgebra(&out->field->field, algebra.degree, params);
    for (size_t i = 0; i < algebra.degree; i++) freeFieldElement(&params[i]);
    free(params);
    if (!cdAlgebraIsValid(&out->algebra)) {
        freeValueField(out->field);
        free(out);
        return NULL;
    }
    return out;
}

static ValueCDElement* cloneValueCDElement(CDElement element) {
    ValueCDElement* out;
    FieldElement* coeffs;
    if (!cdElementIsValid(&element)) return NULL;

    // Rebuild the algebra and then copy coefficients into its owned field
    out = calloc(1, sizeof(ValueCDElement));
    if (!out) return NULL;
    ValueCDAlgebra* algebraCopy = cloneValueCDAlgebra(*element.algebra);
    if (!algebraCopy) {
        free(out);
        return NULL;
    }
    out->field = algebraCopy->field;
    out->algebra = algebraCopy->algebra;
    free(algebraCopy);

    coeffs = calloc(element.dimension, sizeof(FieldElement));
    if (!coeffs) {
        freeCDAlgebra(&out->algebra);
        freeValueField(out->field);
        free(out);
        return NULL;
    }
    for (size_t i = 0; i < element.dimension; i++) {
        coeffs[i] = copyFieldElementToField(&out->field->field, element.coeffs[i]);
        if (!fieldElementIsValid(&coeffs[i])) {
            for (size_t j = 0; j <= i; j++) freeFieldElement(&coeffs[j]);
            free(coeffs);
            freeCDAlgebra(&out->algebra);
            freeValueField(out->field);
            free(out);
            return NULL;
        }
    }
    out->element = constructCDElement(&out->algebra, coeffs);
    for (size_t i = 0; i < element.dimension; i++) freeFieldElement(&coeffs[i]);
    free(coeffs);
    if (!cdElementIsValid(&out->element)) {
        freeCDAlgebra(&out->algebra);
        freeValueField(out->field);
        free(out);
        return NULL;
    }
    return out;
}

static CDElement* cloneCDElementBasisToAlgebra(CDElement* basis, size_t count,
        CDAlgebra* algebra, Field* field) {
    CDElement* out = calloc(count, sizeof(CDElement));
    if (!out && count > 0) return NULL;

    // Copy every basis element into the cloned algebra and base field
    for (size_t i = 0; i < count; i++) {
        FieldElement* coeffs = calloc(basis[i].dimension, sizeof(FieldElement));
        if (!coeffs) {
            for (size_t j = 0; j < i; j++) freeCDElement(&out[j]);
            free(out);
            return NULL;
        }
        for (size_t j = 0; j < basis[i].dimension; j++) {
            coeffs[j] = copyFieldElementToField(field, basis[i].coeffs[j]);
            if (!fieldElementIsValid(&coeffs[j])) {
                for (size_t k = 0; k <= j; k++) freeFieldElement(&coeffs[k]);
                free(coeffs);
                for (size_t k = 0; k < i; k++) freeCDElement(&out[k]);
                free(out);
                return NULL;
            }
        }
        out[i] = constructCDElement(algebra, coeffs);
        for (size_t j = 0; j < basis[i].dimension; j++) freeFieldElement(&coeffs[j]);
        free(coeffs);
        if (!cdElementIsValid(&out[i])) {
            for (size_t j = 0; j <= i; j++) freeCDElement(&out[j]);
            free(out);
            return NULL;
        }
    }
    return out;
}

static ValueCDIdeal* cloneValueCDIdeal(CDIdeal ideal) {
    ValueCDIdeal* out;
    if (!cdIdealIsValid(&ideal)) return NULL;

    // Rebuild the ambient algebra so the ideal owns its basis coordinates
    out = calloc(1, sizeof(ValueCDIdeal));
    if (!out) return NULL;
    ValueCDAlgebra* algebraCopy = cloneValueCDAlgebra(*ideal.algebra);
    if (!algebraCopy) {
        free(out);
        return NULL;
    }
    out->field = algebraCopy->field;
    out->algebra = algebraCopy->algebra;
    free(algebraCopy);

    out->ideal.algebra = &out->algebra;
    out->ideal.count = ideal.count;
    out->ideal.type = ideal.type;
    if (ideal.count > 0) {
        out->ideal.basis = cloneCDElementBasisToAlgebra(ideal.basis, ideal.count,
                &out->algebra, &out->field->field);
        if (!out->ideal.basis) {
            freeCDAlgebra(&out->algebra);
            freeValueField(out->field);
            free(out);
            return NULL;
        }
    }
    if (!cdIdealIsValid(&out->ideal)) {
        freeCDIdeal(&out->ideal);
        freeCDAlgebra(&out->algebra);
        freeValueField(out->field);
        free(out);
        return NULL;
    }
    return out;
}

static ValueCDSubalgebra* cloneValueCDSubalgebra(CDSubalgebra subalgebra) {
    ValueCDSubalgebra* out;
    if (!cdSubalgebraIsValid(&subalgebra)) return NULL;

    // Rebuild the ambient algebra so the subalgebra owns its basis coordinates
    out = calloc(1, sizeof(ValueCDSubalgebra));
    if (!out) return NULL;
    ValueCDAlgebra* algebraCopy = cloneValueCDAlgebra(*subalgebra.cdAlgebra);
    if (!algebraCopy) {
        free(out);
        return NULL;
    }
    out->field = algebraCopy->field;
    out->algebra = algebraCopy->algebra;
    free(algebraCopy);

    out->subalgebra.cdAlgebra = &out->algebra;
    out->subalgebra.count = subalgebra.count;
    out->subalgebra.basis = cloneCDElementBasisToAlgebra(subalgebra.basis, subalgebra.count,
            &out->algebra, &out->field->field);
    if (!out->subalgebra.basis || !cdSubalgebraIsValid(&out->subalgebra)) {
        freeCDSubalgebra(&out->subalgebra);
        freeCDAlgebra(&out->algebra);
        freeValueField(out->field);
        free(out);
        return NULL;
    }
    return out;
}

static bool quaternionMatrixRepIsUsable(QuaternionMatrixRep* rep) {
    return rep != NULL
        && rep->algebra != NULL
        && rep->extField != NULL
        && quaternionAlgebraIsValid(rep->algebra)
        && fieldElementIsValid(&rep->sqrt_a)
        && fieldEq(rep->extField, rep->sqrt_a.field);
}

static ValueQuaternionMatrixRep* cloneValueQuaternionMatrixRep(QuaternionMatrixRep rep) {
    ValueQuaternionMatrixRep* out;
    if (!quaternionMatrixRepIsUsable(&rep)) return NULL;

    out = calloc(1, sizeof(ValueQuaternionMatrixRep));
    if (!out) return NULL;
    ValueCDAlgebra* algebraCopy = cloneValueCDAlgebra(*rep.algebra);
    if (!algebraCopy) {
        free(out);
        return NULL;
    }
    out->field = algebraCopy->field;
    out->algebra = algebraCopy->algebra;
    free(algebraCopy);

    out->rep = constructQuaternionMatrixRep(&out->algebra);
    if (!quaternionMatrixRepIsUsable(&out->rep)) {
        freeQuaternionMatrixRep(&out->rep);
        freeCDAlgebra(&out->algebra);
        freeValueField(out->field);
        free(out);
        return NULL;
    }
    return out;
}

static void printInlineVector(Vector* vector) {
    if (!vector) {
        printf("null");
        return;
    }
    putchar('[');
    for (size_t i = 0; i < vector->numRows; i++) {
        if (i) printf(", ");
        FieldElement elem = getEntry((Matrix*)vector, i, 0);
        char* string = fieldElementToString(elem);
        printf("%s", string ? string : "<invalid>");
        free(string);
        freeFieldElement(&elem);
    }
    putchar(']');
}

static void printInlineGroup(Group* group) {
    if (!group) {
        printf("<group null>");
        return;
    }
    printf("<group card=%zu; elements=[", group->card);
    size_t limit = group->card < 6 ? group->card : 6;
    for (size_t i = 0; i < limit; i++) {
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
    printf("<ring card=%zu; elements=[", ring->card);
    size_t limit = ring->card < 6 ? ring->card : 6;
    for (size_t i = 0; i < limit; i++) {
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
    printf("; card=%zu; elements=[", subgroup->card);
    size_t limit = subgroup->card < 6 ? subgroup->card : 6;
    int* indices = subgroup->type == SUBGROUP_INDEXED ? subgroup->data.indexed.indices : NULL;
    for (size_t i = 0; i < limit; i++) {
        if (i) printf(", ");
        int idx = indices ? indices[i] : -1;
        GroupElement* element = (subgroup->ambient && idx >= 0 && (size_t)idx < subgroup->ambient->card)
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
    size_t limit = (coset->subgroup && coset->subgroup->card < 6) ? coset->subgroup->card : 6;
    int* indices = coset->type == GROUP_COSET_INDEXED ? coset->data.indexed.indices : NULL;
    for (size_t i = 0; i < limit; i++) {
        if (i) printf(", ");
        int idx = indices ? indices[i] : -1;
        GroupElement* element = (coset->group && idx >= 0 && (size_t)idx < coset->group->card)
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
    printf("; card=%zu; elements=[", subring->card);
    size_t limit = subring->card < 6 ? subring->card : 6;
    int* indices = subring->type == SUBRING_INDEXED ? subring->data.indexed.indices : NULL;
    for (size_t i = 0; i < limit; i++) {
        if (i) printf(", ");
        int idx = indices ? indices[i] : -1;
        RingElement* element = (subring->ambient && idx >= 0 && (size_t)idx < subring->ambient->card)
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
    const char* side = ideal->side == IDEAL_LEFT ? "left" : (ideal->side == IDEAL_RIGHT ? "right" : "two-sided");
    printf("<%sIdeal of ", side);
    printInlineRing(ideal->ring);
    printf("; card=%zu; elements=[", ideal->card);
    size_t limit = ideal->card < 6 ? ideal->card : 6;
    int* indices = ideal->type == IDEAL_INDEXED ? ideal->data.indexed.indices : NULL;
    for (size_t i = 0; i < limit; i++) {
        if (i) printf(", ");
        int idx = indices ? indices[i] : -1;
        RingElement* element = (ideal->ring && idx >= 0 && (size_t)idx < ideal->ring->card)
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

static void printInlineConjugacyClass(ConjugacyClass* class) {
    if (!class) {
        printf("<conjugacyClass null>");
        return;
    }
    printf("<conjugacyClass size=%d; rep=%s; elements=[",
           class->size,
           (class->rep && class->rep->repr) ? class->rep->repr : "?");
    int limit = class->size < 6 ? class->size : 6;
    for (int i = 0; i < limit; i++) {
        if (i) printf(", ");
        int idx = class->indices ? class->indices[i] : -1;
        GroupElement* element = (class->group && idx >= 0 && idx < class->group->card)
            ? class->group->elements[idx]
            : NULL;
        printf("%s", (element && element->repr) ? element->repr : "?");
    }
    if (class->size > limit) printf(", ...");
    printf("]>");
}

static void printInlineRepresentation(Representation* rep) {
    if (!rep) {
        printf("<representation null>");
        return;
    }
    if (rep->group) {
        printf("<representation %s; dim=%d; matrixDim=%d; groupCard=%zu>",
               rep->repr ? rep->repr : "?",
               rep->dim,
               rep->mdim,
               rep->group->card);
    } else {
        printf("<representation %s; dim=%d; matrixDim=%d; groupCard=-1>",
               rep->repr ? rep->repr : "?",
               rep->dim,
               rep->mdim);
    }
}

static void printInlineCharacter(Character* chi) {
    if (!chi) {
        printf("<character null>");
        return;
    }
    if (chi->group) {
        printf("<character %s; classes=%d; groupCard=%zu>",
               chi->repr ? chi->repr : "?",
               chi->numClasses,
               chi->group->card);
    } else {
        printf("<character %s; classes=%d; groupCard=-1>",
               chi->repr ? chi->repr : "?",
               chi->numClasses);
    }
}

static void freeCharacterDeep(Character* chi) {
    if (!chi) return;
    for (int i = 0; i < chi->numClasses; i++) freeConjugacyClass(chi->classes[i]);
    freeCharacter(chi);
}

static Character* cloneCharacterDeep(Character* chi) {
    if (!chi) return NULL;

    int n = chi->numClasses;
    ConjugacyClass** classesCopy = malloc((size_t)n * sizeof(ConjugacyClass*));
    if (!classesCopy) return NULL;

    for (int i = 0; i < n; i++) {
        classesCopy[i] = constructConjugacyClass(chi->classes[i]->group, chi->classes[i]->rep);
        if (!classesCopy[i]) {
            for (int j = 0; j < i; j++) freeConjugacyClass(classesCopy[j]);
            free(classesCopy);
            return NULL;
        }
    }

    ComplexNumber* valuesCopy = malloc((size_t)n * sizeof(ComplexNumber));
    if (!valuesCopy) {
        for (int i = 0; i < n; i++) freeConjugacyClass(classesCopy[i]);
        free(classesCopy);
        return NULL;
    }
    for (int i = 0; i < n; i++) valuesCopy[i] = chi->values[i];

    Character* copy = constructCharacter(
        chi->group,
        chi->repr ? chi->repr : "chi",
        classesCopy,
        valuesCopy,
        n
    );
    if (!copy) {
        for (int i = 0; i < n; i++) freeConjugacyClass(classesCopy[i]);
        free(classesCopy);
        free(valuesCopy);
    }
    return copy;
}

static void printInlineCharacterTable(CharacterTable* table) {
    if (!table) {
        printf("<characterTable null>");
        return;
    }
    if (table->group) {
        printf("<characterTable classes=%d; irreps=%d; groupCard=%zu>",
               table->numClasses,
               table->numIrreps,
               table->group->card);
    } else {
        printf("<characterTable classes=%d; irreps=%d; groupCard=-1>",
               table->numClasses,
               table->numIrreps);
    }
}

static void freeCharacterTableDeep(CharacterTable* table) {
    if (!table) return;
    for (int k = 0; k < table->numIrreps; k++) freeCharacter(table->irreps[k]);
    for (int i = 0; i < table->numClasses; i++) freeConjugacyClass(table->classes[i]);
    freeCharacterTable(table);
}

static CharacterTable* cloneCharacterTableDeep(CharacterTable* table) {
    if (!table) return NULL;

    int r = table->numClasses;
    int m = table->numIrreps;

    ConjugacyClass** classesCopy = malloc((size_t)r * sizeof(ConjugacyClass*));
    if (!classesCopy) return NULL;
    for (int i = 0; i < r; i++) {
        classesCopy[i] = constructConjugacyClass(table->classes[i]->group, table->classes[i]->rep);
        if (!classesCopy[i]) {
            for (int j = 0; j < i; j++) freeConjugacyClass(classesCopy[j]);
            free(classesCopy);
            return NULL;
        }
    }

    Character** irrepsCopy = malloc((size_t)m * sizeof(Character*));
    if (!irrepsCopy) {
        for (int i = 0; i < r; i++) freeConjugacyClass(classesCopy[i]);
        free(classesCopy);
        return NULL;
    }

    for (int k = 0; k < m; k++) irrepsCopy[k] = NULL;
    for (int k = 0; k < m; k++) {
        ConjugacyClass** chiClasses = malloc((size_t)r * sizeof(ConjugacyClass*));
        ComplexNumber* chiValues = malloc((size_t)r * sizeof(ComplexNumber));
        if (!chiClasses || !chiValues) {
            free(chiClasses);
            free(chiValues);
            for (int j = 0; j < k; j++) freeCharacter(irrepsCopy[j]);
            free(irrepsCopy);
            for (int i = 0; i < r; i++) freeConjugacyClass(classesCopy[i]);
            free(classesCopy);
            return NULL;
        }
        for (int i = 0; i < r; i++) {
            chiClasses[i] = classesCopy[i];
            chiValues[i] = table->irreps[k]->values[i];
        }
        irrepsCopy[k] = constructCharacter(
            table->irreps[k]->group,
            table->irreps[k]->repr ? table->irreps[k]->repr : "chi",
            chiClasses,
            chiValues,
            r
        );
        if (!irrepsCopy[k]) {
            free(chiClasses);
            free(chiValues);
            for (int j = 0; j < k; j++) freeCharacter(irrepsCopy[j]);
            free(irrepsCopy);
            for (int i = 0; i < r; i++) freeConjugacyClass(classesCopy[i]);
            free(classesCopy);
            return NULL;
        }
    }

    ComplexNumber** valuesCopy = malloc((size_t)m * sizeof(ComplexNumber*));
    if (!valuesCopy) {
        for (int k = 0; k < m; k++) freeCharacter(irrepsCopy[k]);
        free(irrepsCopy);
        for (int i = 0; i < r; i++) freeConjugacyClass(classesCopy[i]);
        free(classesCopy);
        return NULL;
    }
    for (int k = 0; k < m; k++) {
        valuesCopy[k] = malloc((size_t)r * sizeof(ComplexNumber));
        if (!valuesCopy[k]) {
            for (int j = 0; j < k; j++) free(valuesCopy[j]);
            free(valuesCopy);
            for (int kk = 0; kk < m; kk++) freeCharacter(irrepsCopy[kk]);
            free(irrepsCopy);
            for (int i = 0; i < r; i++) freeConjugacyClass(classesCopy[i]);
            free(classesCopy);
            return NULL;
        }
        for (int i = 0; i < r; i++) valuesCopy[k][i] = table->values[k][i];
    }

    CharacterTable* copy = constructCharacterTable(
        table->group,
        classesCopy,
        irrepsCopy,
        valuesCopy,
        r,
        m
    );
    if (!copy) {
        for (int k = 0; k < m; k++) free(valuesCopy[k]);
        free(valuesCopy);
        for (int k = 0; k < m; k++) freeCharacter(irrepsCopy[k]);
        free(irrepsCopy);
        for (int i = 0; i < r; i++) freeConjugacyClass(classesCopy[i]);
        free(classesCopy);
        return NULL;
    }
    return copy;
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

static void printInlineProbabilityDistribution(ProbabilityDistribution* dist) {
    if (!dist) {
        printf("<probabilityDistribution null>");
        return;
    }
    const char* type = dist->type == KUMA_DIST_DISCRETE ? "discrete" : "continuous";
    printf("<probabilityDistribution %s; kind=%d>", type, dist->kind);
}

static void printInlineRandomVariable(RandomVariable* rv) {
    if (!rv) {
        printf("<randomVariable null>");
        return;
    }
    printf("<randomVariable %s; distribution=",
           rv->name ? rv->name : "?");
    printInlineProbabilityDistribution(rv->distribution);
    printf(">");
}

static void printInlineQuaternionMatrixRep(ValueQuaternionMatrixRep* rep) {
    if (!rep || !quaternionMatrixRepIsUsable(&rep->rep)) {
        printf("<quaternionMatrixRep null>");
        return;
    }
    char* algebra = cdAlgebraToString(&rep->algebra);
    printf("<quaternionMatrixRep algebra=%s>", algebra ? algebra : "<invalid>");
    free(algebra);
}

/* ---------- Construct methods ---------- */

static long double zeroTiny(long double x) {
    return fabsl(x) < 1e-15 ? 0.0 : x;
}

static bool matrixHasValidEntries(Matrix* matrix) {
    if (!matrix) return true;
    for (size_t r = 0; r < matrix->numRows; r++) {
        for (size_t c = 0; c < matrix->numCols; c++) {
            FieldElement elem = getEntry(matrix, r, c);
            bool valid = fieldElementIsValid(&elem)
                && matrix->field != NULL
                && fieldEq(matrix->field, elem.field);
            freeFieldElement(&elem);
            if (!valid) return false;
        }
    }
    return true;
}

Value valNone(void)               { Value v = {0}; v.kind = VAL_NONE;     return v; }
Value valError(const char* msg)   { Value v = {0}; v.kind = VAL_ERROR;    v.as.str = dupstr(msg); return v; }
Value valBool(bool b)             { Value v = {0}; v.kind = VAL_BOOL;     v.as.b = b;             return v; }
Value valInt(long long n)         { Value v = {0}; v.kind = VAL_INT;      v.as.i = n;             return v; }
Value valDecimal(long double x)        { if (!isfinite(x)) return valError("numeric overflow or undefined decimal result"); Value v = {0}; v.kind = VAL_DECIMAL;  v.as.d = zeroTiny(x);   return v; }
Value valFraction(Fraction f)     { Value v = {0}; v.kind = VAL_FRACTION; v.as.frac = f;          return v; }
Value valComplex(ComplexNumber c) { if (!isfinite(c.real) || !isfinite(c.imag)) return valError("numeric overflow or undefined complex result"); Value v = {0}; v.kind = VAL_COMPLEX;  c.real = zeroTiny(c.real); c.imag = zeroTiny(c.imag); v.as.cplx = c; return v; }
Value valString(const char* s)    { Value v = {0}; v.kind = VAL_STRING;   v.as.str = dupstr(s);   return v; }
Value valSymbol(const char* s)    { Value v = {0}; v.kind = VAL_SYMBOL;   v.as.str = dupstr(s);   return v; }

Value valList(Value* items, size_t n) {
    Value v = {0};
    v.kind = VAL_LIST;
    v.as.list.items = items;
    v.as.list.n     = n;
    return v;
}

Value valField(Field field) {
    ValueField* copy = cloneValueField(&field);
    if (!copy) return valError("invalid field");
    Value v = {0};
    v.kind = VAL_FIELD;
    v.as.ptr = copy;
    return v;
}

Value valFieldElement(FieldElement element) {
    ValueFieldElement* copy = cloneValueFieldElement(element);
    if (!copy) return valError("invalid field element");
    Value v = {0};
    v.kind = VAL_FIELD_ELEMENT;
    v.as.ptr = copy;
    return v;
}

Value valCDAlgebra(CDAlgebra algebra) {
    ValueCDAlgebra* copy = cloneValueCDAlgebra(algebra);
    if (!copy) return valError("invalid Cayley-Dickson algebra");
    Value v = {0};
    v.kind = VAL_CD_ALGEBRA;
    v.as.ptr = copy;
    return v;
}

Value valCDElement(CDElement element) {
    ValueCDElement* copy = cloneValueCDElement(element);
    if (!copy) return valError("invalid Cayley-Dickson element");
    Value v = {0};
    v.kind = VAL_CD_ELEMENT;
    v.as.ptr = copy;
    return v;
}

Value valCDIdeal(CDIdeal ideal) {
    ValueCDIdeal* copy = cloneValueCDIdeal(ideal);
    if (!copy) return valError("invalid Cayley-Dickson ideal");
    Value v = {0};
    v.kind = VAL_CD_IDEAL;
    v.as.ptr = copy;
    return v;
}

Value valCDSubalgebra(CDSubalgebra subalgebra) {
    ValueCDSubalgebra* copy = cloneValueCDSubalgebra(subalgebra);
    if (!copy) return valError("invalid Cayley-Dickson subalgebra");
    Value v = {0};
    v.kind = VAL_CD_SUBALGEBRA;
    v.as.ptr = copy;
    return v;
}

Value valQuaternionMatrixRep(QuaternionMatrixRep rep) {
    ValueQuaternionMatrixRep* copy = cloneValueQuaternionMatrixRep(rep);
    if (!copy) return valError("invalid Quaternion matrix representation");
    Value v = {0};
    v.kind = VAL_QUATERNION_MATRIX_REP;
    v.as.ptr = copy;
    return v;
}

Value valPtr(ValueKind kind, void* p) {
    if ((kind == VAL_MATRIX || kind == VAL_VECTOR) && !matrixHasValidEntries((Matrix*)p)) {
        freeMatrix((Matrix*)p);
        return valError("numeric overflow or undefined matrix/vector result");
    }
    Value v = {0};
    v.kind = kind;
    v.as.ptr = p;
    return v;
}

/* ---------- Free and clone ---------- */

// Free a Value's own payload; see note in value.h
void valFree(Value v) {
    switch (v.kind) {
        case VAL_MATRIX:
        case VAL_VECTOR:
            freeMatrix((Matrix*)v.as.ptr);
            break;
        case VAL_COMBSET:
            if (v.as.ptr) freeCombset((CombSet*)v.as.ptr);
            break;
        case VAL_CONJUGACY_CLASS:
            if (v.as.ptr) freeConjugacyClass((ConjugacyClass*)v.as.ptr);
            break;
        case VAL_REPRESENTATION:
            if (v.as.ptr) freeRepresentation((Representation*)v.as.ptr);
            break;
        case VAL_CHARACTER:
            if (v.as.ptr) freeCharacterDeep((Character*)v.as.ptr);
            break;
        case VAL_CHARACTER_TABLE:
            if (v.as.ptr) freeCharacterTableDeep((CharacterTable*)v.as.ptr);
            break;
        case VAL_NEKO_EXPR:
            if (v.as.ptr) nekoFreeExpr((NekoExpr*)v.as.ptr);
            break;
        case VAL_FIELD: {
            freeValueField((ValueField*)v.as.ptr);
            break;
        }
        case VAL_FIELD_ELEMENT: {
            ValueFieldElement* element = (ValueFieldElement*)v.as.ptr;
            if (element) {
                freeFieldElement(&element->element);
                freeValueField(element->field);
                free(element);
            }
            break;
        }
        case VAL_CD_ALGEBRA: {
            ValueCDAlgebra* algebra = (ValueCDAlgebra*)v.as.ptr;
            if (algebra) {
                freeCDAlgebra(&algebra->algebra);
                freeValueField(algebra->field);
                free(algebra);
            }
            break;
        }
        case VAL_CD_ELEMENT: {
            ValueCDElement* element = (ValueCDElement*)v.as.ptr;
            if (element) {
                freeCDElement(&element->element);
                freeCDAlgebra(&element->algebra);
                freeValueField(element->field);
                free(element);
            }
            break;
        }
        case VAL_CD_IDEAL: {
            ValueCDIdeal* ideal = (ValueCDIdeal*)v.as.ptr;
            if (ideal) {
                freeCDIdeal(&ideal->ideal);
                freeCDAlgebra(&ideal->algebra);
                freeValueField(ideal->field);
                free(ideal);
            }
            break;
        }
        case VAL_CD_SUBALGEBRA: {
            ValueCDSubalgebra* subalgebra = (ValueCDSubalgebra*)v.as.ptr;
            if (subalgebra) {
                freeCDSubalgebra(&subalgebra->subalgebra);
                freeCDAlgebra(&subalgebra->algebra);
                freeValueField(subalgebra->field);
                free(subalgebra);
            }
            break;
        }
        case VAL_QUATERNION_MATRIX_REP: {
            ValueQuaternionMatrixRep* rep = (ValueQuaternionMatrixRep*)v.as.ptr;
            if (rep) {
                freeQuaternionMatrixRep(&rep->rep);
                freeCDAlgebra(&rep->algebra);
                freeValueField(rep->field);
                free(rep);
            }
            break;
        }
        case VAL_PROBABILITY_DISTRIBUTION:
            if (v.as.ptr) freeProbabilityDistribution((ProbabilityDistribution*)v.as.ptr);
            break;
        case VAL_RANDOM_VARIABLE:
            if (v.as.ptr) freeRandomVariable((RandomVariable*)v.as.ptr);
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
        case VAL_CONJUGACY_CLASS: {
            ConjugacyClass* class = (ConjugacyClass*)v.as.ptr;
            if (!class) return valPtr(VAL_CONJUGACY_CLASS, NULL);
            return valPtr(VAL_CONJUGACY_CLASS,
                          constructConjugacyClass(class->group, class->rep));
        }
        case VAL_REPRESENTATION: {
            Representation* rep = (Representation*)v.as.ptr;
            if (!rep) return valPtr(VAL_REPRESENTATION, NULL);
            Group* group = rep->group;
            if (!group || !rep->images) return valPtr(VAL_REPRESENTATION, NULL);

            Matrix** images = malloc(group->card * sizeof(Matrix*));
            if (!images) return valPtr(VAL_REPRESENTATION, NULL);

            for (int i = 0; i < group->card; i++) {
                images[i] = copyMatrix(rep->images[i]);
                if (!images[i]) {
                    for (int j = 0; j < i; j++) freeMatrix(images[j]);
                    free(images);
                    return valPtr(VAL_REPRESENTATION, NULL);
                }
            }

            Representation* copy = constructRepresentation(
                group,
                rep->repr ? rep->repr : "rep",
                images,
                rep->mdim,
                rep->dim
            );
            if (!copy) {
                for (int i = 0; i < group->card; i++) freeMatrix(images[i]);
                free(images);
            }
            return valPtr(VAL_REPRESENTATION, copy);
        }
        case VAL_CHARACTER:
            return valPtr(VAL_CHARACTER,
                          v.as.ptr ? cloneCharacterDeep((Character*)v.as.ptr) : NULL);
        case VAL_CHARACTER_TABLE:
            return valPtr(VAL_CHARACTER_TABLE,
                          v.as.ptr ? cloneCharacterTableDeep((CharacterTable*)v.as.ptr) : NULL);
        case VAL_NEKO_EXPR:
            return valPtr(VAL_NEKO_EXPR,
                          v.as.ptr ? nekoCloneExpr((NekoExpr*)v.as.ptr) : NULL);
        case VAL_FIELD: {
            ValueField* field = (ValueField*)v.as.ptr;
            return field ? valField(field->field) : valPtr(VAL_FIELD, NULL);
        }
        case VAL_FIELD_ELEMENT: {
            ValueFieldElement* element = (ValueFieldElement*)v.as.ptr;
            return element ? valFieldElement(element->element) : valPtr(VAL_FIELD_ELEMENT, NULL);
        }
        case VAL_CD_ALGEBRA: {
            ValueCDAlgebra* algebra = (ValueCDAlgebra*)v.as.ptr;
            return algebra ? valCDAlgebra(algebra->algebra) : valPtr(VAL_CD_ALGEBRA, NULL);
        }
        case VAL_CD_ELEMENT: {
            ValueCDElement* element = (ValueCDElement*)v.as.ptr;
            return element ? valCDElement(element->element) : valPtr(VAL_CD_ELEMENT, NULL);
        }
        case VAL_CD_IDEAL: {
            ValueCDIdeal* ideal = (ValueCDIdeal*)v.as.ptr;
            return ideal ? valCDIdeal(ideal->ideal) : valPtr(VAL_CD_IDEAL, NULL);
        }
        case VAL_CD_SUBALGEBRA: {
            ValueCDSubalgebra* subalgebra = (ValueCDSubalgebra*)v.as.ptr;
            return subalgebra ? valCDSubalgebra(subalgebra->subalgebra) : valPtr(VAL_CD_SUBALGEBRA, NULL);
        }
        case VAL_QUATERNION_MATRIX_REP: {
            ValueQuaternionMatrixRep* rep = (ValueQuaternionMatrixRep*)v.as.ptr;
            return rep ? valQuaternionMatrixRep(rep->rep) : valPtr(VAL_QUATERNION_MATRIX_REP, NULL);
        }
        case VAL_PROBABILITY_DISTRIBUTION:
            return valPtr(VAL_PROBABILITY_DISTRIBUTION,
                          v.as.ptr ? copyProbabilityDistribution((ProbabilityDistribution*)v.as.ptr) : NULL);
        case VAL_RANDOM_VARIABLE: {
            RandomVariable* rv = (RandomVariable*)v.as.ptr;
            if (!rv) return valPtr(VAL_RANDOM_VARIABLE, NULL);
            ProbabilityDistribution* distCopy = copyProbabilityDistribution(rv->distribution);
            if (!distCopy) return valPtr(VAL_RANDOM_VARIABLE, NULL);
            RandomVariable* rvCopy = constructRandomVariable(rv->name ? rv->name : "X", distCopy, true);
            if (!rvCopy) freeProbabilityDistribution(distCopy);
            return valPtr(VAL_RANDOM_VARIABLE, rvCopy);
        }
        case VAL_STRING: return valString(v.as.str);
        case VAL_SYMBOL: return valSymbol(v.as.str);
        case VAL_ERROR:  return valError(v.as.str);
        case VAL_LIST: {
            Value* items = calloc(v.as.list.n, sizeof(Value));
            if (!items && v.as.list.n > 0) return valError("out of memory while cloning list");
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
        case VAL_NEKO_EXPR:     return "neko_expr";
        case VAL_FIELD:         return "field";
        case VAL_FIELD_ELEMENT: return "field_element";
        case VAL_CD_ALGEBRA:    return "cd_algebra";
        case VAL_CD_ELEMENT:    return "cd_element";
        case VAL_CD_IDEAL:      return "cd_ideal";
        case VAL_CD_SUBALGEBRA: return "cd_subalgebra";
        case VAL_QUATERNION_MATRIX_REP: return "quaternion_matrix_rep";
        case VAL_MATRIX:        return "matrix";
        case VAL_VECTOR:        return "vector";
        case VAL_COMBSET:       return "combset";
        case VAL_GROUP:         return "group";
        case VAL_GROUP_ELEMENT: return "group_element";
        case VAL_CONJUGACY_CLASS: return "conjugacy_class";
        case VAL_REPRESENTATION: return "representation";
        case VAL_CHARACTER: return "character";
        case VAL_CHARACTER_TABLE: return "character_table";
        case VAL_SUBGROUP:      return "subgroup";
        case VAL_GROUP_COSET:   return "group_coset";
        case VAL_GROUP_HOMOMORPHISM: return "group_homomorphism";
        case VAL_RING:          return "ring";
        case VAL_RING_ELEMENT:  return "ring_element";
        case VAL_SUBRING:       return "subring";
        case VAL_IDEAL:         return "ideal";
        case VAL_RING_HOMOMORPHISM: return "ring_homomorphism";
        case VAL_BODY:          return "body";
        case VAL_BODY_SYSTEM:   return "body_system";
        case VAL_FORCE:         return "force";
        case VAL_PROBABILITY_DISTRIBUTION: return "probability_distribution";
        case VAL_RANDOM_VARIABLE: return "random_variable";
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
        case VAL_DECIMAL:  printf("%Lg", v.as.d); break;
        case VAL_FRACTION: printFraction(v.as.frac); break;
        case VAL_COMPLEX:  printf("(%Lg + %Lgi)", v.as.cplx.real, v.as.cplx.imag); break;
        case VAL_STRING:   printf("\"%s\"", v.as.str ? v.as.str : ""); break;
        case VAL_SYMBOL:   printf("%s", v.as.str ? v.as.str : ""); break;
        case VAL_LIST:
            putchar('[');
            for (size_t i = 0; i < v.as.list.n; i++) {
                if (i) printf(", ");
                valPrint(v.as.list.items[i]);
            }
            putchar(']');
            break;
        case VAL_NEKO_EXPR:
            if (v.as.ptr) nekoPrintExpr((NekoExpr*)v.as.ptr);
            else printf("<neko_expr null>");
            break;
        case VAL_FIELD: {
            ValueField* field = (ValueField*)v.as.ptr;
            char* string = field ? fieldToString(&field->field) : NULL;
            printf("%s", string ? string : "<field null>");
            free(string);
            break;
        }
        case VAL_FIELD_ELEMENT: {
            ValueFieldElement* element = (ValueFieldElement*)v.as.ptr;
            char* string = element ? fieldElementToString(element->element) : NULL;
            printf("%s", string ? string : "<field_element null>");
            free(string);
            break;
        }
        case VAL_CD_ALGEBRA: {
            ValueCDAlgebra* algebra = (ValueCDAlgebra*)v.as.ptr;
            char* string = algebra ? cdAlgebraToString(&algebra->algebra) : NULL;
            printf("%s", string ? string : "<cd_algebra null>");
            free(string);
            break;
        }
        case VAL_CD_ELEMENT: {
            ValueCDElement* element = (ValueCDElement*)v.as.ptr;
            char* string = element ? cdElementToString(&element->element) : NULL;
            printf("%s", string ? string : "<cd_element null>");
            free(string);
            break;
        }
        case VAL_CD_IDEAL: {
            ValueCDIdeal* ideal = (ValueCDIdeal*)v.as.ptr;
            char* string = ideal ? cdIdealToString(&ideal->ideal) : NULL;
            printf("%s", string ? string : "<cd_ideal null>");
            free(string);
            break;
        }
        case VAL_CD_SUBALGEBRA: {
            ValueCDSubalgebra* subalgebra = (ValueCDSubalgebra*)v.as.ptr;
            char* string = subalgebra ? cdSubalgebraToString(&subalgebra->subalgebra) : NULL;
            printf("%s", string ? string : "<cd_subalgebra null>");
            free(string);
            break;
        }
        case VAL_QUATERNION_MATRIX_REP:
            printInlineQuaternionMatrixRep((ValueQuaternionMatrixRep*)v.as.ptr);
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
                for (size_t i = 0; i < combset->card; i++) {
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
            printf("<body mass=%Lg; pos=", body->mass);
            printInlineVector(body->pos);
            printf("; velocity=");
            printInlineVector(body->velocity);
            printf("; forces=%zu>", body->nForces);
            break;
        }
        case VAL_BODY_SYSTEM: {
            BodySystem* system = (BodySystem*)v.as.ptr;
            if (!system) { printf("<bodySystem null>"); break; }
            printf("<bodySystem bodies=%zu; sample=[", system->nBodies);
            size_t limit = system->nBodies < 3 ? system->nBodies : 3;
            for (size_t i = 0; i < limit; i++) {
                if (i) printf(", ");
                Body* body = system->bodies ? system->bodies[i] : NULL;
                if (!body) {
                    printf("null");
                    continue;
                }
                printf("{m=%Lg,pos=", body->mass);
                printInlineVector(body->pos);
                printf("}");
            }
            if (system->nBodies > limit) printf(", ...");
            printf("]>");
            break;
        }
        case VAL_FORCE: {
            Force* force = (Force*)v.as.ptr;
            if (!force) { printf("<force null>"); break; }
            printf("<force \"%s\": vector=", force->name ? force->name : "");
            printInlineVector(force->vector);
            printf("; tailPos=");
            printInlineVector(force->tailPos);
            printf("; magnitude=%Lg>", force->vector ? l2Norm(force->vector) : 0.0);
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
        case VAL_CONJUGACY_CLASS:
            printInlineConjugacyClass((ConjugacyClass*)v.as.ptr);
            break;
        case VAL_REPRESENTATION:
            printInlineRepresentation((Representation*)v.as.ptr);
            break;
        case VAL_CHARACTER:
            printInlineCharacter((Character*)v.as.ptr);
            break;
        case VAL_CHARACTER_TABLE:
            printInlineCharacterTable((CharacterTable*)v.as.ptr);
            break;
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
        case VAL_PROBABILITY_DISTRIBUTION:
            printInlineProbabilityDistribution((ProbabilityDistribution*)v.as.ptr);
            break;
        case VAL_RANDOM_VARIABLE:
            printInlineRandomVariable((RandomVariable*)v.as.ptr);
            break;
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

// Collapse a numeric value to a long double (0.0 on non-numeric)
long double valToDouble(Value v) {
    switch (v.kind) {
        case VAL_INT:      return (long double)v.as.i;
        case VAL_DECIMAL:  return v.as.d;
        case VAL_FRACTION:
            return v.as.frac.denom ? (long double)v.as.frac.num / (long double)v.as.frac.denom : 0.0;
        case VAL_BOOL:     return v.as.b ? 1.0 : 0.0;
        default:           return 0.0;
    }
}
