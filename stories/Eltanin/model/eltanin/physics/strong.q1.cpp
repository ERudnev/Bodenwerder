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

}
