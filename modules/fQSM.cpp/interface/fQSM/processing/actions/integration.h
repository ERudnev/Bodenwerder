#pragma once

#include <fQSM/state/patch.h>
#include <fQSM/state/world.h>

// facade
namespace fqsm::processing::actions {
    void integrate(state::world::Data&, const state::world::Patch&);
}

// implementation
namespace fqsm::processing::actions::details {

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
}