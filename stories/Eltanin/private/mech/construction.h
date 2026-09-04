#pragma once

#include <eltanin/mech/construction.q1.h>
#include <eltanin/physics/rigid.q1.h>

namespace eltanin::mech {

    template<typename Fn>
    void forEachPrimitiveLoop(const Construction& construction, Fn&& fn) {
        for (const auto& [id, knot] : construction.knots)
            fn(id, knot);
        for (const auto& [id, rib] : construction.ribs)
            fn(id, rib);
        for (const auto& [id, membrane] : construction.membranes)
            fn(id, membrane);
        for (const auto& [id, plate] : construction.plates)
            fn(id, plate);
        for (const auto& [id, faces] : construction.volumes) {
            for (const auto& face : faces)
                fn(id, face);
        }
    }

    struct FrameIslands {
        vector<vector<Construction::Primitive::Id>> islands;
        vector<Construction::Primitive::Id> shedSkin;
    };

    void bindKnotWelds(Construction&);
    auto cookHull(const Construction&, const vector<vec3>& shape) -> phys::rigid::Hull;
    auto connectedIslands(const Construction&) -> FrameIslands;
    auto islandSpans3d(const Construction&, const vector<Construction::Primitive::Id>&) -> bool;
    auto islandIsConstruct(const Construction&, const vector<Construction::Primitive::Id>&) -> bool;

    inline void compileParticles(Construction& construction) {
        construction.evaluatedParticles.clear();
        forEachPrimitiveLoop(construction, [&](Construction::Primitive::Id, const Construction::Primitive& primitive) {
            for (const auto& welded : primitive.loop)
                construction.evaluatedParticles.push_back(Construction::Primitive::Point{.gridPos = welded.gridPos, .mass = welded.mass});
        });
        bindKnotWelds(construction);
    }

}
