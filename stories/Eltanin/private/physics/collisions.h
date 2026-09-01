#pragma once

#include <eltanin/physics/body.q1.h>
#include <eltanin/physics/rigid.q1.h>

#include <vector>

namespace eltanin::phys::collision {

    using namespace fqsm::api;
    using namespace rmmr;

    // Tick-local collision workspace (private physics, not Q1).
    // Citizens are collision cohorts (Body::compound anchor + Solid/Crystal members). Broad: spheres in a uniform grid (cell = 2×maxR, 27-neighbour), then OBB. Isolates never leave the sphere pass.
    // Narrow: box/sphere SAT; solid vs frozen hull; Crystal vs Crystal is asymmetric — each side's live particles vs the other's pose*shape.
    // Each particle queries the rest-space hull BVH; depth is signed distance to that primitive (polygon slab or 2-vert capsule).
    // Solver once per tick: pull the tested point onto frozen pose*shape. Next iteration is the next physics tick.
    // Solid vs crystal: both queries detect; impulse is only solid-vs-shape — restitution+friction on the Solid (center.prev), kick face supports on Crystal.
    // Contact = positional constraint + event payload — not a force into accumulateForces.
    // Rays are not Occupants. After solve, traceRays() CCD-tests each Ray segment (prev→position) against Solids and Crystal hulls. Rays do not see each other.

    // One side of a candidate or contact. Solid and Crystal share Body::Id; type is sphere, box, or crystal.
    struct Endpoint {
        enum class Type {
            crystal, // Particle (tested point) or hull face (frozen shape), selected by `face`
            sphere,
            box,
            ray, // segment prev→position; own phase, not Occupant
        };
        Type type;
        Body::Id body;
        integer face; // Particle index when this side is the tested point; hull face index when it is the shape; ignored for sphere and box
    };

    // Broad-phase pair (order not significant until keyed).
    struct Candidate {
        Endpoint a;
        Endpoint b;
    };

    // Narrow-phase hit. Solver pulls the tested point onto frozen pose*shape once per tick.
    struct Contact {
        Endpoint a;
        Endpoint b;
        vec3 point; // world
        vec3 normal; // unit, from a toward b; solve moves only a along −normal (b is frozen pose*shape, except solid–solid)
        float penetration; // > 0 overlapping depth at build
        integer candidate; // index into State.candidates
        float correction; // normal separation this tick (m), written by solver
        float relativeNormalSpeed; // closing (+) along normal at build; Commit may re-sample
    };

    // Last connectivity tick. Observational; not a profiler.
    struct Census {
        integer crystals;
        integer solids;
        integer spheres;
        integer boxes;
        integer rays;
        integer compounds;
        integer cohorts;
        integer occupants;
        integer maxOccupants;
        integer particles;
        integer hullFaces;
        integer cohortPairs;
        integer cohortHits;
        integer obbHits;
        integer occupantTries;
        integer candidates;
        integer contacts;
        integer rayTries;
        integer rayHits;
    };

    // Per-tick buffer: clear or reuse at start of build; discard after Commit.
    struct State {
        vector<Candidate> candidates;
        vector<Contact> contacts;
        Census census;

        void build(Stewarding);
        void solve(Stewarding);
        void traceRays(Stewarding); // CCD segment vs frozen Solid / Crystal; one hit per ray per tick
    };

}
