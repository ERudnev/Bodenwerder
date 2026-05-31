#pragma once

#include <unordered_map>
#include <utility>

#include <fQSM/state/details/aware_at.h>
#include <fQSM/state/delta.h>
#include <fQSM/state/patch.h>
#include <fQSM/state/world.h>

namespace fqsm::state::world {

    struct Overlay : View {
        using Slices = std::unordered_map<aspect::Rtid, ref<AbstractSlice>, aspect::Rtid::Hash>;

        Overlay(const View& state, Patch& patch)
            : View(state.schema)
            , state(state)
            , patch(patch)
        {}

        template<aspect::Any Meta>
        auto delta() const -> ::fqsm::state::slice::Delta<Meta> {
            using SliceOverlay = ::fqsm::state::slice::Overlay<Meta>;
            return ::fqsm::state::slice::Delta<Meta>{
                *base::shared_ref_cast<const SliceOverlay>(View::slice<Meta>())
            };
        }

    protected:
        auto slice(aspect::Rtid runtimeTypeId) const -> cref<AbstractSlice> override {
            const auto it = slices.find(runtimeTypeId);
            if (it != slices.end()) {
                return it->second;
            }

            auto created = aware_at(schema->nodes, runtimeTypeId).binding.createOverlay(state, patch);
            const auto inserted = slices.emplace(runtimeTypeId, std::move(created));
            return inserted.first->second;
        }

    private:
        const View& state;
        Patch& patch;
        mutable Slices slices;
    };

}