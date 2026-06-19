#pragma once

#include <fQSM/meta/rtid.h>
#include <fQSM/model/complex/state.h>
#include <fQSM/model/complex/patch.h>
#include <fQSM/model/linear/delta.h>

namespace fqsm::model::complex {

    struct Draft {
        Draft( const State& state, Patch& patch, const aspect::Rtid::Set& dirty = {}) : state(state), patch(patch), dirty(std::move(dirty)) {}

        template<aspect::Any Meta>
        linear::Delta<Meta> delta() const;

    private:
        const State& state;
        Patch& patch;
        const aspect::Rtid::Set dirty;
    };
}

namespace fqsm::model::complex {

    template<aspect::Any Meta>
    linear::Delta<Meta> Draft::delta() const {
        using Delta = linear::Delta<Meta>;
        const auto mode = dirty.contains(aspect::Rtid::of<Meta>())
            ? Delta::Mode::dirty
            : Delta::Mode::clean;
        return Delta{state.aspect<Meta>(), patch.aspect<Meta>(), mode};
    }

}
