#pragma once

#include <vector>

#include <base/types/common_types.h>
#include <eltanin/mech/semantics.q1.h>

// Topology tables only. Enums/types: model/eltanin/mech/semantics.q1.h (doctrine/mech/semantics.q1.types).
// Tables are inline const (not constexpr): MSVC STL rejects non-transient constexpr std::vector.

namespace eltanin::mech {

    using base::common_types::ivec3;

    // Unit cube lattice — shared address space for corners / edges / faces. Cell size = 1; frame cuts keep a subset of these 8 corner indices.
    // Metric embedding (edge2meters / cell2local) → mech::space in space.h.
    namespace cube {

        // Ordered closed walk of corners (CCW from outside when it is a face perimeter).
        using Loop = std::vector<Corner>;

        // Corner → (x,y,z) in {0,1}³
        inline const std::vector<ivec3> corners{
            {0, 0, 0}, // 0
            {1, 0, 0}, // 1
            {0, 1, 0}, // 2
            {1, 1, 0}, // 3
            {0, 0, 1}, // 4
            {1, 0, 1}, // 5
            {0, 1, 1}, // 6
            {1, 1, 1}, // 7
        };

        // CCW from outside; index matches Face enumerator order.
        inline const std::vector<Loop> faces{
            {1, 3, 7, 5}, // Xp
            {0, 4, 6, 2}, // Xn
            {2, 6, 7, 3}, // Yp
            {0, 1, 5, 4}, // Yn
            {4, 5, 7, 6}, // Zp
            {0, 2, 3, 1}, // Zn
        };

    } // namespace cube

    // Level 1 — form alphabet tables (enums in model).
    namespace frame {

        using Corners = std::vector<cube::Corner>;
        using Loop = cube::Loop;
        // Index into faces[shape] / skinning::default_plate[shape] (same numbering).
        using FaceIndex = int;

        // Corners present (RedStar CarcassCube::vertexIndex; flats from former Wing::vertexIndex).
        inline const std::vector<Corners> corners{
            {0, 1, 2, 3, 4, 5, 6, 7},
            {0, 1, 3, 4, 5, 6, 7},
            {0, 1, 4, 5, 6, 7},
            {1, 4, 5, 7},
            {0, 1, 2, 3},
            {0, 1, 2},
            {0, 1, 6, 7},
            {1, 4, 7},
        };

        // Face loops per shape; index aligns with skinning::default_plate.
        // Volumetric: cut/special face first (when present), then cube::Face order with missing corners dropped (destroyed axis faces omitted).
        // Flats: one outer membrane face (ex-wing).
        inline const std::vector<std::vector<Loop>> faces{
            // k8
            {
                {1, 3, 7, 5},
                {0, 4, 6, 2},
                {2, 6, 7, 3},
                {0, 1, 5, 4},
                {4, 5, 7, 6},
                {0, 2, 3, 1},
            },
            // k7 — remove corner 2; cut = p222V
            {
                {0, 6, 3},    // 0 cut
                {1, 3, 7, 5}, // 1 Xp
                {0, 4, 6},    // 2 Xn'
                {6, 7, 3},    // 3 Yp'
                {0, 1, 5, 4}, // 4 Yn
                {4, 5, 7, 6}, // 5 Zp
                {0, 3, 1},    // 6 Zn'
            },
            // k6 — remove edge 2–3; cut = p2121
            {
                {0, 6, 7, 1}, // 0 cut
                {1, 7, 5},    // 1 Xp'
                {0, 4, 6},    // 2 Xn'
                {0, 1, 5, 4}, // 3 Yn
                {4, 5, 7, 6}, // 4 Zp
            },
            // k4 — tetra {1,4,5,7}; apex = p222A
            {
                {4, 1, 7}, // 0 apex
                {1, 5, 7},
                {4, 5, 7},
                {1, 4, 5},
            },
            // k4f1111 — single outer face
            {
                {0, 2, 3, 1},
            },
            // k3f121
            {
                {0, 2, 1},
            },
            // k4f2121
            {
                {0, 6, 7, 1},
            },
            // k3f222
            {
                {4, 1, 7},
            },
        };

    } // namespace frame

    inline auto isFlat(frame::shape s) -> bool {
        switch (s) {
            case frame::shape::k4f1111:
            case frame::shape::k3f121:
            case frame::shape::k4f2121:
            case frame::shape::k3f222: return true;
            default: return false;
        }
    }

    // Plate perimeter tables (enum in model). Digits = edge codes around perimeter (CCW out).
    namespace plate {

        // Canonical perimeter in unit-cube corner indices, CCW from outside. Must match interframe membrane authorship (entry pivot + spanned corners); p222A reversed vs RedStar for CCW-out.
        inline const std::vector<frame::Loop> perimeter{
            {0, 2, 3, 1}, // p1111 — Zn (u1111)
            {1, 3, 0},    // p121  — Zn triangle; spells 1-2-1; CCW out (u121 at corners 0,1,3 — not Xn {4,0,6})
            {0, 6, 7, 1}, // p2121 — K6 diagonal; spells 2-1-2-1 from corner 0 (u2121)
            {4, 1, 7},    // p222A — K4 apex (u222A; RedStar was 4,7,1)
            {0, 6, 3},    // p222V — K7 valley (u222V)
        };

    } // namespace plate

    // Frame cell → default plate topology per plate index (RedStar CarcassCube::plateType).
    namespace skinning {

        // [frame::shape] → plate shapes for that cell's faces (length = plate count). Same indices as frame::faces.
        inline const std::vector<std::vector<plate::shape>> default_plate{
            {plate::shape::p1111, plate::shape::p1111, plate::shape::p1111, plate::shape::p1111, plate::shape::p1111, plate::shape::p1111},
            {plate::shape::p222V, plate::shape::p1111, plate::shape::p121, plate::shape::p121, plate::shape::p1111, plate::shape::p1111, plate::shape::p121},
            {plate::shape::p2121, plate::shape::p121, plate::shape::p121, plate::shape::p1111, plate::shape::p1111},
            {plate::shape::p222A, plate::shape::p121, plate::shape::p121, plate::shape::p121},
            {plate::shape::p1111},
            {plate::shape::p121},
            {plate::shape::p2121},
            {plate::shape::p222A},
        };

    } // namespace skinning

}
