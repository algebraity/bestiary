#ifndef TORA_H
#define TORA_H

#include<stdbool.h>
#include "hebi.h"
#include "sokko.h"
#include "usagi.h"

/* ---------- Definitions of structs ---------- */

typedef struct ConjugacyClass ConjugacyClass;
typedef struct Representation Representation;
typedef struct Character Character;
typedef struct CharacterTable CharacterTable;

struct ConjugacyClass {
    Group* group;
    GroupElement* rep;
    int* indices;
    int size;
    int elementOrder;
};

struct Representation {
    char* repr;
    Group* group;
    Matrix** images;
    int mdim;
    int dim;
};

struct Character {
    char* repr;
    Group* group;
    ConjugacyClass** classes;
    ComplexNumber* values;
    int numClasses;
};

struct CharacterTable {
    Group* group;
    ConjugacyClass** classes;
    Character** irreps;
    ComplexNumber** values;
    int numClasses;
    int numIrreps;
};


/* ---------- Construct and free methods ---------- */
ConjugacyClass* constructConjugacyClass(Group* group, GroupElement* rep);
void freeConjugacyClass(ConjugacyClass* class);
Representation* constructRepresentation(Group* group, char* repr, Matrix** images, int mdim, int dim);
void freeRepresentation(Representation* rep);
Character* constructCharacter(Group* group, char* repr, ConjugacyClass** classes, ComplexNumber* values, int numClasses);
void freeCharacter(Character* character);
CharacterTable* constructCharacterTable(Group* group, ConjugacyClass** classes, Character** irreps, ComplexNumber** values, int numClasses, int numIrreps);
void freeCharacterTable(CharacterTable* table);


/* ---------- ConjugacyClass helpers ---------- */
ConjugacyClass* getConjugacyClass(GroupElement* element);
ConjugacyClass** getConjugacyClasses(Group* group, int* numClasses);

/* ---------- Representations ---------- */
Representation* trivialRepresentation(Group* G);
Representation* regularRepresentation(Group* G);
Representation* permutationRepresentation(Group* G);
Representation* standardRepresentation(Group* G);
Representation* signRepresentation(Group* Sn);
Representation* dualRepresentation(Representation* V);
Representation* conjugateRepresentation(Representation* V);
GroupHomomorphism* projectToAbelianization(Group* G);
Representation** get1Dreps(Group* G, int* count);
Representation* crossProductReps(Representation* V, Representation* W, Group* GxH);
Representation* tensorProduct(Representation* V, Representation* W);
Representation* symmetricProduct(Representation* V);
Representation* wedgeProduct(Representation* V);
Representation* restrictRepresentation(Representation* V, SubGroup* H, Group* Hgroup);
Representation* inducedRepresentation(Representation* V, SubGroup* H);

/* ---------- Characters and character tables ---------- */
ComplexNumber characterValue(Representation* rep, ConjugacyClass* class);
Character* characterOfRepresentation(Representation* rep);
ComplexNumber characterInnerProduct(Character* chi, Character* psi);
bool isIrreducible(Representation* rep);
int* decomposeRepresentation(Representation* V, CharacterTable* T);
Character** allIrreducibleCharacters(Group* G, int* count);
CharacterTable* characterTable(Group* G);
void printCharacterTable(CharacterTable* T);

#endif
