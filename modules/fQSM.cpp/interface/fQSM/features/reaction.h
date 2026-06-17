#pragma once

#include <vector>

#include <fQSM/model/_forwards.h>
#include <fQSM/processing/review.h>

namespace fqsm::manipulation {}

namespace fqsm::features::reactions {
    namespace ask = ::fqsm::manipulation;
}

namespace fqsm::features {

    // Norma is a special kind of Reaction; Codex collects morms specifically.
    struct Reaction {
        using Reviewing = processing::Review;
        using Draft = state::world::Draft;
        using Patch = state::world::Patch;
        using Sources = meta::aspect::TypeSet;

        virtual ~Reaction() = default;

        virtual void apply(Reviewing) = 0;
        virtual Sources listens() const = 0;

    protected:
        // derived class helper:
        template<aspect::Any... Metas>
        static Sources typed_set() {
            return Sources{ meta::aspect::Rtid::of<Metas>()... };
        }

        template<aspect::Any Meta>
        static auto will_be(const Reviewing& context) -> state::slice::Delta<Meta> {
            return context.template will_be<Meta>();
        }
    };
}
