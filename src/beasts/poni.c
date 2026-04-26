#include<stdlib.h>
#include<stdio.h>
#include<math.h>
#include<string.h>
#include "hebi.h"
#include "poni.h"
#include "sokko.h"

/* PONI assumes every value is a real double. Sokko's MatrixElement is a
   tagged union over double/ComplexNumber, so we funnel everything through
   these two helpers: re() pulls the real component out of a MatrixElement,
   and R() lifts a double into one. */
static inline double re(MatrixElement e) { return e.value.real; }
static inline MatrixElement R(double x) { return elemFromReal(x); }

static char* poniDupstr(const char* s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char* copy = malloc(n + 1);
    if (!copy) return NULL;
    memcpy(copy, s, n + 1);
    return copy;
}

/* ---------- Kinematics primitives ---------- */

// Computes the displacement between two points
double* displacement(Vector* p1, Vector* p2) {
    if (!p1 || !p2) return NULL;
    if (p1->numRows != p2->numRows) return NULL;

    double* disp = (double*)malloc(p1->numRows * sizeof(double));
    if (!disp) return NULL;
    for (int i = 0; i < p1->numRows; i++) {
	    disp[i] = re(getEntry(p2, i, 0)) - re(getEntry(p1, i, 0));
    }
    return disp;
}

// Computes the average velocity between two points given a time interval
double* averageVelocity(Vector* p1, Vector* p2, double time) {
    if (!p1 || !p2 || time <= 0) return NULL;
    if (p1->numRows != p2->numRows) return NULL;

    double* velocity = (double*)malloc(p1->numRows * sizeof(double));
    if (!velocity) return NULL;
    for (int i = 0; i < p1->numRows; i++) {
	    velocity[i] = (re(getEntry(p2, i, 0)) - re(getEntry(p1, i, 0))) / time;
    }
    return velocity;
}

// Computes the average acceleration between two points given a time interval
double* averageAcceleration(Vector* p1, Vector* p2, Vector* initVel, double time) {
    if (!p1 || !p2 || !initVel || time <= 0) return NULL;
    if (p1->numRows != p2->numRows || p1->numRows != initVel->numRows) return NULL;

    double* acceleration = (double*)malloc(p1->numRows * sizeof(double));
    if (!acceleration) return NULL;

    for (int i = 0; i < p1->numRows; i++) {
	    acceleration[i] = 2 * (re(getEntry(p2, i, 0)) - re(getEntry(p1, i, 0)) - re(getEntry(initVel, i, 0)) * time) / (time * time);
    }

    return acceleration;
}

// Returns the velocity at time t given acceleration a and initial velocity initVel
Vector* velocityAtTime(Vector* initVel, Vector* acceleration, double time) {
    if (!initVel || !acceleration || time < 0) return NULL;
    if (initVel->numRows != acceleration->numRows) return NULL;

    Vector* velocity = constructVector(initVel->numRows);
    if (!velocity) return NULL;

    for (int i = 0; i < initVel->numRows; i++) {
	    setEntry(velocity, i, 0, R(re(getEntry(initVel, i, 0)) + re(getEntry(acceleration, i, 0)) * time));
    }

    return velocity;
}

// Returns the position at time t given acceleration a, initial velocity initVel, and initial position initPos
Vector* positionAtTime(Vector* initPos, Vector* initVel, Vector* acceleration, double time) {
    if (!initPos || !initVel || !acceleration || time < 0) return NULL;
    if (initPos->numRows != initVel->numRows || initPos->numRows != acceleration->numRows) return NULL;

    Vector* position = constructVector(initPos->numRows);
    if (!position) return NULL;

    for (int i = 0; i < initPos->numRows; i++) {
	    double pos = re(getEntry(initPos, i, 0)) + re(getEntry(initVel, i, 0)) * time + 0.5 * re(getEntry(acceleration, i, 0)) * time * time;
	    setEntry(position, i, 0, R(pos));
    }

    return position;
}

// Returns the speed at position pos given acceleration a, initial velocity initVel, and initial position initPos
Vector* speedAtPosition(Vector* initPos, Vector* initVel, Vector* acceleration, Vector* pos) {
    if (!initPos || !initVel || !acceleration || !pos) return NULL;
    if (initPos->numRows != initVel->numRows || initPos->numRows != acceleration->numRows || initPos->numRows != pos->numRows) return NULL;

    Vector* speed = constructVector(initPos->numRows);
    if (!speed) return NULL;

    for (int i = 0; i < initPos->numRows; i++) {
	    double deltaPos = re(getEntry(pos, i, 0)) - re(getEntry(initPos, i, 0));
	    double iv = re(getEntry(initVel, i, 0));
	    double a = re(getEntry(acceleration, i, 0));
	    double vel = sqrt(iv * iv + 2 * a * deltaPos);
	    setEntry(speed, i, 0, R(vel));
    }

    return speed;
}

// Returns the velocity at position pos given acceleration a, initial velocity initVel, and initial position initPos
Vector* velocityAtPosition(Vector* initPos, Vector* initVel, Vector* acceleration, Vector* pos) {
    if (!initPos || !initVel || !acceleration || !pos) return NULL;
    if (initPos->numRows != initVel->numRows || initPos->numRows != acceleration->numRows || initPos->numRows != pos->numRows) return NULL;

    Vector* velocity = constructVector(initPos->numRows);
    if (!velocity) return NULL;

    for (int i = 0; i < initPos->numRows; i++) {
	    double deltaPos = re(getEntry(pos, i, 0)) - re(getEntry(initPos, i, 0));
	    double iv = re(getEntry(initVel, i, 0));
        double a = re(getEntry(acceleration, i, 0));
        if (a == 0) {
            setEntry(velocity, i, 0, R(iv));
            continue;
        }

        double disc = iv * iv + 2*a*deltaPos;
        if (disc < 0) {
            freeVector(velocity);
            return NULL;
        }
        double sq = sqrt(disc);
        double t1 = (-iv + sq) / a;
        double t2 = (-iv - sq) / a;
        double t = (t1 >= 0 && (t2 < 0 || t1 < t2)) ? t1 : t2;
        if (t < 0) {
            freeVector(velocity);
            return NULL;
        }

        setEntry(velocity, i, 0, R(iv + a * t));
    }

    return velocity;
}

// Returns the range, peak heightm and flight time of a projectile given initial velocity initVel, launch angle angle, from initial height initHeight
ProjectileInfo* getProjectileInfo(double initVel, double angle, double initHeight) {
    if (initVel < 0 || angle < 0 || angle > 90) return NULL;

    const double degreesToRadians = 0.017453292519943295;
    double radAngle = angle * degreesToRadians;
    double timeOfFlight = (initVel * sin(radAngle) + sqrt(initVel * sin(radAngle) * initVel * sin(radAngle) + 2 * A_GRAVITY * initHeight)) / A_GRAVITY;
    double range = initVel * cos(radAngle) * timeOfFlight;
    double peakHeight = initHeight + (initVel * sin(radAngle)) * (initVel * sin(radAngle)) / (2 * A_GRAVITY);

    ProjectileInfo* results = malloc(sizeof(ProjectileInfo));
    if (!results) return NULL;
    results->range = range;
    results->peakHeight = peakHeight;
    results->timeOfFlight = timeOfFlight;

    return results;
}

// Returns the centripetal acceleration given velocity vel and radius of curvature radius
double centripetalAcceleration(double vel, double radius) {
    if (vel < 0 || radius <= 0) return NAN;
    return (vel * vel) / radius;
}

// Returns the angular velocity given linear velocity vel and radius of curvature radius
double angularVelocity(double vel, double radius) {
    if (vel < 0 || radius <= 0) return NAN;
    return vel / radius;
}

/* ---------- Dynamics ---------- */

// Construct a Body with a given mass, position, and initial velocity
Body* constructBody(double mass, Vector* pos, Vector* initVel) {
    if (mass <= 0 || !pos || !initVel) return NULL;
    Body* body = (Body*)malloc(sizeof(Body));
    if (!body) return NULL;

    body->mass = mass;
    body->pos = pos;
    body->velocity = initVel;
    body->nForces = 0;
    body->forces = malloc(MAX_FORCES * sizeof(Force*));
    if (!body->forces) {
	free(body);
	return NULL;
    }

    return body;
}

// Construct a Force
Force* constructForce(char* name, Vector* vector, Vector* tailPos) {
    if (!name || *name == '\0' || !vector || !tailPos) return NULL;

    Force* force = malloc(sizeof(Force));
    if (!force) return NULL;

    force->name = name;
    force->vector = vector;
    force->tailPos = tailPos;

    return force;
}

// Adds a force F to a Body
void addForce(Body* body, Force* F) {
    if (!body || !F) return;

    // Add the force to the Body's array of forces
    if (body->nForces < MAX_FORCES) {
	body->forces[body->nForces] = F;
	body->nForces++;
    } else {
	return;
    }
}

// Removes a force F from a body
void removeForce(Body* body, Force* F) {
    if (!body || !F) return;

    for (int i = 0; i < body->nForces; i++) {
	if (body->forces[i] == F) {
	    for (int j = i; j < body->nForces - 1; j++) {
		body->forces[j] = body->forces[j + 1];
	    }
	    body->nForces--;
	    return;
	}
    }
}

// Computes the net force on a body by summing all forces in its forces array
Vector* netForce(Body* body) {
    if (!body) return NULL;
    if (body->nForces == 0) {
	    Vector* zeroForce = constructVector(body->pos->numRows);
	    if (!zeroForce) return NULL;
	    for (int i = 0; i < zeroForce->numRows; i++) {
	        setEntry(zeroForce, i, 0, R(0));
	    }
	    return zeroForce;
    }

    Vector* net = constructVector(body->pos->numRows);
    if (!net) return NULL;
    for (int i = 0; i < body->nForces; i++) {
        Vector* tmp = addVectors(net, body->forces[i]->vector);
        if (!tmp) {
            free(net);
            return NULL;
        }
        free(net);
        net = tmp;
    }

    return net;
}

// Computes the acceleration of a Body given the net force acting upon it
Vector* accelerationFromForce(Body* body) {
    if (!body) return NULL;

    Vector* net = netForce(body);
    Vector* a = scaleVector(net, R(1.0 / body->mass));
    freeVector(net);
    return a;
}

// Construct the gravitational force acting on a body (points in -y)
Force* gravityForce(Body* body) {
    if (!body || !body->pos) return NULL;
    if (body->pos->numRows < 2) return NULL;

    Vector* gVec = constructVector(body->pos->numRows);
    if (!gVec) return NULL;
    setEntry(gVec, 1, 0, R(-body->mass * A_GRAVITY));

    Vector* tail = copyMatrix(body->pos);
    if (!tail) {
        freeVector(gVec);
        return NULL;
    }

    return constructForce("gravity", gVec, tail);
}

// Construct the normal force on a body resting on a surface with outward normal surfaceNormal
Force* normalForce(Body* body, Vector* surfaceNormal) {
    if (!body || !surfaceNormal || !body->pos) return NULL;
    if (body->pos->numRows != surfaceNormal->numRows) return NULL;
    if (body->pos->numRows < 2) return NULL;

    Vector* nHat = normalizeVector(surfaceNormal);
    if (!nHat) return NULL;

    Vector* weight = constructVector(body->pos->numRows);
    if (!weight) {
        freeVector(nHat);
        return NULL;
    }
    setEntry(weight, 1, 0, R(-body->mass * A_GRAVITY));

    double proj = re(vectorDotProduct(weight, nHat));
    freeVector(weight);

    Vector* forceVec = scaleVector(nHat, R(-proj));
    freeVector(nHat);
    if (!forceVec) return NULL;

    Vector* tail = copyMatrix(body->pos);
    if (!tail) {
        freeVector(forceVec);
        return NULL;
    }

    return constructForce("normal", forceVec, tail);
}

// Construct the force of friction (kinetic or static)
Force* frictionForce(Force* normal, double mu, Vector* direction) {
    if (!normal || !direction || mu < 0) return NULL;

    Vector* neg = negativeVector(direction);
    if (!neg) return NULL;
    Vector* unitDir = normalizeVector(neg);
    freeVector(neg);
    if (!unitDir) return NULL;

    double mag = mu * l2Norm(normal->vector);
    Vector* vec = scaleVector(unitDir, R(mag));
    freeVector(unitDir);
    if (!vec) return NULL;

    Vector* tail = copyMatrix(normal->tailPos);
    if (!tail) {
        freeVector(vec);
        return NULL;
    }

    return constructForce("friction", vec, tail);
}

Force* springForce(Body* body, Vector* anchor, double k, double restLength) {
    if (!body || !anchor || !body->pos) return NULL;
    if (k < 0 || restLength < 0) return NULL;
    if (body->pos->numRows != anchor->numRows) return NULL;

    Vector* displacement = subtractVectors(body->pos, anchor);
    if (!displacement) return NULL;

    double distance = l2Norm(displacement);
    Vector* forceVec = constructVector(body->pos->numRows);
    if (!forceVec) {
        freeVector(displacement);
        return NULL;
    }

    if (distance > 0) {
        double magnitude = -k * (distance - restLength);
        for (int i = 0; i < forceVec->numRows; i++) {
            double component = re(getEntry(displacement, i, 0)) / distance;
            setEntry(forceVec, i, 0, R(magnitude * component));
        }
    } else {
        for (int i = 0; i < forceVec->numRows; i++) setEntry(forceVec, i, 0, R(0));
    }

    freeVector(displacement);

    Vector* tail = copyMatrix(body->pos);
    if (!tail) {
        freeVector(forceVec);
        return NULL;
    }

    return constructForce(poniDupstr("spring"), forceVec, tail);
}

Force* dragForce(Body* body, double coeff) {
    if (!body || !body->velocity || !body->pos || coeff < 0) return NULL;

    Vector* forceVec = constructVector(body->velocity->numRows);
    if (!forceVec) return NULL;
    for (int i = 0; i < forceVec->numRows; i++) {
        setEntry(forceVec, i, 0, R(-coeff * re(getEntry(body->velocity, i, 0))));
    }

    Vector* tail = copyMatrix(body->pos);
    if (!tail) {
        freeVector(forceVec);
        return NULL;
    }

    return constructForce(poniDupstr("drag"), forceVec, tail);
}

Force* gravitationalForce(Body* body, Body* other) {
    if (!body || !other || !body->pos || !other->pos) return NULL;
    if (body->pos->numRows != other->pos->numRows) return NULL;

    Vector* displacement = subtractVectors(other->pos, body->pos);
    if (!displacement) return NULL;
    double distance = l2Norm(displacement);
    if (distance == 0) {
        freeVector(displacement);
        return NULL;
    }

    double magnitude = A_BIG_G * body->mass * other->mass / (distance * distance * distance);
    Vector* forceVec = constructVector(displacement->numRows);
    if (!forceVec) {
        freeVector(displacement);
        return NULL;
    }
    for (int i = 0; i < forceVec->numRows; i++) {
        setEntry(forceVec, i, 0, R(magnitude * re(getEntry(displacement, i, 0))));
    }

    freeVector(displacement);

    Vector* tail = copyMatrix(body->pos);
    if (!tail) {
        freeVector(forceVec);
        return NULL;
    }

    return constructForce(poniDupstr("gravitation"), forceVec, tail);
}

Body* stepBody(Body* body, double timeStep) {
    if (!body || !body->pos || !body->velocity || timeStep < 0) return NULL;

    Vector* acceleration = accelerationFromForce(body);
    if (!acceleration) return NULL;
    if (acceleration->numRows != body->pos->numRows || acceleration->numRows != body->velocity->numRows) {
        freeVector(acceleration);
        return NULL;
    }

    for (int i = 0; i < body->pos->numRows; i++) {
        double pos = re(getEntry(body->pos, i, 0));
        double vel = re(getEntry(body->velocity, i, 0));
        double accel = re(getEntry(acceleration, i, 0));
        setEntry(body->pos, i, 0, R(pos + vel * timeStep + 0.5 * accel * timeStep * timeStep));
        setEntry(body->velocity, i, 0, R(vel + accel * timeStep));
    }

    freeVector(acceleration);
    return body;
}

BodySystem* constructBodySystem(Body** bodies, int nBodies) {
    if (!bodies || nBodies < 1) return NULL;

    BodySystem* system = malloc(sizeof(BodySystem));
    if (!system) return NULL;
    system->bodies = malloc((size_t)nBodies * sizeof(Body*));
    if (!system->bodies) {
        free(system);
        return NULL;
    }

    system->nBodies = nBodies;
    for (int i = 0; i < nBodies; i++) {
        if (!bodies[i]) {
            free(system->bodies);
            free(system);
            return NULL;
        }
        system->bodies[i] = bodies[i];
    }

    return system;
}

BodySystem* stepBodySystem(BodySystem* system, double timeStep) {
    if (!system || timeStep < 0) return NULL;
    for (int i = 0; i < system->nBodies; i++) {
        if (!stepBody(system->bodies[i], timeStep)) return NULL;
    }
    return system;
}

BodySystem* simulateBodySystem(BodySystem* system, double timeStep, int steps) {
    if (!system || timeStep < 0 || steps < 0) return NULL;
    for (int i = 0; i < steps; i++) {
        if (!stepBodySystem(system, timeStep)) return NULL;
    }
    return system;
}

/* ---------- Conservation quantities ---------- */

// Compute the magnitude of the momentum of a body
double momentumMagnitude(Body* body) {
    if (!body) return NAN;

    return body->mass * l2Norm(body->velocity);
}

// Compute the momentum vector of a body
Vector* momentumVector(Body* body) {
    if (!body) return NULL;

    int dim = body->velocity->numRows;
    double mass = body->mass;
    Vector* mom = constructVector(dim);
    for (int i = 0; i < dim; i++) {
        setEntry(mom, i, 0, R(mass * re(getEntry(body->velocity, i, 0))));
    }

    return mom;
}

// Compute the kinetic energy of a body
double kineticEnergy(Body* body) {
    if (!body) return NAN;

    double velMag = l2Norm(body->velocity);
    return 0.5 * body->mass * velMag * velMag;
}

// Compute the gravitational potential energy of a body at a certain height
double gravPotentialEnergy(Body* body, double height) {
    if (!body) return NAN;
    if (height == 0) return 0;

    return body->mass * height * A_GRAVITY;
}

// Compute the potential energy of a body in a spring with spring constant k and compression x
double springPotentialEnergy(double k, double x) {
    if (k < 0 || x < 0) return NAN;
    return 0.5 * k * x * x;
}

Vector* totalMomentum(BodySystem* system) {
    if (!system || system->nBodies < 1 || !system->bodies || !system->bodies[0]) return NULL;

    int dim = system->bodies[0]->velocity->numRows;
    Vector* total = constructVector(dim);
    if (!total) return NULL;
    for (int j = 0; j < dim; j++) setEntry(total, j, 0, R(0));

    for (int i = 0; i < system->nBodies; i++) {
        Body* body = system->bodies[i];
        if (!body || !body->velocity || body->velocity->numRows != dim) {
            freeVector(total);
            return NULL;
        }
        for (int j = 0; j < dim; j++) {
            double prev = re(getEntry(total, j, 0));
            double add = body->mass * re(getEntry(body->velocity, j, 0));
            setEntry(total, j, 0, R(prev + add));
        }
    }

    return total;
}

double totalEnergy(BodySystem* system) {
    if (!system || !system->bodies) return NAN;

    double total = 0;
    for (int i = 0; i < system->nBodies; i++) {
        Body* body = system->bodies[i];
        if (!body) return NAN;
        total += kineticEnergy(body);
        if (body->pos && body->pos->numRows >= 2) {
            total += gravPotentialEnergy(body, re(getEntry(body->pos, 1, 0)));
        }
    }

    return total;
}

// Compute the work done on an object by a force over a distance
double work(Force* F, Vector* disp) {
    if (!F || !disp) return NAN;
    if (F->vector->numRows != disp->numRows) return NAN;

    return re(vectorDotProduct(F->vector, disp));
}

// Compute the power delivered by a force F at velocity v
double power(Force* F, Vector* velocity) {
    if (!F || !velocity) return NAN;
    if (F->vector->numRows != velocity->numRows) return NAN;

    return re(vectorDotProduct(F->vector, velocity));
}

// Compute the magnitude of the impulse delivered by a force F over a time interval time
double impulseMagnitude(Force* F, double time) {
    if (!F || time < 0) return NAN;
    return l2Norm(F->vector) * time;
}

// Compute the impulse vector of a body
Vector* impulseVector(Force* F, double time) {
    if (!F) return NULL;

    int dim = F->vector->numRows;
    Vector* imp = constructVector(dim);
    for (int i = 0; i < dim; i++) {
        setEntry(imp, i, 0, R(time * re(getEntry(F->vector, i, 0))));
    }

    return imp;
}

/* ---------- Collisions ---------- */

// Find the center of mass of a system of bodies
Vector* centerOfMass(Body** bodies, int nBodies) {
    if (!bodies || nBodies <= 0) return NULL;
    if (!bodies[0] || !bodies[0]->pos) return NULL;

    int dim = bodies[0]->pos->numRows;
    Vector* com = constructVector(dim);
    if (!com) return NULL;
    for (int j = 0; j < dim; j++) setEntry(com, j, 0, R(0));
    double totalMass = 0;

    for (int i = 0; i < nBodies; i++) {
        if (!bodies[i] || bodies[i]->pos->numRows != dim) {
            freeVector(com);
            return NULL;
        }
        double mass = bodies[i]->mass;
        totalMass += mass;
        for (int j = 0; j < dim; j++) {
            double prev = re(getEntry(com, j, 0));
            setEntry(com, j, 0, R(prev + mass * re(getEntry(bodies[i]->pos, j, 0))));
        }
    }

    for (int j = 0; j < dim; j++) {
        double prev = re(getEntry(com, j, 0));
        setEntry(com, j, 0, R(prev / totalMass));
    }

    return com;
}

// Find the velocity of the center of mass of a system of bodies by taking the mass-weighted average of their velocities
Vector* centerOfMassVelocity(Body** bodies, int nBodies) {
    if (!bodies || nBodies <= 0) return NULL;
    if (!bodies[0] || !bodies[0]->velocity) return NULL;

    int dim = bodies[0]->velocity->numRows;
    Vector* comVel = constructVector(dim);
    if (!comVel) return NULL;
    for (int j = 0; j < dim; j++) setEntry(comVel, j, 0, R(0));
    double totalMass = 0;

    for (int i = 0; i < nBodies; i++) {
        if (!bodies[i] || bodies[i]->velocity->numRows != dim) {
            freeVector(comVel);
            return NULL;
        }
        double mass = bodies[i]->mass;
        totalMass += mass;
        for (int j = 0; j < dim; j++) {
            double prev = re(getEntry(comVel, j, 0));
            setEntry(comVel, j, 0, R(prev + mass * re(getEntry(bodies[i]->velocity, j, 0))));
        }
    }

    for (int j = 0; j < dim; j++) {
        double prev = re(getEntry(comVel, j, 0));
        setEntry(comVel, j, 0, R(prev / totalMass));
    }

    return comVel;
}

// Handle an elastic collision between two bodies and update them with the resulting properties
void elasticCollision1D(Body* b1, Body* b2) {
    if (!b1 || !b2) return;

    double m1 = b1->mass;
    double m2 = b2->mass;
    double v1 = re(getEntry(b1->velocity, 0, 0));
    double v2 = re(getEntry(b2->velocity, 0, 0));

    double newV1 = (v1 * (m1 - m2) + 2 * m2 * v2) / (m1 + m2);
    double newV2 = (v2 * (m2 - m1) + 2 * m1 * v1) / (m1 + m2);

    setEntry(b1->velocity, 0, 0, R(newV1));
    setEntry(b2->velocity, 0, 0, R(newV2));
}

// Handle an inelastic collision between two bodies and update them with the resulting properties (they stick together and move with the same velocity)
void inelasticCollision1D(Body* b1, Body* b2) {
    if (!b1 || !b2) return;

    double m1 = b1->mass;
    double m2 = b2->mass;
    double v1 = re(getEntry(b1->velocity, 0, 0));
    double v2 = re(getEntry(b2->velocity, 0, 0));

    double newV = (m1 * v1 + m2 * v2) / (m1 + m2);

    setEntry(b1->velocity, 0, 0, R(newV));
    setEntry(b2->velocity, 0, 0, R(newV));
}

/* ---------- Rotational dynamics ---------- */

// Get the moment of inertia of a point mass at distance r from the axis
double momentOfInertiaPoint(double m, double r) {
    if (m < 0 || r < 0) return NAN;
    return m * r * r;
}

// Get the moment of inertia of a rod of length L about its center of mass
double momentOfInertiaRod(double m, double L) {
    if (m < 0 || L < 0) return NAN;
    return (1.0 / 12.0) * m * L * L;
}

// Get the moment of inertia of a uniform disk of radius R about its central axis
double momentOfInertiaDisk(double m, double R) {
    if (m < 0 || R < 0) return NAN;
    return 0.5 * m * R * R;
}

// Apply the parallel axis theorem to find the moment of inertia
double parallelAxisTheorem(double I_cm, double m, double d) {
    if (I_cm < 0 || m < 0) return NAN;
    return I_cm + m * d * d;
}

// Get the torque applied by a force about a pivot. In 2D returns a 1-vector (scalar z-component);
// in 3D returns the full r x F cross product.
Vector* torque(Force* F, Vector* pivot) {
    if (!F || !pivot) return NULL;
    if (F->vector->numRows != pivot->numRows) return NULL;

    int dim = F->vector->numRows;
    if (dim != 2 && dim != 3) return NULL;

    Vector* r = subtractVectors(F->tailPos, pivot);
    if (!r) return NULL;

    if (dim == 2) {
        double rx = re(getEntry(r, 0, 0));
        double ry = re(getEntry(r, 1, 0));
        double fx = re(getEntry(F->vector, 0, 0));
        double fy = re(getEntry(F->vector, 1, 0));
        freeVector(r);
        Vector* t = constructVector(1);
        if (!t) return NULL;
        setEntry(t, 0, 0, R(rx * fy - ry * fx));
        return t;
    }

    Vector* t = crossProduct(r, F->vector);
    freeVector(r);
    return t;
}

// Get the angular momentum of a Body about a pivot (L = r x p).
// In 2D returns a 1-vector (scalar z-component); in 3D returns the full cross product.
Vector* angularMomentum(Body* body, Vector* pivot) {
    if (!body || !pivot) return NULL;
    if (!body->pos || !body->velocity) return NULL;
    if (body->pos->numRows != pivot->numRows) return NULL;

    int dim = body->pos->numRows;
    if (dim != 2 && dim != 3) return NULL;

    Vector* r = subtractVectors(body->pos, pivot);
    if (!r) return NULL;
    Vector* p = momentumVector(body);
    if (!p) {
        freeVector(r);
        return NULL;
    }

    if (dim == 2) {
        double rx = re(getEntry(r, 0, 0));
        double ry = re(getEntry(r, 1, 0));
        double px = re(getEntry(p, 0, 0));
        double py = re(getEntry(p, 1, 0));
        freeVector(r);
        freeVector(p);
        Vector* L = constructVector(1);
        if (!L) return NULL;
        setEntry(L, 0, 0, R(rx * py - ry * px));
        return L;
    }

    Vector* L = crossProduct(r, p);
    freeVector(r);
    freeVector(p);
    return L;
}

// Get the rotational kinetic energy given the moment of inertia and angular velocity
double rotationalKineticEnergy(double I, double omega) {
    if (I < 0) return NAN;
    return 0.5 * I * omega * omega;
}

// Get the angular acceleration about a fixed axis given the net torque and moment of inertia
double angularAccelerationFromTorque(double netTorque, double I) {
    if (I <= 0) return NAN;
    return netTorque / I;
}
