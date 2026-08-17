#include "stones/generator.h"

#include "mech/semantics/space.h"

#include <array>
#include <cmath>
#include <cstdint>

#include <glm/common.hpp>
#include <glm/geometric.hpp>

namespace eltanin::geo {

    using namespace fqsm::api;

    namespace {

        constexpr int mixChannels = 16;
        constexpr integer maxScale = 16;
        constexpr float lumpAmp = 0.55f;

        using MixWeights = std::array<float, 16>;

        auto hash31(int x, int y, int z, int seed) -> float {
            auto mix = [](std::uint32_t value) -> std::uint32_t {
                value ^= value >> 16;
                value *= 0x7feb352du;
                value ^= value >> 15;
                value *= 0x846ca68bu;
                value ^= value >> 16;
                return value;
            };
            const std::uint32_t h = mix(std::uint32_t(x) * 73856093u ^ std::uint32_t(y) * 19349663u ^ std::uint32_t(z) * 83492791u ^ std::uint32_t(seed) * 2654435761u);
            return float(h >> 8) * (1.0f / 16777215.0f);
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

        auto fbm(float x, float y, float z, int seed) -> float {
            float sum = 0.0f;
            float amp = 0.5f;
            float freq = 1.0f;
            for (int octave = 0; octave < 4; ++octave) {
                sum += amp * valueNoise(x * freq, y * freq, z * freq, seed + octave * 17);
                amp *= 0.5f;
                freq *= 2.0f;
            }
            return sum;
        }

        auto edgeCells(integer scale) -> integer {
            return 1 << scale;
        }

        auto unpackMix(Mix mix) -> MixWeights {
            MixWeights weights{};
            for (int channel = 0; channel < mixChannels; ++channel)
                weights[static_cast<std::size_t>(channel)] = static_cast<float>((mix >> (channel * 4)) & 0xF) / 15.0f;
            return weights;
        }

        auto packMix(const MixWeights& weights) -> Mix {
            float mass = 0.0f;
            for (float weight : weights)
                mass += weight;
            if (mass <= 0.0f)
                return Mix{0};
            Mix packed = 0;
            for (int channel = 0; channel < mixChannels; ++channel) {
                const int nibble = static_cast<int>(glm::clamp(weights[static_cast<std::size_t>(channel)] / mass, 0.0f, 1.0f) * 15.0f + 0.5f);
                packed |= Mix{static_cast<std::uint64_t>(nibble)} << (channel * 4);
            }
            return packed;
        }

        auto mixAt(vec3 point, const Recipe& recipe) -> MixWeights {
            const MixWeights mean = unpackMix(recipe.mix);
            MixWeights local{};
            float mass = 0.0f;
            const float contrast = glm::clamp(recipe.spotContrast, 0.0f, 1.0f);
            const float freq = 1.0f / glm::max(recipe.spotMeters, mech::space::local::edge2meters);
            for (int channel = 0; channel < mixChannels; ++channel) {
                const float meanWeight = mean[static_cast<std::size_t>(channel)];
                if (meanWeight <= 0.0f)
                    continue;
                const float noise = fbm(point.x * freq, point.y * freq, point.z * freq, static_cast<int>(recipe.seed) + channel * 19);
                const float gain = 1.0f + contrast * (2.0f * noise - 1.0f);
                local[static_cast<std::size_t>(channel)] = meanWeight * glm::max(gain, 0.0f);
                mass += local[static_cast<std::size_t>(channel)];
            }
            if (mass <= 0.0f)
                return mean;
            for (int channel = 0; channel < mixChannels; ++channel)
                local[static_cast<std::size_t>(channel)] /= mass;
            return local;
        }

        auto averageWeights(const vector<MixWeights>& parts) -> MixWeights {
            MixWeights sum{};
            if (parts.empty())
                return sum;
            for (const auto& part : parts) {
                for (int channel = 0; channel < mixChannels; ++channel)
                    sum[static_cast<std::size_t>(channel)] += part[static_cast<std::size_t>(channel)];
            }
            const float inv = 1.0f / static_cast<float>(parts.size());
            for (int channel = 0; channel < mixChannels; ++channel)
                sum[static_cast<std::size_t>(channel)] *= inv;
            return sum;
        }

        struct BuildNode {
            index3 origin;
            integer scale;
            MixWeights weights;
            vector<BuildNode> children;
        };

        auto toVolume(const BuildNode& node) -> Volume {
            Volume volume{.origin = node.origin, .scale = node.scale, .mix = node.children.empty() ? packMix(node.weights) : Mix{0}, .children = {}};
            volume.children.reserve(node.children.size());
            for (const auto& child : node.children)
                volume.children.push_back(toVolume(child));
            return volume;
        }

        auto radiusAt(vec3 point, float radius, float amp, integer seed) -> float {
            const float length = glm::length(point);
            if (length < 1.0e-5f)
                return radius;
            const vec3 dir = point / length;
            const float lobe = 2.0f * fbm(dir.x * 3.0f, dir.y * 3.0f, dir.z * 3.0f, static_cast<int>(seed) + 7) - 1.0f;
            const float dent = 2.0f * fbm(point.x / radius, point.y / radius, point.z / radius, static_cast<int>(seed) + 13) - 1.0f;
            return glm::clamp(radius * (1.0f + amp * (0.65f * lobe + 0.35f * dent)), radius * (1.0f - amp), radius * (1.0f + amp));
        }

        enum class Occupancy { vacuum, solid, mixed };

        auto occupancy(index3 origin, integer scale, float rMin, float rMax) -> Occupancy {
            const integer edge = edgeCells(scale);
            const float meters = mech::space::local::edge2meters;
            const vec3 aabbMin = vec3{static_cast<float>(origin.x), static_cast<float>(origin.y), static_cast<float>(origin.z)} * meters;
            const vec3 aabbMax = vec3{static_cast<float>(origin.x + edge), static_cast<float>(origin.y + edge), static_cast<float>(origin.z + edge)} * meters;
            if (glm::length(glm::clamp(vec3{0.0f, 0.0f, 0.0f}, aabbMin, aabbMax)) >= rMax)
                return Occupancy::vacuum;
            const vec3 farthest{
                glm::abs(aabbMin.x) > glm::abs(aabbMax.x) ? aabbMin.x : aabbMax.x,
                glm::abs(aabbMin.y) > glm::abs(aabbMax.y) ? aabbMin.y : aabbMax.y,
                glm::abs(aabbMin.z) > glm::abs(aabbMax.z) ? aabbMin.z : aabbMax.z,
            };
            if (glm::length(farthest) < rMin)
                return Occupancy::solid;
            return Occupancy::mixed;
        }

        auto brickCenter(index3 origin, integer scale) -> vec3 {
            const float half = static_cast<float>(edgeCells(scale)) * 0.5f;
            return vec3{static_cast<float>(origin.x) + half, static_cast<float>(origin.y) + half, static_cast<float>(origin.z) + half} * mech::space::local::edge2meters;
        }

        auto makeNode(index3 origin, integer scale, const Recipe& recipe, float radius, float amp) -> optional<BuildNode> {
            const float rMin = radius * (1.0f - amp);
            const float rMax = radius * (1.0f + amp);
            const Occupancy occ = occupancy(origin, scale, rMin, rMax);
            if (occ == Occupancy::vacuum)
                return {};
            const vec3 center = brickCenter(origin, scale);
            if (occ == Occupancy::solid)
                return BuildNode{.origin = origin, .scale = scale, .weights = mixAt(center, recipe), .children = {}};
            if (scale == 0) {
                if (glm::length(center) < radiusAt(center, radius, amp, recipe.seed))
                    return BuildNode{.origin = origin, .scale = scale, .weights = mixAt(center, recipe), .children = {}};
                return {};
            }
            BuildNode node{.origin = origin, .scale = scale, .weights = MixWeights{}, .children = {}};
            const integer childScale = scale - 1;
            const integer half = edgeCells(childScale);
            for (const auto& octant : mech::cube::corners) {
                const index3 childOrigin{origin.x + octant.x * half, origin.y + octant.y * half, origin.z + octant.z * half};
                if (auto child = makeNode(childOrigin, childScale, recipe, radius, amp))
                    node.children.push_back(std::move(*child));
            }
            if (node.children.empty())
                return {};
            if (node.children.size() == 8) {
                bool allLeaves = true;
                for (const auto& child : node.children) {
                    if (not child.children.empty())
                        allLeaves = false;
                }
                if (allLeaves) {
                    vector<MixWeights> parts;
                    parts.reserve(8);
                    for (const auto& child : node.children)
                        parts.push_back(child.weights);
                    return BuildNode{.origin = origin, .scale = scale, .weights = averageWeights(parts), .children = {}};
                }
            }
            return node;
        }

    } // namespace

    auto generateRockVolume(const Recipe& recipe) -> Volume {
        const float radius = recipe.diameterMeters * 0.5f;
        const float amp = glm::clamp(recipe.lump, 0.0f, 1.0f) * lumpAmp;
        const float envelope = radius * (1.0f + amp);
        const float meters = mech::space::local::edge2meters;
        integer cells = static_cast<integer>(std::ceil(2.0f * envelope / meters));
        if (cells < 2)
            cells = 2;
        integer scale = 0;
        while (edgeCells(scale) < cells and scale < maxScale)
            ++scale;
        const integer half = edgeCells(scale) / 2;
        const index3 origin{-half, -half, -half};
        if (auto root = makeNode(origin, scale, recipe, radius, amp))
            return toVolume(*root);
        return Volume{.origin = origin, .scale = scale, .mix = 0, .children = {}};
    }

    auto rockSdf(const Recipe& recipe, vec3 point) -> float {
        const float radius = recipe.diameterMeters * 0.5f;
        const float amp = glm::clamp(recipe.lump, 0.0f, 1.0f) * lumpAmp;
        return glm::length(point) - radiusAt(point, radius, amp, recipe.seed);
    }

}
