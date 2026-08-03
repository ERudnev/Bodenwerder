#pragma once

#include <vector>

#include <glm/ext/matrix_int3x3.hpp>
#include <glm/ext/vector_int3.hpp>
#include <glm/matrix.hpp>
#include <glm/vec3.hpp>

// Eltanin constructor vocabulary (levels 0–1). Lattice/topology from RedStar Carcass*. Winding: face/plate perimeters CCW from outside (RH outward); RedStar/D3D K8 mixed, restated CCW. Digit strings keep RedStar spelling (p2121 = walk from corner 0 on that plate's loop).

namespace eltanin::mech {

    // Level 0 — how a piece lives on the ship (not form, not function).
    enum class Layer {
        hull,  // skin on the outside of frame volumes (plates, fairings)
        frame, // volumetric skeleton; cells that dock frame↔frame
        inner, // mechanism inscribed in frame cell(s); binds to the cell lattice
    };

    // Unit cube lattice — shared address space for corners / edges / faces. Cell size = 1; frame cuts keep a subset of these 8 corner indices.
    namespace cube {

        // index → (x,y,z) in {0,1}³  (RedStar coord_index3)
        inline constexpr std::vector<glm::ivec3> corners{
            {0, 0, 0}, // 0
            {1, 0, 0}, // 1
            {0, 1, 0}, // 2
            {1, 1, 0}, // 3
            {0, 0, 1}, // 4
            {1, 0, 1}, // 5
            {0, 1, 1}, // 6
            {1, 1, 1}, // 7
        };

        enum class Face { Xp, Xn, Yp, Yn, Zp, Zn, };

        // CCW from outside,
        inline constexpr std::vector<std::vector<int>> faces {
            {1, 3, 7, 5}, // Xp
            {0, 4, 6, 2}, // Xn
            {2, 6, 7, 3}, // Yp
            {0, 1, 5, 4}, // Yn
            {4, 5, 7, 6}, // Zp
            {0, 2, 3, 1}, // Zn
        };

        // Cell edge in game meters (kube4m de-facto diameter). Lattice {0,1} → body-local meters.
        // Project rule: cell geometry is centered on the origin — subtract ½ is fixed, not a parameter.
        inline constexpr float edgeMeters = 4.0f;

        inline constexpr auto toLocal(glm::ivec3 lattice) -> glm::vec3 {
            return (glm::vec3(lattice) - glm::vec3{0.5f}) * edgeMeters;
        }

    } // namespace cube

    // Level 1 — form alphabet (topology only; device roles are level 2).

    struct frame {
        enum class shape {
            k8, // full cube — 8 corners, 6 plates
            k7, // one corner removed — 7 corners, 7 plates
            k6, // edge cut — 6 corners, 5 plates
            k4, // tetrahedral remainder — 4 corners, 4 plates
        };

        // Corners present (RedStar CarcassCube::vertexIndex).
        inline static constexpr std::vector<std::vector<int>> corners{
            {0, 1, 2, 3, 4, 5, 6, 7},
            {0, 1, 3, 4, 5, 6, 7},
            {0, 1, 4, 5, 6, 7},
            {1, 4, 5, 7},
        };
    };

    // Hull plate. Digits = edge codes around perimeter (CCW out). A/V: sharp tip up / down on √2-√2-√2 triangles.
    struct hull {
        enum class shape {
            p1111,
            p121,
            p2121,
            p222A,
            p222V,
        };

        // Canonical perimeter in unit-cube corner indices, CCW from outside. Exemplars from RedStar geometry*tuple; p222A reversed vs RedStar for CCW-out.
        inline static constexpr std::vector<std::vector<int>> perimeter{
            {0, 2, 3, 1}, // p1111 — Zn
            {4, 0, 6},    // p121  — walk spells 1-2-1; CCW out (Xn side)
            {0, 6, 7, 1}, // p2121 — K6 diagonal; spells 2-1-2-1 from corner 0
            {4, 1, 7},    // p222A — K4 apex (RedStar was 4,7,1)
            {0, 6, 3},    // p222V — K7 valley
        };
    };

    // Free diagonal membrane (RedStar Wing). Corner sets from Wing::vertexIndex; rebuild emission loops with CCW-out when generating mesh.
    struct wing {
        enum class shape {
            w1111,
            w121,
            w2121,
            w321,
            w222,
        };

        inline static constexpr std::vector<std::vector<int>> corners{
            {0, 1, 2, 3},
            {0, 1, 2},
            {0, 1, 6, 7},
            {0, 1, 7},
            {1, 4, 7},
        };
    };

    // Inner volume form only. engine / hangar / … → level 2 later.
    struct inner {
        enum class category {
            full,
            half,
            quarter,
            octa,
        };
    };

    // Frame cell → default hull topology per plate index (RedStar CarcassCube::plateType).
    namespace skinning {

        // [frame::shape] → hull shapes for that cell's plates (length = plate count).
        inline constexpr std::vector<std::vector<hull::shape>> default_hull{
            {hull::shape::p1111, hull::shape::p1111, hull::shape::p1111, hull::shape::p1111, hull::shape::p1111, hull::shape::p1111},
            {hull::shape::p222V, hull::shape::p1111, hull::shape::p121, hull::shape::p121, hull::shape::p1111, hull::shape::p1111, hull::shape::p121},
            {hull::shape::p2121, hull::shape::p121, hull::shape::p121, hull::shape::p1111, hull::shape::p1111},
            {hull::shape::p222A, hull::shape::p121, hull::shape::p121, hull::shape::p121},
        };

    } // namespace skinning

    // Cube orientation alphabet (RedStar CubicRotation). Index 0..23; 0 = identity.
    // matrix[i] — 3×3 in {-1,0,1}; listed by rows (see rows()). Body-local meters: rotate about center, then × edgeMeters (no +½ — centering is the project rule).
    // turn* — after ±90° about local axis: new index = turnXp[old] (etc.).
    namespace orient {

        // GLM ctor takes columns; rows() keeps the table readable as three row vectors.
        inline constexpr auto rows(glm::ivec3 r0, glm::ivec3 r1, glm::ivec3 r2) -> glm::imat3 {
            return glm::transpose(glm::imat3{r0, r1, r2});
        }

        inline constexpr std::vector<glm::imat3> matrix{
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

        inline constexpr std::vector<int> invert{
            0, 3, 2, 1, 12, 18, 6, 22, 8, 9, 10, 11, 4, 20, 14, 16, 15, 17, 5, 21, 13, 19, 7, 23,
        };

        inline constexpr std::vector<int> turnXp{1, 2, 3, 0, 16, 17, 18, 19, 11, 8, 9, 10, 22, 23, 20, 21, 14, 15, 12, 13, 4, 5, 6, 7};
        inline constexpr std::vector<int> turnXn{3, 0, 1, 2, 20, 21, 22, 23, 9, 10, 11, 8, 18, 19, 16, 17, 4, 5, 6, 7, 14, 15, 12, 13};
        inline constexpr std::vector<int> turnYp{4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2, 3, 17, 18, 19, 16, 23, 20, 21, 22};
        inline constexpr std::vector<int> turnYn{12, 13, 14, 15, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 19, 16, 17, 18, 21, 22, 23, 20};
        inline constexpr std::vector<int> turnZp{19, 16, 17, 18, 7, 4, 5, 6, 23, 20, 21, 22, 13, 14, 15, 12, 11, 8, 9, 10, 3, 0, 1, 2};
        inline constexpr std::vector<int> turnZn{21, 22, 23, 20, 5, 6, 7, 4, 17, 18, 19, 16, 15, 12, 13, 14, 1, 2, 3, 0, 9, 10, 11, 8};

        // Body-local meters: R · (lattice − ½) · edgeMeters.
        inline constexpr auto toLocal(int orientation, glm::ivec3 lattice) -> glm::vec3 {
            return glm::mat3(matrix[static_cast<std::size_t>(orientation)]) * (glm::vec3(lattice) - glm::vec3{0.5f}) * cube::edgeMeters;
        }

        // Lattice corner index 0..7 after orientation ({0,1} ↔ {−1,+1} round-trip, exact on the group).
        inline constexpr auto cornerIndex(int orientation, int corner) -> int {
            const glm::ivec3 signedIn = cube::corners[static_cast<std::size_t>(corner)] * 2 - 1;
            const glm::ivec3 want = (matrix[static_cast<std::size_t>(orientation)] * signedIn + 1) / 2;
            for (std::size_t i = 0; i < cube::corners.size(); ++i) {
                if (cube::corners[i] == want) return static_cast<int>(i);
            }
            return corner;
        }

    } // namespace orient

} // namespace eltanin::mech
