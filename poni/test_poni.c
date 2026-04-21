
/* AI GENERATED TEST SUITE */

// Everything here looks good to me, but note this was generated my an LLM.
// More tests might be warranted if using for production.

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "hebi.h"
#include "poni.h"
#include "sokko.h"

/* ---------- Test infrastructure ---------- */

static int testsRun = 0;
static int testsPassed = 0;

#define CHECK(cond, name) do { \
    testsRun++; \
    if (cond) { testsPassed++; printf("  [PASS] %s\n", name); } \
    else { printf("  [FAIL] %s (line %d)\n", name, __LINE__); } \
} while (0)

#define SECTION(name) printf("\n=== %s ===\n", name)

// Approximate equality for doubles
static bool approx(double a, double b, double tol) {
    if (isnan(a) && isnan(b)) return true;
    return fabs(a - b) <= tol;
}

/* PONI is real-only. These helpers bridge to Sokko's MatrixElement API:
   R() lifts a double into a real MatrixElement; re() pulls the real part
   back out. Using them keeps the tests readable. */
static inline double re(MatrixElement e) { return e.value.real; }
static inline MatrixElement R(double x) { return elemFromReal(x); }

/* ---------- Helper tests ---------- */

void testComp() {
    SECTION("comp");
    long long a = 1, b = 2, c = 1;
    CHECK(comp(&a, &b) < 0, "comp a < b");
    CHECK(comp(&b, &a) > 0, "comp b > a");
    CHECK(comp(&a, &c) == 0, "comp a == c");

    long long arr[] = {3, 1, 4, 1, 5, 9, 2, 6};
    qsort(arr, 8, sizeof(long long), comp);
    CHECK(arr[0] == 1 && arr[7] == 9, "qsort with comp");
}

/* ---------- Kinematics primitive tests ---------- */

void testDisplacement() {
    SECTION("displacement");
    CHECK(displacement(NULL, NULL) == NULL, "NULL inputs");

    Vector* v2 = constructVector(2);
    Vector* v3 = constructVector(3);
    CHECK(displacement(v2, v3) == NULL, "dim mismatch");
    freeVector(v2); freeVector(v3);

    // Origin to (3, 4, 5)
    Vector* origin = constructVector3(R(0), R(0), R(0));
    Vector* p = constructVector3(R(3), R(4), R(5));
    double* d = displacement(origin, p);
    CHECK(d != NULL, "allocated");
    CHECK(approx(d[0], 3.0, 1e-10) && approx(d[1], 4.0, 1e-10) && approx(d[2], 5.0, 1e-10),
          "values correct");
    free(d);

    // Self-displacement = 0
    double* z = displacement(p, p);
    CHECK(approx(z[0], 0.0, 1e-10) && approx(z[1], 0.0, 1e-10) && approx(z[2], 0.0, 1e-10),
          "self-displacement zero");
    free(z);

    // Reverse direction: negative
    Vector* q = constructVector3(R(1), R(2), R(3));
    double* dn = displacement(p, q);
    CHECK(approx(dn[0], -2.0, 1e-10) && approx(dn[1], -2.0, 1e-10) && approx(dn[2], -2.0, 1e-10),
          "reversed direction");
    free(dn);

    freeVector(origin); freeVector(p); freeVector(q);
}

void testAverageVelocity() {
    SECTION("averageVelocity");
    CHECK(averageVelocity(NULL, NULL, 1.0) == NULL, "NULL inputs");

    Vector* p1 = constructVector3(R(0), R(0), R(0));
    Vector* p2 = constructVector3(R(6), R(8), R(0));
    CHECK(averageVelocity(p1, p2, 0) == NULL, "time 0 NULL");
    CHECK(averageVelocity(p1, p2, -1) == NULL, "negative time NULL");

    Vector* v2 = constructVector(2);
    CHECK(averageVelocity(p1, v2, 1.0) == NULL, "dim mismatch");
    freeVector(v2);

    double* v = averageVelocity(p1, p2, 2.0);
    CHECK(v != NULL, "allocated");
    CHECK(approx(v[0], 3.0, 1e-10) && approx(v[1], 4.0, 1e-10) && approx(v[2], 0.0, 1e-10),
          "(6,8,0) over 2s = (3,4,0)");
    free(v);

    // Same-point: zero velocity
    double* z = averageVelocity(p1, p1, 5.0);
    CHECK(approx(z[0], 0.0, 1e-10) && approx(z[1], 0.0, 1e-10) && approx(z[2], 0.0, 1e-10),
          "same point: zero velocity");
    free(z);

    freeVector(p1); freeVector(p2);
}

void testAverageAcceleration() {
    SECTION("averageAcceleration");
    CHECK(averageAcceleration(NULL, NULL, NULL, 1.0) == NULL, "NULL inputs");

    Vector* p1 = constructVector3(R(0), R(0), R(0));
    Vector* p2 = constructVector3(R(10), R(0), R(0));
    Vector* v0 = constructVector3(R(0), R(0), R(0));
    CHECK(averageAcceleration(p1, p2, NULL, 1.0) == NULL, "NULL initVel");
    CHECK(averageAcceleration(p1, p2, v0, 0) == NULL, "time 0 NULL");
    CHECK(averageAcceleration(p1, p2, v0, -1) == NULL, "negative time NULL");

    Vector* v2 = constructVector(2);
    CHECK(averageAcceleration(p1, p2, v2, 1.0) == NULL, "dim mismatch");
    freeVector(v2);

    // From rest, 10 m in 2 s: 10 = 0 + 0.5*a*4 => a = 5
    double* a = averageAcceleration(p1, p2, v0, 2.0);
    CHECK(a != NULL, "allocated");
    CHECK(approx(a[0], 5.0, 1e-10), "rest to 10m in 2s: a = 5");
    free(a);

    // With initial velocity: v0 = (2,0,0), t=3s, p2 = 0 + 2*3 + 0.5*a*9 = 6 + 4.5a
    // Pick p2 = (10.5, 0, 0) => a = 1
    Vector* v1 = constructVector3(R(2), R(0), R(0));
    Vector* p3 = constructVector3(R(10.5), R(0), R(0));
    double* a2 = averageAcceleration(p1, p3, v1, 3.0);
    CHECK(approx(a2[0], 1.0, 1e-10), "a=1 from formula");
    free(a2);

    // Inverts positionAtTime: p0, v0, a given, compute p, then recover a
    Vector* accel = constructVector3(R(2.0), R(-1.0), R(0.5));
    Vector* vInit = constructVector3(R(1.0), R(3.0), R(0.0));
    Vector* posFromAt = positionAtTime(p1, vInit, accel, 4.0);
    double* aBack = averageAcceleration(p1, posFromAt, vInit, 4.0);
    CHECK(approx(aBack[0], 2.0, 1e-10) &&
          approx(aBack[1], -1.0, 1e-10) &&
          approx(aBack[2], 0.5, 1e-10), "round-trip with positionAtTime");
    free(aBack);
    freeVector(accel); freeVector(vInit); freeVector(posFromAt);

    freeVector(p1); freeVector(p2); freeVector(v0); freeVector(v1); freeVector(p3);
}

void testVelocityAtTime() {
    SECTION("velocityAtTime");
    CHECK(velocityAtTime(NULL, NULL, 1.0) == NULL, "NULL inputs");

    Vector* v0 = constructVector3(R(1), R(2), R(3));
    Vector* a = constructVector3(R(0), R(0), R(0));
    CHECK(velocityAtTime(v0, a, -1) == NULL, "negative time NULL");

    Vector* v2 = constructVector(2);
    CHECK(velocityAtTime(v0, v2, 1.0) == NULL, "dim mismatch");
    freeVector(v2);

    // t = 0: v = v0
    Vector* vt0 = velocityAtTime(v0, a, 0.0);
    CHECK(matrixComp(vt0, v0, 1e-10), "t=0: v = v0");
    freeVector(vt0);

    // No accel: v = v0 at all t
    Vector* vConst = velocityAtTime(v0, a, 5.0);
    CHECK(matrixComp(vConst, v0, 1e-10), "no accel: v = v0");
    freeVector(vConst);

    // With accel: (1,2,3) + (2,0,-1)*5 = (11, 2, -2)
    Vector* a1 = constructVector3(R(2), R(0), R(-1));
    Vector* v5 = velocityAtTime(v0, a1, 5.0);
    CHECK(approx(re(getEntry(v5, 0, 0)), 11.0, 1e-10) &&
          approx(re(getEntry(v5, 1, 0)), 2.0, 1e-10) &&
          approx(re(getEntry(v5, 2, 0)), -2.0, 1e-10), "v(5) = (11, 2, -2)");
    freeVector(v5); freeVector(a1);

    freeVector(v0); freeVector(a);
}

void testPositionAtTime() {
    SECTION("positionAtTime");
    CHECK(positionAtTime(NULL, NULL, NULL, 1.0) == NULL, "NULL inputs");

    Vector* p0 = constructVector3(R(0), R(0), R(0));
    Vector* v0 = constructVector3(R(1), R(2), R(3));
    Vector* a = constructVector3(R(0), R(0), R(0));
    CHECK(positionAtTime(p0, v0, a, -1) == NULL, "negative time NULL");

    Vector* v2 = constructVector(2);
    CHECK(positionAtTime(p0, v0, v2, 1.0) == NULL, "dim mismatch");
    freeVector(v2);

    // t = 0: position = p0
    Vector* p0t = positionAtTime(p0, v0, a, 0.0);
    CHECK(matrixComp(p0t, p0, 1e-10), "t=0: pos = p0");
    freeVector(p0t);

    // No accel: p = p0 + v0*t = (3, 6, 9) at t=3
    Vector* p3 = positionAtTime(p0, v0, a, 3.0);
    CHECK(approx(re(getEntry(p3, 0, 0)), 3.0, 1e-10) &&
          approx(re(getEntry(p3, 1, 0)), 6.0, 1e-10) &&
          approx(re(getEntry(p3, 2, 0)), 9.0, 1e-10), "no accel: p = v0*t");
    freeVector(p3);

    // With accel at t=2: p = 0 + (1,2,3)*2 + 0.5*(2,-1,0)*4 = (2+4, 4-2, 6+0) = (6, 2, 6)
    Vector* a1 = constructVector3(R(2), R(-1), R(0));
    Vector* p2t = positionAtTime(p0, v0, a1, 2.0);
    CHECK(approx(re(getEntry(p2t, 0, 0)), 6.0, 1e-10) &&
          approx(re(getEntry(p2t, 1, 0)), 2.0, 1e-10) &&
          approx(re(getEntry(p2t, 2, 0)), 6.0, 1e-10), "p(2) = (6, 2, 6)");
    freeVector(p2t); freeVector(a1);

    freeVector(p0); freeVector(v0); freeVector(a);
}

void testSpeedAtPosition() {
    SECTION("speedAtPosition");
    CHECK(speedAtPosition(NULL, NULL, NULL, NULL) == NULL, "NULL inputs");

    Vector* p0 = constructVector3(R(0), R(0), R(0));
    Vector* v0 = constructVector3(R(0), R(0), R(0));
    Vector* a = constructVector3(R(2), R(0), R(0));
    Vector* pos = constructVector3(R(9), R(0), R(0));

    Vector* v2 = constructVector(2);
    CHECK(speedAtPosition(p0, v2, a, pos) == NULL, "dim mismatch");
    freeVector(v2);

    // From rest, a=2, x=9: v^2 = 0 + 2*2*9 = 36 => speed = 6
    Vector* s = speedAtPosition(p0, v0, a, pos);
    CHECK(approx(re(getEntry(s, 0, 0)), 6.0, 1e-10), "rest, a=2, x=9: speed=6");
    freeVector(s);

    // With v0 = 3: v^2 = 9 + 2*2*8 = 41
    Vector* v03 = constructVector3(R(3), R(0), R(0));
    Vector* pos8 = constructVector3(R(8), R(0), R(0));
    Vector* s2 = speedAtPosition(p0, v03, a, pos8);
    CHECK(approx(re(getEntry(s2, 0, 0)), sqrt(41.0), 1e-10), "v0=3, a=2, x=8: speed=sqrt(41)");
    freeVector(s2); freeVector(v03); freeVector(pos8);

    // No accel: speed stays |v0|
    Vector* aZero = constructVector3(R(0), R(0), R(0));
    Vector* v5 = constructVector3(R(5), R(0), R(0));
    Vector* anywhere = constructVector3(R(100), R(0), R(0));
    Vector* sConst = speedAtPosition(p0, v5, aZero, anywhere);
    CHECK(approx(re(getEntry(sConst, 0, 0)), 5.0, 1e-10), "no accel: speed = |v0|");
    freeVector(sConst); freeVector(aZero); freeVector(v5); freeVector(anywhere);

    // At initial position: speed = |v0|
    Vector* vInit = constructVector3(R(7), R(0), R(0));
    Vector* sInit = speedAtPosition(p0, vInit, a, p0);
    CHECK(approx(re(getEntry(sInit, 0, 0)), 7.0, 1e-10), "at initPos: speed = |v0|");
    freeVector(sInit); freeVector(vInit);

    freeVector(p0); freeVector(v0); freeVector(a); freeVector(pos);
}

void testVelocityAtPosition() {
    SECTION("velocityAtPosition");
    CHECK(velocityAtPosition(NULL, NULL, NULL, NULL) == NULL, "NULL inputs");

    Vector* p0 = constructVector3(R(0), R(0), R(0));
    Vector* v0 = constructVector3(R(0), R(0), R(0));
    Vector* a = constructVector3(R(2), R(0), R(0));
    Vector* pos = constructVector3(R(9), R(0), R(0));

    Vector* v2 = constructVector(2);
    CHECK(velocityAtPosition(p0, v2, a, pos) == NULL, "dim mismatch");
    freeVector(v2);

    // From rest, a=2, x=9: t=3 => v = 0 + 2*3 = 6
    Vector* v = velocityAtPosition(p0, v0, a, pos);
    CHECK(v != NULL, "allocated");
    CHECK(approx(re(getEntry(v, 0, 0)), 6.0, 1e-10), "rest, a=2, x=9: v=6");
    freeVector(v);

    // Ball thrown up: v0=10, a=-10 along y. At y=4 on ascent: t = smallest root
    // quadratic: 0.5*(-10)*t^2 + 10*t - 4 = 0 => -5t^2 + 10t - 4 = 0 => t = (10 ± sqrt(100-80))/10
    // Smaller t = (10-sqrt(20))/10 ≈ 0.553, v = 10 - 10*t = sqrt(20)
    Vector* p0y = constructVector3(R(0), R(0), R(0));
    Vector* vUp0 = constructVector3(R(0), R(10), R(0));
    Vector* aDown = constructVector3(R(0), R(-10), R(0));
    Vector* y4 = constructVector3(R(0), R(4), R(0));
    Vector* vAt4 = velocityAtPosition(p0y, vUp0, aDown, y4);
    CHECK(approx(re(getEntry(vAt4, 1, 0)), sqrt(20.0), 1e-9), "ball on ascent at y=4: v=+sqrt(20)");
    freeVector(vAt4);

    // At peak (y=5): t=1, v = 10 - 10*1 = 0
    Vector* yPeak = constructVector3(R(0), R(5), R(0));
    Vector* vPeak = velocityAtPosition(p0y, vUp0, aDown, yPeak);
    CHECK(approx(re(getEntry(vPeak, 1, 0)), 0.0, 1e-9), "at peak: v=0");
    freeVector(vPeak);

    // At initial position: smallest non-negative time is 0, v = v0
    Vector* vStart = velocityAtPosition(p0y, vUp0, aDown, p0y);
    CHECK(approx(re(getEntry(vStart, 1, 0)), 10.0, 1e-9), "at initPos: v = v0 (t=0)");
    freeVector(vStart);

    // Unreachable: y = 10 (peak is at 5)
    Vector* yUnreach = constructVector3(R(0), R(10), R(0));
    CHECK(velocityAtPosition(p0y, vUp0, aDown, yUnreach) == NULL, "unreachable position NULL");
    freeVector(yUnreach);

    // Zero acceleration: v = v0 everywhere
    Vector* aZero = constructVector3(R(0), R(0), R(0));
    Vector* vConstInit = constructVector3(R(1), R(2), R(3));
    Vector* anyPos = constructVector3(R(100), R(200), R(300));
    Vector* vConst = velocityAtPosition(p0, vConstInit, aZero, anyPos);
    CHECK(matrixComp(vConst, vConstInit, 1e-10), "zero accel: v = v0");
    freeVector(vConst); freeVector(aZero); freeVector(vConstInit); freeVector(anyPos);

    freeVector(p0y); freeVector(vUp0); freeVector(aDown); freeVector(y4);
    freeVector(p0); freeVector(v0); freeVector(a); freeVector(pos);
}

void testProjectileInfo() {
    SECTION("getProjectileInfo");
    CHECK(getProjectileInfo(-1, 45, 0) == NULL, "negative initVel NULL");
    CHECK(getProjectileInfo(10, -1, 0) == NULL, "negative angle NULL");
    CHECK(getProjectileInfo(10, 91, 0) == NULL, "angle > 90 NULL");

    // Launch at 45 deg from ground, v0=10:
    //   range     = v^2 * sin(2θ) / g = 100 / g
    //   peak      = (v*sinθ)^2 / (2g) = 25 / g
    //   flightT   = 2*v*sinθ / g = 10*sqrt(2) / g
    ProjectileInfo* i45 = getProjectileInfo(10, 45, 0);
    CHECK(i45 != NULL, "allocated");
    CHECK(approx(i45->range, 100.0 / A_GRAVITY, 1e-6), "45 deg: range = 100/g");
    CHECK(approx(i45->peakHeight, 25.0 / A_GRAVITY, 1e-6), "45 deg: peak = 25/g");
    CHECK(approx(i45->timeOfFlight, 10.0 * sqrt(2.0) / A_GRAVITY, 1e-6), "45 deg: flight = 10*sqrt(2)/g");
    free(i45);

    // Straight up (90 deg) from ground, v0=10:
    //   range = 0, peak = v^2/(2g) = 50/g, flight = 2v/g = 20/g
    ProjectileInfo* iUp = getProjectileInfo(10, 90, 0);
    CHECK(approx(iUp->range, 0.0, 1e-6), "straight up: range = 0");
    CHECK(approx(iUp->peakHeight, 50.0 / A_GRAVITY, 1e-6), "straight up: peak = 50/g");
    CHECK(approx(iUp->timeOfFlight, 20.0 / A_GRAVITY, 1e-6), "straight up: flight = 20/g");
    free(iUp);

    // Horizontal (0 deg) from height h=20, v0=10:
    //   flight = sqrt(2h/g), range = v*flight, peak = h
    ProjectileInfo* iH = getProjectileInfo(10, 0, 20);
    double tH = sqrt(2.0 * 20.0 / A_GRAVITY);
    CHECK(approx(iH->timeOfFlight, tH, 1e-6), "horizontal: flight = sqrt(2h/g)");
    CHECK(approx(iH->range, 10.0 * tH, 1e-6), "horizontal: range = v*flight");
    CHECK(approx(iH->peakHeight, 20.0, 1e-6), "horizontal: peak = h");
    free(iH);

    // Zero velocity: range = 0, peak = h
    ProjectileInfo* iZero = getProjectileInfo(0, 45, 5);
    CHECK(approx(iZero->range, 0.0, 1e-6), "v=0: range = 0");
    CHECK(approx(iZero->peakHeight, 5.0, 1e-6), "v=0: peak = h");
    free(iZero);
}

void testCentripetalAcceleration() {
    SECTION("centripetalAcceleration");
    CHECK(isnan(centripetalAcceleration(-1, 5)), "negative vel NAN");
    CHECK(isnan(centripetalAcceleration(5, 0)), "zero radius NAN");
    CHECK(isnan(centripetalAcceleration(5, -1)), "negative radius NAN");

    CHECK(approx(centripetalAcceleration(10, 5), 20.0, 1e-10), "v=10, r=5: a=20");
    CHECK(approx(centripetalAcceleration(0, 5), 0.0, 1e-10), "v=0: a=0");
    CHECK(approx(centripetalAcceleration(3, 9), 1.0, 1e-10), "v=3, r=9: a=1");
}

void testAngularVelocity() {
    SECTION("angularVelocity");
    CHECK(isnan(angularVelocity(-1, 5)), "negative vel NAN");
    CHECK(isnan(angularVelocity(5, 0)), "zero radius NAN");
    CHECK(isnan(angularVelocity(5, -1)), "negative radius NAN");

    CHECK(approx(angularVelocity(10, 5), 2.0, 1e-10), "v=10, r=5: omega=2");
    CHECK(approx(angularVelocity(0, 5), 0.0, 1e-10), "v=0: omega=0");
    CHECK(approx(angularVelocity(3, 6), 0.5, 1e-10), "v=3, r=6: omega=0.5");
}

/* ---------- Dynamics tests ---------- */

void testConstructBody() {
    SECTION("constructBody");
    Vector* p = constructVector3(R(0), R(0), R(0));
    Vector* v = constructVector3(R(1), R(2), R(3));
    CHECK(constructBody(-1, p, v) == NULL, "negative mass NULL");
    CHECK(constructBody(0, p, v) == NULL, "zero mass NULL");
    CHECK(constructBody(1, NULL, v) == NULL, "NULL pos");
    CHECK(constructBody(1, p, NULL) == NULL, "NULL vel");

    Body* b = constructBody(2.5, p, v);
    CHECK(b != NULL, "allocated");
    CHECK(b->mass == 2.5, "mass set");
    CHECK(b->pos == p, "pos set");
    CHECK(b->velocity == v, "velocity set");
    CHECK(b->nForces == 0, "nForces = 0");
    CHECK(b->forces != NULL, "forces allocated");

    free(b->forces); free(b);
    freeVector(p); freeVector(v);
}

void testAddRemoveForce() {
    SECTION("add/removeForce");
    Vector* p = constructVector3(R(0), R(0), R(0));
    Vector* v = constructVector3(R(0), R(0), R(0));
    Body* b = constructBody(1, p, v);

    Vector* fv = constructVector3(R(1), R(0), R(0));
    Vector* ft = constructVector3(R(0), R(0), R(0));
    Force* f = constructForce("test", fv, ft);

    addForce(b, f);
    CHECK(b->nForces == 1, "nForces = 1 after add");
    CHECK(b->forces[0] == f, "force stored");

    removeForce(b, f);
    CHECK(b->nForces == 0, "nForces = 0 after remove");

    removeForce(b, f);
    CHECK(b->nForces == 0, "remove nonexistent no-op");

    addForce(NULL, f);
    addForce(b, NULL);
    removeForce(NULL, f);
    removeForce(b, NULL);
    CHECK(b->nForces == 0, "null args no-op");

    free(f);
    freeVector(fv); freeVector(ft);
    free(b->forces); free(b);
    freeVector(p); freeVector(v);
}

void testNetForce() {
    SECTION("netForce");
    CHECK(netForce(NULL) == NULL, "NULL body NULL");

    Vector* p = constructVector3(R(0), R(0), R(0));
    Vector* v = constructVector3(R(0), R(0), R(0));
    Body* b = constructBody(1, p, v);

    Vector* zero = constructVector3(R(0), R(0), R(0));
    Vector* n0 = netForce(b);
    CHECK(n0 != NULL && matrixComp(n0, zero, 1e-10), "no forces = zero");
    freeVector(n0);

    Vector* fv1 = constructVector3(R(3), R(0), R(0));
    Vector* ft1 = constructVector3(R(0), R(0), R(0));
    Force* f1 = constructForce("f1", fv1, ft1);
    addForce(b, f1);

    Vector* n1 = netForce(b);
    Vector* expect1 = constructVector3(R(3), R(0), R(0));
    CHECK(matrixComp(n1, expect1, 1e-10), "one force = that force");
    freeVector(n1); freeVector(expect1);

    Vector* fv2 = constructVector3(R(1), R(2), R(-1));
    Vector* ft2 = constructVector3(R(0), R(0), R(0));
    Force* f2 = constructForce("f2", fv2, ft2);
    addForce(b, f2);

    Vector* n2 = netForce(b);
    Vector* expect2 = constructVector3(R(4), R(2), R(-1));
    CHECK(matrixComp(n2, expect2, 1e-10), "sum of forces");
    freeVector(n2); freeVector(expect2);

    free(f1); free(f2);
    freeVector(fv1); freeVector(ft1);
    freeVector(fv2); freeVector(ft2);
    freeVector(zero);
    free(b->forces); free(b);
    freeVector(p); freeVector(v);
}

void testAccelerationFromForce() {
    SECTION("accelerationFromForce");
    CHECK(accelerationFromForce(NULL) == NULL, "NULL body NULL");

    Vector* p = constructVector3(R(0), R(0), R(0));
    Vector* v = constructVector3(R(0), R(0), R(0));
    Body* b = constructBody(2.0, p, v);

    Vector* zero = constructVector3(R(0), R(0), R(0));
    Vector* a0 = accelerationFromForce(b);
    CHECK(matrixComp(a0, zero, 1e-10), "no forces = zero accel");
    freeVector(a0); freeVector(zero);

    Vector* fv = constructVector3(R(10), R(0), R(0));
    Vector* ft = constructVector3(R(0), R(0), R(0));
    Force* f = constructForce("f", fv, ft);
    addForce(b, f);

    Vector* a1 = accelerationFromForce(b);
    Vector* exp = constructVector3(R(5), R(0), R(0));
    CHECK(matrixComp(a1, exp, 1e-10), "F=10,m=2 -> a=5");
    freeVector(a1); freeVector(exp);

    free(f);
    freeVector(fv); freeVector(ft);
    free(b->forces); free(b);
    freeVector(p); freeVector(v);
}

void testGravityForce() {
    SECTION("gravityForce");
    CHECK(gravityForce(NULL) == NULL, "NULL body NULL");

    Vector* p = constructVector3(R(0), R(0), R(0));
    Vector* v = constructVector3(R(0), R(0), R(0));
    Body* b = constructBody(2.0, p, v);

    Force* g = gravityForce(b);
    CHECK(g != NULL, "allocated");
    CHECK(approx(re(getEntry(g->vector, 0, 0)), 0.0, 1e-10), "x = 0");
    CHECK(approx(re(getEntry(g->vector, 1, 0)), -2.0 * A_GRAVITY, 1e-10), "y = -m*g");
    CHECK(approx(re(getEntry(g->vector, 2, 0)), 0.0, 1e-10), "z = 0");
    CHECK(matrixComp(g->tailPos, p, 1e-10), "tailPos = body pos");

    freeVector(g->vector); freeVector(g->tailPos); free(g);
    free(b->forces); free(b);
    freeVector(p); freeVector(v);
}

void testNormalForce() {
    SECTION("normalForce");
    Vector* p = constructVector3(R(0), R(0), R(0));
    Vector* v = constructVector3(R(0), R(0), R(0));
    Body* b = constructBody(3.0, p, v);
    Vector* up = constructVector3(R(0), R(1), R(0));
    Vector* v2 = constructVector(2);

    CHECK(normalForce(NULL, up) == NULL, "NULL body NULL");
    CHECK(normalForce(b, NULL) == NULL, "NULL normal NULL");
    CHECK(normalForce(b, v2) == NULL, "dim mismatch NULL");
    freeVector(v2);

    // Flat ground, upward normal: N = m*g in +y
    Force* n = normalForce(b, up);
    CHECK(n != NULL, "allocated");
    CHECK(approx(re(getEntry(n->vector, 0, 0)), 0.0, 1e-10), "x = 0");
    CHECK(approx(re(getEntry(n->vector, 1, 0)), 3.0 * A_GRAVITY, 1e-10), "y = m*g");
    CHECK(approx(re(getEntry(n->vector, 2, 0)), 0.0, 1e-10), "z = 0");
    freeVector(n->vector); freeVector(n->tailPos); free(n);

    // 45 deg incline: magnitude = m*g*cos(45)
    Vector* tilt = constructVector3(R(sin(M_PI/4)), R(cos(M_PI/4)), R(0));
    Force* n2 = normalForce(b, tilt);
    CHECK(approx(l2Norm(n2->vector), 3.0 * A_GRAVITY * cos(M_PI/4), 1e-10), "45 deg incline magnitude");
    freeVector(n2->vector); freeVector(n2->tailPos); free(n2);

    // Non-unit normal: should be normalized internally
    Vector* big = constructVector3(R(0), R(5), R(0));
    Force* n3 = normalForce(b, big);
    CHECK(approx(re(getEntry(n3->vector, 1, 0)), 3.0 * A_GRAVITY, 1e-10), "non-unit normal normalized");
    freeVector(n3->vector); freeVector(n3->tailPos); free(n3);

    freeVector(tilt); freeVector(big); freeVector(up);
    free(b->forces); free(b);
    freeVector(p); freeVector(v);
}

void testFrictionForce() {
    SECTION("frictionForce");
    Vector* dir = constructVector3(R(1), R(0), R(0));
    Vector* fv = constructVector3(R(0), R(10), R(0));
    Vector* ft = constructVector3(R(7), R(8), R(9));
    Force* normal = constructForce("n", fv, ft);

    CHECK(frictionForce(NULL, 0.3, dir) == NULL, "NULL normal NULL");
    CHECK(frictionForce(normal, 0.3, NULL) == NULL, "NULL dir NULL");
    CHECK(frictionForce(normal, -0.1, dir) == NULL, "negative mu NULL");

    // mu=0.3, |N|=10, motion +x -> friction = -3 in x
    Force* f = frictionForce(normal, 0.3, dir);
    CHECK(f != NULL, "allocated");
    CHECK(approx(re(getEntry(f->vector, 0, 0)), -3.0, 1e-10), "opposes motion");
    CHECK(approx(re(getEntry(f->vector, 1, 0)), 0.0, 1e-10), "y = 0");
    CHECK(matrixComp(f->tailPos, ft, 1e-10), "tailPos from normal");

    freeVector(f->vector); freeVector(f->tailPos); free(f);
    free(normal);
    freeVector(dir); freeVector(fv); freeVector(ft);
}

/* ---------- Conservation tests ---------- */

void testMomentum() {
    SECTION("momentumMagnitude / momentumVector");
    CHECK(isnan(momentumMagnitude(NULL)), "NULL body NAN");
    CHECK(momentumVector(NULL) == NULL, "NULL body vector NULL");

    Vector* p = constructVector3(R(0), R(0), R(0));
    Vector* v = constructVector3(R(3), R(4), R(0));
    Body* b = constructBody(2.0, p, v);
    CHECK(approx(momentumMagnitude(b), 10.0, 1e-10), "m=2, |v|=5 -> |p|=10");

    Vector* pv = momentumVector(b);
    CHECK(pv != NULL, "vector allocated");
    CHECK(approx(re(getEntry(pv, 0, 0)), 6.0, 1e-10), "p_x = m*v_x = 6");
    CHECK(approx(re(getEntry(pv, 1, 0)), 8.0, 1e-10), "p_y = m*v_y = 8");
    CHECK(approx(re(getEntry(pv, 2, 0)), 0.0, 1e-10), "p_z = 0");
    freeVector(pv);

    free(b->forces); free(b);
    freeVector(p); freeVector(v);
}

void testKineticEnergy() {
    SECTION("kineticEnergy");
    CHECK(isnan(kineticEnergy(NULL)), "NULL body NAN");

    Vector* p = constructVector3(R(0), R(0), R(0));
    Vector* v = constructVector3(R(3), R(4), R(0));
    Body* b = constructBody(2.0, p, v);
    // KE = 0.5 * m * |v|^2 = 0.5 * 2 * 25 = 25
    CHECK(approx(kineticEnergy(b), 25.0, 1e-10), "m=2, |v|=5 -> KE=25");

    free(b->forces); free(b);
    freeVector(p); freeVector(v);
}

void testGravPotentialEnergy() {
    SECTION("gravPotentialEnergy");
    CHECK(isnan(gravPotentialEnergy(NULL, 10)), "NULL body NAN");

    Vector* p = constructVector3(R(0), R(0), R(0));
    Vector* v = constructVector3(R(0), R(0), R(0));
    Body* b = constructBody(2.0, p, v);

    CHECK(approx(gravPotentialEnergy(b, 0), 0.0, 1e-10), "h=0 -> 0");
    CHECK(approx(gravPotentialEnergy(b, 5), 2.0 * A_GRAVITY * 5, 1e-10), "h=5 -> mgh");
    CHECK(approx(gravPotentialEnergy(b, -3), -2.0 * A_GRAVITY * 3, 1e-10), "h<0 negative");

    free(b->forces); free(b);
    freeVector(p); freeVector(v);
}

void testSpringPotentialEnergy() {
    SECTION("springPotentialEnergy");
    CHECK(isnan(springPotentialEnergy(-1, 1)), "negative k NAN");
    CHECK(isnan(springPotentialEnergy(1, -1)), "negative x NAN");
    CHECK(approx(springPotentialEnergy(100, 0.2), 2.0, 1e-10), "k=100, x=0.2 -> 2");
    CHECK(approx(springPotentialEnergy(0, 5), 0.0, 1e-10), "k=0 -> 0");
    CHECK(approx(springPotentialEnergy(50, 0), 0.0, 1e-10), "x=0 -> 0");
}

void testWork() {
    SECTION("work");
    Vector* fv = constructVector3(R(3), R(0), R(0));
    Vector* ft = constructVector3(R(0), R(0), R(0));
    Force* f = constructForce("f", fv, ft);
    Vector* disp = constructVector3(R(4), R(0), R(0));
    Vector* disp2 = constructVector(2);

    CHECK(isnan(work(NULL, disp)), "NULL F NAN");
    CHECK(isnan(work(f, NULL)), "NULL disp NAN");
    CHECK(isnan(work(f, disp2)), "dim mismatch NAN");
    freeVector(disp2);

    CHECK(approx(work(f, disp), 12.0, 1e-10), "parallel: F.disp = 12");

    Vector* perp = constructVector3(R(0), R(4), R(0));
    CHECK(approx(work(f, perp), 0.0, 1e-10), "perpendicular: 0");
    freeVector(perp);

    Vector* opp = constructVector3(R(-4), R(0), R(0));
    CHECK(approx(work(f, opp), -12.0, 1e-10), "antiparallel: -12");
    freeVector(opp);

    free(f);
    freeVector(fv); freeVector(ft); freeVector(disp);
}

void testPower() {
    SECTION("power");
    Vector* fv = constructVector3(R(3), R(0), R(0));
    Vector* ft = constructVector3(R(0), R(0), R(0));
    Force* f = constructForce("f", fv, ft);
    Vector* vel = constructVector3(R(2), R(0), R(0));
    Vector* vel2 = constructVector(2);

    CHECK(isnan(power(NULL, vel)), "NULL F NAN");
    CHECK(isnan(power(f, NULL)), "NULL vel NAN");
    CHECK(isnan(power(f, vel2)), "dim mismatch NAN");
    freeVector(vel2);

    CHECK(approx(power(f, vel), 6.0, 1e-10), "F.v = 6");

    Vector* perp = constructVector3(R(0), R(5), R(0));
    CHECK(approx(power(f, perp), 0.0, 1e-10), "perpendicular: 0");
    freeVector(perp);

    free(f);
    freeVector(fv); freeVector(ft); freeVector(vel);
}

void testImpulse() {
    SECTION("impulseMagnitude / impulseVector");
    Vector* fv = constructVector3(R(3), R(4), R(0));  // |F| = 5
    Vector* ft = constructVector3(R(0), R(0), R(0));
    Force* f = constructForce("f", fv, ft);

    CHECK(isnan(impulseMagnitude(NULL, 1)), "NULL F NAN");
    CHECK(isnan(impulseMagnitude(f, -1)), "negative time NAN");
    CHECK(impulseVector(NULL, 1) == NULL, "NULL F vector NULL");

    CHECK(approx(impulseMagnitude(f, 2), 10.0, 1e-10), "|F|*t = 10");
    CHECK(approx(impulseMagnitude(f, 0), 0.0, 1e-10), "t=0 -> 0");

    // Vector: J = F * t
    Vector* J = impulseVector(f, 2);
    CHECK(J != NULL, "vector allocated");
    CHECK(approx(re(getEntry(J, 0, 0)), 6.0, 1e-10), "J_x = F_x*t = 6");
    CHECK(approx(re(getEntry(J, 1, 0)), 8.0, 1e-10), "J_y = F_y*t = 8");
    CHECK(approx(re(getEntry(J, 2, 0)), 0.0, 1e-10), "J_z = 0");
    freeVector(J);

    Vector* J0 = impulseVector(f, 0);
    CHECK(approx(l2Norm(J0), 0.0, 1e-10), "t=0: |J|=0");
    freeVector(J0);

    free(f);
    freeVector(fv); freeVector(ft);
}

/* ---------- Collision tests ---------- */

void testCenterOfMass() {
    SECTION("centerOfMass");
    CHECK(centerOfMass(NULL, 3) == NULL, "NULL bodies NULL");

    Vector* p1 = constructVector3(R(0), R(0), R(0));
    Vector* v1 = constructVector3(R(0), R(0), R(0));
    Body* b1 = constructBody(2.0, p1, v1);

    Vector* p2 = constructVector3(R(6), R(0), R(0));
    Vector* v2 = constructVector3(R(0), R(0), R(0));
    Body* b2 = constructBody(1.0, p2, v2);

    Body* arr[2] = {b1, b2};
    CHECK(centerOfMass(arr, 0) == NULL, "nBodies=0 NULL");
    CHECK(centerOfMass(arr, -1) == NULL, "nBodies<0 NULL");

    // m1=2 at 0, m2=1 at 6: com = (2*0 + 1*6)/3 = 2
    Vector* com = centerOfMass(arr, 2);
    CHECK(com != NULL, "allocated");
    CHECK(approx(re(getEntry(com, 0, 0)), 2.0, 1e-10), "com x = 2");
    CHECK(approx(re(getEntry(com, 1, 0)), 0.0, 1e-10), "com y = 0");
    CHECK(approx(re(getEntry(com, 2, 0)), 0.0, 1e-10), "com z = 0");
    freeVector(com);

    // Single body: com = its position
    Body* one[1] = {b1};
    Vector* comOne = centerOfMass(one, 1);
    CHECK(matrixComp(comOne, p1, 1e-10), "single body: com = its pos");
    freeVector(comOne);

    // Equal masses at (2,0,0) and (-2,0,0): com at origin
    Vector* pa = constructVector3(R(2), R(0), R(0));
    Vector* va = constructVector3(R(0), R(0), R(0));
    Body* ba = constructBody(1.0, pa, va);
    Vector* pb = constructVector3(R(-2), R(0), R(0));
    Vector* vb = constructVector3(R(0), R(0), R(0));
    Body* bb = constructBody(1.0, pb, vb);
    Body* pair[2] = {ba, bb};
    Vector* comPair = centerOfMass(pair, 2);
    Vector* zero = constructVector3(R(0), R(0), R(0));
    CHECK(matrixComp(comPair, zero, 1e-10), "equal masses symmetric: com at origin");
    freeVector(comPair); freeVector(zero);

    // Dim mismatch: second body is 2D
    Vector* p2D = constructVector2(R(1), R(1));
    Vector* v2D = constructVector2(R(0), R(0));
    Body* bMix = constructBody(1.0, p2D, v2D);
    Body* mixed[2] = {b1, bMix};
    CHECK(centerOfMass(mixed, 2) == NULL, "dim mismatch NULL");

    free(b1->forces); free(b1);
    free(b2->forces); free(b2);
    free(ba->forces); free(ba);
    free(bb->forces); free(bb);
    free(bMix->forces); free(bMix);
    freeVector(p1); freeVector(v1); freeVector(p2); freeVector(v2);
    freeVector(pa); freeVector(va); freeVector(pb); freeVector(vb);
    freeVector(p2D); freeVector(v2D);
}

void testCenterOfMassVelocity() {
    SECTION("centerOfMassVelocity");
    CHECK(centerOfMassVelocity(NULL, 3) == NULL, "NULL bodies NULL");

    Vector* p1 = constructVector3(R(0), R(0), R(0));
    Vector* v1 = constructVector3(R(4), R(0), R(0));
    Body* b1 = constructBody(2.0, p1, v1);

    Vector* p2 = constructVector3(R(0), R(0), R(0));
    Vector* v2 = constructVector3(R(-2), R(0), R(0));
    Body* b2 = constructBody(1.0, p2, v2);

    Body* arr[2] = {b1, b2};
    CHECK(centerOfMassVelocity(arr, 0) == NULL, "nBodies=0 NULL");

    // v_cm = (2*4 + 1*(-2))/3 = 6/3 = 2
    Vector* vcm = centerOfMassVelocity(arr, 2);
    CHECK(vcm != NULL, "allocated");
    CHECK(approx(re(getEntry(vcm, 0, 0)), 2.0, 1e-10), "v_cm x = 2");
    CHECK(approx(re(getEntry(vcm, 1, 0)), 0.0, 1e-10), "v_cm y = 0");
    freeVector(vcm);

    // Zero-momentum frame: equal masses, opposite velocities
    Vector* pa = constructVector3(R(0), R(0), R(0));
    Vector* va = constructVector3(R(5), R(0), R(0));
    Body* ba = constructBody(3.0, pa, va);
    Vector* pb = constructVector3(R(0), R(0), R(0));
    Vector* vb = constructVector3(R(-5), R(0), R(0));
    Body* bb = constructBody(3.0, pb, vb);
    Body* pair[2] = {ba, bb};
    Vector* vcmZero = centerOfMassVelocity(pair, 2);
    Vector* zero = constructVector3(R(0), R(0), R(0));
    CHECK(matrixComp(vcmZero, zero, 1e-10), "zero-momentum frame: v_cm = 0");
    freeVector(vcmZero); freeVector(zero);

    free(b1->forces); free(b1);
    free(b2->forces); free(b2);
    free(ba->forces); free(ba);
    free(bb->forces); free(bb);
    freeVector(p1); freeVector(v1); freeVector(p2); freeVector(v2);
    freeVector(pa); freeVector(va); freeVector(pb); freeVector(vb);
}

void testElasticCollision1D() {
    SECTION("elasticCollision1D");

    // Equal masses swap velocities
    Vector* p1 = constructVector3(R(0), R(0), R(0));
    Vector* v1 = constructVector3(R(3), R(0), R(0));
    Body* b1 = constructBody(1.0, p1, v1);
    Vector* p2 = constructVector3(R(5), R(0), R(0));
    Vector* v2 = constructVector3(R(-1), R(0), R(0));
    Body* b2 = constructBody(1.0, p2, v2);

    elasticCollision1D(b1, b2);
    CHECK(approx(re(getEntry(b1->velocity, 0, 0)), -1.0, 1e-10), "equal mass: b1 takes b2's v");
    CHECK(approx(re(getEntry(b2->velocity, 0, 0)), 3.0, 1e-10), "equal mass: b2 takes b1's v");

    // Momentum and KE conservation on a nontrivial mass ratio
    Vector* pa = constructVector3(R(0), R(0), R(0));
    Vector* va = constructVector3(R(4), R(0), R(0));
    Body* ba = constructBody(3.0, pa, va);
    Vector* pb = constructVector3(R(5), R(0), R(0));
    Vector* vb = constructVector3(R(-2), R(0), R(0));
    Body* bb = constructBody(1.0, pb, vb);

    double pBefore = 3.0 * 4.0 + 1.0 * (-2.0);
    double keBefore = 0.5 * 3.0 * 16.0 + 0.5 * 1.0 * 4.0;
    elasticCollision1D(ba, bb);
    double va2 = re(getEntry(ba->velocity, 0, 0));
    double vb2 = re(getEntry(bb->velocity, 0, 0));
    double pAfter = 3.0 * va2 + 1.0 * vb2;
    double keAfter = 0.5 * 3.0 * va2 * va2 + 0.5 * 1.0 * vb2 * vb2;
    CHECK(approx(pAfter, pBefore, 1e-10), "momentum conserved");
    CHECK(approx(keAfter, keBefore, 1e-10), "KE conserved");

    // Heavy stationary target (m2 >> m1): light body nearly reverses
    Vector* ph = constructVector3(R(0), R(0), R(0));
    Vector* vh = constructVector3(R(5), R(0), R(0));
    Body* bh = constructBody(0.001, ph, vh);
    Vector* pHvy = constructVector3(R(1), R(0), R(0));
    Vector* vHvy = constructVector3(R(0), R(0), R(0));
    Body* bHvy = constructBody(1000.0, pHvy, vHvy);
    elasticCollision1D(bh, bHvy);
    CHECK(approx(re(getEntry(bh->velocity, 0, 0)), -5.0, 1e-2), "light bounces off heavy: ~-v");
    CHECK(approx(re(getEntry(bHvy->velocity, 0, 0)), 0.0, 1e-2), "heavy barely moves");

    // NULL safety
    elasticCollision1D(NULL, b1);
    elasticCollision1D(b1, NULL);
    CHECK(1, "NULL args no crash");

    free(b1->forces); free(b1);
    free(b2->forces); free(b2);
    free(ba->forces); free(ba);
    free(bb->forces); free(bb);
    free(bh->forces); free(bh);
    free(bHvy->forces); free(bHvy);
    freeVector(p1); freeVector(v1); freeVector(p2); freeVector(v2);
    freeVector(pa); freeVector(va); freeVector(pb); freeVector(vb);
    freeVector(ph); freeVector(vh); freeVector(pHvy); freeVector(vHvy);
}

void testInelasticCollision1D() {
    SECTION("inelasticCollision1D");

    // Equal masses head-on with opposite speeds: both stop
    Vector* p1 = constructVector3(R(0), R(0), R(0));
    Vector* v1 = constructVector3(R(3), R(0), R(0));
    Body* b1 = constructBody(1.0, p1, v1);
    Vector* p2 = constructVector3(R(5), R(0), R(0));
    Vector* v2 = constructVector3(R(-3), R(0), R(0));
    Body* b2 = constructBody(1.0, p2, v2);

    inelasticCollision1D(b1, b2);
    CHECK(approx(re(getEntry(b1->velocity, 0, 0)), 0.0, 1e-10), "symmetric headon: b1 stops");
    CHECK(approx(re(getEntry(b2->velocity, 0, 0)), 0.0, 1e-10), "symmetric headon: b2 stops");

    // Momentum conserved, KE lost
    Vector* pa = constructVector3(R(0), R(0), R(0));
    Vector* va = constructVector3(R(10), R(0), R(0));
    Body* ba = constructBody(2.0, pa, va);
    Vector* pb = constructVector3(R(5), R(0), R(0));
    Vector* vb = constructVector3(R(0), R(0), R(0));
    Body* bb = constructBody(3.0, pb, vb);

    double pBefore = 2.0 * 10.0 + 3.0 * 0.0;
    double keBefore = 0.5 * 2.0 * 100.0 + 0.0;
    inelasticCollision1D(ba, bb);
    double va2 = re(getEntry(ba->velocity, 0, 0));
    double vb2 = re(getEntry(bb->velocity, 0, 0));
    CHECK(approx(va2, vb2, 1e-10), "inelastic: velocities equal");
    CHECK(approx(va2, pBefore / 5.0, 1e-10), "v = p_total / M_total");
    double keAfter = 0.5 * 5.0 * va2 * va2;
    CHECK(keAfter < keBefore, "KE lost in inelastic");

    // NULL safety
    inelasticCollision1D(NULL, b1);
    inelasticCollision1D(b1, NULL);
    CHECK(1, "NULL args no crash");

    free(b1->forces); free(b1);
    free(b2->forces); free(b2);
    free(ba->forces); free(ba);
    free(bb->forces); free(bb);
    freeVector(p1); freeVector(v1); freeVector(p2); freeVector(v2);
    freeVector(pa); freeVector(va); freeVector(pb); freeVector(vb);
}

/* ---------- Rotational dynamics tests ---------- */

void testMomentOfInertiaPoint() {
    SECTION("momentOfInertiaPoint");
    CHECK(isnan(momentOfInertiaPoint(-1, 1)), "negative m NAN");
    CHECK(isnan(momentOfInertiaPoint(1, -1)), "negative r NAN");
    CHECK(approx(momentOfInertiaPoint(2, 3), 18.0, 1e-10), "m=2, r=3: I = mr^2 = 18");
    CHECK(approx(momentOfInertiaPoint(5, 0), 0.0, 1e-10), "r=0: I=0");
    CHECK(approx(momentOfInertiaPoint(0, 7), 0.0, 1e-10), "m=0: I=0");
}

void testMomentOfInertiaRod() {
    SECTION("momentOfInertiaRod");
    CHECK(isnan(momentOfInertiaRod(-1, 1)), "negative m NAN");
    CHECK(isnan(momentOfInertiaRod(1, -1)), "negative L NAN");
    // m=12, L=1: I = (1/12)*12*1 = 1
    CHECK(approx(momentOfInertiaRod(12, 1), 1.0, 1e-10), "m=12, L=1: I = 1");
    // m=3, L=2: I = (1/12)*3*4 = 1
    CHECK(approx(momentOfInertiaRod(3, 2), 1.0, 1e-10), "m=3, L=2: I = 1");
    CHECK(approx(momentOfInertiaRod(0, 5), 0.0, 1e-10), "m=0: I=0");
    CHECK(approx(momentOfInertiaRod(5, 0), 0.0, 1e-10), "L=0: I=0");
}

void testMomentOfInertiaDisk() {
    SECTION("momentOfInertiaDisk");
    CHECK(isnan(momentOfInertiaDisk(-1, 1)), "negative m NAN");
    CHECK(isnan(momentOfInertiaDisk(1, -1)), "negative R NAN");
    // m=2, R=3: I = 0.5*2*9 = 9
    CHECK(approx(momentOfInertiaDisk(2, 3), 9.0, 1e-10), "m=2, R=3: I = 9");
    CHECK(approx(momentOfInertiaDisk(0, 5), 0.0, 1e-10), "m=0: I=0");
    CHECK(approx(momentOfInertiaDisk(5, 0), 0.0, 1e-10), "R=0: I=0");
}

void testParallelAxisTheorem() {
    SECTION("parallelAxisTheorem");
    CHECK(isnan(parallelAxisTheorem(-1, 1, 1)), "negative I_cm NAN");
    CHECK(isnan(parallelAxisTheorem(1, -1, 1)), "negative m NAN");
    // I_cm = 5, m = 2, d = 3: I = 5 + 2*9 = 23
    CHECK(approx(parallelAxisTheorem(5, 2, 3), 23.0, 1e-10), "I_cm + m*d^2 = 23");
    // d = 0: I = I_cm
    CHECK(approx(parallelAxisTheorem(7, 2, 0), 7.0, 1e-10), "d=0: I = I_cm");
    // Rod about end = (1/3) m L^2 = I_cm + m*(L/2)^2 = (1/12)mL^2 + (1/4)mL^2
    double Icm = momentOfInertiaRod(6, 2);
    double Iend = parallelAxisTheorem(Icm, 6, 1);
    CHECK(approx(Iend, (1.0/3.0) * 6 * 4, 1e-10), "rod about end via parallel axis");
}

void testTorque() {
    SECTION("torque");
    Vector* pivot = constructVector3(R(0), R(0), R(0));
    Vector* fv = constructVector3(R(0), R(1), R(0));
    Vector* ft = constructVector3(R(1), R(0), R(0));
    Force* F = constructForce("f", fv, ft);

    CHECK(torque(NULL, pivot) == NULL, "NULL F NULL");
    CHECK(torque(F, NULL) == NULL, "NULL pivot NULL");

    Vector* pivot2 = constructVector2(R(0), R(0));
    CHECK(torque(F, pivot2) == NULL, "dim mismatch NULL");

    // 3D: r = (1,0,0), F = (0,1,0) -> tau = (0,0,1)
    Vector* t3 = torque(F, pivot);
    CHECK(t3 != NULL && t3->numRows == 3, "3D returns 3-vector");
    CHECK(approx(re(getEntry(t3, 0, 0)), 0.0, 1e-10), "tau x = 0");
    CHECK(approx(re(getEntry(t3, 1, 0)), 0.0, 1e-10), "tau y = 0");
    CHECK(approx(re(getEntry(t3, 2, 0)), 1.0, 1e-10), "tau z = 1");
    freeVector(t3);

    // 3D: force at pivot -> zero torque
    Vector* ftAt = constructVector3(R(0), R(0), R(0));
    Force* Fat = constructForce("fat", fv, ftAt);
    Vector* tZero = torque(Fat, pivot);
    Vector* zero3 = constructVector3(R(0), R(0), R(0));
    CHECK(matrixComp(tZero, zero3, 1e-10), "force at pivot: zero torque");
    freeVector(tZero); freeVector(zero3);

    // 3D: F parallel to r -> zero torque
    Vector* fParallel = constructVector3(R(2), R(0), R(0));
    Force* Fp = constructForce("fp", fParallel, ft);
    Vector* tPar = torque(Fp, pivot);
    Vector* zeroP = constructVector3(R(0), R(0), R(0));
    CHECK(matrixComp(tPar, zeroP, 1e-10), "F parallel to r: zero torque");
    freeVector(tPar); freeVector(zeroP);

    // 2D: r = (1, 0), F = (0, 2) -> tau_z = 1*2 - 0*0 = 2
    Vector* ft2 = constructVector2(R(1), R(0));
    Vector* fv2 = constructVector2(R(0), R(2));
    Force* F2 = constructForce("f2", fv2, ft2);
    Vector* t2 = torque(F2, pivot2);
    CHECK(t2 != NULL && t2->numRows == 1, "2D returns 1-vector");
    CHECK(approx(re(getEntry(t2, 0, 0)), 2.0, 1e-10), "2D tau_z = 2");
    freeVector(t2);

    // 2D: reversed r, same F -> negative torque
    Vector* ft2neg = constructVector2(R(-1), R(0));
    Force* F2n = constructForce("f2n", fv2, ft2neg);
    Vector* t2n = torque(F2n, pivot2);
    CHECK(approx(re(getEntry(t2n, 0, 0)), -2.0, 1e-10), "2D tau_z flips sign with -r");
    freeVector(t2n);

    free(F); free(Fat); free(Fp); free(F2); free(F2n);
    freeVector(pivot); freeVector(pivot2);
    freeVector(fv); freeVector(ft); freeVector(ftAt);
    freeVector(fParallel); freeVector(ft2); freeVector(fv2); freeVector(ft2neg);
}

void testAngularMomentum() {
    SECTION("angularMomentum");
    Vector* pivot = constructVector3(R(0), R(0), R(0));
    Vector* pos = constructVector3(R(1), R(0), R(0));
    Vector* vel = constructVector3(R(0), R(2), R(0));
    Body* b = constructBody(3.0, pos, vel);

    CHECK(angularMomentum(NULL, pivot) == NULL, "NULL body NULL");
    CHECK(angularMomentum(b, NULL) == NULL, "NULL pivot NULL");

    Vector* pivot2 = constructVector2(R(0), R(0));
    CHECK(angularMomentum(b, pivot2) == NULL, "dim mismatch NULL");

    // 3D: r=(1,0,0), p=(0,6,0), L = r x p = (0,0,6)
    Vector* L3 = angularMomentum(b, pivot);
    CHECK(L3 != NULL && L3->numRows == 3, "3D returns 3-vector");
    CHECK(approx(re(getEntry(L3, 0, 0)), 0.0, 1e-10), "L x = 0");
    CHECK(approx(re(getEntry(L3, 1, 0)), 0.0, 1e-10), "L y = 0");
    CHECK(approx(re(getEntry(L3, 2, 0)), 6.0, 1e-10), "L z = 6");
    freeVector(L3);

    // 3D: body at pivot -> L = 0
    Vector* posAt = constructVector3(R(0), R(0), R(0));
    Vector* velAt = constructVector3(R(1), R(2), R(3));
    Body* bAt = constructBody(1.0, posAt, velAt);
    Vector* LAt = angularMomentum(bAt, pivot);
    Vector* zero3 = constructVector3(R(0), R(0), R(0));
    CHECK(matrixComp(LAt, zero3, 1e-10), "body at pivot: L=0");
    freeVector(LAt); freeVector(zero3);

    // 2D: r=(1,0), p=(0,6), L_z = 6
    Vector* pos2 = constructVector2(R(1), R(0));
    Vector* vel2 = constructVector2(R(0), R(2));
    Body* b2 = constructBody(3.0, pos2, vel2);
    Vector* L2 = angularMomentum(b2, pivot2);
    CHECK(L2 != NULL && L2->numRows == 1, "2D returns 1-vector");
    CHECK(approx(re(getEntry(L2, 0, 0)), 6.0, 1e-10), "2D L_z = 6");
    freeVector(L2);

    // 2D: radial velocity -> L = 0
    Vector* pos2r = constructVector2(R(2), R(0));
    Vector* vel2r = constructVector2(R(3), R(0));
    Body* b2r = constructBody(1.0, pos2r, vel2r);
    Vector* L2r = angularMomentum(b2r, pivot2);
    CHECK(approx(re(getEntry(L2r, 0, 0)), 0.0, 1e-10), "2D radial motion: L_z = 0");
    freeVector(L2r);

    free(b->forces); free(b);
    free(bAt->forces); free(bAt);
    free(b2->forces); free(b2);
    free(b2r->forces); free(b2r);
    freeVector(pivot); freeVector(pivot2);
    freeVector(pos); freeVector(vel);
    freeVector(posAt); freeVector(velAt);
    freeVector(pos2); freeVector(vel2);
    freeVector(pos2r); freeVector(vel2r);
}

void testRotationalKineticEnergy() {
    SECTION("rotationalKineticEnergy");
    CHECK(isnan(rotationalKineticEnergy(-1, 2)), "negative I NAN");
    // I=4, omega=3: KE = 0.5 * 4 * 9 = 18
    CHECK(approx(rotationalKineticEnergy(4, 3), 18.0, 1e-10), "I=4, omega=3: KE = 18");
    CHECK(approx(rotationalKineticEnergy(10, 0), 0.0, 1e-10), "omega=0: KE=0");
    CHECK(approx(rotationalKineticEnergy(0, 5), 0.0, 1e-10), "I=0: KE=0");
    // Sign of omega irrelevant (squared)
    CHECK(approx(rotationalKineticEnergy(2, -3), rotationalKineticEnergy(2, 3), 1e-10),
          "omega sign irrelevant");
}

void testAngularAccelerationFromTorque() {
    SECTION("angularAccelerationFromTorque");
    CHECK(isnan(angularAccelerationFromTorque(5, 0)), "I=0 NAN");
    CHECK(isnan(angularAccelerationFromTorque(5, -1)), "negative I NAN");
    // tau=10, I=2: alpha = 5
    CHECK(approx(angularAccelerationFromTorque(10, 2), 5.0, 1e-10), "tau=10, I=2: alpha=5");
    CHECK(approx(angularAccelerationFromTorque(0, 3), 0.0, 1e-10), "no torque: alpha=0");
    CHECK(approx(angularAccelerationFromTorque(-6, 2), -3.0, 1e-10), "negative tau: negative alpha");
}

/* ---------- Main ---------- */

int main(void) {
    printf("Running poni tests...\n");

    testComp();
    testDisplacement();
    testAverageVelocity();
    testAverageAcceleration();
    testVelocityAtTime();
    testPositionAtTime();
    testSpeedAtPosition();
    testVelocityAtPosition();
    testProjectileInfo();
    testCentripetalAcceleration();
    testAngularVelocity();

    testConstructBody();
    testAddRemoveForce();
    testNetForce();
    testAccelerationFromForce();
    testGravityForce();
    testNormalForce();
    testFrictionForce();

    testMomentum();
    testKineticEnergy();
    testGravPotentialEnergy();
    testSpringPotentialEnergy();
    testWork();
    testPower();
    testImpulse();

    testCenterOfMass();
    testCenterOfMassVelocity();
    testElasticCollision1D();
    testInelasticCollision1D();

    testMomentOfInertiaPoint();
    testMomentOfInertiaRod();
    testMomentOfInertiaDisk();
    testParallelAxisTheorem();
    testTorque();
    testAngularMomentum();
    testRotationalKineticEnergy();
    testAngularAccelerationFromTorque();

    printf("\n========================================\n");
    printf("Results: %d / %d tests passed\n", testsPassed, testsRun);
    printf("========================================\n");

    return (testsPassed == testsRun) ? 0 : 1;
}
