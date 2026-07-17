#pragma once

#include <vector>
#include <string>
#include <fQSM/processing/contexts/operational.h>
#include <fQSM/processing/contexts/retrospective.h>
#include <fQSM/model/complex/future.h>
#include <fQSM/model/linear/delta.h>
#include <fQSM/utility/bad_value.h>

namespace fqsm::processing {

    // TODO: consider to tighten type scope, make mini-schema's
    // and give different Reaction scoped Review, like [typeA, typeB, typeC]
    struct Review final {
        using Context = context::Operational;
        using RetrospectiveContext = context::Retrospective;
        using PatchRef = Context::PatchRef;

        const model::complex::Future& proposal;
        // TODO: (really Do): tighten Context -> WorkersInterface Zag-Zag
        Context::Ptr reactions;
        RetrospectiveContext::Ptr retrospective;

        Review(const model::complex::Future& proposal, const model::complex::State& origin, Context::PatchRef target)
            : proposal(proposal)
            , origin(origin)
            , reactions(std::make_shared<Context>(proposal, target, Context::Upstream{}))
            , retrospective(std::make_shared<RetrospectiveContext>(origin, target, RetrospectiveContext::Upstream{}))
        {}
        // helpers:
        utility::BadValue refuse(std::string message) { reactions->accumulator->summary.critical.emplace_back(std::move(message)); return {};}
        void warning(std::string message) {reactions->accumulator->summary.warning.emplace_back(std::move(message)); }

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

    private:
        const model::complex::State& origin;
    };


}
