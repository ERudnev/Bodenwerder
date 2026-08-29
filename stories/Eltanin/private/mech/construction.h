#pragma once

#include <eltanin/mech/construction.q1.h>

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

}
