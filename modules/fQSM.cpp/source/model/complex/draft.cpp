#include <fQSM/model/complex/draft.h>
#include <fQSM/model/structure/schema.h>

namespace fqsm::model::complex {

    void Draft::initStructure() {
        for (const auto& [typeId, node] : schema->nodes) {
            lines.container.emplace(typeId, node.binding.createDraft(state, patch));
        }
    }
}
