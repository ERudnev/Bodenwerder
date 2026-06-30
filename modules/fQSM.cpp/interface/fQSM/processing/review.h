#pragma once

#include <vector>
#include <string>
#include <fQSM/processing/contexts/operational.h>
#include <fQSM/model/complex/future.h>
#include <fQSM/model/linear/delta.h>


namespace fqsm::processing::review {
    struct Notes {
        using Category = std::vector<std::string>;
        Category critical;
        Category warning;

        bool rejection() const { return not critical.empty(); }
    };
}

namespace fqsm::processing {

    struct Review final {
        using Context = context::Operational;
        using PatchRef = Context::PatchRef;

        const model::complex::Future& proposal;
        review::Notes& notes;
        Context::Ptr reactions;

        Review(const model::complex::Future& proposal, Context::PatchRef target, review::Notes& notes)
            : proposal(proposal)
            , notes(notes)
            , reactions(std::make_shared<Context>(proposal, target, base::cannonball::SeeChanges::observable, Context::Upstream{}))
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