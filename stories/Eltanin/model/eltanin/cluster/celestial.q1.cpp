#include <eltanin/cluster/celestial.q1.h>

#include <algorithm>
#include <cmath>

#include <glm/common.hpp>

namespace eltanin::cluster {

    using namespace fqsm::api;

    auto Star::Quantum::look() const -> vec3 {
        const float t = std::clamp((temperature - 2500.0f) / 9500.0f, 0.0f, 1.0f);
        const vec3 photosphere = t < 0.40f ? glm::mix(vec3{1.00f, 0.56f, 0.30f}, vec3{1.00f, 0.84f, 0.66f}, t / 0.40f) : glm::mix(vec3{1.00f, 0.84f, 0.66f}, vec3{0.70f, 0.78f, 1.00f}, (t - 0.40f) / 0.60f);
        vec3 scatter{0.0f, 0.0f, 0.0f};
        vec3 absorb{0.0f, 0.0f, 0.0f};
        float sum = 0.0f;
        const auto& gases = chemistry::Volatile::table();
        for (integer index = 0; index < chemistry::Volatile::kindCount; ++index) {
            const float amount = float(chemistry::Volatile::nibble(mix, static_cast<chemistry::Volatile::Kind>(index)));
            if (amount <= 0.0f)
                continue;
            scatter += amount * gases[static_cast<std::size_t>(index)].scatter;
            absorb += amount * gases[static_cast<std::size_t>(index)].absorb;
            sum += amount;
        }
        if (sum <= 0.0f)
            return photosphere;
        scatter /= sum;
        absorb /= sum;
        return glm::clamp(photosphere * (vec3{1.0f, 1.0f, 1.0f} - absorb * 0.35f) + scatter * 0.18f, vec3{0.0f, 0.0f, 0.0f}, vec3{1.0f, 1.0f, 1.0f});
    }

    auto doctrine::celestial() -> Schema {
        return ask::schema::merge({
            ask::schema::aspect<Celestial>(),
            ask::schema::aspect<Star>(),
        });
    }

}
