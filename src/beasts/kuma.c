/* ---------- Helper methods ---------- */

// Returns true is a Number can be used for KUMA methods, else false
bool isValidStatsNumber(Number x) {
    switch (x.type) {
        case NUMBER_INT:
            return true;
        case NUMBER_FRACTION:
            return fractionIsValid(x.as.frac);
        case NUMBER_REAL:
            return isfinite(x.as.x);
        case NUMBER_COMPLEX:
        case NUMBER_NAN:
            return false;
    }
    return false;
}