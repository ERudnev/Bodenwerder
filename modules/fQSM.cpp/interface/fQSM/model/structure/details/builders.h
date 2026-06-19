#pragma once

#include <base/shared_reference.h>

#include <fQSM/meta/concepts.h>
#include <fQSM/processing/actions/integration.h>
#include <fQSM/processing/actions/merge.h>
#include <fQSM/schema/binding.h>
#include <fQSM/state/details/analysis.h>
#include <fQSM/state/patch.h>
#include <fQSM/state/slice/taint.h>
#include <fQSM/model/complex/view.h>

namespace fqsm::schema::details {
    namespace axis = meta::axis;

    template<aspect::Any Meta, axis::order Order>
    auto createSlice() -> ref<state::slice::Abstract<Order>> {
        return base::make_shared<state::slice::Data<Meta, Order>>();
    }

    template<aspect::Any Meta>
    auto cloneState(const model::complex::State& source) -> ref<state::slice::Abstract<axis::order::state>> {
        auto out = base::make_shared<state::slice::Data<Meta, axis::order::state>>();
        out->global() = source.template global<Meta>();
        for (const auto entry : source.template slice<Meta>()->items()) {
            out->items().insert(entry.first, entry.second);
        }
        return out;
    }


    // TODO: remove
    template<aspect::Any Meta>
    auto createOverlay(const model::complex::State& state, const model::complex::Patch& patch) -> ref<state::slice::Abstract<axis::order::state>> {
        return base::shared_ref_cast<state::slice::Abstract<axis::order::state>>(
            base::make_shared<state::slice::Draft<Meta>>(
                state.template slice<Meta>(),
                patch.template slice<Meta>())
        );
    }

    template<aspect::Any Meta>
    void integratePatchSlice(state::world::Data& world, const model::complex::Patch& patch) {
        if (patch.template slice<Meta>()->tainted()) return;
        fqsm::processing::actions::details::integrate<Meta>(world, patch);
    }

    template<aspect::Any Meta>
    void mergePatchSlice(const state::world::View& base, ref<model::complex::Patch> target, cref<model::complex::Patch> source) {
        if (source->template slice<Meta>()->tainted()) {
            const auto aspectId = aspect::Rtid::of<Meta>();
            if (!target->composite().slices.contains(aspectId)) {
                target->composite().slices.emplace(aspectId, target->schema->nodes.at(aspectId).binding.createDirtyVirtualPatch());
            }
            return;
        }
        fqsm::processing::actions::details::merge<Meta>(base, target, source);
    }

    template<aspect::Any Meta>
    void analyzePatchSlice(const model::complex::Patch& patch, analysis::Patch& out) {
        if (patch.template slice<Meta>()->tainted()) return;
        auto entry = analysis::Patch::SliceEntry{};
        if (patch.template global<Meta>().has_value()) {
            ++entry.modified;
        }
        for (const auto patchEntry : patch.template items<Meta>()) {
            if (patchEntry.second.has_value()) {
                ++entry.modified;
            } else {
                ++entry.deleted;
            }
        }

        if (entry.total() != 0) {
            out.perSlice.emplace(aspect::Rtid::of<Meta>(), entry);
        }
    }

    template<aspect::Any Meta>
    auto binding() -> Binding {
        return Binding{
            &createSlice<Meta, axis::order::state>,
            &createSlice<Meta, axis::order::patch>,
            &createDirtyVirtualPatch<Meta>,
            &cloneState<Meta>,
            &createOverlay<Meta>,
            &integratePatchSlice<Meta>,
            &mergePatchSlice<Meta>,
            &analyzePatchSlice<Meta>,
        };
    }
}
