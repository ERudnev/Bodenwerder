#include <fQSM/processing/jobs/merge.h>
#include <fQSM/model/intertype/schema.h>

namespace fqsm::processing::jobs {

    void merge(Reading base, fqsm::ref<Patch> target, fqsm::cref<Patch> source) {
        for (const auto& entry : source->schema->nodes) {
            entry.second.binding.mergePatchSlice(base, *target, *source);
        }
    }

}
