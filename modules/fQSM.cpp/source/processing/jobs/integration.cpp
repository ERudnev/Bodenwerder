#include <fQSM/processing/jobs/integration.h>
#include <fQSM/model/intertype/schema.h>

namespace fqsm::processing::jobs {
    void integrate(model::complex::Reality& world, const model::complex::Patch& patch) {
        for (const auto& [aspectId, node] : world.schema->nodes) {
            node.binding.integratePatchSlice(world, patch);
        }
    }
}
