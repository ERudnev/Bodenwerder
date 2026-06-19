#pragma once

#include <functional>
#include <memory>
#include <base/logging.h>
#include <fQSM/model/_forwards.h>
#include <fQSM/model/complex/state.h>
#include <fQSM/processing/_forwards.h>

namespace fqsm::processing {

    // TODO: rename to Context / context::Operational / OperationalContext or something like "TransitionalRAIIOperationalBuffer" :[
    struct Context final {
        using Ptr = std::shared_ptr<Context>;
        using PatchRef = fqsm::ref<Patch>;
        using Upstream = std::function<void(PatchRef)>;

        const State& state;
        PatchRef patch; // always created outside
        Upstream upstream;

        ~Context() { finish(); } // _DEBUG_REPORT_;

        void finish() {
            if (upstream) {
                upstream(patch);
            }
        }
    };

    // TODO: rename to GateOperational
    struct GateWrite {
        using PatchRef = Context::PatchRef;

        const State& state; // TODO: consider removal of "const"
        Context::Ptr parent;

        operator Reading() const { return state; }

        Patch& patch() const { return *parent->patch; }
    };

    struct GateImmediate {
        using PatchRef = Context::PatchRef;

        const State& placeholder;
        //WorldAddressable& state;
        Context::Ptr parent;

        operator Reading() const { return placeholder; }

        Patch& patch() const { return *parent->patch; }

        // optimization stuff: looks as oxymoron (broad updates do not resize container?)
        //template<aspect::Any Meta>
        //void reserve_broad_update() const;
    };
}