#pragma once

#include <cstdint>
#include <vector>

#include <glm/vec3.hpp>

namespace kubes::resource {

    // Cartoon stellar particle. Units: ly, K, L☉, M☉. Origin = SMBH.
    struct Star {
        glm::vec3 position_ly{};
        float temperature_K = 5800.0f;
        float luminosity_sun = 1.0f;
        float mass_sun = 1.0f;
    };

    using Galaxy = std::vector<Star>;

    auto generate_spiral_galaxy(std::size_t count, std::uint32_t seed) -> Galaxy;

}
