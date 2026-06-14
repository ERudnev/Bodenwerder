/*
#include <fQSM/processing/actions/merge.h>

#include <fQSM/schema/dag.h>

namespace fqsm::processing::actions {

    void merge(Reading base, fqsm::ref<Patch> target, fqsm::cref<Patch> source) {
        for (const auto& entry : source->schema->nodes) {
            entry.second.binding.mergePatchSlice(base, target, source);
        }
    }

}
*/
