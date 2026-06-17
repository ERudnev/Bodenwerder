#include <fQSM/model/internal/analysis.h>

#include <fQSM/schema/binding.h>
#include <fQSM/model/complex/patch.h>

namespace fqsm::analysis {

    Patch::Patch(const state::world::Patch& patch) {
        for (const auto& [_, node] : patch.schema->nodes) {
            node.binding.analyzePatchSlice(patch, *this);
        }
    }

    int Patch::overallChanges() const {
        int out = 0;
        for (const auto& [_, slice] : perSlice) {
            out += slice.total();
        }
        return out;
    }
}
