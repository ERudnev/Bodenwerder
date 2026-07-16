#include <fQSM/processing/algorithms/integration.h>
#include <fQSM/model/intertype/schema.h>
#include <fQSM/processing/_forwards.h>
#include <fQSM/utility/logging.h>

namespace fqsm::processing::algorithm {
    void integrate(model::complex::Reality& world, const model::complex::Patch& patch) {
        _DBG_TX_("integrate: patch={}", utility::format_patch(patch));
        for (const auto& [aspectId, node] : world.schema->nodes) {
            const auto line = patch.lines.container.find(aspectId);
            if (line == patch.lines.container.end() or not line->second->has_changes()) {
                continue;
            }
            node.binding.integratePatchSlice(world, patch);
        }
    }
}
