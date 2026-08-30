#pragma once

#include "physics/collisions.h"
#include "physics/settings.h"

#include <eltanin/physics/body.q1.h>
#include <eltanin/physics/rigid.q1.h>
#include <rmmr/scene/root.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin::phys {

    using namespace fqsm::api;
    using namespace rmmr;

    // Private physics subsystem (not Q1).
    // Fixed tick Settings::fixedStep; frame dt accumulates as seconds debt. Same clock as Thing.now.
    // One pass: accumulate forces → Verlet → Horn (query pose) → connectivity → Horn (absorb kicks) → pull to shape.
    // After the last tick of this Dock: Thing::followBodies copies Body pose onto Node (missing ward → skip).
    // One Dock per tick; hot mutation via Stewarding::direct<Body>() and direct<rigid::Crystal>() / direct<rigid::Solid>().
    // Orientation: Horn unit-quaternion method (symmetric N 4×4 + Jacobi), see physics/horn.h.
    // Thermal: Boulder. Rock Volume thermal deferred. Accumulate to thermalStep, then radiate. No conduction.

    struct System {
        scene::Root::Id scene;

        System(scene::Root::Id);
        void step(establish::Realm&, seconds dt);
        auto collisionCensus() const -> const collision::Census& { return collisions.census; }

    private:
        seconds debt;
        seconds thermalDebt;
        collision::State collisions;

        void tick(Stewarding);
        void accumulateForces(Stewarding);
        void applyAerodynamics(Stewarding);
        void applyLinearGravity(Stewarding);
        void integrate(fqsm::Direct<rigid::Crystal>);
        void integrateSolids(fqsm::Direct<Body>, fqsm::Direct<rigid::Solid>);
        void integrateRays(fqsm::Direct<Body>, fqsm::Direct<rigid::Ray>);
        void restoreBases(Stewarding);
        void applyConnectivity(Stewarding);
        void applyConstraintWishes(Stewarding);
        void radiate(Stewarding);
    };

}
