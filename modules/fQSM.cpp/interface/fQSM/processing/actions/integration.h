#pragma once

#include <fQSM/state/world/actual.h>
#include <fQSM/state/patch.h>

// facade
namespace fqsm::processing::actions {
    void integrate(state::world::Actual&, const state::world::Patch&);
}

// implementation
namespace fqsm::processing::actions::details {

    template<aspect::Any Meta>
    void integrate(state::world::Data& world, const state::world::Patch& patch) {
        if (patch.template global<Meta>().has_value()) {
            world.template global<Meta>() = patch.template global<Meta>().value();
        }

        auto& target = world.slice<Meta>()->items();

        for (const auto entry : patch.template items<Meta>()) {
            if (entry.second) {
                target.insert(entry.first, *entry.second);
            } else {
                target.erase(entry.first);
            }
        }
    }
}