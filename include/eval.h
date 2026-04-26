#ifndef BESTIARY_EVAL_H
#define BESTIARY_EVAL_H

#include<stddef.h>
#include "ast.h"
#include "value.h"

/* ---------- Environment (chained symbol table) ---------- */

typedef struct EnvEntry {
    char* name;
    Value value;
    struct EnvEntry* next;
} EnvEntry;

typedef struct Env {
    EnvEntry* head;
    struct Env* parent;
} Env;

// Construct and free methods for environments
Env*  envNew(Env* parent);
void  envFree(Env* env);

// Look up `name` walking up the parent chain. Returns 1 on hit; the
// found Value is written to *out (borrowed -- clone before freeing).
int   envGet(Env* env, const char* name, Value* out);

// Set `name` in the nearest scope that already binds it, else in env
void  envSet(Env* env, const char* name, Value v);

/* ---------- Command registry ---------- */

typedef struct EvalContext EvalContext;

typedef Value (*CommandFn)(EvalContext* ctx, Value* args, size_t nargs);

typedef struct CommandEntry {
    const char* name;            // without the leading backslash
    int arity;                   // expected number of args; -1 = variadic
    const char* doc;             // one-line help summary
    CommandFn fn;
    struct CommandEntry* next;
} CommandEntry;

// Register (or replace) `name` in the global command table. Names like
// "+", "-", "*", "/", "u-", "u+", "^", "_" are the canonical hooks for
// built-in operators; AST_MATRIX nodes dispatch to "__<tag>__" with
// "__matrix__" as a fallback.
void                registerCommand(const char* name, int arity, CommandFn fn);
const CommandEntry* lookupCommand(const char* name);
const CommandEntry* commandRegistry(void);

// Populate +, -, *, /, u-, u+, pi, frac, etc. Call once at startup
void registerBuiltins(void);

/* ---------- Evaluation context ---------- */

struct EvalContext {
    Env* env;
};

EvalContext* evalCtxNew(void);
void         evalCtxFree(EvalContext* ctx);

// Walk the AST and produce a Value. Caller owns the returned Value and
// must valFree it when done.
Value eval(EvalContext* ctx, AstNode* node);

#endif
