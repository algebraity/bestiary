#ifndef PONI_H
#define PONI_H

#include<stdbool.h>
#include "hebi.h"
#include "sokko.h"

#define A_GRAVITY 9.80665
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


/* ---------- Kinematics primitives ---------- */
double* displacement(Vector* p1, Vector* p2);
double* averageVelocity(Vector* p1, Vector* p2, double time);
double* averageAcceleration(Vector* p1, Vector* p2, Vector* initVel, double time);
Vector* velocityAtTime(Vector* initVel, Vector* acceleration, double time);
Vector* positionAtTime(Vector* initPos, Vector* initVel, Vector* acceleration, double time);
Vector* speedAtPosition(Vector* initPos, Vector* initVel, Vector* acceleration, Vector* pos);
Vector* velocityAtPosition(Vector* initPos, Vector* initVel, Vector* acceleration, Vector* pos);
double* projectileInfo(double initVel, double angle, double initHeight);
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

/* ---------- Conservation quantities ----------- */
double momentum(Body* body);
double kineticEnergy(Body* body);
double gravPotentialEnergy(Body* body, double height);
double springPotentialEnergy(double k, double x);
double work(Force* F, Vector* disp);
double power(Force* F, Vector* vel);
double impulse(Force* F, double time);


#endif
