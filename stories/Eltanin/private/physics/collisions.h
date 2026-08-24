#pragma once

#include <eltanin/physics/body.q1.h>
#include <eltanin/physics/rigid.q1.h>

#include <vector>

namespace eltanin::phys::collision {

    using namespace fqsm::api;
    using namespace rmmr;

    // Tick-local collision workspace (private physics, not Q1).
    // Built after predict / integrate; feeds constraint solve and Commit consequences.
    // Contact = positional constraint + event payload — not a force into accumulateForces.


    // One side of a candidate or contact. Ball and Crystal share Body::Id; type selects the primitive.
    struct Endpoint {
        enum class Type {
            crystal, // Hull triangle in Crystal.shape
            ball,
            // projectile — segment (prev→position); when the entity exists
        };
        Type type;
        Body::Id body;
        integer face; // Crystal hull face index; ignored for ball
    };

    // Broad-phase pair (order not significant until keyed).
    struct Candidate {
        Endpoint a;
        Endpoint b;
    };

    // Narrow-phase hit. Solver applies separation; Commit reads correction / speed for damage.
    struct Contact {
        Endpoint a;
        Endpoint b;
        vec3 point; // world
        vec3 normal; // unit, from a toward b (resolve by moving a along −normal / b along +normal)
        float penetration; // > 0 overlapping depth at build
        integer candidate; // index into State.candidates
        float correction; // accumulated normal separation this tick (m), written by solver
        float relativeNormalSpeed; // closing (+) along normal at build; Commit may re-sample
    };

    // Contiguous contact slice that shares a constraint island.
    struct Island {
        integer contactBegin;
        integer contactEnd; // half-open [begin, end) into State.contacts
    };

    // Per-tick buffer: clear or reuse at start of build; discard after Commit.
    struct State {
        vector<Candidate> candidates;
        vector<Contact> contacts;
        vector<Island> islands;
    };

}
