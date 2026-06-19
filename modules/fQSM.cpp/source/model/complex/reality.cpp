#include <fQSM/model/complex/reality.h>
#include <fQSM/model/structure/schema.h>

namespace fqsm::model::complex {

    cref<State::Erased> Reality::aspect(Rtid typeId) const {
        return lines.slices.at(typeId);
    }

    ref<State::Erased> Reality::aspect(Rtid typeId) {
        return lines.slices.at(typeId);
    }

    void Reality::generate() {
        for (const auto& [typeId, node] : schema->nodes) {
            lines.slices.emplace(typeId, node.binding.createState());
        }
    }
}