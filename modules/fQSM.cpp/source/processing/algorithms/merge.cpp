#include <fQSM/processing/algorithms/merge.h>
#include <fQSM/processing/contexts/operational.h>
#include <fQSM/model/intertype/schema.h>

namespace fqsm::processing::algorithm {

    void merge(const model::complex::State& base, fqsm::ref<Patch> target, fqsm::cref<Patch> source) {
        for (const auto& entry : source->schema->nodes) {
            entry.second.binding.mergePatchSlice(base, *target, *source);
        }
    }

}
