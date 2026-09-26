#include <fQSM/processing/algorithms/integration.h>

#include <fQSM/erased/algorithms.h>
#include <fQSM/model/complex/patch.h>
#include <fQSM/model/complex/reality.h>
#include <fQSM/processing/_forwards.h>
#include <fQSM/utility/logging.h>

namespace fqsm::processing::algorithm {
    void integrate(model::complex::Reality& world, const model::complex::Patch& patch) {
        _DBG_TX_("integrate: patch={}", utility::format_patch(patch));
        for (model::complex::Patch::Slot slot = 0; slot < world.schema->slotCount(); ++slot) {
            const auto* line = patch.line(slot);
            if (not line or not line->has_changes()) continue;
            erased::integrate(world.writable(slot), *line);
        }
    }

    void integrate_consuming(model::complex::Reality& world, model::complex::Patch& patch, const meta::Rtid::Set& tainted) {
        _DBG_TX_("integrate: patch={}", utility::format_patch(patch));
        for (model::complex::Patch::Slot slot = 0; slot < world.schema->slotCount(); ++slot) {
            const auto* line = patch.line(slot);
            if (not line or not line->has_changes()) continue;
            world.learn(slot, *line);
            erased::integrate_move(world.writable(slot), patch.writable(slot));
        }
        for (const auto& typeId : tainted) {
            const auto slot = world.slotOf(typeId);
            if (not world.schema->linksOfClient[slot].empty()) world.rebuild_inbound(slot);
        }
    }
}
