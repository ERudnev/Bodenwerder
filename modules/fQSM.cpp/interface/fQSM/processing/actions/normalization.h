#pragma once

#include <fQSM/state/_forwards.h>
#include <fQSM/processing/review.h>

namespace fqsm::processing::actions {

    auto update(state::world::Data&, const state::world::Patch&) -> Review::Notes;
}