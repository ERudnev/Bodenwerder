#pragma once

#include <functional>
#include <memory>
#include <base/logging.h>
#include <fQSM/meta/interface.include.h>
#include <fQSM/model/_forwards.h>
#include <fQSM/processing/_forwards.h>
#include <fQSM/references.h>
#include <fQSM/utility/logging.h>

// TODO:
// this part of library needs major restructurisation.
namespace fqsm::processing::context {

    // Abstraction between "own" Draft and "someones else draft"
    struct Operational final {
        using Ptr = std::shared_ptr<Operational>;
        // this may look not clear, but the truth is: Patch is mutable all ehe way through all systems
        using PatchRef = ref<model::complex::Patch>;
        using Upstream = std::function<void(PatchRef)>;

        const model::complex::State& view;
        PatchRef patch;
        Upstream upstream;

        ~Operational() { finish(); } // _DEBUG_REPORT_;

        void finish() {
            if (upstream) {
                utility::log_patch(fqsm::freeze(patch));
                upstream(patch);
            }
            // TODO: clarify this (not tested yet)
            upstream = nullptr;
        }
    };
}

namespace fqsm::processing {

    struct Gate {
        using Context = context::Operational;
        Gate(Context::Ptr parent) : context(std::move(parent)) {}

        operator Reading() const { return context->view; }
        const model::complex::State* operator->() const { return &context->view; }
        Context::PatchRef patch() { return context->patch; }

    private:
        // There is one fundamental problem around.
        // shared_ptr<Context> is quite ineffective for lightweinght Gate
        // but no other way to make transaction living as long as needed is visible
        // TODO: find better day to sovle this
        const Context::Ptr context;
    };
}
