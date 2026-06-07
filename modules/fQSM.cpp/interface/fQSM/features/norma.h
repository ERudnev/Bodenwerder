#pragma once

#include <vector>

#include <fQSM/state/delta.h>
#include <fQSM/state/_forwards.h>
#include <fQSM/processing/review.h>

namespace fqsm::features {

    struct Norma {
        using Filter = std::vector<state::item::ChangeType>;

        using Reviewing = processing::Review;
        using Preview = state::world::Preview;
        using Patch = state::world::Patch;

        virtual ~Norma() = default;

        virtual void apply(Reviewing) = 0;
    };
}
