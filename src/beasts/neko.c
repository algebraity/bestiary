#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<math.h>
#include<limits.h>
#include<stdarg.h>
#include "neko.h"
#include "hebi.h"

/* ---------- Helper methods ---------- */

// Duplicate a string for NEKO-owned names
static char* dupstr(const char* s) {
    // Reject missing input before measuring the string
    if (!s) return NULL;

    // Allocate exactly enough space for the string and terminator
    size_t n = strlen(s);
    char* out = malloc(n + 1);
    if (!out) return NULL;

    // Copy the full byte sequence including the terminator
    memcpy(out, s, n + 1);
    return out;
}

// Check whether a size multiplication can be represented
static bool checkedSizeMul(size_t a, size_t b, size_t* out) {
    // Reject missing output storage
    if (!out) return false;

    // Detect overflow before computing the product
    if (a != 0 && b > ((size_t)-1) / a) return false;

    // Store the checked product for the caller
    *out = a * b;
    return true;
}

typedef struct {
    char* data;
    size_t len;
    size_t cap;
} NekoStringBuf;

// Reserve room for additional bytes in a string buffer
static bool nekoStringBufReserve(NekoStringBuf* buf, size_t extra) {
    // Require a valid buffer before checking capacity
    if (!buf) return false;

    // Grow enough to include the requested bytes and terminator
    size_t needed = buf->len + extra + 1;
    if (needed <= buf->cap) return true;

    // Double capacity until it can hold the requested content
    size_t nextCap = buf->cap ? buf->cap * 2 : 64;
    while (nextCap < needed) nextCap *= 2;
    char* next = realloc(buf->data, nextCap);
    if (!next) return false;

    // Store the grown buffer and capacity
    buf->data = next;
    buf->cap = nextCap;
    return true;
}

// Append raw text to a string buffer
static bool nekoStringBufAppendText(NekoStringBuf* buf, const char* text) {
    // Treat missing text as an empty string
    size_t n = text ? strlen(text) : 0;
    if (!nekoStringBufReserve(buf, n)) return false;

    // Copy the bytes and restore the terminator
    if (n) memcpy(buf->data + buf->len, text, n);
    buf->len += n;
    buf->data[buf->len] = '\0';
    return true;
}

// Append one character to a string buffer
static bool nekoStringBufAppendChar(NekoStringBuf* buf, char ch) {
    // Reserve one byte and then write it
    if (!nekoStringBufReserve(buf, 1)) return false;
    buf->data[buf->len++] = ch;
    buf->data[buf->len] = '\0';
    return true;
}

// Append formatted text to a string buffer
static bool nekoStringBufAppendFormat(NekoStringBuf* buf, const char* fmt, ...) {
    // Measure the formatted output before reserving storage
    va_list args;
    va_start(args, fmt);
    va_list copy;
    va_copy(copy, args);
    int needed = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (needed < 0 || !nekoStringBufReserve(buf, (size_t)needed)) {
        va_end(args);
        return false;
    }

    // Write the formatted bytes into the reserved tail
    vsnprintf(buf->data + buf->len, buf->cap - buf->len, fmt, args);
    va_end(args);
    buf->len += (size_t)needed;
    return true;
}

// Allocate a blank expression node of the requested kind
static NekoExpr* newExpr(NekoExprKind kind) {
    // Zero-initialize the expression so inactive union fields are clean
    NekoExpr* expr = calloc(1, sizeof(NekoExpr));

    // Record the requested expression kind when allocation succeeds
    if (expr) expr->kind = kind;
    return expr;
}

// Test whether an expression is a constant near the requested value
static bool isConst(const NekoExpr* expr, long double value) {
    return expr
        && expr->kind == NEKO_EXPR_CONST
        && fabsl(expr->as.constant - value) <= 1e-12;
}

// Test whether an expression is exactly the named variable
static bool sameVar(const NekoExpr* expr, const char* var) {
    return expr
        && expr->kind == NEKO_EXPR_VAR
        && expr->as.var
        && var
        && strcmp(expr->as.var, var) == 0;
}

// Test whether an expression kind is valid for unary construction
static bool isUnaryKind(NekoExprKind kind) {
    // Accept exactly the expression kinds with one owned child
    switch (kind) {
        case NEKO_EXPR_NEG:
        case NEKO_EXPR_SIN:
        case NEKO_EXPR_COS:
        case NEKO_EXPR_TAN:
        case NEKO_EXPR_ASIN:
        case NEKO_EXPR_ACOS:
        case NEKO_EXPR_ATAN:
        case NEKO_EXPR_EXP:
        case NEKO_EXPR_LOG:
        case NEKO_EXPR_SQRT:
        case NEKO_EXPR_ABS:
        case NEKO_EXPR_ERF:
        case NEKO_EXPR_EI:
        case NEKO_EXPR_STEP:
            return true;
        default:
            return false;
    }
}

// Test whether an expression kind is valid for binary construction
static bool isBinaryKind(NekoExprKind kind) {
    // Accept exactly the expression kinds with two owned children
    switch (kind) {
        case NEKO_EXPR_ADD:
        case NEKO_EXPR_SUB:
        case NEKO_EXPR_MUL:
        case NEKO_EXPR_DIV:
        case NEKO_EXPR_POW:
            return true;
        default:
            return false;
    }
}

// Compare two expression trees structurally
static bool exprEqual(const NekoExpr* a, const NekoExpr* b);

/* ---------- Constructors ---------- */

// Construct a constant expression
NekoExpr* nekoConst(long double c) {
    // Allocate the node and fill in the numeric payload
    NekoExpr* expr = newExpr(NEKO_EXPR_CONST);
    if (expr) expr->as.constant = c;
    return expr;
}

// Construct a variable expression
NekoExpr* nekoVar(const char* name) {
    // Allocate the node and default missing names to x
    NekoExpr* expr = newExpr(NEKO_EXPR_VAR);
    if (expr) expr->as.var = dupstr(name ? name : "x");
    return expr;
}

// Construct a unary expression and take ownership of its argument
NekoExpr* nekoUnary(NekoExprKind kind, NekoExpr* arg) {
    // Reject invalid input and release any argument ownership we received
    if (!arg || !isUnaryKind(kind)) {
        nekoFreeExpr(arg);
        return NULL;
    }

    // Allocate the wrapper node or free the child if allocation fails
    NekoExpr* expr = newExpr(kind);
    if (!expr) {
        nekoFreeExpr(arg);
        return NULL;
    }

    // Attach the owned child to the unary node
    expr->as.unary.arg = arg;
    return expr;
}

// Construct a binary expression and take ownership of both operands
NekoExpr* nekoBinary(NekoExprKind kind, NekoExpr* lhs, NekoExpr* rhs) {
    // Reject invalid input and release any operand ownership we received
    if (!lhs || !rhs || !isBinaryKind(kind)) {
        nekoFreeExpr(lhs);
        nekoFreeExpr(rhs);
        return NULL;
    }

    // Allocate the wrapper node or free both operands if allocation fails
    NekoExpr* expr = newExpr(kind);
    if (!expr) {
        nekoFreeExpr(lhs);
        nekoFreeExpr(rhs);
        return NULL;
    }

    // Attach the owned operands to the binary node
    expr->as.binary.lhs = lhs;
    expr->as.binary.rhs = rhs;
    return expr;
}

// Construct an uninterpreted function call expression
NekoExpr* nekoCall(const char* name, NekoExpr** args, int nargs) {
    // Reject invalid call metadata before taking ownership of the argument array
    if (!name || nargs < 0) return NULL;

    // Allocate the call node and duplicate the function name
    NekoExpr* expr = newExpr(NEKO_EXPR_CALL);
    if (!expr) return NULL;
    expr->as.call.name = dupstr(name);

    // Store the owned argument array exactly as supplied
    expr->as.call.args = args;
    expr->as.call.nargs = nargs;
    return expr;
}

// Construct an addition expression
NekoExpr* nekoAdd(NekoExpr* lhs, NekoExpr* rhs) { return nekoBinary(NEKO_EXPR_ADD, lhs, rhs); }
// Construct a subtraction expression
NekoExpr* nekoSub(NekoExpr* lhs, NekoExpr* rhs) { return nekoBinary(NEKO_EXPR_SUB, lhs, rhs); }
// Construct a multiplication expression
NekoExpr* nekoMul(NekoExpr* lhs, NekoExpr* rhs) { return nekoBinary(NEKO_EXPR_MUL, lhs, rhs); }
// Construct a division expression
NekoExpr* nekoDiv(NekoExpr* lhs, NekoExpr* rhs) { return nekoBinary(NEKO_EXPR_DIV, lhs, rhs); }
// Construct a power expression
NekoExpr* nekoPow(NekoExpr* lhs, NekoExpr* rhs) { return nekoBinary(NEKO_EXPR_POW, lhs, rhs); }
// Construct a negation expression
NekoExpr* nekoNeg(NekoExpr* arg) { return nekoUnary(NEKO_EXPR_NEG, arg); }
// Construct a sine expression
NekoExpr* nekoSin(NekoExpr* arg) { return nekoUnary(NEKO_EXPR_SIN, arg); }
// Construct a cosine expression
NekoExpr* nekoCos(NekoExpr* arg) { return nekoUnary(NEKO_EXPR_COS, arg); }
// Construct a tangent expression
NekoExpr* nekoTan(NekoExpr* arg) { return nekoUnary(NEKO_EXPR_TAN, arg); }
// Construct an inverse-sine expression
NekoExpr* nekoAsin(NekoExpr* arg) { return nekoUnary(NEKO_EXPR_ASIN, arg); }
// Construct an inverse-cosine expression
NekoExpr* nekoAcos(NekoExpr* arg) { return nekoUnary(NEKO_EXPR_ACOS, arg); }
// Construct an inverse-tangent expression
NekoExpr* nekoAtan(NekoExpr* arg) { return nekoUnary(NEKO_EXPR_ATAN, arg); }
// Construct an exponential expression
NekoExpr* nekoExp(NekoExpr* arg) { return nekoUnary(NEKO_EXPR_EXP, arg); }
// Construct a natural-log expression
NekoExpr* nekoLog(NekoExpr* arg) { return nekoUnary(NEKO_EXPR_LOG, arg); }
// Construct a square-root expression
NekoExpr* nekoSqrt(NekoExpr* arg) { return nekoUnary(NEKO_EXPR_SQRT, arg); }
// Construct an absolute-value expression
NekoExpr* nekoAbs(NekoExpr* arg) { return nekoUnary(NEKO_EXPR_ABS, arg); }
// Construct an error-function expression
NekoExpr* nekoErf(NekoExpr* arg) { return nekoUnary(NEKO_EXPR_ERF, arg); }
// Construct an exponential-integral expression
NekoExpr* nekoEi(NekoExpr* arg) { return nekoUnary(NEKO_EXPR_EI, arg); }
// Construct a step-function expression
NekoExpr* nekoStep(NekoExpr* arg) { return nekoUnary(NEKO_EXPR_STEP, arg); }

/* ---------- Free and clone ---------- */

// Free an expression tree and all of its owned children
void nekoFreeExpr(NekoExpr* expr) {
    // Treat NULL as an already-freed tree
    if (!expr) return;

    // Release child storage according to the active expression kind
    switch (expr->kind) {
        case NEKO_EXPR_VAR:
            free(expr->as.var);
            break;
        case NEKO_EXPR_ADD:
        case NEKO_EXPR_SUB:
        case NEKO_EXPR_MUL:
        case NEKO_EXPR_DIV:
        case NEKO_EXPR_POW:
            nekoFreeExpr(expr->as.binary.lhs);
            nekoFreeExpr(expr->as.binary.rhs);
            break;
        case NEKO_EXPR_NEG:
        case NEKO_EXPR_SIN:
        case NEKO_EXPR_COS:
        case NEKO_EXPR_TAN:
        case NEKO_EXPR_ASIN:
        case NEKO_EXPR_ACOS:
        case NEKO_EXPR_ATAN:
        case NEKO_EXPR_EXP:
        case NEKO_EXPR_LOG:
        case NEKO_EXPR_SQRT:
        case NEKO_EXPR_ABS:
        case NEKO_EXPR_ERF:
        case NEKO_EXPR_EI:
        case NEKO_EXPR_STEP:
            nekoFreeExpr(expr->as.unary.arg);
            break;
        case NEKO_EXPR_CALL:
            free(expr->as.call.name);

            // Recursively free every argument owned by the call
            for (int i = 0; i < expr->as.call.nargs; i++)
                nekoFreeExpr(expr->as.call.args[i]);
            free(expr->as.call.args);
            break;
        case NEKO_EXPR_CONST:
            break;
    }

    // Release the expression node itself after its children are gone
    free(expr);
}

// Deep-copy an expression tree
NekoExpr* nekoCloneExpr(const NekoExpr* expr) {
    // Preserve NULL inputs as failed clone results
    if (!expr) return NULL;

    // Clone according to the active expression kind
    switch (expr->kind) {
        case NEKO_EXPR_CONST:
            return nekoConst(expr->as.constant);
        case NEKO_EXPR_VAR:
            return nekoVar(expr->as.var);
        case NEKO_EXPR_ADD:
        case NEKO_EXPR_SUB:
        case NEKO_EXPR_MUL:
        case NEKO_EXPR_DIV:
        case NEKO_EXPR_POW:
            return nekoBinary(expr->kind,
                              nekoCloneExpr(expr->as.binary.lhs),
                              nekoCloneExpr(expr->as.binary.rhs));
        case NEKO_EXPR_NEG:
        case NEKO_EXPR_SIN:
        case NEKO_EXPR_COS:
        case NEKO_EXPR_TAN:
        case NEKO_EXPR_ASIN:
        case NEKO_EXPR_ACOS:
        case NEKO_EXPR_ATAN:
        case NEKO_EXPR_EXP:
        case NEKO_EXPR_LOG:
        case NEKO_EXPR_SQRT:
        case NEKO_EXPR_ABS:
        case NEKO_EXPR_ERF:
        case NEKO_EXPR_EI:
        case NEKO_EXPR_STEP:
            return nekoUnary(expr->kind, nekoCloneExpr(expr->as.unary.arg));
        case NEKO_EXPR_CALL: {
            NekoExpr** args = NULL;

            // Allocate and clone each call argument when the call is nonempty
            if (expr->as.call.nargs > 0) {
                args = calloc((size_t)expr->as.call.nargs, sizeof(NekoExpr*));
                if (!args) return NULL;
                for (int i = 0; i < expr->as.call.nargs; i++)
                    args[i] = nekoCloneExpr(expr->as.call.args[i]);
            }

            // Rebuild the call node around the cloned argument array
            return nekoCall(expr->as.call.name, args, expr->as.call.nargs);
        }
    }
    return NULL;
}

/* ---------- Evaluation ---------- */

// Evaluate an expression numerically at a variable value
long double nekoEvalExpr(const NekoExpr* expr, const char* var, long double x) {
    // Invalid expressions evaluate to NAN
    if (!expr) return NAN;

    // Dispatch recursively according to expression kind
    switch (expr->kind) {
        case NEKO_EXPR_CONST:
            return expr->as.constant;
        case NEKO_EXPR_VAR:
            return (!var || strcmp(expr->as.var, var) == 0) ? x : NAN;
        case NEKO_EXPR_ADD:
            return nekoEvalExpr(expr->as.binary.lhs, var, x)
                 + nekoEvalExpr(expr->as.binary.rhs, var, x);
        case NEKO_EXPR_SUB:
            return nekoEvalExpr(expr->as.binary.lhs, var, x)
                 - nekoEvalExpr(expr->as.binary.rhs, var, x);
        case NEKO_EXPR_MUL:
            return nekoEvalExpr(expr->as.binary.lhs, var, x)
                 * nekoEvalExpr(expr->as.binary.rhs, var, x);
        case NEKO_EXPR_DIV:
            return nekoEvalExpr(expr->as.binary.lhs, var, x)
                 / nekoEvalExpr(expr->as.binary.rhs, var, x);
        case NEKO_EXPR_POW:
            return powl(nekoEvalExpr(expr->as.binary.lhs, var, x),
                       nekoEvalExpr(expr->as.binary.rhs, var, x));
        case NEKO_EXPR_NEG:
            return -nekoEvalExpr(expr->as.unary.arg, var, x);
        case NEKO_EXPR_SIN:
            return sinl(nekoEvalExpr(expr->as.unary.arg, var, x));
        case NEKO_EXPR_COS:
            return cosl(nekoEvalExpr(expr->as.unary.arg, var, x));
        case NEKO_EXPR_TAN:
            return tanl(nekoEvalExpr(expr->as.unary.arg, var, x));
        case NEKO_EXPR_ASIN:
            return asinl(nekoEvalExpr(expr->as.unary.arg, var, x));
        case NEKO_EXPR_ACOS:
            return acosl(nekoEvalExpr(expr->as.unary.arg, var, x));
        case NEKO_EXPR_ATAN:
            return atanl(nekoEvalExpr(expr->as.unary.arg, var, x));
        case NEKO_EXPR_EXP:
            return expl(nekoEvalExpr(expr->as.unary.arg, var, x));
        case NEKO_EXPR_LOG:
            return logl(nekoEvalExpr(expr->as.unary.arg, var, x));
        case NEKO_EXPR_SQRT:
            return sqrtl(nekoEvalExpr(expr->as.unary.arg, var, x));
        case NEKO_EXPR_ABS:
            return fabsl(nekoEvalExpr(expr->as.unary.arg, var, x));
        case NEKO_EXPR_ERF:
            return realErf(nekoEvalExpr(expr->as.unary.arg, var, x));
        case NEKO_EXPR_EI:
            return realEi(nekoEvalExpr(expr->as.unary.arg, var, x));
        case NEKO_EXPR_STEP: {
            long double v = nekoEvalExpr(expr->as.unary.arg, var, x);
            return v >= 0.0L ? 1.0L : 0.0L;
        }
        case NEKO_EXPR_CALL:
            return NAN;
    }
    return NAN;
}

// Evaluate an expression numerically at two variable values
long double nekoEvalExpr2D(const NekoExpr* expr, const char* xvar, long double x, const char* yvar, long double y) {
    // Invalid expressions evaluate to NAN
    if (!expr) return NAN;

    // Dispatch recursively according to expression kind
    switch (expr->kind) {
        case NEKO_EXPR_CONST:
            return expr->as.constant;
        case NEKO_EXPR_VAR:
            if (expr->as.var && xvar && strcmp(expr->as.var, xvar) == 0) return x;
            if (expr->as.var && yvar && strcmp(expr->as.var, yvar) == 0) return y;
            return NAN;
        case NEKO_EXPR_ADD:
            return nekoEvalExpr2D(expr->as.binary.lhs, xvar, x, yvar, y)
                 + nekoEvalExpr2D(expr->as.binary.rhs, xvar, x, yvar, y);
        case NEKO_EXPR_SUB:
            return nekoEvalExpr2D(expr->as.binary.lhs, xvar, x, yvar, y)
                 - nekoEvalExpr2D(expr->as.binary.rhs, xvar, x, yvar, y);
        case NEKO_EXPR_MUL:
            return nekoEvalExpr2D(expr->as.binary.lhs, xvar, x, yvar, y)
                 * nekoEvalExpr2D(expr->as.binary.rhs, xvar, x, yvar, y);
        case NEKO_EXPR_DIV:
            return nekoEvalExpr2D(expr->as.binary.lhs, xvar, x, yvar, y)
                 / nekoEvalExpr2D(expr->as.binary.rhs, xvar, x, yvar, y);
        case NEKO_EXPR_POW:
            return powl(nekoEvalExpr2D(expr->as.binary.lhs, xvar, x, yvar, y),
                        nekoEvalExpr2D(expr->as.binary.rhs, xvar, x, yvar, y));
        case NEKO_EXPR_NEG:
            return -nekoEvalExpr2D(expr->as.unary.arg, xvar, x, yvar, y);
        case NEKO_EXPR_SIN:
            return sinl(nekoEvalExpr2D(expr->as.unary.arg, xvar, x, yvar, y));
        case NEKO_EXPR_COS:
            return cosl(nekoEvalExpr2D(expr->as.unary.arg, xvar, x, yvar, y));
        case NEKO_EXPR_TAN:
            return tanl(nekoEvalExpr2D(expr->as.unary.arg, xvar, x, yvar, y));
        case NEKO_EXPR_ASIN:
            return asinl(nekoEvalExpr2D(expr->as.unary.arg, xvar, x, yvar, y));
        case NEKO_EXPR_ACOS:
            return acosl(nekoEvalExpr2D(expr->as.unary.arg, xvar, x, yvar, y));
        case NEKO_EXPR_ATAN:
            return atanl(nekoEvalExpr2D(expr->as.unary.arg, xvar, x, yvar, y));
        case NEKO_EXPR_EXP:
            return expl(nekoEvalExpr2D(expr->as.unary.arg, xvar, x, yvar, y));
        case NEKO_EXPR_LOG:
            return logl(nekoEvalExpr2D(expr->as.unary.arg, xvar, x, yvar, y));
        case NEKO_EXPR_SQRT:
            return sqrtl(nekoEvalExpr2D(expr->as.unary.arg, xvar, x, yvar, y));
        case NEKO_EXPR_ABS:
            return fabsl(nekoEvalExpr2D(expr->as.unary.arg, xvar, x, yvar, y));
        case NEKO_EXPR_ERF:
            return realErf(nekoEvalExpr2D(expr->as.unary.arg, xvar, x, yvar, y));
        case NEKO_EXPR_EI:
            return realEi(nekoEvalExpr2D(expr->as.unary.arg, xvar, x, yvar, y));
        case NEKO_EXPR_STEP: {
            long double v = nekoEvalExpr2D(expr->as.unary.arg, xvar, x, yvar, y);
            return v >= 0.0L ? 1.0L : 0.0L;
        }
        case NEKO_EXPR_CALL:
            return NAN;
    }
    return NAN;
}

// Sample an explicit graph y = f(x) over an interval
NekoExplicitGraphSample nekoSampleExplicitGraph(const NekoExpr* expr, long double xmin, long double xmax, size_t samples) {
    NekoExplicitGraphSample out = { .status = NEKO_OK, .points = NULL, .count = 0 };

    // Validate the expression, interval, and sample count
    if (!expr || !isfinite(xmin) || !isfinite(xmax) || samples < 2 || xmin == xmax) {
        out.status = NEKO_ERR_INVALID_ARG;
        return out;
    }

    // Allocate one point for each requested sample
    out.points = calloc(samples, sizeof(NekoGraphPoint));
    if (!out.points) {
        out.status = NEKO_ERR_INVALID_ARG;
        return out;
    }
    out.count = samples;

    // Evaluate the expression on an evenly spaced grid
    long double h = (xmax - xmin) / (long double)(samples - 1);
    for (size_t i = 0; i < samples; i++) {
        long double x = xmin + (long double)i * h;
        long double y = nekoEvalExpr(expr, "x", x);
        out.points[i] = (NekoGraphPoint){ .x = x, .y = y, .valid = isfinite(y) };
    }
    return out;
}

// Append a segment to an implicit graph sample
static bool graphSegmentPush(NekoImplicitGraphSample* sample, size_t* cap, NekoGraphSegment segment) {
    // Reject missing sample storage
    if (!sample || !cap) return false;

    // Grow the segment buffer when it is full
    if (sample->count == *cap) {
        size_t nextCap = *cap ? *cap * 2 : 64;
        NekoGraphSegment* next = realloc(sample->segments, nextCap * sizeof(NekoGraphSegment));
        if (!next) return false;
        sample->segments = next;
        *cap = nextCap;
    }

    // Store the new segment
    sample->segments[sample->count++] = segment;
    return true;
}

// Interpolate a zero crossing along a cell edge
static NekoGraphPoint graphEdgePoint(long double x1, long double y1, long double v1,
                                     long double x2, long double y2, long double v2) {
    // Use linear interpolation, with a midpoint fallback for nearly equal values
    long double denom = v1 - v2;
    long double t = fabsl(denom) > 1e-18L ? v1 / denom : 0.5L;
    if (!isfinite(t)) t = 0.5L;
    if (t < 0.0L) t = 0.0L;
    if (t > 1.0L) t = 1.0L;

    // Return the interpolated point on the edge
    return (NekoGraphPoint){
        .x = x1 + t * (x2 - x1),
        .y = y1 + t * (y2 - y1),
        .valid = true
    };
}

// Add zero-contour pieces for one marching-squares cell
static bool sampleImplicitCell(NekoImplicitGraphSample* out, size_t* cap,
                               long double x0, long double x1,
                               long double y0, long double y1,
                               long double v00, long double v10,
                               long double v11, long double v01) {
    // Skip cells with any nonfinite corner
    if (!isfinite(v00) || !isfinite(v10) || !isfinite(v11) || !isfinite(v01)) return true;

    // Collect edge crossings in clockwise order
    NekoGraphPoint points[4];
    int count = 0;
    if ((v00 <= 0.0L && v10 >= 0.0L) || (v00 >= 0.0L && v10 <= 0.0L))
        points[count++] = graphEdgePoint(x0, y0, v00, x1, y0, v10);
    if ((v10 <= 0.0L && v11 >= 0.0L) || (v10 >= 0.0L && v11 <= 0.0L))
        points[count++] = graphEdgePoint(x1, y0, v10, x1, y1, v11);
    if ((v11 <= 0.0L && v01 >= 0.0L) || (v11 >= 0.0L && v01 <= 0.0L))
        points[count++] = graphEdgePoint(x1, y1, v11, x0, y1, v01);
    if ((v01 <= 0.0L && v00 >= 0.0L) || (v01 >= 0.0L && v00 <= 0.0L))
        points[count++] = graphEdgePoint(x0, y1, v01, x0, y0, v00);

    // Connect crossing pairs into contour segments
    if (count == 2) {
        return graphSegmentPush(out, cap, (NekoGraphSegment){
            .x1 = points[0].x, .y1 = points[0].y,
            .x2 = points[1].x, .y2 = points[1].y
        });
    }
    if (count == 4) {
        return graphSegmentPush(out, cap, (NekoGraphSegment){
            .x1 = points[0].x, .y1 = points[0].y,
            .x2 = points[1].x, .y2 = points[1].y
        }) && graphSegmentPush(out, cap, (NekoGraphSegment){
            .x1 = points[2].x, .y1 = points[2].y,
            .x2 = points[3].x, .y2 = points[3].y
        });
    }
    return true;
}

// Sample an implicit graph F(x, y) = 0 over a rectangle
NekoImplicitGraphSample nekoSampleImplicitGraph(const NekoExpr* expr, long double xmin, long double xmax, long double ymin, long double ymax, size_t xsteps, size_t ysteps) {
    NekoImplicitGraphSample out = { .status = NEKO_OK, .segments = NULL, .count = 0 };

    // Validate the expression, viewport, and grid size
    if (!expr || !isfinite(xmin) || !isfinite(xmax) || !isfinite(ymin) || !isfinite(ymax)
            || xmin == xmax || ymin == ymax || xsteps < 2 || ysteps < 2) {
        out.status = NEKO_ERR_INVALID_ARG;
        return out;
    }

    // Allocate the grid of sampled function values
    size_t nx = xsteps + 1;
    size_t ny = ysteps + 1;
    size_t total = 0;
    if (!checkedSizeMul(nx, ny, &total)) {
        out.status = NEKO_ERR_INVALID_ARG;
        return out;
    }
    long double* values = malloc(total * sizeof(long double));
    if (!values) {
        out.status = NEKO_ERR_INVALID_ARG;
        return out;
    }

    // Evaluate F(x, y) at each grid point
    long double hx = (xmax - xmin) / (long double)xsteps;
    long double hy = (ymax - ymin) / (long double)ysteps;
    for (size_t j = 0; j < ny; j++) {
        long double y = ymin + (long double)j * hy;
        for (size_t i = 0; i < nx; i++) {
            long double x = xmin + (long double)i * hx;
            values[j * nx + i] = nekoEvalExpr2D(expr, "x", x, "y", y);
        }
    }

    // Run marching squares on each grid cell
    size_t cap = 0;
    for (size_t j = 0; j < ysteps; j++) {
        long double y0 = ymin + (long double)j * hy;
        long double y1 = y0 + hy;
        for (size_t i = 0; i < xsteps; i++) {
            long double x0 = xmin + (long double)i * hx;
            long double x1 = x0 + hx;
            if (!sampleImplicitCell(&out, &cap, x0, x1, y0, y1,
                                    values[j * nx + i],
                                    values[j * nx + i + 1],
                                    values[(j + 1) * nx + i + 1],
                                    values[(j + 1) * nx + i])) {
                free(values);
                free(out.segments);
                out.segments = NULL;
                out.count = 0;
                out.status = NEKO_ERR_INVALID_ARG;
                return out;
            }
        }
    }

    // Release the temporary value grid
    free(values);
    return out;
}

// Free explicit graph sample storage
void nekoFreeExplicitGraphSample(NekoExplicitGraphSample sample) {
    // Release the owned point array
    free(sample.points);
}

// Free implicit graph sample storage
void nekoFreeImplicitGraphSample(NekoImplicitGraphSample sample) {
    // Release the owned segment array
    free(sample.segments);
}

/* ---------- Simplification ---------- */

typedef struct {
    NekoExpr** items;
    size_t len;
    size_t cap;
} ExprVec;

typedef struct {
    int degree;
    NekoExpr* coeff;
} PolyTerm;

typedef struct {
    PolyTerm* items;
    size_t len;
    size_t cap;
} PolyVec;

// Append an expression to a growable expression vector
static bool exprVecPush(ExprVec* vec, NekoExpr* expr) {
    // Reject invalid inputs without taking ownership
    if (!vec || !expr) return false;

    // Grow the backing store when the vector is full
    if (vec->len == vec->cap) {
        size_t nextCap = vec->cap ? vec->cap * 2 : 4;
        NekoExpr** next = realloc(vec->items, nextCap * sizeof(NekoExpr*));
        if (!next) {
            nekoFreeExpr(expr);
            return false;
        }
        vec->items = next;
        vec->cap = nextCap;
    }

    // Store the owned expression at the next free slot
    vec->items[vec->len++] = expr;
    return true;
}

// Free the contents of a growable expression vector
static void exprVecFree(ExprVec* vec) {
    // Treat NULL as an already-empty vector
    if (!vec) return;

    // Free every expression currently owned by the vector
    for (size_t i = 0; i < vec->len; i++) nekoFreeExpr(vec->items[i]);

    // Reset the vector to an empty reusable state
    free(vec->items);
    vec->items = NULL;
    vec->len = 0;
    vec->cap = 0;
}

// Append a polynomial term to a growable polynomial vector
static bool polyVecPush(PolyVec* vec, int degree, NekoExpr* coeff) {
    // Reject invalid inputs and release coefficient ownership on failure
    if (!vec || !coeff) {
        nekoFreeExpr(coeff);
        return false;
    }

    // Grow the polynomial term array when necessary
    if (vec->len == vec->cap) {
        size_t nextCap = vec->cap ? vec->cap * 2 : 4;
        PolyTerm* next = realloc(vec->items, nextCap * sizeof(PolyTerm));
        if (!next) {
            nekoFreeExpr(coeff);
            return false;
        }
        vec->items = next;
        vec->cap = nextCap;
    }

    // Append the owned coefficient with its degree metadata
    vec->items[vec->len++] = (PolyTerm){ .degree = degree, .coeff = coeff };
    return true;
}

// Free the coefficients held by a polynomial vector
static void polyVecFree(PolyVec* vec) {
    // Treat NULL as an already-empty polynomial vector
    if (!vec) return;

    // Free each coefficient expression owned by the vector
    for (size_t i = 0; i < vec->len; i++) nekoFreeExpr(vec->items[i].coeff);

    // Reset the vector storage after releasing the coefficients
    free(vec->items);
    vec->items = NULL;
    vec->len = 0;
    vec->cap = 0;
}

// Test whether an expression tree mentions a variable
static int exprDependsOnVar(const NekoExpr* expr, const char* var) {
    // Missing expressions or variable names cannot match
    if (!expr || !var) return 0;

    // Recurse according to the expression shape
    switch (expr->kind) {
        case NEKO_EXPR_CONST:
            return 0;
        case NEKO_EXPR_VAR:
            return expr->as.var && strcmp(expr->as.var, var) == 0;
        case NEKO_EXPR_ADD:
        case NEKO_EXPR_SUB:
        case NEKO_EXPR_MUL:
        case NEKO_EXPR_DIV:
        case NEKO_EXPR_POW:
            return exprDependsOnVar(expr->as.binary.lhs, var)
                || exprDependsOnVar(expr->as.binary.rhs, var);
        case NEKO_EXPR_NEG:
        case NEKO_EXPR_SIN:
        case NEKO_EXPR_COS:
        case NEKO_EXPR_TAN:
        case NEKO_EXPR_ASIN:
        case NEKO_EXPR_ACOS:
        case NEKO_EXPR_ATAN:
        case NEKO_EXPR_EXP:
        case NEKO_EXPR_LOG:
        case NEKO_EXPR_SQRT:
        case NEKO_EXPR_ABS:
        case NEKO_EXPR_ERF:
        case NEKO_EXPR_EI:
        case NEKO_EXPR_STEP:
            return exprDependsOnVar(expr->as.unary.arg, var);
        case NEKO_EXPR_CALL:
            // Search every call argument for the requested variable
            for (int i = 0; i < expr->as.call.nargs; i++) {
                if (exprDependsOnVar(expr->as.call.args[i], var)) return 1;
            }
            return 0;
    }
    return 0;
}

// Decode a constant expression as a non-negative integer
static bool constNonNegativeInteger(const NekoExpr* expr, int* out) {
    // Only constant expressions can represent integer exponents here
    if (!expr || expr->kind != NEKO_EXPR_CONST) return false;

    // Round and reject negative or genuinely nonintegral constants
    long double rounded = roundl(expr->as.constant);
    if (expr->as.constant < -1e-12 || fabsl(expr->as.constant - rounded) > 1e-9) return false;

    // Return the decoded integer when the caller asked for it
    if (out) *out = (int)rounded;
    return true;
}

// Split a polynomial monomial into degree and coefficient
static bool splitPolynomialMonomial(const NekoExpr* expr, const char* var, int* degree, NekoExpr** coeff) {
    // Require all output channels before splitting the monomial
    if (!expr || !var || !degree || !coeff) return false;

    // Treat variable-free expressions as degree-zero coefficients
    if (!exprDependsOnVar(expr, var)) {
        *degree = 0;
        *coeff = nekoCloneExpr(expr);
        return *coeff != NULL;
    }

    // Recognize the bare variable as degree one with coefficient one
    if (sameVar(expr, var)) {
        *degree = 1;
        *coeff = nekoConst(1.0);
        return *coeff != NULL;
    }

    // Decompose the remaining supported monomial shapes
    switch (expr->kind) {
        case NEKO_EXPR_NEG: {
            // Split the inner monomial and negate only its coefficient
            NekoExpr* innerCoeff = NULL;
            int innerDegree = 0;
            if (!splitPolynomialMonomial(expr->as.unary.arg, var, &innerDegree, &innerCoeff)) return false;
            *degree = innerDegree;
            *coeff = nekoSimplify(nekoNeg(innerCoeff));
            return *coeff != NULL;
        }
        case NEKO_EXPR_POW: {
            // Accept powers only when the base is the selected variable
            int power = 0;
            if (!sameVar(expr->as.binary.lhs, var) || !constNonNegativeInteger(expr->as.binary.rhs, &power)) {
                return false;
            }

            // Store x^n as degree n with coefficient one
            *degree = power;
            *coeff = nekoConst(1.0);
            return *coeff != NULL;
        }
        case NEKO_EXPR_MUL: {
            // Split both factors and combine their degrees and coefficients
            int leftDegree = 0, rightDegree = 0;
            NekoExpr* leftCoeff = NULL;
            NekoExpr* rightCoeff = NULL;
            if (!splitPolynomialMonomial(expr->as.binary.lhs, var, &leftDegree, &leftCoeff)
                    || !splitPolynomialMonomial(expr->as.binary.rhs, var, &rightDegree, &rightCoeff)) {
                nekoFreeExpr(leftCoeff);
                nekoFreeExpr(rightCoeff);
                return false;
            }
            *degree = leftDegree + rightDegree;
            *coeff = nekoSimplify(nekoMul(leftCoeff, rightCoeff));
            return *coeff != NULL;
        }
        case NEKO_EXPR_DIV:
            // Allow division only by expressions independent of the variable
            if (!exprDependsOnVar(expr->as.binary.rhs, var)) {
                int numDegree = 0;
                NekoExpr* numCoeff = NULL;
                if (!splitPolynomialMonomial(expr->as.binary.lhs, var, &numDegree, &numCoeff)) return false;

                // Divide the numerator coefficient by the cloned denominator
                *degree = numDegree;
                *coeff = nekoSimplify(nekoDiv(numCoeff, nekoCloneExpr(expr->as.binary.rhs)));
                return *coeff != NULL;
            }
            return false;
        default:
            // Reject expression shapes that are not polynomial monomials
            return false;
    }
}

// Add a coefficient into a polynomial vector, merging like degrees
static bool polyVecAddCoeff(PolyVec* vec, int degree, NekoExpr* coeff) {
    // Reject invalid inputs and release coefficient ownership on failure
    if (!vec || !coeff) {
        nekoFreeExpr(coeff);
        return false;
    }

    // Merge with an existing term of the same degree when possible
    for (size_t i = 0; i < vec->len; i++) {
        if (vec->items[i].degree == degree) {
            NekoExpr* merged = nekoSimplify(nekoAdd(vec->items[i].coeff, coeff));
            vec->items[i].coeff = merged;
            return merged != NULL;
        }
    }

    // Append a new degree entry if no like term exists
    return polyVecPush(vec, degree, coeff);
}

// Collect polynomial terms from an additive expression tree
static bool collectPolynomialTerms(const NekoExpr* expr, const char* var, int sign, PolyVec* terms) {
    // Require a valid expression, variable name, and destination vector
    if (!expr || !var || !terms) return false;

    // Traverse additive structure while tracking the current sign
    switch (expr->kind) {
        case NEKO_EXPR_ADD:
            return collectPolynomialTerms(expr->as.binary.lhs, var, sign, terms)
                && collectPolynomialTerms(expr->as.binary.rhs, var, sign, terms);
        case NEKO_EXPR_SUB:
            return collectPolynomialTerms(expr->as.binary.lhs, var, sign, terms)
                && collectPolynomialTerms(expr->as.binary.rhs, var, -sign, terms);
        case NEKO_EXPR_NEG:
            return collectPolynomialTerms(expr->as.unary.arg, var, -sign, terms);
        default: {
            // Split non-additive leaves as monomials
            int degree = 0;
            NekoExpr* coeff = NULL;
            if (!splitPolynomialMonomial(expr, var, &degree, &coeff)) return false;

            // Apply the accumulated sign before storing the coefficient
            if (sign < 0) coeff = nekoSimplify(nekoNeg(coeff));
            return coeff ? polyVecAddCoeff(terms, degree, coeff) : false;
        }
    }
}

// Compare polynomial terms by degree for sorting
static int polyTermCompare(const void* lhs, const void* rhs) {
    // Interpret the qsort payloads as polynomial terms
    const PolyTerm* left = (const PolyTerm*)lhs;
    const PolyTerm* right = (const PolyTerm*)rhs;

    // Sort terms by increasing degree
    return (left->degree > right->degree) - (left->degree < right->degree);
}

// Build a polynomial term from an owned coefficient
static NekoExpr* buildPolynomialTermOwned(const char* var, int degree, NekoExpr* coeff) {
    // Reject missing coefficient ownership
    if (!coeff) return NULL;

    // Degree-zero terms are just their coefficient
    if (degree == 0) return coeff;

    // Build the variable power corresponding to the requested degree
    NekoExpr* power = degree == 1
        ? nekoVar(var)
        : nekoPow(nekoVar(var), nekoConst((long double)degree));
    if (!power) {
        nekoFreeExpr(coeff);
        return NULL;
    }

    // Omit multiplication by one for readable output
    if (isConst(coeff, 1.0)) {
        nekoFreeExpr(coeff);
        return power;
    }

    // Convert multiplication by negative one into unary negation
    if (isConst(coeff, -1.0)) {
        nekoFreeExpr(coeff);
        return nekoNeg(power);
    }

    // Multiply the coefficient and power, then simplify any easy cases
    return nekoSimplify(nekoMul(coeff, power));
}

// Rebuild a normalized polynomial expression from collected terms
static NekoExpr* rebuildPolynomialExpr(const char* var, PolyVec* terms) {
    // Require valid inputs before rebuilding
    if (!terms || !var) return NULL;

    // An empty term list represents the zero polynomial
    if (terms->len == 0) return nekoConst(0.0);

    // Sort by degree so printed terms have stable polynomial order
    qsort(terms->items, terms->len, sizeof(PolyTerm), polyTermCompare);

    // Build an additive expression from the nonzero collected terms
    NekoExpr* out = NULL;
    for (size_t i = 0; i < terms->len; i++) {
        NekoExpr* coeff = terms->items[i].coeff;
        terms->items[i].coeff = NULL;

        // Drop zero or missing coefficients before constructing a term
        if (!coeff || isConst(coeff, 0.0)) {
            nekoFreeExpr(coeff);
            continue;
        }

        // Convert the degree and coefficient back into an expression term
        NekoExpr* term = buildPolynomialTermOwned(var, terms->items[i].degree, coeff);
        if (!term) {
            nekoFreeExpr(out);
            return NULL;
        }

        // Append the term to the accumulated polynomial sum
        out = out ? nekoAdd(out, term) : term;
    }

    // Return zero when every collected term simplified away
    return out ? out : nekoConst(0.0);
}

// Normalize a polynomial expression in a chosen variable
static NekoExpr* normalizePolynomialOwned(NekoExpr* expr, const char* var) {
    // Leave invalid or variable-free expressions untouched
    if (!expr || !var || !exprDependsOnVar(expr, var)) return expr;

    // Try to collect the expression as a polynomial in the selected variable
    PolyVec terms = {0};
    if (!collectPolynomialTerms(expr, var, 1, &terms)) {
        polyVecFree(&terms);
        return expr;
    }

    // Rebuild the collected polynomial into normalized expression form
    NekoExpr* rebuilt = rebuildPolynomialExpr(var, &terms);
    polyVecFree(&terms);
    if (!rebuilt) return expr;

    // Replace the original tree with the normalized tree
    nekoFreeExpr(expr);
    return rebuilt;
}

// Normalize a polynomial expression using the default variable choice
static NekoExpr* normalizePolynomialDefaultOwned(NekoExpr* expr) {
    // Preserve failed simplifications as NULL
    if (!expr) return NULL;

    // Prefer x as the displayed polynomial variable
    if (exprDependsOnVar(expr, "x")) return normalizePolynomialOwned(expr, "x");

    // Fall back to y for the small multivariable cases NEKO supports
    if (exprDependsOnVar(expr, "y")) return normalizePolynomialOwned(expr, "y");

    // Leave variable-free expressions in their current form
    return expr;
}

// Split a product into a scalar coefficient and symbolic factors
static bool collectProductPiecesOwned(NekoExpr* expr, long double* coeff, ExprVec* factors) {
    // Reject invalid inputs and release expression ownership on failure
    if (!expr || !coeff || !factors) {
        nekoFreeExpr(expr);
        return false;
    }

    // Peel off scalar and multiplicative structure while preserving ownership
    switch (expr->kind) {
        case NEKO_EXPR_CONST:
            // Fold constants directly into the scalar coefficient
            *coeff *= expr->as.constant;
            nekoFreeExpr(expr);
            return true;
        case NEKO_EXPR_NEG: {
            // Move unary negation into the scalar coefficient
            NekoExpr* arg = expr->as.unary.arg;
            expr->as.unary.arg = NULL;
            free(expr);
            *coeff = -*coeff;
            return collectProductPiecesOwned(arg, coeff, factors);
        }
        case NEKO_EXPR_MUL: {
            // Detach both factors and collect them recursively
            NekoExpr* lhs = expr->as.binary.lhs;
            NekoExpr* rhs = expr->as.binary.rhs;
            expr->as.binary.lhs = NULL;
            expr->as.binary.rhs = NULL;
            free(expr);
            if (!collectProductPiecesOwned(lhs, coeff, factors)) {
                nekoFreeExpr(rhs);
                return false;
            }

            // Collect the right side after the left side succeeds
            if (!collectProductPiecesOwned(rhs, coeff, factors)) {
                exprVecFree(factors);
                return false;
            }
            return true;
        }
        case NEKO_EXPR_DIV:
            // Treat division by a literal constant as scalar multiplication
            if (expr->as.binary.rhs && expr->as.binary.rhs->kind == NEKO_EXPR_CONST) {
                long double denom = expr->as.binary.rhs->as.constant;
                NekoExpr* lhs = expr->as.binary.lhs;
                expr->as.binary.lhs = NULL;
                nekoFreeExpr(expr->as.binary.rhs);
                expr->as.binary.rhs = NULL;
                free(expr);
                *coeff /= denom;
                return collectProductPiecesOwned(lhs, coeff, factors);
            }
            break;
        default:
            break;
    }

    // Keep non-scalar factors in their original expression form
    return exprVecPush(factors, expr);
}

// Rebuild a product from a scalar coefficient and collected factors
static NekoExpr* rebuildCollectedProduct(long double coeff, ExprVec* factors) {
    // Require the factor vector that owns the collected factors
    if (!factors) return NULL;

    // A zero scalar collapses the whole product to zero
    if (fabsl(coeff) <= 1e-12) {
        exprVecFree(factors);
        return nekoConst(0.0);
    }

    // If no symbolic factors remain, the scalar is the full product
    if (factors->len == 0) {
        free(factors->items);
        factors->items = NULL;
        factors->len = 0;
        factors->cap = 0;
        return nekoConst(coeff);
    }

    // Choose the first product node based on the scalar coefficient
    size_t index = 0;
    NekoExpr* out = NULL;
    if (fabsl(coeff - 1.0) <= 1e-12) {
        out = factors->items[index++];
    } else if (fabsl(coeff + 1.0) <= 1e-12 && factors->len == 1) {
        out = nekoNeg(factors->items[index++]);
    } else {
        out = nekoMul(nekoConst(coeff), factors->items[index++]);
    }

    // Multiply in all remaining symbolic factors from left to right
    for (; index < factors->len; index++) out = nekoMul(out, factors->items[index]);

    // Release vector storage after transferring all expression ownership
    free(factors->items);
    factors->items = NULL;
    factors->len = 0;
    factors->cap = 0;
    return out;
}

// Simplify an expression tree and return the simplified owned tree
NekoExpr* nekoSimplify(NekoExpr* expr) {
    // Preserve NULL simplification inputs
    if (!expr) return NULL;

    // Simplify unary expressions before applying unary rewrite rules
    if (isUnaryKind(expr->kind)) {
        expr->as.unary.arg = nekoSimplify(expr->as.unary.arg);
        NekoExpr* arg = expr->as.unary.arg;

        // Collapse negation of constants and double negation
        if (expr->kind == NEKO_EXPR_NEG) {
            if (arg && arg->kind == NEKO_EXPR_CONST) {
                long double v = -arg->as.constant;
                nekoFreeExpr(expr);
                return nekoConst(v);
            }
            if (arg && arg->kind == NEKO_EXPR_NEG) {
                NekoExpr* out = arg->as.unary.arg;
                arg->as.unary.arg = NULL;
                nekoFreeExpr(expr);
                return out;
            }
        }

        // Evaluate unary functions with finite constant arguments
        if (arg && arg->kind == NEKO_EXPR_CONST) {
            long double v = nekoEvalExpr(expr, "x", 0.0);
            if (isfinite(v)) {
                nekoFreeExpr(expr);
                return nekoConst(v);
            }
        }
        return expr;
    }

    // Non-binary non-unary expressions are already simple
    if (!isBinaryKind(expr->kind)) return expr;

    // Simplify both operands before applying binary rewrite rules
    expr->as.binary.lhs = nekoSimplify(expr->as.binary.lhs);
    expr->as.binary.rhs = nekoSimplify(expr->as.binary.rhs);

    // Keep the current node when a child failed to simplify
    NekoExpr* lhs = expr->as.binary.lhs;
    NekoExpr* rhs = expr->as.binary.rhs;
    if (!lhs || !rhs) return expr;

    // Constant-fold finite binary operations
    if (lhs->kind == NEKO_EXPR_CONST && rhs->kind == NEKO_EXPR_CONST) {
        long double v = nekoEvalExpr(expr, "x", 0.0);
        if (isfinite(v)) {
            nekoFreeExpr(expr);
            return nekoConst(v);
        }
    }

    // Apply operator-specific algebraic identities
    switch (expr->kind) {
        case NEKO_EXPR_ADD:
            // Remove additive identities
            if (isConst(lhs, 0.0)) {
                expr->as.binary.rhs = NULL;
                nekoFreeExpr(expr);
                return rhs;
            }
            if (isConst(rhs, 0.0)) {
                expr->as.binary.lhs = NULL;
                nekoFreeExpr(expr);
                return lhs;
            }
            break;
        case NEKO_EXPR_SUB:
            // Remove subtractive identities and rewrite 0-rhs as negation
            if (isConst(rhs, 0.0)) {
                expr->as.binary.lhs = NULL;
                nekoFreeExpr(expr);
                return lhs;
            }
            if (isConst(lhs, 0.0)) {
                expr->as.binary.rhs = NULL;
                NekoExpr* out = nekoNeg(rhs);
                nekoFreeExpr(expr);
                return nekoSimplify(out);
            }
            break;
        case NEKO_EXPR_MUL:
            // Collapse multiplication by zero, one, and negative one
            if (isConst(lhs, 0.0) || isConst(rhs, 0.0)) {
                nekoFreeExpr(expr);
                return nekoConst(0.0);
            }
            if (isConst(lhs, 1.0)) {
                expr->as.binary.rhs = NULL;
                nekoFreeExpr(expr);
                return rhs;
            }
            if (isConst(rhs, 1.0)) {
                expr->as.binary.lhs = NULL;
                nekoFreeExpr(expr);
                return lhs;
            }
            if (isConst(lhs, -1.0)) {
                expr->as.binary.rhs = NULL;
                NekoExpr* out = nekoNeg(rhs);
                nekoFreeExpr(expr);
                return nekoSimplify(out);
            }
            if (isConst(rhs, -1.0)) {
                expr->as.binary.lhs = NULL;
                NekoExpr* out = nekoNeg(lhs);
                nekoFreeExpr(expr);
                return nekoSimplify(out);
            }
            {
                // Collect scalar factors to keep coefficients readable
                ExprVec factors = {0};
                long double coeff = 1.0;
                if (collectProductPiecesOwned(expr, &coeff, &factors)) {
                    return rebuildCollectedProduct(coeff, &factors);
                }
                exprVecFree(&factors);
                return NULL;
            }
            break;
        case NEKO_EXPR_DIV:
            // Collapse division by identity and self-division
            if (isConst(lhs, 0.0)) {
                nekoFreeExpr(expr);
                return nekoConst(0.0);
            }
            if (isConst(rhs, 1.0)) {
                expr->as.binary.lhs = NULL;
                nekoFreeExpr(expr);
                return lhs;
            }
            if (exprEqual(lhs, rhs)) {
                nekoFreeExpr(expr);
                return nekoConst(1.0);
            }
            if (rhs->kind == NEKO_EXPR_CONST) {
                // Fold literal denominators into the scalar coefficient
                ExprVec factors = {0};
                long double coeff = 1.0;
                NekoExpr* lhsOwned = lhs;
                expr->as.binary.lhs = NULL;
                expr->as.binary.rhs = NULL;
                long double denom = rhs->as.constant;
                nekoFreeExpr(rhs);
                free(expr);
                coeff /= denom;
                if (collectProductPiecesOwned(lhsOwned, &coeff, &factors)) {
                    return rebuildCollectedProduct(coeff, &factors);
                }
                exprVecFree(&factors);
                return NULL;
            }
            break;
        case NEKO_EXPR_POW:
            // Collapse common power identities
            if (isConst(rhs, 0.0)) {
                nekoFreeExpr(expr);
                return nekoConst(1.0);
            }
            if (isConst(rhs, 1.0)) {
                expr->as.binary.lhs = NULL;
                nekoFreeExpr(expr);
                return lhs;
            }
            if (isConst(lhs, 0.0)) {
                nekoFreeExpr(expr);
                return nekoConst(0.0);
            }
            if (isConst(lhs, 1.0)) {
                nekoFreeExpr(expr);
                return nekoConst(1.0);
            }
            break;
        default:
            break;
    }

    // Normalize supported polynomial expressions before returning
    return normalizePolynomialDefaultOwned(expr);
}

/* ---------- Differentiation ---------- */

// Wrap a successful differentiation result
static NekoDiffResult diffOk(NekoExpr* expr) {
    // Encode allocation failure as an invalid-argument status
    NekoDiffResult r = { .status = expr ? NEKO_OK : NEKO_ERR_INVALID_ARG, .expr = expr };
    return r;
}

// Wrap a failed differentiation result
static NekoDiffResult diffErr(NekoStatus status) {
    // Return the requested error with no expression payload
    NekoDiffResult r = { .status = status, .expr = NULL };
    return r;
}

// Differentiate a child expression or mark the parent operation failed
static NekoExpr* derivOrFree(const NekoExpr* expr, const char* var, bool* ok) {
    // Delegate to the public differentiator for the child subtree
    NekoDiffResult d = nekoDifferentiateExpr(expr, var);

    // Propagate failure through the shared ok flag
    if (d.status != NEKO_OK) {
        *ok = false;
        nekoFreeExpr(d.expr);
        return NULL;
    }
    return d.expr;
}

// Symbolically differentiate an expression with respect to a variable
NekoDiffResult nekoDifferentiateExpr(const NekoExpr* expr, const char* var) {
    // Require both an expression and differentiation variable
    if (!expr || !var) return diffErr(NEKO_ERR_INVALID_ARG);

    // Dispatch by expression kind and apply the matching differentiation rule
    switch (expr->kind) {
        case NEKO_EXPR_CONST:
            return diffOk(nekoConst(0.0));

        case NEKO_EXPR_VAR:
            return diffOk(nekoConst(strcmp(expr->as.var, var) == 0 ? 1.0 : 0.0));

        case NEKO_EXPR_ADD: {
            // Differentiate both summands independently
            bool ok = true;
            NekoExpr* dl = derivOrFree(expr->as.binary.lhs, var, &ok);
            NekoExpr* dr = derivOrFree(expr->as.binary.rhs, var, &ok);

            // Clean up partial derivatives if either child is unsupported
            if (!ok) {
                nekoFreeExpr(dl);
                nekoFreeExpr(dr);
                return diffErr(NEKO_ERR_UNSUPPORTED);
            }

            // Reassemble the derivative with the sum rule
            return diffOk(nekoSimplify(nekoAdd(dl, dr)));
        }

        case NEKO_EXPR_SUB: {
            // Differentiate both operands independently
            bool ok = true;
            NekoExpr* dl = derivOrFree(expr->as.binary.lhs, var, &ok);
            NekoExpr* dr = derivOrFree(expr->as.binary.rhs, var, &ok);

            // Clean up partial derivatives if either child is unsupported
            if (!ok) {
                nekoFreeExpr(dl);
                nekoFreeExpr(dr);
                return diffErr(NEKO_ERR_UNSUPPORTED);
            }

            // Reassemble the derivative with the difference rule
            return diffOk(nekoSimplify(nekoSub(dl, dr)));
        }

        case NEKO_EXPR_MUL: {
            // Clone both factors because the product rule needs originals and derivatives
            bool ok = true;
            NekoExpr* f = nekoCloneExpr(expr->as.binary.lhs);
            NekoExpr* g = nekoCloneExpr(expr->as.binary.rhs);
            NekoExpr* df = derivOrFree(expr->as.binary.lhs, var, &ok);
            NekoExpr* dg = derivOrFree(expr->as.binary.rhs, var, &ok);

            // Release every temporary if either derivative is unsupported
            if (!ok) {
                nekoFreeExpr(f); nekoFreeExpr(g); nekoFreeExpr(df); nekoFreeExpr(dg);
                return diffErr(NEKO_ERR_UNSUPPORTED);
            }

            // Apply the product rule f'g + fg'
            return diffOk(nekoSimplify(nekoAdd(nekoMul(df, g), nekoMul(f, dg))));
        }

        case NEKO_EXPR_DIV: {
            // Clone both operands because the quotient rule reuses the denominator
            bool ok = true;
            NekoExpr* f = nekoCloneExpr(expr->as.binary.lhs);
            NekoExpr* g = nekoCloneExpr(expr->as.binary.rhs);
            NekoExpr* df = derivOrFree(expr->as.binary.lhs, var, &ok);
            NekoExpr* dg = derivOrFree(expr->as.binary.rhs, var, &ok);

            // Release every temporary if either derivative is unsupported
            if (!ok) {
                nekoFreeExpr(f); nekoFreeExpr(g); nekoFreeExpr(df); nekoFreeExpr(dg);
                return diffErr(NEKO_ERR_UNSUPPORTED);
            }

            // Build the quotient-rule numerator f'g-fg'
            NekoExpr* numerator = nekoSub(nekoMul(df, nekoCloneExpr(expr->as.binary.rhs)),
                                          nekoMul(f, dg));

            // Divide by the squared denominator
            NekoExpr* denominator = nekoPow(g, nekoConst(2.0));
            return diffOk(nekoSimplify(nekoDiv(numerator, denominator)));
        }

        case NEKO_EXPR_POW: {
            // Separate base and exponent for constant-power and general-power rules
            bool ok = true;
            const NekoExpr* base = expr->as.binary.lhs;
            const NekoExpr* exponent = expr->as.binary.rhs;

            // Use the ordinary power rule when the exponent is constant
            if (exponent->kind == NEKO_EXPR_CONST) {
                long double n = exponent->as.constant;
                NekoExpr* db = derivOrFree(base, var, &ok);

                // Report unsupported if the base derivative failed
                if (!ok) {
                    nekoFreeExpr(db);
                    return diffErr(NEKO_ERR_UNSUPPORTED);
                }

                // Build n*f^(n-1)*f'
                NekoExpr* out = nekoMul(nekoMul(nekoConst(n),
                                                nekoPow(nekoCloneExpr(base), nekoConst(n - 1.0))),
                                        db);
                return diffOk(nekoSimplify(out));
            }

            // Clone and differentiate both pieces for f^g
            NekoExpr* f = nekoCloneExpr(base);
            NekoExpr* g = nekoCloneExpr(exponent);
            NekoExpr* df = derivOrFree(base, var, &ok);
            NekoExpr* dg = derivOrFree(exponent, var, &ok);

            // Release every temporary if either derivative is unsupported
            if (!ok) {
                nekoFreeExpr(f); nekoFreeExpr(g); nekoFreeExpr(df); nekoFreeExpr(dg);
                return diffErr(NEKO_ERR_UNSUPPORTED);
            }

            // Build the logarithmic derivative g'log(f)+g*f'/f
            NekoExpr* inner = nekoAdd(nekoMul(dg, nekoLog(nekoCloneExpr(base))),
                                      nekoMul(g, nekoDiv(df, nekoCloneExpr(base))));

            // Multiply by f^g to finish the general power rule
            NekoExpr* out = nekoMul(nekoPow(f, nekoCloneExpr(exponent)), inner);
            return diffOk(nekoSimplify(out));
        }

        case NEKO_EXPR_NEG: {
            // Differentiate the inner expression and negate the result
            NekoDiffResult d = nekoDifferentiateExpr(expr->as.unary.arg, var);
            if (d.status != NEKO_OK) return d;
            return diffOk(nekoSimplify(nekoNeg(d.expr)));
        }

        case NEKO_EXPR_SIN: {
            // Apply the chain rule for sin(u)
            bool ok = true;
            NekoExpr* du = derivOrFree(expr->as.unary.arg, var, &ok);
            if (!ok) return diffErr(NEKO_ERR_UNSUPPORTED);
            return diffOk(nekoSimplify(nekoMul(nekoCos(nekoCloneExpr(expr->as.unary.arg)), du)));
        }

        case NEKO_EXPR_COS: {
            // Apply the chain rule for cos(u)
            bool ok = true;
            NekoExpr* du = derivOrFree(expr->as.unary.arg, var, &ok);
            if (!ok) return diffErr(NEKO_ERR_UNSUPPORTED);
            return diffOk(nekoSimplify(nekoMul(nekoNeg(nekoSin(nekoCloneExpr(expr->as.unary.arg))), du)));
        }

        case NEKO_EXPR_TAN: {
            // Apply the chain rule using sec^2(u)
            bool ok = true;
            NekoExpr* du = derivOrFree(expr->as.unary.arg, var, &ok);
            if (!ok) return diffErr(NEKO_ERR_UNSUPPORTED);

            // Represent sec^2(u) as 1/cos(u)^2
            NekoExpr* sec2 = nekoDiv(nekoConst(1.0),
                                     nekoPow(nekoCos(nekoCloneExpr(expr->as.unary.arg)), nekoConst(2.0)));
            return diffOk(nekoSimplify(nekoMul(sec2, du)));
        }

        case NEKO_EXPR_ASIN: {
            // Clone u and compute u' for the inverse-sine rule
            bool ok = true;
            NekoExpr* u = nekoCloneExpr(expr->as.unary.arg);
            NekoExpr* du = derivOrFree(expr->as.unary.arg, var, &ok);

            // Release temporaries if the inner derivative failed
            if (!ok) {
                nekoFreeExpr(u); nekoFreeExpr(du);
                return diffErr(NEKO_ERR_UNSUPPORTED);
            }

            // Build u'/sqrt(1-u^2)
            return diffOk(nekoSimplify(nekoDiv(du, nekoSqrt(nekoSub(nekoConst(1.0), nekoPow(u, nekoConst(2.0)))))));
        }

        case NEKO_EXPR_ACOS: {
            // Clone u and compute u' for the inverse-cosine rule
            bool ok = true;
            NekoExpr* u = nekoCloneExpr(expr->as.unary.arg);
            NekoExpr* du = derivOrFree(expr->as.unary.arg, var, &ok);

            // Release temporaries if the inner derivative failed
            if (!ok) {
                nekoFreeExpr(u); nekoFreeExpr(du);
                return diffErr(NEKO_ERR_UNSUPPORTED);
            }

            // Build -u'/sqrt(1-u^2)
            return diffOk(nekoSimplify(nekoNeg(nekoDiv(du, nekoSqrt(nekoSub(nekoConst(1.0), nekoPow(u, nekoConst(2.0))))))));
        }

        case NEKO_EXPR_ATAN: {
            // Clone u and compute u' for the inverse-tangent rule
            bool ok = true;
            NekoExpr* u = nekoCloneExpr(expr->as.unary.arg);
            NekoExpr* du = derivOrFree(expr->as.unary.arg, var, &ok);

            // Release temporaries if the inner derivative failed
            if (!ok) {
                nekoFreeExpr(u); nekoFreeExpr(du);
                return diffErr(NEKO_ERR_UNSUPPORTED);
            }

            // Build u'/(1+u^2)
            return diffOk(nekoSimplify(nekoDiv(du, nekoAdd(nekoConst(1.0), nekoPow(u, nekoConst(2.0))))));
        }

        case NEKO_EXPR_EXP: {
            // Apply the chain rule for exp(u)
            bool ok = true;
            NekoExpr* du = derivOrFree(expr->as.unary.arg, var, &ok);
            if (!ok) return diffErr(NEKO_ERR_UNSUPPORTED);
            return diffOk(nekoSimplify(nekoMul(nekoExp(nekoCloneExpr(expr->as.unary.arg)), du)));
        }

        case NEKO_EXPR_LOG: {
            // Apply the chain rule for log(u)
            bool ok = true;
            NekoExpr* du = derivOrFree(expr->as.unary.arg, var, &ok);
            if (!ok) return diffErr(NEKO_ERR_UNSUPPORTED);
            return diffOk(nekoSimplify(nekoDiv(du, nekoCloneExpr(expr->as.unary.arg))));
        }

        case NEKO_EXPR_SQRT: {
            // Apply the chain rule for sqrt(u)
            bool ok = true;
            NekoExpr* du = derivOrFree(expr->as.unary.arg, var, &ok);
            if (!ok) return diffErr(NEKO_ERR_UNSUPPORTED);
            return diffOk(nekoSimplify(nekoDiv(du, nekoMul(nekoConst(2.0), nekoSqrt(nekoCloneExpr(expr->as.unary.arg))))));
        }

        case NEKO_EXPR_ABS: {
            // Clone u and compute u' for the formal absolute-value rule
            bool ok = true;
            NekoExpr* u = nekoCloneExpr(expr->as.unary.arg);
            NekoExpr* du = derivOrFree(expr->as.unary.arg, var, &ok);

            // Release temporaries if the inner derivative failed
            if (!ok) {
                nekoFreeExpr(u); nekoFreeExpr(du);
                return diffErr(NEKO_ERR_UNSUPPORTED);
            }

            // Build u'*(u/abs(u)) away from nondifferentiable points
            return diffOk(nekoSimplify(nekoMul(du, nekoDiv(u, nekoAbs(nekoCloneExpr(expr->as.unary.arg))))));
        }

        case NEKO_EXPR_ERF: {
            // Apply the chain rule for erf(u)
            bool ok = true;
            NekoExpr* du = derivOrFree(expr->as.unary.arg, var, &ok);
            if (!ok) return diffErr(NEKO_ERR_UNSUPPORTED);

            // Build (2/sqrt(pi))*exp(-u^2)*u'
            NekoExpr* scale = nekoConst(2.0L / sqrtl(M_PI));
            NekoExpr* gaussian = nekoExp(nekoNeg(nekoPow(nekoCloneExpr(expr->as.unary.arg), nekoConst(2.0L))));
            return diffOk(nekoSimplify(nekoMul(nekoMul(scale, gaussian), du)));
        }

        case NEKO_EXPR_EI: {
            // Apply the chain rule for Ei(u)
            bool ok = true;
            NekoExpr* du = derivOrFree(expr->as.unary.arg, var, &ok);
            if (!ok) return diffErr(NEKO_ERR_UNSUPPORTED);

            // Build exp(u)*u'/u
            NekoExpr* numerator = nekoMul(nekoExp(nekoCloneExpr(expr->as.unary.arg)), du);
            return diffOk(nekoSimplify(nekoDiv(numerator, nekoCloneExpr(expr->as.unary.arg))));
        }

        case NEKO_EXPR_STEP:
            // The step function is intentionally unsupported by symbolic differentiation
            return diffErr(NEKO_ERR_UNSUPPORTED);

        case NEKO_EXPR_CALL:
            // Calls are intentionally unsupported by symbolic differentiation
            return diffErr(NEKO_ERR_UNSUPPORTED);
    }

    // Unknown expression kinds are treated as unsupported
    return diffErr(NEKO_ERR_UNSUPPORTED);
}

/* ---------- Function wrappers ---------- */

// Build a numeric function wrapper from an expression
NekoFunc* nekoFuncFromExpr(const NekoExpr* expr) {
    // Require an expression to clone into the function wrapper
    if (!expr) return NULL;

    // Allocate the wrapper and clone the expression into it
    NekoFunc* func = calloc(1, sizeof(NekoFunc));
    if (!func) return NULL;
    func->expr = nekoCloneExpr(expr);

    // Release the wrapper if cloning the expression failed
    if (!func->expr) {
        free(func);
        return NULL;
    }
    return func;
}

// Build a numeric function wrapper from a callback
NekoFunc* nekoFuncFromCallback(NekoEvalFn callback, void* userdata) {
    // Require an actual callback function
    if (!callback) return NULL;

    // Allocate the wrapper around the callback and user data
    NekoFunc* func = calloc(1, sizeof(NekoFunc));
    if (!func) return NULL;
    func->callback = callback;
    func->userdata = userdata;
    return func;
}

// Free a numeric function wrapper
void nekoFreeFunc(NekoFunc* func) {
    // Treat NULL as an already-freed function wrapper
    if (!func) return;

    // Free any expression payload before releasing the wrapper
    nekoFreeExpr(func->expr);
    free(func);
}

// Evaluate a numeric function wrapper at x
long double nekoEvalFunc(const NekoFunc* func, long double x) {
    // Missing function wrappers evaluate to NAN
    if (!func) return NAN;

    // Prefer callback evaluation when a callback is present
    if (func->callback) return func->callback(x, func->userdata);

    // Otherwise evaluate the stored expression in the variable x
    if (func->expr) return nekoEvalExpr(func->expr, "x", x);

    // Empty wrappers cannot produce a numeric value
    return NAN;
}

/* ---------- Symbolic integration ---------- */

// Wrap a successful integration result
static NekoIntegralResult integOk(NekoExpr* expr) {
    // Encode allocation failure as an invalid-argument status
    NekoIntegralResult r = { .status = expr ? NEKO_OK : NEKO_ERR_INVALID_ARG, .expr = expr };
    return r;
}

// Wrap a failed integration result
static NekoIntegralResult integErr(NekoStatus status) {
    // Return the requested error with no expression payload
    NekoIntegralResult r = { .status = status, .expr = NULL };
    return r;
}

// Integrate a child expression or mark the parent operation failed
static NekoExpr* integOrFree(const NekoExpr* expr, const char* var, bool* ok) {
    // Delegate to the public symbolic integrator for the child subtree
    NekoIntegralResult r = nekoIntegrateExpr(expr, var);

    // Propagate failure through the shared ok flag
    if (r.status != NEKO_OK) {
        *ok = false;
        nekoFreeExpr(r.expr);
        return NULL;
    }
    return r.expr;
}

// Extract linear coefficients a and b from a*x+b
static bool linearCoeff(const NekoExpr* expr, const char* var, long double* a, long double* b) {
    // Require an expression and output coefficient storage
    if (!expr || !a || !b) return false;

    // Constants are linear with zero slope
    if (expr->kind == NEKO_EXPR_CONST) {
        *a = 0.0;
        *b = expr->as.constant;
        return true;
    }

    // The selected variable is 1*x+0
    if (sameVar(expr, var)) {
        *a = 1.0;
        *b = 0.0;
        return true;
    }

    // Negation flips both linear coefficients
    if (expr->kind == NEKO_EXPR_NEG) {
        long double ca, cb;
        if (!linearCoeff(expr->as.unary.arg, var, &ca, &cb)) return false;
        *a = -ca;
        *b = -cb;
        return true;
    }

    // Additive expressions combine linear coefficients termwise
    if (expr->kind == NEKO_EXPR_ADD || expr->kind == NEKO_EXPR_SUB) {
        long double la, lb, ra, rb;
        if (!linearCoeff(expr->as.binary.lhs, var, &la, &lb)) return false;
        if (!linearCoeff(expr->as.binary.rhs, var, &ra, &rb)) return false;
        *a = expr->kind == NEKO_EXPR_ADD ? la + ra : la - ra;
        *b = expr->kind == NEKO_EXPR_ADD ? lb + rb : lb - rb;
        return true;
    }

    // Multiplication is linear only when exactly one factor is constant
    if (expr->kind == NEKO_EXPR_MUL) {
        const NekoExpr* lhs = expr->as.binary.lhs;
        const NekoExpr* rhs = expr->as.binary.rhs;
        long double ca, cb;

        // Constant on the left scales the right-side linear coefficients
        if (lhs->kind == NEKO_EXPR_CONST && linearCoeff(rhs, var, &ca, &cb)) {
            *a = lhs->as.constant * ca;
            *b = lhs->as.constant * cb;
            return true;
        }

        // Constant on the right scales the left-side linear coefficients
        if (rhs->kind == NEKO_EXPR_CONST && linearCoeff(lhs, var, &ca, &cb)) {
            *a = rhs->as.constant * ca;
            *b = rhs->as.constant * cb;
            return true;
        }
    }

    // Other expression shapes are not recognized as linear
    return false;
}

// Split an expression into a constant multiplier and inner expression
static bool splitConstMultiple(const NekoExpr* expr, NekoExpr** inner, long double* coeff) {
    // This owned split only recognizes explicit multiplication by a constant
    if (!expr || !inner || !coeff || expr->kind != NEKO_EXPR_MUL) return false;

    // Pull a constant from the left side and clone the right side
    if (expr->as.binary.lhs->kind == NEKO_EXPR_CONST) {
        *coeff = expr->as.binary.lhs->as.constant;
        *inner = nekoCloneExpr(expr->as.binary.rhs);
        return true;
    }

    // Pull a constant from the right side and clone the left side
    if (expr->as.binary.rhs->kind == NEKO_EXPR_CONST) {
        *coeff = expr->as.binary.rhs->as.constant;
        *inner = nekoCloneExpr(expr->as.binary.lhs);
        return true;
    }

    // Non-constant products are not split here
    return false;
}

// Compare two expressions for structural equality
static bool exprEqual(const NekoExpr* a, const NekoExpr* b) {
    // Expressions with different shapes are never structurally equal
    if (!a || !b || a->kind != b->kind) return false;

    // Compare the payload appropriate to the common expression kind
    switch (a->kind) {
        case NEKO_EXPR_CONST:
            return fabsl(a->as.constant - b->as.constant) <= 1e-9;
        case NEKO_EXPR_VAR:
            return a->as.var && b->as.var && strcmp(a->as.var, b->as.var) == 0;
        case NEKO_EXPR_ADD:
        case NEKO_EXPR_SUB:
        case NEKO_EXPR_MUL:
        case NEKO_EXPR_DIV:
        case NEKO_EXPR_POW:
            return exprEqual(a->as.binary.lhs, b->as.binary.lhs)
                && exprEqual(a->as.binary.rhs, b->as.binary.rhs);
        case NEKO_EXPR_NEG:
        case NEKO_EXPR_SIN:
        case NEKO_EXPR_COS:
        case NEKO_EXPR_TAN:
        case NEKO_EXPR_ASIN:
        case NEKO_EXPR_ACOS:
        case NEKO_EXPR_ATAN:
        case NEKO_EXPR_EXP:
        case NEKO_EXPR_LOG:
        case NEKO_EXPR_SQRT:
        case NEKO_EXPR_ABS:
        case NEKO_EXPR_ERF:
        case NEKO_EXPR_EI:
        case NEKO_EXPR_STEP:
            return exprEqual(a->as.unary.arg, b->as.unary.arg);
        case NEKO_EXPR_CALL:
            // Calls must have matching names and arity before comparing arguments
            if (!a->as.call.name || !b->as.call.name || strcmp(a->as.call.name, b->as.call.name) != 0
                    || a->as.call.nargs != b->as.call.nargs) return false;

            // Compare each call argument in order
            for (int i = 0; i < a->as.call.nargs; i++)
                if (!exprEqual(a->as.call.args[i], b->as.call.args[i])) return false;
            return true;
    }

    // Unknown expression kinds are treated as unequal
    return false;
}

// Borrow-split an expression into a constant multiplier and inner expression
static bool splitConstBorrowed(const NekoExpr* expr, const NekoExpr** inner, long double* coeff) {
    // Require all output channels before borrowing expression pieces
    if (!expr || !inner || !coeff) return false;

    // Constants have no symbolic inner expression
    if (expr->kind == NEKO_EXPR_CONST) {
        *inner = NULL;
        *coeff = expr->as.constant;
        return true;
    }

    // Explicit multiplication by a constant exposes the other factor
    if (expr->kind == NEKO_EXPR_MUL) {
        // Borrow the right factor when the left factor is constant
        if (expr->as.binary.lhs->kind == NEKO_EXPR_CONST) {
            *coeff = expr->as.binary.lhs->as.constant;
            *inner = expr->as.binary.rhs;
            return true;
        }

        // Borrow the left factor when the right factor is constant
        if (expr->as.binary.rhs->kind == NEKO_EXPR_CONST) {
            *coeff = expr->as.binary.rhs->as.constant;
            *inner = expr->as.binary.lhs;
            return true;
        }
    }

    // Expressions without an explicit scalar have coefficient one
    *coeff = 1.0;
    *inner = expr;
    return true;
}

// Test whether one expression is a constant multiple of another
static bool proportionalTo(const NekoExpr* a, const NekoExpr* b, long double* coeff) {
    // Require both expressions and an output coefficient
    if (!a || !b || !coeff) return false;

    // Simplify clones so scalar factors are easy to identify
    NekoExpr* sa = nekoSimplify(nekoCloneExpr(a));
    NekoExpr* sb = nekoSimplify(nekoCloneExpr(b));

    // Borrow each simplified expression as scalar times symbolic core
    const NekoExpr* ia = NULL;
    const NekoExpr* ib = NULL;
    long double ca = 1.0, cb = 1.0;
    splitConstBorrowed(sa, &ia, &ca);
    splitConstBorrowed(sb, &ib, &cb);

    // Compare the symbolic cores and compute the scalar ratio when valid
    bool ok = false;
    if (!ia && !ib && fabsl(cb) > 1e-12) {
        *coeff = ca / cb;
        ok = true;
    } else if (ia && ib && exprEqual(ia, ib) && fabsl(cb) > 1e-12) {
        *coeff = ca / cb;
        ok = true;
    }

    // Release the simplified clones before returning the result
    nekoFreeExpr(sa);
    nekoFreeExpr(sb);
    return ok;
}

// Integrate a power of a linear expression
static NekoExpr* integratePowerOfLinear(const NekoExpr* base, long double exponent, const char* var) {
    // Extract the linear slope and reject non-linear or constant bases
    long double a, b;
    if (!linearCoeff(base, var, &a, &b) || fabsl(a) <= 1e-12) return NULL;

    // The exponent -1 integrates to log(abs(base))/a
    if (fabsl(exponent + 1.0) <= 1e-12) {
        return nekoDiv(nekoLog(nekoAbs(nekoCloneExpr(base))), nekoConst(a));
    }

    // Other powers use base^(n+1)/(a*(n+1))
    return nekoDiv(nekoPow(nekoCloneExpr(base), nekoConst(exponent + 1.0)),
                   nekoConst(a * (exponent + 1.0)));
}

// Decode a positive integer exponent
static bool positiveIntegerPower(long double x, int* n) {
    // Round and reject values outside the small supported exponent range
    long double r = roundl(x);
    if (fabsl(x - r) > 1e-9 || r < 0.0 || r > 64.0) return false;

    // Return the exponent when the caller requested it
    if (n) *n = (int)r;
    return true;
}

// Integrate a supported trig power with a linear argument
static NekoExpr* integrateTrigPowerLinear(NekoExprKind trigKind, const NekoExpr* arg, int n, const char* var) {
    // Extract the linear argument scale needed for substitution
    long double a, b;
    if (!linearCoeff(arg, var, &a, &b) || fabsl(a) <= 1e-12) return NULL;
    (void)b;

    // The zeroth power integrates to the integration variable
    if (n == 0) return nekoVar(var);

    // Use direct antiderivatives for first powers
    if (n == 1) {
        if (trigKind == NEKO_EXPR_SIN) return nekoDiv(nekoNeg(nekoCos(nekoCloneExpr(arg))), nekoConst(a));
        if (trigKind == NEKO_EXPR_COS) return nekoDiv(nekoSin(nekoCloneExpr(arg)), nekoConst(a));
        if (trigKind == NEKO_EXPR_TAN) return nekoDiv(nekoNeg(nekoLog(nekoCos(nekoCloneExpr(arg)))), nekoConst(a));
    }

    // Recursively integrate the power reduced by two
    NekoExpr* prev = integrateTrigPowerLinear(trigKind, arg, n - 2, var);
    if (!prev) return NULL;

    // Apply the sine power reduction formula
    NekoExpr* term = NULL;
    if (trigKind == NEKO_EXPR_SIN) {
        term = nekoDiv(nekoNeg(nekoMul(nekoPow(nekoSin(nekoCloneExpr(arg)), nekoConst((long double)n - 1.0)),
                                      nekoCos(nekoCloneExpr(arg)))),
                       nekoConst(a * (long double)n));
        return nekoAdd(term, nekoMul(nekoConst(((long double)n - 1.0) / (long double)n), prev));
    }

    // Apply the cosine power reduction formula
    if (trigKind == NEKO_EXPR_COS) {
        term = nekoDiv(nekoMul(nekoSin(nekoCloneExpr(arg)),
                               nekoPow(nekoCos(nekoCloneExpr(arg)), nekoConst((long double)n - 1.0))),
                       nekoConst(a * (long double)n));
        return nekoAdd(term, nekoMul(nekoConst(((long double)n - 1.0) / (long double)n), prev));
    }

    // Apply the tangent reduction formula with its special square case
    if (trigKind == NEKO_EXPR_TAN) {
        if (n == 2) {
            nekoFreeExpr(prev);
            return nekoSub(nekoDiv(nekoTan(nekoCloneExpr(arg)), nekoConst(a)), nekoVar(var));
        }
        term = nekoDiv(nekoPow(nekoTan(nekoCloneExpr(arg)), nekoConst((long double)n - 1.0)),
                       nekoConst(a * ((long double)n - 1.0)));
        return nekoSub(term, prev);
    }

    // Release the recursive result if the trig kind is unsupported
    nekoFreeExpr(prev);
    return NULL;
}

// Try to integrate a supported trigonometric power expression
static NekoExpr* tryTrigPowerIntegral(const NekoExpr* expr, const char* var) {
    // Recognize only explicit powers with constant exponents
    if (!expr || expr->kind != NEKO_EXPR_POW || expr->as.binary.rhs->kind != NEKO_EXPR_CONST) return NULL;

    // Require the base to be one of the supported trigonometric functions
    const NekoExpr* base = expr->as.binary.lhs;
    if (!base || (base->kind != NEKO_EXPR_SIN && base->kind != NEKO_EXPR_COS && base->kind != NEKO_EXPR_TAN)) return NULL;

    // Decode the exponent and dispatch to the trig power integrator
    int n = 0;
    if (!positiveIntegerPower(expr->as.binary.rhs->as.constant, &n)) return NULL;
    return integrateTrigPowerLinear(base->kind, base->as.unary.arg, n, var);
}

// Try a u-substitution integration pair
static NekoExpr* integrateUSubPair(const NekoExpr* factor, const NekoExpr* outer, const char* var) {
    // Require both pieces of the candidate product
    if (!factor || !outer) return NULL;

    // Identify the inner function of the outer expression
    const NekoExpr* inner = NULL;
    if (isUnaryKind(outer->kind)) {
        inner = outer->as.unary.arg;
    } else if (outer->kind == NEKO_EXPR_POW) {
        inner = outer->as.binary.lhs;
    } else if (outer->kind == NEKO_EXPR_DIV && isConst(outer->as.binary.lhs, 1.0)) {
        inner = outer->as.binary.rhs;
    } else {
        return NULL;
    }

    // Differentiate the inner function to compare against the factor
    NekoDiffResult d = nekoDifferentiateExpr(inner, var);
    if (d.status != NEKO_OK) {
        nekoFreeExpr(d.expr);
        return NULL;
    }

    // Accept the factor only when it is proportional to the inner derivative
    long double coeff = 0.0;
    bool ok = proportionalTo(factor, d.expr, &coeff);
    nekoFreeExpr(d.expr);
    if (!ok) return NULL;

    // Build the antiderivative of the outer expression with respect to the inner
    NekoExpr* antiderivative = NULL;
    switch (outer->kind) {
        case NEKO_EXPR_SIN:
            antiderivative = nekoNeg(nekoCos(nekoCloneExpr(inner)));
            break;
        case NEKO_EXPR_COS:
            antiderivative = nekoSin(nekoCloneExpr(inner));
            break;
        case NEKO_EXPR_TAN:
            antiderivative = nekoNeg(nekoLog(nekoCos(nekoCloneExpr(inner))));
            break;
        case NEKO_EXPR_EXP:
            antiderivative = nekoExp(nekoCloneExpr(inner));
            break;
        case NEKO_EXPR_LOG:
            antiderivative = nekoSub(nekoMul(nekoCloneExpr(inner), nekoLog(nekoCloneExpr(inner))),
                                     nekoCloneExpr(inner));
            break;
        case NEKO_EXPR_POW:
            // Integrate powers of the inner expression, including reciprocal powers
            if (outer->as.binary.rhs->kind == NEKO_EXPR_CONST) {
                long double n = outer->as.binary.rhs->as.constant;
                antiderivative = fabsl(n + 1.0) <= 1e-12
                    ? nekoLog(nekoAbs(nekoCloneExpr(inner)))
                    : nekoDiv(nekoPow(nekoCloneExpr(inner), nekoConst(n + 1.0)), nekoConst(n + 1.0));
            }
            break;
        case NEKO_EXPR_DIV:
            antiderivative = nekoLog(nekoAbs(nekoCloneExpr(inner)));
            break;
        default:
            break;
    }

    // Scale the inner antiderivative by the detected proportionality constant
    return antiderivative ? nekoMul(nekoConst(coeff), antiderivative) : NULL;
}

// Integrate an expression using supported u-substitution patterns
NekoIntegralResult nekoIntegrateUSubExpr(const NekoExpr* expr, const char* var) {
    // Require a product expression so one side can be u'
    if (!expr || !var) return integErr(NEKO_ERR_INVALID_ARG);
    if (expr->kind != NEKO_EXPR_MUL) return integErr(NEKO_ERR_UNSUPPORTED);

    // Try both factor orderings as derivative times outer expression
    NekoExpr* out = integrateUSubPair(expr->as.binary.lhs, expr->as.binary.rhs, var);
    if (!out) out = integrateUSubPair(expr->as.binary.rhs, expr->as.binary.lhs, var);

    // Return the simplified antiderivative or report unsupported
    return out ? integOk(nekoSimplify(out)) : integErr(NEKO_ERR_UNSUPPORTED);
}

// Report whether u-substitution integration is available
bool nekoCanIntegrateUSub(const NekoExpr* expr, const char* var) {
    // Attempt integration using the u-substitution entry point
    NekoIntegralResult r = nekoIntegrateUSubExpr(expr, var);

    // Convert the result status to a boolean and release any expression
    bool ok = r.status == NEKO_OK;
    nekoFreeExpr(r.expr);
    return ok;
}

// Symbolically integrate an expression with respect to a variable
NekoIntegralResult nekoIntegrateExpr(const NekoExpr* expr, const char* var) {
    // Require both an expression and integration variable
    if (!expr || !var) return integErr(NEKO_ERR_INVALID_ARG);

    // Dispatch by expression kind and apply the supported integration rule
    switch (expr->kind) {
        case NEKO_EXPR_CONST:
            return integOk(nekoSimplify(nekoMul(nekoConst(expr->as.constant), nekoVar(var))));

        case NEKO_EXPR_VAR:
            if (!sameVar(expr, var)) return integErr(NEKO_ERR_UNSUPPORTED);
            return integOk(nekoSimplify(nekoDiv(nekoPow(nekoVar(var), nekoConst(2.0)), nekoConst(2.0))));

        case NEKO_EXPR_ADD:
        case NEKO_EXPR_SUB: {
            // Integrate both operands of additive expressions
            bool ok = true;
            NekoExpr* il = integOrFree(expr->as.binary.lhs, var, &ok);
            NekoExpr* ir = integOrFree(expr->as.binary.rhs, var, &ok);

            // Clean up partial integrals if either side is unsupported
            if (!ok) {
                nekoFreeExpr(il);
                nekoFreeExpr(ir);
                return integErr(NEKO_ERR_UNSUPPORTED);
            }

            // Reassemble the integral with the original additive operator
            return integOk(nekoSimplify(expr->kind == NEKO_EXPR_ADD ? nekoAdd(il, ir) : nekoSub(il, ir)));
        }

        case NEKO_EXPR_NEG: {
            // Integrate the inner expression and negate the result
            NekoIntegralResult r = nekoIntegrateExpr(expr->as.unary.arg, var);
            if (r.status != NEKO_OK) return r;
            return integOk(nekoSimplify(nekoNeg(r.expr)));
        }

        case NEKO_EXPR_MUL: {
            // Try u-substitution before falling back to constant multiples
            NekoIntegralResult usub = nekoIntegrateUSubExpr(expr, var);
            if (usub.status == NEKO_OK) return usub;

            // Split explicit constant multiples from the integrand
            NekoExpr* inner = NULL;
            long double coeff = 0.0;
            if (!splitConstMultiple(expr, &inner, &coeff)) return integErr(NEKO_ERR_UNSUPPORTED);

            // Integrate the nonconstant inner expression
            NekoIntegralResult r = nekoIntegrateExpr(inner, var);
            nekoFreeExpr(inner);
            if (r.status != NEKO_OK) return r;

            // Restore the constant multiplier around the antiderivative
            return integOk(nekoSimplify(nekoMul(nekoConst(coeff), r.expr)));
        }

        case NEKO_EXPR_DIV: {
            // Rewrite constant-over-expression as a constant multiple of a reciprocal
            if (expr->as.binary.lhs->kind == NEKO_EXPR_CONST) {
                NekoExpr* reciprocal = NULL;

                // Preserve existing powers by negating their exponent
                if (expr->as.binary.rhs->kind == NEKO_EXPR_POW
                        && expr->as.binary.rhs->as.binary.rhs->kind == NEKO_EXPR_CONST) {
                    reciprocal = nekoPow(
                        nekoCloneExpr(expr->as.binary.rhs->as.binary.lhs),
                        nekoConst(-expr->as.binary.rhs->as.binary.rhs->as.constant)
                    );
                } else {
                    reciprocal = nekoPow(nekoCloneExpr(expr->as.binary.rhs), nekoConst(-1.0));
                }

                // Integrate the reciprocal form and restore the numerator constant
                NekoIntegralResult r = nekoIntegrateExpr(reciprocal, var);
                nekoFreeExpr(reciprocal);
                if (r.status != NEKO_OK) return r;
                return integOk(nekoSimplify(nekoMul(nekoConst(expr->as.binary.lhs->as.constant), r.expr)));
            }
            return integErr(NEKO_ERR_UNSUPPORTED);
        }

        case NEKO_EXPR_POW:
            // Integrate constant powers through trig-power or linear-power patterns
            if (expr->as.binary.rhs->kind == NEKO_EXPR_CONST) {
                NekoExpr* trig = tryTrigPowerIntegral(expr, var);
                if (trig) return integOk(nekoSimplify(trig));
                NekoExpr* out = integratePowerOfLinear(expr->as.binary.lhs, expr->as.binary.rhs->as.constant, var);
                return out ? integOk(nekoSimplify(out)) : integErr(NEKO_ERR_UNSUPPORTED);
            }
            return integErr(NEKO_ERR_UNSUPPORTED);

        case NEKO_EXPR_SIN: {
            // Integrate sine of a linear argument
            long double a, b;
            if (!linearCoeff(expr->as.unary.arg, var, &a, &b) || fabsl(a) <= 1e-12) return integErr(NEKO_ERR_UNSUPPORTED);
            (void)b;
            return integOk(nekoSimplify(nekoDiv(nekoNeg(nekoCos(nekoCloneExpr(expr->as.unary.arg))), nekoConst(a))));
        }

        case NEKO_EXPR_COS: {
            // Integrate cosine of a linear argument
            long double a, b;
            if (!linearCoeff(expr->as.unary.arg, var, &a, &b) || fabsl(a) <= 1e-12) return integErr(NEKO_ERR_UNSUPPORTED);
            (void)b;
            return integOk(nekoSimplify(nekoDiv(nekoSin(nekoCloneExpr(expr->as.unary.arg)), nekoConst(a))));
        }

        case NEKO_EXPR_TAN: {
            // Integrate tangent of a linear argument
            long double a, b;
            if (!linearCoeff(expr->as.unary.arg, var, &a, &b) || fabsl(a) <= 1e-12) return integErr(NEKO_ERR_UNSUPPORTED);
            (void)b;
            return integOk(nekoSimplify(nekoDiv(nekoNeg(nekoLog(nekoCos(nekoCloneExpr(expr->as.unary.arg)))), nekoConst(a))));
        }

        case NEKO_EXPR_EXP: {
            // Integrate exponential of a linear argument
            long double a, b;
            if (!linearCoeff(expr->as.unary.arg, var, &a, &b) || fabsl(a) <= 1e-12) return integErr(NEKO_ERR_UNSUPPORTED);
            (void)b;
            return integOk(nekoSimplify(nekoDiv(nekoExp(nekoCloneExpr(expr->as.unary.arg)), nekoConst(a))));
        }

        case NEKO_EXPR_LOG:
            // Support the basic antiderivative of log(x)
            if (sameVar(expr->as.unary.arg, var)) {
                return integOk(nekoSimplify(nekoSub(nekoMul(nekoVar(var), nekoLog(nekoVar(var))), nekoVar(var))));
            }
            return integErr(NEKO_ERR_UNSUPPORTED);

        case NEKO_EXPR_ASIN:
            // Support the basic antiderivative of asin(x)
            if (sameVar(expr->as.unary.arg, var)) {
                NekoExpr* out = nekoAdd(nekoMul(nekoVar(var), nekoAsin(nekoVar(var))),
                                        nekoSqrt(nekoSub(nekoConst(1.0), nekoPow(nekoVar(var), nekoConst(2.0)))));
                return integOk(nekoSimplify(out));
            }
            return integErr(NEKO_ERR_UNSUPPORTED);

        case NEKO_EXPR_ACOS:
            // Support the basic antiderivative of acos(x)
            if (sameVar(expr->as.unary.arg, var)) {
                NekoExpr* out = nekoSub(nekoMul(nekoVar(var), nekoAcos(nekoVar(var))),
                                        nekoSqrt(nekoSub(nekoConst(1.0), nekoPow(nekoVar(var), nekoConst(2.0)))));
                return integOk(nekoSimplify(out));
            }
            return integErr(NEKO_ERR_UNSUPPORTED);

        case NEKO_EXPR_ATAN:
            // Support the basic antiderivative of atan(x)
            if (sameVar(expr->as.unary.arg, var)) {
                NekoExpr* out = nekoSub(nekoMul(nekoVar(var), nekoAtan(nekoVar(var))),
                                        nekoMul(nekoConst(0.5),
                                                nekoLog(nekoAdd(nekoConst(1.0), nekoPow(nekoVar(var), nekoConst(2.0))))));
                return integOk(nekoSimplify(out));
            }
            return integErr(NEKO_ERR_UNSUPPORTED);

        case NEKO_EXPR_SQRT: {
            // Rewrite sqrt(u) as u^(1/2) and reuse power integration
            NekoExpr* asPower = nekoPow(nekoCloneExpr(expr->as.unary.arg), nekoConst(0.5));
            NekoIntegralResult r = nekoIntegrateExpr(asPower, var);
            nekoFreeExpr(asPower);
            return r;
        }

        case NEKO_EXPR_ABS:
        case NEKO_EXPR_ERF:
        case NEKO_EXPR_EI:
        case NEKO_EXPR_STEP:
        case NEKO_EXPR_CALL:
            // These unary forms and calls are intentionally unsupported here
            return integErr(NEKO_ERR_UNSUPPORTED);
    }

    // Unknown expression kinds are treated as unsupported
    return integErr(NEKO_ERR_UNSUPPORTED);
}

/* ---------- Numerical integration and applications ---------- */

// Estimate an integral with one Simpson-rule panel
static long double simpsonRaw(const NekoFunc* func, long double a, long double b) {
    // Use the midpoint of the interval for the Simpson panel
    long double c = 0.5 * (a + b);

    // Combine endpoint and midpoint samples with Simpson weights
    return (b - a) * (nekoEvalFunc(func, a) + 4.0 * nekoEvalFunc(func, c) + nekoEvalFunc(func, b)) / 6.0;
}

// Refine an adaptive Simpson integral recursively
static long double adaptiveSimpsonRecur(const NekoFunc* func, long double a, long double b,
                                   long double eps, long double whole, int depth) {
    // Split the interval and estimate both halves
    long double c = 0.5 * (a + b);
    long double left = simpsonRaw(func, a, c);
    long double right = simpsonRaw(func, c, b);

    // Compare the refined estimate against the original whole-panel estimate
    long double delta = left + right - whole;
    if (depth <= 0 || fabsl(delta) <= 15.0 * eps) return left + right + delta / 15.0;

    // Recurse on both halves with half the tolerance budget
    return adaptiveSimpsonRecur(func, a, c, eps * 0.5, left, depth - 1)
         + adaptiveSimpsonRecur(func, c, b, eps * 0.5, right, depth - 1);
}

// Numerically integrate a function over an interval
NekoNumericResult nekoIntegrateNumeric(const NekoFunc* func, long double a, long double b,
                                       NekoIntegrateMethod method, int intervals, long double tol) {
    // Initialize the result with the caller-provided interval count
    NekoNumericResult r = { .status = NEKO_OK, .value = NAN, .intervals = intervals };

    // Reject missing functions and non-finite integration bounds
    if (!func || !isfinite(a) || !isfinite(b)) {
        r.status = NEKO_ERR_INVALID_ARG;
        return r;
    }

    // A zero-width interval has integral zero and no subintervals
    if (a == b) {
        r.value = 0.0;
        r.intervals = 0;
        return r;
    }

    // Normalize reversed bounds and remember the sign change
    long double sign = 1.0;
    if (b < a) {
        long double tmp = a;
        a = b;
        b = tmp;
        sign = -1.0;
    }

    // Use recursive adaptive Simpson when requested
    if (method == NEKO_INTEGRATE_ADAPTIVE_SIMPSON) {
        long double eps = tol > 0.0 ? tol : 1e-8;
        long double whole = simpsonRaw(func, a, b);
        r.value = sign * adaptiveSimpsonRecur(func, a, b, eps, whole, 20);
        r.intervals = 0;
        return r;
    }

    // Choose a default interval count and make Simpson counts even
    if (intervals < 1) intervals = 1024;
    if (method == NEKO_INTEGRATE_SIMPSON && intervals % 2) intervals++;
    r.intervals = intervals;

    // Compute the uniform grid spacing used by composite rules
    long double h = (b - a) / intervals;
    long double sum = 0.0;

    // Apply the composite trapezoid rule
    if (method == NEKO_INTEGRATE_TRAPEZOID) {
        sum = 0.5 * (nekoEvalFunc(func, a) + nekoEvalFunc(func, b));
        for (int i = 1; i < intervals; i++) sum += nekoEvalFunc(func, a + i * h);
        r.value = sign * h * sum;
        return r;
    }

    // Apply the composite Simpson rule
    sum = nekoEvalFunc(func, a) + nekoEvalFunc(func, b);
    for (int i = 1; i < intervals; i++) {
        sum += (i % 2 ? 4.0 : 2.0) * nekoEvalFunc(func, a + i * h);
    }

    // Scale the Simpson sum and restore the original orientation
    r.value = sign * h * sum / 3.0;
    return r;
}

typedef struct {
    const NekoFunc* f;
    const NekoFunc* g;
} AreaBetweenData;

// Evaluate the vertical distance between two functions
static long double areaBetweenEval(long double x, void* userdata) {
    // Interpret the callback data as the two functions being compared
    AreaBetweenData* data = userdata;

    // Return the absolute vertical separation at the sampled x value
    return fabsl(nekoEvalFunc(data->f, x) - nekoEvalFunc(data->g, x));
}

// Numerically compute area between two functions
NekoNumericResult nekoAreaBetween(const NekoFunc* f, const NekoFunc* g, long double a, long double b,
                                  NekoIntegrateMethod method, int intervals, long double tol) {
    // Require both functions before constructing the integration callback
    if (!f || !g) {
        NekoNumericResult r = { .status = NEKO_ERR_INVALID_ARG, .value = NAN, .intervals = intervals };
        return r;
    }

    // Wrap the pair as a single absolute-distance function
    AreaBetweenData data = { .f = f, .g = g };
    NekoFunc wrapped = { .callback = areaBetweenEval, .userdata = &data };

    // Integrate the absolute-distance function over the requested interval
    return nekoIntegrateNumeric(&wrapped, a, b, method, intervals, tol);
}

// Evaluate an optimization objective with goal-adjusted sign
static long double optEval(const NekoFunc* func, long double x, NekoOptGoal goal) {
    // Evaluate the original objective first
    long double y = nekoEvalFunc(func, x);

    // Maxima become minima by negating the objective value
    return goal == NEKO_OPT_MINIMIZE ? y : -y;
}

// Optimize a one-dimensional function by golden-section search
static NekoOptResult goldenSection(const NekoFunc* func, long double a, long double b,
                                   NekoOptGoal goal, long double tol, int maxIter) {
    // Initialize an empty successful-looking result that later checks may update
    NekoOptResult r = { .status = NEKO_OK, .x = NAN, .value = NAN, .iterations = 0 };

    // Reject invalid functions and intervals
    if (!func || !isfinite(a) || !isfinite(b) || a > b) {
        r.status = NEKO_ERR_INVALID_ARG;
        return r;
    }

    // A single-point interval evaluates directly
    if (a == b) {
        r.x = a;
        r.value = nekoEvalFunc(func, a);
        return r;
    }

    // Choose default tolerances and iteration limits when needed
    if (tol <= 0.0) tol = 1e-8;
    if (maxIter < 1) maxIter = 128;

    // Seed the two interior golden-section probe points
    const long double invphi = (sqrtl(5.0) - 1.0) / 2.0;
    long double c = b - invphi * (b - a);
    long double d = a + invphi * (b - a);
    long double fc = optEval(func, c, goal);
    long double fd = optEval(func, d, goal);

    // Shrink the bracket until the interval is small enough or iterations run out
    for (int i = 0; i < maxIter && fabsl(b - a) > tol; i++) {
        if (fc < fd) {
            // Keep the left subinterval when c is better than d
            b = d;
            d = c;
            fd = fc;
            c = b - invphi * (b - a);
            fc = optEval(func, c, goal);
        } else {
            // Keep the right subinterval when d is at least as good as c
            a = c;
            c = d;
            fc = fd;
            d = a + invphi * (b - a);
            fd = optEval(func, d, goal);
        }
        r.iterations = i + 1;
    }

    // Report the midpoint of the final bracket in the original objective scale
    r.x = 0.5 * (a + b);
    r.value = nekoEvalFunc(func, r.x);

    // Flag domain errors when the final objective value is not finite
    if (!isfinite(r.value)) r.status = NEKO_ERR_DOMAIN;
    return r;
}

// Find a local minimum on an interval
NekoOptResult nekoFindMinimum(const NekoFunc* func, long double a, long double b, long double tol, int maxIter) {
    return goldenSection(func, a, b, NEKO_OPT_MINIMIZE, tol, maxIter);
}

// Find a local maximum on an interval
NekoOptResult nekoFindMaximum(const NekoFunc* func, long double a, long double b, long double tol, int maxIter) {
    return goldenSection(func, a, b, NEKO_OPT_MAXIMIZE, tol, maxIter);
}

// Test whether all numeric constraints are feasible at x
static bool feasibleAt(long double x, const NekoConstraint* constraints, int nconstraints) {
    // Check each inequality constraint in sequence
    for (int i = 0; i < nconstraints; i++) {
        if (!constraints[i].func) return false;
        long double v = nekoEvalFunc(constraints[i].func, x);
        if (!isfinite(v) || v < -1e-10) return false;
    }

    // All constraints accepted the point
    return true;
}

// Optimize an objective with optional inequality constraints
NekoOptResult nekoOptimize(const NekoFunc* objective, long double a, long double b, NekoOptGoal goal,
                           const NekoConstraint* constraints, int nconstraints,
                           int samples, long double tol, int maxIter) {
    // Start with no feasible point until a candidate interval succeeds
    NekoOptResult best = { .status = NEKO_ERR_NO_FEASIBLE_POINT, .x = NAN, .value = NAN, .iterations = 0 };

    // Validate the objective, search interval, and constraint array
    if (!objective || !isfinite(a) || !isfinite(b) || a > b || nconstraints < 0
            || (nconstraints > 0 && !constraints)) {
        best.status = NEKO_ERR_INVALID_ARG;
        return best;
    }

    // Use a reasonable default sampling grid for feasible intervals
    if (samples < 8) samples = 256;

    // Initialize the first possible feasible segment
    bool haveBest = false;
    long double segStart = NAN;
    bool inSeg = false;
    long double prevX = a;
    bool prevFeasible = feasibleAt(prevX, constraints, nconstraints);
    if (prevFeasible) {
        segStart = a;
        inSeg = true;
    }

    // Scan the interval for contiguous feasible segments
    for (int i = 1; i <= samples; i++) {
        long double x = a + (b - a) * (long double)i / (long double)samples;
        bool feasible = feasibleAt(x, constraints, nconstraints);

        // Begin a feasible segment at the previous sample boundary
        if (feasible && !inSeg) {
            segStart = prevX;
            inSeg = true;
        }

        // Optimize each feasible segment when it ends
        if ((!feasible || i == samples) && inSeg) {
            long double segEnd = feasible ? x : prevX;
            if (segEnd >= segStart) {
                NekoOptResult candidate = goldenSection(objective, segStart, segEnd, goal, tol, maxIter);

                // Accept only candidates that remain feasible after local optimization
                if (candidate.status == NEKO_OK && feasibleAt(candidate.x, constraints, nconstraints)) {
                    if (!haveBest
                            || (goal == NEKO_OPT_MINIMIZE && candidate.value < best.value)
                            || (goal == NEKO_OPT_MAXIMIZE && candidate.value > best.value)) {
                        best = candidate;
                        haveBest = true;
                    }
                }
            }
            inSeg = false;
        }

        // Advance the scan state to the current sample
        prevX = x;
        prevFeasible = feasible;
        (void)prevFeasible;
    }

    // Mark success if at least one feasible optimized candidate was found
    if (haveBest) best.status = NEKO_OK;
    return best;
}

/* ---------- ODE solvers ---------- */

// Clone a numeric function wrapper
static NekoFunc* cloneFunc(const NekoFunc* func) {
    // Reject missing wrappers
    if (!func) return NULL;

    // Clone expression-backed functions by cloning their expression
    if (func->expr) return nekoFuncFromExpr(func->expr);

    // Clone callback-backed functions by reusing the callback and user data
    if (func->callback) return nekoFuncFromCallback(func->callback, func->userdata);

    // Empty wrappers cannot be cloned meaningfully
    return NULL;
}

// Wrap a successful ODE solve expression
static NekoSolveResult solveOk(NekoExpr* expr) {
    // Encode allocation failure as an invalid-argument status
    NekoSolveResult r = { .status = expr ? NEKO_OK : NEKO_ERR_INVALID_ARG, .expr = expr };
    return r;
}

// Wrap a failed ODE solve result
static NekoSolveResult solveErr(NekoStatus status) {
    // Return the requested error with no expression payload
    NekoSolveResult r = { .status = status, .expr = NULL };
    return r;
}

// Compute a factorial as a long double for symbolic coefficients
static long double factorialDouble(int n) {
    // Accumulate the factorial iteratively in long double precision
    long double out = 1.0;
    for (int i = 2; i <= n; i++) out *= (long double)i;
    return out;
}

// Scale an owned expression by a numeric coefficient
static NekoExpr* scaleExpr(long double coeff, NekoExpr* expr) {
    // Reject missing expression ownership
    if (!expr) return NULL;

    // Zero coefficients collapse the expression and free the old tree
    if (fabsl(coeff) <= 1e-12) {
        nekoFreeExpr(expr);
        return nekoConst(0.0);
    }

    // Identity coefficients preserve the expression shape
    if (fabsl(coeff - 1.0) <= 1e-12) return expr;

    // Negative identity coefficients become unary negation
    if (fabsl(coeff + 1.0) <= 1e-12) return nekoNeg(expr);

    // General coefficients become explicit multiplication
    return nekoMul(nekoConst(coeff), expr);
}

// Build the shifted expression x-x0
static NekoExpr* xShiftExpr(long double x0) {
    // Avoid printing a useless zero shift
    if (fabsl(x0) <= 1e-12) return nekoVar("x");

    // Build the translated variable expression
    return nekoSub(nekoVar("x"), nekoConst(x0));
}

// Build an arbitrary-constant symbol expression
static NekoExpr* constantSymbolExpr(int index) {
    // Format constants using the C1, C2, ... convention
    char name[16];
    snprintf(name, sizeof(name), "C%d", index);

    // Return the constant as a symbolic variable
    return nekoVar(name);
}

// Build a general homogeneous basis term
static NekoExpr* generalBasisTerm(int degree, int constantIndex) {
    // Degree zero is just the arbitrary constant
    if (degree == 0) return constantSymbolExpr(constantIndex);

    // Build the power of x used by the homogeneous basis term
    NekoExpr* power = degree == 1
        ? nekoVar("x")
        : nekoPow(nekoVar("x"), nekoConst((long double)degree));

    // Attach the constant and factorial scaling
    return scaleExpr(1.0 / factorialDouble(degree),
                     nekoMul(constantSymbolExpr(constantIndex), power));
}

// Build an initial-value basis term around x0
static NekoExpr* shiftedBasisTerm(int degree, long double x0, long double coeff) {
    // Zero coefficients contribute no initial-value term
    if (fabsl(coeff) <= 1e-12) return nekoConst(0.0);

    // Degree zero initial data is a constant term
    if (degree == 0) return nekoConst(coeff);

    // Build the shifted power (x-x0)^degree
    NekoExpr* power = xShiftExpr(x0);
    if (degree > 1) power = nekoPow(power, nekoConst((long double)degree));

    // Scale by coeff/degree! for repeated integration
    return scaleExpr(coeff / factorialDouble(degree), power);
}

// Split a scalar factor from an expression and clone the remaining core
static bool splitScalarFactorExpr(const NekoExpr* expr, long double* coeff, NekoExpr** core) {
    // Require an expression and both output channels
    if (!expr || !coeff || !core) return false;

    // Split based on the expression shape
    switch (expr->kind) {
        case NEKO_EXPR_CONST:
            // Constants become the scalar with a unit symbolic core
            *coeff = expr->as.constant;
            *core = nekoConst(1.0);
            return *core != NULL;
        case NEKO_EXPR_NEG: {
            // Negation flips the scalar coefficient of the inner expression
            long double innerCoeff = 0.0;
            NekoExpr* innerCore = NULL;
            if (!splitScalarFactorExpr(expr->as.unary.arg, &innerCoeff, &innerCore)) return false;
            *coeff = -innerCoeff;
            *core = innerCore;
            return true;
        }
        case NEKO_EXPR_MUL: {
            // Split both factors and multiply their scalar coefficients
            long double leftCoeff = 0.0, rightCoeff = 0.0;
            NekoExpr* leftCore = NULL;
            NekoExpr* rightCore = NULL;
            if (!splitScalarFactorExpr(expr->as.binary.lhs, &leftCoeff, &leftCore)
                    || !splitScalarFactorExpr(expr->as.binary.rhs, &rightCoeff, &rightCore)) {
                nekoFreeExpr(leftCore);
                nekoFreeExpr(rightCore);
                return false;
            }
            *coeff = leftCoeff * rightCoeff;

            // Recombine the symbolic cores after removing scalar factors
            *core = nekoSimplify(nekoMul(leftCore, rightCore));
            return *core != NULL;
        }
        case NEKO_EXPR_DIV:
            // Pull scalar denominators into the coefficient
            if (expr->as.binary.rhs->kind == NEKO_EXPR_CONST) {
                long double numCoeff = 0.0;
                NekoExpr* numCore = NULL;
                if (!splitScalarFactorExpr(expr->as.binary.lhs, &numCoeff, &numCore)) return false;
                *coeff = numCoeff / expr->as.binary.rhs->as.constant;
                *core = numCore;
                return true;
            }
            break;
        default:
            break;
    }

    // Expressions without explicit scalar factors keep coefficient one
    *coeff = 1.0;
    *core = nekoCloneExpr(expr);
    return *core != NULL;
}

// Integrate an expression repeatedly
static NekoExpr* integrateRepeatedlyExpr(const NekoExpr* expr, int times, const char* var) {
    // Start from a clone so the input expression remains borrowed
    NekoExpr* current = nekoCloneExpr(expr);
    if (!current) return NULL;

    // Apply symbolic integration the requested number of times
    for (int i = 0; i < times; i++) {
        // Split scalar factors before integrating the symbolic core
        long double coeff = 1.0;
        NekoExpr* core = NULL;
        if (!splitScalarFactorExpr(current, &coeff, &core)) {
            nekoFreeExpr(current);
            return NULL;
        }

        // Integrate the core and release the previous iteration state
        NekoIntegralResult r = nekoIntegrateExpr(core, var);
        nekoFreeExpr(core);
        nekoFreeExpr(current);

        // Abort if the next symbolic integration step is unsupported
        if (r.status != NEKO_OK) {
            nekoFreeExpr(r.expr);
            return NULL;
        }

        // Restore the scalar factor around the integrated expression
        current = nekoSimplify(scaleExpr(coeff, r.expr));
    }

    // Return the fully integrated expression in simplified form
    return nekoSimplify(current);
}

// Differentiate an expression repeatedly
static NekoExpr* differentiateRepeatedlyExpr(const NekoExpr* expr, int times, const char* var) {
    // Start from a clone so the input expression remains borrowed
    NekoExpr* current = nekoCloneExpr(expr);
    if (!current) return NULL;

    // Apply symbolic differentiation the requested number of times
    for (int i = 0; i < times; i++) {
        NekoDiffResult r = nekoDifferentiateExpr(current, var);
        nekoFreeExpr(current);

        // Abort if the next symbolic derivative is unsupported
        if (r.status != NEKO_OK) {
            nekoFreeExpr(r.expr);
            return NULL;
        }

        // Carry the derivative into the next iteration
        current = r.expr;
    }

    // Return the fully differentiated expression in simplified form
    return nekoSimplify(current);
}

// Build a second-order particular solution ansatz
static NekoExpr* buildSecondOrderParticular(long double p, long double q, long double r, NekoExpr* arg) {
    // A zero forcing term has zero particular solution
    if (fabsl(r) <= 1e-12) {
        nekoFreeExpr(arg);
        return nekoConst(0.0);
    }

    // Nonzero y coefficient admits a constant particular solution
    if (fabsl(q) > 1e-12) {
        nekoFreeExpr(arg);
        return nekoConst(r / q);
    }

    // Nonzero y' coefficient admits a linear particular solution
    if (fabsl(p) > 1e-12) return scaleExpr(r / p, arg);

    // Pure y''=r integrates to a quadratic particular solution
    return scaleExpr(0.5 * r, nekoPow(arg, nekoConst(2.0)));
}

// Evaluate a particular solution at zero
static long double secondOrderParticularAtZero(long double p, long double q, long double r) {
    // p is irrelevant for the zero-value cases represented here
    (void)p;

    // Zero forcing gives zero particular contribution
    if (fabsl(r) <= 1e-12) return 0.0;

    // Constant particular solutions contribute r/q at zero
    if (fabsl(q) > 1e-12) return r / q;

    // Linear and quadratic particular solutions vanish at zero
    return 0.0;
}

// Evaluate the derivative of a particular solution at zero
static long double secondOrderParticularDerivAtZero(long double p, long double q, long double r) {
    // Zero forcing and constant particular solutions have zero derivative
    if (fabsl(r) <= 1e-12 || fabsl(q) > 1e-12) return 0.0;

    // Linear particular solutions contribute their slope
    if (fabsl(p) > 1e-12) return r / p;

    // Quadratic particular solutions have zero derivative at zero
    return 0.0;
}

// Solve a supported first-order ODE in general form
static NekoExpr* solveFirstOrderGeneralExpr(const NekoOde* ode) {
    // Normalize y'=ky from the stored a*y'=b*y coefficients
    long double k = ode->as.firstOrder.b / ode->as.firstOrder.a;

    // Return C1*exp(kx)
    return nekoSimplify(nekoMul(constantSymbolExpr(1),
                                nekoExp(scaleExpr(k, nekoVar("x")))));
}

// Solve a supported first-order initial-value ODE
static NekoExpr* solveFirstOrderInitialExpr(const NekoOde* ode) {
    // Normalize y'=ky from the stored a*y'=b*y coefficients
    long double k = ode->as.firstOrder.b / ode->as.firstOrder.a;

    // Return y0*exp(k*(x-x0))
    return nekoSimplify(scaleExpr(ode->as.firstOrder.y0,
                                  nekoExp(scaleExpr(k, xShiftExpr(ode->as.firstOrder.x0)))));
}

// Solve a supported second-order ODE in general form
static NekoExpr* solveSecondOrderGeneralExpr(const NekoOde* ode) {
    // Read and normalize the second-order coefficients
    long double a = ode->as.secondOrder.a;
    long double b = ode->as.secondOrder.b;
    long double c = ode->as.secondOrder.c;
    long double d = ode->as.secondOrder.d;
    long double p = b / a;
    long double q = c / a;
    long double r = d / a;
    long double disc = b * b - 4.0 * a * c;
    NekoExpr* hom = NULL;

    // Distinct real characteristic roots give two exponentials
    if (disc > 1e-12) {
        long double s = sqrtl(disc);
        long double r1 = (-b + s) / (2.0 * a);
        long double r2 = (-b - s) / (2.0 * a);
        NekoExpr* x = nekoVar("x");
        NekoExpr* t1 = nekoMul(constantSymbolExpr(1), nekoExp(scaleExpr(r1, nekoCloneExpr(x))));
        NekoExpr* t2 = nekoMul(constantSymbolExpr(2), nekoExp(scaleExpr(r2, x)));
        hom = nekoAdd(t1, t2);
    } else if (disc < -1e-12) {
        // Complex characteristic roots give damped sine and cosine terms
        long double alpha = -b / (2.0 * a);
        long double beta = sqrtl(-disc) / (2.0 * fabsl(a));
        NekoExpr* x = nekoVar("x");
        NekoExpr* cosTerm = nekoMul(constantSymbolExpr(1),
                                    nekoCos(scaleExpr(beta, nekoCloneExpr(x))));
        NekoExpr* sinTerm = nekoMul(constantSymbolExpr(2),
                                    nekoSin(scaleExpr(beta, x)));
        NekoExpr* inner = nekoAdd(cosTerm, sinTerm);
        hom = nekoMul(nekoExp(scaleExpr(alpha, nekoVar("x"))), inner);
    } else {
        // A repeated real root gives exp(root*x)*(C1+C2*x)
        long double root = -b / (2.0 * a);
        NekoExpr* x = nekoVar("x");
        NekoExpr* inner = nekoAdd(constantSymbolExpr(1),
                                  nekoMul(constantSymbolExpr(2), nekoCloneExpr(x)));
        hom = nekoMul(inner, nekoExp(scaleExpr(root, x)));
    }

    // Add the supported constant-forcing particular solution
    return nekoSimplify(nekoAdd(hom, buildSecondOrderParticular(p, q, r, nekoVar("x"))));
}

// Solve a supported second-order initial-value ODE
static NekoExpr* solveSecondOrderInitialExpr(const NekoOde* ode) {
    // Read and normalize the second-order initial-value data
    long double a = ode->as.secondOrder.a;
    long double b = ode->as.secondOrder.b;
    long double c = ode->as.secondOrder.c;
    long double d = ode->as.secondOrder.d;
    long double y0 = ode->as.secondOrder.y0;
    long double dy0 = ode->as.secondOrder.dy0;
    long double x0 = ode->as.secondOrder.x0;
    long double p = b / a;
    long double q = c / a;
    long double r = d / a;
    long double disc = b * b - 4.0 * a * c;
    long double yShift = y0 - secondOrderParticularAtZero(p, q, r);
    long double dyShift = dy0 - secondOrderParticularDerivAtZero(p, q, r);
    NekoExpr* hom = NULL;

    // Distinct real characteristic roots solve for two exponential constants
    if (disc > 1e-12) {
        long double s = sqrtl(disc);
        long double r1 = (-b + s) / (2.0 * a);
        long double r2 = (-b - s) / (2.0 * a);
        long double A = (dyShift - r2 * yShift) / (r1 - r2);
        long double B = yShift - A;
        NekoExpr* t = xShiftExpr(x0);
        NekoExpr* left = scaleExpr(A, nekoExp(scaleExpr(r1, nekoCloneExpr(t))));
        NekoExpr* right = scaleExpr(B, nekoExp(scaleExpr(r2, t)));
        hom = nekoAdd(left, right);
    } else if (disc < -1e-12) {
        // Complex roots solve for sine and cosine coefficients at x0
        long double alpha = -b / (2.0 * a);
        long double beta = sqrtl(-disc) / (2.0 * fabsl(a));
        long double A = yShift;
        long double B = (dyShift - alpha * yShift) / beta;
        NekoExpr* t = xShiftExpr(x0);
        NekoExpr* left = scaleExpr(A, nekoCos(scaleExpr(beta, nekoCloneExpr(t))));
        NekoExpr* right = scaleExpr(B, nekoSin(scaleExpr(beta, t)));
        NekoExpr* inner = nekoAdd(left, right);
        hom = nekoMul(nekoExp(scaleExpr(alpha, xShiftExpr(x0))), inner);
    } else {
        // Repeated roots solve for coefficients of (A+B(x-x0))*exp(root*(x-x0))
        long double root = -b / (2.0 * a);
        long double A = yShift;
        long double B = dyShift - root * yShift;
        NekoExpr* t = xShiftExpr(x0);
        NekoExpr* inner = nekoAdd(nekoConst(A), scaleExpr(B, nekoCloneExpr(t)));
        hom = nekoMul(inner, nekoExp(scaleExpr(root, t)));
    }

    // Add the particular solution shifted to the initial point
    return nekoSimplify(nekoAdd(hom, buildSecondOrderParticular(p, q, r, xShiftExpr(x0))));
}

// Solve a supported nth-order integrable ODE in general form
static NekoExpr* solveNthOrderGeneralExpr(const NekoOde* ode) {
    // Scale the right-hand side by the leading coefficient
    NekoExpr* scaledRhs = scaleExpr(1.0 / ode->as.nthOrder.a, nekoCloneExpr(ode->as.nthOrder.rhs));

    // Integrate the scaled right-hand side order many times
    NekoExpr* solution = integrateRepeatedlyExpr(scaledRhs, ode->as.nthOrder.order, "x");
    nekoFreeExpr(scaledRhs);
    if (!solution) return NULL;

    // Add the full homogeneous polynomial basis with arbitrary constants
    for (int i = 0; i < ode->as.nthOrder.order; i++) {
        solution = nekoSimplify(nekoAdd(solution, generalBasisTerm(i, i + 1)));
    }

    // Return the general symbolic solution
    return solution;
}

// Solve a supported nth-order integrable initial-value ODE
static NekoExpr* solveNthOrderInitialExpr(const NekoOde* ode) {
    // Initial-value solving requires stored initial values
    if (!ode->as.nthOrder.initialValues) return NULL;

    // Scale the right-hand side by the leading coefficient
    NekoExpr* scaledRhs = scaleExpr(1.0 / ode->as.nthOrder.a, nekoCloneExpr(ode->as.nthOrder.rhs));

    // Integrate the forced term order many times
    NekoExpr* solution = integrateRepeatedlyExpr(scaledRhs, ode->as.nthOrder.order, "x");
    nekoFreeExpr(scaledRhs);
    if (!solution) return NULL;

    // Add correction basis terms so each derivative matches initial data
    for (int i = 0; i < ode->as.nthOrder.order; i++) {
        // Evaluate the current ith derivative at the initial point
        NekoExpr* deriv = differentiateRepeatedlyExpr(solution, i, "x");
        long double solvedValue = deriv ? nekoEvalExpr(deriv, "x", ode->as.nthOrder.x0) : NAN;
        long double correction = ode->as.nthOrder.initialValues[i] - solvedValue;
        nekoFreeExpr(deriv);

        // Abort if the correction cannot be represented numerically
        if (!isfinite(correction)) {
            nekoFreeExpr(solution);
            return NULL;
        }

        // Add the shifted basis term that fixes this derivative
        solution = nekoSimplify(nekoAdd(solution,
                                        shiftedBasisTerm(i, ode->as.nthOrder.x0, correction)));
    }

    // Return the initial-value symbolic solution
    return solution;
}

// Construct a Bernoulli ODE descriptor
NekoOde* nekoOdeBernoulli(const NekoFunc* P, const NekoFunc* Q, long double n, long double x0, long double y0) {
    // Validate coefficient functions and initial data
    if (!P || !Q || !isfinite(n) || !isfinite(x0) || !isfinite(y0)) return NULL;

    // Allocate the descriptor and clone its coefficient functions
    NekoOde* ode = calloc(1, sizeof(NekoOde));
    if (!ode) return NULL;
    ode->kind = NEKO_ODE_BERNOULLI;
    ode->as.bernoulli.P = cloneFunc(P);
    ode->as.bernoulli.Q = cloneFunc(Q);
    ode->as.bernoulli.n = n;
    ode->as.bernoulli.x0 = x0;
    ode->as.bernoulli.y0 = y0;

    // Release the descriptor if either coefficient clone failed
    if (!ode->as.bernoulli.P || !ode->as.bernoulli.Q) {
        nekoFreeOde(ode);
        return NULL;
    }
    return ode;
}

// Construct a first-order linear constant-coefficient ODE descriptor
NekoOde* nekoOdeFirstOrderLinearConst(long double a, long double b, long double x0, long double y0) {
    // Validate finite data and a nonzero leading coefficient
    if (!isfinite(a) || !isfinite(b) || !isfinite(x0) || !isfinite(y0) || fabsl(a) <= 1e-12) {
        return NULL;
    }

    // Allocate and fill the first-order descriptor
    NekoOde* ode = calloc(1, sizeof(NekoOde));
    if (!ode) return NULL;
    ode->kind = NEKO_ODE_FIRST_ORDER_LINEAR_CONST;
    ode->as.firstOrder.a = a;
    ode->as.firstOrder.b = b;
    ode->as.firstOrder.x0 = x0;
    ode->as.firstOrder.y0 = y0;
    return ode;
}

// Construct a homogeneous second-order constant-coefficient ODE descriptor
NekoOde* nekoOdeSecondOrderConst(long double a, long double b, long double c, long double x0, long double y0, long double dy0) {
    // Delegate to the forced constructor with zero forcing
    return nekoOdeSecondOrderConstForced(a, b, c, 0.0, x0, y0, dy0);
}

// Construct a forced second-order constant-coefficient ODE descriptor
NekoOde* nekoOdeSecondOrderConstForced(long double a, long double b, long double c, long double d, long double x0, long double y0, long double dy0) {
    // Validate finite data and a nonzero leading coefficient
    if (!isfinite(a) || !isfinite(b) || !isfinite(c) || !isfinite(x0)
            || !isfinite(d) || !isfinite(y0) || !isfinite(dy0) || fabsl(a) <= 1e-12) {
        return NULL;
    }

    // Allocate and fill the second-order descriptor
    NekoOde* ode = calloc(1, sizeof(NekoOde));
    if (!ode) return NULL;
    ode->kind = NEKO_ODE_SECOND_ORDER_LINEAR_CONST;
    ode->as.secondOrder.a = a;
    ode->as.secondOrder.b = b;
    ode->as.secondOrder.c = c;
    ode->as.secondOrder.d = d;
    ode->as.secondOrder.x0 = x0;
    ode->as.secondOrder.y0 = y0;
    ode->as.secondOrder.dy0 = dy0;
    return ode;
}

// Construct an nth-order integrable ODE descriptor
NekoOde* nekoOdeNthOrderIntegrable(int order, long double a, const NekoExpr* rhs, long double x0, const long double* initialValues) {
    // Validate the order, leading coefficient, right-hand side, and base point
    if (order < 1 || !rhs || !isfinite(a) || fabsl(a) <= 1e-12 || !isfinite(x0)) return NULL;

    // Allocate and initialize the descriptor shell
    NekoOde* ode = calloc(1, sizeof(NekoOde));
    if (!ode) return NULL;
    ode->kind = NEKO_ODE_NTH_ORDER_INTEGRABLE;
    ode->as.nthOrder.order = order;
    ode->as.nthOrder.a = a;
    ode->as.nthOrder.x0 = x0;
    ode->as.nthOrder.rhs = nekoCloneExpr(rhs);

    // Release the descriptor if cloning the right-hand side failed
    if (!ode->as.nthOrder.rhs) {
        nekoFreeOde(ode);
        return NULL;
    }

    // Copy optional initial values for initial-value solving
    if (initialValues) {
        ode->as.nthOrder.initialValues = malloc((size_t)order * sizeof(long double));
        if (!ode->as.nthOrder.initialValues) {
            nekoFreeOde(ode);
            return NULL;
        }

        // Validate each initial value while copying it into owned storage
        for (int i = 0; i < order; i++) {
            if (!isfinite(initialValues[i])) {
                nekoFreeOde(ode);
                return NULL;
            }
            ode->as.nthOrder.initialValues[i] = initialValues[i];
        }
    }
    return ode;
}

// Construct a constant linear ODE-system descriptor
NekoOde* nekoOdeLinearSystemConst(const long double* A, const long double* y0, int dim, long double x0) {
    // Validate input pointers, dimension, and base point
    if (!A || !y0 || dim < 1 || !isfinite(x0)) return NULL;

    // Compute the matrix size safely before allocating
    size_t dimSq;
    if (!checkedSizeMul((size_t)dim, (size_t)dim, &dimSq)
            || dimSq > (size_t)INT_MAX) {
        return NULL;
    }

    // Allocate the descriptor and its owned arrays
    NekoOde* ode = calloc(1, sizeof(NekoOde));
    if (!ode) return NULL;
    ode->kind = NEKO_ODE_LINEAR_SYSTEM_CONST;
    ode->as.linearSystem.dim = dim;
    ode->as.linearSystem.x0 = x0;
    ode->as.linearSystem.A = malloc(dimSq * sizeof(long double));
    ode->as.linearSystem.y0 = malloc((size_t)dim * sizeof(long double));

    // Release the descriptor if either array allocation failed
    if (!ode->as.linearSystem.A || !ode->as.linearSystem.y0) {
        nekoFreeOde(ode);
        return NULL;
    }

    // Copy and validate the matrix coefficients
    for (size_t i = 0; i < dimSq; i++) {
        if (!isfinite(A[i])) {
            nekoFreeOde(ode);
            return NULL;
        }
        ode->as.linearSystem.A[i] = A[i];
    }

    // Copy and validate the initial vector
    for (int i = 0; i < dim; i++) {
        if (!isfinite(y0[i])) {
            nekoFreeOde(ode);
            return NULL;
        }
        ode->as.linearSystem.y0[i] = y0[i];
    }
    return ode;
}

// Free an ODE descriptor and all owned data
void nekoFreeOde(NekoOde* ode) {
    // Treat NULL as an already-freed descriptor
    if (!ode) return;

    // Release kind-specific owned payloads
    switch (ode->kind) {
        case NEKO_ODE_BERNOULLI:
            nekoFreeFunc(ode->as.bernoulli.P);
            nekoFreeFunc(ode->as.bernoulli.Q);
            break;
        case NEKO_ODE_FIRST_ORDER_LINEAR_CONST:
            break;
        case NEKO_ODE_NTH_ORDER_INTEGRABLE:
            nekoFreeExpr(ode->as.nthOrder.rhs);
            free(ode->as.nthOrder.initialValues);
            break;
        case NEKO_ODE_LINEAR_SYSTEM_CONST:
            free(ode->as.linearSystem.A);
            free(ode->as.linearSystem.y0);
            break;
        case NEKO_ODE_SECOND_ORDER_LINEAR_CONST:
            break;
    }

    // Release the descriptor shell
    free(ode);
}

// Test whether an ODE descriptor has the requested kind
bool nekoMatchOdePattern(const NekoOde* ode, NekoOdeKind kind) {
    // A descriptor matches only when it exists and has the requested kind
    return ode && ode->kind == kind;
}

// Solve an ODE descriptor in general form
NekoSolveResult nekoSolveOdeGeneral(const NekoOde* ode) {
    // Require a descriptor to choose a solve strategy
    if (!ode) return solveErr(NEKO_ERR_INVALID_ARG);

    // Dispatch only the ODE kinds with implemented general symbolic solvers
    switch (ode->kind) {
        case NEKO_ODE_FIRST_ORDER_LINEAR_CONST:
            return solveOk(solveFirstOrderGeneralExpr(ode));
        case NEKO_ODE_SECOND_ORDER_LINEAR_CONST:
            return solveOk(solveSecondOrderGeneralExpr(ode));
        case NEKO_ODE_NTH_ORDER_INTEGRABLE:
            return solveOk(solveNthOrderGeneralExpr(ode));
        case NEKO_ODE_BERNOULLI:
        case NEKO_ODE_LINEAR_SYSTEM_CONST:
            return solveErr(NEKO_ERR_UNSUPPORTED);
    }

    // Unknown ODE kinds are treated as unsupported
    return solveErr(NEKO_ERR_UNSUPPORTED);
}

// Solve an ODE descriptor with initial values
NekoSolveResult nekoSolveOdeInitialValue(const NekoOde* ode) {
    // Require a descriptor to choose a solve strategy
    if (!ode) return solveErr(NEKO_ERR_INVALID_ARG);

    // Dispatch only the ODE kinds with implemented initial-value symbolic solvers
    switch (ode->kind) {
        case NEKO_ODE_FIRST_ORDER_LINEAR_CONST:
            return solveOk(solveFirstOrderInitialExpr(ode));
        case NEKO_ODE_SECOND_ORDER_LINEAR_CONST:
            return solveOk(solveSecondOrderInitialExpr(ode));
        case NEKO_ODE_NTH_ORDER_INTEGRABLE:
            return solveOk(solveNthOrderInitialExpr(ode));
        case NEKO_ODE_BERNOULLI:
        case NEKO_ODE_LINEAR_SYSTEM_CONST:
            return solveErr(NEKO_ERR_UNSUPPORTED);
    }

    // Unknown ODE kinds are treated as unsupported
    return solveErr(NEKO_ERR_UNSUPPORTED);
}

// Build a scalar ODE evaluation error
static NekoOdeResult odeScalarError(NekoStatus status, long double x) {
    // Package the status and x value with no finite scalar result
    NekoOdeResult r = { .status = status, .x = x, .value = NAN, .iterations = 0 };
    return r;
}

// Evaluate the right-hand side of a Bernoulli ODE
static long double bernoulliRhs(const NekoOde* ode, long double x, long double y) {
    // Evaluate the coefficient functions at the current x
    long double p = nekoEvalFunc(ode->as.bernoulli.P, x);
    long double q = nekoEvalFunc(ode->as.bernoulli.Q, x);
    long double n = ode->as.bernoulli.n;

    // Reject non-finite coefficients and non-real fractional powers
    if (!isfinite(p) || !isfinite(q)) return NAN;
    if (y < 0.0 && fabsl(n - floorl(n)) > 1e-12) return NAN;

    // Return Q(x)y^n-P(x)y for the Bernoulli form used here
    return q * powl(y, n) - p * y;
}

// Numerically evaluate a Bernoulli ODE by time-stepping
static NekoOdeResult evalBernoulli(const NekoOde* ode, long double x, int steps) {
    // Initialize the result at the stored initial condition
    NekoOdeResult r = { .status = NEKO_OK, .x = x, .value = ode->as.bernoulli.y0, .iterations = 0 };

    // Choose a default step count when needed
    if (steps < 1) steps = 1024;

    // Start integration from the descriptor base point
    long double t = ode->as.bernoulli.x0;
    long double y = ode->as.bernoulli.y0;
    if (x == t) return r;

    // Use fixed-step RK4 over the interval from x0 to x
    long double h = (x - t) / (long double)steps;
    for (int i = 0; i < steps; i++) {
        // Compute the four Runge-Kutta stages
        long double k1 = bernoulliRhs(ode, t, y);
        long double k2 = bernoulliRhs(ode, t + 0.5 * h, y + 0.5 * h * k1);
        long double k3 = bernoulliRhs(ode, t + 0.5 * h, y + 0.5 * h * k2);
        long double k4 = bernoulliRhs(ode, t + h, y + h * k3);

        // Abort if the ODE leaves the real numeric domain
        if (!isfinite(k1) || !isfinite(k2) || !isfinite(k3) || !isfinite(k4)) {
            r.status = NEKO_ERR_DOMAIN;
            r.value = NAN;
            r.iterations = i;
            return r;
        }

        // Advance the state with the RK4 weighted average
        y += h * (k1 + 2.0 * k2 + 2.0 * k3 + k4) / 6.0;
        t += h;
        r.iterations = i + 1;
    }

    // Store the final scalar solution value
    r.value = y;
    return r;
}

// Evaluate a closed-form ODE solution at x
static NekoOdeResult evalClosedFormOde(const NekoOde* ode, long double x) {
    // Build the symbolic initial-value solution first
    NekoSolveResult solved = nekoSolveOdeInitialValue(ode);

    // Convert solve failure into a scalar evaluation error
    if (solved.status != NEKO_OK || !solved.expr) {
        nekoFreeExpr(solved.expr);
        return odeScalarError(solved.status, x);
    }

    // Evaluate the solved expression at the requested x
    NekoOdeResult r = { .status = NEKO_OK, .x = x, .value = nekoEvalExpr(solved.expr, "x", x), .iterations = 0 };
    nekoFreeExpr(solved.expr);

    // Mark non-finite evaluations as domain errors
    if (!isfinite(r.value)) r.status = NEKO_ERR_DOMAIN;
    return r;
}

// Multiply a dense square matrix by a vector
static void matVecMul(const long double* A, const long double* y, long double* out, int dim) {
    // Compute each output row as a dot product
    for (int i = 0; i < dim; i++) {
        long double sum = 0.0;

        // Accumulate the row-vector product
        for (int j = 0; j < dim; j++) sum += A[i * dim + j] * y[j];
        out[i] = sum;
    }
}

// Evaluate a scalar ODE descriptor at x
NekoOdeResult nekoEvalOde(const NekoOde* ode, long double x, int steps) {
    // Validate descriptor and target point
    if (!ode || !isfinite(x)) return odeScalarError(NEKO_ERR_INVALID_ARG, x);

    // Dispatch scalar ODE evaluation by descriptor kind
    switch (ode->kind) {
        case NEKO_ODE_BERNOULLI:
            return evalBernoulli(ode, x, steps);
        case NEKO_ODE_FIRST_ORDER_LINEAR_CONST:
        case NEKO_ODE_SECOND_ORDER_LINEAR_CONST:
        case NEKO_ODE_NTH_ORDER_INTEGRABLE:
            return evalClosedFormOde(ode, x);
        case NEKO_ODE_LINEAR_SYSTEM_CONST:
            return odeScalarError(NEKO_ERR_INVALID_ARG, x);
    }

    // Unknown ODE kinds are treated as unsupported
    return odeScalarError(NEKO_ERR_UNSUPPORTED, x);
}

// Evaluate an ODE-system descriptor at x
NekoOdeSystemResult nekoEvalOdeSystem(const NekoOde* ode, long double x, int steps) {
    // Initialize an empty system result
    NekoOdeSystemResult r = { .status = NEKO_OK, .x = x, .values = NULL, .dim = 0, .iterations = 0 };

    // Require a linear-system descriptor and a finite target point
    if (!ode || ode->kind != NEKO_ODE_LINEAR_SYSTEM_CONST || !isfinite(x)) {
        r.status = NEKO_ERR_INVALID_ARG;
        return r;
    }

    // Choose a default step count when needed
    if (steps < 1) steps = 1024;

    // Validate the system dimension and matrix storage size
    int dim = ode->as.linearSystem.dim;
    size_t dimSq;
    if (!checkedSizeMul((size_t)dim, (size_t)dim, &dimSq)
            || dimSq > (size_t)INT_MAX) {
        r.status = NEKO_ERR_INVALID_ARG;
        return r;
    }

    // Allocate the solution vector and RK4 work buffers
    r.dim = dim;
    r.values = malloc((size_t)dim * sizeof(long double));
    long double* k1 = malloc((size_t)dim * sizeof(long double));
    long double* k2 = malloc((size_t)dim * sizeof(long double));
    long double* k3 = malloc((size_t)dim * sizeof(long double));
    long double* k4 = malloc((size_t)dim * sizeof(long double));
    long double* tmp = malloc((size_t)dim * sizeof(long double));

    // Release partial allocations if any work buffer fails
    if (!r.values || !k1 || !k2 || !k3 || !k4 || !tmp) {
        free(r.values); free(k1); free(k2); free(k3); free(k4); free(tmp);
        r.values = NULL;
        r.dim = 0;
        r.status = NEKO_ERR_INVALID_ARG;
        return r;
    }

    // Initialize the solution vector from y0 and compute the step size
    for (int i = 0; i < dim; i++) r.values[i] = ode->as.linearSystem.y0[i];
    long double h = (x - ode->as.linearSystem.x0) / (long double)steps;

    // Advance the linear system using fixed-step RK4
    for (int s = 0; s < steps; s++) {
        // Compute the four RK4 slopes for y'=Ay
        matVecMul(ode->as.linearSystem.A, r.values, k1, dim);
        for (int i = 0; i < dim; i++) tmp[i] = r.values[i] + 0.5 * h * k1[i];
        matVecMul(ode->as.linearSystem.A, tmp, k2, dim);
        for (int i = 0; i < dim; i++) tmp[i] = r.values[i] + 0.5 * h * k2[i];
        matVecMul(ode->as.linearSystem.A, tmp, k3, dim);
        for (int i = 0; i < dim; i++) tmp[i] = r.values[i] + h * k3[i];
        matVecMul(ode->as.linearSystem.A, tmp, k4, dim);

        // Update every component with the RK4 weighted average
        for (int i = 0; i < dim; i++) {
            r.values[i] += h * (k1[i] + 2.0 * k2[i] + 2.0 * k3[i] + k4[i]) / 6.0;

            // Stop and return partial values if any component leaves the domain
            if (!isfinite(r.values[i])) {
                r.status = NEKO_ERR_DOMAIN;
                r.iterations = s;
                free(k1); free(k2); free(k3); free(k4); free(tmp);
                return r;
            }
        }

        // Record how many full RK4 steps completed
        r.iterations = s + 1;
    }

    // Release work buffers before returning the owned result vector
    free(k1); free(k2); free(k3); free(k4); free(tmp);
    return r;
}

// Free memory held by an ODE-system evaluation result
void nekoFreeOdeSystemResult(NekoOdeSystemResult result) {
    // Release the values array owned by the result object
    free(result.values);
}

/* ---------- Print ---------- */

// Return the printable name for a unary expression kind
static const char* unaryName(NekoExprKind kind) {
    // Map known unary expression kinds to their printed names
    switch (kind) {
        case NEKO_EXPR_NEG: return "-";
        case NEKO_EXPR_SIN: return "sin";
        case NEKO_EXPR_COS: return "cos";
        case NEKO_EXPR_TAN: return "tan";
        case NEKO_EXPR_ASIN: return "asin";
        case NEKO_EXPR_ACOS: return "acos";
        case NEKO_EXPR_ATAN: return "atan";
        case NEKO_EXPR_EXP: return "exp";
        case NEKO_EXPR_LOG: return "log";
        case NEKO_EXPR_SQRT: return "sqrt";
        case NEKO_EXPR_ABS: return "abs";
        case NEKO_EXPR_ERF: return "erf";
        case NEKO_EXPR_EI: return "Ei";
        case NEKO_EXPR_STEP: return "step";
        default: return "?";
    }
}

// Return the printer precedence for an expression
static int exprPrecedence(const NekoExpr* expr) {
    // Missing expressions bind tightly to avoid unnecessary parentheses
    if (!expr) return 100;

    // Return the precedence level used by the recursive printer
    switch (expr->kind) {
        case NEKO_EXPR_ADD:
        case NEKO_EXPR_SUB:
            return 10;
        case NEKO_EXPR_MUL:
        case NEKO_EXPR_DIV:
            return 20;
        case NEKO_EXPR_NEG:
            return 30;
        case NEKO_EXPR_POW:
            return 40;
        default:
            return 50;
    }
}

typedef struct {
    const NekoExpr* expr;
    int sign;
} PrintTerm;

typedef struct {
    PrintTerm* items;
    size_t len;
    size_t cap;
} PrintTermVec;

// Append a signed additive term for pretty-printing
static bool printTermVecPush(PrintTermVec* vec, const NekoExpr* expr, int sign) {
    // Reject invalid borrowed inputs
    if (!vec || !expr) return false;

    // Grow the borrowed term vector when it is full
    if (vec->len == vec->cap) {
        size_t nextCap = vec->cap ? vec->cap * 2 : 4;
        PrintTerm* next = realloc(vec->items, nextCap * sizeof(PrintTerm));
        if (!next) return false;
        vec->items = next;
        vec->cap = nextCap;
    }

    // Store the borrowed expression reference and sign
    vec->items[vec->len++] = (PrintTerm){ .expr = expr, .sign = sign };
    return true;
}

// Free a pretty-printer term vector
static void printTermVecFree(PrintTermVec* vec) {
    // Treat NULL as an already-empty vector
    if (!vec) return;

    // Release only the vector storage because expressions are borrowed
    free(vec->items);
    vec->items = NULL;
    vec->len = 0;
    vec->cap = 0;
}

// Collect additive terms for pretty-printing
static bool collectPrintTerms(const NekoExpr* expr, int sign, PrintTermVec* vec) {
    // Require a borrowed expression and destination vector
    if (!expr || !vec) return false;

    // Flatten addition, subtraction, and negation into signed terms
    switch (expr->kind) {
        case NEKO_EXPR_ADD:
            return collectPrintTerms(expr->as.binary.lhs, sign, vec)
                && collectPrintTerms(expr->as.binary.rhs, sign, vec);
        case NEKO_EXPR_SUB:
            return collectPrintTerms(expr->as.binary.lhs, sign, vec)
                && collectPrintTerms(expr->as.binary.rhs, -sign, vec);
        case NEKO_EXPR_NEG:
            return collectPrintTerms(expr->as.unary.arg, -sign, vec);
        default:
            // Store non-additive leaves as printable terms
            return printTermVecPush(vec, expr, sign);
    }
}

typedef struct {
    const NekoExpr** items;
    size_t len;
    size_t cap;
} ExprRefVec;

// Append an expression reference for pretty-printing
static bool exprRefVecPush(ExprRefVec* vec, const NekoExpr* expr) {
    // Reject invalid borrowed inputs
    if (!vec || !expr) return false;

    // Grow the reference vector when it is full
    if (vec->len == vec->cap) {
        size_t nextCap = vec->cap ? vec->cap * 2 : 4;
        const NekoExpr** next = realloc(vec->items, nextCap * sizeof(NekoExpr*));
        if (!next) return false;
        vec->items = next;
        vec->cap = nextCap;
    }

    // Store the borrowed expression reference
    vec->items[vec->len++] = expr;
    return true;
}

// Free a pretty-printer expression-reference vector
static void exprRefVecFree(ExprRefVec* vec) {
    // Treat NULL as an already-empty vector
    if (!vec) return;

    // Release only the vector storage because expressions are borrowed
    free(vec->items);
    vec->items = NULL;
    vec->len = 0;
    vec->cap = 0;
}

// Collect multiplicative factors for pretty-printing
static bool collectMulFactors(const NekoExpr* expr, ExprRefVec* vec) {
    // Require a borrowed expression and destination vector
    if (!expr || !vec) return false;

    // Flatten multiplication trees into a left-to-right factor list
    if (expr->kind == NEKO_EXPR_MUL) {
        return collectMulFactors(expr->as.binary.lhs, vec)
            && collectMulFactors(expr->as.binary.rhs, vec);
    }

    // Store non-multiplicative leaves as individual factors
    return exprRefVecPush(vec, expr);
}

// Print an expression with awareness of parent precedence
static void printExprPrec(const NekoExpr* expr, int parentPrec);

// Print an additive expression with clean signs
static void printAdditiveExpr(const NekoExpr* expr, int parentPrec) {
    // Collect additive leaves with explicit signs
    PrintTermVec terms = {0};
    if (!collectPrintTerms(expr, 1, &terms) || terms.len == 0) {
        printTermVecFree(&terms);
        printf("<null>");
        return;
    }

    // Parenthesize only when the parent binds more tightly than addition
    int needsParens = parentPrec > 10;
    if (needsParens) putchar('(');

    // Print the first term without a leading plus sign
    for (size_t i = 0; i < terms.len; i++) {
        if (i == 0) {
            if (terms.items[i].sign < 0) {
                putchar('-');
                printExprPrec(terms.items[i].expr, 30);
            } else {
                printExprPrec(terms.items[i].expr, 10);
            }
        } else {
            // Print later terms with explicit binary plus or minus separators
            printf(terms.items[i].sign < 0 ? " - " : " + ");
            printExprPrec(terms.items[i].expr, 10);
        }
    }

    // Close any parentheses and release borrowed-term storage
    if (needsParens) putchar(')');
    printTermVecFree(&terms);
}

// Print a multiplicative expression with clean factors
static void printMultiplicativeExpr(const NekoExpr* expr, int parentPrec) {
    // Collect multiplication leaves as borrowed factor references
    ExprRefVec factors = {0};
    if (!collectMulFactors(expr, &factors) || factors.len == 0) {
        exprRefVecFree(&factors);
        printf("<null>");
        return;
    }

    // Parenthesize only when the parent binds more tightly than multiplication
    int needsParens = parentPrec > 20;
    if (needsParens) putchar('(');

    // Print each factor with multiplication separators
    for (size_t i = 0; i < factors.len; i++) {
        if (i) printf(" * ");
        int childPrec = exprPrecedence(factors.items[i]);
        int childNeedsParens = childPrec < 20 || factors.items[i]->kind == NEKO_EXPR_NEG;

        // Protect additive and negative child factors with parentheses
        if (childNeedsParens) putchar('(');
        printExprPrec(factors.items[i], childNeedsParens ? 0 : 20);
        if (childNeedsParens) putchar(')');
    }

    // Close any parentheses and release borrowed-factor storage
    if (needsParens) putchar(')');
    exprRefVecFree(&factors);
}

// Print an expression, adding parentheses where precedence requires them
static void printExprPrec(const NekoExpr* expr, int parentPrec) {
    // Print a visible placeholder for missing expression nodes
    if (!expr) {
        printf("<null>");
        return;
    }

    // Dispatch by expression kind while respecting parent precedence
    switch (expr->kind) {
        case NEKO_EXPR_CONST:
            printf("%Lg", expr->as.constant);
            return;
        case NEKO_EXPR_VAR:
            printf("%s", expr->as.var ? expr->as.var : "?");
            return;
        case NEKO_EXPR_ADD:
        case NEKO_EXPR_SUB:
            printAdditiveExpr(expr, parentPrec);
            return;
        case NEKO_EXPR_MUL:
            printMultiplicativeExpr(expr, parentPrec);
            return;
        case NEKO_EXPR_DIV: {
            // Parenthesize division beneath tighter parent operators
            int needsParens = parentPrec > 20;
            if (needsParens) putchar('(');
            printExprPrec(expr->as.binary.lhs, 20);
            printf(" / ");
            printExprPrec(expr->as.binary.rhs, 21);
            if (needsParens) putchar(')');
            return;
        }
        case NEKO_EXPR_POW: {
            // Parenthesize powers beneath tighter parent operators
            int needsParens = parentPrec > 40;
            if (needsParens) putchar('(');
            printExprPrec(expr->as.binary.lhs, 40);
            printf(" ^ ");
            printExprPrec(expr->as.binary.rhs, 41);
            if (needsParens) putchar(')');
            return;
        }
        case NEKO_EXPR_NEG:
            // Print unary negation and protect it when required by the parent
            if (parentPrec > 30) putchar('(');
            putchar('-');
            printExprPrec(expr->as.unary.arg, 30);
            if (parentPrec > 30) putchar(')');
            return;
        case NEKO_EXPR_SIN:
        case NEKO_EXPR_COS:
        case NEKO_EXPR_TAN:
        case NEKO_EXPR_ASIN:
        case NEKO_EXPR_ACOS:
        case NEKO_EXPR_ATAN:
        case NEKO_EXPR_EXP:
        case NEKO_EXPR_LOG:
        case NEKO_EXPR_SQRT:
        case NEKO_EXPR_ABS:
        case NEKO_EXPR_ERF:
        case NEKO_EXPR_EI:
        case NEKO_EXPR_STEP:
            // Print unary functions using function-call notation
            printf("%s(", unaryName(expr->kind));
            printExprPrec(expr->as.unary.arg, 0);
            putchar(')');
            return;
        case NEKO_EXPR_CALL:
            // Print uninterpreted calls by name with comma-separated arguments
            printf("%s(", expr->as.call.name ? expr->as.call.name : "?");
            for (int i = 0; i < expr->as.call.nargs; i++) {
                if (i) printf(", ");
                printExprPrec(expr->as.call.args[i], 0);
            }
            putchar(')');
            return;
    }
}

// Print a NEKO expression to stdout
void nekoPrintExpr(const NekoExpr* expr) {
    // Start recursive printing with no parent precedence
    printExprPrec(expr, 0);
}

// Append a readable expression string with simple precedence handling
static bool appendExprText(NekoStringBuf* buf, const NekoExpr* expr, int parentPrec) {
    // Print a visible placeholder for missing expression nodes
    if (!expr) return nekoStringBufAppendText(buf, "<null>");

    // Dispatch by expression kind while respecting parent precedence
    switch (expr->kind) {
        case NEKO_EXPR_CONST:
            return nekoStringBufAppendFormat(buf, "%Lg", expr->as.constant);
        case NEKO_EXPR_VAR:
            return nekoStringBufAppendText(buf, expr->as.var ? expr->as.var : "?");
        case NEKO_EXPR_ADD:
        case NEKO_EXPR_SUB:
        case NEKO_EXPR_MUL:
        case NEKO_EXPR_DIV:
        case NEKO_EXPR_POW: {
            int prec = exprPrecedence(expr);
            bool parens = parentPrec > prec;
            const char* op = expr->kind == NEKO_EXPR_ADD ? " + "
                : expr->kind == NEKO_EXPR_SUB ? " - "
                : expr->kind == NEKO_EXPR_MUL ? " * "
                : expr->kind == NEKO_EXPR_DIV ? " / "
                : " ^ ";
            if (parens && !nekoStringBufAppendChar(buf, '(')) return false;
            if (!appendExprText(buf, expr->as.binary.lhs, prec)) return false;
            if (!nekoStringBufAppendText(buf, op)) return false;
            if (!appendExprText(buf, expr->as.binary.rhs, prec + (expr->kind == NEKO_EXPR_POW || expr->kind == NEKO_EXPR_DIV))) return false;
            if (parens && !nekoStringBufAppendChar(buf, ')')) return false;
            return true;
        }
        case NEKO_EXPR_NEG:
            if (parentPrec > 30 && !nekoStringBufAppendChar(buf, '(')) return false;
            if (!nekoStringBufAppendChar(buf, '-')) return false;
            if (!appendExprText(buf, expr->as.unary.arg, 30)) return false;
            if (parentPrec > 30 && !nekoStringBufAppendChar(buf, ')')) return false;
            return true;
        case NEKO_EXPR_SIN:
        case NEKO_EXPR_COS:
        case NEKO_EXPR_TAN:
        case NEKO_EXPR_ASIN:
        case NEKO_EXPR_ACOS:
        case NEKO_EXPR_ATAN:
        case NEKO_EXPR_EXP:
        case NEKO_EXPR_LOG:
        case NEKO_EXPR_SQRT:
        case NEKO_EXPR_ABS:
        case NEKO_EXPR_ERF:
        case NEKO_EXPR_EI:
        case NEKO_EXPR_STEP:
            return nekoStringBufAppendText(buf, unaryName(expr->kind))
                && nekoStringBufAppendChar(buf, '(')
                && appendExprText(buf, expr->as.unary.arg, 0)
                && nekoStringBufAppendChar(buf, ')');
        case NEKO_EXPR_CALL:
            if (!nekoStringBufAppendText(buf, expr->as.call.name ? expr->as.call.name : "?")) return false;
            if (!nekoStringBufAppendChar(buf, '(')) return false;
            for (int i = 0; i < expr->as.call.nargs; i++) {
                if (i && !nekoStringBufAppendText(buf, ", ")) return false;
                if (!appendExprText(buf, expr->as.call.args[i], 0)) return false;
            }
            return nekoStringBufAppendChar(buf, ')');
    }
    return false;
}

// Return an owned readable string for a NEKO expression
char* nekoExprToString(const NekoExpr* expr) {
    // Build the string with recursive expression formatting
    NekoStringBuf buf = {0};
    if (!appendExprText(&buf, expr, 0)) {
        free(buf.data);
        return NULL;
    }
    return buf.data;
}

// Append a serialized expression tree
static bool appendSerializedExpr(NekoStringBuf* buf, const NekoExpr* expr) {
    // Reject missing expressions because the format stores complete trees
    if (!expr) return false;

    // Emit a compact typed prefix form
    switch (expr->kind) {
        case NEKO_EXPR_CONST:
            return nekoStringBufAppendFormat(buf, "C:%La;", expr->as.constant);
        case NEKO_EXPR_VAR: {
            const char* name = expr->as.var ? expr->as.var : "";
            return nekoStringBufAppendFormat(buf, "V:%zu:%s;", strlen(name), name);
        }
        case NEKO_EXPR_ADD:
        case NEKO_EXPR_SUB:
        case NEKO_EXPR_MUL:
        case NEKO_EXPR_DIV:
        case NEKO_EXPR_POW:
            return nekoStringBufAppendFormat(buf, "B:%d:{", (int)expr->kind)
                && appendSerializedExpr(buf, expr->as.binary.lhs)
                && nekoStringBufAppendText(buf, "}{")
                && appendSerializedExpr(buf, expr->as.binary.rhs)
                && nekoStringBufAppendText(buf, "};");
        case NEKO_EXPR_NEG:
        case NEKO_EXPR_SIN:
        case NEKO_EXPR_COS:
        case NEKO_EXPR_TAN:
        case NEKO_EXPR_ASIN:
        case NEKO_EXPR_ACOS:
        case NEKO_EXPR_ATAN:
        case NEKO_EXPR_EXP:
        case NEKO_EXPR_LOG:
        case NEKO_EXPR_SQRT:
        case NEKO_EXPR_ABS:
        case NEKO_EXPR_ERF:
        case NEKO_EXPR_EI:
        case NEKO_EXPR_STEP:
            return nekoStringBufAppendFormat(buf, "U:%d:{", (int)expr->kind)
                && appendSerializedExpr(buf, expr->as.unary.arg)
                && nekoStringBufAppendText(buf, "};");
        case NEKO_EXPR_CALL: {
            const char* name = expr->as.call.name ? expr->as.call.name : "";
            if (!nekoStringBufAppendFormat(buf, "F:%zu:%s:%d:", strlen(name), name, expr->as.call.nargs)) return false;
            for (int i = 0; i < expr->as.call.nargs; i++) {
                if (!nekoStringBufAppendChar(buf, '{')) return false;
                if (!appendSerializedExpr(buf, expr->as.call.args[i])) return false;
                if (!nekoStringBufAppendChar(buf, '}')) return false;
            }
            return nekoStringBufAppendChar(buf, ';');
        }
    }
    return false;
}

// Return an owned serialized string for a NEKO expression
char* nekoSerializeExpr(const NekoExpr* expr) {
    // Serialize the tree into a GUI-safe ASCII payload
    NekoStringBuf buf = {0};
    if (!appendSerializedExpr(&buf, expr)) {
        free(buf.data);
        return NULL;
    }
    return buf.data;
}

// Parse an unsigned size from a serialized expression
static bool parseSerializedSize(const char** p, size_t* out) {
    // Require valid parser state
    if (!p || !*p || !out) return false;

    // Accumulate decimal digits
    size_t value = 0;
    const char* s = *p;
    if (*s < '0' || *s > '9') return false;
    while (*s >= '0' && *s <= '9') {
        value = value * 10 + (size_t)(*s - '0');
        s++;
    }
    *p = s;
    *out = value;
    return true;
}

// Parse an integer from a serialized expression
static bool parseSerializedInt(const char** p, int* out) {
    // Parse through size_t because kind tags are nonnegative
    size_t value = 0;
    if (!parseSerializedSize(p, &value) || value > (size_t)INT_MAX) return false;
    *out = (int)value;
    return true;
}

// Consume a required serialized character
static bool consumeSerializedChar(const char** p, char ch) {
    // Check and advance over the requested character
    if (!p || !*p || **p != ch) return false;
    (*p)++;
    return true;
}

// Parse one braced serialized child expression
static NekoExpr* parseSerializedBracedExpr(const char** p);

// Parse one serialized expression tree
static NekoExpr* parseSerializedExpr(const char** p) {
    // Require an expression tag
    if (!p || !*p || !**p) return NULL;
    char tag = *(*p)++;
    if (!consumeSerializedChar(p, ':')) return NULL;

    // Parse constants directly with strtold
    if (tag == 'C') {
        char* end = NULL;
        long double value = strtold(*p, &end);
        if (end == *p || !end || *end != ';') return NULL;
        *p = end + 1;
        return nekoConst(value);
    }

    // Parse variables using the stored name length
    if (tag == 'V') {
        size_t len = 0;
        if (!parseSerializedSize(p, &len) || !consumeSerializedChar(p, ':')) return NULL;
        char* name = malloc(len + 1);
        if (!name) return NULL;
        memcpy(name, *p, len);
        name[len] = '\0';
        *p += len;
        if (!consumeSerializedChar(p, ';')) {
            free(name);
            return NULL;
        }
        NekoExpr* expr = nekoVar(name);
        free(name);
        return expr;
    }

    // Parse unary nodes with one braced child
    if (tag == 'U') {
        int kindValue = 0;
        if (!parseSerializedInt(p, &kindValue) || !consumeSerializedChar(p, ':')) return NULL;
        NekoExpr* arg = parseSerializedBracedExpr(p);
        if (!arg || !consumeSerializedChar(p, ';')) {
            nekoFreeExpr(arg);
            return NULL;
        }
        return nekoUnary((NekoExprKind)kindValue, arg);
    }

    // Parse binary nodes with two braced children
    if (tag == 'B') {
        int kindValue = 0;
        if (!parseSerializedInt(p, &kindValue) || !consumeSerializedChar(p, ':')) return NULL;
        NekoExpr* lhs = parseSerializedBracedExpr(p);
        NekoExpr* rhs = parseSerializedBracedExpr(p);
        if (!lhs || !rhs || !consumeSerializedChar(p, ';')) {
            nekoFreeExpr(lhs);
            nekoFreeExpr(rhs);
            return NULL;
        }
        return nekoBinary((NekoExprKind)kindValue, lhs, rhs);
    }

    // Parse call nodes with length-prefixed names and braced args
    if (tag == 'F') {
        size_t len = 0;
        int nargs = 0;
        if (!parseSerializedSize(p, &len) || !consumeSerializedChar(p, ':')) return NULL;
        char* name = malloc(len + 1);
        if (!name) return NULL;
        memcpy(name, *p, len);
        name[len] = '\0';
        *p += len;
        if (!consumeSerializedChar(p, ':') || !parseSerializedInt(p, &nargs) || !consumeSerializedChar(p, ':')) {
            free(name);
            return NULL;
        }
        NekoExpr** args = nargs > 0 ? calloc((size_t)nargs, sizeof(NekoExpr*)) : NULL;
        if (nargs > 0 && !args) {
            free(name);
            return NULL;
        }
        for (int i = 0; i < nargs; i++) {
            args[i] = parseSerializedBracedExpr(p);
            if (!args[i]) {
                for (int j = 0; j < i; j++) nekoFreeExpr(args[j]);
                free(args);
                free(name);
                return NULL;
            }
        }
        if (!consumeSerializedChar(p, ';')) {
            for (int i = 0; i < nargs; i++) nekoFreeExpr(args[i]);
            free(args);
            free(name);
            return NULL;
        }
        NekoExpr* expr = nekoCall(name, args, nargs);
        free(name);
        return expr;
    }
    return NULL;
}

// Parse one braced serialized child expression
static NekoExpr* parseSerializedBracedExpr(const char** p) {
    // Read the opening brace, child expression, and closing brace
    if (!consumeSerializedChar(p, '{')) return NULL;
    NekoExpr* expr = parseSerializedExpr(p);
    if (!expr || !consumeSerializedChar(p, '}')) {
        nekoFreeExpr(expr);
        return NULL;
    }
    return expr;
}

// Reconstruct a NEKO expression from a serialized string
NekoExpr* nekoDeserializeExpr(const char* text) {
    // Parse one complete expression and reject trailing junk
    if (!text) return NULL;
    const char* p = text;
    NekoExpr* expr = parseSerializedExpr(&p);
    if (!expr || *p != '\0') {
        nekoFreeExpr(expr);
        return NULL;
    }
    return expr;
}
