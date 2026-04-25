#ifndef BESTIARY_VALUE_H
#define BESTIARY_VALUE_H

#include<stdbool.h>
#include<stddef.h>
#include "hebi.h"

/* ---------- BEAST opaque pointer forward-decls ---------- */
typedef struct Matrix Matrix;
typedef Matrix Vector;
typedef struct CombSet CombSet;
typedef struct Group Group;
typedef struct GroupElement GroupElement;
typedef struct SubGroup SubGroup;
typedef struct GroupCoset GroupCoset;
typedef struct GroupHomomorphism GroupHomomorphism;
typedef struct Ring Ring;
typedef struct RingElement RingElement;
typedef struct SubRing SubRing;
typedef struct Ideal Ideal;
typedef struct RingHomomorphism RingHomomorphism;
typedef struct Body Body;
typedef struct Force Force;

/* ---------- Value kinds ---------- */

typedef enum {
    VAL_NONE = 0,
    VAL_ERROR,
    VAL_BOOL,
    VAL_INT,             // long long
    VAL_DECIMAL,         // double
    VAL_FRACTION,        // Fraction (by value)
    VAL_COMPLEX,         // ComplexNumber (by value)
    VAL_STRING,          // owned char*
    VAL_SYMBOL,          // unresolved identifier
    VAL_LIST,            // tuple / variadic payload

    /* BEAST opaque pointer kinds -- add more as you wire up functions. */
    VAL_MATRIX,
    VAL_VECTOR,
    VAL_COMBSET,
    VAL_GROUP,
    VAL_GROUP_ELEMENT,
    VAL_SUBGROUP,
    VAL_GROUP_COSET,
    VAL_GROUP_HOMOMORPHISM,
    VAL_RING,
    VAL_RING_ELEMENT,
    VAL_SUBRING,
    VAL_IDEAL,
    VAL_RING_HOMOMORPHISM,
    VAL_BODY,
    VAL_FORCE
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
        double        d;
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
Value valDecimal(double x);
Value valFraction(Fraction f);
Value valComplex(ComplexNumber c);
Value valString(const char* s);
Value valSymbol(const char* name);
Value valList(Value* items, size_t n);            // takes ownership of items[]
Value valPtr(ValueKind kind, void* p);            // generic opaque-pointer ctor

/* ---------- Free, clone, introspection ---------- */

// Free the Value payload. Strings/lists are owned directly; matrices,
// vectors, and CombSets are also owned and released here. Other opaque
// BEAST pointers are still treated as borrowed until their wrappers define
// ownership.
void valFree(Value v);

// Produce an independently-freeable duplicate. Strings, lists, matrices,
// and vectors are deep-copied; other opaque pointers remain shallow.
Value valClone(Value v);

const char* valKindName(ValueKind k);
void        valPrint(Value v);

/* ---------- Numeric promotion helpers ---------- */

bool   valIsNumeric(Value v);
double valToDouble(Value v);

#endif
