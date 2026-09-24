#include <fQSM/model/complex/reality.h>

#include <fQSM/model/complex/pool.h>

namespace fqsm::model::complex {

    Reality::Reality(Schema schema) : State(schema, std::make_shared<LinePool>(schema)) {
        lines.reserve(schema->slotCount());
        for (const auto& descriptor : schema->descriptors)
            lines.push_back(std::make_unique<erased::Line>(descriptor));
    }

    Reality::Reality(const State& source) : Reality(source.schema) {
        for (Slot slot = 0; slot < lines.size(); ++slot)
            lines[slot]->clone(source.line(slot));
    }
}
