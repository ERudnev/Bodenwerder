#include <fQSM/model/complex/reality.h>
#include <fQSM/model/structure/schema.h>

namespace fqsm::model::complex {

    void Reality::initStructure() {
        for (const auto& [typeId, node] : schema->nodes) {
            lines.container.emplace(typeId, node.binding.createState());
        }
    }

    Reality::Reality(const State& source) : State(source.schema) {
        for (const auto& [typeId, node] : source.schema->nodes)
            lines.container.emplace(typeId, node.binding.cloneState(source));
    }
}