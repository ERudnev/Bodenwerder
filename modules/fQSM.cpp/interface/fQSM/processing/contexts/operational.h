#pragma once

#include <functional>
#include <memory>
#include <fQSM/references.h>
#include <fQSM/processing/_forwards.h>
#include <fQSM/model/_forwards.h>
#include <fQSM/model/complex/future.h>
#include <fQSM/utility/poisoned.h>

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

    struct View {
        explicit View(const model::complex::State& world) : state(world) {}
        const model::complex::State* operator->() const { return &state; }
        const model::complex::State& operator*() const { return state; }
    private:
        const model::complex::State& state;
    };

    struct Gate {
        using Context = context::Operational;
        Gate(Context::Ptr parent) : context(std::move(parent)) {}

        operator View() const { return View(context->world); }
        const model::complex::State* operator->() const { return &context->world; }

        model::complex::WorkersInterface& workers_interface() { return *context->accumulator; }
        // helpers:
        utility::Poisoned refuse(std::string message) { context->accumulator->summary.critical.emplace_back(std::move(message)); return {}; }
        void warning(std::string message) {context->accumulator->summary.warning.emplace_back(std::move(message)); }

    private:
        // There is one fundamental problem around.
        // shared_ptr<Context> is quite ineffective for lightweinght Gate
        // but no other way to make transaction living
        // TODO: find better day to do this
        const Context::Ptr context;
    };
}