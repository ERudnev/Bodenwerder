#pragma once

#include <base/cannonball/patch.h>
#include <fQSM/model/_forwards.h>
#include <fQSM/meta/interface.include.h>

namespace fqsm::model::linear {

    template<category::Any Meta>
    struct Patch : patch::Erased {
        base::cannonball::Patch<Id<Meta>, Quantum<Meta>> items;
        std::optional<GlobalValue<Meta>> global; // nullopt means "no change"
    };
}
