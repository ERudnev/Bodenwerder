#include "stones/marchingCubes.h"

#include "mech/semantics/space.h"

#include <rmmr/semantics/geometry.h>

#include <array>
#include <cstdint>
#include <utility>

#include <glm/common.hpp>
#include <glm/geometric.hpp>

namespace eltanin::geo {

    using namespace fqsm::api;
    using rmmr::resource::builders::geometry::CpuPresentation;

    namespace {

        constexpr float isoLevel = 0.5f;

        const ivec3 cubeCorner[8]{
            {0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1},
            {0, 1, 0}, {1, 1, 0}, {1, 1, 1}, {0, 1, 1},
        };

        const int tetCorners[6][4]{
            {0, 1, 2, 6}, {0, 2, 3, 6}, {0, 3, 7, 6},
            {0, 7, 4, 6}, {0, 4, 5, 6}, {0, 5, 1, 6},
        };

        auto edgeCells(integer scale) -> integer {
            return 1 << scale;
        }

        void stampOccupied(vector<std::uint8_t>& occupied, vector<Mix>& cellMix, index3 rootOrigin, integer extent, const Volume& node) {
            if (not node.children.empty()) {
                for (const auto& child : node.children)
                    stampOccupied(occupied, cellMix, rootOrigin, extent, child);
                return;
            }
            if (node.mix == 0)
                return;
            const integer edge = edgeCells(node.scale);
            for (integer cellZ = 0; cellZ < edge; ++cellZ) {
                for (integer cellY = 0; cellY < edge; ++cellY) {
                    for (integer cellX = 0; cellX < edge; ++cellX) {
                        const integer gridX = node.origin.x + cellX - rootOrigin.x;
                        const integer gridY = node.origin.y + cellY - rootOrigin.y;
                        const integer gridZ = node.origin.z + cellZ - rootOrigin.z;
                        if (gridX < 0 or gridY < 0 or gridZ < 0 or gridX >= extent or gridY >= extent or gridZ >= extent)
                            continue;
                        const auto index = static_cast<std::size_t>(gridX + extent * (gridY + extent * gridZ));
                        occupied[index] = 1;
                        cellMix[index] = node.mix;
                    }
                }
            }
        }

        auto vertexFill(const vector<std::uint8_t>& occupied, integer extent, integer gridX, integer gridY, integer gridZ) -> float {
            float sum = 0.0f;
            for (integer offsetZ = 0; offsetZ < 2; ++offsetZ) {
                for (integer offsetY = 0; offsetY < 2; ++offsetY) {
                    for (integer offsetX = 0; offsetX < 2; ++offsetX) {
                        const integer cellX = gridX - offsetX;
                        const integer cellY = gridY - offsetY;
                        const integer cellZ = gridZ - offsetZ;
                        if (cellX < 0 or cellY < 0 or cellZ < 0 or cellX >= extent or cellY >= extent or cellZ >= extent)
                            continue;
                        sum += occupied[static_cast<std::size_t>(cellX + extent * (cellY + extent * cellZ))];
                    }
                }
            }
            return sum / 8.0f;
        }

        struct Corner {
            vec3 pos;
            float fill;
        };

        auto interpolate(const Corner& from, const Corner& to) -> vec3 {
            const float delta = to.fill - from.fill;
            if (glm::abs(delta) < 1.0e-6f)
                return from.pos;
            return glm::mix(from.pos, to.pos, (isoLevel - from.fill) / delta);
        }

        auto solidHint(const std::array<Corner, 4>& corners, int mask) -> vec3 {
            vec3 solid{0.0f, 0.0f, 0.0f};
            float solidCount = 0.0f;
            vec3 onIso{0.0f, 0.0f, 0.0f};
            float onIsoCount = 0.0f;
            vec3 vacuum{0.0f, 0.0f, 0.0f};
            float vacuumCount = 0.0f;
            for (int corner = 0; corner < 4; ++corner) {
                const auto& node = corners[static_cast<std::size_t>(corner)];
                if (node.fill > isoLevel) {
                    solid += node.pos;
                    solidCount += 1.0f;
                    continue;
                }
                if ((mask & (1 << corner)) != 0) {
                    onIso += node.pos;
                    onIsoCount += 1.0f;
                    continue;
                }
                vacuum += node.pos;
                vacuumCount += 1.0f;
            }
            if (solidCount > 0.0f)
                return solid / solidCount;
            onIso /= onIsoCount;
            vacuum /= vacuumCount;
            return onIso - (vacuum - onIso);
        }

        void emitOriented(CpuPresentation& cpu, vec3 a, vec3 b, vec3 c, vec3 inside) {
            vec3 face = glm::cross(b - a, c - a);
            const float length = glm::length(face);
            if (length < 1.0e-8f)
                return;
            face /= length;
            const vec3 mid = (a + b + c) / 3.0f;
            if (glm::dot(face, mid - inside) < 0.0f) {
                std::swap(b, c);
                face = -face;
            }
            cpu.positions.push_back(a);
            cpu.positions.push_back(b);
            cpu.positions.push_back(c);
            cpu.normals.push_back(face);
            cpu.normals.push_back(face);
            cpu.normals.push_back(face);
        }

        void emitTet(CpuPresentation& cpu, const std::array<Corner, 4>& corners) {
            int mask = 0;
            for (int corner = 0; corner < 4; ++corner) {
                if (corners[static_cast<std::size_t>(corner)].fill >= isoLevel)
                    mask |= 1 << corner;
            }
            const int insideCount = (mask & 1) + ((mask >> 1) & 1) + ((mask >> 2) & 1) + ((mask >> 3) & 1);
            if (insideCount == 0 or insideCount == 4)
                return;
            const vec3 inside = solidHint(corners, mask);

            if (insideCount == 1 or insideCount == 3) {
                const int lone = insideCount == 1 ? mask : (mask ^ 15);
                int loneIndex = 0;
                while ((lone & (1 << loneIndex)) == 0)
                    ++loneIndex;
                const int a = (loneIndex + 1) & 3;
                const int b = (loneIndex + 2) & 3;
                const int c = (loneIndex + 3) & 3;
                emitOriented(cpu, interpolate(corners[static_cast<std::size_t>(loneIndex)], corners[static_cast<std::size_t>(a)]), interpolate(corners[static_cast<std::size_t>(loneIndex)], corners[static_cast<std::size_t>(b)]), interpolate(corners[static_cast<std::size_t>(loneIndex)], corners[static_cast<std::size_t>(c)]), inside);
                return;
            }

            int first = -1;
            int second = -1;
            for (int corner = 0; corner < 4; ++corner) {
                if ((mask & (1 << corner)) == 0)
                    continue;
                if (first < 0)
                    first = corner;
                else
                    second = corner;
            }
            int outside[2];
            int outsideCount = 0;
            for (int corner = 0; corner < 4; ++corner) {
                if ((mask & (1 << corner)) != 0)
                    continue;
                outside[outsideCount] = corner;
                ++outsideCount;
            }
            const auto ac = interpolate(corners[static_cast<std::size_t>(first)], corners[static_cast<std::size_t>(outside[0])]);
            const auto ad = interpolate(corners[static_cast<std::size_t>(first)], corners[static_cast<std::size_t>(outside[1])]);
            const auto bc = interpolate(corners[static_cast<std::size_t>(second)], corners[static_cast<std::size_t>(outside[0])]);
            const auto bd = interpolate(corners[static_cast<std::size_t>(second)], corners[static_cast<std::size_t>(outside[1])]);
            emitOriented(cpu, ac, bc, bd, inside);
            emitOriented(cpu, ac, bd, ad, inside);
        }

    } // namespace

    auto meshVolume(const Volume& root) -> CpuPresentation {
        CpuPresentation cpu{
            .layout = rmmr::primitive::GeometrySemantics::layoutIds(vector<string>{"position", "normal", "mix0"}),
            .positions = {},
            .normals = {},
            .uv0 = {},
            .color0 = {},
            .indices = {},
            .mix0 = {},
        };
        if (root.scale < 0)
            return cpu;
        const integer extent = edgeCells(root.scale);
        vector<std::uint8_t> occupied(static_cast<std::size_t>(extent * extent * extent), 0);
        vector<Mix> cellMix(static_cast<std::size_t>(extent * extent * extent), Mix{0});
        stampOccupied(occupied, cellMix, root.origin, extent, root);

        const float meters = mech::space::local::edge2meters;
        const integer verts = extent + 1;
        auto latticeIndex = [&](integer gridX, integer gridY, integer gridZ) -> std::size_t {
            return static_cast<std::size_t>(gridX + verts * (gridY + verts * gridZ));
        };
        auto latticePos = [&](integer gridX, integer gridY, integer gridZ) -> vec3 {
            return vec3{static_cast<float>(root.origin.x + gridX), static_cast<float>(root.origin.y + gridY), static_cast<float>(root.origin.z + gridZ)} * meters;
        };
        vector<float> latticeFill(static_cast<std::size_t>(verts * verts * verts), 0.0f);
        for (integer gridZ = 0; gridZ < verts; ++gridZ) {
            for (integer gridY = 0; gridY < verts; ++gridY) {
                for (integer gridX = 0; gridX < verts; ++gridX)
                    latticeFill[latticeIndex(gridX, gridY, gridZ)] = vertexFill(occupied, extent, gridX, gridY, gridZ);
            }
        }

        auto fillAt = [&](vec3 world) -> float {
            vec3 grid = world / meters - vec3{static_cast<float>(root.origin.x), static_cast<float>(root.origin.y), static_cast<float>(root.origin.z)};
            grid = glm::clamp(grid, vec3{0.0f, 0.0f, 0.0f}, vec3{static_cast<float>(extent), static_cast<float>(extent), static_cast<float>(extent)});
            const vec3 originCell = glm::floor(grid);
            const vec3 frac = grid - originCell;
            const integer x0 = static_cast<integer>(originCell.x);
            const integer y0 = static_cast<integer>(originCell.y);
            const integer z0 = static_cast<integer>(originCell.z);
            const integer x1 = x0 < extent ? x0 + 1 : extent;
            const integer y1 = y0 < extent ? y0 + 1 : extent;
            const integer z1 = z0 < extent ? z0 + 1 : extent;
            const float c000 = latticeFill[latticeIndex(x0, y0, z0)];
            const float c100 = latticeFill[latticeIndex(x1, y0, z0)];
            const float c010 = latticeFill[latticeIndex(x0, y1, z0)];
            const float c110 = latticeFill[latticeIndex(x1, y1, z0)];
            const float c001 = latticeFill[latticeIndex(x0, y0, z1)];
            const float c101 = latticeFill[latticeIndex(x1, y0, z1)];
            const float c011 = latticeFill[latticeIndex(x0, y1, z1)];
            const float c111 = latticeFill[latticeIndex(x1, y1, z1)];
            return glm::mix(glm::mix(glm::mix(c000, c100, frac.x), glm::mix(c010, c110, frac.x), frac.y), glm::mix(glm::mix(c001, c101, frac.x), glm::mix(c011, c111, frac.x), frac.y), frac.z);
        };
        auto fillAtGrid = [&](integer gridX, integer gridY, integer gridZ) -> float {
            if (gridX < 0 or gridY < 0 or gridZ < 0 or gridX > extent or gridY > extent or gridZ > extent)
                return vertexFill(occupied, extent, gridX, gridY, gridZ);
            return latticeFill[latticeIndex(gridX, gridY, gridZ)];
        };
        for (integer cellZ = -1; cellZ <= extent; ++cellZ) {
            for (integer cellY = -1; cellY <= extent; ++cellY) {
                for (integer cellX = -1; cellX <= extent; ++cellX) {
                    std::array<Corner, 8> cube{};
                    for (int corner = 0; corner < 8; ++corner) {
                        const integer gridX = cellX + cubeCorner[corner].x;
                        const integer gridY = cellY + cubeCorner[corner].y;
                        const integer gridZ = cellZ + cubeCorner[corner].z;
                        cube[static_cast<std::size_t>(corner)] = Corner{.pos = latticePos(gridX, gridY, gridZ), .fill = fillAtGrid(gridX, gridY, gridZ)};
                    }
                    for (const auto& tet : tetCorners)
                        emitTet(cpu, {cube[static_cast<std::size_t>(tet[0])], cube[static_cast<std::size_t>(tet[1])], cube[static_cast<std::size_t>(tet[2])], cube[static_cast<std::size_t>(tet[3])]});
                }
            }
        }

        const float step = meters;
        for (std::size_t vertex = 0; vertex < cpu.positions.size(); ++vertex) {
            const vec3 pos = cpu.positions[vertex];
            const vec3 grad{
                fillAt(pos + vec3{step, 0.0f, 0.0f}) - fillAt(pos - vec3{step, 0.0f, 0.0f}),
                fillAt(pos + vec3{0.0f, step, 0.0f}) - fillAt(pos - vec3{0.0f, step, 0.0f}),
                fillAt(pos + vec3{0.0f, 0.0f, step}) - fillAt(pos - vec3{0.0f, 0.0f, step}),
            };
            const float length = glm::length(grad);
            if (length < 1.0e-8f)
                continue;
            cpu.normals[vertex] = -grad / length;
        }

        using MixWeights = std::array<float, 16>;

        auto unpackMix = [](Mix mix) -> MixWeights {
            MixWeights weights{};
            for (int channel = 0; channel < 16; ++channel)
                weights[static_cast<std::size_t>(channel)] = static_cast<float>((mix >> (channel * 4)) & 0xF) / 15.0f;
            return weights;
        };
        auto packMix = [](const MixWeights& weights) -> Mix {
            float mass = 0.0f;
            for (float weight : weights)
                mass += weight;
            if (mass <= 0.0f)
                return Mix{0};
            Mix packed = 0;
            for (int channel = 0; channel < 16; ++channel) {
                const int nibble = static_cast<int>(glm::clamp(weights[static_cast<std::size_t>(channel)] / mass, 0.0f, 1.0f) * 15.0f + 0.5f);
                packed |= Mix{static_cast<std::uint64_t>(nibble)} << (channel * 4);
            }
            return packed;
        };
        auto lerpWeights = [](const MixWeights& a, const MixWeights& b, float t) -> MixWeights {
            MixWeights out{};
            for (int channel = 0; channel < 16; ++channel)
                out[static_cast<std::size_t>(channel)] = glm::mix(a[static_cast<std::size_t>(channel)], b[static_cast<std::size_t>(channel)], t);
            return out;
        };
        auto mixAt = [&](vec3 world) -> Mix {
            vec3 grid = world / meters - vec3{static_cast<float>(root.origin.x), static_cast<float>(root.origin.y), static_cast<float>(root.origin.z)};
            const vec3 p = grid - vec3{0.5f, 0.5f, 0.5f};
            const vec3 originCell = glm::floor(p);
            const vec3 frac = p - originCell;
            const integer x0 = static_cast<integer>(originCell.x);
            const integer y0 = static_cast<integer>(originCell.y);
            const integer z0 = static_cast<integer>(originCell.z);
            auto sample = [&](integer x, integer y, integer z) -> MixWeights {
                if (x < 0 or y < 0 or z < 0 or x >= extent or y >= extent or z >= extent)
                    return MixWeights{};
                const auto index = static_cast<std::size_t>(x + extent * (y + extent * z));
                if (occupied[index] == 0)
                    return MixWeights{};
                return unpackMix(cellMix[index]);
            };
            const MixWeights c0 = lerpWeights(lerpWeights(sample(x0, y0, z0), sample(x0 + 1, y0, z0), frac.x), lerpWeights(sample(x0, y0 + 1, z0), sample(x0 + 1, y0 + 1, z0), frac.x), frac.y);
            const MixWeights c1 = lerpWeights(lerpWeights(sample(x0, y0, z0 + 1), sample(x0 + 1, y0, z0 + 1), frac.x), lerpWeights(sample(x0, y0 + 1, z0 + 1), sample(x0 + 1, y0 + 1, z0 + 1), frac.x), frac.y);
            return packMix(lerpWeights(c0, c1, frac.z));
        };
        cpu.mix0.resize(cpu.positions.size(), Mix{0});
        for (std::size_t vertex = 0; vertex < cpu.positions.size(); ++vertex) {
            const vec3 inward = cpu.positions[vertex] - cpu.normals[vertex] * (0.25f * meters);
            cpu.mix0[vertex] = mixAt(inward);
        }
        return cpu;
    }

}
