#pragma once

#include <base/cannonball/delta/interface.h>
#include <fQSM/meta/interface.include.h>
#include <fQSM/model/linear/patch.h>
#include <fQSM/model/linear/state.h>

namespace fqsm::model::linear {

    template<category::Any Meta>
    struct Delta {
        using Actual = base::cannonball::delta::Delta<Id<Meta>, Quantum<Meta>>;
        using Layer = base::cannonball::delta::Layer;
        enum class Mode {
            clean,
            dirty,
        };

        Delta(const State<Meta>& state, const Patch<Meta>& patch, Mode mode);

        auto begin() const { return actual.begin(); }
        auto end() const { return actual.end(); }
        auto added() const { return actual.added(); }
        auto addedOrUpdated() const { return actual.addedOrUpdated(); }
        auto removed() const { return actual.removed(); }
        auto updated() const { return actual.updated(); }

    private:
        const Actual actual;
    };
}

namespace fqsm::model::linear {

    template<category::Any Meta>
    Delta<Meta>::Delta(const State<Meta>& state, const Patch<Meta>& patch, Mode mode)
        : actual(state.items(), patch.items,
            mode == Mode::clean ? base::cannonball::delta::Mode::clean : base::cannonball::delta::Mode::dirty)
    {}
}
