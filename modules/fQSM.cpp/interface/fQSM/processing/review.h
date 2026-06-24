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
        //Review(model::complex::Future draft, Context::PatchRef target, review::Notes& notes)
        //    : proposal(std::move(draft)), corrections(target), notes(notes), context(std::make_shared<Context>(Context{proposal, corrections, {}})) {}

        using PatchRef = Context::PatchRef;

        const model::complex::Future proposal;
        //PatchRef corrections;
        review::Notes& notes;
        Context::Ptr reactions;


        Review(model::complex::Future draft, Context::PatchRef target, review::Notes& notes)
            : proposal(std::move(draft))
            , notes(notes)
            , reactions(std::make_shared<Context>(proposal, target, base::cannonball::SeeChanges::blind, Context::Upstream{}))
        {}
            //, corrections(std::make_shared<Context>(proposal->world, std::make_shared<

        // helpers:
        template<category::Any Meta>
        auto changes() const -> model::linear::Delta<Meta> {
            return proposal.delta<Meta>();
        }

        operator Gate() const {
            // old version where Gate couldhaeve own View:
            //return Writing{ proposal, std::make_shared<Context>(Context{proposal, corrections,{}}) };
            return processing::Gate(reactions);
        }
    };


}