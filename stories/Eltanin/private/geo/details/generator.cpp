#include "geo/details/generator.h"
#include "geo/celestial/planet.h"

#include <cstdint>

namespace eltanin::locality::geo {

    using namespace fqsm::api;
    using namespace rmmr;

    namespace {

        auto hash32(integer x, integer y, integer z, integer salt) -> std::uint32_t {
            std::uint32_t value = std::uint32_t(x) * 73856093u ^ std::uint32_t(y) * 19349663u ^ std::uint32_t(z) * 83492791u ^ std::uint32_t(salt) * 2654435761u;
            value ^= value >> 16;
            value *= 0x7feb352du;
            value ^= value >> 15;
            value *= 0x846ca68bu;
            value ^= value >> 16;
            return value;
        }

        void generateHeights(planet::Planet& planet) {
            const integer count = planet.heights.pack.storedCount();
            for (integer index = 0; index < count; ++index) {
                const auto slot = planet.heights.pack.slotOf(index);
                planet.heights.at(slot) = planet.passport.radius + (float(hash32(slot.diamond, slot.iu, slot.iv, planet.passport.seed) >> 8) * (1.0f / 16777215.0f) * 2.0f - 1.0f) * 5.0f;
            }
        }

        void generateColors(planet::Planet& planet) {
            const RGB palette[6] = {
                RGB{1.0f, 0.0f, 0.0f},
                RGB{0.0f, 1.0f, 0.0f},
                RGB{0.0f, 0.0f, 1.0f},
                RGB{1.0f, 1.0f, 0.0f},
                RGB{1.0f, 0.0f, 1.0f},
                RGB{0.0f, 1.0f, 1.0f},
            };
            const integer count = planet.colors.pack.storedCount();
            for (integer index = 0; index < count; ++index) {
                const auto slot = planet.colors.pack.slotOf(index);
                planet.colors.at(slot) = palette[hash32(slot.diamond, slot.iu, slot.iv, planet.passport.seed ^ 0x9e3779b9) % 6u];
            }
            planet.colors.stitch();
        }

    }

    void generate(planet::Planet& planet) {
        generateHeights(planet);
        generateColors(planet);
    }

}
