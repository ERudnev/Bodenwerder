#pragma once

#include <eltanin/physics/body.q1.h>
#include <eltanin/physics/rigid.q1.h>
#include <eltanin/physics/resting.q1.h>

#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace eltanin::phys::collision {

    using namespace fqsm::api;
    using namespace rmmr;

    // Tick-local collision workspace (private physics, not Q1).
    // Citizens are collision cohorts (Body::compound anchor + Solid/Crystal members). Broad: spheres in a uniform grid (cell = 2×maxR, 27-neighbour), then OBB. Isolates never leave the sphere pass.
    // Narrow: box/sphere SAT; solid vs frozen hull; Crystal vs Crystal is asymmetric — each side's live particles vs the other's pose*shape.
    // Each particle queries the rest-space hull BVH; depth is signed distance to that primitive (polygon slab or 2-vert capsule).
    // Solver once per tick: one-sided hull faces place the tested point on the closest surface point (shortest exit) and drop closing Verlet speed (rest also drops tangent); two-sided plates still separate along −normal×depth.
    // Solid vs crystal: both queries detect; impulse is only solid-vs-shape — restitution+friction on the Solid (center.prev), semiKick face supports on Crystal.
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

    // Narrow-phase hit. One-sided: tested point → closest surface point. Two-sided: separate a along −normal×depth.
    struct Contact {
        Endpoint a;
        Endpoint b;
        vec3 point; // world closest on frozen shape
        vec3 normal; // unit, from a toward b (shape outward ≈ −normal for one-sided solids)
        float penetration; // > 0 overlapping depth at build
        integer candidate; // index into State.candidates
        float correction; // separation this tick (m), written by solver
        float relativeNormalSpeed; // closing (+) along normal at build; Commit may re-sample
    };

    struct PairKey {
        Body::Id first;
        Body::Id second;

        auto operator==(const PairKey&) const -> bool = default;
    };

    struct PairKeyHash {
        auto operator()(const PairKey& pair) const -> std::size_t {
            return std::hash<Body::Id>{}(pair.first) ^ (std::hash<Body::Id>{}(pair.second) + 0x9e3779b97f4a7c15ull + (std::hash<Body::Id>{}(pair.first) << 6) + (std::hash<Body::Id>{}(pair.first) >> 2));
        }
    };

    // Cross-tick capture only. Established pairs are phys::Resting world entities.
    // Local contacts, not originOffset: Body origins are a lever of the larger radius, so Horn jitter of 0.01 rad looks like meters.
    struct RestProbe {
        vec3 localFirst;
        vec3 localSecond;
        quat relativeOrientation;
        integer firstShape;
        integer secondShape;
        seconds stable;
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
        integer restProbes;
        seconds restProbeStable;
        integer restingPairs;
        integer restingIslands;
        integer restingSkipped;
        integer rayTries;
        integer rayHits;
    };

    // Tick-local contacts plus capture probes. Established resting pairs are world entities.
    struct State {
        vector<Candidate> candidates;
        vector<Contact> contacts;
        Census census;
        std::unordered_map<PairKey, RestProbe, PairKeyHash> probes;
        std::unordered_set<PairKey, PairKeyHash> activeResting;

        void build(Stewarding);
        void solve(Stewarding);
        void traceRays(Stewarding); // CCD segment vs frozen Solid / Crystal; one hit per ray per tick
    };

}
