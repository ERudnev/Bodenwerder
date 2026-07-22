#pragma once

#include <functional>
#include <memory>

#include <base/cannonball/table.h>
#include <fQSM/model/_forwards.h>
#include <fQSM/model/complex/reality.h>
#include <fQSM/processing/_forwards.h>
#include <fQSM/processing/contexts/operational.h>
#include <fQSM/utility/bad_value.h>

// TODO:
// this part of library needs major restructurisation.
namespace fqsm::processing::context {

    // direct context is "complex" (heteroneous full scope of all Aspects, while Breach is narrow
    struct Synchronous final {
        using Ptr = std::shared_ptr<Synchronous>;
        using PatchRef = ref<model::complex::Patch>;
        using Upstream = std::function<void(PatchRef, Rtid::Set directlyMutated)>;
        using Future = model::complex::Future;

        // data
        model::complex::Reality& reality;
        Rtid::Set dirty;
        Upstream callback;
        std::shared_ptr<context::Operational> subcontext;

        // may look too complex (simplify?): stops inner Cnotext, provoking immediate job delivery,intercepts it, extends and returns as own job
        ~Synchronous() { finish(); }

        Synchronous(model::complex::Reality& reality, PatchRef patch, Upstream upstream)
            : reality(reality)
            , dirty{}
            , callback(upstream)
            , subcontext(std::make_shared<context::Operational>(
                reality,
                patch,
                [this](PatchRef immediate){
                    acceptWorkers(immediate);
                }
            )){}

        void finish() { subcontext.reset(); }

    private:

        void acceptWorkers(PatchRef immediate) {
            if (callback)
                callback(immediate, std::move(dirty));
            callback = nullptr;
        }
    };

}

namespace fqsm::processing {

    template<category::Any Meta>
    struct Breach {
        using Context = context::Synchronous;
        using Container = base::cannonball::Table<Id<Meta>, Quantum<Meta>>;
        using Global = GlobalValue<Meta>;

        explicit Breach(Context::Ptr parent)
            : items(static_cast<Container&>(parent->reality.aspect<Meta>().items()))
            , global(parent->reality.aspect<Meta>().global())
            , context(std::move(parent))
        {
            context->dirty.insert(Rtid::of<Meta>());
        }

        Container& items;
        Global& global;

        operator View() const { return View(context->reality); }

    private:
        const Context::Ptr context;
    };

    // AREA: Val& (Breach) + workers (Gate) in one session; commit = Operational collapse → Synchronous Upstream(patch, dirty).
    // Sync only: Gate/Breach must not outlive this Dock / its Synchronous.
    struct Dock final {
        using Context = context::Synchronous;

        explicit Dock(Context::Ptr parent) : context(std::move(parent)) {}

        // session read = Reality ⊕ patch (same as Writing sees)
        operator View() const { return View(context->subcontext->world); }
        const model::complex::State* operator->() const { return &context->subcontext->world; }

        // same Operational whose collapse feeds acceptWorkers — not a fresh nested session
        operator Gate() const { return Gate(context->subcontext); }

        template<meta::category::Any Meta>
        operator Breach<Meta>() const { return Breach<Meta>(context); }

        // convenience; same surface as Gate (could also be Writing{*this} and drop these)
        model::complex::WorkersInterface& workers_interface() { return *context->subcontext->accumulator; }
        utility::BadValue refuse(std::string message) { context->subcontext->accumulator->summary.critical.emplace_back(std::move(message)); return {}; }
        void warning(std::string message) { context->subcontext->accumulator->summary.warning.emplace_back(std::move(message)); }

    private:
        const Context::Ptr context;
    };

}