#pragma once

#include <vector>
#include <string>
#include <fQSM/processing/context.h>
#include <fQSM/model/complex/draft.h>
#include <fQSM/model/linear/delta.h>

namespace fqsm::processing {

    struct Review final {
        using PatchRef = Context::PatchRef;
        struct Notes {
            using Category = std::vector<std::string>;
            Category critical;
            Category warning;

            bool rejection() const { return not critical.empty(); }
        };

        const model::complex::Draft proposal;
        PatchRef corrections;
        Notes& notes;

        template<aspect::Any Meta>
        auto changes() const -> model::linear::Delta<Meta> {
            return proposal.delta<Meta>();
        }

        operator Writing() const {
            return Writing{ proposal, std::make_shared<Context>(Context{proposal, corrections,{}}) };
        }
    };

}