#include "geo/stones/boulderMesh.h"

#include <rmmr/semantics/geometry.h>

#include <array>
#include <cmath>
#include <map>
#include <numbers>
#include <utility>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

namespace eltanin::locality::geo {

    using namespace fqsm::api;
    using rmmr::resource::builders::geometry::CpuPresentation;

    namespace {

        constexpr float lumpAmplitude = 0.55f;
        constexpr float targetEdgeMeters = 0.45f;
        constexpr int silhouetteSides = 32;
        constexpr int subdivMin = 2;
        constexpr int subdivMax = 4;
        constexpr float icosaEdgeOnUnit = 1.0514622242382672f;

        auto subdivFromRadius(float radius) -> int {
            const float angularEdge = (2.0f * std::numbers::pi_v<float> * radius) / static_cast<float>(silhouetteSides);
            const float target = glm::min(targetEdgeMeters, angularEdge);
            float edge = radius * icosaEdgeOnUnit;
            int subdiv = 0;
            while (subdiv < subdivMax and edge > target) {
                edge *= 0.5f;
                ++subdiv;
            }
            if (subdiv < subdivMin)
                return subdivMin;
            return subdiv;
        }

        auto lumpNoise(vec3 dir, integer seed) -> float {
            const float phase = 0.17f * static_cast<float>(seed);
            const float n0 = glm::sin(glm::dot(dir, vec3{2.1f, 3.3f, 1.7f}) + phase);
            const float n1 = glm::sin(glm::dot(dir, vec3{5.2f, 1.1f, 4.4f}) + phase * 1.7f);
            const float n2 = glm::sin(glm::dot(dir, vec3{8.3f, 6.7f, 2.9f}) + phase * 0.6f);
            return lumpAmplitude * (0.55f * n0 + 0.30f * n1 + 0.15f * n2);
        }

        auto midpointIndex(int vertA, int vertB, vector<vec3>& verts, std::map<std::pair<int, int>, int>& cache) -> int {
            const std::pair<int, int> key = vertA < vertB ? std::pair<int, int>{vertA, vertB} : std::pair<int, int>{vertB, vertA};
            const auto found = cache.find(key);
            if (found != cache.end())
                return found->second;
            const int index = static_cast<int>(verts.size());
            verts.push_back(glm::normalize(verts[static_cast<std::size_t>(vertA)] + verts[static_cast<std::size_t>(vertB)]));
            cache.emplace(key, index);
            return index;
        }

        auto icosphere(int subdiv) -> std::pair<vector<vec3>, vector<std::array<int, 3>>> {
            const float phi = (1.0f + std::sqrt(5.0f)) * 0.5f;
            vector<vec3> verts{
                glm::normalize(vec3{-1.0f, phi, 0.0f}),
                glm::normalize(vec3{1.0f, phi, 0.0f}),
                glm::normalize(vec3{-1.0f, -phi, 0.0f}),
                glm::normalize(vec3{1.0f, -phi, 0.0f}),
                glm::normalize(vec3{0.0f, -1.0f, phi}),
                glm::normalize(vec3{0.0f, 1.0f, phi}),
                glm::normalize(vec3{0.0f, -1.0f, -phi}),
                glm::normalize(vec3{0.0f, 1.0f, -phi}),
                glm::normalize(vec3{phi, 0.0f, -1.0f}),
                glm::normalize(vec3{phi, 0.0f, 1.0f}),
                glm::normalize(vec3{-phi, 0.0f, -1.0f}),
                glm::normalize(vec3{-phi, 0.0f, 1.0f}),
            };
            vector<std::array<int, 3>> faces{
                {0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11},
                {1, 5, 9}, {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
                {3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8}, {3, 8, 9},
                {4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1},
            };
            for (int level = 0; level < subdiv; ++level) {
                std::map<std::pair<int, int>, int> cache;
                vector<std::array<int, 3>> next;
                next.reserve(faces.size() * 4);
                for (const auto& face : faces) {
                    const int ab = midpointIndex(face[0], face[1], verts, cache);
                    const int bc = midpointIndex(face[1], face[2], verts, cache);
                    const int ca = midpointIndex(face[2], face[0], verts, cache);
                    next.push_back({face[0], ab, ca});
                    next.push_back({face[1], bc, ab});
                    next.push_back({face[2], ca, bc});
                    next.push_back({ab, bc, ca});
                }
                faces = std::move(next);
            }
            return {std::move(verts), std::move(faces)};
        }

    } // namespace

    auto meshDebris(const GeneralizedRecipe& recipe) -> CpuPresentation {
        CpuPresentation cpu{
            .layout = rmmr::primitive::GeometrySemantics::layoutIds(vector<string>{"position", "normal", "cohesion"}),
            .positions = {},
            .normals = {},
            .uv0 = {},
            .color0 = {},
            .indices = {},
            .mix0 = {},
        };
        const float radius = recipe.radius;
        if (radius <= 0.0f)
            return cpu;

        auto [dirs, faces] = icosphere(subdivFromRadius(recipe.radius));
        cpu.positions.resize(dirs.size());
        cpu.normals.assign(dirs.size(), vec3{0.0f, 0.0f, 0.0f});
        cpu.indices.reserve(faces.size() * 3);

        for (std::size_t index = 0; index < dirs.size(); ++index) {
            const vec3 dir = dirs[index];
            cpu.positions[index] = dir * (radius * (1.0f + recipe.lump * lumpNoise(dir, recipe.seed)));
        }

        for (const auto& face : faces) {
            const auto ia = static_cast<std::size_t>(face[0]);
            const auto ib = static_cast<std::size_t>(face[1]);
            const auto ic = static_cast<std::size_t>(face[2]);
            const vec3 faceNormal = glm::cross(cpu.positions[ib] - cpu.positions[ia], cpu.positions[ic] - cpu.positions[ia]);
            cpu.normals[ia] += faceNormal;
            cpu.normals[ib] += faceNormal;
            cpu.normals[ic] += faceNormal;
            cpu.indices.push_back(static_cast<integer>(face[0]));
            cpu.indices.push_back(static_cast<integer>(face[1]));
            cpu.indices.push_back(static_cast<integer>(face[2]));
        }
        for (std::size_t index = 0; index < cpu.normals.size(); ++index) {
            const float length2 = glm::dot(cpu.normals[index], cpu.normals[index]);
            if (length2 <= 1.0e-16f)
                cpu.normals[index] = dirs[index];
            else
                cpu.normals[index] /= std::sqrt(length2);
        }
        cpu.cohesion.assign(cpu.positions.size(), 0.0f);
        return cpu;
    }

}
