#pragma once

#include <eltanin/physics/body.q1.h>

namespace eltanin::phys::verlet {

    // Displace a Verlet particle. resilience ∈ [0, 1] is how much previous follows the same Δ:
    // 0 = only current (ball; invents Δv = Δ / dt), 1 = current and previous (teleport), ½ = halfKick.
    inline void semiKick(Particle& particle, dvec3 delta, double resilience) {
        particle.position += delta;
        particle.prev += delta * resilience;
    }

}
