#pragma once

#include <base/cannonball/patch.h>
#include <fQSM/model/_forwards.h>
#include <fQSM/meta/interface.include.h>

namespace fqsm::model::linear {

    template<aspect::Any Meta>
    struct Patch : patch::Erased {
        base::cannonball::Patch<Id<Meta>, Quantum<Meta>> items;
        base::cannonball::Patchlet<GlobalValue<Meta>> global;
    };
}
