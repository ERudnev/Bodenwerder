#pragma once

#include <fQSM/processing/contexts/operational.h>

namespace fqsm::processing::context {
    struct Retrospective final {
        using Ptr = std::shared_ptr<Retrospective>;

        Operational::Ptr writer;
        const model::complex::State& base;

        Retrospective(const State& initial, Operational::Ptr writer);//, Upstream);
    };

}

namespace fqsm::processing {

    struct Wall {
        using Context = context::Retrospective;
        Wall(Context::Ptr parent) : context(std::move(parent)) {}

        operator View() const { return View(context->base); }
        const model::complex::State* operator->() const { return &context->base; }

        operator Gate() const { return Gate(context->writer); }

        model::complex::WorkersInterface& workers_interface() { return context->writer->future; }

    private:
        const Context::Ptr context;
    };
}