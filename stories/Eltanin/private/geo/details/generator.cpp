#include "geo/details/generator.h"
#include "geo/celestial/planet.h"

#include <base/logging.h>

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
            const integer last = planet.heights.pack.edgeSegments();
            const float sea = planet.passport.radius;
            for (integer diamond = 0; diamond < IcosaPack::diamondCount; ++diamond) {
                for (integer iu = 0; iu <= last; ++iu) {
                    planet.heights.at(IcosaPack::Slot{.diamond = diamond, .iu = iu, .iv = 0}) = sea;
                    planet.heights.at(IcosaPack::Slot{.diamond = diamond, .iu = iu, .iv = last}) = sea;
                }
                for (integer iv = 1; iv < last; ++iv) {
                    planet.heights.at(IcosaPack::Slot{.diamond = diamond, .iu = 0, .iv = iv}) = sea;
                    planet.heights.at(IcosaPack::Slot{.diamond = diamond, .iu = last, .iv = iv}) = sea;
                }
            }
            //temp disabled: base::warning("eltanin::locality::geo::generate: height stitch {}", planet.heights.stitch());
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
            base::warning("eltanin::locality::geo::generate: color stitch {}", planet.colors.stitch());
        }

    }

    void generateSurfaceWeights(planet::Planet&) {
    }

    void generate(planet::Planet& planet) {
        generateHeights(planet);
        generateColors(planet);
        generateSurfaceWeights(planet);
    }

}
