#include "geo/celestial/planetiod.h"
#include "geo/celestial/horizon.h"
#include "physics/settings.h"

#include <eltanin/locality/thing.q1.h>
#include <eltanin/physics/body.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/resources/texpack.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/node.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/semantics/geometry.h>

#include <base/logging.h>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <numbers>
#include <optional>
#include <string>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

namespace eltanin::locality::geo {

    using namespace fqsm::api;
    using namespace rmmr;

    using Mix = std::uint64_t;

    struct TerrainAsync {
        enum class Lifecycle : std::uint8_t { pending, generating, completed, resident };

        struct Desired {
            Landscape::PatchKey key;
            std::uint8_t coarserEdges;
            bool wireframe;
            bool residentMatches;
            double priority;
        };

        struct Completed {
            Landscape::PatchKey key;
            std::uint8_t coarserEdges;
            bool wireframe;
            resource::builders::geometry::CpuPresentation cpu;
        };

        struct Snapshot {
            std::uint32_t pending = 0;
            std::uint32_t generating = 0;
            std::uint32_t completed = 0;
            std::uint32_t resident = 0;
            std::uint64_t startedTotal = 0;
            std::uint64_t committedTotal = 0;
            std::uint64_t discardedTotal = 0;
            double latencyAverageMs = 0.0;
            double latencyP95Ms = 0.0;
            double latencyMaxMs = 0.0;
        };

        explicit TerrainAsync(Landscape::Look look);
        ~TerrainAsync();
        TerrainAsync(const TerrainAsync&) = delete;
        auto operator=(const TerrainAsync&) -> TerrainAsync& = delete;

        void reconcile(const std::vector<Desired>& desired);
        auto takeCompleted(std::size_t limit) -> std::vector<Completed>;
        void markResident(const Landscape::PatchKey& key, std::uint8_t coarserEdges, bool wireframe);
        void noteCommitted();
        auto takeStartedSinceFrame() -> std::uint32_t;
        auto snapshot() const -> Snapshot;
        void invalidate(Landscape::Look look);

    private:
        struct State;
        std::unique_ptr<State> state;
    };

    namespace {

        auto terrainAsyncState() -> std::shared_ptr<TerrainAsync>& {
            static std::shared_ptr<TerrainAsync> state;
            return state;
        }

        constexpr int mixChannels = 16;
        constexpr int grid = 33;
        constexpr int cells = 32;
        constexpr int faceCount = 6;
        constexpr int maxLevel = 5;
        constexpr float craterDepthMultiplier = 1.5f;
        constexpr float rareBasinDepthMultiplier = 3.0f;
        constexpr float splitNear = 2.8f;
        constexpr float splitKeep = 3.7f;
        constexpr int mineralIce = 0;
        constexpr int mineralOlivine = 1;
        constexpr int mineralPyroxene = 2;
        constexpr int mineralIron = 6;
        // Fixed demo palette for Surface.mix packing (same order as planetoid.frag).
        constexpr int planetPalette[4] = {mineralIce, mineralOlivine, mineralPyroxene, mineralIron};
        constexpr bool flipWinding[faceCount] = {false, true, true, false, false, true};

        using PatchKey = Landscape::PatchKey;
        using PatchKeyHash = Landscape::PatchKeyHash;
        using Patch = Landscape::Patch;
        using PatchMap = std::unordered_map<PatchKey, Patch, PatchKeyHash>;

        auto terrainStagingPatches() -> PatchMap& {
            static PatchMap patches;
            return patches;
        }

        auto hash31(int x, int y, int z, int seed) -> float {
            auto mix = [](std::uint32_t value) -> std::uint32_t {
                value ^= value >> 16;
                value *= 0x7feb352du;
                value ^= value >> 15;
                value *= 0x846ca68bu;
                value ^= value >> 16;
                return value;
            };
            const std::uint32_t hashed = mix(std::uint32_t(x) * 73856093u ^ std::uint32_t(y) * 19349663u ^ std::uint32_t(z) * 83492791u ^ std::uint32_t(seed) * 2654435761u);
            return float(hashed >> 8) * (1.0f / 16777215.0f);
        }

        auto valueNoise(float x, float y, float z, int seed) -> float {
            const int x0 = static_cast<int>(std::floor(x));
            const int y0 = static_cast<int>(std::floor(y));
            const int z0 = static_cast<int>(std::floor(z));
            const float tx = x - static_cast<float>(x0);
            const float ty = y - static_cast<float>(y0);
            const float tz = z - static_cast<float>(z0);
            const float sx = tx * tx * (3.0f - 2.0f * tx);
            const float sy = ty * ty * (3.0f - 2.0f * ty);
            const float sz = tz * tz * (3.0f - 2.0f * tz);
            const float c000 = hash31(x0, y0, z0, seed);
            const float c100 = hash31(x0 + 1, y0, z0, seed);
            const float c010 = hash31(x0, y0 + 1, z0, seed);
            const float c110 = hash31(x0 + 1, y0 + 1, z0, seed);
            const float c001 = hash31(x0, y0, z0 + 1, seed);
            const float c101 = hash31(x0 + 1, y0, z0 + 1, seed);
            const float c011 = hash31(x0, y0 + 1, z0 + 1, seed);
            const float c111 = hash31(x0 + 1, y0 + 1, z0 + 1, seed);
            const float c00 = c000 + (c100 - c000) * sx;
            const float c10 = c010 + (c110 - c010) * sx;
            const float c01 = c001 + (c101 - c001) * sx;
            const float c11 = c011 + (c111 - c011) * sx;
            const float c0 = c00 + (c10 - c00) * sy;
            const float c1 = c01 + (c11 - c01) * sy;
            return c0 + (c1 - c0) * sz;
        }

        auto signedNoise(vec3 point, int seed) -> float {
            return valueNoise(point.x, point.y, point.z, seed) * 2.0f - 1.0f;
        }

        auto fbm(vec3 point, int seed) -> float {
            float sum = 0.0f;
            float amp = 0.5f;
            vec3 scaled = point;
            for (int octave = 0; octave < 4; ++octave) {
                sum += amp * signedNoise(scaled, seed + octave * 19);
                scaled *= 2.07f;
                amp *= 0.5f;
            }
            return sum;
        }

        auto cubePoint(int face, float faceU, float faceV) -> vec3 {
            switch (face) {
                case 0: return vec3{1.0f, faceU, faceV};
                case 1: return vec3{-1.0f, faceU, faceV};
                case 2: return vec3{faceU, 1.0f, faceV};
                case 3: return vec3{faceU, -1.0f, faceV};
                case 4: return vec3{faceU, faceV, 1.0f};
                default: return vec3{faceU, faceV, -1.0f};
            }
        }

        auto cubeDir(int face, float faceU, float faceV) -> vec3 {
            return glm::normalize(cubePoint(face, faceU, faceV));
        }

        constexpr float craterRadiusMax = 0.16f;
        constexpr float craterSupport = 1.55f;
        constexpr float contourBroad = 0.04f;
        constexpr float contourFine = 0.015f;
        constexpr float sculptedBroad = 0.12f;
        constexpr float sculptedFine = 0.035f;
        constexpr float sculptedReach = craterSupport * (1.0f + sculptedBroad + sculptedFine);
        // Catalog craters always trim the largest .155 radial lobe by .035.
        // The trim is monotone, so 1.12 bounds its largest radius (plus roundoff).
        constexpr float craterReach = craterSupport * (1.0f + sculptedBroad + 0.000001f);
        constexpr int craterCells = 10;
        constexpr int craterCount = faceCount * craterCells * craterCells;
        constexpr float craterCellSize = 2.0f / craterCells;
        auto craterRadius(int face, int iu, int iv, int seed) -> float {
            constexpr float radiusMin = 0.015f;
            constexpr float alpha = 2.10f;
            const float u = hash31(face, iu, iv, seed + 4);
            const float minPow = std::pow(radiusMin, 1.0f - alpha);
            const float maxPow = std::pow(craterRadiusMax, 1.0f - alpha);
            return std::pow(minPow + u * (maxPow - minPow), 1.0f / (1.0f - alpha));
        }

        struct CraterHit {
            float field;
            float meters;
            float cover;
        };

        struct Crater {
            struct Profile {
                float floorRadius;
                float rounding;
                float rimInnerWidth;
                float rimScale;
                float breach;
            };
            struct Shape {
                std::array<vec3, 3> fanDirections;
                float wallScale;
                float talusScale;
            };

            vec3 center;
            float radius;
            float depthUnit;
            float depthScale;
            int noiseSeed;
            Profile profile;
            Shape shape;
        };

        auto craterFanDirections(vec3 center, float phase = 0.0f) -> std::array<vec3, 3>;

        auto craterProfile(float weathering, float floorVariation) -> Crater::Profile {
            // Art-directed erosion, not an impact-age simulation. Parameters are
            // cached per crater; no profile selection or random rolls per vertex.
            return Crater::Profile{
                .floorRadius = glm::mix(0.08f, 0.46f, floorVariation) * (1.0f - 0.30f * weathering),
                .rounding = glm::mix(0.065f, 0.20f, weathering),
                .rimInnerWidth = glm::mix(0.14f, 0.32f, weathering),
                .rimScale = glm::mix(1.0f, 0.15f, weathering),
                .breach = glm::smoothstep(0.20f, 0.85f, weathering),
            };
        }

        constexpr int maxBasinCount = 4;

        struct BasinCatalog {
            std::array<Crater, maxBasinCount> craters;
            int count;
        };

        auto makeBasins(integer seed) -> BasinCatalog {
            const int basinSeed = static_cast<int>(seed) + 1700;
            const int count = 2 + glm::clamp(static_cast<int>(hash31(basinSeed, 0, 0, basinSeed + 1) * 3.0f), 0, maxBasinCount - 2);
            std::array<Crater, maxBasinCount> result;
            for (int index = 0; index < count; ++index) {
                const float rank = count > 1 ? static_cast<float>(index) / static_cast<float>(count - 1) : 0.0f;
                float radius = glm::mix(0.39f, 0.22f, rank) * glm::mix(0.92f, 1.06f, hash31(index, basinSeed, 0, basinSeed + 2));
                if (index > 0)
                    radius = glm::min(radius, result[static_cast<std::size_t>(index - 1)].radius * 0.88f);

                vec3 center{0.0f, 0.0f, 1.0f};
                for (int attempt = 0; attempt < 12; ++attempt) {
                    const float vertical = hash31(index, attempt, basinSeed, basinSeed + 11) * 2.0f - 1.0f;
                    const float azimuth = hash31(index, attempt, basinSeed, basinSeed + 17) * 2.0f * std::numbers::pi_v<float>;
                    const float radial = std::sqrt(glm::max(0.0f, 1.0f - vertical * vertical));
                    center = vec3{radial * std::cos(azimuth), vertical, radial * std::sin(azimuth)};
                    bool separated = true;
                    for (int previous = 0; previous < index; ++previous) {
                        const vec3 offset = center - result[static_cast<std::size_t>(previous)].center;
                        const float required = 0.78f * (radius + result[static_cast<std::size_t>(previous)].radius);
                        if (glm::dot(offset, offset) < required * required) {
                            separated = false;
                            break;
                        }
                    }
                    if (separated)
                        break;
                }

                const float weathering = hash31(index, basinSeed, 1, basinSeed + 23);
                const float phase = hash31(index, basinSeed, 2, basinSeed + 29) * 2.0f * std::numbers::pi_v<float>;
                result[static_cast<std::size_t>(index)] = Crater{
                    .center = center,
                    .radius = radius,
                    .depthUnit = glm::mix(0.86f, 1.02f, 1.0f - weathering),
                    .depthScale = radius * glm::mix(0.074f, 0.098f, hash31(index, basinSeed, 3, basinSeed + 31)) * glm::mix(0.82f, 1.0f, 1.0f - weathering) * rareBasinDepthMultiplier,
                    .noiseSeed = basinSeed + index * 53,
                    .profile = Crater::Profile{
                        .floorRadius = glm::mix(0.27f, 0.40f, hash31(index, basinSeed, 4, basinSeed + 37)),
                        .rounding = glm::mix(0.050f, 0.080f, weathering),
                        .rimInnerWidth = glm::mix(0.18f, 0.27f, weathering),
                        .rimScale = glm::mix(0.88f, 0.52f, weathering),
                        .breach = glm::mix(0.10f, 0.42f, weathering),
                    },
                    .shape = Crater::Shape{
                        .fanDirections = craterFanDirections(center, phase),
                        .wallScale = glm::mix(0.58f, 0.38f, weathering),
                        .talusScale = glm::mix(0.78f, 0.58f, weathering),
                    },
                };
            }
            return BasinCatalog{.craters = result, .count = count};
        }

        constexpr int lineamentTrunkCount = 10;
        constexpr int lineamentBranchCount = 12;
        constexpr int lineamentCount = lineamentTrunkCount + lineamentBranchCount;

        struct Lineament {
            vec3 center;
            vec3 along;
            vec3 across;
            float halfLength;
            float halfWidth;
            float heightScale;
            bool canyon;
        };

        auto makeLineaments(integer seed) -> std::array<Lineament, lineamentCount> {
            const int structureSeed = static_cast<int>(seed) + 2900;
            const BasinCatalog basins = makeBasins(seed);
            std::array<Lineament, lineamentCount> result;
            for (int index = 0; index < lineamentTrunkCount; ++index) {
                const bool canyon = index % 3 == 2;
                const float halfLength = glm::mix(0.18f, 0.42f, hash31(index, structureSeed, 3, structureSeed + 7));
                vec3 center{0.0f, 0.0f, 1.0f};
                vec3 along{1.0f, 0.0f, 0.0f};
                if (canyon) {
                    // Major canyons are impact-linked: start at a generated basin
                    // rim and run away from it. Repeating the largest basin when
                    // fewer than three exist keeps the count deterministic.
                    const int basinIndex = (index / 3) % basins.count;
                    const Crater& basin = basins.craters[static_cast<std::size_t>(basinIndex)];
                    const vec3 reference = std::abs(basin.center.y) < 0.86f ? vec3{0.0f, 1.0f, 0.0f} : vec3{1.0f, 0.0f, 0.0f};
                    const vec3 tangent0 = glm::normalize(glm::cross(reference, basin.center));
                    const vec3 tangent1 = glm::normalize(glm::cross(basin.center, tangent0));
                    const float phase = hash31(index, structureSeed, 2, structureSeed + 5) * 2.0f * std::numbers::pi_v<float>;
                    const vec3 outward = glm::normalize(tangent0 * std::cos(phase) + tangent1 * std::sin(phase));
                    const vec3 junction = glm::normalize(basin.center + outward * basin.radius * 1.08f);
                    center = glm::normalize(junction + outward * halfLength * 0.80f);
                    along = glm::normalize(outward - center * glm::dot(outward, center));
                } else {
                    // Mountain chains predate the visible large impacts. Reject
                    // candidates whose trunk would visibly emerge from a basin.
                    for (int attempt = 0; attempt < 12; ++attempt) {
                        const float vertical = hash31(index, attempt, structureSeed, structureSeed + 1) * 2.0f - 1.0f;
                        const float azimuth = hash31(index, attempt, structureSeed, structureSeed + 3) * 2.0f * std::numbers::pi_v<float>;
                        const float radial = std::sqrt(glm::max(0.0f, 1.0f - vertical * vertical));
                        center = vec3{radial * std::cos(azimuth), vertical, radial * std::sin(azimuth)};
                        bool outsideBasins = true;
                        for (int basinIndex = 0; basinIndex < basins.count; ++basinIndex) {
                            const Crater& basin = basins.craters[static_cast<std::size_t>(basinIndex)];
                            const float required = basin.radius * 1.30f + halfLength;
                            const vec3 offset = center - basin.center;
                            if (glm::dot(offset, offset) < required * required) {
                                outsideBasins = false;
                                break;
                            }
                        }
                        if (outsideBasins)
                            break;
                    }
                    const vec3 reference = std::abs(center.y) < 0.86f ? vec3{0.0f, 1.0f, 0.0f} : vec3{1.0f, 0.0f, 0.0f};
                    const vec3 tangent0 = glm::normalize(glm::cross(reference, center));
                    const vec3 tangent1 = glm::normalize(glm::cross(center, tangent0));
                    const float phase = hash31(index, structureSeed, 2, structureSeed + 5) * 2.0f * std::numbers::pi_v<float>;
                    along = glm::normalize(tangent0 * std::cos(phase) + tangent1 * std::sin(phase));
                }
                const vec3 across = glm::normalize(glm::cross(center, along));
                result[static_cast<std::size_t>(index)] = Lineament{
                    .center = center,
                    .along = along,
                    .across = across,
                    .halfLength = halfLength,
                    .halfWidth = canyon
                        ? glm::mix(0.025f, 0.050f, hash31(index, structureSeed, 4, structureSeed + 11))
                        : glm::mix(0.032f, 0.072f, hash31(index, structureSeed, 4, structureSeed + 11)),
                    .heightScale = canyon
                        ? glm::mix(0.65f, 1.25f, hash31(index, structureSeed, 5, structureSeed + 13))
                        : glm::mix(1.35f, 2.80f, hash31(index, structureSeed, 5, structureSeed + 13)),
                    .canyon = canyon,
                };
            }

            // Branches inherit the type of a trunk and start close to it. Canyon
            // trunks receive two tributaries; six mountain trunks receive one
            // spur. All choices are seed-only and cached with the trunk catalog.
            constexpr std::array<int, lineamentBranchCount> parents{2, 2, 5, 5, 8, 8, 0, 1, 3, 4, 6, 7};
            for (int branchIndex = 0; branchIndex < lineamentBranchCount; ++branchIndex) {
                const int index = lineamentTrunkCount + branchIndex;
                const Lineament& parent = result[static_cast<std::size_t>(parents[static_cast<std::size_t>(branchIndex)])];
                const float joinOffset = glm::mix(-0.56f, 0.56f, hash31(branchIndex, structureSeed, 6, structureSeed + 17)) * parent.halfLength;
                const vec3 junction = glm::normalize(parent.center + parent.along * joinOffset);
                const vec3 parentAlong = glm::normalize(parent.along - junction * glm::dot(parent.along, junction));
                const vec3 parentAcross = glm::normalize(glm::cross(junction, parentAlong));
                const float side = hash31(branchIndex, structureSeed, 7, structureSeed + 19) < 0.5f ? -1.0f : 1.0f;
                const float branchAngle = side * glm::mix(0.48f, 0.92f, hash31(branchIndex, structureSeed, 8, structureSeed + 23));
                const vec3 branchAlongAtJunction = glm::normalize(parentAlong * std::cos(branchAngle) + parentAcross * std::sin(branchAngle));
                const float halfLength = parent.halfLength * glm::mix(0.42f, 0.68f, hash31(branchIndex, structureSeed, 9, structureSeed + 29));
                // Offset the branch centre so one tapered end joins its parent,
                // instead of producing an X-shaped crossing through the trunk.
                const vec3 center = glm::normalize(junction + branchAlongAtJunction * halfLength * 0.52f);
                const vec3 along = glm::normalize(branchAlongAtJunction - center * glm::dot(branchAlongAtJunction, center));
                const vec3 across = glm::normalize(glm::cross(center, along));
                result[static_cast<std::size_t>(index)] = Lineament{
                    .center = center,
                    .along = along,
                    .across = across,
                    .halfLength = halfLength,
                    .halfWidth = parent.halfWidth * glm::mix(0.46f, 0.68f, hash31(branchIndex, structureSeed, 10, structureSeed + 31)),
                    .heightScale = parent.heightScale * glm::mix(0.52f, 0.78f, hash31(branchIndex, structureSeed, 11, structureSeed + 37)),
                    .canyon = parent.canyon,
                };
            }
            return result;
        }

        auto lineamentField(vec3 dir, integer seed, float reliefScale, float detail, vec3 broadBend) -> float {
            struct Cache {
                integer seed;
                std::array<Lineament, lineamentCount> lineaments;
            };
            auto createCache = [](integer seed) {
                return Cache{seed, makeLineaments(seed)};
            };
            thread_local Cache cache = createCache(seed);
            if (cache.seed != seed)
                cache = createCache(seed);

            float height = 0.0f;
            for (const auto& lineament : cache.lineaments) {
                const float front = glm::dot(dir, lineament.center);
                const float local = glm::smoothstep(0.72f, 0.88f, front);
                if (local <= 0.0f)
                    continue;
                const float alongT = std::abs(glm::dot(dir, lineament.along)) / lineament.halfLength;
                const float alongEnvelope = 1.0f - glm::smoothstep(0.68f, 1.0f, alongT);
                if (alongEnvelope <= 0.0f)
                    continue;
                const float taper = glm::mix(0.42f, 1.0f, 1.0f - glm::clamp(alongT * alongT, 0.0f, 1.0f));
                const float effectiveWidth = lineament.halfWidth * taper;
                const float bend = glm::dot(broadBend, lineament.across);
                const float bentAcross = glm::dot(dir, lineament.across) + bend * lineament.halfWidth * 0.42f + detail * lineament.halfWidth * 0.16f;
                const float acrossT = std::abs(bentAcross) / effectiveWidth;
                const float modulation = glm::mix(0.72f, 1.25f, glm::clamp(detail * 0.5f + 0.5f, 0.0f, 1.0f));
                if (lineament.canyon) {
                    const float trench = 1.0f - glm::smoothstep(0.12f, 1.0f, acrossT);
                    const float shoulder = glm::smoothstep(0.72f, 1.02f, acrossT) * (1.0f - glm::smoothstep(1.02f, 1.65f, acrossT));
                    height += (-trench + shoulder * 0.17f) * lineament.heightScale * reliefScale * alongEnvelope * local * modulation;
                } else {
                    const float crest = 1.0f - glm::smoothstep(0.08f, 1.0f, acrossT);
                    const float brokenCrest = glm::mix(0.70f, 1.18f, glm::clamp(detail * 0.5f + 0.5f, 0.0f, 1.0f));
                    height += crest * lineament.heightScale * reliefScale * alongEnvelope * local * brokenCrest;
                }
            }
            return height;
        }

        auto makeCraters(integer seed) -> std::array<Crater, craterCount> {
            constexpr int faceCells = craterCells;
            constexpr float cellSize = craterCellSize;
            constexpr float jitter = 0.28f * cellSize;
            const int craterSeed = static_cast<int>(seed) + 40;
            std::array<Crater, faceCount * faceCells * faceCells> result;
            for (int index = 0; index < static_cast<int>(result.size()); ++index) {
                const int face = index / (faceCells * faceCells);
                const int iu = (index / faceCells) % faceCells;
                const int iv = index % faceCells;
                const int packed = (face << 16) ^ (iu << 8) ^ iv;
                const float jitterU = (hash31(face, iu, iv, craterSeed + 1) * 2.0f - 1.0f) * jitter;
                const float jitterV = (hash31(face, iu, iv, craterSeed + 2) * 2.0f - 1.0f) * jitter;
                const float craterU = -1.0f + (iu + 0.5f) * cellSize + jitterU;
                const float craterV = -1.0f + (iv + 0.5f) * cellSize + jitterV;
                const float radius = craterRadius(face, iu, iv, craterSeed);
                const float roll = hash31(face, iu, iv, craterSeed + 5);
                const float weathering = hash31(face, iu, iv, craterSeed + 6);
                const float depthRetention = glm::mix(1.0f, 0.25f, weathering);
                result[index] = Crater{.center = cubeDir(face, craterU, craterV), .radius = radius, .depthUnit = (0.35f + 0.80f * roll) * depthRetention, .depthScale = (0.20f + 0.20f * roll) * radius * depthRetention / 1.5f, .noiseSeed = craterSeed + packed, .profile = craterProfile(weathering, hash31(face, iu, iv, craterSeed + 7))};
                auto& crater = result[index];
                // Keep a readable floor, but reserve more of the radius for the
                // inner wall. Shorter rounded joins make the wall/floor break
                // visible without introducing a discontinuity in height or slope.
                crater.profile.floorRadius = glm::mix(0.42f, 0.58f, hash31(face, iu, iv, craterSeed + 7)) - 0.05f * weathering;
                crater.profile.rounding = glm::mix(0.060f, 0.105f, weathering);
                const float phase = hash31(face, iu, iv, craterSeed + 8) * 2.0f * std::numbers::pi_v<float>;
                crater.shape = Crater::Shape{
                    .fanDirections = craterFanDirections(crater.center, phase),
                    .wallScale = glm::mix(0.60f, 0.25f, weathering) * glm::mix(0.85f, 1.10f, hash31(face, iu, iv, craterSeed + 9)),
                    .talusScale = glm::mix(0.75f, 1.0f, weathering) * glm::mix(0.65f, 1.0f, hash31(face, iu, iv, craterSeed + 10)),
                };
            }

            // Rare basins are generated from the same seed and replace ordinary
            // slots, retaining the common buckets, LOD, material and physics path.
            const auto basins = makeBasins(seed);
            for (int index = 0; index < basins.count; ++index)
                result[static_cast<std::size_t>(index)] = basins.craters[static_cast<std::size_t>(index)];
            return result;
        }

        auto craterBuckets(const std::array<Crater, craterCount>& craters) -> std::array<vector<std::uint16_t>, craterCount> {
            std::array<vector<std::uint16_t>, craterCount> buckets;
            // On a cube face normalization cannot expand distances. Half the cell
            // diagonal bounds every query direction, also beside cube edges/corners.
            constexpr float cellReach = 0.707107f * craterCellSize;
            for (int cell = 0; cell < craterCount; ++cell) {
                const int face = cell / (craterCells * craterCells);
                const int iu = (cell / craterCells) % craterCells;
                const int iv = cell % craterCells;
                const vec3 center = cubeDir(face, -1.0f + (iu + 0.5f) * craterCellSize, -1.0f + (iv + 0.5f) * craterCellSize);
                for (int index = 0; index < craterCount; ++index) {
                    const vec3 offset = center - craters[index].center;
                    const float reach = cellReach + craters[index].radius * craterReach;
                    if (glm::dot(offset, offset) <= reach * reach)
                        buckets[cell].push_back(static_cast<std::uint16_t>(index));
                }
            }
            return buckets;
        }

        auto craterCell(vec3 dir) -> int {
            const vec3 extent = glm::abs(dir);
            int face;
            vec2 uv;
            if (extent.x >= extent.y and extent.x >= extent.z) {
                face = dir.x >= 0.0f ? 0 : 1;
                uv = vec2{dir.y, dir.z} / extent.x;
            } else if (extent.y >= extent.z) {
                face = dir.y >= 0.0f ? 2 : 3;
                uv = vec2{dir.x, dir.z} / extent.y;
            } else {
                face = dir.z >= 0.0f ? 4 : 5;
                uv = vec2{dir.x, dir.y} / extent.z;
            }
            const int iu = glm::clamp(static_cast<int>((uv.x + 1.0f) / craterCellSize), 0, craterCells - 1);
            const int iv = glm::clamp(static_cast<int>((uv.y + 1.0f) / craterCellSize), 0, craterCells - 1);
            return face * craterCells * craterCells + iu * craterCells + iv;
        }

        struct CraterSample {
            float distance;
            float rimAmp;
            float wallBias;
            float apronScale;
            float talus;
            float terraceOffset;
            float terraceIntegrity;
        };

        auto craterFanDirections(vec3 center, float phase) -> std::array<vec3, 3> {
            const vec3 base = glm::normalize(glm::cross(center, std::abs(center.y) < 0.9f ? vec3{0.0f, 1.0f, 0.0f} : vec3{1.0f, 0.0f, 0.0f}));
            const vec3 axis = base * std::cos(phase) + glm::cross(center, base) * std::sin(phase);
            const vec3 across = glm::cross(center, axis);
            // Cached per preview crater, 120 degrees apart; no per-sample trig.
            return {axis, -0.5f * axis + 0.8660254f * across, -0.5f * axis - 0.8660254f * across};
        }

        auto sampleCrater(vec3 offset, float distanceSquared, const Crater& crater, bool sculpted = false, const std::array<vec3, 3>* fanDirections = nullptr) -> CraterSample {
            // Crater-local coordinates keep feature size proportional to its radius.
            // Reuse the two rim-noise samples for the contour: no extra noise octaves.
            const vec3 craterLocal = offset / crater.radius;
            const vec3 tangentLocal = craterLocal - crater.center * glm::dot(craterLocal, crater.center);
            vec3 local = craterLocal;
            if (sculpted) {
                // Sample sectors, not radial noise: each wall keeps a monotone
                // descent instead of developing isolated bumps. The center is
                // regularized inside the flat floor, where angular detail is hidden.
                local = tangentLocal / std::sqrt(glm::max(glm::dot(tangentLocal, tangentLocal), 0.0001f));
            }
            const float broad = valueNoise(local.x * 1.7f, local.y * 1.7f, local.z * 1.7f, crater.noiseSeed);
            // Sub-300 m bowls do not project enough contour detail to justify a
            // second 3D noise sample. Reusing broad keeps their cost bounded as
            // the catalog becomes denser.
            const float fine = crater.radius < 0.030f
                ? broad
                : valueNoise(local.x * 4.3f, local.y * 4.3f, local.z * 4.3f, crater.noiseSeed + 11);
            const float sector = sculpted ? glm::smoothstep(0.20f, 0.80f, broad) : broad;
            float radiusScale = 1.0f + (sculpted ? sculptedBroad : contourBroad) * (2.0f * sector - 1.0f) + (sculpted ? sculptedFine : contourFine) * (2.0f * fine - 1.0f);
            // Trim only the strongest outward lobes; retain the inward cuts and
            // the large-scale outline rather than blurring the complete crater.
            if (fanDirections) radiusScale -= 0.035f * glm::smoothstep(0.04f, 0.155f, radiusScale - 1.0f);
            // Broad, smoothly bounded gaps reuse the contour noise. At maximum
            // weathering low-noise sectors lose the rim, without moving the floor up.
            const float integrity = 1.0f - crater.profile.breach * (1.0f - glm::smoothstep(0.32f, 0.62f, broad));
            const float wallBias = sculpted ? 0.18f * (2.0f * sector - 1.0f) + 0.04f * (2.0f * fine - 1.0f) : 0.0f;
            const float apronScale = sculpted ? glm::mix(0.55f, 1.0f, sector) : 1.0f;
            const float distance = std::sqrt(distanceSquared) / (crater.radius * radiusScale);
            float talus = 0.0f;
            // Outside the bowl both toe profiles are exactly zero: no fan work.
            if (fanDirections and distance < 1.0f) {
                constexpr std::array<float, 3> edges{0.55f, 0.60f, 0.65f};
                constexpr std::array<float, 3> amplitudes{0.38f, 0.30f, 0.42f};
                for (std::size_t fan = 0; fan < fanDirections->size(); ++fan) {
                    const float alignment = glm::dot(local, (*fanDirections)[fan]);
                    const float weight = glm::smoothstep(edges[fan], 1.0f, alignment);
                    // Supports are narrower than their 120-degree separation,
                    // so max meets at zero without overlapping ridges or creases.
                    talus = glm::max(talus, amplitudes[fan] * weight);
                }
            }
            const float terraceOffset = sculpted ? 0.10f * (broad - 0.5f) + 0.04f * (fine - 0.5f) : 0.0f;
            const float terraceIntegrity = sculpted ? glm::smoothstep(0.24f, 0.72f, broad) * glm::mix(0.65f, 1.0f, fine) : 1.0f;
            return CraterSample{
                .distance = distance,
                .rimAmp = glm::mix(0.5f, 1.0f, broad * 0.72f + fine * 0.28f) * integrity,
                .wallBias = wallBias,
                .apronScale = apronScale,
                .talus = talus,
                .terraceOffset = terraceOffset,
                .terraceIntegrity = terraceIntegrity,
            };
        }

        struct CraterBlend {
            vec2 bowlsSquared;
            vec2 rims;
            float cover;
        };

        auto finishCraters(const CraterBlend& blend) -> CraterHit {
            // A root-sum-square union stays between the deepest bowl and the sum,
            // without max() creases or order-dependent attenuation of excavations.
            const vec2 surface = blend.rims * (1.0f - blend.cover) - glm::sqrt(blend.bowlsSquared);
            return CraterHit{.field = glm::clamp(surface.x, -1.4f, 0.55f), .meters = surface.y, .cover = blend.cover};
        }

        auto craterBowl(float distance, const Crater::Profile& profile) -> float {
            const float floorRadius = profile.floorRadius;
            const float rounding = profile.rounding;
            const float rise = 1.0f - floorRadius - rounding;
            if (distance <= floorRadius) return -1.0f;
            if (distance >= 1.0f) return 0.0f;
            // Integrating smoothstep gives a flat floor, a straight wall and short
            // rounded joins. Both height and slope meet the surrounding ground.
            if (distance < floorRadius + rounding) {
                const float blend = (distance - floorRadius) / rounding;
                return -1.0f + rounding * blend * blend * blend * (1.0f - 0.5f * blend) / rise;
            }
            if (distance > 1.0f - rounding) {
                const float blend = (1.0f - distance) / rounding;
                return -rounding * blend * blend * blend * (1.0f - 0.5f * blend) / rise;
            }
            return -1.0f + (distance - floorRadius - 0.5f * rounding) / rise;
        }

        void addCrater(CraterBlend& blend, const Crater& crater, float bowlT, float planetRadius, float rimAmp, float wallBias = 0.0f, float apronScale = 1.0f, float talus = 0.0f, float terraceOffset = 0.0f, float terraceIntegrity = 1.0f) {
            // |wallBias| <= .22 keeps this radial map strictly increasing.
            // It changes wall steepness and the foot of each sector, not depth.
            const float wallT = bowlT < 1.0f ? bowlT + wallBias * 4.0f * bowlT * (1.0f - bowlT) : bowlT;
            float bowl = craterBowl(wallT, crater.profile);
            if (talus > 0.0f) {
                auto toeProfile = crater.profile;
                toeProfile.floorRadius = glm::max(0.10f, toeProfile.floorRadius - 0.16f);
                toeProfile.rounding = glm::min(0.14f, 0.5f * (1.0f - toeProfile.floorRadius));
                // Convex blending of monotone profiles adds a shallow foot, not
                // a detached positive mound. Center depth and ground level agree.
                bowl = glm::mix(bowl, craterBowl(wallT, toeProfile), talus);
            }
            if (crater.radius >= 0.14f) {
                // Large impacts expose broad, broken terraces. Per-sector phase
                // and integrity stop the bands reading as concentric contour lines.
                const float terraceStrength = 0.48f * glm::smoothstep(0.14f, 0.32f, crater.radius) * terraceIntegrity;
                const float bands = crater.radius >= 0.30f ? 3.0f : 2.0f;
                const float wallHeight = glm::clamp(bowl + 1.0f, 0.0f, 1.0f);
                const float phaseFade = glm::smoothstep(0.04f, 0.28f, wallHeight) * (1.0f - glm::smoothstep(0.82f, 0.98f, wallHeight));
                const float shiftedHeight = glm::clamp(wallHeight + terraceOffset * phaseFade, 0.0f, 1.0f);
                const float bandHeight = shiftedHeight * bands;
                const float terracedHeight =
                    (glm::floor(bandHeight) + glm::smoothstep(0.32f, 0.68f, glm::fract(bandHeight))) / bands - terraceOffset * phaseFade;
                bowl = glm::mix(bowl, glm::clamp(terracedHeight, 0.0f, 1.0f) - 1.0f, terraceStrength);

            }
            const float rimT = (bowlT - 1.02f) / ((bowlT < 1.02f ? crater.profile.rimInnerWidth : craterSupport - 1.02f) * apronScale);
            const float rimFoot = glm::max(0.0f, 1.0f - rimT * rimT);
            const float rim = rimFoot * rimFoot * rimAmp * crater.profile.rimScale;
            const float expose = glm::smoothstep(0.012f, 0.032f, crater.radius);
            const vec2 depth{crater.depthUnit * expose, planetRadius * crater.depthScale};
            // Deepen geometry only; retain the rim height and material-field scale.
            const vec2 depression = bowl * depth * vec2{1.0f, craterDepthMultiplier};
            blend.bowlsSquared += depression * depression;
            // Excavated areas suppress rim ridges, never the bowls themselves.
            blend.rims += rim * depth * vec2{0.30f, 0.08f};
            // Smooth union avoids the crease of max(coverA, coverB) on overlaps.
            blend.cover += (-bowl) * (1.0f - blend.cover);
        }

        auto craterField(vec3 dir, integer seed, float planetRadius) -> CraterHit {
            // Bounded per-thread memoization of seed-only coefficients, never terrain
            // state. Reused by mesh generation and physics; no per-vertex allocation.
            struct Cache {
                integer seed;
                std::array<Crater, craterCount> craters;
                std::array<vector<std::uint16_t>, craterCount> buckets;
            };
            auto createCache = [](integer seed) {
                auto craters = makeCraters(seed);
                return Cache{seed, craters, craterBuckets(craters)};
            };
            thread_local Cache cache = createCache(seed);
            if (cache.seed != seed)
                cache = createCache(seed);
            CraterBlend blend{.bowlsSquared = vec2{0.0f}, .rims = vec2{0.0f}, .cover = 0.0f};
            // Stable face/iu/iv order also keeps floating-point sums independent of
            // the tile and of the query's position within a crater-distribution cell.
            for (std::uint16_t index : cache.buckets[craterCell(dir)]) {
                const Crater& crater = cache.craters[index];
                const vec3 offset = dir - crater.center;
                const float distanceSquared = glm::dot(offset, offset);
                const float reach = crater.radius * craterReach;
                if (distanceSquared > reach * reach)
                    continue;
                const auto sample = sampleCrater(offset, distanceSquared, crater, true, &crater.shape.fanDirections);
                addCrater(blend, crater, sample.distance, planetRadius, sample.rimAmp, sample.wallBias * crater.shape.wallScale, sample.apronScale, sample.talus * crater.shape.talusScale, sample.terraceOffset, sample.terraceIntegrity);
            }
            return finishCraters(blend);
        }

        struct Relief {
            float height;
            float crater;
        };

        using TerrainClock = std::chrono::steady_clock;

        struct TerrainTelemetry {
            std::uint64_t heightSamples = 0;
            std::uint64_t heightNanoseconds = 0;
            std::uint64_t heightMaxNanoseconds = 0;
            std::uint64_t physicalHeightQueries = 0;
            std::uint64_t altitudeQueries = 0;
            std::uint64_t frames = 0;
            std::uint64_t patchBuilds = 0;
            std::size_t cpuTransientPeakBytes = 0;
            vector<std::uint64_t> patchBuildNanoseconds;
            vector<std::uint32_t> patchSamples;
            vector<std::uint32_t> createdPerFrame;
            vector<std::uint32_t> rebuiltPerFrame;
            vector<std::uint32_t> jobsStartedPerFrame;
            vector<std::uint32_t> jobsCommittedPerFrame;
        };

        auto terrainTelemetry() -> TerrainTelemetry& {
            static TerrainTelemetry telemetry;
            return telemetry;
        }

        auto terrainTelemetryMutex() -> std::mutex& {
            static std::mutex mutex;
            return mutex;
        }

        struct HeightSampleBatch {
            std::uint64_t samples = 0;
            std::uint64_t nanoseconds = 0;
            std::uint64_t maxNanoseconds = 0;
        };

        thread_local HeightSampleBatch* activeHeightSampleBatch = nullptr;

        struct HeightSampleTimer {
            TerrainClock::time_point started = TerrainClock::now();

            ~HeightSampleTimer() {
                const auto elapsed = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(TerrainClock::now() - started).count());
                if (activeHeightSampleBatch) {
                    ++activeHeightSampleBatch->samples;
                    activeHeightSampleBatch->nanoseconds += elapsed;
                    activeHeightSampleBatch->maxNanoseconds = std::max(activeHeightSampleBatch->maxNanoseconds, elapsed);
                    return;
                }
                const std::scoped_lock lock(terrainTelemetryMutex());
                auto& telemetry = terrainTelemetry();
                ++telemetry.heightSamples;
                telemetry.heightNanoseconds += elapsed;
                telemetry.heightMaxNanoseconds = std::max(telemetry.heightMaxNanoseconds, elapsed);
            }
        };

        auto testCraterField(vec3 dir, const Planetoid::Look& look) -> CraterHit {
            // Preview sector-shaped walls here before enabling them in the catalog.
            static const auto sharpProfile = [] {
                auto profile = craterProfile(0.05f, 0.20f);
                // Widen this preview's floor without changing its depth or outer rim.
                profile.floorRadius = 0.65f;
                profile.rounding = 0.085f;
                return profile;
            }();
            static const std::array<Crater, 2> pair{
                Crater{.center = glm::normalize(vec3{-0.045f, 0.0f, 1.0f}), .radius = 0.075f, .depthUnit = 0.7f, .depthScale = 0.012f, .noiseSeed = 47, .profile = sharpProfile},
                Crater{.center = glm::normalize(vec3{0.045f, 0.01f, 1.0f}), .radius = 0.055f, .depthUnit = 0.7f, .depthScale = 0.008f, .noiseSeed = 71, .profile = craterProfile(0.90f, 0.90f)},
            };
            static const auto fanDirections = craterFanDirections(pair[0].center);
            CraterBlend blend{.bowlsSquared = vec2{0.0f}, .rims = vec2{0.0f}, .cover = 0.0f};
            for (std::size_t index = 0; index < pair.size(); ++index) {
                if (look.terrainTest == Landscape::TerrainTest::off or (look.terrainTest == Landscape::TerrainTest::first and index != 0) or (look.terrainTest == Landscape::TerrainTest::second and index != 1)) continue;
                const auto& crater = pair[index];
                const vec3 offset = dir - crater.center;
                const float distanceSquared = glm::dot(offset, offset);
                const float reach = crater.radius * sculptedReach;
                if (distanceSquared > reach * reach) continue;
                const auto sample = sampleCrater(offset, distanceSquared, crater, true, index == 0 ? &fanDirections : nullptr);
                // Soften radial folds in the sharp preview, keeping its irregular
                // outer contour and broad floor instead of smoothing the whole bowl.
                const float wallBias = sample.wallBias * (index == 0 ? 0.60f : 1.0f);
                addCrater(blend, crater, sample.distance, look.radius, look.testRims ? sample.rimAmp : 0.0f, wallBias, sample.apronScale, sample.talus, sample.terraceOffset, sample.terraceIntegrity);
            }
            return finishCraters(blend);
        }

        auto reliefOf(vec3 dir, const Planetoid::Look& look) -> Relief {
            const HeightSampleTimer sampleTimer;
            const float len = glm::length(dir);
            if (len < 1.0e-6f)
                return Relief{.height = 0.0f, .crater = 0.0f};
            dir /= len;
            if (look.terrainTest != Landscape::TerrainTest::off) {
                const auto pits = testCraterField(dir, look);
                return Relief{.height = pits.meters, .crater = pits.field};
            }
            const int seed = static_cast<int>(look.seed);
            const vec3 warp = vec3{signedNoise(dir * 2.1f, seed + 3), signedNoise(vec3{dir.y, dir.z, dir.x} * 2.1f, seed + 7), signedNoise(vec3{dir.z, dir.x, dir.y} * 2.1f, seed + 11)};
            const vec3 warped = glm::normalize(dir + warp * 0.14f);
            const vec3 fold = vec3{signedNoise(warped * 1.7f, seed + 19), signedNoise(vec3{warped.y, warped.z, warped.x} * 1.7f, seed + 23), signedNoise(vec3{warped.z, warped.x, warped.y} * 1.7f, seed + 29)};
            const vec3 folded = glm::normalize(warped + fold * 0.24f);
            const float shape = fbm(warped * 2.4f, seed) * 0.55f + signedNoise(warped * 8.0f, seed + 13) * 0.18f;
            const float massif = fbm(folded * 1.35f, seed + 37);
            const CraterHit pits = craterField(dir, look.seed, look.radius);
            const float ground = 0.48f * shape * look.maxRelief;
            const float massifAbs = std::abs(massif);
            const float plateau = std::copysign(glm::smoothstep(0.10f, 0.70f, massifAbs), massif);
            const float boundaryRidge = glm::smoothstep(0.84f, 0.97f, 1.0f - massifAbs);
            const float tectonic = (glm::mix(massif, plateau, 0.45f) + 0.28f * boundaryRidge) * look.tectonic * look.radius;
            // A shared middle-frequency sample bends and breaks the generated
            // mountain chains and canyons, then also drives local erosion below.
            const vec3 eroded = warped + fold * 0.085f;
            const float erosionNoise = 0.68f * signedNoise(eroded * 22.0f, seed + 71) + 0.32f * signedNoise(eroded * 51.0f, seed + 79);
            const float structures = lineamentField(dir, look.seed, look.maxRelief, erosionNoise, fold);
            // Later impacts erase positive ranges inside their bowls. Negative
            // fractures remain as floor relief and can themselves be overprinted.
            const float excavatedStructures = glm::min(structures, 0.0f);
            const float base = glm::mix(tectonic + ground + structures, glm::min(tectonic, 0.0f) + ground + excavatedStructures, pits.cover);
            const float bowlDamp = glm::smoothstep(0.0f, 0.85f, glm::clamp(-pits.field, 0.0f, 1.4f) / 1.4f);
            const float rimBoost = glm::smoothstep(0.04f, 0.40f, glm::clamp(pits.field, 0.0f, 0.55f));
            const float heightBoost = glm::smoothstep(-0.25f, 0.55f, shape);
            // A middle band of warped, sharpened relief bridges broad tectonics
            // and sub-metre grit. It is strongest on exposed rims and restrained
            // on crater floors, so large bowls remain legible instead of noisy.
            const float erosion = std::copysign(glm::smoothstep(0.12f, 0.78f, std::abs(erosionNoise)), erosionNoise);
            const float erosionAmp = 34.0f * (1.0f - 0.58f * bowlDamp) * (1.0f + 1.65f * rimBoost);
            float gritAmp = 0.55f * (1.0f - 0.88f * bowlDamp) * (1.0f + 2.0f * rimBoost) * (0.40f + 0.60f * heightBoost);
            const float grit = 0.62f * signedNoise(dir * (look.radius / 8.0f), seed + 53) + 0.38f * signedNoise(dir * (look.radius / 4.0f), seed + 59);
            return Relief{.height = base + pits.meters + erosion * erosionAmp + grit * gritAmp, .crater = pits.field};
        }

        auto heightOf(vec3 dir, const Planetoid::Look& look) -> float {
            return reliefOf(dir, look).height;
        }

        // Demo crust: Ice / Olivine / Pyroxene / Iron only (matches planetoid.frag).
        auto materialWeights(vec3 dir, float height, float slope, float crater, const Planetoid::Look& look) -> std::array<float, mixChannels> {
            const float polar = std::abs(dir.y);
            const float altitude = look.maxRelief > 1.0e-4f ? height / look.maxRelief : 0.0f;
            const float bowl = glm::smoothstep(0.05f, 0.85f, glm::clamp(-crater, 0.0f, 1.4f) / 1.4f);
            const float lowland = glm::smoothstep(0.25f, 1.50f, glm::max(-altitude, 0.0f));
            const float basinExposure = bowl * lowland;
            const float flats = 1.0f - slope;
            const float midLat = 1.0f - glm::smoothstep(0.45f, 0.82f, polar);
            const float highland = glm::smoothstep(0.05f, 0.55f, altitude);
            std::array<float, mixChannels> weights{};
            weights[mineralIce] =
                0.78f * glm::smoothstep(0.52f, 0.88f, polar) +
                0.34f * glm::smoothstep(0.18f, 0.68f, slope) * (0.35f + 0.65f * highland);
            weights[mineralOlivine] = 0.55f * flats * midLat * (0.55f + 0.45f * (1.0f - highland));
            weights[mineralPyroxene] = 0.48f * (0.35f + 0.65f * highland) * (0.55f + 0.45f * midLat);
            weights[mineralIron] =
                (0.20f * bowl + 1.80f * basinExposure) * (0.45f + 0.55f * midLat);
            return weights;
        }

        auto cohesionOf(vec3 dir, float height, float slope, float crater, const Planetoid::Look& look) -> float {
            const float polar = std::abs(dir.y);
            const float altitude = look.maxRelief > 1.0e-4f ? height / look.maxRelief : 0.0f;
            const float bowl = glm::smoothstep(0.05f, 0.85f, glm::clamp(-crater, 0.0f, 1.4f) / 1.4f);
            const float highland = glm::smoothstep(0.05f, 0.55f, altitude);
            const float ice = glm::smoothstep(0.52f, 0.88f, polar);
            const float packed = 0.74f + 0.16f * ice + 0.08f * highland - 0.18f * bowl - 0.12f * slope;
            return glm::clamp(packed, 0.62f, 0.95f);
        }

        auto materialMix(vec3 dir, float height, float slope, float crater, const Planetoid::Look& look) -> Mix {
            const auto weights = materialWeights(dir, height, slope, crater, look);
            float mass = 0.0f;
            for (int channel : planetPalette)
                mass += weights[static_cast<std::size_t>(channel)];
            if (mass <= 1.0e-6f)
                return Mix{255u} | (Mix{static_cast<std::uint32_t>(mineralOlivine)} << 32);
            std::uint32_t weightBits = 0;
            std::uint32_t indexBits = 0;
            int used = 0;
            int lastLive = -1;
            for (int slot = 0; slot < 4; ++slot) {
                const int channel = planetPalette[slot];
                const int byte = static_cast<int>(weights[static_cast<std::size_t>(channel)] / mass * 255.0f + 0.5f);
                const int clamped = glm::clamp(byte, 0, 255);
                weightBits |= static_cast<std::uint32_t>(clamped) << (slot * 8);
                indexBits |= static_cast<std::uint32_t>(channel) << (slot * 8);
                used += clamped;
                if (clamped > 0)
                    lastLive = slot;
            }
            if (used == 0)
                return Mix{255u} | (Mix{static_cast<std::uint32_t>(mineralOlivine)} << 32);
            if (used != 255 and lastLive >= 0) {
                const int shift = lastLive * 8;
                const int current = static_cast<int>((weightBits >> shift) & 255u);
                const int adjusted = glm::clamp(current + (255 - used), 0, 255);
                weightBits = (weightBits & ~(255u << shift)) | (static_cast<std::uint32_t>(adjusted) << shift);
            }
            return Mix{weightBits} | (Mix{indexBits} << 32);
        }

        auto surfacePoint(vec3 dir, const Planetoid::Look& look) -> vec3 {
            return dir * (look.radius + heightOf(dir, look));
        }

        auto gradientNormal(vec3 dir, vec3 origin, const Planetoid::Look& look) -> vec3 {
            const float len = glm::length(dir);
            if (len < 1.0e-6f)
                return vec3{0.0f, 1.0f, 0.0f};
            dir /= len;
            vec3 tangentU = glm::cross(vec3{0.0f, 1.0f, 0.0f}, dir);
            if (glm::dot(tangentU, tangentU) < 1.0e-8f)
                tangentU = glm::cross(vec3{1.0f, 0.0f, 0.0f}, dir);
            tangentU = glm::normalize(tangentU);
            const vec3 tangentV = glm::cross(dir, tangentU);
            constexpr float eps = 0.0024f;
            // A fixed angular stencil makes shared normals independent of patch/LOD.
            // Reuse the sampled origin: only two additional height queries per vertex.
            const vec3 alongU = surfacePoint(glm::normalize(dir + tangentU * eps), look) - origin;
            const vec3 alongV = surfacePoint(glm::normalize(dir + tangentV * eps), look) - origin;
            vec3 normal = glm::cross(alongU, alongV);
            const float normalLen = glm::length(normal);
            if (normalLen < 1.0e-8f)
                return dir;
            normal /= normalLen;
            if (glm::dot(normal, dir) < 0.0f)
                normal = -normal;
            return normal;
        }

        auto toLocal(const Landscape& state, Pos worldPos) -> vec3 {
            return glm::inverse(state.pose.rotation) * (worldPos - state.pose.position);
        }

        auto tileRange(int level, int index) -> float {
            return -1.0f + static_cast<float>(index) * (2.0f / static_cast<float>(1 << level));
        }

        auto patchCenter(const Landscape& state, const PatchKey& key) -> vec3 {
            const float tileSize = 2.0f / static_cast<float>(1 << key.level);
            const float faceU = tileRange(key.level, key.iu) + tileSize * 0.5f;
            const float faceV = tileRange(key.level, key.iv) + tileSize * 0.5f;
            return state.pose.position + state.pose.rotation * (cubeDir(key.face, faceU, faceV) * state.look.radius);
        }

        void emitTri(resource::builders::geometry::CpuPresentation& cpu, integer first, integer second, integer third) {
            if (first == second or second == third or first == third)
                return;
            cpu.indices.push_back(first);
            cpu.indices.push_back(second);
            cpu.indices.push_back(third);
        }

        auto cpuAllocatedBytes(const resource::builders::geometry::CpuPresentation& cpu) -> std::size_t {
            return cpu.positions.capacity() * sizeof(decltype(cpu.positions)::value_type) +
                cpu.normals.capacity() * sizeof(decltype(cpu.normals)::value_type) +
                cpu.uv0.capacity() * sizeof(decltype(cpu.uv0)::value_type) +
                cpu.color0.capacity() * sizeof(decltype(cpu.color0)::value_type) +
                cpu.indices.capacity() * sizeof(decltype(cpu.indices)::value_type) +
                cpu.mix0.capacity() * sizeof(decltype(cpu.mix0)::value_type) +
                cpu.cohesion.capacity() * sizeof(decltype(cpu.cohesion)::value_type);
        }

        auto activePatchGpuBytes(std::uint8_t coarserEdges, bool wireframe) -> std::size_t {
            auto stitchedIndex = [&](int column, int row) -> integer {
                if ((column == 0 and (coarserEdges & 1u)) or (column == cells and (coarserEdges & 2u)))
                    row -= row % 2;
                if ((row == 0 and (coarserEdges & 4u)) or (row == cells and (coarserEdges & 8u)))
                    column -= column % 2;
                return row * grid + column;
            };
            std::size_t indexCount = 0;
            auto countTri = [&](integer first, integer second, integer third) {
                if (first != second and second != third and first != third)
                    indexCount += 3;
            };
            for (int row = 0; row < cells; ++row) {
                for (int column = 0; column < cells; ++column) {
                    const integer indexA = stitchedIndex(column, row);
                    const integer indexB = stitchedIndex(column + 1, row);
                    const integer indexC = stitchedIndex(column, row + 1);
                    const integer indexD = stitchedIndex(column + 1, row + 1);
                    countTri(indexA, indexB, indexD);
                    countTri(indexA, indexD, indexC);
                }
            }
            const std::size_t vertexCount = wireframe ? indexCount : static_cast<std::size_t>(grid * grid);
            constexpr std::size_t vertexStride = 9 * sizeof(float);
            return vertexCount * vertexStride + indexCount * sizeof(std::uint32_t) +
                (indexCount / 3) * sizeof(resource::geometry::SurfaceId);
        }

        auto terrainGeometryLayout() -> const decltype(resource::builders::geometry::CpuPresentation::layout)& {
            static const auto layout = primitive::GeometrySemantics::layoutIds(vector<string>{"position", "normal", "uv0", "cohesion"});
            return layout;
        }

        auto buildPatch(const Planetoid::Look& look, const PatchKey& key, std::uint8_t coarserEdges) -> resource::builders::geometry::CpuPresentation {
            const auto buildStarted = TerrainClock::now();
            HeightSampleBatch sampleBatch;
            struct SampleBatchScope {
                HeightSampleBatch* previous;
                explicit SampleBatchScope(HeightSampleBatch& batch) : previous(activeHeightSampleBatch) { activeHeightSampleBatch = &batch; }
                ~SampleBatchScope() { activeHeightSampleBatch = previous; }
            } sampleBatchScope(sampleBatch);
            resource::builders::geometry::CpuPresentation cpu{
                .layout = terrainGeometryLayout(),
                .positions = {},
                .normals = {},
                .uv0 = {},
                .color0 = {},
                .indices = {},
                .mix0 = {},
                .cohesion = {},
            };
            const float tileSize = 2.0f / static_cast<float>(1 << key.level);
            const float originU = tileRange(key.level, key.iu);
            const float originV = tileRange(key.level, key.iv);
            const int mainCount = grid * grid;
            const float reliefScale = look.maxRelief > 1.0e-4f ? 1.0f / look.maxRelief : 0.0f;
            cpu.positions.reserve(static_cast<std::size_t>(mainCount));
            cpu.normals.reserve(cpu.positions.capacity());
            cpu.uv0.reserve(cpu.positions.capacity());
            cpu.cohesion.reserve(cpu.positions.capacity());
            cpu.indices.reserve(static_cast<std::size_t>(cells * cells * 6));

            for (int row = 0; row < grid; ++row) {
                const float faceV = originV + tileSize * (static_cast<float>(row) / static_cast<float>(cells));
                for (int column = 0; column < grid; ++column) {
                    const float faceU = originU + tileSize * (static_cast<float>(column) / static_cast<float>(cells));
                    const vec3 dir = cubeDir(key.face, faceU, faceV);
                    const Relief relief = reliefOf(dir, look);
                    const vec3 position = dir * (look.radius + relief.height);
                    const vec3 normal = gradientNormal(dir, position, look);
                    const float slope = 1.0f - glm::clamp(glm::dot(normal, dir), 0.0f, 1.0f);
                    cpu.positions.push_back(position);
                    cpu.normals.push_back(normal);
                    cpu.uv0.push_back(UV{relief.height * reliefScale, relief.crater});
                    cpu.cohesion.push_back(cohesionOf(dir, relief.height, slope, relief.crater, look));
                }
            }

            // Use the coarse edge's endpoints directly, so raster interpolation of
            // positions, normals and material drivers agrees on both sides.
            auto stitchedIndex = [&](int column, int row) -> integer {
                if ((column == 0 and (coarserEdges & 1u)) or (column == cells and (coarserEdges & 2u)))
                    row -= row % 2;
                if ((row == 0 and (coarserEdges & 4u)) or (row == cells and (coarserEdges & 8u)))
                    column -= column % 2;
                return row * grid + column;
            };

            const bool flip = flipWinding[key.face];
            for (int row = 0; row < cells; ++row) {
                for (int column = 0; column < cells; ++column) {
                    const integer indexA = stitchedIndex(column, row);
                    const integer indexB = stitchedIndex(column + 1, row);
                    const integer indexC = stitchedIndex(column, row + 1);
                    const integer indexD = stitchedIndex(column + 1, row + 1);
                    if (flip) {
                        emitTri(cpu, indexA, indexC, indexD);
                        emitTri(cpu, indexA, indexD, indexB);
                    } else {
                        emitTri(cpu, indexA, indexB, indexD);
                        emitTri(cpu, indexA, indexD, indexC);
                    }
                }
            }
            const auto buildNanoseconds = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(TerrainClock::now() - buildStarted).count());
            const std::scoped_lock lock(terrainTelemetryMutex());
            auto& telemetry = terrainTelemetry();
            telemetry.heightSamples += sampleBatch.samples;
            telemetry.heightNanoseconds += sampleBatch.nanoseconds;
            telemetry.heightMaxNanoseconds = std::max(telemetry.heightMaxNanoseconds, sampleBatch.maxNanoseconds);
            ++telemetry.patchBuilds;
            telemetry.patchBuildNanoseconds.push_back(buildNanoseconds);
            telemetry.patchSamples.push_back(static_cast<std::uint32_t>(sampleBatch.samples));
            telemetry.cpuTransientPeakBytes = std::max(telemetry.cpuTransientPeakBytes, cpuAllocatedBytes(cpu));
            return cpu;
        }

        void addWireframe(resource::builders::geometry::CpuPresentation& cpu) {
            auto indexed = std::move(cpu);
            cpu = resource::builders::geometry::CpuPresentation{
                .layout = terrainGeometryLayout(),
                .positions = {}, .normals = {}, .uv0 = {}, .color0 = {}, .indices = {}, .mix0 = {}, .cohesion = {},
            };
            const auto count = indexed.indices.size();
            cpu.positions.reserve(count);
            cpu.normals.reserve(count);
            cpu.uv0.reserve(count);
            cpu.cohesion.reserve(count);
            cpu.indices.reserve(count);
            // Debug-only barycentrics describe the actual stitched triangles.
            // Reinstalling the regular grid when leaving this view releases the buffers.
            for (std::size_t corner = 0; corner < count; ++corner) {
                const auto index = static_cast<std::size_t>(indexed.indices[corner]);
                cpu.positions.push_back(indexed.positions[index]);
                cpu.normals.push_back(indexed.normals[index]);
                cpu.cohesion.push_back(indexed.cohesion[index]);
                // This material does not use terrain drivers; reuse uv0 for two
                // barycentric components and derive the third in the shader.
                cpu.uv0.push_back(corner % 3 == 0 ? vec2{1.0f, 0.0f} : corner % 3 == 1 ? vec2{0.0f, 1.0f} : vec2{0.0f});
                cpu.indices.push_back(static_cast<integer>(corner));
            }
        }

        auto childKey(const PatchKey& key, int childU, int childV) -> PatchKey {
            return PatchKey{.face = key.face, .level = static_cast<std::uint8_t>(key.level + 1), .iu = static_cast<std::uint16_t>(key.iu * 2 + childU), .iv = static_cast<std::uint16_t>(key.iv * 2 + childV)};
        }

        auto hasChild(const Landscape& state, const PatchKey& key) -> bool {
            return state.patches.contains(childKey(key, 0, 0)) or state.patches.contains(childKey(key, 1, 0)) or state.patches.contains(childKey(key, 0, 1)) or state.patches.contains(childKey(key, 1, 1));
        }

        auto parentOf(PatchKey key) -> PatchKey {
            return PatchKey{.face = key.face, .level = static_cast<std::uint8_t>(key.level - 1), .iu = static_cast<std::uint16_t>(key.iu / 2), .iv = static_cast<std::uint16_t>(key.iv / 2)};
        }

        auto neighborKey(PatchKey key, int edge) -> PatchKey {
            const int tiles = 1 << key.level;
            const auto last = static_cast<std::uint16_t>(tiles - 1);
            const auto level = key.level;
            const auto iu = key.iu;
            const auto iv = key.iv;
            if (edge == 0 and iu > 0)
                return PatchKey{.face = key.face, .level = level, .iu = static_cast<std::uint16_t>(iu - 1), .iv = iv};
            if (edge == 1 and iu < last)
                return PatchKey{.face = key.face, .level = level, .iu = static_cast<std::uint16_t>(iu + 1), .iv = iv};
            if (edge == 2 and iv > 0)
                return PatchKey{.face = key.face, .level = level, .iu = iu, .iv = static_cast<std::uint16_t>(iv - 1)};
            if (edge == 3 and iv < last)
                return PatchKey{.face = key.face, .level = level, .iu = iu, .iv = static_cast<std::uint16_t>(iv + 1)};
            switch (key.face) {
                case 0:
                    if (edge == 0)
                        return PatchKey{.face = 3, .level = level, .iu = last, .iv = iv};
                    if (edge == 1)
                        return PatchKey{.face = 2, .level = level, .iu = last, .iv = iv};
                    if (edge == 2)
                        return PatchKey{.face = 5, .level = level, .iu = last, .iv = iu};
                    return PatchKey{.face = 4, .level = level, .iu = last, .iv = iu};
                case 1:
                    if (edge == 0)
                        return PatchKey{.face = 3, .level = level, .iu = 0, .iv = iv};
                    if (edge == 1)
                        return PatchKey{.face = 2, .level = level, .iu = 0, .iv = iv};
                    if (edge == 2)
                        return PatchKey{.face = 5, .level = level, .iu = 0, .iv = iu};
                    return PatchKey{.face = 4, .level = level, .iu = 0, .iv = iu};
                case 2:
                    if (edge == 0)
                        return PatchKey{.face = 1, .level = level, .iu = last, .iv = iv};
                    if (edge == 1)
                        return PatchKey{.face = 0, .level = level, .iu = last, .iv = iv};
                    if (edge == 2)
                        return PatchKey{.face = 5, .level = level, .iu = iu, .iv = last};
                    return PatchKey{.face = 4, .level = level, .iu = iu, .iv = last};
                case 3:
                    if (edge == 0)
                        return PatchKey{.face = 1, .level = level, .iu = 0, .iv = iv};
                    if (edge == 1)
                        return PatchKey{.face = 0, .level = level, .iu = 0, .iv = iv};
                    if (edge == 2)
                        return PatchKey{.face = 5, .level = level, .iu = iu, .iv = 0};
                    return PatchKey{.face = 4, .level = level, .iu = iu, .iv = 0};
                case 4:
                    if (edge == 0)
                        return PatchKey{.face = 1, .level = level, .iu = iv, .iv = last};
                    if (edge == 1)
                        return PatchKey{.face = 0, .level = level, .iu = iv, .iv = last};
                    if (edge == 2)
                        return PatchKey{.face = 3, .level = level, .iu = iu, .iv = last};
                    return PatchKey{.face = 2, .level = level, .iu = iu, .iv = last};
                default:
                    if (edge == 0)
                        return PatchKey{.face = 1, .level = level, .iu = iv, .iv = 0};
                    if (edge == 1)
                        return PatchKey{.face = 0, .level = level, .iu = iv, .iv = 0};
                    if (edge == 2)
                        return PatchKey{.face = 3, .level = level, .iu = iu, .iv = 0};
                    return PatchKey{.face = 2, .level = level, .iu = iu, .iv = 0};
            }
        }

        using LeafSet = std::unordered_set<PatchKey, PatchKeyHash>;

        auto coveringLeaf(const LeafSet& leaves, PatchKey query) -> PatchKey {
            PatchKey cursor = query;
            while (true) {
                if (leaves.contains(cursor))
                    return cursor;
                if (cursor.level == 0)
                    break;
                cursor = parentOf(cursor);
            }
            for (const PatchKey& leaf : leaves) {
                if (leaf.face != query.face or leaf.level <= query.level)
                    continue;
                const int shift = leaf.level - query.level;
                if ((leaf.iu >> shift) == query.iu and (leaf.iv >> shift) == query.iv)
                    return leaf;
            }
            return query;
        }

        void restrictLeaves(LeafSet& leaves) {
            for (int pass = 0; pass < 24; ++pass) {
                vector<PatchKey> split;
                for (const PatchKey& leaf : leaves) {
                    for (int edge = 0; edge < 4; ++edge) {
                        const PatchKey cover = coveringLeaf(leaves, neighborKey(leaf, edge));
                        if (cover.level + 1 < leaf.level)
                            split.push_back(cover);
                    }
                }
                if (split.empty())
                    return;
                for (const PatchKey& cover : split) {
                    if (not leaves.contains(cover) or cover.level >= maxLevel)
                        continue;
                    leaves.erase(cover);
                    leaves.insert(childKey(cover, 0, 0));
                    leaves.insert(childKey(cover, 1, 0));
                    leaves.insert(childKey(cover, 0, 1));
                    leaves.insert(childKey(cover, 1, 1));
                }
            }
        }

        auto edgeFlags(const LeafSet& leaves, const PatchKey& key) -> std::uint8_t {
            std::uint8_t flags = 0;
            for (int edge = 0; edge < 4; ++edge) {
                if (coveringLeaf(leaves, neighborKey(key, edge)).level < key.level)
                    flags |= static_cast<std::uint8_t>(1u << edge);
            }
            return flags;
        }

        void collectLeaves(const Landscape& state, Pos camera, PatchKey key, vector<PatchKey>& wanted) {
            const float tileSize = 2.0f / static_cast<float>(1 << key.level);
            const float dist = glm::length(camera - patchCenter(state, key));
            const float span = state.look.radius * tileSize;
            if (key.level < maxLevel) {
                const float splitAt = (hasChild(state, key) ? splitKeep : splitNear) * span;
                if (dist < splitAt) {
                    collectLeaves(state, camera, childKey(key, 0, 0), wanted);
                    collectLeaves(state, camera, childKey(key, 1, 0), wanted);
                    collectLeaves(state, camera, childKey(key, 0, 1), wanted);
                    collectLeaves(state, camera, childKey(key, 1, 1), wanted);
                    return;
                }
            }
            wanted.push_back(key);
        }

        void stripCovered(LeafSet& leaves) {
            vector<PatchKey> covered;
            for (const PatchKey& leaf : leaves) {
                PatchKey cursor = leaf;
                while (cursor.level > 0) {
                    cursor = parentOf(cursor);
                    if (leaves.contains(cursor))
                        covered.push_back(cursor);
                }
            }
            for (const PatchKey& key : covered)
                leaves.erase(key);
        }

        void dropActor(Writing context, scene::actor::Mesh::Id actor) {
            if (with<scene::Node>::exists(context, actor))
                with<scene::Node>::modify(context, actor)->visible = false;
            const auto scene = with<Thing>::get_global(context).scene;
            if (with<scene::Node_group>::exists(context, scene) and with<scene::Node_group>::get(context, scene).contains(actor))
                with<scene::Node_group>::deleteElement(context, scene, actor);
            else if (with<scene::Node>::exists(context, actor))
                with<scene::Node>::remove(context, actor);
        }

        void dropPatch(Writing context, Patch patch) {
            dropActor(context, patch.actor);
        }

        void bindAtmosphereMesh(scene::actor::MeshState::Quantum& mesh, const Landscape& state) {
            const auto& atmosphere = state.look.atmosphere;
            mesh.albedo = atmosphere.day;
            mesh.opacity = atmosphere.seaDensity;
            mesh.heat = vec2{state.look.radius, atmosphere.radius};
            mesh.scale = vec3{atmosphere.radius * 1.08f};
            mesh.latticeStep = 0.0f;
            mesh.patternScale = Horizon::locality;
        }

        void spawnAtmosphere(Writing context, Landscape& state) {
            if (state.look.atmosphere.radius <= state.look.radius)
                return;
            const auto material = with<resource::Assets>::find<resource::material::Asset>(context, resource::Unit::Name::from("Eltanin", "atmosphere"));
            if (not material)
                return (void)context.refuse("eltanin::locality::geo::Planetoid::place: atmosphere material missing");
            const auto sphere = with<resource::Assets>::find<resource::geometry::Asset>(context, resource::Unit::Name::from("Eltanin", "atmosphereSphere"));
            if (not sphere)
                return (void)context.refuse("eltanin::locality::geo::Planetoid::place: atmosphereSphere geometry missing");
            auto meshQuantum = with<scene::actor::Mesh>::composeOne(context, *sphere, *material);
            if (not meshQuantum)
                return (void)context.refuse("eltanin::locality::geo::Planetoid::place: atmosphere mesh compose failed");
            auto meshState = with<scene::actor::MeshState>::defaults(state.look.atmosphere.day, state.look.atmosphere.seaDensity, vec3{1.0f});
            bindAtmosphereMesh(meshState, state);
            const auto scene = with<Thing>::get_global(context).scene;
            state.atmosphere = with<scene::Interface>::createMeshActor(context, scene, state.pose, std::move(*meshQuantum), meshState);
        }

        auto commitPatch(Writing context, Landscape& state, PatchMap& destination, const PatchKey& key, std::uint8_t coarserEdges, bool wireframe, bool visible, resource::builders::geometry::CpuPresentation cpu) -> bool {
            const auto scene = with<Thing>::get_global(context).scene;
            if (cpu.positions.empty())
                return false;
            const auto existing = destination.find(key);
            if (existing != destination.end()) {
                if (not with<resource::geometry::Asset>::install(context, existing->second.geometry, state.device, cpu)) {
                    context.refuse("eltanin::locality::geo::Planetoid: geometry reinstall failed");
                    return false;
                }
                existing->second.coarserEdges = coarserEdges;
                existing->second.wireframe = wireframe;
                return true;
            }
            static std::uint64_t geometrySerial = 0;
            const string own = "planetoid-" + std::to_string(key.face) + "-" + std::to_string(key.level) + "-" + std::to_string(key.iu) + "-" + std::to_string(key.iv) + "-" + std::to_string(++geometrySerial);
            const auto manager = with<resource::Manager>::singleton(context);
            const auto geometryId = with<resource::Unit_group>::addElement(context, manager, resource::Unit::Quantum{.name = resource::Unit::Name::from("Eltanin", own)});
            with<resource::geometry::Asset>::extend(context, geometryId, resource::geometry::Asset::Quantum{});
            if (not with<resource::geometry::Asset>::install(context, geometryId, state.device, cpu)) {
                context.refuse("eltanin::locality::geo::Planetoid: geometry install failed");
                return false;
            }
            const auto material = state.debugView == Landscape::DebugView::normal ? base::maybe<resource::material::Asset::Id>{state.material} : with<resource::Assets>::find<resource::material::Asset>(context, resource::Unit::Name::from("Eltanin", "terrainDebug"));
            if (not material) {
                context.refuse("Planetoid: diagnostic material missing");
                return false;
            }
            auto meshQuantum = with<scene::actor::Mesh>::composeWithTexpack(context, geometryId, *material, state.crust);
            if (not meshQuantum) {
                context.refuse("eltanin::locality::geo::Planetoid: mesh compose failed");
                return false;
            }
            auto meshState = with<scene::actor::MeshState>::defaults(RGB{1.0f, 1.0f, 1.0f}, 1.0f);
            meshState.patternScale = glm::max(0.5f, state.look.radius * 2.0f);
            meshState.heat.x = static_cast<float>(state.debugView);
            const auto actor = with<scene::Interface>::createMeshActor(context, scene, state.pose, std::move(*meshQuantum), meshState);
            if (not visible and with<scene::Node>::exists(context, actor))
                with<scene::Node>::modify(context, actor)->visible = false;
            destination.emplace(key, Patch{.actor = actor, .geometry = geometryId, .coarserEdges = coarserEdges, .wireframe = wireframe});
            return true;
        }

        template <class T>
        auto telemetryAverage(const vector<T>& values) -> double {
            if (values.empty())
                return 0.0;
            long double sum = 0.0;
            for (const T value : values)
                sum += static_cast<long double>(value);
            return static_cast<double>(sum / static_cast<long double>(values.size()));
        }

        template <class T>
        auto telemetryP95(const vector<T>& values) -> T {
            if (values.empty())
                return T{};
            vector<T> sorted = values;
            std::sort(sorted.begin(), sorted.end());
            const std::size_t index = static_cast<std::size_t>(std::ceil(static_cast<double>(sorted.size()) * 0.95)) - 1;
            return sorted[std::min(index, sorted.size() - 1)];
        }

        void reportTerrainTelemetry(const Landscape& state) {
            TerrainTelemetry telemetry;
            {
                const std::scoped_lock lock(terrainTelemetryMutex());
                telemetry = terrainTelemetry();
            }
            if (telemetry.frames == 0 or telemetry.frames % 60 != 0)
                return;

            const double heightAverageUs = telemetry.heightSamples > 0
                ? static_cast<double>(telemetry.heightNanoseconds) / static_cast<double>(telemetry.heightSamples) / 1000.0
                : 0.0;
            const double patchAverageMs = telemetryAverage(telemetry.patchBuildNanoseconds) / 1.0e6;
            const double patchP95Ms = static_cast<double>(telemetryP95(telemetry.patchBuildNanoseconds)) / 1.0e6;
            const double patchMaxMs = telemetry.patchBuildNanoseconds.empty()
                ? 0.0
                : static_cast<double>(*std::max_element(telemetry.patchBuildNanoseconds.begin(), telemetry.patchBuildNanoseconds.end())) / 1.0e6;
            const auto createdMax = telemetry.createdPerFrame.empty() ? 0u : *std::max_element(telemetry.createdPerFrame.begin(), telemetry.createdPerFrame.end());
            const auto rebuiltMax = telemetry.rebuiltPerFrame.empty() ? 0u : *std::max_element(telemetry.rebuiltPerFrame.begin(), telemetry.rebuiltPerFrame.end());

            std::array<std::uint32_t, maxLevel + 1> lodCounts{};
            std::size_t gpuBytes = 0;
            for (const auto& [key, patch] : state.patches) {
                ++lodCounts[std::min<int>(key.level, maxLevel)];
                gpuBytes += activePatchGpuBytes(patch.coarserEdges, patch.wireframe);
            }

            const auto& asyncState = terrainAsyncState();
            const auto async = asyncState ? asyncState->snapshot() : TerrainAsync::Snapshot{};

            base::message(
                "terrain.telemetry frame={} height_samples={} height_us(avg/max)={:.3f}/{:.3f} physical_queries={} altitude_queries={}",
                telemetry.frames, telemetry.heightSamples, heightAverageUs,
                static_cast<double>(telemetry.heightMaxNanoseconds) / 1000.0,
                telemetry.physicalHeightQueries, telemetry.altitudeQueries);
            base::message(
                "terrain.telemetry patch_builds={} samples_per_patch(avg/p95/max)={:.1f}/{}/{} generation_ms(avg/p95/max)={:.3f}/{:.3f}/{:.3f}",
                telemetry.patchBuilds, telemetryAverage(telemetry.patchSamples), telemetryP95(telemetry.patchSamples),
                telemetry.patchSamples.empty() ? 0u : *std::max_element(telemetry.patchSamples.begin(), telemetry.patchSamples.end()),
                patchAverageMs, patchP95Ms, patchMaxMs);
            base::message(
                "terrain.telemetry patches_per_frame created(avg/p95/max)={:.2f}/{}/{} rebuilt(avg/p95/max)={:.2f}/{}/{} active={} lod=[{},{},{},{},{},{}]",
                telemetryAverage(telemetry.createdPerFrame), telemetryP95(telemetry.createdPerFrame), createdMax,
                telemetryAverage(telemetry.rebuiltPerFrame), telemetryP95(telemetry.rebuiltPerFrame), rebuiltMax,
                state.patches.size(), lodCounts[0], lodCounts[1], lodCounts[2], lodCounts[3], lodCounts[4], lodCounts[5]);
            base::message(
                "terrain.telemetry mesh_bytes cpu_active=0 cpu_transient_peak={} gpu_payload_estimate={} (driver overhead excluded)",
                telemetry.cpuTransientPeakBytes, gpuBytes);
            base::message(
                "terrain.telemetry jobs queued={} generating={} completed_waiting={} resident={} started_total={} committed_total={} stale_discarded={}",
                async.pending, async.generating, async.completed, async.resident,
                async.startedTotal, async.committedTotal, async.discardedTotal);
            base::message(
                "terrain.telemetry jobs_per_frame started(avg/p95/max)={:.2f}/{}/{} committed(avg/p95/max)={:.2f}/{}/{} latency_ms(avg/p95/max)={:.3f}/{:.3f}/{:.3f}",
                telemetryAverage(telemetry.jobsStartedPerFrame), telemetryP95(telemetry.jobsStartedPerFrame),
                telemetry.jobsStartedPerFrame.empty() ? 0u : *std::max_element(telemetry.jobsStartedPerFrame.begin(), telemetry.jobsStartedPerFrame.end()),
                telemetryAverage(telemetry.jobsCommittedPerFrame), telemetryP95(telemetry.jobsCommittedPerFrame),
                telemetry.jobsCommittedPerFrame.empty() ? 0u : *std::max_element(telemetry.jobsCommittedPerFrame.begin(), telemetry.jobsCommittedPerFrame.end()),
                async.latencyAverageMs, async.latencyP95Ms, async.latencyMaxMs);
        }

        auto wellQuantum(Pose pose, const Planetoid::Look& look) -> phys::Body::Quantum {
            const float volume = (4.0f / 3.0f) * std::numbers::pi_v<float> * look.radius * look.radius * look.radius;
            return phys::Body::Quantum{
                .position = dvec3{pose.position},
                .orientation = pose.rotation,
                .totalMass = volume * 3000.0f,
                .radius = look.radius + look.maxRelief,
                .compound = phys::Body::Id::please_never_use_this_except_patch_rejection_mechanism(),
            };
        }

        auto makeWell(Writing context, Pose pose, const Planetoid::Look& look) -> phys::Body::Id {
            return phys::createBody(context, wellQuantum(pose, look), {});
        }

        void bindWell(phys::Body::Quantum& body, Pose pose, const Planetoid::Look& look) {
            const auto next = wellQuantum(pose, look);
            body.position = next.position;
            body.orientation = next.orientation;
            body.totalMass = next.totalMass;
            body.radius = next.radius;
        }

    }

    struct TerrainAsync::State {
        struct Record {
            Lifecycle lifecycle = Lifecycle::pending;
            std::uint8_t coarserEdges = 0;
            bool wireframe = false;
            bool desired = true;
            double priority = 0.0;
            std::uint64_t revision = 0;
            TerrainClock::time_point queuedAt{};
            std::optional<resource::builders::geometry::CpuPresentation> cpu;
        };

        mutable std::mutex mutex;
        std::condition_variable wake;
        bool stopping = false;
        Landscape::Look look;
        std::unordered_map<PatchKey, Record, PatchKeyHash> records;
        std::vector<std::thread> workers;
        std::uint64_t nextRevision = 1;
        std::uint64_t startedTotal = 0;
        std::uint64_t committedTotal = 0;
        std::uint64_t discardedTotal = 0;
        std::uint32_t startedSinceFrame = 0;
        std::uint32_t generating = 0;
        std::vector<std::uint64_t> latencyNanoseconds;

        explicit State(Landscape::Look initialLook) : look(std::move(initialLook)) {
            constexpr unsigned workerCount = 2;
            workers.reserve(workerCount);
            for (unsigned index = 0; index < workerCount; ++index)
                workers.emplace_back([this] { work(); });
        }

        ~State() {
            {
                const std::scoped_lock lock(mutex);
                stopping = true;
            }
            wake.notify_all();
            for (auto& worker : workers)
                if (worker.joinable())
                    worker.join();
        }

        auto hasPending() const -> bool {
            for (const auto& [key, record] : records)
                if (record.lifecycle == Lifecycle::pending and record.desired)
                    return true;
            return false;
        }

        void work() {
            while (true) {
                PatchKey key{};
                std::uint8_t coarserEdges = 0;
                bool wireframe = false;
                std::uint64_t revision = 0;
                double bestPriority = std::numeric_limits<double>::infinity();
                TerrainClock::time_point queuedAt{};
                Landscape::Look jobLook{};
                {
                    std::unique_lock lock(mutex);
                    wake.wait(lock, [this] { return stopping or hasPending(); });
                    if (stopping)
                        return;
                    auto selected = records.end();
                    for (auto candidate = records.begin(); candidate != records.end(); ++candidate) {
                        const auto& record = candidate->second;
                        if (record.lifecycle == Lifecycle::pending and record.desired and record.priority < bestPriority) {
                            selected = candidate;
                            bestPriority = record.priority;
                        }
                    }
                    if (selected == records.end())
                        continue;
                    selected->second.lifecycle = Lifecycle::generating;
                    key = selected->first;
                    coarserEdges = selected->second.coarserEdges;
                    wireframe = selected->second.wireframe;
                    revision = selected->second.revision;
                    queuedAt = selected->second.queuedAt;
                    jobLook = look;
                    ++generating;
                    ++startedTotal;
                    ++startedSinceFrame;
                }

                auto cpu = buildPatch(jobLook, key, coarserEdges);
                if (wireframe)
                    addWireframe(cpu);
                const auto finishedAt = TerrainClock::now();

                {
                    const std::scoped_lock lock(mutex);
                    --generating;
                    const auto found = records.find(key);
                    if (found == records.end() or found->second.revision != revision or not found->second.desired) {
                        ++discardedTotal;
                        continue;
                    }
                    found->second.cpu = std::move(cpu);
                    found->second.lifecycle = Lifecycle::completed;
                    latencyNanoseconds.push_back(static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(finishedAt - queuedAt).count()));
                }
            }
        }
    };

    TerrainAsync::TerrainAsync(Landscape::Look look) {
        // Geometry semantic lookup belongs to the established renderer/resource
        // setup and is not assumed to be thread-safe. Cache it before workers start.
        (void)terrainGeometryLayout();
        state = std::make_unique<State>(std::move(look));
    }
    TerrainAsync::~TerrainAsync() = default;

    void TerrainAsync::reconcile(const std::vector<Desired>& desired) {
        const std::scoped_lock lock(state->mutex);
        for (auto& [key, record] : state->records)
            record.desired = false;

        for (const auto& item : desired) {
            auto found = state->records.find(item.key);
            const bool samePayload = found != state->records.end() and found->second.coarserEdges == item.coarserEdges and found->second.wireframe == item.wireframe;
            if (item.residentMatches) {
                if (found != state->records.end() and found->second.lifecycle != Lifecycle::resident) {
                    if (found->second.lifecycle != Lifecycle::generating)
                        ++state->discardedTotal;
                    state->records.erase(found);
                }
                auto& resident = state->records[item.key];
                resident.lifecycle = Lifecycle::resident;
                resident.coarserEdges = item.coarserEdges;
                resident.wireframe = item.wireframe;
                resident.desired = true;
                resident.priority = item.priority;
                resident.revision = state->nextRevision++;
                continue;
            }
            if (samePayload and found->second.lifecycle != Lifecycle::resident) {
                found->second.desired = true;
                found->second.priority = item.priority;
                continue;
            }
            if (found != state->records.end()) {
                if (found->second.lifecycle != Lifecycle::resident and found->second.lifecycle != Lifecycle::generating)
                    ++state->discardedTotal;
                state->records.erase(found);
            }
            state->records.emplace(item.key, State::Record{
                .lifecycle = Lifecycle::pending,
                .coarserEdges = item.coarserEdges,
                .wireframe = item.wireframe,
                .desired = true,
                .priority = item.priority,
                .revision = state->nextRevision++,
                .queuedAt = TerrainClock::now(),
            });
        }

        for (auto it = state->records.begin(); it != state->records.end();) {
            if (it->second.desired) {
                ++it;
                continue;
            }
            if (it->second.lifecycle != Lifecycle::resident and it->second.lifecycle != Lifecycle::generating)
                ++state->discardedTotal;
            it = state->records.erase(it);
        }
        state->wake.notify_all();
    }

    auto TerrainAsync::takeCompleted(std::size_t limit) -> std::vector<Completed> {
        std::vector<Completed> result;
        result.reserve(limit);
        const std::scoped_lock lock(state->mutex);
        while (result.size() < limit) {
            auto selected = state->records.end();
            double bestPriority = std::numeric_limits<double>::infinity();
            for (auto candidate = state->records.begin(); candidate != state->records.end(); ++candidate) {
                if (candidate->second.lifecycle == Lifecycle::completed and candidate->second.desired and candidate->second.priority < bestPriority) {
                    selected = candidate;
                    bestPriority = candidate->second.priority;
                }
            }
            if (selected == state->records.end())
                break;
            result.push_back(Completed{
                .key = selected->first,
                .coarserEdges = selected->second.coarserEdges,
                .wireframe = selected->second.wireframe,
                .cpu = std::move(*selected->second.cpu),
            });
            state->records.erase(selected);
        }
        return result;
    }

    void TerrainAsync::markResident(const PatchKey& key, std::uint8_t coarserEdges, bool wireframe) {
        const std::scoped_lock lock(state->mutex);
        auto& record = state->records[key];
        record.lifecycle = Lifecycle::resident;
        record.coarserEdges = coarserEdges;
        record.wireframe = wireframe;
        record.desired = true;
        record.revision = state->nextRevision++;
    }

    void TerrainAsync::noteCommitted() {
        const std::scoped_lock lock(state->mutex);
        ++state->committedTotal;
    }

    auto TerrainAsync::takeStartedSinceFrame() -> std::uint32_t {
        const std::scoped_lock lock(state->mutex);
        return std::exchange(state->startedSinceFrame, 0u);
    }

    auto TerrainAsync::snapshot() const -> Snapshot {
        const std::scoped_lock lock(state->mutex);
        Snapshot result{
            .generating = state->generating,
            .startedTotal = state->startedTotal,
            .committedTotal = state->committedTotal,
            .discardedTotal = state->discardedTotal,
        };
        for (const auto& [key, record] : state->records) {
            switch (record.lifecycle) {
                case Lifecycle::pending: ++result.pending; break;
                case Lifecycle::generating: break;
                case Lifecycle::completed: ++result.completed; break;
                case Lifecycle::resident: ++result.resident; break;
            }
        }
        if (not state->latencyNanoseconds.empty()) {
            auto sorted = state->latencyNanoseconds;
            std::sort(sorted.begin(), sorted.end());
            long double sum = 0.0;
            for (const auto value : sorted)
                sum += static_cast<long double>(value);
            result.latencyAverageMs = static_cast<double>(sum / sorted.size()) / 1.0e6;
            const std::size_t p95 = std::min(sorted.size() - 1, static_cast<std::size_t>(std::ceil(sorted.size() * 0.95)) - 1);
            result.latencyP95Ms = static_cast<double>(sorted[p95]) / 1.0e6;
            result.latencyMaxMs = static_cast<double>(sorted.back()) / 1.0e6;
        }
        return result;
    }

    void TerrainAsync::invalidate(Landscape::Look look) {
        const std::scoped_lock lock(state->mutex);
        state->look = std::move(look);
        for (const auto& [key, record] : state->records)
            if (record.lifecycle != Lifecycle::resident and record.lifecycle != Lifecycle::generating)
                ++state->discardedTotal;
        state->records.clear();
        state->wake.notify_all();
    }

    auto Planetoid::largestBasinDirection(const Look& look) -> vec3 {
        return makeBasins(look.seed).craters[0].center;
    }

    auto Planetoid::placed(Reading context) -> bool {
        return with<Thing>::get_global(context).landscape.has_value();
    }

    void Planetoid::place(Writing context, system::Device::Id device, Pose pose, Look look) {
        {
            const std::scoped_lock lock(terrainTelemetryMutex());
            terrainTelemetry() = TerrainTelemetry{};
        }
        const auto material = with<resource::Assets>::find<resource::material::Asset>(context, resource::Unit::Name::from("Eltanin", "planetoid"));
        if (not material)
            return (void)context.refuse("eltanin::locality::geo::Planetoid::place: planetoid material missing");
        const auto crust = with<resource::Assets>::find<resource::texpack::Pack>(context, resource::Unit::Name::from("Eltanin", "crust"));
        if (not crust)
            return (void)context.refuse("eltanin::locality::geo::Planetoid::place: crust pack missing");
        auto& landscape = with<Thing>::modify_global(context)->landscape;
        terrainAsyncState().reset();
        for (const auto& entry : terrainStagingPatches())
            dropPatch(context, entry.second);
        terrainStagingPatches().clear();
        if (landscape) {
            for (const auto& entry : landscape->patches)
                dropPatch(context, entry.second);
            landscape->patches.clear();
            if (landscape->atmosphere)
                dropActor(context, *landscape->atmosphere);
        }
        const auto well = landscape ? landscape->well : makeWell(context, pose, look);
        bindWell(*with<phys::Body>::modify(context, well), pose, look);
        landscape = Landscape{.look = look, .pose = pose, .device = device, .well = well, .material = *material, .crust = *crust, .patches = {}, .atmosphere = {}, .debugView = Landscape::DebugView::normal};
        terrainAsyncState() = std::make_shared<TerrainAsync>(look);
        with<scene::Root>::modify(context, with<Thing>::get_global(context).scene)->atmosphereDensity = look.atmosphere.seaDensity;
        with<scene::Root>::modify(context, with<Thing>::get_global(context).scene)->atmosphereKerman = look.atmosphere.kerman;
        spawnAtmosphere(context, *landscape);
    }

    void Planetoid::setDebugView(Writing context, Landscape::DebugView view) {
        auto& landscape = with<Thing>::modify_global(context)->landscape;
        if (not landscape or landscape->debugView == view)
            return;
        const auto material = view == Landscape::DebugView::normal ? base::maybe<resource::material::Asset::Id>{landscape->material} : with<resource::Assets>::find<resource::material::Asset>(context, resource::Unit::Name::from("Eltanin", "terrainDebug"));
        if (not material)
            return (void)context.refuse("Planetoid: diagnostic material missing");
        const bool rebuild = (view == Landscape::DebugView::wireframe) != (landscape->debugView == Landscape::DebugView::wireframe);
        auto applyView = [&](PatchMap& patches) {
            for (auto& [key, patch] : patches) {
                auto mesh = with<scene::actor::Mesh>::composeWithTexpack(context, patch.geometry, *material, landscape->crust);
                if (not mesh)
                    return false;
                with<scene::actor::Mesh>::replace(context, patch.actor, std::move(*mesh));
                with<scene::actor::MeshState>::modify(context, patch.actor)->heat.x = static_cast<float>(view);
            }
            return true;
        };
        if (not applyView(landscape->patches) or not applyView(terrainStagingPatches()))
            return (void)context.refuse("Planetoid: diagnostic mesh compose failed");
        landscape->debugView = view;
        if (rebuild and terrainAsyncState())
            terrainAsyncState()->invalidate(landscape->look);
        if (landscape->atmosphere and with<scene::Node>::exists(context, *landscape->atmosphere))
            with<scene::Node>::modify(context, *landscape->atmosphere)->visible = view == Landscape::DebugView::normal and landscape->look.terrainTest == Landscape::TerrainTest::off;
    }

    void Planetoid::setTerrainTest(Writing context, Landscape::TerrainTest test, bool rims) {
        auto& landscape = with<Thing>::modify_global(context)->landscape;
        if (not landscape or (landscape->look.terrainTest == test and landscape->look.testRims == rims)) return;
        landscape->look.terrainTest = test;
        landscape->look.testRims = rims;
        if (terrainAsyncState())
            terrainAsyncState()->invalidate(landscape->look);
        // The next regular update rebuilds only the currently needed LOD leaves.
        for (const auto& entry : landscape->patches) dropPatch(context, entry.second);
        landscape->patches.clear();
        for (const auto& entry : terrainStagingPatches()) dropPatch(context, entry.second);
        terrainStagingPatches().clear();
        if (landscape->atmosphere and with<scene::Node>::exists(context, *landscape->atmosphere))
            with<scene::Node>::modify(context, *landscape->atmosphere)->visible = test == Landscape::TerrainTest::off and landscape->debugView == Landscape::DebugView::normal;
    }

    void Planetoid::update(Writing context, Pos camera) {
        auto& landscape = with<Thing>::modify_global(context)->landscape;
        if (not landscape)
            return;
        {
            const std::scoped_lock lock(terrainTelemetryMutex());
            ++terrainTelemetry().frames;
        }
        const auto scene = with<Thing>::get_global(context).scene;
        landscape->look.atmosphere.seaDensity = with<scene::Root>::get(context, scene).atmosphereDensity;
        landscape->look.atmosphere.kerman = with<scene::Root>::get(context, scene).atmosphereKerman;
        if (landscape->atmosphere and with<scene::actor::MeshState>::exists(context, *landscape->atmosphere))
            bindAtmosphereMesh(*with<scene::actor::MeshState>::modify(context, *landscape->atmosphere), *landscape);
        vector<PatchKey> wanted;
        wanted.reserve(96);
        for (int face = 0; face < faceCount; ++face)
            collectLeaves(*landscape, camera, PatchKey{.face = static_cast<std::uint8_t>(face), .level = 0, .iu = 0, .iv = 0}, wanted);
        LeafSet keep;
        keep.reserve(wanted.size());
        for (const PatchKey& key : wanted)
            keep.insert(key);
        restrictLeaves(keep);
        stripCovered(keep);
        wanted.clear();
        wanted.reserve(keep.size());
        for (const PatchKey& key : keep)
            wanted.push_back(key);
        if (not terrainAsyncState())
            terrainAsyncState() = std::make_shared<TerrainAsync>(landscape->look);
        const bool wireframe = landscape->debugView == Landscape::DebugView::wireframe;
        auto& staging = terrainStagingPatches();
        auto matches = [&](const PatchMap& patches, const PatchKey& key, std::uint8_t edges) {
            const auto found = patches.find(key);
            return found != patches.end() and found->second.coarserEdges == edges and found->second.wireframe == wireframe;
        };

        for (auto it = staging.begin(); it != staging.end();) {
            const auto edges = keep.contains(it->first) ? edgeFlags(keep, it->first) : std::uint8_t{0};
            if (not keep.contains(it->first) or it->second.coarserEdges != edges or it->second.wireframe != wireframe or matches(landscape->patches, it->first, edges)) {
                dropPatch(context, it->second);
                it = staging.erase(it);
            } else {
                ++it;
            }
        }

        vector<TerrainAsync::Desired> desired;
        desired.reserve(wanted.size());
        for (const PatchKey& key : wanted) {
            const auto edges = edgeFlags(keep, key);
            const bool residentMatches = matches(landscape->patches, key, edges) or matches(staging, key, edges);
            const double distance = static_cast<double>(glm::length(camera - patchCenter(*landscape, key)));
            const double detailPenalty = static_cast<double>(key.level) * static_cast<double>(landscape->look.radius) * 0.20;
            desired.push_back(TerrainAsync::Desired{.key = key, .coarserEdges = edges, .wireframe = wireframe, .residentMatches = residentMatches, .priority = distance + detailPenalty});
        }
        terrainAsyncState()->reconcile(desired);

        constexpr std::size_t maxCommitsPerFrame = 4;
        std::uint32_t created = 0;
        std::uint32_t rebuilt = 0;
        for (auto& completed : terrainAsyncState()->takeCompleted(maxCommitsPerFrame)) {
            const bool wasResident = landscape->patches.contains(completed.key);
            if (commitPatch(context, *landscape, staging, completed.key, completed.coarserEdges, completed.wireframe, false, std::move(completed.cpu))) {
                terrainAsyncState()->markResident(completed.key, completed.coarserEdges, completed.wireframe);
                terrainAsyncState()->noteCommitted();
                if (wasResident)
                    ++rebuilt;
                else
                    ++created;
            }
        }

        bool ready = true;
        for (const PatchKey& key : wanted) {
            const auto edges = edgeFlags(keep, key);
            if (not matches(landscape->patches, key, edges) and not matches(staging, key, edges)) {
                ready = false;
                break;
            }
        }
        bool transitionNeeded = not staging.empty();
        for (const auto& [key, patch] : landscape->patches) {
            if (not keep.contains(key) or patch.coarserEdges != edgeFlags(keep, key) or patch.wireframe != wireframe) {
                transitionNeeded = true;
                break;
            }
        }
        if (ready and transitionNeeded) {
            for (auto it = landscape->patches.begin(); it != landscape->patches.end();) {
                if (keep.contains(it->first) and it->second.coarserEdges == edgeFlags(keep, it->first) and it->second.wireframe == wireframe) {
                    ++it;
                    continue;
                }
                dropPatch(context, it->second);
                it = landscape->patches.erase(it);
            }
            for (auto& [key, patch] : staging) {
                if (with<scene::Node>::exists(context, patch.actor))
                    with<scene::Node>::modify(context, patch.actor)->visible = true;
                landscape->patches.emplace(key, std::move(patch));
            }
            staging.clear();
        }
        const auto started = terrainAsyncState()->takeStartedSinceFrame();
        {
            const std::scoped_lock lock(terrainTelemetryMutex());
            auto& telemetry = terrainTelemetry();
            telemetry.createdPerFrame.push_back(created);
            telemetry.rebuiltPerFrame.push_back(rebuilt);
            telemetry.jobsStartedPerFrame.push_back(started);
            telemetry.jobsCommittedPerFrame.push_back(created + rebuilt);
        }
        reportTerrainTelemetry(*landscape);
    }

    auto Planetoid::height(Reading context, vec3 dir) -> float {
        const auto& landscape = with<Thing>::get_global(context).landscape;
        if (not landscape)
            return 0.0f;
        {
            const std::scoped_lock lock(terrainTelemetryMutex());
            ++terrainTelemetry().physicalHeightQueries;
        }
        return heightOf(dir, landscape->look);
    }

    auto Planetoid::altitudeAt(Reading context, Pos worldPos) -> float {
        const auto& landscape = with<Thing>::get_global(context).landscape;
        if (not landscape)
            return 0.0f;
        {
            const std::scoped_lock lock(terrainTelemetryMutex());
            ++terrainTelemetry().altitudeQueries;
        }
        const vec3 local = toLocal(*landscape, worldPos);
        const float radial = glm::length(local);
        if (radial < 1.0e-6f)
            return -landscape->look.radius;
        const vec3 dir = local / radial;
        return radial - (landscape->look.radius + heightOf(dir, landscape->look));
    }

    auto Planetoid::gravityAt(Reading context, dvec3 worldPos) -> dvec3 {
        const auto& landscape = with<Thing>::get_global(context).landscape;
        if (not landscape)
            return dvec3{0.0, 0.0, 0.0};
        const dvec3 offset = worldPos - dvec3{landscape->pose.position};
        const double distance = glm::length(offset);
        if (distance < 1.0e-12)
            return dvec3{0.0, 0.0, 0.0};
        const double radius = double(landscape->look.radius);
        const double surface = double(landscape->look.surfaceAcceleration);
        const double accelScale = distance < radius ? -surface / radius : -surface * radius * radius / (distance * distance * distance);
        return offset * accelScale;
    }

    auto Planetoid::atmosphereRadius(Reading context) -> float {
        const auto& landscape = with<Thing>::get_global(context).landscape;
        if (not landscape)
            return 0.0f;
        return landscape->look.atmosphere.radius;
    }

    auto Planetoid::seaDensity(Reading context) -> float {
        const auto& landscape = with<Thing>::get_global(context).landscape;
        if (not landscape)
            return 0.0f;
        return landscape->look.atmosphere.seaDensity;
    }

    auto Planetoid::airDensity(Reading context, Pos worldPos) -> float {
        const auto& landscape = with<Thing>::get_global(context).landscape;
        if (not landscape)
            return 0.0f;
        return phys::Settings::Air::density(glm::length(worldPos) - landscape->look.radius, landscape->look.atmosphere.seaDensity, landscape->look.atmosphere.kerman);
    }

    auto Planetoid::windAt(Reading, dvec3) -> dvec3 {
        return dvec3{0.0, 0.0, 0.0};
    }

    auto Planetoid::surfaceInfo(Reading context, vec3 dir) -> Surface {
        const auto& landscape = with<Thing>::get_global(context).landscape;
        if (not landscape)
            return Surface{.height = 0.0f, .position = vec3{0.0f, 0.0f, 0.0f}, .normal = vec3{0.0f, 1.0f, 0.0f}, .mix = 0, .slope = 0.0f};
        {
            const std::scoped_lock lock(terrainTelemetryMutex());
            ++terrainTelemetry().physicalHeightQueries;
        }
        const float len = glm::length(dir);
        if (len < 1.0e-6f)
            return Surface{.height = 0.0f, .position = vec3{0.0f, 0.0f, 0.0f}, .normal = vec3{0.0f, 1.0f, 0.0f}, .mix = 0, .slope = 0.0f};
        dir /= len;
        const Relief relief = reliefOf(dir, landscape->look);
        const vec3 position = dir * (landscape->look.radius + relief.height);
        const vec3 normal = gradientNormal(dir, position, landscape->look);
        const float slope = 1.0f - glm::clamp(glm::dot(normal, dir), 0.0f, 1.0f);
        return Surface{.height = relief.height, .position = position, .normal = normal, .mix = materialMix(dir, relief.height, slope, relief.crater, landscape->look), .slope = slope};
    }

}
