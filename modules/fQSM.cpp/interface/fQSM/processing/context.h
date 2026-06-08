#pragma once

#include <functional>
#include <memory>
#include <base/logging.h>
#include <fQSM/state/_forwards.h>
#include <fQSM/processing/_forwards.h>

namespace fqsm::processing {

    struct Context final {
        using PatchRef = fqsm::ref<Patch>;
        using Upstream = std::function<void(PatchRef)>;

        const View& view;
        PatchRef patch; // always created outside
        Upstream upstream;

        ~Context() { finish(); } // _DEBUG_REPORT_;

        void finish() {
            if (upstream) {
                upstream(patch);
            }
        }
    };

    struct Gate {
        using PatchRef = Context::PatchRef;

        const View& view;
        ContextShared parent;

        operator Reading() const { return view; }

        Patch& patch() const { return *parent->patch; }
    };
}