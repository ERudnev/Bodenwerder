#pragma once

#include <eltanin/physics/body.q1.h>
#include <eltanin/physics/compound.q1.h>
#include <eltanin/physics/rigid.q1.h>

#include <vector>

namespace eltanin::phys::collision {

    using namespace fqsm::api;
    using namespace rmmr;

    // Tick-local collision workspace (private physics, not Q1).
    // First-class citizens are Compounds; global sweep is compound-vs-compound.
    // Narrow phase: live particles of one body vs pose*shape of the other.
    // Each particle queries the rest-space hull BVH for the nearest face; depth is signed distance to that primitive (polygon slab or 2-vert capsule).
    // Solver once per tick: pull the tested point onto frozen pose*shape. Next iteration is the next physics tick.
    // Ball vs crystal: both queries detect; impulse is only ball-vs-shape — separate, restitution+friction on the Ball (center.prev), kick face supports on Crystal.
    // Contact = positional constraint + event payload — not a force into accumulateForces.

    // One side of a candidate or contact. Ball and Crystal share Body::Id; type selects the primitive.
    struct Endpoint {
        enum class Type {
            crystal, // Particle (tested point) or hull face (frozen shape), selected by `face`
            ball,
            // projectile — segment (prev→position); when the entity exists
        };
        Type type;
        Body::Id body;
        integer face; // Particle index when this side is the tested point; hull face index when it is the shape; ignored for ball
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
        vec3 normal; // unit, from a toward b; solve moves only a along −normal (b is frozen pose*shape, except ball–ball)
        float penetration; // > 0 overlapping depth at build
        integer candidate; // index into State.candidates
        float correction; // normal separation this tick (m), written by solver
        float relativeNormalSpeed; // closing (+) along normal at build; Commit may re-sample
    };

    // Per-tick buffer: clear or reuse at start of build; discard after Commit.
    struct State {
        vector<Candidate> candidates;
        vector<Contact> contacts;

        void build(Stewarding);
        void solve(Stewarding);
    };

}
