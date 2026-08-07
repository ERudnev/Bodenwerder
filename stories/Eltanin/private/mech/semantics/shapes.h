#pragma once

#include <vector>

#include <glm/ext/vector_int3.hpp>

// Level 1 form alphabet + shared unit-cube lattice topology. Topology from RedStar Carcass*. Winding: face/plate perimeters CCW from outside (RH outward); RedStar/D3D K8 mixed, restated CCW. Digit strings keep RedStar spelling (p2121 = walk from corner 0 on that plate's loop).
// Tables are inline const (not constexpr): MSVC STL rejects non-transient constexpr std::vector.

namespace eltanin::mech {

    // Unit cube lattice — shared address space for corners / edges / faces. Cell size = 1; frame cuts keep a subset of these 8 corner indices.
    // Metric embedding (edgeMeters / toLocal) → mech::physical in space.h.
    namespace cube {

        // 0..7 — index into corners[] / unit-cube vertex (RedStar coord_index3).
        using Corner = int;

        // Corner → (x,y,z) in {0,1}³
        inline const std::vector<glm::ivec3> corners{
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
        inline const std::vector<std::vector<Corner>> faces{
            {1, 3, 7, 5}, // Xp
            {0, 4, 6, 2}, // Xn
            {2, 6, 7, 3}, // Yp
            {0, 1, 5, 4}, // Yn
            {4, 5, 7, 6}, // Zp
            {0, 2, 3, 1}, // Zn
        };

    } // namespace cube

    // Level 1 — form alphabet (topology only; device roles are level 2).

    struct frame {
        enum class shape {
            k8, // full cube — 8 corners, 6 plates
            k7, // one corner removed — 7 corners, 7 plates
            k6, // edge cut — 6 corners, 5 plates
            k4, // tetrahedral remainder — 4 corners, 4 plates
            // Flat membrane halves (ex-wing, cut along): k{N}f{digits} = N cube corners, flat, perimeter edge codes.
            // Two flats may share one cell volume when their corner sets are disjoint.
            k4f1111, // ex w1111 half — 4-gon 1-1-1-1
            k3f121,  // ex w121 half — triangle 1-2-1
            k4f2121, // ex w2121 half — 4-gon 2-1-2-1
            k3f222,  // ex w222 half — triangle 2-2-2 (w321 dropped: own plate alphabet)
        };

        // Corners present (RedStar CarcassCube::vertexIndex; flats from former Wing::vertexIndex).
        inline static const std::vector<std::vector<cube::Corner>> corners{
            {0, 1, 2, 3, 4, 5, 6, 7},
            {0, 1, 3, 4, 5, 6, 7},
            {0, 1, 4, 5, 6, 7},
            {1, 4, 5, 7},
            {0, 1, 2, 3},
            {0, 1, 2},
            {0, 1, 6, 7},
            {1, 4, 7},
        };
    };

    inline auto isFlat(frame::shape s) -> bool {
        switch (s) {
            case frame::shape::k4f1111:
            case frame::shape::k3f121:
            case frame::shape::k4f2121:
            case frame::shape::k3f222: return true;
            default: return false;
        }
    }

    // Plate (skin tile on a frame face). Digits = edge codes around perimeter (CCW out). A/V: sharp tip up / down on √2-√2-√2 triangles.
    struct plate {
        enum class shape {
            p1111,
            p121,
            p2121,
            p222A,
            p222V,
        };

        // Canonical perimeter in unit-cube corner indices, CCW from outside. Exemplars from RedStar geometry*tuple; p222A reversed vs RedStar for CCW-out.
        inline static const std::vector<std::vector<cube::Corner>> perimeter{
            {0, 2, 3, 1}, // p1111 — Zn
            {4, 0, 6},    // p121  — walk spells 1-2-1; CCW out (Xn side)
            {0, 6, 7, 1}, // p2121 — K6 diagonal; spells 2-1-2-1 from corner 0
            {4, 1, 7},    // p222A — K4 apex (RedStar was 4,7,1)
            {0, 6, 3},    // p222V — K7 valley
        };
    };

    // Frame cell → default plate topology per plate index (RedStar CarcassCube::plateType). Flats: empty (no skin alphabet yet).
    namespace skinning {

        // [frame::shape] → plate shapes for that cell's faces (length = plate count).
        inline const std::vector<std::vector<plate::shape>> default_plate{
            {plate::shape::p1111, plate::shape::p1111, plate::shape::p1111, plate::shape::p1111, plate::shape::p1111, plate::shape::p1111},
            {plate::shape::p222V, plate::shape::p1111, plate::shape::p121, plate::shape::p121, plate::shape::p1111, plate::shape::p1111, plate::shape::p121},
            {plate::shape::p2121, plate::shape::p121, plate::shape::p121, plate::shape::p1111, plate::shape::p1111},
            {plate::shape::p222A, plate::shape::p121, plate::shape::p121, plate::shape::p121},
            {},
            {},
            {},
            {},
        };

    } // namespace skinning

}
