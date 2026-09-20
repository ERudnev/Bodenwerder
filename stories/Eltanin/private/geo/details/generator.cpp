#include "geo/celestial/generator.h"
#include "geo/details/compose.h"

#include <base/logging.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <stb_image.h>
#include <stb_image_write.h>

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

        auto fromByte(std::uint8_t value) -> float {
            return float(value) / 255.0f;
        }

        constexpr std::uint32_t cacheEpoch = 7;
        constexpr char cacheMagic[8] = {'E', 'L', 'T', 'N', 'M', 'A', 'P', '7'};

#pragma pack(push, 1)
        struct MapHeader {
            char magic[8];
            std::uint32_t epoch;
            std::int32_t seed;
            std::int32_t edgeBase;
            std::int32_t tessellation;
            std::int32_t heightCount;
            std::int32_t farCount;
            std::uint64_t bulk;
            std::uint32_t volatiles;
            float ageGyr;
            double mass;
            float radius;
            float spinAxisX;
            float spinAxisY;
            float spinAxisZ;
            float spinPeriod;
            float stellarFlux;
            float eccentricity;
            float tidalHeat;
            float debrisFlux;
            float surfaceAcceleration;
            float reliefAmplitude;
            float atmosphereOuterRadius;
            float atmosphereSeaDensity;
            float atmosphereKerman;
            float atmosphereZenithTau;
            float atmosphereDayR;
            float atmosphereDayG;
            float atmosphereDayB;
        };
#pragma pack(pop)

        struct CacheFiles {
            std::filesystem::path directory;
            std::filesystem::path map;
            std::filesystem::path view;
        };

        auto mixHash(std::uint64_t hash, std::uint64_t value) -> std::uint64_t {
            hash ^= value;
            hash *= 1099511628211ull;
            return hash;
        }

        auto mixFloat(std::uint64_t hash, float value) -> std::uint64_t {
            std::uint32_t bits = 0;
            std::memcpy(&bits, &value, sizeof(bits));
            return mixHash(hash, bits);
        }

        auto mixDouble(std::uint64_t hash, double value) -> std::uint64_t {
            std::uint64_t bits = 0;
            std::memcpy(&bits, &value, sizeof(bits));
            return mixHash(hash, bits);
        }

        auto cacheKey(const Planet& planet) -> std::uint64_t {
            const Passport& passport = planet.passport;
            std::uint64_t hash = 14695981039346656037ull;
            hash = mixHash(hash, cacheEpoch);
            hash = mixHash(hash, static_cast<std::uint64_t>(static_cast<std::uint32_t>(passport.seed)));
            hash = mixHash(hash, static_cast<std::uint64_t>(static_cast<std::uint32_t>(planet.heights.pack.edgeBase)));
            hash = mixHash(hash, static_cast<std::uint64_t>(static_cast<std::uint32_t>(planet.heights.pack.tessellation)));
            hash = mixHash(hash, passport.bulk);
            hash = mixHash(hash, passport.volatiles);
            hash = mixFloat(hash, passport.ageGyr);
            hash = mixDouble(hash, passport.mass);
            hash = mixFloat(hash, passport.radius);
            hash = mixFloat(hash, passport.spin.axis.x);
            hash = mixFloat(hash, passport.spin.axis.y);
            hash = mixFloat(hash, passport.spin.axis.z);
            hash = mixFloat(hash, passport.spin.period);
            hash = mixFloat(hash, passport.environment.stellarFlux);
            hash = mixFloat(hash, passport.environment.eccentricity);
            hash = mixFloat(hash, passport.environment.tidalHeat);
            hash = mixFloat(hash, passport.environment.debrisFlux);
            return hash;
        }

        auto hashStem(std::uint64_t hash) -> std::string {
            constexpr char alphabet[] = "0123456789abcdefghjkmnpqrstvwxyz";
            std::string stem(12, '0');
            for (integer index = 11; index >= 0; --index) {
                stem[static_cast<std::size_t>(index)] = alphabet[hash & 31u];
                hash >>= 5;
            }
            return stem;
        }

        auto cacheFiles(const Planet& planet) -> CacheFiles {
            const integer kilometres = std::max(integer{1}, static_cast<integer>(std::lround(double(planet.passport.radius) / 1000.0)));
            const std::string key = std::to_string(kilometres) + "_" + hashStem(cacheKey(planet));
            CacheFiles files;
            files.directory = std::filesystem::path{DAQL_ASSETS_DIR} / "Eltanin" / "planetsCache";
            files.map = files.directory / ("planet_map_" + key + ".bin");
            files.view = files.directory / ("planet_view_" + key + ".png");
            return files;
        }

        auto makeHeader(const Planet& planet) -> MapHeader {
            const Passport& passport = planet.passport;
            MapHeader header;
            std::memcpy(header.magic, cacheMagic, sizeof(header.magic));
            header.epoch = cacheEpoch;
            header.seed = passport.seed;
            header.edgeBase = planet.heights.pack.edgeBase;
            header.tessellation = planet.heights.pack.tessellation;
            header.heightCount = planet.heights.pack.storedCount();
            header.farCount = planet.farAlbedo.pack.storedCount();
            header.bulk = passport.bulk;
            header.volatiles = passport.volatiles;
            header.ageGyr = passport.ageGyr;
            header.mass = passport.mass;
            header.radius = passport.radius;
            header.spinAxisX = passport.spin.axis.x;
            header.spinAxisY = passport.spin.axis.y;
            header.spinAxisZ = passport.spin.axis.z;
            header.spinPeriod = passport.spin.period;
            header.stellarFlux = passport.environment.stellarFlux;
            header.eccentricity = passport.environment.eccentricity;
            header.tidalHeat = passport.environment.tidalHeat;
            header.debrisFlux = passport.environment.debrisFlux;
            header.surfaceAcceleration = planet.runtime.surfaceAcceleration;
            header.reliefAmplitude = planet.runtime.reliefAmplitude;
            header.atmosphereOuterRadius = planet.runtime.atmosphere.outerRadius;
            header.atmosphereSeaDensity = planet.runtime.atmosphere.seaDensity;
            header.atmosphereKerman = planet.runtime.atmosphere.kerman;
            header.atmosphereZenithTau = planet.runtime.atmosphere.zenithTau;
            header.atmosphereDayR = planet.runtime.atmosphere.day.x;
            header.atmosphereDayG = planet.runtime.atmosphere.day.y;
            header.atmosphereDayB = planet.runtime.atmosphere.day.z;
            return header;
        }

        auto headerMatches(const MapHeader& header, const Planet& planet) -> bool {
            const MapHeader expected = makeHeader(planet);
            return std::memcmp(&header, &expected, offsetof(MapHeader, surfaceAcceleration)) == 0;
        }

        auto packView(const Planet& planet) -> vector<std::uint8_t> {
            const auto& map = planet.farAlbedo;
            const index2 atlasSize = map.pack.atlasSize();
            vector<std::uint8_t> pixels(static_cast<std::size_t>(atlasSize.x * atlasSize.y) * 4u, std::uint8_t{0});
            for (integer index = 0; index < map.pack.storedCount(); ++index) {
                const auto slot = map.pack.slotOf(index);
                const index2 coord = map.pack.atlasCoord(slot);
                const vec4 color = map.at(slot);
                const std::size_t pixel = static_cast<std::size_t>(coord.y * atlasSize.x + coord.x) * 4u;
                pixels[pixel] = toByte(color.x);
                pixels[pixel + 1] = toByte(color.y);
                pixels[pixel + 2] = toByte(color.z);
                pixels[pixel + 3] = toByte(color.w);
            }
            return pixels;
        }

        auto unpackView(Planet& planet, const std::uint8_t* pixels, integer width, integer height) -> bool {
            const index2 atlasSize = planet.farAlbedo.pack.atlasSize();
            if (width != atlasSize.x or height != atlasSize.y)
                return false;
            for (integer index = 0; index < planet.farAlbedo.pack.storedCount(); ++index) {
                const auto slot = planet.farAlbedo.pack.slotOf(index);
                const index2 coord = planet.farAlbedo.pack.atlasCoord(slot);
                const std::size_t pixel = static_cast<std::size_t>(coord.y * atlasSize.x + coord.x) * 4u;
                planet.farAlbedo.at(slot) = vec4{fromByte(pixels[pixel]), fromByte(pixels[pixel + 1]), fromByte(pixels[pixel + 2]), fromByte(pixels[pixel + 3])};
            }
            planet.farAlbedo.stitch();
            return true;
        }

        auto loadCache(Planet& planet, const CacheFiles& files) -> bool {
            if (not std::filesystem::exists(files.map) or not std::filesystem::exists(files.view))
                return false;
            std::ifstream input{files.map, std::ios::binary};
            MapHeader header;
            input.read(reinterpret_cast<char*>(&header), static_cast<std::streamsize>(sizeof(header)));
            if (not input or not headerMatches(header, planet))
                return false;
            planet.runtime.surfaceAcceleration = header.surfaceAcceleration;
            planet.runtime.reliefAmplitude = header.reliefAmplitude;
            planet.runtime.atmosphere.outerRadius = header.atmosphereOuterRadius;
            planet.runtime.atmosphere.seaDensity = header.atmosphereSeaDensity;
            planet.runtime.atmosphere.kerman = header.atmosphereKerman;
            planet.runtime.atmosphere.zenithTau = header.atmosphereZenithTau;
            planet.runtime.atmosphere.day = RGB{header.atmosphereDayR, header.atmosphereDayG, header.atmosphereDayB};
            const std::size_t heightBytes = planet.heights.values.size() * sizeof(std::int16_t);
            const std::size_t coverBytes = planet.covers.values.size() * sizeof(std::uint16_t);
            input.read(reinterpret_cast<char*>(planet.heights.values.data()), static_cast<std::streamsize>(heightBytes));
            input.read(reinterpret_cast<char*>(planet.covers.values.data()), static_cast<std::streamsize>(coverBytes));
            if (not input)
                return false;
            int width = 0;
            int height = 0;
            int components = 0;
            stbi_uc* pixels = stbi_load(files.view.string().c_str(), &width, &height, &components, STBI_rgb_alpha);
            if (pixels == nullptr)
                return false;
            const bool unpacked = unpackView(planet, pixels, width, height);
            stbi_image_free(pixels);
            if (not unpacked)
                return false;
            planet.heights.stitch();
            planet.covers.stitch();
            base::message("eltanin::planet::Generator: cache hit {} + {}", files.map.string(), files.view.string());
            return true;
        }

        void saveCache(const Planet& planet, const CacheFiles& files) {
            std::error_code error;
            std::filesystem::create_directories(files.directory, error);
            if (error) {
                base::warning("eltanin::planet::Generator: cannot create '{}': {}", files.directory.string(), error.message());
                return;
            }
            const MapHeader header = makeHeader(planet);
            std::ofstream output{files.map, std::ios::binary | std::ios::trunc};
            output.write(reinterpret_cast<const char*>(&header), static_cast<std::streamsize>(sizeof(header)));
            output.write(reinterpret_cast<const char*>(planet.heights.values.data()), static_cast<std::streamsize>(planet.heights.values.size() * sizeof(std::int16_t)));
            output.write(reinterpret_cast<const char*>(planet.covers.values.data()), static_cast<std::streamsize>(planet.covers.values.size() * sizeof(std::uint16_t)));
            if (not output) {
                base::warning("eltanin::planet::Generator: cannot write '{}'", files.map.string());
                return;
            }
            output.close();
            const vector<std::uint8_t> pixels = packView(planet);
            const index2 atlasSize = planet.farAlbedo.pack.atlasSize();
            if (stbi_write_png(files.view.string().c_str(), atlasSize.x, atlasSize.y, 4, pixels.data(), atlasSize.x * 4) == 0) {
                base::warning("eltanin::planet::Generator: cannot write '{}'", files.view.string());
                return;
            }
            base::message("eltanin::planet::Generator: cache bake {} + {}", files.map.string(), files.view.string());
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
        const CacheFiles files = cacheFiles(planet);
        if (loadCache(planet, files)) {
            generateFarNormal(planet);
            logFieldSummary(planet);
            const Geology geology = Compose::derive(planet.passport);
            planet.weather = Weather::spawn(geology, planet.heights.pack.edgeSegments(), planet.runtime.atmosphere.kerman, planet.runtime.atmosphere.seaDensity);
            return;
        }
        const Geology geology = Compose::derive(planet.passport);
        Compose::form(planet, geology);
        generateFarAlbedo(planet);
        generateFarNormal(planet);
        saveCache(planet, files);
        logFieldSummary(planet);
        planet.weather = Weather::spawn(geology, planet.heights.pack.edgeSegments(), planet.runtime.atmosphere.kerman, planet.runtime.atmosphere.seaDensity);
    }

}
