#pragma once

#include <fQSM/state/delta.h>
#include <fQSM/state/patch.h>
#include <fQSM/state/world.h>

namespace fqsm::state::world {

    struct Overlay : View {
        Overlay(const View& state, const Patch& patch) : View(state.schema), state(state), patch(patch) {}

        template<aspect::Any Meta>
        auto delta() const -> slice::Delta<Meta> {
            return slice::Delta<Meta>{*base::shared_ref_cast<const slice::Overlay<Meta>>(slice<Meta>())};
        }

    protected:
        auto slice(aspect::Rtid runtimeTypeId) const -> cref<AbstractSlice> override {
            return schema->nodes.at(runtimeTypeId).binding.createOverlay(state, patch);
        }

    private:
        const View& state;
        const Patch& patch;
    };

}