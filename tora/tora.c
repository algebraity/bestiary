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

// Returns an array containing the 1D representations of a Group
Representation** get1Dreps(Group* G) {
    if (!G) return NULL;

    int num1D = 0;
    for (int i = 0; i < G->card; i++) {
        if (elementOrder(G, G->elements[i]) == 1) num1D++;
    }

    Representation** reps = malloc(num1D * sizeof(Representation*));
    if (!reps) return NULL;

    int count = 0;
    for (int i = 0; i < G->card; i++) {
        if (elementOrder(G, G->elements[i]) == 1) {
            Matrix** images = malloc(G->card * sizeof(Matrix*));
            if (!images) {
                for (int j = 0; j < count; j++) freeRepresentation(reps[j]);
                free(reps);
                return NULL;
            }
            for (int j = 0; j < G->card; j++) {
                images[j] = constructMatrix(1, 1);
                if (!images[j]) {
                    for (int m = 0; m < j; m++) freeMatrix(images[m]);
                    free(images);
                    for (int m = 0; m < count; m++) freeRepresentation(reps[m]);
                    free(reps);
                    return NULL;
                }
                setEntry(images[j], 0, 0, elemFromReal((j == i) ? 1.0 : 0.0));
            }
            char repr[20];
            sprintf(repr, "C%d", count+1);
            reps[count] = constructRepresentation(G, repr, images, 1, 1);
            if (!reps[count]) {
                for (int m = 0; m < G->card; m++) freeMatrix(images[m]);
                free(images);
                for (int m = 0; m < count; m++) freeRepresentation(reps[m]);
                free(reps);
                return NULL;
            }
            count++;
        }
    }

    return reps;
}