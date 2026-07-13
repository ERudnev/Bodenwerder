#pragma once

#include <fQSM/processing/contexts/operational.h>

namespace fqsm::processing::context {
    struct Retrospective final {
        using Ptr = std::shared_ptr<Retrospective>;
        using PatchRef = ref<model::complex::Patch>;
        using Upstream = std::function<void(PatchRef)>;

        PatchRef accumulator; // abstraction: ownership of Patch (local/remote)??
        const model::complex::State& base;
        Upstream callback;

        Retrospective(const State& initial, PatchRef patch, Upstream);
        ~Retrospective() { collapse(); }

    private:
        void collapse();
    };

}

namespace fqsm::processing {

    struct Wall {
        using Context = context::Retrospective;
        Wall(Context::Ptr parent) : context(std::move(parent)) {}

        operator View() const { return View(context->base); }
        operator Gate() const {
            return Gate(std::make_shared<context::Operational>(
                context->base,
                context->accumulator,
                context::Operational::Upstream{}));
        }
        const model::complex::State* operator->() const { return &context->base; }

        model::complex::WorkersInterface& workers_interface() { return *context->accumulator; }

    private:
        const Context::Ptr context;
    };
}