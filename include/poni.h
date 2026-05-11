#ifndef PONI_H
#define PONI_H

#include<stdbool.h>
#include<stddef.h>
#include "hebi.h"
#include "sokko.h"

#define A_GRAVITY 9.80665L
#define A_BIG_G 6.67430e-11L
#define MAX_FORCES ((size_t)20)

/* --------- Definitions of structs ---------- */

typedef struct Force {
    char* name;
    Vector* vector;
    Vector* tailPos;
} Force;

typedef struct Body {
    long double mass;
    Vector* pos;
    Vector* velocity;
    Force** forces;
    size_t nForces;
} Body;

typedef struct BodySystem {
    Body** bodies;
    size_t nBodies;
} BodySystem;

typedef struct ProjectileInfo {
    long double range;
    long double peakHeight;
    long double timeOfFlight;
} ProjectileInfo;


/* ---------- Kinematics primitives ---------- */
long double* displacement(Vector* p1, Vector* p2);
long double* averageVelocity(Vector* p1, Vector* p2, long double time);
long double* averageAcceleration(Vector* p1, Vector* p2, Vector* initVel, long double time);
Vector* velocityAtTime(Vector* initVel, Vector* acceleration, long double time);
Vector* positionAtTime(Vector* initPos, Vector* initVel, Vector* acceleration, long double time);
Vector* speedAtPosition(Vector* initPos, Vector* initVel, Vector* acceleration, Vector* pos);
Vector* velocityAtPosition(Vector* initPos, Vector* initVel, Vector* acceleration, Vector* pos);
ProjectileInfo* getProjectileInfo(long double initVel, long double angle, long double initHeight);
long double centripetalAcceleration(long double vel, long double radius);
long double angularVelocity(long double vel, long double radius);

/* ---------- Dynamics ----------- */
Body* constructBody(long double mass, Vector* pos, Vector* velocity);
Force* constructForce(char* name, Vector* vector, Vector* tailPos);
void addForce(Body* body, Force* F);
void removeForce(Body* body, Force* F);
Vector* netForce(Body* body);
Vector* accelerationFromForce(Body* body);
Force* gravityForce(Body* body);
Force* normalForce(Body* body, Vector* surfaceNormal);
Force* frictionForce(Force* normal, long double mu, Vector* direction);
Force* springForce(Body* body, Vector* anchor, long double k, long double restLength);
Force* dragForce(Body* body, long double coeff);
Force* gravitationalForce(Body* body, Body* other);
Body* stepBody(Body* body, long double timeStep);
BodySystem* constructBodySystem(Body** bodies, size_t nBodies);
BodySystem* stepBodySystem(BodySystem* system, long double timeStep);
BodySystem* simulateBodySystem(BodySystem* system, long double timeStep, int steps);

/* ---------- Conservation quantities ----------- */
long double momentumMagnitude(Body* body);
Vector* momentumVector(Body* body);
long double kineticEnergy(Body* body);
long double gravPotentialEnergy(Body* body, long double height);
long double springPotentialEnergy(long double k, long double x);
Vector* totalMomentum(BodySystem* system);
long double totalEnergy(BodySystem* system);
long double work(Force* F, Vector* disp);
long double power(Force* F, Vector* vel);
long double impulseMagnitude(Force* F, long double time);
Vector* impulseVector(Force* F, long double time);

/* ---------- Collisions ---------- */
Vector* centerOfMass(Body** bodies, size_t nBodies);
Vector* centerOfMassVelocity(Body** bodies, size_t nBodies);
void elasticCollision1D(Body* b1, Body* b2);
void inelasticCollision1D(Body* b1, Body* b2);

/* ---------- Rotational dynamics ---------- */
long double momentOfInertiaPoint(long double m, long double r);
long double momentOfInertiaRod(long double m, long double L);
long double momentOfInertiaDisk(long double m, long double R);
long double parallelAxisTheorem(long double I_cm, long double m, long double d);
Vector* torque(Force* F, Vector* pivot);
Vector* angularMomentum(Body* body, Vector* pivot);
long double rotationalKineticEnergy(long double I, long double omega);
long double angularAccelerationFromTorque(long double netTorque, long double I);


#endif
