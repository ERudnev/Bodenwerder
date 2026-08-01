#include <eltanin/physics/strong.q1.h>

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

}
