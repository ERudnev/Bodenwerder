#pragma once

#include <functional>
#include <memory>
#include <base/logging.h>
#include <fQSM/model/_forwards.h>
#include <fQSM/model/complex/state.h>
#include <fQSM/model/complex/draft.h>
#include <fQSM/processing/_forwards.h>

namespace fqsm::processing {

    // Abstraction between "own" Draft and "someones"
    struct Context final {
        using Ptr = std::shared_ptr<Context>;
        using PatchRef = ref<model::complex::Patch>;
        using Result = cref<model::complex::Patch>;
        using Upstream = std::function<void(Result)>;

        const model::complex::State& view;
        PatchRef patch;
        Upstream upstream;

        ~Context() { finish(); } // _DEBUG_REPORT_;

        void finish() {
            if (upstream) {
                upstream(patch);
            }
        }
    };

    // Clarify role of this dinosaur...
    struct GateOperational {
        const model::complex::State& view;
        Context::Ptr parent;

        operator Reading() const { return view; }

        Context::Result result() const { return freeze(parent->patch); }
    };

    struct GateImmediate {
        const State& placeholder;
        //WorldAddressable& state;
        Context::Ptr parent;

        operator Reading() const { return placeholder; }
        //Patch& patch() const { return *parent->patch; }

        // optimization stuff: looks as oxymoron (broad updates do not resize container?)
        //template<aspect::Any Meta>
        //void reserve_broad_update() const;
    };
}