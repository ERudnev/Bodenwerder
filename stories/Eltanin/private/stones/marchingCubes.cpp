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

        void stampOccupied(vector<std::uint8_t>& occupied, index3 rootOrigin, integer extent, const Volume& node) {
            if (not node.children.empty()) {
                for (const auto& child : node.children)
                    stampOccupied(occupied, rootOrigin, extent, child);
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
                        occupied[static_cast<std::size_t>(gridX + extent * (gridY + extent * gridZ))] = 1;
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

        auto onIso(float fill) -> bool {
            return glm::abs(fill - isoLevel) <= 1.0e-6f;
        }

        auto axisAlignedFace(vec3 a, vec3 b, vec3 c) -> bool {
            const float eps = 1.0e-5f;
            return (glm::abs(a.x - b.x) <= eps and glm::abs(a.x - c.x) <= eps) or (glm::abs(a.y - b.y) <= eps and glm::abs(a.y - c.y) <= eps) or (glm::abs(a.z - b.z) <= eps and glm::abs(a.z - c.z) <= eps);
        }

        auto interpolate(vec3 a, float fillA, vec3 b, float fillB) -> vec3 {
            const float delta = fillB - fillA;
            if (glm::abs(delta) < 1.0e-6f)
                return a;
            return glm::mix(a, b, (isoLevel - fillA) / delta);
        }

        void emitOriented(CpuPresentation& cpu, vec3 a, vec3 b, vec3 c, vec3 inside) {
            vec3 normal = glm::cross(b - a, c - a);
            const float length = glm::length(normal);
            if (length < 1.0e-8f)
                return;
            normal /= length;
            const vec3 mid = (a + b + c) / 3.0f;
            if (glm::dot(normal, mid - inside) < 0.0f) {
                std::swap(b, c);
                normal = -normal;
            }
            cpu.positions.push_back(a);
            cpu.positions.push_back(b);
            cpu.positions.push_back(c);
            cpu.normals.push_back(normal);
            cpu.normals.push_back(normal);
            cpu.normals.push_back(normal);
        }

        void emitTet(CpuPresentation& cpu, const std::array<vec3, 4>& pos, const std::array<float, 4>& fill) {
            int mask = 0;
            for (int corner = 0; corner < 4; ++corner) {
                if (fill[corner] >= isoLevel)
                    mask |= 1 << corner;
            }
            const int insideCount = (mask & 1) + ((mask >> 1) & 1) + ((mask >> 2) & 1) + ((mask >> 3) & 1);
            if (insideCount == 0)
                return;
            if (insideCount == 4) {
                int on[4];
                int onCount = 0;
                vec3 centroid{0.0f, 0.0f, 0.0f};
                for (int corner = 0; corner < 4; ++corner) {
                    centroid += pos[corner];
                    if (not onIso(fill[corner]))
                        continue;
                    on[onCount] = corner;
                    ++onCount;
                }
                centroid *= 0.25f;
                if (onCount == 3 and axisAlignedFace(pos[on[0]], pos[on[1]], pos[on[2]]))
                    emitOriented(cpu, pos[on[0]], pos[on[1]], pos[on[2]], centroid);
                return;
            }

            vec3 inside{0.0f, 0.0f, 0.0f};
            float insideMass = 0.0f;
            for (int corner = 0; corner < 4; ++corner) {
                if ((mask & (1 << corner)) == 0)
                    continue;
                inside += pos[corner];
                insideMass += 1.0f;
            }
            inside /= insideMass;

            if (insideCount == 1 or insideCount == 3) {
                const int lone = insideCount == 1 ? mask : (mask ^ 15);
                int loneIndex = 0;
                while ((lone & (1 << loneIndex)) == 0)
                    ++loneIndex;
                const int a = (loneIndex + 1) & 3;
                const int b = (loneIndex + 2) & 3;
                const int c = (loneIndex + 3) & 3;
                emitOriented(cpu, interpolate(pos[loneIndex], fill[loneIndex], pos[a], fill[a]), interpolate(pos[loneIndex], fill[loneIndex], pos[b], fill[b]), interpolate(pos[loneIndex], fill[loneIndex], pos[c], fill[c]), inside);
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
            const auto ac = interpolate(pos[first], fill[first], pos[outside[0]], fill[outside[0]]);
            const auto ad = interpolate(pos[first], fill[first], pos[outside[1]], fill[outside[1]]);
            const auto bc = interpolate(pos[second], fill[second], pos[outside[0]], fill[outside[0]]);
            const auto bd = interpolate(pos[second], fill[second], pos[outside[1]], fill[outside[1]]);
            emitOriented(cpu, ac, bc, bd, inside);
            emitOriented(cpu, ac, bd, ad, inside);
        }

    } // namespace

    auto meshVolume(const Volume& root) -> CpuPresentation {
        CpuPresentation cpu{
            .layout = rmmr::primitive::GeometrySemantics::layoutIds(vector<string>{"position", "normal"}),
            .positions = {},
            .normals = {},
            .uv0 = {},
            .color0 = {},
            .indices = {},
        };
        if (root.scale < 0)
            return cpu;
        const integer extent = edgeCells(root.scale);
        vector<std::uint8_t> occupied(static_cast<std::size_t>(extent * extent * extent), 0);
        stampOccupied(occupied, root.origin, extent, root);

        const float meters = mech::space::local::edge2meters;
        auto latticePos = [&](integer gridX, integer gridY, integer gridZ) -> vec3 {
            return vec3{static_cast<float>(root.origin.x + gridX), static_cast<float>(root.origin.y + gridY), static_cast<float>(root.origin.z + gridZ)} * meters;
        };

        for (integer cellZ = 0; cellZ < extent; ++cellZ) {
            for (integer cellY = 0; cellY < extent; ++cellY) {
                for (integer cellX = 0; cellX < extent; ++cellX) {
                    std::array<vec3, 8> cornerPos{};
                    std::array<float, 8> cornerFill{};
                    for (int corner = 0; corner < 8; ++corner) {
                        const integer gridX = cellX + cubeCorner[corner].x;
                        const integer gridY = cellY + cubeCorner[corner].y;
                        const integer gridZ = cellZ + cubeCorner[corner].z;
                        cornerPos[static_cast<std::size_t>(corner)] = latticePos(gridX, gridY, gridZ);
                        cornerFill[static_cast<std::size_t>(corner)] = vertexFill(occupied, extent, gridX, gridY, gridZ);
                    }
                    for (const auto& tet : tetCorners) {
                        emitTet(cpu, {cornerPos[static_cast<std::size_t>(tet[0])], cornerPos[static_cast<std::size_t>(tet[1])], cornerPos[static_cast<std::size_t>(tet[2])], cornerPos[static_cast<std::size_t>(tet[3])]}, {cornerFill[static_cast<std::size_t>(tet[0])], cornerFill[static_cast<std::size_t>(tet[1])], cornerFill[static_cast<std::size_t>(tet[2])], cornerFill[static_cast<std::size_t>(tet[3])]});
                    }
                }
            }
        }
        return cpu;
    }

}
