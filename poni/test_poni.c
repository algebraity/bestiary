
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
    Vector* origin = constructVector3(0, 0, 0);
    Vector* p = constructVector3(3, 4, 5);
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
    Vector* q = constructVector3(1, 2, 3);
    double* dn = displacement(p, q);
    CHECK(approx(dn[0], -2.0, 1e-10) && approx(dn[1], -2.0, 1e-10) && approx(dn[2], -2.0, 1e-10),
          "reversed direction");
    free(dn);

    freeVector(origin); freeVector(p); freeVector(q);
}

void testAverageVelocity() {
    SECTION("averageVelocity");
    CHECK(averageVelocity(NULL, NULL, 1.0) == NULL, "NULL inputs");

    Vector* p1 = constructVector3(0, 0, 0);
    Vector* p2 = constructVector3(6, 8, 0);
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

    Vector* p1 = constructVector3(0, 0, 0);
    Vector* p2 = constructVector3(10, 0, 0);
    Vector* v0 = constructVector3(0, 0, 0);
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
    Vector* v1 = constructVector3(2, 0, 0);
    Vector* p3 = constructVector3(10.5, 0, 0);
    double* a2 = averageAcceleration(p1, p3, v1, 3.0);
    CHECK(approx(a2[0], 1.0, 1e-10), "a=1 from formula");
    free(a2);

    // Inverts positionAtTime: p0, v0, a given, compute p, then recover a
    Vector* accel = constructVector3(2.0, -1.0, 0.5);
    Vector* vInit = constructVector3(1.0, 3.0, 0.0);
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

    Vector* v0 = constructVector3(1, 2, 3);
    Vector* a = constructVector3(0, 0, 0);
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
    Vector* a1 = constructVector3(2, 0, -1);
    Vector* v5 = velocityAtTime(v0, a1, 5.0);
    CHECK(approx(getEntry(v5, 0, 0), 11.0, 1e-10) &&
          approx(getEntry(v5, 1, 0), 2.0, 1e-10) &&
          approx(getEntry(v5, 2, 0), -2.0, 1e-10), "v(5) = (11, 2, -2)");
    freeVector(v5); freeVector(a1);

    freeVector(v0); freeVector(a);
}

void testPositionAtTime() {
    SECTION("positionAtTime");
    CHECK(positionAtTime(NULL, NULL, NULL, 1.0) == NULL, "NULL inputs");

    Vector* p0 = constructVector3(0, 0, 0);
    Vector* v0 = constructVector3(1, 2, 3);
    Vector* a = constructVector3(0, 0, 0);
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
    CHECK(approx(getEntry(p3, 0, 0), 3.0, 1e-10) &&
          approx(getEntry(p3, 1, 0), 6.0, 1e-10) &&
          approx(getEntry(p3, 2, 0), 9.0, 1e-10), "no accel: p = v0*t");
    freeVector(p3);

    // With accel at t=2: p = 0 + (1,2,3)*2 + 0.5*(2,-1,0)*4 = (2+4, 4-2, 6+0) = (6, 2, 6)
    Vector* a1 = constructVector3(2, -1, 0);
    Vector* p2t = positionAtTime(p0, v0, a1, 2.0);
    CHECK(approx(getEntry(p2t, 0, 0), 6.0, 1e-10) &&
          approx(getEntry(p2t, 1, 0), 2.0, 1e-10) &&
          approx(getEntry(p2t, 2, 0), 6.0, 1e-10), "p(2) = (6, 2, 6)");
    freeVector(p2t); freeVector(a1);

    freeVector(p0); freeVector(v0); freeVector(a);
}

void testSpeedAtPosition() {
    SECTION("speedAtPosition");
    CHECK(speedAtPosition(NULL, NULL, NULL, NULL) == NULL, "NULL inputs");

    Vector* p0 = constructVector3(0, 0, 0);
    Vector* v0 = constructVector3(0, 0, 0);
    Vector* a = constructVector3(2, 0, 0);
    Vector* pos = constructVector3(9, 0, 0);

    Vector* v2 = constructVector(2);
    CHECK(speedAtPosition(p0, v2, a, pos) == NULL, "dim mismatch");
    freeVector(v2);

    // From rest, a=2, x=9: v^2 = 0 + 2*2*9 = 36 => speed = 6
    Vector* s = speedAtPosition(p0, v0, a, pos);
    CHECK(approx(getEntry(s, 0, 0), 6.0, 1e-10), "rest, a=2, x=9: speed=6");
    freeVector(s);

    // With v0 = 3: v^2 = 9 + 2*2*8 = 41
    Vector* v03 = constructVector3(3, 0, 0);
    Vector* pos8 = constructVector3(8, 0, 0);
    Vector* s2 = speedAtPosition(p0, v03, a, pos8);
    CHECK(approx(getEntry(s2, 0, 0), sqrt(41.0), 1e-10), "v0=3, a=2, x=8: speed=sqrt(41)");
    freeVector(s2); freeVector(v03); freeVector(pos8);

    // No accel: speed stays |v0|
    Vector* aZero = constructVector3(0, 0, 0);
    Vector* v5 = constructVector3(5, 0, 0);
    Vector* anywhere = constructVector3(100, 0, 0);
    Vector* sConst = speedAtPosition(p0, v5, aZero, anywhere);
    CHECK(approx(getEntry(sConst, 0, 0), 5.0, 1e-10), "no accel: speed = |v0|");
    freeVector(sConst); freeVector(aZero); freeVector(v5); freeVector(anywhere);

    // At initial position: speed = |v0|
    Vector* vInit = constructVector3(7, 0, 0);
    Vector* sInit = speedAtPosition(p0, vInit, a, p0);
    CHECK(approx(getEntry(sInit, 0, 0), 7.0, 1e-10), "at initPos: speed = |v0|");
    freeVector(sInit); freeVector(vInit);

    freeVector(p0); freeVector(v0); freeVector(a); freeVector(pos);
}

void testVelocityAtPosition() {
    SECTION("velocityAtPosition");
    CHECK(velocityAtPosition(NULL, NULL, NULL, NULL) == NULL, "NULL inputs");

    Vector* p0 = constructVector3(0, 0, 0);
    Vector* v0 = constructVector3(0, 0, 0);
    Vector* a = constructVector3(2, 0, 0);
    Vector* pos = constructVector3(9, 0, 0);

    Vector* v2 = constructVector(2);
    CHECK(velocityAtPosition(p0, v2, a, pos) == NULL, "dim mismatch");
    freeVector(v2);

    // From rest, a=2, x=9: t=3 => v = 0 + 2*3 = 6
    Vector* v = velocityAtPosition(p0, v0, a, pos);
    CHECK(v != NULL, "allocated");
    CHECK(approx(getEntry(v, 0, 0), 6.0, 1e-10), "rest, a=2, x=9: v=6");
    freeVector(v);

    // Ball thrown up: v0=10, a=-10 along y. At y=4 on ascent: t = smallest root
    // quadratic: 0.5*(-10)*t^2 + 10*t - 4 = 0 => -5t^2 + 10t - 4 = 0 => t = (10 ± sqrt(100-80))/10
    // Smaller t = (10-sqrt(20))/10 ≈ 0.553, v = 10 - 10*t = sqrt(20)
    Vector* p0y = constructVector3(0, 0, 0);
    Vector* vUp0 = constructVector3(0, 10, 0);
    Vector* aDown = constructVector3(0, -10, 0);
    Vector* y4 = constructVector3(0, 4, 0);
    Vector* vAt4 = velocityAtPosition(p0y, vUp0, aDown, y4);
    CHECK(approx(getEntry(vAt4, 1, 0), sqrt(20.0), 1e-9), "ball on ascent at y=4: v=+sqrt(20)");
    freeVector(vAt4);

    // At peak (y=5): t=1, v = 10 - 10*1 = 0
    Vector* yPeak = constructVector3(0, 5, 0);
    Vector* vPeak = velocityAtPosition(p0y, vUp0, aDown, yPeak);
    CHECK(approx(getEntry(vPeak, 1, 0), 0.0, 1e-9), "at peak: v=0");
    freeVector(vPeak);

    // At initial position: smallest non-negative time is 0, v = v0
    Vector* vStart = velocityAtPosition(p0y, vUp0, aDown, p0y);
    CHECK(approx(getEntry(vStart, 1, 0), 10.0, 1e-9), "at initPos: v = v0 (t=0)");
    freeVector(vStart);

    // Unreachable: y = 10 (peak is at 5)
    Vector* yUnreach = constructVector3(0, 10, 0);
    CHECK(velocityAtPosition(p0y, vUp0, aDown, yUnreach) == NULL, "unreachable position NULL");
    freeVector(yUnreach);

    // Zero acceleration: v = v0 everywhere
    Vector* aZero = constructVector3(0, 0, 0);
    Vector* vConstInit = constructVector3(1, 2, 3);
    Vector* anyPos = constructVector3(100, 200, 300);
    Vector* vConst = velocityAtPosition(p0, vConstInit, aZero, anyPos);
    CHECK(matrixComp(vConst, vConstInit, 1e-10), "zero accel: v = v0");
    freeVector(vConst); freeVector(aZero); freeVector(vConstInit); freeVector(anyPos);

    freeVector(p0y); freeVector(vUp0); freeVector(aDown); freeVector(y4);
    freeVector(p0); freeVector(v0); freeVector(a); freeVector(pos);
}

void testProjectileInfo() {
    SECTION("projectileInfo");
    CHECK(projectileInfo(-1, 45, 0) == NULL, "negative initVel NULL");
    CHECK(projectileInfo(10, -1, 0) == NULL, "negative angle NULL");
    CHECK(projectileInfo(10, 91, 0) == NULL, "angle > 90 NULL");

    // Launch at 45 deg from ground, v0=10:
    //   range     = v^2 * sin(2θ) / g = 100 / g
    //   peak      = (v*sinθ)^2 / (2g) = 25 / g
    //   flightT   = 2*v*sinθ / g = 10*sqrt(2) / g
    double* i45 = projectileInfo(10, 45, 0);
    CHECK(i45 != NULL, "allocated");
    CHECK(approx(i45[0], 100.0 / A_GRAVITY, 1e-6), "45 deg: range = 100/g");
    CHECK(approx(i45[1], 25.0 / A_GRAVITY, 1e-6), "45 deg: peak = 25/g");
    CHECK(approx(i45[2], 10.0 * sqrt(2.0) / A_GRAVITY, 1e-6), "45 deg: flight = 10*sqrt(2)/g");
    free(i45);

    // Straight up (90 deg) from ground, v0=10:
    //   range = 0, peak = v^2/(2g) = 50/g, flight = 2v/g = 20/g
    double* iUp = projectileInfo(10, 90, 0);
    CHECK(approx(iUp[0], 0.0, 1e-6), "straight up: range = 0");
    CHECK(approx(iUp[1], 50.0 / A_GRAVITY, 1e-6), "straight up: peak = 50/g");
    CHECK(approx(iUp[2], 20.0 / A_GRAVITY, 1e-6), "straight up: flight = 20/g");
    free(iUp);

    // Horizontal (0 deg) from height h=20, v0=10:
    //   flight = sqrt(2h/g), range = v*flight, peak = h
    double* iH = projectileInfo(10, 0, 20);
    double tH = sqrt(2.0 * 20.0 / A_GRAVITY);
    CHECK(approx(iH[2], tH, 1e-6), "horizontal: flight = sqrt(2h/g)");
    CHECK(approx(iH[0], 10.0 * tH, 1e-6), "horizontal: range = v*flight");
    CHECK(approx(iH[1], 20.0, 1e-6), "horizontal: peak = h");
    free(iH);

    // Zero velocity: range = 0, peak = h
    double* iZero = projectileInfo(0, 45, 5);
    CHECK(approx(iZero[0], 0.0, 1e-6), "v=0: range = 0");
    CHECK(approx(iZero[1], 5.0, 1e-6), "v=0: peak = h");
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
    Vector* p = constructVector3(0, 0, 0);
    Vector* v = constructVector3(1, 2, 3);
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
    Vector* p = constructVector3(0, 0, 0);
    Vector* v = constructVector3(0, 0, 0);
    Body* b = constructBody(1, p, v);

    Vector* fv = constructVector3(1, 0, 0);
    Vector* ft = constructVector3(0, 0, 0);
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

    Vector* p = constructVector3(0, 0, 0);
    Vector* v = constructVector3(0, 0, 0);
    Body* b = constructBody(1, p, v);

    Vector* zero = constructVector3(0, 0, 0);
    Vector* n0 = netForce(b);
    CHECK(n0 != NULL && matrixComp(n0, zero, 1e-10), "no forces = zero");
    freeVector(n0);

    Vector* fv1 = constructVector3(3, 0, 0);
    Vector* ft1 = constructVector3(0, 0, 0);
    Force* f1 = constructForce("f1", fv1, ft1);
    addForce(b, f1);

    Vector* n1 = netForce(b);
    Vector* expect1 = constructVector3(3, 0, 0);
    CHECK(matrixComp(n1, expect1, 1e-10), "one force = that force");
    freeVector(n1); freeVector(expect1);

    Vector* fv2 = constructVector3(1, 2, -1);
    Vector* ft2 = constructVector3(0, 0, 0);
    Force* f2 = constructForce("f2", fv2, ft2);
    addForce(b, f2);

    Vector* n2 = netForce(b);
    Vector* expect2 = constructVector3(4, 2, -1);
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

    Vector* p = constructVector3(0, 0, 0);
    Vector* v = constructVector3(0, 0, 0);
    Body* b = constructBody(2.0, p, v);

    Vector* zero = constructVector3(0, 0, 0);
    Vector* a0 = accelerationFromForce(b);
    CHECK(matrixComp(a0, zero, 1e-10), "no forces = zero accel");
    freeVector(a0); freeVector(zero);

    Vector* fv = constructVector3(10, 0, 0);
    Vector* ft = constructVector3(0, 0, 0);
    Force* f = constructForce("f", fv, ft);
    addForce(b, f);

    Vector* a1 = accelerationFromForce(b);
    Vector* exp = constructVector3(5, 0, 0);
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

    Vector* p = constructVector3(0, 0, 0);
    Vector* v = constructVector3(0, 0, 0);
    Body* b = constructBody(2.0, p, v);

    Force* g = gravityForce(b);
    CHECK(g != NULL, "allocated");
    CHECK(approx(getEntry(g->vector, 0, 0), 0.0, 1e-10), "x = 0");
    CHECK(approx(getEntry(g->vector, 1, 0), -2.0 * A_GRAVITY, 1e-10), "y = -m*g");
    CHECK(approx(getEntry(g->vector, 2, 0), 0.0, 1e-10), "z = 0");
    CHECK(matrixComp(g->tailPos, p, 1e-10), "tailPos = body pos");

    freeVector(g->vector); freeVector(g->tailPos); free(g);
    free(b->forces); free(b);
    freeVector(p); freeVector(v);
}

void testNormalForce() {
    SECTION("normalForce");
    Vector* p = constructVector3(0, 0, 0);
    Vector* v = constructVector3(0, 0, 0);
    Body* b = constructBody(3.0, p, v);
    Vector* up = constructVector3(0, 1, 0);
    Vector* v2 = constructVector(2);

    CHECK(normalForce(NULL, up) == NULL, "NULL body NULL");
    CHECK(normalForce(b, NULL) == NULL, "NULL normal NULL");
    CHECK(normalForce(b, v2) == NULL, "dim mismatch NULL");
    freeVector(v2);

    // Flat ground, upward normal: N = m*g in +y
    Force* n = normalForce(b, up);
    CHECK(n != NULL, "allocated");
    CHECK(approx(getEntry(n->vector, 0, 0), 0.0, 1e-10), "x = 0");
    CHECK(approx(getEntry(n->vector, 1, 0), 3.0 * A_GRAVITY, 1e-10), "y = m*g");
    CHECK(approx(getEntry(n->vector, 2, 0), 0.0, 1e-10), "z = 0");
    freeVector(n->vector); freeVector(n->tailPos); free(n);

    // 45 deg incline: magnitude = m*g*cos(45)
    Vector* tilt = constructVector3(sin(M_PI/4), cos(M_PI/4), 0);
    Force* n2 = normalForce(b, tilt);
    CHECK(approx(l2Norm(n2->vector), 3.0 * A_GRAVITY * cos(M_PI/4), 1e-10), "45 deg incline magnitude");
    freeVector(n2->vector); freeVector(n2->tailPos); free(n2);

    // Non-unit normal: should be normalized internally
    Vector* big = constructVector3(0, 5, 0);
    Force* n3 = normalForce(b, big);
    CHECK(approx(getEntry(n3->vector, 1, 0), 3.0 * A_GRAVITY, 1e-10), "non-unit normal normalized");
    freeVector(n3->vector); freeVector(n3->tailPos); free(n3);

    freeVector(tilt); freeVector(big); freeVector(up);
    free(b->forces); free(b);
    freeVector(p); freeVector(v);
}

void testFrictionForce() {
    SECTION("frictionForce");
    Vector* dir = constructVector3(1, 0, 0);
    Vector* fv = constructVector3(0, 10, 0);
    Vector* ft = constructVector3(7, 8, 9);
    Force* normal = constructForce("n", fv, ft);

    CHECK(frictionForce(NULL, 0.3, dir) == NULL, "NULL normal NULL");
    CHECK(frictionForce(normal, 0.3, NULL) == NULL, "NULL dir NULL");
    CHECK(frictionForce(normal, -0.1, dir) == NULL, "negative mu NULL");

    // mu=0.3, |N|=10, motion +x -> friction = -3 in x
    Force* f = frictionForce(normal, 0.3, dir);
    CHECK(f != NULL, "allocated");
    CHECK(approx(getEntry(f->vector, 0, 0), -3.0, 1e-10), "opposes motion");
    CHECK(approx(getEntry(f->vector, 1, 0), 0.0, 1e-10), "y = 0");
    CHECK(matrixComp(f->tailPos, ft, 1e-10), "tailPos from normal");

    freeVector(f->vector); freeVector(f->tailPos); free(f);
    free(normal);
    freeVector(dir); freeVector(fv); freeVector(ft);
}

/* ---------- Conservation tests ---------- */

void testMomentum() {
    SECTION("momentum");
    CHECK(isnan(momentum(NULL)), "NULL body NAN");

    Vector* p = constructVector3(0, 0, 0);
    Vector* v = constructVector3(3, 4, 0);
    Body* b = constructBody(2.0, p, v);
    CHECK(approx(momentum(b), 10.0, 1e-10), "m=2, |v|=5 -> 10");

    free(b->forces); free(b);
    freeVector(p); freeVector(v);
}

void testKineticEnergy() {
    SECTION("kineticEnergy");
    CHECK(isnan(kineticEnergy(NULL)), "NULL body NAN");

    Vector* p = constructVector3(0, 0, 0);
    Vector* v = constructVector3(3, 4, 0);
    Body* b = constructBody(2.0, p, v);
    // KE = 0.5 * m * |v|^2 = 0.5 * 2 * 25 = 25
    CHECK(approx(kineticEnergy(b), 25.0, 1e-10), "m=2, |v|=5 -> KE=25");

    free(b->forces); free(b);
    freeVector(p); freeVector(v);
}

void testGravPotentialEnergy() {
    SECTION("gravPotentialEnergy");
    CHECK(isnan(gravPotentialEnergy(NULL, 10)), "NULL body NAN");

    Vector* p = constructVector3(0, 0, 0);
    Vector* v = constructVector3(0, 0, 0);
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
    Vector* fv = constructVector3(3, 0, 0);
    Vector* ft = constructVector3(0, 0, 0);
    Force* f = constructForce("f", fv, ft);
    Vector* disp = constructVector3(4, 0, 0);
    Vector* disp2 = constructVector(2);

    CHECK(isnan(work(NULL, disp)), "NULL F NAN");
    CHECK(isnan(work(f, NULL)), "NULL disp NAN");
    CHECK(isnan(work(f, disp2)), "dim mismatch NAN");
    freeVector(disp2);

    CHECK(approx(work(f, disp), 12.0, 1e-10), "parallel: F.disp = 12");

    Vector* perp = constructVector3(0, 4, 0);
    CHECK(approx(work(f, perp), 0.0, 1e-10), "perpendicular: 0");
    freeVector(perp);

    Vector* opp = constructVector3(-4, 0, 0);
    CHECK(approx(work(f, opp), -12.0, 1e-10), "antiparallel: -12");
    freeVector(opp);

    free(f);
    freeVector(fv); freeVector(ft); freeVector(disp);
}

void testPower() {
    SECTION("power");
    Vector* fv = constructVector3(3, 0, 0);
    Vector* ft = constructVector3(0, 0, 0);
    Force* f = constructForce("f", fv, ft);
    Vector* vel = constructVector3(2, 0, 0);
    Vector* vel2 = constructVector(2);

    CHECK(isnan(power(NULL, vel)), "NULL F NAN");
    CHECK(isnan(power(f, NULL)), "NULL vel NAN");
    CHECK(isnan(power(f, vel2)), "dim mismatch NAN");
    freeVector(vel2);

    CHECK(approx(power(f, vel), 6.0, 1e-10), "F.v = 6");

    Vector* perp = constructVector3(0, 5, 0);
    CHECK(approx(power(f, perp), 0.0, 1e-10), "perpendicular: 0");
    freeVector(perp);

    free(f);
    freeVector(fv); freeVector(ft); freeVector(vel);
}

void testImpulse() {
    SECTION("impulse");
    Vector* fv = constructVector3(3, 4, 0);  // |F| = 5
    Vector* ft = constructVector3(0, 0, 0);
    Force* f = constructForce("f", fv, ft);

    CHECK(isnan(impulse(NULL, 1)), "NULL F NAN");
    CHECK(isnan(impulse(f, -1)), "negative time NAN");

    CHECK(approx(impulse(f, 2), 10.0, 1e-10), "|F|*t = 10");
    CHECK(approx(impulse(f, 0), 0.0, 1e-10), "t=0 -> 0");

    free(f);
    freeVector(fv); freeVector(ft);
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

    printf("\n========================================\n");
    printf("Results: %d / %d tests passed\n", testsPassed, testsRun);
    printf("========================================\n");

    return (testsPassed == testsRun) ? 0 : 1;
}
