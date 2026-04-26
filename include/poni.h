#ifndef PONI_H
#define PONI_H

#include<stdbool.h>
#include "hebi.h"
#include "sokko.h"

#define A_GRAVITY 9.80665
#define A_BIG_G 6.67430e-11
#define MAX_FORCES 20

/* --------- Definitions of structs ---------- */

typedef struct Force {
    char* name;
    Vector* vector;
    Vector* tailPos;
} Force;

typedef struct Body {
    double mass;
    Vector* pos;
    Vector* velocity;
    Force** forces;
    int nForces;
} Body;

typedef struct BodySystem {
    Body** bodies;
    int nBodies;
} BodySystem;

typedef struct ProjectileInfo {
    double range;
    double peakHeight;
    double timeOfFlight;
} ProjectileInfo;


/* ---------- Kinematics primitives ---------- */
double* displacement(Vector* p1, Vector* p2);
double* averageVelocity(Vector* p1, Vector* p2, double time);
double* averageAcceleration(Vector* p1, Vector* p2, Vector* initVel, double time);
Vector* velocityAtTime(Vector* initVel, Vector* acceleration, double time);
Vector* positionAtTime(Vector* initPos, Vector* initVel, Vector* acceleration, double time);
Vector* speedAtPosition(Vector* initPos, Vector* initVel, Vector* acceleration, Vector* pos);
Vector* velocityAtPosition(Vector* initPos, Vector* initVel, Vector* acceleration, Vector* pos);
ProjectileInfo* getProjectileInfo(double initVel, double angle, double initHeight);
double centripetalAcceleration(double vel, double radius);
double angularVelocity(double vel, double radius);

/* ---------- Dynamics ----------- */
Body* constructBody(double mass, Vector* pos, Vector* velocity);
Force* constructForce(char* name, Vector* vector, Vector* tailPos);
void addForce(Body* body, Force* F);
void removeForce(Body* body, Force* F);
Vector* netForce(Body* body);
Vector* accelerationFromForce(Body* body);
Force* gravityForce(Body* body);
Force* normalForce(Body* body, Vector* surfaceNormal);
Force* frictionForce(Force* normal, double mu, Vector* direction);
Force* springForce(Body* body, Vector* anchor, double k, double restLength);
Force* dragForce(Body* body, double coeff);
Force* gravitationalForce(Body* body, Body* other);
Body* stepBody(Body* body, double timeStep);
BodySystem* constructBodySystem(Body** bodies, int nBodies);
BodySystem* stepBodySystem(BodySystem* system, double timeStep);
BodySystem* simulateBodySystem(BodySystem* system, double timeStep, int steps);

/* ---------- Conservation quantities ----------- */
double momentumMagnitude(Body* body);
Vector* momentumVector(Body* body);
double kineticEnergy(Body* body);
double gravPotentialEnergy(Body* body, double height);
double springPotentialEnergy(double k, double x);
Vector* totalMomentum(BodySystem* system);
double totalEnergy(BodySystem* system);
double work(Force* F, Vector* disp);
double power(Force* F, Vector* vel);
double impulseMagnitude(Force* F, double time);
Vector* impulseVector(Force* F, double time);

/* ---------- Collisions ---------- */
Vector* centerOfMass(Body** bodies, int nBodies);
Vector* centerOfMassVelocity(Body** bodies, int nBodies);
void elasticCollision1D(Body* b1, Body* b2);
void inelasticCollision1D(Body* b1, Body* b2);

/* ---------- Rotational dynamics ---------- */
double momentOfInertiaPoint(double m, double r);
double momentOfInertiaRod(double m, double L);
double momentOfInertiaDisk(double m, double R);
double parallelAxisTheorem(double I_cm, double m, double d);
Vector* torque(Force* F, Vector* pivot);
Vector* angularMomentum(Body* body, Vector* pivot);
double rotationalKineticEnergy(double I, double omega);
double angularAccelerationFromTorque(double netTorque, double I);


#endif
