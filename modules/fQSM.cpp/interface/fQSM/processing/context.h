#pragma once

#include <functional>
#include <memory>
#include <base/logging.h>
#include <fQSM/model/_forwards.h>
#include <fQSM/processing/_forwards.h>

namespace fqsm::processing {

    // TODO: rename to Context / context::Operational / OperationalContext or something like "TransitionalRAIIOperationalBuffer" :[
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

    // TODO: rename to GateOperational
    struct GateWrite {
        using PatchRef = Commit::PatchRef;

        const World& state;
        ContextShared parent;

        operator Reading() const { return state; }

        Patch& patch() const { return *parent->patch; }
    };

    struct GateImmediate {
        using PatchRef = Commit::PatchRef;

        model::WorldAddressable& state;
        ContextShared parent;

        operator Reading() const { return state; }

        Patch& patch() const { return *parent->patch; }

        // optimization stuff: looks as oxymoron (broad updates do not resize container?)
        //template<aspect::Any Meta>
        //void reserve_broad_update() const;
    };
}