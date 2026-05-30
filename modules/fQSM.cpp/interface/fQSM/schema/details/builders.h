#pragma once

#include <base/shared_reference.h>

#include <fQSM/meta/concepts.h>
#include <fQSM/processing/actions/integration.h>
#include <fQSM/processing/actions/merge.h>
#include <fQSM/schema/binding.h>
#include <fQSM/state/patch.h>
#include <fQSM/state/world.h>

namespace fqsm::schema::details {
    namespace axis = meta::axis;

    template<aspect::Any Meta, axis::order Order>
    auto createSlice() -> ref<state::slice::Abstract<Order>> {
        return base::make_shared<state::slice::Data<Meta, Order>>();
    }

    template<aspect::Any Meta>
    auto cloneState(const state::world::View& source) -> ref<state::slice::Abstract<axis::order::state>> {
        auto out = base::make_shared<state::slice::Data<Meta, axis::order::state>>();
        for (const auto entry : source.template slice<Meta>()->items()) {
            out->items().insert(entry.first, entry.second);
        }
        return out;
    }

    template<aspect::Any Meta>
    auto createOverlay(const state::world::View& state, const state::world::Patch& patch) -> ref<state::slice::Abstract<axis::order::state>> {
        return base::make_shared<state::slice::Overlay<Meta>>(
            state.template slice<Meta>(),
            patch.template slice<Meta>());
    }

    template<aspect::Any Meta>
    void integratePatchSlice(state::world::Data& world, const state::world::Patch& patch) {
        fqsm::processing::actions::integrate<Meta>(world, patch);
    }

    template<aspect::Any Meta>
    void mergePatchSlice(const state::world::View& base, ref<state::world::Patch> target, cref<state::world::Patch> source) {
        fqsm::processing::actions::merge<Meta>(base, target, source);
    }

    template<aspect::Any Meta>
    auto binding() -> Binding {
        return Binding{
            &createSlice<Meta, axis::order::state>,
            &createSlice<Meta, axis::order::patch>,
            &cloneState<Meta>,
            &createOverlay<Meta>,
            &integratePatchSlice<Meta>,
            &mergePatchSlice<Meta>,
        };
    }
}
