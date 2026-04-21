#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<math.h>
#include "hebi.h"
#include "usagi.h"

/* ---------- Construct and free methods ---------- */

// Constructs a ConjugacyClass struct given a representative GroupElement and a Group
ConjugacyClass* constructConjugacyClass(Group* group, GroupElement* rep) {
    if (!group || !rep) return NULL;

    ConjugacyClass* class = malloc(sizeof(ConjugacyClass));
    if (!class) return NULL;

    class->group = group;
    class->rep = rep;
    class->elementOrder = elementOrder(rep);
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
Representation* constructRepresentation(Group* group, char* repr, Matrix** images, int dim) {
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
    rep->dim = dim;

    return rep;
}

// Free a Representation struct
void freeRepresentation(Representation* rep) {
    if (!rep) return;
    free(rep->repr);
    for (int i = 0; i < rep->group->card; i++) freeMatrix(rep->images[i]);
    free(rep->images);
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
    for (int i = 0; i < table->numIrreps; i++) free(table->values[i]);
    free(table->values);
    free(table);
}


/* ----------  ---------- */

// Gets the ConjugacyClasses of a Group
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