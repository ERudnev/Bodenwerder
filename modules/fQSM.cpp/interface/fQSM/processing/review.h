#pragma once

#include <vector>
#include <string>
#include <fQSM/processing/commit.h>
#include <fQSM/state/world/draft.h>
#include <fQSM/state/slice/delta.h>

namespace fqsm::processing {

    struct Review final {
        using PatchRef = Commit::PatchRef;
        struct Notes {
            using Category = std::vector<std::string>;
            Category critical;
            Category warning;

            bool rejection() const { return not critical.empty(); }
        };

        state::world::Draft draft;
        PatchRef corrections;
        Notes& notes;

        // TODO: rename will_be -> something like "planed_updates"?
        template<aspect::Any Meta>
        auto changes() const -> state::slice::Delta<Meta> {
            return draft.template delta<Meta>();
        }

        operator GateWrite() const {
            return GateWrite{ draft, std::make_shared<Commit>(Commit{draft, corrections,{}}) };
        }
    };

}