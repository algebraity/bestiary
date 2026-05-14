#ifndef BESTIARY_VALUE_H
#define BESTIARY_VALUE_H

#include<stdbool.h>
#include<stddef.h>
#include "hebi.h"
#include "quaternionic.h"

/* ---------- BEAST opaque pointer forward-decls ---------- */
typedef struct Matrix Matrix;
typedef Matrix Vector;
typedef struct CombSet CombSet;
typedef struct Group Group;
typedef struct GroupElement GroupElement;
typedef struct SubGroup SubGroup;
typedef struct GroupCoset GroupCoset;
typedef struct ConjugacyClass ConjugacyClass;
typedef struct Representation Representation;
typedef struct Character Character;
typedef struct CharacterTable CharacterTable;
typedef struct GroupHomomorphism GroupHomomorphism;
typedef struct Ring Ring;
typedef struct RingElement RingElement;
typedef struct SubRing SubRing;
typedef struct Ideal Ideal;
typedef struct RingHomomorphism RingHomomorphism;
typedef struct Body Body;
typedef struct BodySystem BodySystem;
typedef struct Force Force;
typedef struct NekoExpr NekoExpr;
typedef struct ProbabilityDistribution ProbabilityDistribution;
typedef struct RandomVariable RandomVariable;

/* ---------- Owned algebra wrappers ---------- */

typedef struct ValueField ValueField;

struct ValueField {
    Field field;
    ValueField* base;
};

typedef struct {
    ValueField* field;
    FieldElement element;
} ValueFieldElement;

typedef struct {
    ValueField* field;
    CDAlgebra algebra;
} ValueCDAlgebra;

typedef struct {
    ValueField* field;
    CDAlgebra algebra;
    CDElement element;
} ValueCDElement;

typedef struct {
    ValueField* field;
    CDAlgebra algebra;
    CDIdeal ideal;
} ValueCDIdeal;

typedef struct {
    ValueField* field;
    CDAlgebra algebra;
    CDSubalgebra subalgebra;
} ValueCDSubalgebra;

typedef struct {
    ValueField* field;
    CDAlgebra algebra;
    QuaternionMatrixRep rep;
} ValueQuaternionMatrixRep;

/* ---------- Value kinds ---------- */

typedef enum {
    VAL_NONE = 0,
    VAL_ERROR,
    VAL_BOOL,
    VAL_INT,             // long long
    VAL_DECIMAL,         // long double
    VAL_FRACTION,        // Fraction (by value)
    VAL_COMPLEX,         // ComplexNumber (by value)
    VAL_STRING,          // owned char*
    VAL_SYMBOL,          // unresolved identifier
    VAL_LIST,            // tuple / variadic payload
    VAL_NEKO_EXPR,       // owned Neko symbolic expression
    VAL_FIELD,           // owned HEBI Field wrapper
    VAL_FIELD_ELEMENT,   // owned HEBI FieldElement wrapper
    VAL_CD_ALGEBRA,      // owned Cayley-Dickson algebra wrapper
    VAL_CD_ELEMENT,      // owned Cayley-Dickson element wrapper
    VAL_CD_IDEAL,        // owned Cayley-Dickson ideal wrapper
    VAL_CD_SUBALGEBRA,   // owned Cayley-Dickson subalgebra wrapper
    VAL_QUATERNION_MATRIX_REP, // owned Quaternion matrix representation wrapper

    /* BEAST opaque pointer kinds -- add more as you wire up functions. */
    VAL_MATRIX,
    VAL_VECTOR,
    VAL_COMBSET,
    VAL_GROUP,
    VAL_GROUP_ELEMENT,
    VAL_CONJUGACY_CLASS,
    VAL_REPRESENTATION,
    VAL_CHARACTER,
    VAL_CHARACTER_TABLE,
    VAL_SUBGROUP,
    VAL_GROUP_COSET,
    VAL_GROUP_HOMOMORPHISM,
    VAL_RING,
    VAL_RING_ELEMENT,
    VAL_SUBRING,
    VAL_IDEAL,
    VAL_RING_HOMOMORPHISM,
    VAL_BODY,
    VAL_BODY_SYSTEM,
    VAL_FORCE,
    VAL_PROBABILITY_DISTRIBUTION,
    VAL_RANDOM_VARIABLE
} ValueKind;

/* ---------- Value struct ---------- */

typedef struct Value Value;

typedef struct {
    Value* items;
    size_t n;
} ValueList;

struct Value {
    ValueKind kind;
    union {
        bool          b;
        long long     i;
        long double        d;
        Fraction      frac;
        ComplexNumber cplx;
        char*         str;       // VAL_STRING, VAL_SYMBOL, VAL_ERROR: owned
        void*         ptr;       // VAL_MATRIX/VAL_VECTOR are owned by Value
        ValueList     list;
    } as;
};

/* ---------- Construct methods ---------- */

Value valNone(void);
Value valError(const char* msg);
Value valBool(bool b);
Value valInt(long long n);
Value valDecimal(long double x);
Value valFraction(Fraction f);
Value valComplex(ComplexNumber c);
Value valString(const char* s);
Value valSymbol(const char* name);
Value valList(Value* items, size_t n);            // takes ownership of items[]
Value valField(Field field);
Value valFieldElement(FieldElement element);
Value valCDAlgebra(CDAlgebra algebra);
Value valCDElement(CDElement element);
Value valCDIdeal(CDIdeal ideal);
Value valCDSubalgebra(CDSubalgebra subalgebra);
Value valQuaternionMatrixRep(QuaternionMatrixRep rep);
Value valPtr(ValueKind kind, void* p);            // generic opaque-pointer ctor

/* ---------- Free, clone, introspection ---------- */

// Free the Value payload; strings/lists are owned directly, and matrices,
// vectors, CombSets, fields, field elements, Cayley-Dickson objects, TORA
// conjugacy classes, TORA representations, and TORA character tables,
// probability distributions, and random variables are also owned and released
// here; other opaque BEAST pointers are still treated as borrowed until their
// wrappers define ownership
void valFree(Value v);

// Produce an independently-freeable duplicate; strings, lists, matrices,
// vectors, fields, field elements, Cayley-Dickson objects, conjugacy classes,
// representations, character tables, probability distributions, and random
// variables are deep-copied; other opaque pointers remain shallow
Value valClone(Value v);

const char* valKindName(ValueKind k);
void        valPrint(Value v);

/* ---------- Numeric promotion helpers ---------- */

bool   valIsNumeric(Value v);
long double valToDouble(Value v);

#endif
