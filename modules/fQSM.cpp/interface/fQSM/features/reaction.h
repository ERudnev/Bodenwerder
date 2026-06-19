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
        using Draft = model::complex::Draft;
        using Patch = model::complex::Patch;
        using Sources = meta::aspect::Rtid::Set;

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
        static auto changes(const Reviewing& context) -> model::linear::Delta<Meta> {
            return context.template changes<Meta>();
        }
    };
}
