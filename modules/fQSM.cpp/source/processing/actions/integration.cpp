#include <fQSM/processing/actions/integration.h>
#include <fQSM/model/structure/schema.h>

namespace fqsm::processing::actions {
    void integrate(model::complex::Reality& world, const model::complex::Patch& patch) {
        for (const auto& [aspectId, node] : world.schema->nodes) {
            node.binding.integratePatchSlice(world, patch);
        }
    }
}
