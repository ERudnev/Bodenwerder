#include <fQSM/model/complex/patch.h>

#include <fQSM/model/intertype/schema.h>

namespace fqsm::model::complex {

    bool Patch::has_changes() const {
        for (const auto& entry : lines.container) {
            if (entry.second->has_changes())
                return true;
        }
        return false;
    }

    intertype::Composite<linear::patch::Erased> Patch::composition(Schema schema) {
        intertype::Composite<linear::patch::Erased> lines;
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
