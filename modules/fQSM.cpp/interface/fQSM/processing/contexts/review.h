#pragma once

#include <vector>
#include <string>
#include <fQSM/processing/contexts/operational.h>
#include <fQSM/model/complex/future.h>
#include <fQSM/model/linear/delta.h>


namespace fqsm::processing::review {
    struct Result {
        using Category = std::vector<std::string>;
        Category critical;
        Category warning;

        bool good() const { return critical.empty(); }
    };
}

namespace fqsm::processing {

    // TODO: consider to tighten type scope, make mini-schema's
    // and give different Reaction scoped Review, like [typeA, typeB, typeC]
    struct Review final {
        using Context = context::Operational;
        using PatchRef = Context::PatchRef;

        const model::complex::Future& proposal;
        review::Result& result;
        // TODO: (really Do): tighten Context -> WorkersInterface Zag-Zag
        Context::Ptr reactions;

        Review(const model::complex::Future& proposal, Context::PatchRef target, review::Result& result)
            : proposal(proposal)
            , result(result)
            , reactions(std::make_shared<Context>(proposal, target, Context::Upstream{}))
        {}
        // helpers:
        template<category::Any Meta>
        auto changes() const -> model::linear::Delta<Meta> {
            return proposal.delta<Meta>();
        }

        template<category::Any Meta>
        auto reaction() -> model::linear::Patch<Meta>& {
            return reactions->accumulator->aspect<Meta>();
        }

        operator Reading() const { return processing::View(proposal); }
        operator Gate() const { return processing::Gate(reactions); }
    };


}
