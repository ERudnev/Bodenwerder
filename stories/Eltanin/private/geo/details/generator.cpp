#include "geo/details/generator.h"
#include "geo/details/facies.h"
#include "geo/celestial/planet.h"

#include <base/logging.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <string>
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

        auto packLayers(Facies surface, Facies below) -> std::uint16_t {
            return std::uint16_t(std::uint16_t(surface) | (std::uint16_t(below) << 8));
        }

        auto faciesMeans() -> const std::array<vec4, 48>& {
            static const std::array<vec4, 48> means{
                vec4{0.88f, 0.91f, 0.94f, 0.78f}, // Snow
                vec4{0.50f, 0.68f, 0.78f, 0.42f}, // Glacier
                vec4{0.48f, 0.50f, 0.46f, 0.76f}, // DirtyIce
                vec4{0.82f, 0.87f, 0.94f, 0.48f}, // VolatileFrost
                vec4{0.52f, 0.58f, 0.56f, 0.72f}, // Hydrate
                vec4{0.34f, 0.40f, 0.20f, 0.72f}, // Dunite
                vec4{0.28f, 0.32f, 0.20f, 0.76f}, // Peridotite
                vec4{0.22f, 0.24f, 0.20f, 0.78f}, // Pyroxenite
                vec4{0.20f, 0.24f, 0.15f, 0.66f}, // Komatiite
                vec4{0.16f, 0.15f, 0.14f, 0.82f}, // Basalt
                vec4{0.28f, 0.28f, 0.27f, 0.76f}, // Gabbro
                vec4{0.42f, 0.39f, 0.36f, 0.74f}, // Andesite
                vec4{0.60f, 0.53f, 0.46f, 0.70f}, // Granite
                vec4{0.68f, 0.62f, 0.56f, 0.68f}, // Rhyolite
                vec4{0.035f, 0.030f, 0.040f, 0.22f}, // Obsidian
                vec4{0.78f, 0.76f, 0.72f, 0.66f}, // Anorthosite
                vec4{0.46f, 0.30f, 0.20f, 0.88f}, // ClayPan
                vec4{0.52f, 0.22f, 0.12f, 0.84f}, // Laterite
                vec4{0.58f, 0.45f, 0.30f, 0.84f}, // Arenite
                vec4{0.72f, 0.68f, 0.56f, 0.78f}, // Evaporite
                vec4{0.64f, 0.62f, 0.56f, 0.62f}, // Carbonate
                vec4{0.12f, 0.11f, 0.10f, 0.86f}, // Chondrite
                vec4{0.42f, 0.15f, 0.08f, 0.72f}, // Tholin
                vec4{0.045f, 0.035f, 0.030f, 0.55f}, // Bitumen
                vec4{0.42f, 0.43f, 0.40f, 0.38f}, // IronMetal
                vec4{0.48f, 0.49f, 0.46f, 0.34f}, // NickelMetal
                vec4{0.38f, 0.29f, 0.12f, 0.48f}, // Sulfide
                vec4{0.78f, 0.67f, 0.10f, 0.82f}, // SulfurPlains
                vec4{0.88f, 0.90f, 0.94f, 0.40f}, // SO2Frost
                vec4{0.58f, 0.48f, 0.16f, 0.78f}, // Fumarole
                vec4{0.52f, 0.18f, 0.10f, 0.82f}, // Hematite
                vec4{0.10f, 0.10f, 0.11f, 0.48f}, // Magnetite
                vec4{0.20f, 0.14f, 0.09f, 0.72f}, // DesertVarnish
                vec4{0.50f, 0.30f, 0.15f, 0.38f}, // BaseMetal
                vec4{0.45f, 0.32f, 0.28f, 0.68f}, // Porphyry
                vec4{0.56f, 0.55f, 0.50f, 0.34f}, // PGMLag
                vec4{0.48f, 0.27f, 0.20f, 0.82f}, // REELaterite
                vec4{0.30f, 0.30f, 0.24f, 0.76f}, // Actinide
                vec4{0.08f, 0.075f, 0.070f, 0.58f}, // Pahoehoe
                vec4{0.13f, 0.10f, 0.085f, 0.88f}, // Scoria
                vec4{0.66f, 0.64f, 0.58f, 0.92f}, // Pumice
                vec4{0.72f, 0.67f, 0.56f, 0.72f}, // SilicaSinter
                vec4{0.25f, 0.20f, 0.16f, 0.92f}, // RegolithMafic
                vec4{0.55f, 0.49f, 0.42f, 0.90f}, // RegolithFelsic
                vec4{0.36f, 0.27f, 0.22f, 0.82f}, // Breccia
                vec4{0.70f, 0.58f, 0.52f, 0.62f}, // Pegmatite
                vec4{0.35f, 0.16f, 0.48f, 0.44f}, // Exotic
                vec4{0.76f, 0.69f, 0.55f, 0.86f}, // Caliche
            };
            return means;
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
                Facies below = Facies::Basalt;
                if (oxides > 0)
                    surface = cohesion < 0.45f ? Facies::Hematite : Facies::DesertVarnish;
                if (clay > oxides and clay > 0)
                    surface = Facies::ClayPan;
                if (pyroxene > 0)
                    below = cohesion < 0.4f ? Facies::RegolithMafic : Facies::Basalt;
                if (feldspar > pyroxene and feldspar > 0)
                    below = Facies::RegolithFelsic;
                if (carbonaceous > 6 and polar < 0.4f)
                    surface = Facies::Chondrite;
                if (salts > 0 and relief < -0.05f * float(planet::Planet::reliefPeak))
                    surface = cohesion < 0.5f ? Facies::Evaporite : Facies::Caliche;
                if (iron > 8 and inCrater)
                    below = Facies::IronMetal;
                if (shield > 0.55f and pyroxene > 0) {
                    surface = age < 0.35f ? Facies::Pahoehoe : Facies::Scoria;
                    below = Facies::Basalt;
                }
                if (inCanyon) {
                    surface = slope > 0.08f ? Facies::Gabbro : Facies::Basalt;
                    below = differentiation > 0.35f and olivine > 0 ? Facies::Peridotite : Facies::Gabbro;
                }
                if (inCrater and feldspar + pyroxene > 0) {
                    surface = Facies::Breccia;
                    below = iron > 8 ? Facies::IronMetal : Facies::RegolithMafic;
                }
                if (ice > 0 and polar > cap) {
                    if (age > 0.65f and cohesion > 0.55f)
                        surface = Facies::Glacier;
                    else if (cohesion < 0.35f)
                        surface = Facies::Snow;
                    else
                        surface = Facies::DirtyIce;
                    below = Facies::DirtyIce;
                }
                planet.covers.at(slot) = packLayers(surface, below);
            }
        }

        auto surfacePoint(const planet::Planet& planet, IcosaPack::Slot slot) -> vec3 {
            const integer last = planet.heights.pack.edgeSegments();
            slot.iu = std::clamp(slot.iu, integer{0}, last);
            slot.iv = std::clamp(slot.iv, integer{0}, last);
            const vec3 dir = planet.heights.pack.direction(slot);
            return dir * float(planet.surfaceRadius(planet.heights.at(slot)));
        }

        auto meanSurface(const planet::Planet& planet, IcosaPack::Slot slot) -> vec4 {
            const integer last = planet.heights.pack.edgeSegments();
            slot.iu = std::clamp(slot.iu, integer{0}, last);
            slot.iv = std::clamp(slot.iv, integer{0}, last);
            const vec3 point = surfacePoint(planet, slot);
            const vec3 radial = glm::normalize(point);
            vec3 normal = glm::cross(surfacePoint(planet, IcosaPack::Slot{.diamond = slot.diamond, .iu = slot.iu + 1, .iv = slot.iv}) - surfacePoint(planet, IcosaPack::Slot{.diamond = slot.diamond, .iu = slot.iu - 1, .iv = slot.iv}), surfacePoint(planet, IcosaPack::Slot{.diamond = slot.diamond, .iu = slot.iu, .iv = slot.iv + 1}) - surfacePoint(planet, IcosaPack::Slot{.diamond = slot.diamond, .iu = slot.iu, .iv = slot.iv - 1}));
            const float magnitude = glm::length(normal);
            if (magnitude < 1.0e-8f)
                normal = radial;
            else {
                normal /= magnitude;
                if (glm::dot(normal, radial) < 0.0f)
                    normal = -normal;
            }
            const float slope = 1.0f - glm::clamp(glm::dot(normal, radial), 0.0f, 1.0f);
            const float below = glm::smoothstep(0.0038f, 0.034f, slope);
            const std::uint16_t cover = planet.covers.at(slot);
            const auto& means = faciesMeans();
            return glm::mix(means[static_cast<std::size_t>(cover & 255u)], means[static_cast<std::size_t>((cover >> 8) & 255u)], below);
        }

        auto toByte(float value) -> std::uint8_t {
            return static_cast<std::uint8_t>(std::lround(glm::clamp(value, 0.0f, 1.0f) * 255.0f));
        }

        void writeFarAlbedo(const planet::Planet& planet) {
            const auto& map = planet.farAlbedo;
            const index2 atlasSize = map.pack.atlasSize();
            vector<std::uint8_t> pixels(static_cast<std::size_t>(atlasSize.x * atlasSize.y) * 4u, std::uint8_t{0});
            for (integer index = 0; index < map.pack.storedCount(); ++index) {
                const auto slot = map.pack.slotOf(index);
                const index2 coord = map.pack.atlasCoord(slot);
                const vec4 color = map.at(slot);
                const std::size_t pixel = static_cast<std::size_t>(coord.y * atlasSize.x + coord.x) * 4u;
                pixels[pixel] = toByte(color.z);
                pixels[pixel + 1] = toByte(color.y);
                pixels[pixel + 2] = toByte(color.x);
                pixels[pixel + 3] = toByte(color.w);
            }
            const std::filesystem::path directory = std::filesystem::path{DAQL_ASSETS_DIR} / "Eltanin" / "planetsCache";
            std::error_code error;
            std::filesystem::create_directories(directory, error);
            if (error) {
                base::warning("eltanin::geo::generate: cannot create '{}': {}", directory.string(), error.message());
                return;
            }
            const auto stamp = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            const std::string stem = "planet-" + std::to_string(planet.passport.seed) + "-" + std::to_string(stamp);
            std::filesystem::path file = directory / (stem + ".tga");
            integer collision = 1;
            while (std::filesystem::exists(file))
                file = directory / (stem + "-" + std::to_string(collision++) + ".tga");
            std::array<std::uint8_t, 18> header{};
            header[2] = 2;
            header[12] = static_cast<std::uint8_t>(atlasSize.x & 255);
            header[13] = static_cast<std::uint8_t>((atlasSize.x >> 8) & 255);
            header[14] = static_cast<std::uint8_t>(atlasSize.y & 255);
            header[15] = static_cast<std::uint8_t>((atlasSize.y >> 8) & 255);
            header[16] = 32;
            header[17] = 0x28;
            std::ofstream output{file, std::ios::binary};
            output.write(reinterpret_cast<const char*>(header.data()), static_cast<std::streamsize>(header.size()));
            output.write(reinterpret_cast<const char*>(pixels.data()), static_cast<std::streamsize>(pixels.size()));
            if (not output)
                base::warning("eltanin::geo::generate: cannot write '{}'", file.string());
            else
                base::message("eltanin::geo::generate: far albedo cache → {}", file.string());
        }

        void blurFarAlbedo(planet::Planet& planet) {
            auto& map = planet.farAlbedo;
            const integer last = map.pack.edgeSegments();
            constexpr float sigmaSpace = 1.1f;
            constexpr float sigmaColor = 0.16f;
            const integer radius = 4;
            auto sample = [&](const vector<vec4>& field, integer diamond, integer iu, integer iv) -> vec4 {
                return field[static_cast<std::size_t>(map.pack.index(IcosaPack::Slot{.diamond = diamond, .iu = std::clamp(iu, integer{0}, last), .iv = std::clamp(iv, integer{0}, last)}))];
            };
            const vector<vec4> source = map.values;
            const float spaceScale = 1.0f / (2.0f * sigmaSpace * sigmaSpace);
            const float colorScale = 1.0f / (2.0f * sigmaColor * sigmaColor);
            for (integer diamond = 0; diamond < IcosaPack::diamondCount; ++diamond) {
                for (integer iv = 0; iv <= last; ++iv) {
                    for (integer iu = 0; iu <= last; ++iu) {
                        const vec4 center = sample(source, diamond, iu, iv);
                        vec4 sum{0.0f};
                        float weightSum = 0.0f;
                        for (integer dv = -radius; dv <= radius; ++dv) {
                            for (integer du = -radius; du <= radius; ++du) {
                                const vec4 neighbor = sample(source, diamond, iu + du, iv + dv);
                                const float space = std::exp(-float(du * du + dv * dv) * spaceScale);
                                const float color = std::exp(-glm::dot(vec3(neighbor - center), vec3(neighbor - center)) * colorScale);
                                const float weight = space * color;
                                sum += neighbor * weight;
                                weightSum += weight;
                            }
                        }
                        map.at(IcosaPack::Slot{.diamond = diamond, .iu = iu, .iv = iv}) = sum / std::max(weightSum, 1.0e-6f);
                    }
                }
            }
        }

        void generateFarAlbedo(planet::Planet& planet) {
            const integer sourceLast = planet.heights.pack.edgeSegments();
            const integer targetLast = planet.farAlbedo.pack.edgeSegments();
            const integer sampleWidth = std::max(sourceLast / std::max(targetLast, integer{1}), integer{1});
            for (integer index = 0; index < planet.farAlbedo.pack.storedCount(); ++index) {
                const auto target = planet.farAlbedo.pack.slotOf(index);
                const integer centerU = static_cast<integer>(std::lround(double(target.iu) * double(sourceLast) / double(targetLast)));
                const integer centerV = static_cast<integer>(std::lround(double(target.iv) * double(sourceLast) / double(targetLast)));
                vec4 sum{0.0f};
                integer samples = 0;
                for (integer iv = 0; iv < sampleWidth; ++iv) {
                    for (integer iu = 0; iu < sampleWidth; ++iu) {
                        const integer sourceU = std::clamp(centerU + iu - sampleWidth / 2, integer{0}, sourceLast);
                        const integer sourceV = std::clamp(centerV + iv - sampleWidth / 2, integer{0}, sourceLast);
                        sum += meanSurface(planet, IcosaPack::Slot{.diamond = target.diamond, .iu = sourceU, .iv = sourceV});
                        samples += 1;
                    }
                }
                planet.farAlbedo.at(target) = sum / float(std::max(samples, integer{1}));
            }
            planet.farAlbedo.stitch();
            blurFarAlbedo(planet);
            planet.farAlbedo.stitch();
            writeFarAlbedo(planet);
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
        const std::size_t coverBytes = static_cast<std::size_t>(stored) * sizeof(std::uint16_t);
        const std::size_t farBytes = static_cast<std::size_t>(planet.farAlbedo.pack.storedCount()) * 4u;
        const std::size_t fieldBytes = heightBytes + coverBytes + farBytes;
        const double fieldMiB = double(fieldBytes) / (1024.0 * 1024.0);
        base::message("eltanin::geo::generate: icosa edgeBase={} tessellation={} → {} segments/edge, {} verts/diamond side, {:.1f} m/texel (R={:.0f} m, arc={:.0f} m)", pack.edgeBase, pack.tessellation, segments, span, texelMeters, planet.passport.radius, arc);
        base::message("eltanin::geo::generate: field matrices {} stored slots ({} unique), diamond {}×{}, atlas {}×{}, 10 layers → heights {} B, cover {} B, far {} B, total {} B ({:.2f} MiB)", stored, unique, span, span, atlas.x, atlas.y, heightBytes, coverBytes, farBytes, fieldBytes, fieldMiB);
    }

    void generate(planet::Planet& planet) {
        generateHeights(planet);
        generateSurfaceWeights(planet);
        generateFarAlbedo(planet);
        logFieldSummary(planet);
    }

}
