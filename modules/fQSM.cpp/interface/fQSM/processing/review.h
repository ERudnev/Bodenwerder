#pragma once

#include <vector>
#include <string>
#include <fQSM/processing/commit.h>
#include <fQSM/model/complex/future.h>
#include <fQSM/model/linear/delta.h>

namespace fqsm::processing {

    struct Review final {
        using PatchRef = Commit::PatchRef;
        struct Notes {
            using Category = std::vector<std::string>;
            Category critical;
            Category warning;

            bool rejection() const { return not critical.empty(); }
        };

        model::complex::Future expectation;
        PatchRef corrections;
        Notes& notes;

        template<aspect::Any Meta>
        auto changes() const -> model::linear::Delta<Meta> {
            return expectation.delta<Meta>();
        }

        operator GateWrite() const {
            return GateWrite{ expectation, std::make_shared<Commit>(Commit{expectation, corrections,{}}) };
        }
    };

}