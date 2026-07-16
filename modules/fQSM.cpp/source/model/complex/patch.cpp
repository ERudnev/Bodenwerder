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
        for (const auto& [typeId, node] : schema->nodes) {
            const auto source = other.lines.container.find(typeId);
            if (source == other.lines.container.end() or not source->second->has_changes()) {
                continue;
            }
            node.binding.patch.absorb(*this, other);
        }

        summary.critical.insert(summary.critical.end(), other.summary.critical.begin(), other.summary.critical.end());
        summary.warning.insert(summary.warning.end(), other.summary.warning.begin(), other.summary.warning.end());
    }

    void Patch::clear() {
        for (const auto& [typeId, node] : schema->nodes) {
            const auto line = lines.container.find(typeId);
            if (line == lines.container.end() or not line->second->has_changes()) {
                continue;
            }
            node.binding.patch.clear(*this);
        }

        summary = {};
    }

}
