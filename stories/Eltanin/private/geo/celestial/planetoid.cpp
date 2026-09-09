#include "geo/celestial/planetiod.h"

#include "geo/stones/crust.h"

#include <eltanin/locality/thing.q1.h>
#include <eltanin/physics/body.q1.h>
#include <eltanin/physics/rigid.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/resources/texture3array.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/node.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/semantics/geometry.h>

#include <base/maybe.h>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace eltanin::locality::geo {

    using namespace fqsm::api;
    using namespace rmmr;

    using Mix = std::uint64_t;

    namespace {

        constexpr int mixChannels = 16;
        constexpr int grid = 33;
        constexpr int cells = 32;
        constexpr int faceCount = 6;
        constexpr int maxLevel = 5;
        constexpr float splitNear = 2.8f;
        constexpr float splitKeep = 3.7f;
        constexpr int mineralIce = 0;
        constexpr int mineralOlivine = 1;
        constexpr int mineralPyroxene = 2;
        constexpr int mineralFeldspar = 3;
        constexpr int mineralCarbonaceous = 5;
        constexpr int mineralIron = 6;
        constexpr int mineralOxides = 9;
        constexpr int mineralSalts = 14;
        constexpr bool flipWinding[faceCount] = {false, true, true, false, false, true};

        struct PatchKey {
            std::uint8_t face;
            std::uint8_t level;
            std::uint16_t iu;
            std::uint16_t iv;

            auto operator==(const PatchKey&) const -> bool = default;
        };

        struct PatchKeyHash {
            auto operator()(const PatchKey& key) const noexcept -> std::size_t {
                return (static_cast<std::size_t>(key.face) << 40) ^ (static_cast<std::size_t>(key.level) << 32) ^ (static_cast<std::size_t>(key.iu) << 16) ^ static_cast<std::size_t>(key.iv);
            }
        };

        struct Patch {
            scene::actor::Mesh::Id actor;
            resource::geometry::Asset::Id geometry;
            std::uint8_t coarserEdges;
        };

        struct Runtime {
            Planetoid::Look look;
            Pose pose;
            system::Device::Id device;
            phys::Body::Id well;
            resource::material::Asset::Id material;
            resource::texture3array::Asset::Id crust;
            std::unordered_map<PatchKey, Patch, PatchKeyHash> patches;
        };

        base::maybe<Runtime> runtime;

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

        struct FaceUv {
            int face;
            float u;
            float v;
        };

        auto faceUv(vec3 dir) -> FaceUv {
            const vec3 extent = glm::abs(dir);
            if (extent.x >= extent.y and extent.x >= extent.z)
                return FaceUv{.face = dir.x >= 0.0f ? 0 : 1, .u = dir.y / extent.x, .v = dir.z / extent.x};
            if (extent.y >= extent.x and extent.y >= extent.z)
                return FaceUv{.face = dir.y >= 0.0f ? 2 : 3, .u = dir.x / extent.y, .v = dir.z / extent.y};
            return FaceUv{.face = dir.z >= 0.0f ? 4 : 5, .u = dir.x / extent.z, .v = dir.y / extent.z};
        }

        struct FaceCell {
            int face;
            int iu;
            int iv;
        };

        auto wrapFaceCell(int face, int iu, int iv, int cells) -> FaceCell {
            const int last = cells - 1;
            for (int pass = 0; pass < 2; ++pass) {
                if (iu >= 0 and iu < cells and iv >= 0 and iv < cells)
                    return FaceCell{.face = face, .iu = iu, .iv = iv};
                if (iu < 0 or iu >= cells) {
                    const bool high = iu >= cells;
                    const int into = high ? iu - cells : -1 - iu;
                    switch (face) {
                        case 0: face = high ? 2 : 3; iu = last - into; break;
                        case 1: face = high ? 2 : 3; iu = into; break;
                        case 2: face = high ? 0 : 1; iu = last - into; break;
                        case 3: face = high ? 0 : 1; iu = into; break;
                        case 4: { const int along = iv; face = high ? 0 : 1; iu = along; iv = last - into; break; }
                        default: { const int along = iv; face = high ? 0 : 1; iu = along; iv = into; break; }
                    }
                    continue;
                }
                const bool high = iv >= cells;
                const int into = high ? iv - cells : -1 - iv;
                switch (face) {
                    case 0: { const int along = iu; face = high ? 4 : 5; iu = last - into; iv = along; break; }
                    case 1: { const int along = iu; face = high ? 4 : 5; iu = into; iv = along; break; }
                    case 2: face = high ? 4 : 5; iv = last - into; break;
                    case 3: face = high ? 4 : 5; iv = into; break;
                    case 4: face = high ? 2 : 3; iv = last - into; break;
                    default: face = high ? 2 : 3; iv = into; break;
                }
            }
            return FaceCell{.face = face, .iu = glm::clamp(iu, 0, last), .iv = glm::clamp(iv, 0, last)};
        }

        auto unitGauss(float first, float second) -> float {
            return std::sqrt(-2.0f * std::log(glm::max(first, 1.0e-6f))) * std::cos(second * 2.0f * std::numbers::pi_v<float>);
        }

        auto craterRadius(int face, int iu, int iv, int seed) -> float {
            constexpr float radiusMin = 0.10f;
            constexpr float radiusMax = 0.30f;
            constexpr float radiusMean = 0.136f;
            constexpr float radiusSigma = 0.055f;
            const float sample = unitGauss(hash31(face, iu, iv, seed + 4), hash31(face, iu, iv, seed + 6));
            return glm::clamp(radiusMean + radiusSigma * sample, radiusMin, radiusMax);
        }

        auto craterField(vec3 dir, integer seed) -> float {
            constexpr int faceCells = 8;
            constexpr float cellSize = 2.0f / static_cast<float>(faceCells);
            const FaceUv coord = faceUv(dir);
            const int originU = static_cast<int>(std::floor((coord.u + 1.0f) / cellSize));
            const int originV = static_cast<int>(std::floor((coord.v + 1.0f) / cellSize));
            const int craterSeed = static_cast<int>(seed) + 40;
            float height = 0.0f;
            int seen[25];
            int seenCount = 0;
            for (int offsetV = -2; offsetV <= 2; ++offsetV) {
                for (int offsetU = -2; offsetU <= 2; ++offsetU) {
                    const FaceCell cellId = wrapFaceCell(coord.face, originU + offsetU, originV + offsetV, faceCells);
                    const int packed = (cellId.face << 16) ^ (cellId.iu << 8) ^ cellId.iv;
                    bool duplicate = false;
                    for (int index = 0; index < seenCount; ++index)
                        duplicate = duplicate or seen[index] == packed;
                    if (duplicate)
                        continue;
                    seen[seenCount++] = packed;
                    const float jitterU = (hash31(cellId.face, cellId.iu, cellId.iv, craterSeed + 1) * 2.0f - 1.0f) * 0.28f * cellSize;
                    const float jitterV = (hash31(cellId.face, cellId.iu, cellId.iv, craterSeed + 2) * 2.0f - 1.0f) * 0.28f * cellSize;
                    const float craterU = -1.0f + (static_cast<float>(cellId.iu) + 0.5f) * cellSize + jitterU;
                    const float craterV = -1.0f + (static_cast<float>(cellId.iv) + 0.5f) * cellSize + jitterV;
                    const vec3 crater = cubeDir(cellId.face, craterU, craterV);
                    const float ang = std::sqrt(glm::max(0.0f, 2.0f - 2.0f * glm::clamp(glm::dot(dir, crater), -1.0f, 1.0f)));
                    const float radius = craterRadius(cellId.face, cellId.iu, cellId.iv, craterSeed);
                    const float bowlT = ang / radius;
                    if (bowlT > 1.55f)
                        continue;
                    const float depth = 0.35f + 0.80f * hash31(cellId.face, cellId.iu, cellId.iv, craterSeed + 5);
                    const float bowl = bowlT < 1.0f ? (bowlT * bowlT - 1.0f) * depth : 0.0f;
                    const float rimT = (bowlT - 1.02f) / 0.14f;
                    const float rim = std::exp(-rimT * rimT) * 0.30f * depth;
                    height += bowl + rim;
                }
            }
            return glm::clamp(height, -1.4f, 0.55f);
        }

        struct Relief {
            float height;
            float crater;
        };

        auto reliefOf(vec3 dir, const Planetoid::Look& look) -> Relief {
            const float len = glm::length(dir);
            if (len < 1.0e-6f)
                return Relief{.height = 0.0f, .crater = 0.0f};
            dir /= len;
            const int seed = static_cast<int>(look.seed);
            const vec3 warp = vec3{signedNoise(dir * 2.1f, seed + 3), signedNoise(vec3{dir.y, dir.z, dir.x} * 2.1f, seed + 7), signedNoise(vec3{dir.z, dir.x, dir.y} * 2.1f, seed + 11)};
            const vec3 warped = glm::normalize(dir + warp * 0.14f);
            const float shape = fbm(warped * 2.4f, seed) * 0.55f + signedNoise(warped * 8.0f, seed + 13) * 0.18f;
            float ridged = 1.0f - std::abs(signedNoise(warped * 5.4f, seed + 19));
            ridged = ridged * ridged;
            const float craters = craterField(dir, look.seed);
            const float combined = 0.48f * shape + look.ridge * (ridged * 2.0f - 1.0f) + 0.34f * craters;
            const float base = glm::clamp(combined, -1.0f, 1.0f) * look.maxRelief;
            const float bowlDamp = glm::smoothstep(0.0f, 0.85f, glm::clamp(-craters, 0.0f, 1.4f) / 1.4f);
            const float rimBoost = glm::smoothstep(0.04f, 0.40f, glm::clamp(craters, 0.0f, 0.55f));
            const float heightBoost = glm::smoothstep(-0.25f, 0.55f, combined);
            float gritAmp = 0.55f * (1.0f - 0.88f * bowlDamp) * (1.0f + 2.0f * rimBoost) * (1.0f + 1.35f * ridged * ridged) * (0.40f + 0.60f * heightBoost);
            const float grit = 0.62f * signedNoise(dir * (look.radius / 8.0f), seed + 53) + 0.38f * signedNoise(dir * (look.radius / 4.0f), seed + 59);
            return Relief{.height = base + grit * gritAmp, .crater = craters};
        }

        auto heightOf(vec3 dir, const Planetoid::Look& look) -> float {
            return reliefOf(dir, look).height;
        }

        auto packMix(const std::array<float, mixChannels>& weights) -> Mix {
            std::array<int, 4> best{-1, -1, -1, -1};
            std::array<float, 4> bestW{0.0f, 0.0f, 0.0f, 0.0f};
            for (int channel = 0; channel < mixChannels; ++channel) {
                const float weight = weights[static_cast<std::size_t>(channel)];
                if (weight <= bestW[3])
                    continue;
                int slot = 3;
                while (slot > 0 and weight > bestW[static_cast<std::size_t>(slot - 1)]) {
                    best[static_cast<std::size_t>(slot)] = best[static_cast<std::size_t>(slot - 1)];
                    bestW[static_cast<std::size_t>(slot)] = bestW[static_cast<std::size_t>(slot - 1)];
                    --slot;
                }
                best[static_cast<std::size_t>(slot)] = channel;
                bestW[static_cast<std::size_t>(slot)] = weight;
            }
            float mass = 0.0f;
            for (float weight : bestW)
                mass += weight;
            if (mass <= 1.0e-6f)
                return Mix{15} << (mineralOlivine * 4);
            Mix packed = 0;
            int used = 0;
            int lastLive = -1;
            for (int slot = 0; slot < 4; ++slot) {
                if (best[static_cast<std::size_t>(slot)] < 0 or bestW[static_cast<std::size_t>(slot)] <= 0.0f)
                    continue;
                const int nibble = static_cast<int>(bestW[static_cast<std::size_t>(slot)] / mass * 15.0f + 0.5f);
                packed |= Mix{static_cast<std::uint64_t>(glm::clamp(nibble, 0, 15))} << (best[static_cast<std::size_t>(slot)] * 4);
                used += nibble;
                lastLive = best[static_cast<std::size_t>(slot)];
            }
            if (used == 0)
                return Mix{15} << (mineralOlivine * 4);
            if (used != 15 and lastLive >= 0) {
                const int current = static_cast<int>((packed >> (lastLive * 4)) & 15);
                const int adjusted = glm::clamp(current + (15 - used), 0, 15);
                packed &= ~(Mix{15} << (lastLive * 4));
                packed |= Mix{static_cast<std::uint64_t>(adjusted)} << (lastLive * 4);
            }
            return packed;
        }

        auto materialMix(vec3 dir, float height, float slope, float crater, const Planetoid::Look& look) -> Mix {
            const int seed = static_cast<int>(look.seed);
            const float polar = std::abs(dir.y);
            const float altitude = look.maxRelief > 1.0e-4f ? height / look.maxRelief : 0.0f;
            const float speckle = signedNoise(dir * 18.0f, seed + 29);
            const float bowl = glm::clamp(-crater, 0.0f, 1.0f);
            std::array<float, mixChannels> weights{};
            weights[mineralOlivine] = 0.50f * (1.0f - slope * 0.65f) * (1.0f - polar * 0.35f);
            weights[mineralPyroxene] = 0.42f * (0.45f + 0.55f * (0.5f + 0.5f * altitude));
            weights[mineralFeldspar] = 0.38f * glm::max(0.0f, altitude) * (1.0f - slope) * (0.7f + 0.3f * speckle);
            weights[mineralIce] = 0.62f * glm::smoothstep(0.58f, 0.90f, polar) + 0.22f * glm::max(0.0f, -altitude) * polar;
            weights[mineralIron] = 0.55f * bowl * (1.0f - polar * 0.5f);
            weights[mineralOxides] = 0.28f * slope;
            weights[mineralCarbonaceous] = 0.22f * glm::max(0.0f, -altitude) * (1.0f - polar);
            weights[mineralSalts] = 0.16f * glm::max(0.0f, -altitude) * (1.0f - slope) * glm::smoothstep(0.35f, 0.75f, polar);
            return packMix(weights);
        }

        auto cohesionOf(vec3 dir, float height, float slope, float crater, const Planetoid::Look& look) -> float {
            const int seed = static_cast<int>(look.seed);
            const float polar = std::abs(dir.y);
            const float altitude = look.maxRelief > 1.0e-4f ? height / look.maxRelief : 0.0f;
            const float dust = 0.5f + 0.5f * signedNoise(dir * 14.0f, seed + 47);
            const float bowl = glm::clamp(-crater, 0.0f, 1.0f);
            const float ice = glm::smoothstep(0.58f, 0.90f, polar);
            float packed = 0.22f * glm::max(0.0f, altitude) * (1.0f - slope);
            packed += 0.18f * slope;
            packed += 0.28f * bowl;
            packed += 0.20f * ice;
            packed = glm::clamp(packed, 0.0f, 1.0f);
            const float loose = 0.02f + 0.14f * dust;
            return glm::clamp(loose + 0.54f * packed * packed, 0.0f, 0.7f);
        }

        auto surfacePoint(vec3 dir, const Planetoid::Look& look) -> vec3 {
            return dir * (look.radius + heightOf(dir, look));
        }

        auto gradientNormal(vec3 dir, const Planetoid::Look& look) -> vec3 {
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
            const vec3 origin = surfacePoint(dir, look);
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

        auto toLocal(const Runtime& state, Pos worldPos) -> vec3 {
            return glm::inverse(state.pose.rotation) * (worldPos - state.pose.position);
        }

        auto tileRange(int level, int index) -> float {
            return -1.0f + static_cast<float>(index) * (2.0f / static_cast<float>(1 << level));
        }

        auto patchCenter(const Runtime& state, const PatchKey& key) -> vec3 {
            const float tileSize = 2.0f / static_cast<float>(1 << key.level);
            const float faceU = tileRange(key.level, key.iu) + tileSize * 0.5f;
            const float faceV = tileRange(key.level, key.iv) + tileSize * 0.5f;
            return state.pose.position + state.pose.rotation * (cubeDir(key.face, faceU, faceV) * state.look.radius);
        }

        void emitTri(resource::builders::geometry::CpuPresentation& cpu, integer first, integer second, integer third) {
            cpu.indices.push_back(first);
            cpu.indices.push_back(second);
            cpu.indices.push_back(third);
        }

        auto buildPatch(const Planetoid::Look& look, const PatchKey& key, std::uint8_t coarserEdges) -> resource::builders::geometry::CpuPresentation {
            resource::builders::geometry::CpuPresentation cpu{
                .layout = primitive::GeometrySemantics::layoutIds(vector<string>{"position", "normal", "mix0", "cohesion"}),
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
            cpu.positions.reserve(static_cast<std::size_t>(mainCount));
            cpu.normals.reserve(cpu.positions.capacity());
            cpu.mix0.reserve(cpu.positions.capacity());
            cpu.cohesion.reserve(cpu.positions.capacity());
            cpu.indices.reserve(static_cast<std::size_t>(cells * cells * 6));

            vector<float> heights;
            vector<float> craters;
            heights.reserve(static_cast<std::size_t>(mainCount));
            craters.reserve(static_cast<std::size_t>(mainCount));
            for (int row = 0; row < grid; ++row) {
                const float faceV = originV + tileSize * (static_cast<float>(row) / static_cast<float>(cells));
                for (int column = 0; column < grid; ++column) {
                    const float faceU = originU + tileSize * (static_cast<float>(column) / static_cast<float>(cells));
                    const vec3 dir = cubeDir(key.face, faceU, faceV);
                    const Relief relief = reliefOf(dir, look);
                    heights.push_back(relief.height);
                    craters.push_back(relief.crater);
                    cpu.positions.push_back(dir * (look.radius + relief.height));
                    cpu.normals.push_back(dir);
                    cpu.mix0.push_back(0);
                    cpu.cohesion.push_back(0.0f);
                }
            }

            auto gridIndex = [&](int column, int row) -> std::size_t {
                return static_cast<std::size_t>(row * grid + column);
            };
            for (int row = 0; row < grid; ++row) {
                for (int column = 0; column < grid; ++column) {
                    const std::size_t index = gridIndex(column, row);
                    const int columnPrev = column > 0 ? column - 1 : column;
                    const int columnNext = column < cells ? column + 1 : column;
                    const int rowPrev = row > 0 ? row - 1 : row;
                    const int rowNext = row < cells ? row + 1 : row;
                    const vec3 alongU = cpu.positions[gridIndex(columnNext, row)] - cpu.positions[gridIndex(columnPrev, row)];
                    const vec3 alongV = cpu.positions[gridIndex(column, rowNext)] - cpu.positions[gridIndex(column, rowPrev)];
                    vec3 normal = glm::cross(alongU, alongV);
                    const float normalLen = glm::length(normal);
                    const vec3 dir = glm::normalize(cpu.positions[index]);
                    if (normalLen > 1.0e-8f)
                        normal /= normalLen;
                    else
                        normal = dir;
                    if (glm::dot(normal, dir) < 0.0f)
                        normal = -normal;
                    cpu.normals[index] = normal;
                    const float slope = 1.0f - glm::clamp(glm::dot(normal, dir), 0.0f, 1.0f);
                    cpu.mix0[index] = materialMix(dir, heights[index], slope, craters[index], look);
                    cpu.cohesion[index] = cohesionOf(dir, heights[index], slope, craters[index], look);
                }
            }

            auto snapOdd = [&](int origin, int stride) {
                for (int step = 1; step < grid; step += 2) {
                    const int mid = origin + step * stride;
                    const int prev = origin + (step - 1) * stride;
                    const int next = origin + (step + 1) * stride;
                    cpu.positions[static_cast<std::size_t>(mid)] = 0.5f * (cpu.positions[static_cast<std::size_t>(prev)] + cpu.positions[static_cast<std::size_t>(next)]);
                    vec3 normal = cpu.normals[static_cast<std::size_t>(prev)] + cpu.normals[static_cast<std::size_t>(next)];
                    const float normalLen = glm::length(normal);
                    cpu.normals[static_cast<std::size_t>(mid)] = normalLen > 1.0e-8f ? normal / normalLen : cpu.normals[static_cast<std::size_t>(prev)];
                    cpu.cohesion[static_cast<std::size_t>(mid)] = 0.5f * (cpu.cohesion[static_cast<std::size_t>(prev)] + cpu.cohesion[static_cast<std::size_t>(next)]);
                }
            };
            if (coarserEdges & 1u)
                snapOdd(0, grid);
            if (coarserEdges & 2u)
                snapOdd(cells, grid);
            if (coarserEdges & 4u)
                snapOdd(0, 1);
            if (coarserEdges & 8u)
                snapOdd(cells * grid, 1);

            const bool flip = flipWinding[key.face];
            for (int row = 0; row < cells; ++row) {
                for (int column = 0; column < cells; ++column) {
                    const integer indexA = row * grid + column;
                    const integer indexB = indexA + 1;
                    const integer indexC = indexA + grid;
                    const integer indexD = indexC + 1;
                    if (flip) {
                        emitTri(cpu, indexA, indexC, indexD);
                        emitTri(cpu, indexA, indexD, indexB);
                    } else {
                        emitTri(cpu, indexA, indexB, indexD);
                        emitTri(cpu, indexA, indexD, indexC);
                    }
                }
            }
            return cpu;
        }

        auto childKey(const PatchKey& key, int childU, int childV) -> PatchKey {
            return PatchKey{.face = key.face, .level = static_cast<std::uint8_t>(key.level + 1), .iu = static_cast<std::uint16_t>(key.iu * 2 + childU), .iv = static_cast<std::uint16_t>(key.iv * 2 + childV)};
        }

        auto hasChild(const Runtime& state, const PatchKey& key) -> bool {
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

        void collectLeaves(const Runtime& state, Pos camera, PatchKey key, vector<PatchKey>& wanted) {
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

        void dropPatch(Writing context, Patch patch) {
            if (with<scene::Node>::exists(context, patch.actor))
                with<scene::Node>::modify(context, patch.actor)->visible = false;
            const auto scene = with<Thing>::get_global(context).scene;
            if (with<scene::Node_group>::exists(context, scene) and with<scene::Node_group>::get(context, scene).contains(patch.actor))
                with<scene::Node_group>::deleteElement(context, scene, patch.actor);
            else if (with<scene::Node>::exists(context, patch.actor))
                with<scene::Node>::remove(context, patch.actor);
        }

        auto spawnPatch(Writing context, Runtime& state, const PatchKey& key, std::uint8_t coarserEdges) -> bool {
            const auto scene = with<Thing>::get_global(context).scene;
            auto cpu = buildPatch(state.look, key, coarserEdges);
            if (cpu.positions.empty())
                return false;
            const string own = "planetoid-" + std::to_string(key.face) + "-" + std::to_string(key.level) + "-" + std::to_string(key.iu) + "-" + std::to_string(key.iv);
            const auto manager = with<resource::Manager>::singleton(context);
            const auto geometryId = with<resource::Unit_group>::addElement(context, manager, resource::Unit::Quantum{.name = resource::Unit::Name::from("Eltanin", own)});
            with<resource::geometry::Asset>::extend(context, geometryId, resource::geometry::Asset::Quantum{});
            if (not with<resource::geometry::Asset>::install(context, geometryId, state.device, cpu)) {
                context.refuse("eltanin::locality::geo::Planetoid: geometry install failed");
                return false;
            }
            const auto& runtimes = with<resource::Runtimes>::get(context, state.device);
            if (runtimes.texture3arrays_id_mapping.find(state.crust) == runtimes.texture3arrays_id_mapping.end()) {
                if (not with<resource::texture3array::Asset>::install(context, state.crust, state.device, generateCrust())) {
                    context.refuse("eltanin::locality::geo::Planetoid: crust install failed");
                    return false;
                }
            }
            auto meshQuantum = with<scene::actor::Mesh>::composeOne(context, geometryId, state.material, state.crust);
            if (not meshQuantum) {
                context.refuse("eltanin::locality::geo::Planetoid: mesh compose failed");
                return false;
            }
            auto meshState = with<scene::actor::MeshState>::defaults(RGB{1.0f, 1.0f, 1.0f}, 1.0f);
            meshState.patternScale = glm::max(0.5f, state.look.radius * 2.0f);
            const auto actor = with<scene::Interface>::createMeshActor(context, scene, state.pose, std::move(*meshQuantum), meshState);
            state.patches.emplace(key, Patch{.actor = actor, .geometry = geometryId, .coarserEdges = coarserEdges});
            return true;
        }

        auto makeWell(Writing context, Pose pose, const Planetoid::Look& look) -> phys::Body::Id {
            const float volume = (4.0f / 3.0f) * std::numbers::pi_v<float> * look.radius * look.radius * look.radius;
            const float mass = volume * 3000.0f;
            vector<phys::Particle> particles;
            particles.push_back(phys::Particle{phys::Matter{.position = dvec3{pose.position}, .mass = mass, .temperature = 0.0f, .cohesion = 1.0f}, dvec3{pose.position}, vec3{0.0f, 0.0f, 0.0f}});
            vector<vec3> shape;
            shape.push_back(vec3{0.0f, 0.0f, 0.0f});
            const auto body = phys::createBody(context, phys::rigid::restoredBody(pose, particles, shape), {});
            with<phys::rigid::Crystal>::extend(context, body, phys::rigid::Crystal::Quantum{
                .particles = std::move(particles),
                .shape = std::move(shape),
                .com = vec3{0.0f, 0.0f, 0.0f},
                .hull = phys::rigid::Hull{.faces = {}, .bvh = {.nodes = {}, .root = -1}},
                .visualHurtStale = false,
            });
            with<phys::rigid::CelestialGravity>::extend(context, body, phys::rigid::CelestialGravity::Quantum{.averageRadius = look.radius, .surfaceAcceleration = look.surfaceAcceleration});
            return body;
        }

    }

    auto Planetoid::placed() -> bool {
        return runtime.has_value();
    }

    void Planetoid::place(Writing context, system::Device::Id device, Pose pose, Look look) {
        const auto material = with<resource::Assets>::find<resource::material::Asset>(context, resource::Unit::Name::from("Eltanin", "planetoid"));
        if (not material)
            return (void)context.refuse("eltanin::locality::geo::Planetoid::place: planetoid material missing");
        const auto crust = with<resource::Assets>::find<resource::texture3array::Asset>(context, resource::Unit::Name::from("Eltanin", "crust"));
        if (not crust)
            return (void)context.refuse("eltanin::locality::geo::Planetoid::place: crust pack missing");
        if (runtime) {
            for (const auto& entry : runtime->patches)
                dropPatch(context, entry.second);
            runtime->patches.clear();
        }
        const auto well = runtime ? runtime->well : makeWell(context, pose, look);
        runtime = Runtime{.look = look, .pose = pose, .device = device, .well = well, .material = *material, .crust = *crust, .patches = {}};
    }

    void Planetoid::update(Writing context, Pos camera) {
        if (not runtime)
            return;
        vector<PatchKey> wanted;
        wanted.reserve(96);
        for (int face = 0; face < faceCount; ++face)
            collectLeaves(*runtime, camera, PatchKey{.face = static_cast<std::uint8_t>(face), .level = 0, .iu = 0, .iv = 0}, wanted);
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
        vector<PatchKey> stale;
        for (const auto& entry : runtime->patches) {
            if (not keep.contains(entry.first) or entry.second.coarserEdges != edgeFlags(keep, entry.first))
                stale.push_back(entry.first);
        }
        for (const PatchKey& key : stale) {
            const auto found = runtime->patches.find(key);
            if (found == runtime->patches.end())
                continue;
            dropPatch(context, found->second);
            runtime->patches.erase(found);
        }
        for (const PatchKey& key : wanted) {
            if (not runtime->patches.contains(key))
                spawnPatch(context, *runtime, key, edgeFlags(keep, key));
        }
    }

    auto Planetoid::height(vec3 dir) -> float {
        if (not runtime)
            return 0.0f;
        return heightOf(dir, runtime->look);
    }

    auto Planetoid::altitudeAt(Pos worldPos) -> float {
        if (not runtime)
            return 0.0f;
        const vec3 local = toLocal(*runtime, worldPos);
        const float radial = glm::length(local);
        if (radial < 1.0e-6f)
            return -runtime->look.radius;
        const vec3 dir = local / radial;
        return radial - (runtime->look.radius + heightOf(dir, runtime->look));
    }

    auto Planetoid::gravityAt(Pos worldPos) -> vec3 {
        if (not runtime)
            return vec3{0.0f, 0.0f, 0.0f};
        const vec3 offset = worldPos - runtime->pose.position;
        const float distance = glm::length(offset);
        if (distance < 1.0e-6f)
            return vec3{0.0f, 0.0f, 0.0f};
        const float radius = runtime->look.radius;
        const float surface = runtime->look.surfaceAcceleration;
        const float accelScale = distance < radius ? -surface / radius : -surface * radius * radius / (distance * distance * distance);
        return offset * accelScale;
    }

    auto Planetoid::surfaceInfo(vec3 dir) -> Surface {
        if (not runtime)
            return Surface{.height = 0.0f, .position = vec3{0.0f, 0.0f, 0.0f}, .normal = vec3{0.0f, 1.0f, 0.0f}, .mix = 0, .slope = 0.0f};
        const float len = glm::length(dir);
        if (len < 1.0e-6f)
            return Surface{.height = 0.0f, .position = vec3{0.0f, 0.0f, 0.0f}, .normal = vec3{0.0f, 1.0f, 0.0f}, .mix = 0, .slope = 0.0f};
        dir /= len;
        const Relief relief = reliefOf(dir, runtime->look);
        const vec3 normal = gradientNormal(dir, runtime->look);
        const float slope = 1.0f - glm::clamp(glm::dot(normal, dir), 0.0f, 1.0f);
        return Surface{.height = relief.height, .position = dir * (runtime->look.radius + relief.height), .normal = normal, .mix = materialMix(dir, relief.height, slope, relief.crater, runtime->look), .slope = slope};
    }

}
