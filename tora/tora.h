#ifndef TORA_H
#define TORA_H

#include<stdbool.h>
#include "hebi.h"
#include "usagi.h"

/* ---------- Definitions of structs ---------- */

typedef struct ComplexMatrix ComplexMatrix;
typedef struct CyclotomicField CyclotomicField;
typedef struct CyclotomicElement CyclotomicElement;
typedef struct ConjugacyClass ConjugacyClass;
typedef struct Representation Representation;
typedef struct Character Character;
typedef struct CharacterTable CharacterTable;


struct ComplexMatrix {
    void** data;
    int numRows;
    int numCols;
};

struct CyclotomicField {
    int n;
};

struct CyclotomicElement {
    CyclotomicField field;
    Fraction* coeffs;
    int len;
};

struct ConjugacyClass {
    Group* group;
    int* indices;
    int size;
    int elementOrder;
};

struct Representation {
    char* repr;
    Group* group;
    ComplexMatrix** images;
    int dim;
};

struct Character {
    char* repr;
    Group* group;
    ConjugacyClass** classes;
    CyclotomicElement** values;
    int numClasses;
};

struct CharacterTable {
    Group* group;
    ConjugacyClass** classes;
    Character** irreps;
    int numIrreps;
    CyclotomicField* valueField
};

#endif