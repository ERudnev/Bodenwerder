#pragma once

#include <functional>
#include <memory>
#include <base/logging.h>
#include <fQSM/state/_forwards.h>
#include <fQSM/state/world/patch.h>
#include <fQSM/state/world/actual.h>
#include <fQSM/processing/_forwards.h>

namespace fqsm::processing {

    struct Commit final {
        using PatchRef = fqsm::ref<Patch>;
        using Upstream = std::function<void(PatchRef)>;

        const World& state;
        PatchRef patch; // always created outside
        Upstream upstream;

        ~Commit() { finish(); } // _DEBUG_REPORT_;

        void finish() {
            if (upstream) {
                upstream(patch);
            }
        }
    };

    struct Gate {
        using PatchRef = Commit::PatchRef;

        const World& state;
        ContextShared parent;

        operator Reading() const { return state; }

        Patch& patch() const { return *parent->patch; }

        // optimization stuff:
        template<aspect::Any Meta>
        void reserve_broad_update() const;
    };
}

namespace fqsm::processing {
    template<aspect::Any Meta>
    void Gate::reserve_broad_update() const {
        patch().template slice<Meta>().reserve(state.slice<Meta>().size());
    }
}