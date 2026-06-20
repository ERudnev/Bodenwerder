#pragma once

#include <memory>
#include <base/cannonball/delta/direct.h>
#include <base/cannonball/delta/interface.h>
#include <base/cannonball/delta/operational.h>
#include <fQSM/meta/interface.include.h>
#include <fQSM/model/linear/patch.h>
#include <fQSM/model/linear/state.h>

namespace fqsm::model::linear {

    template<aspect::Any Meta>
    struct Delta {
        using Interface = base::cannonball::delta::Interface<Id<Meta>, Quantum<Meta>>;
        using Layer = Interface::Layer;
        enum class Mode {
            clean,
            dirty,
        };

        Delta(const State<Meta>& state, const Patch<Meta>& patch, Mode mode);

        // Funnt LLM artifact? const Interface& get() const { return *actual; }

        auto begin() const { return actual->begin(); }
        auto end() const { return actual->end(); }
        auto added() const { return actual->added(); }
        auto addedOrUpdated() const { return actual->addedOrUpdated(); }
        auto removed() const { return actual->removed(); }
        auto updated() const { return actual->updated(); }

    private:
        using Clean = base::cannonball::delta::Operational<Id<Meta>, Quantum<Meta>>;
        using Dirty = base::cannonball::delta::Direct<Id<Meta>, Quantum<Meta>>;
        const std::unique_ptr<const Interface> actual;
    };
}

namespace fqsm::model::linear {

    template<aspect::Any Meta>
    Delta<Meta>::Delta(const State<Meta>& state, const Patch<Meta>& patch, Mode mode)
        : actual(mode == Mode::clean
            ? std::unique_ptr<const Interface>(std::make_unique<Clean>(state, patch))
            : std::unique_ptr<const Interface>(std::make_unique<Dirty>(state, patch)))
    {}

}
