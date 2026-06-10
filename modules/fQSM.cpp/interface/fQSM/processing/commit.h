#pragma once

#include <functional>
#include <memory>
#include <base/logging.h>
#include <fQSM/state/_forwards.h>
#include <fQSM/state/patch.h>
#include <fQSM/state/world/view.h>
#include <fQSM/processing/_forwards.h>

namespace fqsm::processing {

    struct Commit final {
        using PatchRef = fqsm::ref<Patch>;
        using Upstream = std::function<void(PatchRef)>;

        const View& view;
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

        const View& state;
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
        patch().template items<Meta>().reserve(state.items<Meta>().size());
    }
}