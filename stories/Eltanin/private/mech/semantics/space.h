#pragma once

#include "shapes.h"

#include <vector>

#include <glm/ext/matrix_int3x3.hpp>
#include <glm/ext/vector_int3.hpp>
#include <glm/vec3.hpp>

namespace eltanin::mech {

    // Cube orientation alphabet (RedStar CubicRotation). Index 0..23; 0 = identity.
    // matrix[i] — 3×3 in {-1,0,1}; listed by rows (see rows()). Body-local meters: rotate about center, then × edgeMeters (no +½ — centering is the project rule).
    // turn* — after ±90° about local axis: new index = turnXp[old] (etc.).
    namespace orient {

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

        inline const std::vector<int> invert{
            0, 3, 2, 1, 12, 18, 6, 22, 8, 9, 10, 11, 4, 20, 14, 16, 15, 17, 5, 21, 13, 19, 7, 23,
        };

        inline const std::vector<int> turnXp{1, 2, 3, 0, 16, 17, 18, 19, 11, 8, 9, 10, 22, 23, 20, 21, 14, 15, 12, 13, 4, 5, 6, 7};
        inline const std::vector<int> turnXn{3, 0, 1, 2, 20, 21, 22, 23, 9, 10, 11, 8, 18, 19, 16, 17, 4, 5, 6, 7, 14, 15, 12, 13};
        inline const std::vector<int> turnYp{4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2, 3, 17, 18, 19, 16, 23, 20, 21, 22};
        inline const std::vector<int> turnYn{12, 13, 14, 15, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 19, 16, 17, 18, 21, 22, 23, 20};
        inline const std::vector<int> turnZp{19, 16, 17, 18, 7, 4, 5, 6, 23, 20, 21, 22, 13, 14, 15, 12, 11, 8, 9, 10, 3, 0, 1, 2};
        inline const std::vector<int> turnZn{21, 22, 23, 20, 5, 6, 7, 4, 17, 18, 19, 16, 15, 12, 13, 14, 1, 2, 3, 0, 9, 10, 11, 8};

        // Body-local meters: R · (lattice − ½) · edgeMeters.
        inline auto toLocal(int orientation, glm::ivec3 lattice) -> glm::vec3 {
            return glm::mat3(matrix[static_cast<std::size_t>(orientation)]) * (glm::vec3(lattice) - glm::vec3{0.5f}) * cube::edgeMeters;
        }

        // Lattice corner index 0..7 after orientation ({0,1} ↔ {−1,+1} round-trip, exact on the group).
        inline auto cornerIndex(int orientation, int corner) -> int {
            const glm::ivec3 signedIn = cube::corners[static_cast<std::size_t>(corner)] * 2 - 1;
            const glm::ivec3 want = (matrix[static_cast<std::size_t>(orientation)] * signedIn + 1) / 2;
            for (std::size_t i = 0; i < cube::corners.size(); ++i) {
                if (cube::corners[i] == want) return static_cast<int>(i);
            }
            return corner;
        }

    } // namespace orient

}
