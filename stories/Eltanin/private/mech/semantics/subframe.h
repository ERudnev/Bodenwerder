#pragma once

#include "shapes.h"
#include "space.h"

#include <map>
#include <string_view>
#include <vector>

// Skeleton inventory + smoke recipes (types: model/eltanin/mech/semantics.q1.h).
// 1) cornerSpecs / halfribSpecs / membraneSpecs — interframe mesh inventory (asset tag, …). No per-cell placement.
// 2) shapes.h frame::corners — which cell vertices exist on each k* (topology). Not repeated here.
// 3) recipes[shape] — smoke assembly only: which mesh kind at which cell vertex/edge + orient roll.
// Edge row: cellAt0/cellAtRay = cell lattice after orient (cornerIndex(orient, mesh slot 0 / ray)); cellAt0 < cellAtRay.
// poleAtMesh0 = halfrib pole (starts/ends) at authored mesh slot 0. Mesh ray slot comes from halfribSpecs[kind].ray — not stored per row.

namespace eltanin::mech::skeleton {

    struct CornerSpec {
        struct Ray {
            cube::Corner target;
            Bend bend;
        };
        std::string_view code;
        std::vector<Ray> rays;
    };

    inline const std::map<Corner::Kind, CornerSpec> cornerSpecs{
            {Corner::Kind::c124, {.code = "c124", .rays = {{.target = 1, .bend = Bend::deg90}, {.target = 2, .bend = Bend::deg90}, {.target = 4, .bend = Bend::deg90}}}},
            {Corner::Kind::c1364, {.code = "c1364", .rays = {{.target = 1, .bend = Bend::deg90}, {.target = 3, .bend = Bend::deg125}, {.target = 6, .bend = Bend::deg125}, {.target = 4, .bend = Bend::deg90}}}},
            {Corner::Kind::c164, {.code = "c164", .rays = {{.target = 1, .bend = Bend::deg45}, {.target = 6, .bend = Bend::deg90}, {.target = 4, .bend = Bend::deg90}}}},
            {Corner::Kind::c134, {.code = "c134", .rays = {{.target = 1, .bend = Bend::deg90}, {.target = 3, .bend = Bend::deg90}, {.target = 4, .bend = Bend::deg45}}}},
            {Corner::Kind::c135, {.code = "c135", .rays = {{.target = 1, .bend = Bend::deg90}, {.target = 3, .bend = Bend::deg71}, {.target = 5, .bend = Bend::deg71}}}},
            {Corner::Kind::c12, {.code = "c12", .rays = {{.target = 1, .bend = Bend::deg90}, {.target = 2, .bend = Bend::deg90}}}},
            {Corner::Kind::c13, {.code = "c13", .rays = {{.target = 1, .bend = Bend::deg90}, {.target = 3, .bend = Bend::deg90}}}},
            {Corner::Kind::c15, {.code = "c15", .rays = {{.target = 1, .bend = Bend::deg90}, {.target = 5, .bend = Bend::deg90}}}},
            {Corner::Kind::c16, {.code = "c16", .rays = {{.target = 1, .bend = Bend::deg45}, {.target = 6, .bend = Bend::deg90}}}},
            {Corner::Kind::c34, {.code = "c34", .rays = {{.target = 3, .bend = Bend::deg90}, {.target = 4, .bend = Bend::deg45}}}},
            {Corner::Kind::c35, {.code = "c35", .rays = {{.target = 3, .bend = Bend::deg71}, {.target = 5, .bend = Bend::deg71}}}},
        };

    inline auto opposite(Halfrib::Pole pole) -> Halfrib::Pole {
        return pole == Halfrib::Pole::starts ? Halfrib::Pole::ends : Halfrib::Pole::starts;
    }

    struct HalfribSpec {
        std::string_view code;
        int ray;
        Bend bend;
    };

    inline const std::map<Halfrib::Kind, HalfribSpec> halfribSpecs{
        {Halfrib::Kind::he1deg90, {.code = "he1deg90", .ray = 1, .bend = Bend::deg90}},
        {Halfrib::Kind::he1deg45, {.code = "he1deg45", .ray = 1, .bend = Bend::deg45}},
        {Halfrib::Kind::he3deg71, {.code = "he3deg71", .ray = 3, .bend = Bend::deg71}},
        {Halfrib::Kind::he3deg90, {.code = "he3deg90", .ray = 3, .bend = Bend::deg90}},
        {Halfrib::Kind::he3deg125, {.code = "he3deg125", .ray = 3, .bend = Bend::deg125}},
    };

    struct MembraneSpec {
        std::string_view code;
    };

    inline const std::map<Membrane::Kind, MembraneSpec> membraneSpecs{
        {Membrane::Kind::u1111, {.code = "u1111"}},
        {Membrane::Kind::u121, {.code = "u121"}},
        {Membrane::Kind::u2121, {.code = "u2121"}},
        {Membrane::Kind::u222A, {.code = "u222A"}},
        {Membrane::Kind::u222V, {.code = "u222V"}},
    };

    inline auto membraneKindOf(plate::shape shape) -> Membrane::Kind {
        switch (shape) {
            case plate::shape::p1111: return Membrane::Kind::u1111;
            case plate::shape::p121: return Membrane::Kind::u121;
            case plate::shape::p2121: return Membrane::Kind::u2121;
            case plate::shape::p222A: return Membrane::Kind::u222A;
            case plate::shape::p222V: return Membrane::Kind::u222V;
        }
        return Membrane::Kind::u1111;
    }

    struct Recipe {
        struct AtCorner {
            Corner::Kind kind;
            cube::Corner cellVertex;
            space::orient::key orient;
        };
        struct AtEdge {
            Halfrib::Kind kind;
            cube::Corner cellAt0;
            cube::Corner cellAtRay;
            space::orient::key orient;
            Halfrib::Pole poleAtMesh0;
        };
        std::vector<AtCorner> corners;
        std::vector<AtEdge> edges;
    };

    inline const std::map<frame::shape, Recipe> recipes{
        {frame::shape::k8, {
            .corners = {
                {.kind = Corner::Kind::c124, .cellVertex = 0, .orient = 0},
                {.kind = Corner::Kind::c124, .cellVertex = 1, .orient = 4},
                {.kind = Corner::Kind::c124, .cellVertex = 2, .orient = 3},
                {.kind = Corner::Kind::c124, .cellVertex = 3, .orient = 7},
                {.kind = Corner::Kind::c124, .cellVertex = 4, .orient = 1},
                {.kind = Corner::Kind::c124, .cellVertex = 5, .orient = 5},
                {.kind = Corner::Kind::c124, .cellVertex = 6, .orient = 2},
                {.kind = Corner::Kind::c124, .cellVertex = 7, .orient = 6},
            },
            .edges = {
                {.kind = Halfrib::Kind::he1deg90, .cellAt0 = 0, .cellAtRay = 1, .orient = 0, .poleAtMesh0 = Halfrib::Pole::starts},
                {.kind = Halfrib::Kind::he1deg90, .cellAt0 = 0, .cellAtRay = 2, .orient = 15, .poleAtMesh0 = Halfrib::Pole::starts},
                {.kind = Halfrib::Kind::he1deg90, .cellAt0 = 0, .cellAtRay = 4, .orient = 16, .poleAtMesh0 = Halfrib::Pole::starts},
                {.kind = Halfrib::Kind::he1deg90, .cellAt0 = 1, .cellAtRay = 3, .orient = 21, .poleAtMesh0 = Halfrib::Pole::starts},
                {.kind = Halfrib::Kind::he1deg90, .cellAt0 = 1, .cellAtRay = 5, .orient = 4, .poleAtMesh0 = Halfrib::Pole::starts},
                {.kind = Halfrib::Kind::he1deg90, .cellAt0 = 2, .cellAtRay = 3, .orient = 3, .poleAtMesh0 = Halfrib::Pole::starts},
                {.kind = Halfrib::Kind::he1deg90, .cellAt0 = 2, .cellAtRay = 6, .orient = 14, .poleAtMesh0 = Halfrib::Pole::starts},
                {.kind = Halfrib::Kind::he1deg90, .cellAt0 = 3, .cellAtRay = 7, .orient = 20, .poleAtMesh0 = Halfrib::Pole::starts},
                {.kind = Halfrib::Kind::he1deg90, .cellAt0 = 4, .cellAtRay = 5, .orient = 1, .poleAtMesh0 = Halfrib::Pole::starts},
                {.kind = Halfrib::Kind::he1deg90, .cellAt0 = 4, .cellAtRay = 6, .orient = 17, .poleAtMesh0 = Halfrib::Pole::starts},
                {.kind = Halfrib::Kind::he1deg90, .cellAt0 = 5, .cellAtRay = 7, .orient = 5, .poleAtMesh0 = Halfrib::Pole::starts},
                {.kind = Halfrib::Kind::he1deg90, .cellAt0 = 6, .cellAtRay = 7, .orient = 2, .poleAtMesh0 = Halfrib::Pole::starts},
            },
        }},
        {frame::shape::k7, {
            .corners = {
                {.kind = Corner::Kind::c1364, .cellVertex = 0, .orient = 0},
                {.kind = Corner::Kind::c124, .cellVertex = 1, .orient = 4},
                {.kind = Corner::Kind::c1364, .cellVertex = 3, .orient = 20},
                {.kind = Corner::Kind::c124, .cellVertex = 4, .orient = 1},
                {.kind = Corner::Kind::c124, .cellVertex = 5, .orient = 5},
                {.kind = Corner::Kind::c1364, .cellVertex = 6, .orient = 13},
                {.kind = Corner::Kind::c124, .cellVertex = 7, .orient = 6},
            },
            .edges = {
                {.kind = Halfrib::Kind::he1deg90, .cellAt0 = 0, .cellAtRay = 1, .orient = 0, .poleAtMesh0 = Halfrib::Pole::starts},
                {.kind = Halfrib::Kind::he3deg125, .cellAt0 = 0, .cellAtRay = 3, .orient = 0, .poleAtMesh0 = Halfrib::Pole::starts},
                {.kind = Halfrib::Kind::he1deg90, .cellAt0 = 0, .cellAtRay = 4, .orient = 16, .poleAtMesh0 = Halfrib::Pole::starts},
                {.kind = Halfrib::Kind::he3deg125, .cellAt0 = 0, .cellAtRay = 6, .orient = 13, .poleAtMesh0 = Halfrib::Pole::ends},
                {.kind = Halfrib::Kind::he1deg90, .cellAt0 = 1, .cellAtRay = 3, .orient = 21, .poleAtMesh0 = Halfrib::Pole::starts},
                {.kind = Halfrib::Kind::he1deg90, .cellAt0 = 1, .cellAtRay = 5, .orient = 4, .poleAtMesh0 = Halfrib::Pole::starts},
                {.kind = Halfrib::Kind::he3deg125, .cellAt0 = 3, .cellAtRay = 6, .orient = 20, .poleAtMesh0 = Halfrib::Pole::starts},
                {.kind = Halfrib::Kind::he1deg90, .cellAt0 = 3, .cellAtRay = 7, .orient = 20, .poleAtMesh0 = Halfrib::Pole::starts},
                {.kind = Halfrib::Kind::he1deg90, .cellAt0 = 4, .cellAtRay = 5, .orient = 1, .poleAtMesh0 = Halfrib::Pole::starts},
                {.kind = Halfrib::Kind::he1deg90, .cellAt0 = 4, .cellAtRay = 6, .orient = 17, .poleAtMesh0 = Halfrib::Pole::starts},
                {.kind = Halfrib::Kind::he1deg90, .cellAt0 = 5, .cellAtRay = 7, .orient = 5, .poleAtMesh0 = Halfrib::Pole::starts},
                {.kind = Halfrib::Kind::he1deg90, .cellAt0 = 6, .cellAtRay = 7, .orient = 2, .poleAtMesh0 = Halfrib::Pole::starts},
            },
        }},
        {frame::shape::k6, {
            .corners = {
                {.kind = Corner::Kind::c164, .cellVertex = 0, .orient = 0},
                {.kind = Corner::Kind::c134, .cellVertex = 1, .orient = 4},
                {.kind = Corner::Kind::c124, .cellVertex = 4, .orient = 1},
                {.kind = Corner::Kind::c124, .cellVertex = 5, .orient = 5},
                {.kind = Corner::Kind::c134, .cellVertex = 6, .orient = 13},
                {.kind = Corner::Kind::c164, .cellVertex = 7, .orient = 9},
            },
            .edges = {
                {.kind = Halfrib::Kind::he1deg45, .cellAt0 = 0, .cellAtRay = 1, .orient = 0, .poleAtMesh0 = Halfrib::Pole::starts},
                {.kind = Halfrib::Kind::he1deg90, .cellAt0 = 0, .cellAtRay = 4, .orient = 16, .poleAtMesh0 = Halfrib::Pole::starts},
                {.kind = Halfrib::Kind::he3deg90, .cellAt0 = 0, .cellAtRay = 6, .orient = 13, .poleAtMesh0 = Halfrib::Pole::starts},
                {.kind = Halfrib::Kind::he1deg90, .cellAt0 = 1, .cellAtRay = 5, .orient = 4, .poleAtMesh0 = Halfrib::Pole::starts},
                {.kind = Halfrib::Kind::he3deg90, .cellAt0 = 1, .cellAtRay = 7, .orient = 4, .poleAtMesh0 = Halfrib::Pole::starts},
                {.kind = Halfrib::Kind::he1deg90, .cellAt0 = 4, .cellAtRay = 5, .orient = 1, .poleAtMesh0 = Halfrib::Pole::starts},
                {.kind = Halfrib::Kind::he1deg90, .cellAt0 = 4, .cellAtRay = 6, .orient = 17, .poleAtMesh0 = Halfrib::Pole::starts},
                {.kind = Halfrib::Kind::he1deg90, .cellAt0 = 5, .cellAtRay = 7, .orient = 5, .poleAtMesh0 = Halfrib::Pole::starts},
                {.kind = Halfrib::Kind::he1deg45, .cellAt0 = 6, .cellAtRay = 7, .orient = 9, .poleAtMesh0 = Halfrib::Pole::ends},
            },
        }},
        {frame::shape::k4, {
            .corners = {
                {.kind = Corner::Kind::c135, .cellVertex = 1, .orient = 4},
                {.kind = Corner::Kind::c135, .cellVertex = 4, .orient = 1},
                {.kind = Corner::Kind::c124, .cellVertex = 5, .orient = 5},
                {.kind = Corner::Kind::c135, .cellVertex = 7, .orient = 23},
            },
            .edges = {
                {.kind = Halfrib::Kind::he3deg71, .cellAt0 = 1, .cellAtRay = 4, .orient = 1, .poleAtMesh0 = Halfrib::Pole::ends},
                {.kind = Halfrib::Kind::he1deg90, .cellAt0 = 1, .cellAtRay = 5, .orient = 4, .poleAtMesh0 = Halfrib::Pole::starts},
                {.kind = Halfrib::Kind::he3deg71, .cellAt0 = 1, .cellAtRay = 7, .orient = 4, .poleAtMesh0 = Halfrib::Pole::starts},
                {.kind = Halfrib::Kind::he1deg90, .cellAt0 = 4, .cellAtRay = 5, .orient = 1, .poleAtMesh0 = Halfrib::Pole::starts},
                {.kind = Halfrib::Kind::he3deg71, .cellAt0 = 4, .cellAtRay = 7, .orient = 23, .poleAtMesh0 = Halfrib::Pole::ends},
                {.kind = Halfrib::Kind::he1deg90, .cellAt0 = 5, .cellAtRay = 7, .orient = 5, .poleAtMesh0 = Halfrib::Pole::starts},
            },
        }},
        {frame::shape::k4f1111, {
            .corners = {
                {.kind = Corner::Kind::c12, .cellVertex = 0, .orient = 0},
                {.kind = Corner::Kind::c12, .cellVertex = 1, .orient = 21},
                {.kind = Corner::Kind::c12, .cellVertex = 2, .orient = 19},
                {.kind = Corner::Kind::c12, .cellVertex = 3, .orient = 10},
            },
            .edges = {
                {.kind = Halfrib::Kind::he1deg90, .cellAt0 = 0, .cellAtRay = 1, .orient = 0, .poleAtMesh0 = Halfrib::Pole::starts},
                {.kind = Halfrib::Kind::he1deg90, .cellAt0 = 0, .cellAtRay = 2, .orient = 15, .poleAtMesh0 = Halfrib::Pole::starts},
                {.kind = Halfrib::Kind::he1deg90, .cellAt0 = 1, .cellAtRay = 3, .orient = 21, .poleAtMesh0 = Halfrib::Pole::starts},
                {.kind = Halfrib::Kind::he1deg90, .cellAt0 = 2, .cellAtRay = 3, .orient = 3, .poleAtMesh0 = Halfrib::Pole::starts},
            },
        }},
        {frame::shape::k3f121, {
            .corners = {
                {.kind = Corner::Kind::c12, .cellVertex = 0, .orient = 0},
                {.kind = Corner::Kind::c15, .cellVertex = 1, .orient = 11},
                {.kind = Corner::Kind::c13, .cellVertex = 2, .orient = 19},
            },
            .edges = {
                {.kind = Halfrib::Kind::he1deg90, .cellAt0 = 0, .cellAtRay = 1, .orient = 0, .poleAtMesh0 = Halfrib::Pole::starts},
                {.kind = Halfrib::Kind::he1deg90, .cellAt0 = 0, .cellAtRay = 2, .orient = 15, .poleAtMesh0 = Halfrib::Pole::starts},
                {.kind = Halfrib::Kind::he3deg90, .cellAt0 = 1, .cellAtRay = 2, .orient = 19, .poleAtMesh0 = Halfrib::Pole::ends},
            },
        }},
        {frame::shape::k4f2121, {
            .corners = {
                {.kind = Corner::Kind::c164, .cellVertex = 0, .orient = 0},
                {.kind = Corner::Kind::c134, .cellVertex = 1, .orient = 4},
                {.kind = Corner::Kind::c134, .cellVertex = 6, .orient = 13},
                {.kind = Corner::Kind::c164, .cellVertex = 7, .orient = 9},
            },
            .edges = {
                {.kind = Halfrib::Kind::he1deg45, .cellAt0 = 0, .cellAtRay = 1, .orient = 0, .poleAtMesh0 = Halfrib::Pole::starts},
                {.kind = Halfrib::Kind::he3deg90, .cellAt0 = 0, .cellAtRay = 6, .orient = 13, .poleAtMesh0 = Halfrib::Pole::ends},
                {.kind = Halfrib::Kind::he3deg90, .cellAt0 = 1, .cellAtRay = 7, .orient = 4, .poleAtMesh0 = Halfrib::Pole::starts},
                {.kind = Halfrib::Kind::he1deg45, .cellAt0 = 6, .cellAtRay = 7, .orient = 9, .poleAtMesh0 = Halfrib::Pole::ends},
            },
        }},
        {frame::shape::k3f222, {
            .corners = {
                {.kind = Corner::Kind::c135, .cellVertex = 1, .orient = 4},
                {.kind = Corner::Kind::c135, .cellVertex = 4, .orient = 1},
                {.kind = Corner::Kind::c135, .cellVertex = 7, .orient = 23},
            },
            .edges = {
                {.kind = Halfrib::Kind::he3deg71, .cellAt0 = 1, .cellAtRay = 4, .orient = 1, .poleAtMesh0 = Halfrib::Pole::ends},
                {.kind = Halfrib::Kind::he3deg71, .cellAt0 = 1, .cellAtRay = 7, .orient = 4, .poleAtMesh0 = Halfrib::Pole::starts},
                {.kind = Halfrib::Kind::he3deg71, .cellAt0 = 4, .cellAtRay = 7, .orient = 23, .poleAtMesh0 = Halfrib::Pole::ends},
            },
        }},
    };

} // namespace eltanin::mech::skeleton
