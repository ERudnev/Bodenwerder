#include "geo/details/generator.h"
#include "geo/celestial/planet.h"

#include <base/logging.h>

#include <cmath>
#include <cstdint>

#include <glm/common.hpp>
#include <glm/geometric.hpp>

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

        auto hash01(integer x, integer y, integer z, integer salt) -> float {
            return float(hash32(x, y, z, salt) >> 8) * (1.0f / 16777215.0f);
        }

        auto valueNoise(float x, float y, float z, integer seed) -> float {
            const integer x0 = static_cast<integer>(std::floor(x));
            const integer y0 = static_cast<integer>(std::floor(y));
            const integer z0 = static_cast<integer>(std::floor(z));
            const float tx = x - float(x0);
            const float ty = y - float(y0);
            const float tz = z - float(z0);
            const float sx = tx * tx * (3.0f - 2.0f * tx);
            const float sy = ty * ty * (3.0f - 2.0f * ty);
            const float sz = tz * tz * (3.0f - 2.0f * tz);
            const float c000 = hash01(x0, y0, z0, seed);
            const float c100 = hash01(x0 + 1, y0, z0, seed);
            const float c010 = hash01(x0, y0 + 1, z0, seed);
            const float c110 = hash01(x0 + 1, y0 + 1, z0, seed);
            const float c001 = hash01(x0, y0, z0 + 1, seed);
            const float c101 = hash01(x0 + 1, y0, z0 + 1, seed);
            const float c011 = hash01(x0, y0 + 1, z0 + 1, seed);
            const float c111 = hash01(x0 + 1, y0 + 1, z0 + 1, seed);
            const float c00 = c000 + (c100 - c000) * sx;
            const float c10 = c010 + (c110 - c010) * sx;
            const float c01 = c001 + (c101 - c001) * sx;
            const float c11 = c011 + (c111 - c011) * sx;
            return (c00 + (c10 - c00) * sy) * (1.0f - sz) + (c01 + (c11 - c01) * sy) * sz;
        }

        auto fbm(vec3 point, integer seed) -> float {
            float sum = 0.0f;
            float amp = 0.5f;
            float freq = 1.0f;
            for (integer octave = 0; octave < 5; ++octave) {
                sum += amp * valueNoise(point.x * freq, point.y * freq, point.z * freq, seed + octave * 17);
                amp *= 0.5f;
                freq *= 2.17f;
            }
            return sum;
        }

        auto canyon(vec3 dir, vec3 axis, float halfWidth, float depth) -> float {
            const float span = std::abs(glm::dot(dir, glm::normalize(axis)));
            const float t = glm::clamp(1.0f - span / halfWidth, 0.0f, 1.0f);
            return -depth * t * t;
        }

        void generateHeights(planet::Planet& planet) {
            const integer count = planet.heights.pack.storedCount();
            const integer seed = planet.passport.seed;
            const float relief = planet.passport.geology.maxRelief;
            const float radius = planet.passport.radius;
            for (integer index = 0; index < count; ++index) {
                const auto slot = planet.heights.pack.slotOf(index);
                const vec3 dir = planet.heights.pack.direction(slot);
                float height = radius + (fbm(dir * 4.0f, seed) * 2.0f - 1.0f) * relief;
                height += canyon(dir, vec3{1.0f, 0.18f, 0.0f}, 0.11f, radius * 0.22f);
                height += canyon(dir, vec3{0.22f, 1.0f, 0.0f}, 0.09f, radius * 0.18f);
                planet.heights.at(slot) = height;
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
            base::warning("eltanin::locality::geo::generate: color stitch {}", planet.colors.stitch());
        }

        void fillCovers(planet::Planet& planet) {
            using Kind = Mineral::Kind;
            const auto packLayers = [](Kind a, Kind b, Kind c, Kind d) -> std::uint32_t {
                return std::uint32_t(a) | (std::uint32_t(b) << 4) | (std::uint32_t(c) << 8) | (std::uint32_t(d) << 12);
            };
            const std::uint32_t polar = packLayers(Kind::Ice, Kind::Olivine, Kind::Pyroxene, Kind::Iron);
            const std::uint32_t tropics = packLayers(Kind::Feldspar, Kind::Clay, Kind::Carbonaceous, Kind::Oxides);
            const integer count = planet.covers.pack.storedCount();
            for (integer index = 0; index < count; ++index) {
                const auto slot = planet.covers.pack.slotOf(index);
                const vec3 dir = planet.covers.pack.direction(slot);
                planet.covers.at(slot) = std::abs(dir.y) > 0.5f ? polar : tropics;
            }
        }

    }

    void generateSurfaceWeights(planet::Planet& planet) {
        fillCovers(planet);
    }

    void generate(planet::Planet& planet) {
        generateHeights(planet);
        generateColors(planet);
        generateSurfaceWeights(planet);
    }

}
