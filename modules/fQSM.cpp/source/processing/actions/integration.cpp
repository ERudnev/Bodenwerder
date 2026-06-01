#include <fQSM/processing/actions/integration.h>

namespace fqsm::processing::actions {
    void integrate(state::world::Data& world, const state::world::Patch& patch) {
        for (const auto& [aspectId, node] : world.schema->nodes) {
            node.binding.integratePatchSlice(world, patch);
        }
    }
}