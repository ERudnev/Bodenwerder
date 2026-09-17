#include "geo/celestial/generator.h"

#include <base/logging.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <glm/common.hpp>
#include <glm/geometric.hpp>

namespace eltanin::planet {

    using namespace fqsm::api;
    using namespace rmmr;
    using geo::IcosaMap;
    using geo::IcosaPack;

    namespace {

        auto faciesMeans() -> const std::array<vec4, 48>& {
            static const std::array<vec4, 48> means{
                vec4{0.799f, 0.818f, 0.825f, 0.78f}, // Snow
                vec4{0.344f, 0.442f, 0.401f, 0.42f}, // Glacier
                vec4{0.521f, 0.571f, 0.542f, 0.76f}, // DirtyIce
                vec4{0.818f, 0.850f, 0.895f, 0.48f}, // VolatileFrost
                vec4{0.323f, 0.383f, 0.370f, 0.72f}, // Hydrate
                vec4{0.350f, 0.352f, 0.289f, 0.72f}, // Dunite
                vec4{0.396f, 0.448f, 0.296f, 0.76f}, // Peridotite
                vec4{0.393f, 0.294f, 0.242f, 0.78f}, // Pyroxenite
                vec4{0.091f, 0.169f, 0.129f, 0.66f}, // Komatiite
                vec4{0.228f, 0.210f, 0.177f, 0.82f}, // Basalt
                vec4{0.357f, 0.350f, 0.312f, 0.76f}, // Gabbro
                vec4{0.516f, 0.493f, 0.471f, 0.74f}, // Andesite
                vec4{0.662f, 0.587f, 0.521f, 0.70f}, // Granite
                vec4{0.377f, 0.360f, 0.324f, 0.68f}, // Rhyolite
                vec4{0.114f, 0.100f, 0.124f, 0.22f}, // Obsidian
                vec4{0.333f, 0.336f, 0.363f, 0.66f}, // Anorthosite
                vec4{0.709f, 0.466f, 0.266f, 0.88f}, // ClayPan
                vec4{0.704f, 0.325f, 0.113f, 0.84f}, // Laterite
                vec4{0.611f, 0.544f, 0.430f, 0.84f}, // Arenite
                vec4{0.806f, 0.795f, 0.784f, 0.78f}, // Evaporite
                vec4{0.843f, 0.814f, 0.743f, 0.62f}, // Carbonate
                vec4{0.308f, 0.314f, 0.311f, 0.86f}, // Chondrite
                vec4{0.781f, 0.223f, 0.044f, 0.72f}, // Tholin
                vec4{0.111f, 0.085f, 0.076f, 0.55f}, // Bitumen
                vec4{0.583f, 0.289f, 0.137f, 0.38f}, // IronMetal
                vec4{0.587f, 0.600f, 0.614f, 0.34f}, // NickelMetal
                vec4{0.671f, 0.665f, 0.590f, 0.48f}, // Sulfide
                vec4{0.383f, 0.330f, 0.000f, 0.82f}, // SulfurPlains
                vec4{0.875f, 0.882f, 0.894f, 0.40f}, // SO2Frost
                vec4{0.571f, 0.496f, 0.178f, 0.78f}, // Fumarole
                vec4{0.494f, 0.241f, 0.102f, 0.82f}, // Hematite
                vec4{0.147f, 0.146f, 0.154f, 0.48f}, // Magnetite
                vec4{0.287f, 0.147f, 0.065f, 0.72f}, // DesertVarnish
                vec4{0.457f, 0.346f, 0.284f, 0.38f}, // BaseMetal
                vec4{0.386f, 0.384f, 0.259f, 0.68f}, // Porphyry
                vec4{0.604f, 0.613f, 0.626f, 0.34f}, // PGMLag
                vec4{0.729f, 0.734f, 0.719f, 0.82f}, // REELaterite
                vec4{0.356f, 0.351f, 0.317f, 0.76f}, // Actinide
                vec4{0.378f, 0.286f, 0.304f, 0.58f}, // Pahoehoe
                vec4{0.269f, 0.235f, 0.273f, 0.88f}, // Scoria
                vec4{0.320f, 0.317f, 0.314f, 0.92f}, // Pumice
                vec4{0.490f, 0.499f, 0.525f, 0.72f}, // SilicaSinter
                vec4{0.321f, 0.235f, 0.191f, 0.92f}, // RegolithMafic
                vec4{0.383f, 0.274f, 0.208f, 0.90f}, // RegolithFelsic
                vec4{0.525f, 0.345f, 0.212f, 0.82f}, // Breccia
                vec4{0.708f, 0.691f, 0.672f, 0.62f}, // Pegmatite
                vec4{0.299f, 0.273f, 0.243f, 0.44f}, // Exotic
                vec4{0.852f, 0.703f, 0.485f, 0.86f}, // Caliche
            };
            return means;
        }

        auto surfacePoint(const Planet& planet, IcosaPack::Slot slot) -> vec3 {
            const integer last = planet.heights.pack.edgeSegments();
            slot.iu = std::clamp(slot.iu, integer{0}, last);
            slot.iv = std::clamp(slot.iv, integer{0}, last);
            const vec3 dir = planet.heights.pack.direction(slot);
            return dir * float(planet.surfaceRadius(planet.heights.at(slot)));
        }

        auto meanSurface(const Planet& planet, IcosaPack::Slot slot) -> vec4 {
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

        auto heightQuantum(const Planet& planet, integer diamond, integer iu, integer iv) -> float {
            const integer last = planet.heights.pack.edgeSegments();
            return float(planet.heights.at(IcosaPack::Slot{.diamond = diamond, .iu = std::clamp(iu, integer{0}, last), .iv = std::clamp(iv, integer{0}, last)}));
        }

        auto reliefGain(const Planet& planet, integer diamond, integer iu, integer iv) -> float {
            const float height = heightQuantum(planet, diamond, iu, iv);
            float mean = 0.0f;
            float spread = 1.0f;
            for (integer dv = -2; dv <= 2; ++dv) {
                for (integer du = -2; du <= 2; ++du) {
                    const float neighbor = heightQuantum(planet, diamond, iu + du, iv + dv);
                    mean += neighbor;
                    spread = std::max(spread, std::abs(height - neighbor));
                }
            }
            mean /= 25.0f;
            return 1.0f + glm::clamp((height - mean) / spread, -0.38f, 0.22f);
        }

        auto toByte(float value) -> std::uint8_t {
            return static_cast<std::uint8_t>(std::lround(glm::clamp(value, 0.0f, 1.0f) * 255.0f));
        }

        void writeFarAlbedo(const Planet& planet) {
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
                base::warning("eltanin::planet::Generator: cannot create '{}': {}", directory.string(), error.message());
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
                base::warning("eltanin::planet::Generator: cannot write '{}'", file.string());
            else
                base::message("eltanin::planet::Generator: far albedo cache → {}", file.string());
        }

        void blurFarAlbedo(Planet& planet) {
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

        void generateFarAlbedo(Planet& planet) {
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
            for (integer index = 0; index < planet.farAlbedo.pack.storedCount(); ++index) {
                const auto target = planet.farAlbedo.pack.slotOf(index);
                const integer centerU = static_cast<integer>(std::lround(double(target.iu) * double(sourceLast) / double(targetLast)));
                const integer centerV = static_cast<integer>(std::lround(double(target.iv) * double(sourceLast) / double(targetLast)));
                const float gain = reliefGain(planet, target.diamond, centerU, centerV);
                vec4 color = planet.farAlbedo.at(target);
                planet.farAlbedo.at(target) = vec4{color.x * gain, color.y * gain, color.z * gain, color.w};
            }
            planet.farAlbedo.stitch();
            writeFarAlbedo(planet);
        }

        auto reliefNormal(const Planet& planet, vec3 dir) -> vec3 {
            dir = glm::normalize(dir);
            vec3 tangentU = glm::cross(vec3{0.0f, 1.0f, 0.0f}, dir);
            if (glm::dot(tangentU, tangentU) < 1.0e-8f)
                tangentU = glm::cross(vec3{1.0f, 0.0f, 0.0f}, dir);
            tangentU = glm::normalize(tangentU);
            const vec3 tangentV = glm::cross(dir, tangentU);
            const float eps = 1.0f / float(std::max(planet.farAlbedo.pack.edgeSegments(), integer{1}));
            auto surface = [&](vec3 sample) -> vec3 {
                sample = glm::normalize(sample);
                return sample * float(planet.height(sample));
            };
            vec3 normal = glm::cross(surface(dir + tangentU * eps) - surface(dir - tangentU * eps), surface(dir + tangentV * eps) - surface(dir - tangentV * eps));
            const float magnitude = glm::length(normal);
            if (magnitude < 1.0e-8f)
                return dir;
            normal /= magnitude;
            if (glm::dot(normal, dir) < 0.0f)
                normal = -normal;
            const vec3 tilt = normal - dir * glm::dot(normal, dir);
            return glm::normalize(dir + tilt * 2.0f);
        }

        void blurFarNormal(Planet& planet) {
            auto& map = planet.farNormal;
            const integer last = map.pack.edgeSegments();
            constexpr float sigmaSpace = 1.0f;
            const integer radius = 2;
            auto sample = [&](const vector<vec3>& field, integer diamond, integer iu, integer iv) -> vec3 {
                return field[static_cast<std::size_t>(map.pack.index(IcosaPack::Slot{.diamond = diamond, .iu = std::clamp(iu, integer{0}, last), .iv = std::clamp(iv, integer{0}, last)}))];
            };
            const vector<vec3> source = map.values;
            const float spaceScale = 1.0f / (2.0f * sigmaSpace * sigmaSpace);
            for (integer diamond = 0; diamond < IcosaPack::diamondCount; ++diamond) {
                for (integer iv = 0; iv <= last; ++iv) {
                    for (integer iu = 0; iu <= last; ++iu) {
                        vec3 sum{0.0f};
                        float weightSum = 0.0f;
                        for (integer dv = -radius; dv <= radius; ++dv) {
                            for (integer du = -radius; du <= radius; ++du) {
                                const float weight = std::exp(-float(du * du + dv * dv) * spaceScale);
                                sum += sample(source, diamond, iu + du, iv + dv) * weight;
                                weightSum += weight;
                            }
                        }
                        const vec3 blurred = sum / std::max(weightSum, 1.0e-6f);
                        const float magnitude = glm::length(blurred);
                        map.at(IcosaPack::Slot{.diamond = diamond, .iu = iu, .iv = iv}) = magnitude < 1.0e-8f ? sample(source, diamond, iu, iv) : blurred / magnitude;
                    }
                }
            }
        }

        void generateFarNormal(Planet& planet) {
            for (integer index = 0; index < planet.farNormal.pack.storedCount(); ++index) {
                const auto slot = planet.farNormal.pack.slotOf(index);
                planet.farNormal.at(slot) = reliefNormal(planet, planet.farNormal.pack.direction(slot));
            }
            planet.farNormal.stitch();
            blurFarNormal(planet);
            planet.farNormal.stitch();
            for (auto& normal : planet.farNormal.values) {
                const float magnitude = glm::length(normal);
                if (magnitude > 1.0e-8f)
                    normal /= magnitude;
            }
        }

    }

    void Generator::generateSurfaceWeights(Planet& planet) {
        paintCover(planet, marsCover(planet.passport));
    }

    void logFieldSummary(const Planet& planet) {
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
        const std::size_t farNormalBytes = static_cast<std::size_t>(planet.farNormal.pack.storedCount()) * 4u;
        const std::size_t fieldBytes = heightBytes + coverBytes + farBytes + farNormalBytes;
        const double fieldMiB = double(fieldBytes) / (1024.0 * 1024.0);
        base::message("eltanin::planet::Generator: icosa edgeBase={} tessellation={} → {} segments/edge, {} verts/diamond side, {:.1f} m/texel (R={:.0f} m, arc={:.0f} m)", pack.edgeBase, pack.tessellation, segments, span, texelMeters, planet.passport.radius, arc);
        base::message("eltanin::planet::Generator: field matrices {} stored slots ({} unique), diamond {}×{}, atlas {}×{}, 10 layers → heights {} B, cover {} B, far {} B, farN {} B, total {} B ({:.2f} MiB)", stored, unique, span, span, atlas.x, atlas.y, heightBytes, coverBytes, farBytes, farNormalBytes, fieldBytes, fieldMiB);
    }

    void Generator::generate(Planet& planet) {
        mars(planet);
        generateFarAlbedo(planet);
        generateFarNormal(planet);
        logFieldSummary(planet);
    }

}
