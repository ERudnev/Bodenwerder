#pragma once

#include "shapes.h"
#include "space.h"

#include <map>
#include <string_view>
#include <vector>

// Subframe data (no duplicated meaning):
// 1) corner::specs / halfEdge::specs / membrane::specs — interframe mesh inventory (asset tag, …). No per-cell placement.
// 2) shapes.h frame::corners — which cell vertices exist on each k* (topology). Not repeated here.
// 3) recipes[shape] — smoke assembly only: which mesh kind at which cell vertex/edge + orient roll.
// Edge row: cellAt0/cellAtRay = cell lattice after orient (cornerIndex(orient, mesh slot 0 / ray)); cellAt0 < cellAtRay.
// poleAtMesh0 = half-edge pole (s/e) at authored mesh slot 0. Mesh ray slot comes from halfEdge::specs[kind].ray — not stored per row.

namespace eltanin::mech::subframe {

    enum class Bend {
        deg45,
        deg71,
        deg90,
        deg125,
    };

    namespace corner {

        enum class kind {
            c124,
            c1364,
            c164,
            c134,
            c135,
            c12,
            c13,
            c15,
            c16,
            c34,
            c35,
        };

        struct Ray {
            cube::Corner target;
            Bend bend;
        };

        struct Spec {
            std::string_view code;
            std::vector<Ray> rays;
        };

        inline const std::map<kind, Spec> specs{
            {kind::c124, {.code = "c124", .rays = {{.target = 1, .bend = Bend::deg90}, {.target = 2, .bend = Bend::deg90}, {.target = 4, .bend = Bend::deg90}}}},
            {kind::c1364, {.code = "c1364", .rays = {{.target = 1, .bend = Bend::deg90}, {.target = 3, .bend = Bend::deg125}, {.target = 6, .bend = Bend::deg125}, {.target = 4, .bend = Bend::deg90}}}},
            {kind::c164, {.code = "c164", .rays = {{.target = 1, .bend = Bend::deg45}, {.target = 6, .bend = Bend::deg90}, {.target = 4, .bend = Bend::deg90}}}},
            {kind::c134, {.code = "c134", .rays = {{.target = 1, .bend = Bend::deg90}, {.target = 3, .bend = Bend::deg90}, {.target = 4, .bend = Bend::deg45}}}},
            {kind::c135, {.code = "c135", .rays = {{.target = 1, .bend = Bend::deg90}, {.target = 3, .bend = Bend::deg71}, {.target = 5, .bend = Bend::deg71}}}},
            {kind::c12, {.code = "c12", .rays = {{.target = 1, .bend = Bend::deg90}, {.target = 2, .bend = Bend::deg90}}}},
            {kind::c13, {.code = "c13", .rays = {{.target = 1, .bend = Bend::deg90}, {.target = 3, .bend = Bend::deg90}}}},
            {kind::c15, {.code = "c15", .rays = {{.target = 1, .bend = Bend::deg90}, {.target = 5, .bend = Bend::deg90}}}},
            {kind::c16, {.code = "c16", .rays = {{.target = 1, .bend = Bend::deg45}, {.target = 6, .bend = Bend::deg90}}}},
            {kind::c34, {.code = "c34", .rays = {{.target = 3, .bend = Bend::deg90}, {.target = 4, .bend = Bend::deg45}}}},
            {kind::c35, {.code = "c35", .rays = {{.target = 3, .bend = Bend::deg71}, {.target = 5, .bend = Bend::deg71}}}},
        };

    } // namespace corner

    namespace halfEdge {

        enum class kind {
            he1deg90,
            he1deg45,
            he3deg71,
            he3deg90,
            he3deg125,
        };

        enum class Pole {
            s,
            e,
        };

        struct Spec {
            std::string_view code;
            int ray;
            Bend bend;
        };

        inline auto opposite(Pole pole) -> Pole {
            return pole == Pole::s ? Pole::e : Pole::s;
        }

        inline const std::map<kind, Spec> specs{
            {kind::he1deg90, {.code = "he1deg90", .ray = 1, .bend = Bend::deg90}},
            {kind::he1deg45, {.code = "he1deg45", .ray = 1, .bend = Bend::deg45}},
            {kind::he3deg71, {.code = "he3deg71", .ray = 3, .bend = Bend::deg71}},
            {kind::he3deg90, {.code = "he3deg90", .ray = 3, .bend = Bend::deg90}},
            {kind::he3deg125, {.code = "he3deg125", .ray = 3, .bend = Bend::deg125}},
        };

    } // namespace halfEdge

    namespace membrane {
        enum class kind {
            u1111,
            u121,
            u2121,
            u222A,
            u222V,
        };

        struct Spec {
            std::string_view code;
        };

        inline const std::map<kind, Spec> specs{
            {kind::u1111, {.code = "u1111"}},
            {kind::u121, {.code = "u121"}},
            {kind::u2121, {.code = "u2121"}},
            {kind::u222A, {.code = "u222A"}},
            {kind::u222V, {.code = "u222V"}},
        };

        inline auto kindOf(plate::shape shape) -> kind {
            switch (shape) {
                case plate::shape::p1111: return kind::u1111;
                case plate::shape::p121: return kind::u121;
                case plate::shape::p2121: return kind::u2121;
                case plate::shape::p222A: return kind::u222A;
                case plate::shape::p222V: return kind::u222V;
            }
            return kind::u1111;
        }
    }

    struct Recipe {
        struct Corner {
            corner::kind kind;
            cube::Corner cellVertex;
            space::orient::key orient;
        };
        struct Edge {
            halfEdge::kind kind;
            cube::Corner cellAt0;
            cube::Corner cellAtRay;
            space::orient::key orient;
            halfEdge::Pole poleAtMesh0;
        };
        std::vector<Corner> corners;
        std::vector<Edge> edges;
    };

    inline const std::map<frame::shape, Recipe> recipes{
        {frame::shape::k8, {
            .corners = {
                {.kind = corner::kind::c124, .cellVertex = 0, .orient = 0},
                {.kind = corner::kind::c124, .cellVertex = 1, .orient = 4},
                {.kind = corner::kind::c124, .cellVertex = 2, .orient = 3},
                {.kind = corner::kind::c124, .cellVertex = 3, .orient = 7},
                {.kind = corner::kind::c124, .cellVertex = 4, .orient = 1},
                {.kind = corner::kind::c124, .cellVertex = 5, .orient = 5},
                {.kind = corner::kind::c124, .cellVertex = 6, .orient = 2},
                {.kind = corner::kind::c124, .cellVertex = 7, .orient = 6},
            },
            .edges = {
                {.kind = halfEdge::kind::he1deg90, .cellAt0 = 0, .cellAtRay = 1, .orient = 0, .poleAtMesh0 = halfEdge::Pole::s},
                {.kind = halfEdge::kind::he1deg90, .cellAt0 = 0, .cellAtRay = 2, .orient = 15, .poleAtMesh0 = halfEdge::Pole::s},
                {.kind = halfEdge::kind::he1deg90, .cellAt0 = 0, .cellAtRay = 4, .orient = 16, .poleAtMesh0 = halfEdge::Pole::s},
                {.kind = halfEdge::kind::he1deg90, .cellAt0 = 1, .cellAtRay = 3, .orient = 21, .poleAtMesh0 = halfEdge::Pole::s},
                {.kind = halfEdge::kind::he1deg90, .cellAt0 = 1, .cellAtRay = 5, .orient = 4, .poleAtMesh0 = halfEdge::Pole::s},
                {.kind = halfEdge::kind::he1deg90, .cellAt0 = 2, .cellAtRay = 3, .orient = 3, .poleAtMesh0 = halfEdge::Pole::s},
                {.kind = halfEdge::kind::he1deg90, .cellAt0 = 2, .cellAtRay = 6, .orient = 14, .poleAtMesh0 = halfEdge::Pole::s},
                {.kind = halfEdge::kind::he1deg90, .cellAt0 = 3, .cellAtRay = 7, .orient = 20, .poleAtMesh0 = halfEdge::Pole::s},
                {.kind = halfEdge::kind::he1deg90, .cellAt0 = 4, .cellAtRay = 5, .orient = 1, .poleAtMesh0 = halfEdge::Pole::s},
                {.kind = halfEdge::kind::he1deg90, .cellAt0 = 4, .cellAtRay = 6, .orient = 17, .poleAtMesh0 = halfEdge::Pole::s},
                {.kind = halfEdge::kind::he1deg90, .cellAt0 = 5, .cellAtRay = 7, .orient = 5, .poleAtMesh0 = halfEdge::Pole::s},
                {.kind = halfEdge::kind::he1deg90, .cellAt0 = 6, .cellAtRay = 7, .orient = 2, .poleAtMesh0 = halfEdge::Pole::s},
            },
        }},
        {frame::shape::k7, {
            .corners = {
                {.kind = corner::kind::c1364, .cellVertex = 0, .orient = 0},
                {.kind = corner::kind::c124, .cellVertex = 1, .orient = 4},
                {.kind = corner::kind::c1364, .cellVertex = 3, .orient = 20},
                {.kind = corner::kind::c124, .cellVertex = 4, .orient = 1},
                {.kind = corner::kind::c124, .cellVertex = 5, .orient = 5},
                {.kind = corner::kind::c1364, .cellVertex = 6, .orient = 13},
                {.kind = corner::kind::c124, .cellVertex = 7, .orient = 6},
            },
            .edges = {
                {.kind = halfEdge::kind::he1deg90, .cellAt0 = 0, .cellAtRay = 1, .orient = 0, .poleAtMesh0 = halfEdge::Pole::s},
                {.kind = halfEdge::kind::he3deg125, .cellAt0 = 0, .cellAtRay = 3, .orient = 0, .poleAtMesh0 = halfEdge::Pole::s},
                {.kind = halfEdge::kind::he1deg90, .cellAt0 = 0, .cellAtRay = 4, .orient = 16, .poleAtMesh0 = halfEdge::Pole::s},
                {.kind = halfEdge::kind::he3deg125, .cellAt0 = 0, .cellAtRay = 6, .orient = 13, .poleAtMesh0 = halfEdge::Pole::e},
                {.kind = halfEdge::kind::he1deg90, .cellAt0 = 1, .cellAtRay = 3, .orient = 21, .poleAtMesh0 = halfEdge::Pole::s},
                {.kind = halfEdge::kind::he1deg90, .cellAt0 = 1, .cellAtRay = 5, .orient = 4, .poleAtMesh0 = halfEdge::Pole::s},
                {.kind = halfEdge::kind::he3deg125, .cellAt0 = 3, .cellAtRay = 6, .orient = 20, .poleAtMesh0 = halfEdge::Pole::s},
                {.kind = halfEdge::kind::he1deg90, .cellAt0 = 3, .cellAtRay = 7, .orient = 20, .poleAtMesh0 = halfEdge::Pole::s},
                {.kind = halfEdge::kind::he1deg90, .cellAt0 = 4, .cellAtRay = 5, .orient = 1, .poleAtMesh0 = halfEdge::Pole::s},
                {.kind = halfEdge::kind::he1deg90, .cellAt0 = 4, .cellAtRay = 6, .orient = 17, .poleAtMesh0 = halfEdge::Pole::s},
                {.kind = halfEdge::kind::he1deg90, .cellAt0 = 5, .cellAtRay = 7, .orient = 5, .poleAtMesh0 = halfEdge::Pole::s},
                {.kind = halfEdge::kind::he1deg90, .cellAt0 = 6, .cellAtRay = 7, .orient = 2, .poleAtMesh0 = halfEdge::Pole::s},
            },
        }},
        {frame::shape::k6, {
            .corners = {
                {.kind = corner::kind::c164, .cellVertex = 0, .orient = 0},
                {.kind = corner::kind::c134, .cellVertex = 1, .orient = 4},
                {.kind = corner::kind::c124, .cellVertex = 4, .orient = 1},
                {.kind = corner::kind::c124, .cellVertex = 5, .orient = 5},
                {.kind = corner::kind::c134, .cellVertex = 6, .orient = 13},
                {.kind = corner::kind::c164, .cellVertex = 7, .orient = 9},
            },
            .edges = {
                {.kind = halfEdge::kind::he1deg45, .cellAt0 = 0, .cellAtRay = 1, .orient = 0, .poleAtMesh0 = halfEdge::Pole::s},
                {.kind = halfEdge::kind::he1deg90, .cellAt0 = 0, .cellAtRay = 4, .orient = 16, .poleAtMesh0 = halfEdge::Pole::s},
                {.kind = halfEdge::kind::he3deg90, .cellAt0 = 0, .cellAtRay = 6, .orient = 13, .poleAtMesh0 = halfEdge::Pole::s},
                {.kind = halfEdge::kind::he1deg90, .cellAt0 = 1, .cellAtRay = 5, .orient = 4, .poleAtMesh0 = halfEdge::Pole::s},
                {.kind = halfEdge::kind::he3deg90, .cellAt0 = 1, .cellAtRay = 7, .orient = 4, .poleAtMesh0 = halfEdge::Pole::s},
                {.kind = halfEdge::kind::he1deg90, .cellAt0 = 4, .cellAtRay = 5, .orient = 1, .poleAtMesh0 = halfEdge::Pole::s},
                {.kind = halfEdge::kind::he1deg90, .cellAt0 = 4, .cellAtRay = 6, .orient = 17, .poleAtMesh0 = halfEdge::Pole::s},
                {.kind = halfEdge::kind::he1deg90, .cellAt0 = 5, .cellAtRay = 7, .orient = 5, .poleAtMesh0 = halfEdge::Pole::s},
                {.kind = halfEdge::kind::he1deg45, .cellAt0 = 6, .cellAtRay = 7, .orient = 9, .poleAtMesh0 = halfEdge::Pole::e},
            },
        }},
        {frame::shape::k4, {
            .corners = {
                {.kind = corner::kind::c135, .cellVertex = 1, .orient = 4},
                {.kind = corner::kind::c135, .cellVertex = 4, .orient = 1},
                {.kind = corner::kind::c124, .cellVertex = 5, .orient = 5},
                {.kind = corner::kind::c135, .cellVertex = 7, .orient = 23},
            },
            .edges = {
                {.kind = halfEdge::kind::he3deg71, .cellAt0 = 1, .cellAtRay = 4, .orient = 1, .poleAtMesh0 = halfEdge::Pole::e},
                {.kind = halfEdge::kind::he1deg90, .cellAt0 = 1, .cellAtRay = 5, .orient = 4, .poleAtMesh0 = halfEdge::Pole::s},
                {.kind = halfEdge::kind::he3deg71, .cellAt0 = 1, .cellAtRay = 7, .orient = 4, .poleAtMesh0 = halfEdge::Pole::s},
                {.kind = halfEdge::kind::he1deg90, .cellAt0 = 4, .cellAtRay = 5, .orient = 1, .poleAtMesh0 = halfEdge::Pole::s},
                {.kind = halfEdge::kind::he3deg71, .cellAt0 = 4, .cellAtRay = 7, .orient = 23, .poleAtMesh0 = halfEdge::Pole::e},
                {.kind = halfEdge::kind::he1deg90, .cellAt0 = 5, .cellAtRay = 7, .orient = 5, .poleAtMesh0 = halfEdge::Pole::s},
            },
        }},
        {frame::shape::k4f1111, {
            .corners = {
                {.kind = corner::kind::c12, .cellVertex = 0, .orient = 0},
                {.kind = corner::kind::c12, .cellVertex = 1, .orient = 21},
                {.kind = corner::kind::c12, .cellVertex = 2, .orient = 19},
                {.kind = corner::kind::c12, .cellVertex = 3, .orient = 10},
            },
            .edges = {
                {.kind = halfEdge::kind::he1deg90, .cellAt0 = 0, .cellAtRay = 1, .orient = 0, .poleAtMesh0 = halfEdge::Pole::s},
                {.kind = halfEdge::kind::he1deg90, .cellAt0 = 0, .cellAtRay = 2, .orient = 15, .poleAtMesh0 = halfEdge::Pole::s},
                {.kind = halfEdge::kind::he1deg90, .cellAt0 = 1, .cellAtRay = 3, .orient = 21, .poleAtMesh0 = halfEdge::Pole::s},
                {.kind = halfEdge::kind::he1deg90, .cellAt0 = 2, .cellAtRay = 3, .orient = 3, .poleAtMesh0 = halfEdge::Pole::s},
            },
        }},
        {frame::shape::k3f121, {
            .corners = {
                {.kind = corner::kind::c12, .cellVertex = 0, .orient = 0},
                {.kind = corner::kind::c15, .cellVertex = 1, .orient = 11},
                {.kind = corner::kind::c13, .cellVertex = 2, .orient = 19},
            },
            .edges = {
                {.kind = halfEdge::kind::he1deg90, .cellAt0 = 0, .cellAtRay = 1, .orient = 0, .poleAtMesh0 = halfEdge::Pole::s},
                {.kind = halfEdge::kind::he1deg90, .cellAt0 = 0, .cellAtRay = 2, .orient = 15, .poleAtMesh0 = halfEdge::Pole::s},
                {.kind = halfEdge::kind::he3deg90, .cellAt0 = 1, .cellAtRay = 2, .orient = 19, .poleAtMesh0 = halfEdge::Pole::e},
            },
        }},
        {frame::shape::k4f2121, {
            .corners = {
                {.kind = corner::kind::c164, .cellVertex = 0, .orient = 0},
                {.kind = corner::kind::c134, .cellVertex = 1, .orient = 4},
                {.kind = corner::kind::c134, .cellVertex = 6, .orient = 13},
                {.kind = corner::kind::c164, .cellVertex = 7, .orient = 9},
            },
            .edges = {
                {.kind = halfEdge::kind::he1deg45, .cellAt0 = 0, .cellAtRay = 1, .orient = 0, .poleAtMesh0 = halfEdge::Pole::s},
                {.kind = halfEdge::kind::he3deg90, .cellAt0 = 0, .cellAtRay = 6, .orient = 13, .poleAtMesh0 = halfEdge::Pole::e},
                {.kind = halfEdge::kind::he3deg90, .cellAt0 = 1, .cellAtRay = 7, .orient = 4, .poleAtMesh0 = halfEdge::Pole::s},
                {.kind = halfEdge::kind::he1deg45, .cellAt0 = 6, .cellAtRay = 7, .orient = 9, .poleAtMesh0 = halfEdge::Pole::e},
            },
        }},
        {frame::shape::k3f222, {
            .corners = {
                {.kind = corner::kind::c135, .cellVertex = 1, .orient = 4},
                {.kind = corner::kind::c135, .cellVertex = 4, .orient = 1},
                {.kind = corner::kind::c135, .cellVertex = 7, .orient = 23},
            },
            .edges = {
                {.kind = halfEdge::kind::he3deg71, .cellAt0 = 1, .cellAtRay = 4, .orient = 1, .poleAtMesh0 = halfEdge::Pole::e},
                {.kind = halfEdge::kind::he3deg71, .cellAt0 = 1, .cellAtRay = 7, .orient = 4, .poleAtMesh0 = halfEdge::Pole::s},
                {.kind = halfEdge::kind::he3deg71, .cellAt0 = 4, .cellAtRay = 7, .orient = 23, .poleAtMesh0 = halfEdge::Pole::e},
            },
        }},
    };

} // namespace eltanin::mech::subframe
