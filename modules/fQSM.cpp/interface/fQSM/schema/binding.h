#pragma once

#include <functional>

#include <base/shared_reference.h>

#include <fQSM/meta/axis.h>
#include <fQSM/meta/concepts.h>
#include <fQSM/references.h>
#include <fQSM/state/slice.h>

namespace fqsm::state::world {
    struct View;
    struct Data;
    struct Patch;
}

namespace fqsm::schema {

    namespace axis = meta::axis;

    struct Binding {
        std::function<ref<state::slice::Abstract<axis::order::state>>()> createState;
        std::function<ref<state::slice::Abstract<axis::order::patch>>()> createPatch;
        std::function<ref<state::slice::Abstract<axis::order::state>>(const state::world::View&)> cloneState;
        std::function<ref<state::slice::Abstract<axis::order::state>>(const state::world::View&, state::world::Patch&)> createOverlay;
        std::function<void(state::world::Data&, const state::world::Patch&)> integratePatchSlice;
        std::function<void(const state::world::View&, ref<state::world::Patch>, cref<state::world::Patch>)> mergePatchSlice;

        template<aspect::Any Meta>
        static Binding make();

        // idea: to simplify syntax, may be.. may be not
        //template<aspect::Any Meta, template<typename> typename Some, typename SomeBase>
        //auto sure_upcast(ref<SomeBase> value) -> ref<Some<Meta>>;
    };
}