#pragma once

#include <vector>

#include <fQSM/state/delta.h>
#include <fQSM/state/_forwards.h>
#include <fQSM/processing/review.h>

namespace fqsm::features {

    // TODO: rename to "Reaction"
    struct Norma {
        using Reviewing = processing::Review;
        using Preview = state::world::Preview;
        using Patch = state::world::Patch;
        using Sources = meta::aspect::TypeSet;

        virtual ~Norma() = default;

        virtual void apply(Reviewing) = 0;
        virtual Sources listens() const = 0;

    protected:
        // derived class helper:
        template<aspect::Any... Metas>
        static Sources typed_set() {
            return Sources{ meta::aspect::Rtid::of<Metas>()... };
        }
    };
}
