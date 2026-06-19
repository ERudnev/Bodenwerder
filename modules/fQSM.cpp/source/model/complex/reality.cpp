#include <fQSM/model/complex/reality.h>
#include <fQSM/model/structure/schema.h>

namespace fqsm::model::complex {

    cref<State::Erased> Reality::aspect(Rtid typeId) const {
        return lines.container.at(typeId);
    }

    ref<State::Erased> Reality::aspect(Rtid typeId) {
        return lines.container.at(typeId);
    }

    void Reality::initStructure() {
        for (const auto& [typeId, node] : schema->nodes) {
            lines.container.emplace(typeId, node.binding.createState());
        }
    }
}