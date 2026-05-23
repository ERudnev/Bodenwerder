#pragma once

#include <fQSM/state/delta.h>
#include <fQSM/state/patch.h>
#include <fQSM/state/world.h>

namespace fqsm::state::world {

    struct Overlay : View {
        using CompositeData = composite::Overlay;

        Overlay(const View& state, const Patch& patch) : View(state.schema) {
            for (const auto& [aspectId, aspect] : schema->aspects) {
                composite().slices.emplace(aspectId, aspect.factory.createOverlay(state, patch));
            }
        }

        auto composite() -> CompositeData& { return slices; }

        template<aspect::Any Meta>
        auto delta() const -> slice::Delta<Meta> {
            return slice::Delta<Meta>{*base::shared_ref_cast<const slice::Overlay<Meta>>(slice<Meta>())};
        }

    protected:
        auto composite() const -> const CompositeView& override { return slices; }

    private:
        CompositeData slices;
    };

}