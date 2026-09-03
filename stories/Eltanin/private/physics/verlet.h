#pragma once

#include <eltanin/physics/body.q1.h>

namespace eltanin::phys::verlet {

    // Displace a Verlet victim (Particle: position + prev). invent ∈ [0, 1]:
    // 0 = teleport (Δv = 0), ½ = half kick, 1 = full kick (Δv = Δ / dt).
    inline void shift(Particle& particle, dvec3 delta, float invent) {
        particle.position += delta;
        particle.prev += delta * double(1.0f - invent);
    }

    inline void teleport(Particle& particle, dvec3 delta) { shift(particle, delta, 0.0f); }
    inline void halfKick(Particle& particle, dvec3 delta) { shift(particle, delta, 0.5f); }
    inline void kick(Particle& particle, dvec3 delta) { shift(particle, delta, 1.0f); }

    inline void teleport(Particle& particle, vec3 delta) { teleport(particle, dvec3{delta}); }
    inline void halfKick(Particle& particle, vec3 delta) { halfKick(particle, dvec3{delta}); }
    inline void kick(Particle& particle, vec3 delta) { kick(particle, dvec3{delta}); }

}
