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
        constexpr float lumpAmp = 0.92f;
        constexpr int maxMixSites = 10;

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

        auto hashDir(int tag, int seed) -> vec3 {
            const vec3 raw{2.0f * hash31(tag, 1, 4, seed) - 1.0f, 2.0f * hash31(tag, 2, 5, seed) - 1.0f, 2.0f * hash31(tag, 3, 6, seed) - 1.0f};
            const float length = glm::length(raw);
            if (length < 1.0e-5f)
                return vec3{0.0f, 1.0f, 0.0f};
            return raw / length;
        }

        auto ellipsoidAxes(int seed) -> vec3 {
            const vec3 axes{0.46f + 0.98f * hash31(0, 8, 1, seed), 0.46f + 0.98f * hash31(1, 8, 2, seed), 0.46f + 0.98f * hash31(2, 8, 3, seed)};
            const float mean = std::pow(axes.x * axes.y * axes.z, 1.0f / 3.0f);
            return axes / glm::max(mean, 1.0e-4f);
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

        auto mixesClose(const MixWeights& left, const MixWeights& right) -> bool {
            for (int channel = 0; channel < mixChannels; ++channel) {
                if (std::abs(left[static_cast<std::size_t>(channel)] - right[static_cast<std::size_t>(channel)]) > 0.07f)
                    return false;
            }
            return true;
        }

        auto siteInBall(int index, int seed, float radius) -> vec3 {
            const vec3 raw{2.0f * hash31(index, 11, 1, seed) - 1.0f, 2.0f * hash31(index, 11, 2, seed) - 1.0f, 2.0f * hash31(index, 11, 3, seed) - 1.0f};
            const float length = glm::length(raw);
            const vec3 dir = length < 1.0e-5f ? vec3{0.0f, 1.0f, 0.0f} : raw / length;
            const float radial = 0.18f + 0.72f * hash31(index, 12, 4, seed);
            return dir * radius * radial;
        }

        auto pickSiteChannel(int index, int seed, const MixWeights& mean, float total) -> int {
            float pick = hash31(index, 13, 5, seed) * total;
            float acc = 0.0f;
            int chosen = 0;
            for (int channel = 0; channel < mixChannels; ++channel) {
                const float weight = mean[static_cast<std::size_t>(channel)];
                if (weight <= 0.0f)
                    continue;
                acc += weight;
                chosen = channel;
                if (pick <= acc)
                    return channel;
            }
            return chosen;
        }

        auto mixAt(vec3 point, const Rock::GeneralizedRecipe& recipe) -> MixWeights {
            const MixWeights mean = unpackMix(recipe.mix);
            float total = 0.0f;
            int present = 0;
            for (int channel = 0; channel < mixChannels; ++channel) {
                if (mean[static_cast<std::size_t>(channel)] <= 0.0f)
                    continue;
                total += mean[static_cast<std::size_t>(channel)];
                ++present;
            }
            const float contrast = glm::clamp(recipe.spotContrast, 0.0f, 1.0f);
            if (present <= 1 or contrast <= 0.02f or total <= 0.0f)
                return mean;

            const float radius = recipe.radius;
            const float patch = glm::clamp(recipe.spotMeters, mech::space::local::edge2meters, glm::max(recipe.radius, mech::space::local::edge2meters));
            const int siteCount = glm::clamp(static_cast<int>(std::lround((recipe.radius * 2.0f) / patch)), 4, maxMixSites);
            const float warp = patch * 0.28f;
            const vec3 query{
                point.x + warp * (2.0f * fbm(point.x / patch, point.y / patch, point.z / patch, static_cast<int>(recipe.seed) + 41) - 1.0f),
                point.y + warp * (2.0f * fbm(point.x / patch, point.y / patch, point.z / patch, static_cast<int>(recipe.seed) + 43) - 1.0f),
                point.z + warp * (2.0f * fbm(point.x / patch, point.y / patch, point.z / patch, static_cast<int>(recipe.seed) + 47) - 1.0f),
            };

            float nearest = 1.0e9f;
            float second = 1.0e9f;
            int nearestChannel = 0;
            int secondChannel = 0;
            for (int index = 0; index < siteCount; ++index) {
                const float distance = glm::length(query - siteInBall(index, static_cast<int>(recipe.seed), radius));
                const int channel = pickSiteChannel(index, static_cast<int>(recipe.seed), mean, total);
                if (distance < nearest) {
                    second = nearest;
                    secondChannel = nearestChannel;
                    nearest = distance;
                    nearestChannel = channel;
                } else if (distance < second) {
                    second = distance;
                    secondChannel = channel;
                }
            }

            MixWeights local{};
            if (nearestChannel == secondChannel or second >= 1.0e8f) {
                local[static_cast<std::size_t>(nearestChannel)] = 1.0f;
                return local;
            }
            const float edge = glm::mix(patch * 0.42f, patch * 0.07f, contrast);
            const float inside = glm::smoothstep(0.0f, glm::max(edge, 1.0f), second - nearest);
            local[static_cast<std::size_t>(nearestChannel)] += inside;
            local[static_cast<std::size_t>(secondChannel)] += 1.0f - inside;
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
            if (amp <= 0.0f)
                return radius;
            const float length = glm::length(point);
            if (length < 1.0e-5f)
                return radius * (1.0f - amp);
            const vec3 dir = point / length;
            const vec3 axes = ellipsoidAxes(static_cast<int>(seed));
            const float ellip = 1.0f / glm::length(vec3{dir.x / axes.x, dir.y / axes.y, dir.z / axes.z});
            float knobs = 0.0f;
            for (int index = 0; index < 4; ++index) {
                const vec3 lobe = hashDir(20 + index, static_cast<int>(seed));
                const float strength = 0.28f + 0.95f * hash31(index, 21, 8, static_cast<int>(seed));
                const float sharpness = 1.6f + 5.5f * hash31(index, 22, 9, static_cast<int>(seed));
                knobs += strength * std::pow(glm::max(glm::dot(dir, lobe), 0.0f), sharpness);
            }
            float craters = 0.0f;
            for (int index = 0; index < 3; ++index) {
                const vec3 pit = hashDir(30 + index, static_cast<int>(seed));
                const float depth = 0.35f + 0.70f * hash31(index, 31, 8, static_cast<int>(seed));
                craters += depth * std::pow(glm::max(glm::dot(dir, pit), 0.0f), 7.0f + 6.0f * hash31(index, 32, 9, static_cast<int>(seed)));
            }
            const float wrinkle = 2.0f * fbm(dir.x * 1.35f, dir.y * 1.35f, dir.z * 1.35f, static_cast<int>(seed) + 7) - 1.0f;
            const float deform = 0.38f * wrinkle + 0.55f * (knobs - 0.55f) - 0.48f * craters;
            return radius * ellip * glm::clamp(1.0f + amp * deform, 1.0f - amp, 1.0f + amp);
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

        auto makeNode(index3 origin, integer scale, const Rock::GeneralizedRecipe& recipe, float radius, float amp, float rMin, float rMax) -> optional<BuildNode> {
            const Occupancy occ = occupancy(origin, scale, rMin, rMax);
            if (occ == Occupancy::vacuum)
                return {};
            const vec3 center = brickCenter(origin, scale);
            const float brickMeters = static_cast<float>(edgeCells(scale)) * mech::space::local::edge2meters;
            const float domainMeters = glm::max(recipe.spotMeters * 0.35f, mech::space::local::edge2meters);
            const bool leafHere = scale == 0 or (occ == Occupancy::solid and brickMeters <= domainMeters);
            if (leafHere) {
                if (glm::length(center) < radiusAt(center, radius, amp, recipe.seed))
                    return BuildNode{.origin = origin, .scale = scale, .weights = mixAt(center, recipe), .children = {}};
                if (occ == Occupancy::solid)
                    return BuildNode{.origin = origin, .scale = scale, .weights = mixAt(center, recipe), .children = {}};
                return {};
            }
            BuildNode node{.origin = origin, .scale = scale, .weights = MixWeights{}, .children = {}};
            const integer childScale = scale - 1;
            const integer half = edgeCells(childScale);
            for (const auto& octant : mech::cube::corners) {
                const index3 childOrigin{origin.x + octant.x * half, origin.y + octant.y * half, origin.z + octant.z * half};
                if (auto child = makeNode(childOrigin, childScale, recipe, radius, amp, rMin, rMax))
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
                    bool sameMix = true;
                    for (const auto& child : node.children) {
                        if (not mixesClose(child.weights, node.children.front().weights))
                            sameMix = false;
                    }
                    if (sameMix) {
                        vector<MixWeights> parts;
                        parts.reserve(8);
                        for (const auto& child : node.children)
                            parts.push_back(child.weights);
                        return BuildNode{.origin = origin, .scale = scale, .weights = averageWeights(parts), .children = {}};
                    }
                }
            }
            return node;
        }

    } // namespace

    auto generateRockVolume(const Rock::GeneralizedRecipe& recipe) -> Volume {
        const float radius = recipe.radius;
        const float amp = glm::clamp(recipe.lump, 0.0f, 1.0f) * lumpAmp;
        const vec3 axes = amp > 0.0f ? ellipsoidAxes(static_cast<int>(recipe.seed)) : vec3{1.0f, 1.0f, 1.0f};
        const float axisMax = glm::max(axes.x, glm::max(axes.y, axes.z));
        const float axisMin = glm::min(axes.x, glm::min(axes.y, axes.z));
        const float rMin = radius * axisMin * (1.0f - amp);
        const float rMax = radius * axisMax * (1.0f + amp);
        const float meters = mech::space::local::edge2meters;
        integer cells = static_cast<integer>(std::ceil(2.0f * rMax / meters));
        if (cells < 2)
            cells = 2;
        integer scale = 0;
        while (edgeCells(scale) < cells and scale < maxScale)
            ++scale;
        const integer half = edgeCells(scale) / 2;
        const index3 origin{-half, -half, -half};
        if (auto root = makeNode(origin, scale, recipe, radius, amp, rMin, rMax))
            return toVolume(*root);
        return Volume{.origin = origin, .scale = scale, .mix = 0, .children = {}};
    }

    auto rockSdf(const Rock::GeneralizedRecipe& recipe, vec3 point) -> float {
        const float radius = recipe.radius;
        const float amp = glm::clamp(recipe.lump, 0.0f, 1.0f) * lumpAmp;
        return glm::length(point) - radiusAt(point, radius, amp, recipe.seed);
    }

    namespace {

        constexpr integer lavaBrickCells = 20;
        constexpr integer lavaBrickHalf = 10;
        constexpr integer lavaLayerCells = 4;
        constexpr integer lavaHeightCells = lavaLayerCells * mixChannels;
        constexpr integer lavaHeightHalf = lavaHeightCells / 2;
        constexpr integer lavaOctreeScale = 6;

        auto lavaLayer(integer cellY) -> integer {
            return glm::clamp((cellY + lavaHeightHalf) / lavaLayerCells, integer{0}, integer{mixChannels - 1});
        }

        auto lavaMixForLayer(integer channel) -> Mix {
            MixWeights weights{};
            weights[static_cast<std::size_t>(channel)] = 1.0f;
            return packMix(weights);
        }

        auto lavaOneLayer(index3 origin, integer scale) -> bool {
            const integer edge = edgeCells(scale);
            const integer y0 = glm::max(origin.y, -lavaHeightHalf);
            const integer y1 = glm::min(origin.y + edge, lavaHeightHalf);
            if (y0 >= y1)
                return true;
            return lavaLayer(y0) == lavaLayer(y1 - 1);
        }

        auto brickOccupancy(index3 origin, integer scale) -> Occupancy {
            const integer edge = edgeCells(scale);
            const integer x1 = origin.x + edge;
            const integer y1 = origin.y + edge;
            const integer z1 = origin.z + edge;
            if (x1 <= -lavaBrickHalf or origin.x >= lavaBrickHalf or y1 <= -lavaHeightHalf or origin.y >= lavaHeightHalf or z1 <= -lavaBrickHalf or origin.z >= lavaBrickHalf)
                return Occupancy::vacuum;
            if (origin.x >= -lavaBrickHalf and x1 <= lavaBrickHalf and origin.y >= -lavaHeightHalf and y1 <= lavaHeightHalf and origin.z >= -lavaBrickHalf and z1 <= lavaBrickHalf)
                return Occupancy::solid;
            return Occupancy::mixed;
        }

        auto makeLavaNode(index3 origin, integer scale) -> optional<Volume> {
            const Occupancy occ = brickOccupancy(origin, scale);
            if (occ == Occupancy::vacuum)
                return {};
            if ((occ == Occupancy::solid and lavaOneLayer(origin, scale)) or scale == 0)
                return Volume{.origin = origin, .scale = scale, .mix = lavaMixForLayer(lavaLayer(origin.y)), .children = {}};
            Volume node{.origin = origin, .scale = scale, .mix = Mix{0}, .children = {}};
            const integer childScale = scale - 1;
            const integer half = edgeCells(childScale);
            for (const auto& octant : mech::cube::corners) {
                const index3 childOrigin{origin.x + octant.x * half, origin.y + octant.y * half, origin.z + octant.z * half};
                if (auto child = makeLavaNode(childOrigin, childScale))
                    node.children.push_back(std::move(*child));
            }
            if (node.children.empty())
                return {};
            if (node.children.size() == 8) {
                const Mix leafMix = node.children[0].mix;
                bool collapse = node.children[0].children.empty();
                for (const auto& child : node.children) {
                    if (not child.children.empty() or child.mix != leafMix)
                        collapse = false;
                }
                if (collapse)
                    return Volume{.origin = origin, .scale = scale, .mix = leafMix, .children = {}};
            }
            return node;
        }

    } // namespace

    auto generateLavaBrickVolume() -> Volume {
        const integer half = edgeCells(lavaOctreeScale) / 2;
        const index3 origin{-half, -half, -half};
        if (auto root = makeLavaNode(origin, lavaOctreeScale))
            return *root;
        return Volume{.origin = origin, .scale = lavaOctreeScale, .mix = 0, .children = {}};
    }

    auto applyLavaBrickHeat(rmmr::resource::builders::geometry::CpuPresentation& cpu) -> void {
        const float meters = mech::space::local::edge2meters;
        const float span = static_cast<float>(lavaBrickCells) * meters;
        const float origin = -static_cast<float>(lavaBrickHalf) * meters;
        cpu.cohesion.resize(cpu.positions.size());
        for (std::size_t vertex = 0; vertex < cpu.positions.size(); ++vertex) {
            const float w = glm::clamp((cpu.positions[vertex].z - origin) / span, 0.0f, 1.0f);
            cpu.cohesion[vertex] = w;
        }
    }

    auto generateIceBlobVolume() -> Volume {
        const Rock::GeneralizedRecipe recipe{
            .mix = Rock::GeneralizedRecipe::homogenous(0),
            .radius = 25.0f,
            .lump = 0.82f,
            .seed = 20260818,
            .spotMeters = 12.0f,
            .spotContrast = 0.0f,
        };
        return generateRockVolume(recipe);
    }

    auto applyIceBlobSinter(rmmr::resource::builders::geometry::CpuPresentation& cpu) -> void {
        cpu.cohesion.resize(cpu.positions.size());
        for (std::size_t vertex = 0; vertex < cpu.positions.size(); ++vertex) {
            const vec3 point = cpu.positions[vertex];
            const float noise = 0.45f * std::sin(glm::dot(point, vec3{0.18f, 0.11f, 0.14f})) + 0.35f * std::sin(glm::dot(point, vec3{0.31f, 0.22f, 0.09f}) + 1.3f) + 0.20f * std::sin(glm::dot(point, vec3{0.55f, 0.17f, 0.41f}) + 2.1f);
            const float unit = glm::clamp(0.5f + 0.5f * noise, 0.0f, 1.0f);
            const float patch = glm::smoothstep(0.62f, 0.88f, unit);
            cpu.cohesion[vertex] = glm::mix(0.08f, 1.0f, patch);
        }
    }

}
