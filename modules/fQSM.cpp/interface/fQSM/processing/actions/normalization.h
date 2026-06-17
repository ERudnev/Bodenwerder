#pragma once

#include <fQSM/model/_forwards.h>
#include <fQSM/processing/review.h>

namespace fqsm::processing::actions {

    struct NormalizationResult {
        fqsm::ref<state::world::Patch> patch;
        Review::Notes notes;
    };

    auto normalize(Reading base, const state::world::Patch&) -> NormalizationResult;
    auto update(state::world::Data&, const state::world::Patch&) -> Review::Notes;
}