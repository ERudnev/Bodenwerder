#pragma once

#include <eltanin/mech/construction.q1.h>
#include <eltanin/physics/rigid.q1.h>

#include <algorithm>

namespace eltanin::mech {

    template<typename Fn>
    void forEachPrimitiveLoop(const Construction& construction, Fn&& fn) {
        auto idsOf = [](const auto& items) {
            vector<Construction::Primitive::Id> ids;
            ids.reserve(items.size());
            for (const auto& [id, _] : items)
                ids.push_back(id);
            std::sort(ids.begin(), ids.end());
            return ids;
        };
        for (const auto id : idsOf(construction.knots))
            fn(id, construction.knots.at(id));
        for (const auto id : idsOf(construction.ribs))
            fn(id, construction.ribs.at(id));
        for (const auto id : idsOf(construction.membranes))
            fn(id, construction.membranes.at(id));
        for (const auto id : idsOf(construction.plates))
            fn(id, construction.plates.at(id));
        for (const auto id : idsOf(construction.volumes)) {
            for (const auto& face : construction.volumes.at(id))
                fn(id, face);
        }
    }

    void bindKnotWelds(Construction&);
    auto cookHull(const Construction&, const vector<vec3>& shape) -> phys::rigid::Hull;
    auto connectedIslands(const Construction&) -> vector<vector<Construction::Primitive::Id>>;
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
