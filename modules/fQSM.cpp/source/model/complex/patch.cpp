#include <fQSM/model/complex/patch.h>

#include <fQSM/model/structure/schema.h>

namespace fqsm::model::complex {

    Composite<linear::patch::Erased> Patch::composition(Schema schema) {
        Composite<linear::patch::Erased> lines;
        for (const auto& [typeId, node] : schema->nodes)
            lines.container.emplace(typeId, node.binding.createPatch());
        return lines;
    }

}
