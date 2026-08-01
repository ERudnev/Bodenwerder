#include <eltanin/physics/atomic.q1.h>

#include "physics/system.h"

namespace eltanin::phys {

    using namespace fqsm::api;

    void Atomic::Actions::debugAddImpulse(Writing context, Id id, vec3 imp) {
        const auto& atomic = with<Atomic>::get(context, id);
        if (atomic.particles.empty()) {
            return (void)context.refuse("eltanin::phys::Atomic::debugAddImpulse: no particles");
        }
        const auto share = imp / static_cast<float>(atomic.particles.size());
        for (const auto particle_id : atomic.particles) {
            auto particle = with<Particle>::modify(context, particle_id);
            // Δv = j / m; Verlet v ≈ (current − prev) / dt  ⇒  prev ← prev − Δv·dt
            particle->prev -= (share / particle->mass) * Settings::fixedDtS;
        }
    }

}
