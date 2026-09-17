#include "geo/celestial/generator.h"

#include <eltanin/geo/minerals.q1.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

#include <glm/common.hpp>
#include <glm/geometric.hpp>

namespace eltanin::planet {

    using namespace fqsm::api;
    using namespace rmmr;

    namespace {

        auto mixNibble(geo::Mineral::Mix mix, geo::Mineral::Kind kind) -> integer {
            return integer((mix >> (static_cast<integer>(kind) * 4)) & 15u);
        }

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

        auto sphereDir(integer index, integer seed, integer saltU, integer saltV) -> vec3 {
            const float u = hash01(index, seed, saltU, 11);
            const float v = hash01(index, seed, saltV, 13);
            const float theta = 2.0f * std::numbers::pi_v<float> * u;
            const float z = 2.0f * v - 1.0f;
            const float r = std::sqrt(std::max(0.0f, 1.0f - z * z));
            return glm::normalize(vec3{r * std::cos(theta), z, r * std::sin(theta)});
        }

        auto impactBurst(vec3 axis, float radius, float depth, integer seed) -> Generator::Burst {
            return Generator::Burst{.axis = axis, .radius = radius, .lift = -depth, .rim = depth * 0.18f, .seed = seed};
        }

        auto eruptionBurst(vec3 axis, float radius, float height, integer seed) -> Generator::Burst {
            return Generator::Burst{.axis = axis, .radius = radius, .lift = height, .rim = 0.0f, .seed = seed};
        }

        auto craterEpoch(integer seed, integer salt, integer count, float radiusMin, float radiusSpan, float depthMin, float depthSpan, float highland, vec3 avoidAxis, float avoidDot) -> vector<Generator::Burst> {
            vector<Generator::Burst> bursts;
            bursts.reserve(static_cast<std::size_t>(count));
            for (integer crater = 0; crater < count; ++crater) {
                const vec3 axis = sphereDir(crater, seed, salt, salt + 2);
                if (axis.y > highland)
                    continue;
                if (glm::dot(axis, avoidAxis) > avoidDot)
                    continue;
                const float radius = radiusMin + radiusSpan * hash01(crater, seed, salt + 4, 17);
                const float depth = depthMin + depthSpan * hash01(crater, seed, salt + 6, 19);
                bursts.push_back(impactBurst(axis, radius, depth, seed + salt * 997 + crater * 31));
            }
            return bursts;
        }

    }

    auto Generator::marsCover(const Passport& passport) -> PaintCover {
        const auto& geology = passport.geology;
        const float differentiation = glm::clamp(geology.differentiation, 0.0f, 1.0f);
        const vec3 tharsis = glm::normalize(vec3{0.72f, 0.12f, 0.35f});
        const vec3 olympus = glm::normalize(tharsis + vec3{0.04f, 0.08f, -0.02f});
        const vec3 canyonCenter = glm::normalize(tharsis + vec3{0.35f, -0.08f, -0.22f});
        const vec3 canyonAlong = glm::normalize(glm::cross(vec3{0.0f, 1.0f, 0.0f}, canyonCenter));
        return PaintCover{
            .ice = mixNibble(geology.mix, geo::Mineral::Kind::Ice),
            .olivine = mixNibble(geology.mix, geo::Mineral::Kind::Olivine),
            .pyroxene = mixNibble(geology.mix, geo::Mineral::Kind::Pyroxene),
            .feldspar = mixNibble(geology.mix, geo::Mineral::Kind::Feldspar),
            .clay = mixNibble(geology.mix, geo::Mineral::Kind::Clay),
            .carbonaceous = mixNibble(geology.mix, geo::Mineral::Kind::Carbonaceous),
            .iron = mixNibble(geology.mix, geo::Mineral::Kind::Iron),
            .oxides = mixNibble(geology.mix, geo::Mineral::Kind::Oxides),
            .salts = mixNibble(geology.mix, geo::Mineral::Kind::Salts),
            .cohesion = glm::clamp(geology.cohesion, 0.0f, 1.0f),
            .age = glm::clamp(geology.surfaceAge, 0.0f, 1.0f),
            .differentiation = differentiation,
            .tharsis = tharsis,
            .olympus = olympus,
            .canyonCenter = canyonCenter,
            .canyonAlong = canyonAlong,
            .canyonHalfWidth = 0.04f + 0.02f * differentiation,
            .canyonHalfLength = 0.46f,
            .seed = passport.seed,
        };
    }

    void Generator::mars(Planet& planet) {
        const auto& geology = planet.passport.geology;
        const float amplitude = geology.amplitude;
        const float grain = glm::clamp(geology.grain, 0.0f, 1.0f);
        const float tectonic = glm::clamp(geology.tectonic, 0.0f, 1.0f);
        const float differentiation = glm::clamp(geology.differentiation, 0.0f, 1.0f);
        const float age = glm::clamp(geology.surfaceAge, 0.0f, 1.0f);
        const integer seed = planet.passport.seed;
        const vec3 tharsis = glm::normalize(vec3{0.72f, 0.12f, 0.35f});
        const vec3 olympus = glm::normalize(tharsis + vec3{0.04f, 0.08f, -0.02f});
        const vec3 arsia = glm::normalize(tharsis + vec3{-0.12f, -0.08f, 0.10f});
        const vec3 pavonis = glm::normalize(tharsis + vec3{-0.04f, -0.02f, 0.08f});
        const vec3 ascrea = glm::normalize(tharsis + vec3{0.05f, 0.04f, 0.12f});
        const vec3 canyonCenter = glm::normalize(tharsis + vec3{0.35f, -0.08f, -0.22f});
        const vec3 canyonAlong = glm::normalize(glm::cross(vec3{0.0f, 1.0f, 0.0f}, canyonCenter));
        const vec3 canyonAcross = glm::normalize(glm::cross(canyonCenter, canyonAlong));
        const vec3 hellas = glm::normalize(vec3{0.18f, -0.72f, 0.52f});
        const vec3 argyre = glm::normalize(vec3{-0.48f, -0.68f, 0.22f});
        const vec3 isidis = glm::normalize(vec3{0.58f, 0.04f, -0.52f});
        const float worn = 0.55f + 0.45f * age;
        geo::IcosaMap<float> relief{planet.heights.pack, 0.0f};

        applyProvinces(relief, Provinces{.count = 2, .seed = seed, .amplitude = amplitude * (0.18f + 0.12f * differentiation)});

        applyBursts(relief, vector<Burst>{
            impactBurst(hellas, 0.24f, amplitude * 0.48f * worn, seed + 101),
            impactBurst(argyre, 0.14f, amplitude * 0.29f * worn, seed + 137),
            impactBurst(isidis, 0.11f, amplitude * 0.20f * worn, seed + 173),
        });
        applyBursts(relief, craterEpoch(seed, 3, 42 + integer(56.0f * grain), 0.025f, 0.075f, amplitude * 0.07f * worn, amplitude * 0.13f * worn, 0.16f, olympus, 0.97f));
        applyErode(relief, Erode{.years = age, .strength = 0.58f, .north = 1.0f, .iterations = 4, .seed = seed + 251});

        applySwell(relief, Swell{.axis = tharsis, .sigma = 0.44f, .amplitude = amplitude * (0.26f + 0.16f * tectonic), .seed = seed + 307});
        applyBursts(relief, vector<Burst>{
            eruptionBurst(olympus, 0.16f, amplitude * (0.36f + 0.16f * tectonic), seed + 331),
            eruptionBurst(arsia, 0.10f, amplitude * 0.18f, seed + 347),
            eruptionBurst(pavonis, 0.09f, amplitude * 0.15f, seed + 367),
            eruptionBurst(ascrea, 0.095f, amplitude * 0.17f, seed + 389),
        });
        applyRift(relief, Rift{.center = canyonCenter, .along = canyonAlong, .halfWidth = 0.04f + 0.02f * differentiation, .halfLength = 0.46f, .depth = amplitude * (0.24f + 0.22f * differentiation), .seed = seed + 401});
        const vector<Rift> minorRifts{
            Rift{.center = glm::normalize(canyonCenter - canyonAlong * 0.25f + canyonAcross * 0.08f), .along = glm::normalize(canyonAlong + canyonAcross * 0.26f), .halfWidth = 0.014f, .halfLength = 0.17f, .depth = amplitude * 0.095f, .seed = seed + 431},
            Rift{.center = glm::normalize(canyonCenter + canyonAlong * 0.22f - canyonAcross * 0.07f), .along = glm::normalize(canyonAlong - canyonAcross * 0.31f), .halfWidth = 0.011f, .halfLength = 0.14f, .depth = amplitude * 0.075f, .seed = seed + 439},
            Rift{.center = glm::normalize(canyonCenter + canyonAcross * 0.13f), .along = glm::normalize(canyonAlong + canyonAcross * 0.12f), .halfWidth = 0.009f, .halfLength = 0.11f, .depth = amplitude * 0.062f, .seed = seed + 443},
            Rift{.center = glm::normalize(canyonCenter - canyonAcross * 0.15f - canyonAlong * 0.06f), .along = glm::normalize(canyonAlong - canyonAcross * 0.18f), .halfWidth = 0.008f, .halfLength = 0.09f, .depth = amplitude * 0.052f, .seed = seed + 449},
        };
        for (const Rift& rift : minorRifts)
            applyRift(relief, rift);
        applyDrainage(relief, Drainage{.seed = seed + 601, .sources = 10 + integer(12.0f * grain), .steps = 90, .stepLength = 0.0035f, .width = 0.0015f + 0.0005f * grain, .depth = amplitude * (0.014f + 0.008f * age)});

        applyBursts(relief, craterEpoch(seed, 41, 20 + integer(28.0f * grain), 0.014f, 0.045f, amplitude * 0.035f, amplitude * 0.075f, 1.0f, olympus, 0.92f));
        applyBombardment(relief, Bombardment{.seed = seed + 701, .count = 1200 + integer(1000.0f * grain * age), .radiusMin = 0.0018f, .radiusMax = 0.010f, .depth = amplitude * (0.012f + 0.008f * grain), .northDensity = 0.34f + 0.18f * (1.0f - age)});
        applyErode(relief, Erode{.years = 0.32f + 0.28f * age, .strength = 0.28f, .north = 0.35f, .iterations = 3, .seed = seed + 457});

        const integer count = planet.heights.pack.storedCount();
        for (integer index = 0; index < count; ++index) {
            const auto slot = planet.heights.pack.slotOf(index);
            planet.heights.at(slot) = planet.encodeRelief(glm::clamp(relief.at(slot), -amplitude, amplitude));
        }
        planet.heights.stitch();
        paintCover(planet, marsCover(planet.passport));
    }

}
