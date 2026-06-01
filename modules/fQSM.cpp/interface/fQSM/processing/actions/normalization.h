#pragma once

#include <fQSM/state/patch.h>
#include <fQSM/state/world.h>

namespace fqsm::processing::actions {

    void update(state::world::Data&, const state::world::Patch&);
}