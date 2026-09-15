#include "geo/details/generator.h"
#include "geo/details/facies.h"
#include "geo/celestial/planet.h"

#include <base/logging.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <numbers>
#include <vector>

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

        auto mixNibble(Mineral::Mix mix, Mineral::Kind kind) -> integer {
            return integer((mix >> (static_cast<integer>(kind) * 4)) & 15u);
        }

        auto packLayers(Facies a, Facies b, Facies c, Facies d) -> std::uint32_t {
            return std::uint32_t(a) | (std::uint32_t(b) << 8) | (std::uint32_t(c) << 16) | (std::uint32_t(d) << 24);
        }

        auto angular(vec3 a, vec3 b) -> float {
            return std::acos(glm::clamp(glm::dot(glm::normalize(a), glm::normalize(b)), -1.0f, 1.0f));
        }

        auto gaussian(float angle, float sigma) -> float {
            const float s = std::max(sigma, 1.0e-4f);
            return std::exp(-0.5f * (angle * angle) / (s * s));
        }

        auto shieldCone(vec3 dir, vec3 axis, float radius, float height) -> float {
            const float minDot = std::cos(std::max(radius, 1.0e-4f));
            const float d = glm::dot(dir, axis);
            if (d < minDot)
                return 0.0f;
            const float t = glm::clamp(1.0f - angular(dir, axis) / std::max(radius, 1.0e-4f), 0.0f, 1.0f);
            return height * t * t;
        }

        auto canyonAlong(vec3 dir, vec3 center, vec3 along, float halfWidth, float halfLength, float depth) -> float {
            const vec3 radial = glm::normalize(glm::cross(along, center));
            const float across = std::abs(glm::dot(dir, radial));
            const float alongSpan = std::abs(glm::dot(dir, glm::normalize(along)));
            const float trough = glm::clamp(1.0f - across / halfWidth, 0.0f, 1.0f);
            const float reach = glm::clamp(1.0f - alongSpan / halfLength, 0.0f, 1.0f);
            return -depth * trough * trough * reach;
        }

        void generateHeights(planet::Planet& planet) {
            const integer count = planet.heights.pack.storedCount();
            const integer seed = planet.passport.seed;
            const auto& geo = planet.passport.geology;
            const float amplitude = geo.amplitude;
            const float grain = glm::clamp(geo.grain, 0.0f, 1.0f);
            const float tectonic = glm::clamp(geo.tectonic, 0.0f, 1.0f);
            const float differentiation = glm::clamp(geo.differentiation, 0.0f, 1.0f);
            const vec3 tharsis = glm::normalize(vec3{0.72f, 0.12f, 0.35f});
            const vec3 olympus = glm::normalize(tharsis + vec3{0.04f, 0.08f, -0.02f});
            const vec3 arsia = glm::normalize(tharsis + vec3{-0.12f, -0.08f, 0.10f});
            const vec3 pavonis = glm::normalize(tharsis + vec3{-0.04f, -0.02f, 0.08f});
            const vec3 ascrea = glm::normalize(tharsis + vec3{0.05f, 0.04f, 0.12f});
            const vec3 canyonCenter = glm::normalize(tharsis + vec3{0.35f, -0.08f, -0.22f});
            const vec3 canyonAlongAxis = glm::normalize(glm::cross(vec3{0.0f, 1.0f, 0.0f}, canyonCenter));
            const float dichotomyAmp = amplitude * (0.18f + 0.12f * differentiation);
            const float bulgeAmp = amplitude * (0.28f + 0.18f * tectonic);
            const float olympusAmp = amplitude * (0.42f + 0.18f * tectonic);
            const float canyonAmp = amplitude * (0.32f + 0.28f * differentiation);
            const float crustAmp = amplitude * (0.10f + 0.22f * grain);
            struct Crater {
                vec3 dir;
                float radius;
                float depth;
                float minDot;
            };
            vector<Crater> craters;
            craters.reserve(72);
            for (integer crater = 0; crater < 72; ++crater) {
                const float u = hash01(crater, seed, 3, 11);
                const float v = hash01(crater, seed, 5, 13);
                const float theta = 2.0f * std::numbers::pi_v<float> * u;
                const float z = 2.0f * v - 1.0f;
                const float r = std::sqrt(std::max(0.0f, 1.0f - z * z));
                const vec3 craterDir = glm::normalize(vec3{r * std::cos(theta), z, r * std::sin(theta)});
                if (glm::dot(craterDir, olympus) > 0.92f)
                    continue;
                const float radius = 0.025f + 0.10f * hash01(crater, seed, 7, 17);
                craters.push_back(Crater{
                    .dir = craterDir,
                    .radius = radius,
                    .depth = amplitude * (0.08f + 0.18f * hash01(crater, seed, 9, 19)),
                    .minDot = std::cos(radius),
                });
            }
            for (integer index = 0; index < count; ++index) {
                const auto slot = planet.heights.pack.slotOf(index);
                const vec3 dir = planet.heights.pack.direction(slot);
                float delta = -dichotomyAmp * glm::smoothstep(-0.22f, 0.22f, dir.y);
                delta += bulgeAmp * gaussian(angular(dir, tharsis), 0.42f);
                delta += shieldCone(dir, olympus, 0.14f, olympusAmp);
                delta += shieldCone(dir, arsia, 0.08f, amplitude * 0.22f);
                delta += shieldCone(dir, pavonis, 0.07f, amplitude * 0.18f);
                delta += shieldCone(dir, ascrea, 0.075f, amplitude * 0.20f);
                delta += canyonAlong(dir, canyonCenter, canyonAlongAxis, 0.07f + 0.04f * differentiation, 0.55f, canyonAmp);
                for (const auto& crater : craters) {
                    const float d = glm::dot(dir, crater.dir);
                    if (d < crater.minDot)
                        continue;
                    const float ang = std::acos(glm::clamp(d, -1.0f, 1.0f));
                    const float t = ang / crater.radius;
                    const float bowl = (1.0f - t * t);
                    const float rim = std::exp(-((t - 0.78f) * (t - 0.78f)) / 0.012f);
                    delta += -crater.depth * bowl + crater.depth * 0.18f * rim;
                }
                delta += (fbm(dir * (2.0f + 6.0f * grain), seed) * 2.0f - 1.0f) * crustAmp;
                planet.heights.at(slot) = planet.encodeRelief(glm::clamp(delta, -amplitude, amplitude));
            }
            planet.heights.stitch();
        }

        void fillCovers(planet::Planet& planet) {
            const auto& geo = planet.passport.geology;
            const integer ice = mixNibble(geo.mix, Mineral::Kind::Ice);
            const integer olivine = mixNibble(geo.mix, Mineral::Kind::Olivine);
            const integer pyroxene = mixNibble(geo.mix, Mineral::Kind::Pyroxene);
            const integer feldspar = mixNibble(geo.mix, Mineral::Kind::Feldspar);
            const integer clay = mixNibble(geo.mix, Mineral::Kind::Clay);
            const integer carbonaceous = mixNibble(geo.mix, Mineral::Kind::Carbonaceous);
            const integer iron = mixNibble(geo.mix, Mineral::Kind::Iron);
            const integer oxides = mixNibble(geo.mix, Mineral::Kind::Oxides);
            const integer salts = mixNibble(geo.mix, Mineral::Kind::Salts);
            const float cohesion = glm::clamp(geo.cohesion, 0.0f, 1.0f);
            const float age = glm::clamp(geo.surfaceAge, 0.0f, 1.0f);
            const float differentiation = glm::clamp(geo.differentiation, 0.0f, 1.0f);
            const vec3 tharsis = glm::normalize(vec3{0.72f, 0.12f, 0.35f});
            const vec3 olympus = glm::normalize(tharsis + vec3{0.04f, 0.08f, -0.02f});
            const vec3 canyonCenter = glm::normalize(tharsis + vec3{0.35f, -0.08f, -0.22f});
            const vec3 canyonAlongAxis = glm::normalize(glm::cross(vec3{0.0f, 1.0f, 0.0f}, canyonCenter));
            const vec3 canyonRadial = glm::normalize(glm::cross(canyonAlongAxis, canyonCenter));
            const integer last = planet.heights.pack.edgeSegments();
            const integer count = planet.covers.pack.storedCount();
            auto heightAt = [&](geo::IcosaPack::Slot slot) -> float {
                slot.iu = std::clamp(slot.iu, integer{0}, last);
                slot.iv = std::clamp(slot.iv, integer{0}, last);
                return float(planet.heights.at(slot));
            };
            for (integer index = 0; index < count; ++index) {
                const auto slot = planet.covers.pack.slotOf(index);
                const vec3 dir = planet.covers.pack.direction(slot);
                const float relief = float(planet.heights.at(slot));
                const float du = heightAt(geo::IcosaPack::Slot{.diamond = slot.diamond, .iu = slot.iu + 1, .iv = slot.iv}) - heightAt(geo::IcosaPack::Slot{.diamond = slot.diamond, .iu = slot.iu - 1, .iv = slot.iv});
                const float dv = heightAt(geo::IcosaPack::Slot{.diamond = slot.diamond, .iu = slot.iu, .iv = slot.iv + 1}) - heightAt(geo::IcosaPack::Slot{.diamond = slot.diamond, .iu = slot.iu, .iv = slot.iv - 1});
                const float slope = std::sqrt(du * du + dv * dv) / float(std::max(planet::Planet::reliefPeak, std::int16_t{1}));
                const float polar = std::abs(dir.y);
                const float cap = 0.62f + 0.12f * (ice / 15.0f) + 0.06f * age;
                const float shield = gaussian(angular(dir, olympus), 0.16f) + gaussian(angular(dir, tharsis), 0.28f);
                const float acrossCanyon = std::abs(glm::dot(dir, canyonRadial));
                const float alongCanyon = std::abs(glm::dot(dir, canyonAlongAxis));
                const bool inCanyon = acrossCanyon < 0.09f and alongCanyon < 0.55f and relief < 0.0f;
                const bool inCrater = relief < -0.12f * float(planet::Planet::reliefPeak) and shield < 0.35f;
                Facies surface = Facies::RegolithMafic;
                Facies shallow = Facies::Basalt;
                Facies deep = Facies::Gabbro;
                Facies mantle = Facies::Peridotite;
                if (oxides > 0)
                    surface = cohesion < 0.45f ? Facies::Hematite : Facies::DesertVarnish;
                if (clay > oxides and clay > 0)
                    surface = Facies::ClayPan;
                if (pyroxene > 0)
                    shallow = cohesion < 0.4f ? Facies::RegolithMafic : Facies::Basalt;
                if (feldspar > pyroxene and feldspar > 0)
                    shallow = Facies::RegolithFelsic;
                if (olivine > 0 and differentiation > 0.4f)
                    mantle = Facies::Peridotite;
                else if (pyroxene > 0)
                    mantle = Facies::Pyroxenite;
                if (carbonaceous > 6 and polar < 0.4f)
                    surface = Facies::Chondrite;
                if (salts > 0 and relief < -0.05f * float(planet::Planet::reliefPeak))
                    surface = cohesion < 0.5f ? Facies::Evaporite : Facies::Caliche;
                if (iron > 8 and inCrater)
                    deep = Facies::IronMetal;
                if (shield > 0.55f and pyroxene > 0) {
                    surface = age < 0.35f ? Facies::Pahoehoe : Facies::Scoria;
                    shallow = Facies::Basalt;
                    deep = Facies::Gabbro;
                }
                if (inCanyon) {
                    surface = slope > 0.08f ? Facies::Gabbro : Facies::Basalt;
                    shallow = Facies::Gabbro;
                    deep = differentiation > 0.35f ? Facies::Peridotite : Facies::Gabbro;
                    mantle = Facies::Peridotite;
                    if (olivine == 0)
                        mantle = Facies::Pyroxenite;
                }
                if (inCrater and feldspar + pyroxene > 0) {
                    surface = Facies::Breccia;
                    shallow = Facies::RegolithMafic;
                }
                if (ice > 0 and polar > cap) {
                    if (age > 0.65f and cohesion > 0.55f)
                        surface = Facies::Glacier;
                    else if (cohesion < 0.35f)
                        surface = Facies::Snow;
                    else
                        surface = Facies::DirtyIce;
                    shallow = Facies::DirtyIce;
                    deep = Facies::Glacier;
                    mantle = olivine > 0 ? Facies::Dunite : Facies::Pyroxenite;
                }
                planet.covers.at(slot) = packLayers(surface, shallow, deep, mantle);
            }
        }

    }

    void generateSurfaceWeights(planet::Planet& planet) {
        fillCovers(planet);
    }

    void logFieldSummary(const planet::Planet& planet) {
        const auto& pack = planet.heights.pack;
        const integer segments = pack.edgeSegments();
        const integer span = pack.edgeVertices();
        const integer stored = pack.storedCount();
        const integer unique = pack.uniqueCount();
        const double arc = std::max(0.0, double(planet.passport.radius)) * std::acos(1.0 / std::sqrt(5.0));
        const float texelMeters = float(arc / double(std::max(segments, integer{1})));
        const auto atlas = pack.atlasSize();
        const std::size_t heightBytes = static_cast<std::size_t>(stored) * sizeof(std::int16_t);
        const std::size_t coverBytes = static_cast<std::size_t>(stored) * sizeof(std::uint32_t);
        const std::size_t fieldBytes = heightBytes + coverBytes;
        const double fieldMiB = double(fieldBytes) / (1024.0 * 1024.0);
        base::message("eltanin::geo::generate: icosa edgeBase={} tessellation={} → {} segments/edge, {} verts/diamond side, {:.1f} m/texel (R={:.0f} m, arc={:.0f} m)", pack.edgeBase, pack.tessellation, segments, span, texelMeters, planet.passport.radius, arc);
        base::message("eltanin::geo::generate: field matrices {} stored slots ({} unique), diamond {}×{}, atlas {}×{}, 10 layers → heights {} B, cover {} B, total {} B ({:.2f} MiB)", stored, unique, span, span, atlas.x, atlas.y, heightBytes, coverBytes, fieldBytes, fieldMiB);
    }

    void generate(planet::Planet& planet) {
        generateHeights(planet);
        generateSurfaceWeights(planet);
        logFieldSummary(planet);
    }

}
