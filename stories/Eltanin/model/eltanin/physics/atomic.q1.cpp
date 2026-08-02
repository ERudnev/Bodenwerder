#include <eltanin/physics/atomic.q1.h>

#include "physics/horn.h"
#include "physics/system.h"

#include <vector>

namespace eltanin::phys {

    using namespace fqsm::api;
    using namespace rmmr;

    void Atomic::Actions::debugAddImpulse(Writing context, Id id, vec3 imp) {
        const auto& atomic = with<Atomic>::get(context, id);
        if (atomic.particles.empty()) {
            return (void)context.refuse("eltanin::phys::Atomic::debugAddImpulse: no particles");
        }
        const auto share = imp / static_cast<float>(atomic.particles.size());
        for (std::size_t i = 0; i < atomic.particles.size(); ++i) {
            auto particle = with<Particle>::modify(context, atomic.particles[i]);
            // Δv = j / m; Verlet v ≈ (current − prev) / dt  ⇒  prev ← prev − Δv·dt
            particle->prev -= (share / particle->mass) * Settings::fixedDtS;
        }
    }

    void Atomic::Actions::satisfy(Stewarding context) {
        auto particles = context.direct<Particle>();
        auto atomics = context.direct<Atomic>();
        std::vector<vec3> world_centered;
        std::vector<float> masses;
        for (auto [_, atomic] : atomics.items) {
            const auto& rest = atomic.rest;
            const auto count = atomic.particles.size();
            if (count == 0 or rest.centered.size() != count) {
                continue;
            }

            vec3 com{0.0f, 0.0f, 0.0f};
            float mass_sum = 0.0f;
            bool missing = false;
            world_centered.resize(count);
            masses.resize(count);
            for (std::size_t i = 0; i < count; ++i) {
                auto* particle = particles.items.find(atomic.particles[i]);
                if (not particle) {
                    missing = true;
                    break;
                }
                world_centered[i] = particle->current;
                masses[i] = particle->mass;
                com += particle->current * particle->mass;
                mass_sum += particle->mass;
            }
            if (missing or mass_sum <= 0.0f) {
                continue;
            }
            com /= mass_sum;
            for (std::size_t i = 0; i < count; ++i) {
                world_centered[i] -= com;
            }

            const quat rotation = horn::orientation(rest.centered, world_centered, masses);

            for (std::size_t i = 0; i < count; ++i) {
                const vec3 goal = com + rotation * rest.centered[i];
                auto& particle = particles.items.at(atomic.particles[i]);
                // prev untouched: Verlet velocity shifts with current.
                particle.current += Settings::constraintStiffness * (goal - particle.current);
            }

            // Origin of pose: com − R·rest.com.
            atomic.restored = Pose{
                .position = com - rotation * rest.com,
                .rotation = rotation,
            };
        }
    }

}
