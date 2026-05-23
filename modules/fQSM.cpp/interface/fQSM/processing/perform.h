#pragma once

#include <fQSM/state/_forwards.h>

namespace fqsm::processing::perform {

    void integration_placeholder(state::world::Data& target, const state::world::Patch& patch) { _INCOMPLETE_; }
    void normalization_placeholder(const state::world::View& before, const state::world::Patch& patch) { _INCOMPLETE_; return {}; }
    
}