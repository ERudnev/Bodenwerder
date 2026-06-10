#pragma once

#include <vector>
#include <string>
#include <fQSM/processing/commit.h>
#include <fQSM/state/world/preview.h>

namespace fqsm::processing {

    struct Review final {
        using PatchRef = Commit::PatchRef;
        struct Notes {
            using Category = std::vector<std::string>;
            Category critical;
            Category warning;

            bool rejection() const { return not critical.empty(); }
        };

        state::world::Preview preview;
        PatchRef patch;
        Notes& notes;

        template<aspect::Any Meta>
        auto will_be() const -> state::slice::Delta<Meta> {
            return preview.template delta<Meta>();
        }

        operator Gate() const {
            return Gate{ preview, std::make_shared<Commit>(Commit{preview, patch,{}}) };
        }
    };

}