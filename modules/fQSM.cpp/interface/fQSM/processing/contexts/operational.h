#pragma once

#include <functional>
#include <memory>
#include <fQSM/references.h>
#include <fQSM/processing/_forwards.h>
#include <fQSM/model/_forwards.h>
#include <fQSM/model/complex/future.h>

namespace fqsm::processing::context {

    struct Operational final {
        using Ptr = std::shared_ptr<Operational>;
        using PatchRef = ref<model::complex::Patch>;
        using Upstream = std::function<void(PatchRef)>;
        using Future = model::complex::Future;

        PatchRef accumulator; // abstraction: ownership of Patch (local/remote)??
        const model::complex::Future world;
        Upstream callback;

        Operational(const State& initial, PatchRef patch, Upstream);
        ~Operational() { collapse(); }

    private:
        void collapse();
    };
}

namespace fqsm::processing {

    struct Gate {
        using Context = context::Operational;
        Gate(Context::Ptr parent) : context(std::move(parent)) {}

        operator View() const { return context->world; }
        const model::complex::State* operator->() const { return &context->world; }
        model::complex::Patch& patch() { return *context->accumulator; }
        //Context::PatchRef patch() { return context->accumulator; }
    private:
        // There is one fundamental problem around.
        // shared_ptr<Context> is quite ineffective for lightweinght Gate
        // but no other way to make transaction living
        // TODO: find better day to do this
        const Context::Ptr context;
    };
}