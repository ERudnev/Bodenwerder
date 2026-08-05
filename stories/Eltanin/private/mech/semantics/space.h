#pragma once

#include "shapes.h"

#include <vector>

#include <base/types/common_types.h>
#include <glm/ext/matrix_int3x3.hpp>
#include <glm/ext/vector_int3.hpp>
#include <glm/common.hpp>
#include <glm/vec3.hpp>

namespace eltanin::mech {

    // Lattice → body-local meters (identity orientation). Topology tables stay in shapes.h / cube.
    namespace physical {

        // Cell edge in game meters. Lattice {0,1} → body-local meters.
        // Project rule: cell geometry is centered on the origin — subtract ½ is fixed, not a parameter.
        inline constexpr float edgeMeters = 4.0f;

        // Lattice integer coords (typically cube::corners {0,1}³) → meters, cell-centered.
        inline auto toLocal(glm::ivec3 lattice) -> glm::vec3 {
            return (glm::vec3(lattice) - glm::vec3{0.5f}) * edgeMeters;
        }

        // Inverse of toLocal on the lattice image (round for float noise).
        inline auto fromLocal(glm::vec3 local) -> glm::ivec3 {
            return glm::ivec3(glm::round(local / edgeMeters + glm::vec3{0.5f}));
        }

    } // namespace physical

    // Cube orientation alphabet (RedStar CubicRotation). Key 0..23; 0 = identity.
    // matrix[i] — 3×3 in {-1,0,1}; listed by rows (see rows()). Body-local meters: rotate about center, then × edgeMeters (no +½ — centering is the project rule).
    // turn* — after ±90° about local axis: new key = turnXp[old] (etc.).
    namespace orient {

        // 0..23 — discrete cube orientation (algebra via invert / turn* tables, not enum).
        using key = int;

        // GLM ctor takes columns; build columns from three row vectors so the table stays row-readable.
        inline auto rows(glm::ivec3 r0, glm::ivec3 r1, glm::ivec3 r2) -> glm::imat3 {
            return glm::imat3{
                glm::ivec3{r0.x, r1.x, r2.x},
                glm::ivec3{r0.y, r1.y, r2.y},
                glm::ivec3{r0.z, r1.z, r2.z},
            };
        }

        inline const std::vector<glm::imat3> matrix{
            rows({ 1,  0,  0}, { 0,  1,  0}, { 0,  0,  1}),
            rows({ 1,  0,  0}, { 0,  0,  1}, { 0, -1,  0}),
            rows({ 1,  0,  0}, { 0, -1,  0}, { 0,  0, -1}),
            rows({ 1,  0,  0}, { 0,  0, -1}, { 0,  1,  0}),
            rows({ 0,  0, -1}, { 0,  1,  0}, { 1,  0,  0}),
            rows({ 0,  0, -1}, { 1,  0,  0}, { 0, -1,  0}),
            rows({ 0,  0, -1}, { 0, -1,  0}, {-1,  0,  0}),
            rows({ 0,  0, -1}, {-1,  0,  0}, { 0,  1,  0}),
            rows({-1,  0,  0}, { 0,  1,  0}, { 0,  0, -1}),
            rows({-1,  0,  0}, { 0,  0, -1}, { 0, -1,  0}),
            rows({-1,  0,  0}, { 0, -1,  0}, { 0,  0,  1}),
            rows({-1,  0,  0}, { 0,  0,  1}, { 0,  1,  0}),
            rows({ 0,  0,  1}, { 0,  1,  0}, {-1,  0,  0}),
            rows({ 0,  0,  1}, {-1,  0,  0}, { 0, -1,  0}),
            rows({ 0,  0,  1}, { 0, -1,  0}, { 1,  0,  0}),
            rows({ 0,  0,  1}, { 1,  0,  0}, { 0,  1,  0}),
            rows({ 0,  1,  0}, { 0,  0,  1}, { 1,  0,  0}),
            rows({ 0,  1,  0}, { 1,  0,  0}, { 0,  0, -1}),
            rows({ 0,  1,  0}, { 0,  0, -1}, {-1,  0,  0}),
            rows({ 0,  1,  0}, {-1,  0,  0}, { 0,  0,  1}),
            rows({ 0, -1,  0}, { 0,  0, -1}, { 1,  0,  0}),
            rows({ 0, -1,  0}, { 1,  0,  0}, { 0,  0,  1}),
            rows({ 0, -1,  0}, { 0,  0,  1}, {-1,  0,  0}),
            rows({ 0, -1,  0}, {-1,  0,  0}, { 0,  0, -1}),
        };

        inline const std::vector<key> invert{
            0, 3, 2, 1, 12, 18, 6, 22, 8, 9, 10, 11, 4, 20, 14, 16, 15, 17, 5, 21, 13, 19, 7, 23,
        };

        inline const std::vector<key> turnXp{1, 2, 3, 0, 16, 17, 18, 19, 11, 8, 9, 10, 22, 23, 20, 21, 14, 15, 12, 13, 4, 5, 6, 7};
        inline const std::vector<key> turnXn{3, 0, 1, 2, 20, 21, 22, 23, 9, 10, 11, 8, 18, 19, 16, 17, 4, 5, 6, 7, 14, 15, 12, 13};
        inline const std::vector<key> turnYp{4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2, 3, 17, 18, 19, 16, 23, 20, 21, 22};
        inline const std::vector<key> turnYn{12, 13, 14, 15, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 19, 16, 17, 18, 21, 22, 23, 20};
        inline const std::vector<key> turnZp{19, 16, 17, 18, 7, 4, 5, 6, 23, 20, 21, 22, 13, 14, 15, 12, 11, 8, 9, 10, 3, 0, 1, 2};
        inline const std::vector<key> turnZn{21, 22, 23, 20, 5, 6, 7, 4, 17, 18, 19, 16, 15, 12, 13, 14, 1, 2, 3, 0, 9, 10, 11, 8};

        // Body-local meters: R · physical::toLocal(lattice).
        inline auto toLocal(key orientation, glm::ivec3 lattice) -> glm::vec3 {
            return glm::mat3(matrix[static_cast<std::size_t>(orientation)]) * physical::toLocal(lattice);
        }

        // Lattice Corner 0..7 after orientation ({0,1} ↔ {−1,+1} round-trip, exact on the group).
        inline auto cornerIndex(key orientation, cube::Corner corner) -> cube::Corner {
            const glm::ivec3 signedIn = cube::corners[static_cast<std::size_t>(corner)] * 2 - 1;
            const glm::ivec3 want = (matrix[static_cast<std::size_t>(orientation)] * signedIn + 1) / 2;
            for (std::size_t i = 0; i < cube::corners.size(); ++i) {
                if (cube::corners[i] == want) return static_cast<cube::Corner>(i);
            }
            return corner;
        }

    } // namespace orient

    // Discrete cell pose on the construction lattice (editor / blueprint space).
    struct Pose {
        base::common_types::index3 pos;
        orient::key ori;
    };

}
