#include <fQSM/model/complex/reality.h>

#include <fQSM/model/complex/pool.h>

namespace fqsm::model::complex {

    Reality::Reality(Schema schema) : State(schema, std::make_shared<LinePool>(schema)), indexes(schema->links.size()) {
        lines.reserve(schema->slotCount());
        for (const auto& descriptor : schema->descriptors)
            lines.push_back(std::make_unique<erased::Line>(descriptor));
    }

    Reality::Reality(const State& source) : Reality(source.schema) {
        for (Slot slot = 0; slot < lines.size(); ++slot)
            lines[slot]->clone(source.line(slot));
        rebuild_inbound();
    }

    void Reality::learn(Slot slot, const erased::PatchLine& patch) {
        for (const auto i : schema->linksOfClient[slot])
            indexes[i].apply(*lines[slot], patch, schema->links[i].read);
    }

    void Reality::rebuild_inbound(Slot slot) {
        for (const auto i : schema->linksOfClient[slot])
            indexes[i].rebuild(*lines[slot], schema->links[i].read);
    }

    void Reality::rebuild_inbound() {
        const auto& links = schema->links;
        for (std::size_t i = 0; i < links.size(); ++i)
            indexes[i].rebuild(*lines[links[i].client], links[i].read);
    }
}
