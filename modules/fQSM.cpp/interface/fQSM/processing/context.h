#pragma once

#include <memory>
#include <base/logging.h>
#include <fQSM/state/_forwards.h>
#include <fQSM/processing/_forwards.h>
#include <fQSM/processing/permit.h> // may be forwarded if needed (move 

namespace fqsm::processing {

    // lightweigth thing, intended to be copied between transactions
    // this class and all objects of this class ust be completely hidden inside of Transactions and transferring Permits
    struct Context final {
        ~Context() {
            _DEBUG_REPORT_;
            finish();
        }
        //using Submission = std::move_only_function<void(state::world::Patch)>;
        using PatchRef = fqsm::ref<Patch>;
        using Submit = std::function<void(PatchRef)>;

        const View& view;
        PatchRef patch;
        std::optional<Submit> submit; // may be empty for immediate transactions

        bool active() { return submit.has_value(); }

        void finish() {
            if (submit) (*submit)(patch);
            submit = std::nullopt;
        }
    };
}