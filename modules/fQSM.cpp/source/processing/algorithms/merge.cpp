#include <fQSM/processing/algorithms/merge.h>

#include <fQSM/erased/algorithms.h>
#include <fQSM/model/complex/patch.h>
#include <fQSM/model/complex/state.h>

namespace fqsm::processing::algorithm {

    void merge(const model::complex::State& base, fqsm::ref<Patch> target, fqsm::cref<Patch> source) {
        for (model::complex::Patch::Slot slot = 0; slot < source->schema->slotCount(); ++slot) {
            const auto* line = source->line(slot);
            if (not line or not line->has_changes()) continue;
            erased::merge_into(base.line(slot), target->writable(slot), *line);
        }
    }

}
