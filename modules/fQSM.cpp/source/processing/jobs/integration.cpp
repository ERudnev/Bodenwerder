#include <fQSM/processing/jobs/integration.h>
#include <fQSM/model/intertype/schema.h>
#include <fQSM/processing/_forwards.h>
#include <fQSM/utility/logging.h>

namespace fqsm::processing::jobs {
    void integrate(model::complex::Reality& world, const model::complex::Patch& patch) {
        _DBG_TX_("integrate: patch={}", utility::format_patch(patch));
        for (const auto& [aspectId, node] : world.schema->nodes) {
            node.binding.integratePatchSlice(world, patch);
        }
    }
}
