#include <fQSM/model/analysis.h>

#include <fQSM/model/_forwards.h>
#include <fQSM/model/structure/binding.h>
#include <fQSM/model/structure/schema.h>
#include <fQSM/model/complex/patch.h>

namespace fqsm::analysis {

    Patch::Patch(const model::complex::Patch& patch) {
        for (const auto& [_, node] : patch.schema->nodes) {
            node.binding.analyzePatchSlice(patch, *this);
        }
        for (const auto& [_, slice] : perSlice) {
            ++overall.nonEmptyLines;
            overall.patchlets += slice.total();
        }
    }

}
