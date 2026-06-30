#include <fQSM/model/complex/patch.h>

#include <fQSM/model/structure/schema.h>

namespace fqsm::model::complex {

    std::size_t Patch::quanta() const {
        std::size_t total = 0;
        for (const auto& entry : lines.container)
            total += entry.second->quanta();
        return total;
    }

    Composite<linear::patch::Erased> Patch::composition(Schema schema) {
        Composite<linear::patch::Erased> lines;
        for (const auto& [typeId, node] : schema->nodes)
            lines.container.emplace(typeId, node.binding.patch.create());
        return lines;
    }

    void Patch::absorb(const Patch& other) {
        for (const auto& [_, node] : schema->nodes)
            node.binding.patch.absorb(*this, other);
    }

    void Patch::clear() {
        for (const auto& [_, node] : schema->nodes)
            node.binding.patch.clear(*this);
    }

}
