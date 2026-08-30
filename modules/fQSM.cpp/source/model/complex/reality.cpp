#include <fQSM/model/complex/reality.h>
#include <fQSM/model/intertype/schema.h>

namespace fqsm::model::complex {

    void Reality::initStructure() {
        for (const auto& [typeId, node] : schema->nodes) {
            if (node.binding.assemble) continue;
            lines.container.emplace(typeId, node.binding.state.create());
        }
    }

    void Reality::putLine(meta::Rtid typeId, ref<linear::state::Erased> line) {
        lines.container.emplace(typeId, std::move(line));
    }

    Reality::Reality(const State& source) : State(source.schema) {
        for (const auto& [typeId, node] : source.schema->nodes)
            lines.container.emplace(typeId, node.binding.state.clone(source));
    }
}