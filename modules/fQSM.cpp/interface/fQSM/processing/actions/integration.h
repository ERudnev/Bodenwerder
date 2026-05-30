#pragma once

#include <fQSM/state/patch.h>
#include <fQSM/state/world.h>

namespace fqsm::processing::actions {

    template<aspect::Any Meta>
    void integrate(state::world::Data& world, const state::world::Patch& patch) {
        auto& target = world.items<Meta>();

        for (const auto entry : patch.template items<Meta>()) {
            if (entry.second) {
                target.insert(entry.first, *entry.second);
            } else {
                target.erase(entry.first);
            }
        }
    }

    inline void integrate(state::world::Data& world, const state::world::Patch& patch) {
        for (const auto& [aspectId, node] : world.schema->nodes) {
            node.binding.integratePatchSlice(world, patch);
        }
    }
}