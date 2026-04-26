#ifndef BESTIARY_SCRIPT_H
#define BESTIARY_SCRIPT_H

#include<stddef.h>

typedef struct EvalContext EvalContext;

typedef struct ScriptRunOptions {
    int dumpTokens;
    int dumpAst;
    int printResults;
} ScriptRunOptions;

typedef struct ScriptRunResult {
    size_t linesRead;
    size_t linesEvaluated;
    size_t errors;
} ScriptRunResult;

int bstRunScriptFile(EvalContext* ctx,
                     const char* filename,
                     const ScriptRunOptions* options,
                     ScriptRunResult* result,
                     char* error,
                     size_t errorSize);

#endif
