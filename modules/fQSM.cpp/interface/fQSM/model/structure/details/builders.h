#pragma once

#include <base/shared_reference.h>

#include <fQSM/meta/interface.include.h>
#include <fQSM/model/_forwards.h>
#include <fQSM/model/analysis.h>
#include <fQSM/model/complex/reality.h>
#include <fQSM/model/linear/draft.h>
#include <fQSM/model/linear/patch.h>
#include <fQSM/model/linear/reality.h>
#include <fQSM/model/structure/binding.h>
#include <fQSM/processing/actions/integration.h>
#include <fQSM/processing/actions/merge.h>

namespace fqsm::schema::details {

    template<category::Any Meta>
    auto createState() -> ref<model::linear::state::Erased> {
        return base::make_shared<model::linear::Reality<Meta>>();
    }

    template<category::Any Meta>
    auto createPatch() -> ref<model::linear::patch::Erased> {
        return base::make_shared<model::linear::Patch<Meta>>();
    }

    template<category::Any Meta>
    auto cloneState(const model::complex::State& source) -> ref<model::linear::state::Erased> {
        auto out = base::make_shared<model::linear::Reality<Meta>>();
        out->global() = source.aspect<Meta>().global();
        for (const auto entry : source.aspect<Meta>().items()) {
            out->items().insert(entry.key, entry.value);
        }
        return out;
    }

    template<category::Any Meta>
    auto createDraft(const model::complex::State& state, ref<model::complex::Patch> patch) -> ref<model::linear::state::Erased> {
        return base::make_shared<model::linear::Draft<Meta>>(
            state.aspect<Meta>(),
            base::shared_ref_cast<model::linear::Patch<Meta>>(patch->lines.container.at(TypeId<Meta>))
        );
    }

    template<category::Any Meta>
    void integratePatchSlice(model::complex::Reality& world, const model::complex::Patch& patch) {
        fqsm::processing::actions::details::integrate<Meta>(world, patch);
    }

    template<category::Any Meta>
    void mergePatchSlice(const model::complex::State& base, model::complex::Patch& target, const model::complex::Patch& source) {
        fqsm::processing::actions::details::merge<Meta>(base, target, source);
    }

    template<category::Any Meta>
    void analyzePatchSlice(const model::complex::Patch& patch, analysis::Patch& out) {
        auto entry = analysis::Patch::SliceEntry{};
        const auto& slice = patch.aspect<Meta>();
        if (slice.global.has_value()) ++entry.modified;
        for (const auto patchEntry : slice.items) {
            if (patchEntry.value.has_value()) ++entry.modified;
            else ++entry.deleted;
        }
        if (entry.total() != 0) out.perSlice.emplace(TypeId<Meta>, entry);
    }

    template<category::Any Meta>
    auto binding() -> model::structure::Binding {
        return model::structure::Binding{
            &createState<Meta>,
            &createPatch<Meta>,
            &cloneState<Meta>,
            &createDraft<Meta>,
            &integratePatchSlice<Meta>,
            &mergePatchSlice<Meta>,
            &analyzePatchSlice<Meta>,
        };
    }
}
