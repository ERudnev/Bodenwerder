#include <eltanin/physics/strong.q1.h>

#include "physics/system.h"

#include <glm/geometric.hpp>

namespace eltanin::phys::strong {

    using namespace fqsm::api;

    auto Nail::Actions::pin(Writing context, Particle::Id particle_id) -> Id {
        if (not with<Particle>::exists(context, particle_id)) {
            return context.refuse("eltanin::phys::strong::Nail::pin: Particle missing");
        }
        const auto& particle = with<Particle>::get(context, particle_id);
        return create(context, Quantum{
            .particle = particle_id,
            .point = particle.current,
        });
    }

    void Nail::Actions::satisfy(Stewarding context) {
        auto particles = context.direct<Particle>();
        auto nails = context.direct<Nail>();
        for (auto [id, nail] : nails.items) {
            auto* particle = particles.items.find(nail.particle);
            if (not particle) {
                with<Nail>::remove(context, id);
                continue;
            }
            // prev untouched: Verlet velocity shifts with current (same as Horn).
            particle->current += Settings::constraintStiffness * (nail.point - particle->current);
        }
    }

    auto Gluon::Actions::clue(Writing context, Particle::Id seed_id) -> Id {
        if (not with<Particle>::exists(context, seed_id)) {
            return context.refuse("eltanin::phys::strong::Gluon::clue: Particle missing");
        }
        const auto seed_pos = with<Particle>::get(context, seed_id).current;
        vector<Affected<Particle>> found;
        for (const auto [id, particle] : context->aspect<Particle>().items()) {
            const vec3 delta = particle.current - seed_pos;
            if (glm::dot(delta, delta) <= Settings::clueTolerance * Settings::clueTolerance) {
                found.push_back(id);
            }
        }
        return create(context, Quantum{.particles = std::move(found)});
    }

    void Gluon::Actions::satisfy(Stewarding context) {
        auto particles = context.direct<Particle>();
        auto gluons = context.direct<Gluon>();
        for (auto [id, gluon] : gluons.items) {
            std::erase_if(gluon.particles, [&](const Affected<Particle>& particle_id) {
                return particles.items.find(particle_id) == nullptr;
            });
            if (gluon.particles.size() < 2) {
                with<Gluon>::remove(context, id);
                continue;
            }

            vec3 com{0.0f, 0.0f, 0.0f};
            float mass_sum = 0.0f;
            for (const auto particle_id : gluon.particles) {
                const auto& particle = particles.items.at(particle_id);
                com += particle.current * particle.mass;
                mass_sum += particle.mass;
            }
            if (mass_sum <= 0.0f) {
                with<Gluon>::remove(context, id);
                continue;
            }
            com /= mass_sum;
            for (const auto particle_id : gluon.particles) {
                auto& particle = particles.items.at(particle_id);
                particle.current += Settings::constraintStiffness * (com - particle.current);
            }
        }
    }

}
