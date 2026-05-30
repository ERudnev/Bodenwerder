#pragma once

#include <unordered_map>

#include <fQSM/state/delta.h>
#include <fQSM/state/patch.h>
#include <fQSM/state/world.h>

namespace fqsm::state::world {

    struct Overlay : View {
        using Slices = std::unordered_map<aspect::Rtid, ref<AbstractSlice>, aspect::Rtid::Hash>;

        Overlay(const View& state, const Patch& patch) : View(state.schema) {
            for (const auto& [aspectId, aspect] : schema->nodes) {
                slices.emplace(aspectId, aspect.binding.createOverlay(state, patch));
            }
        }

        template<aspect::Any Meta>
        auto delta() const -> ::fqsm::state::slice::Delta<Meta> {
            using SliceOverlay = ::fqsm::state::slice::Overlay<Meta>;
            return ::fqsm::state::slice::Delta<Meta>{
                *base::shared_ref_cast<const SliceOverlay>(View::slice<Meta>())
            };
        }

    protected:
        auto slice(aspect::Rtid runtimeTypeId) const -> cref<AbstractSlice> override {
            return slices.at(runtimeTypeId);
        }

    private:
        Slices slices;
    };

}